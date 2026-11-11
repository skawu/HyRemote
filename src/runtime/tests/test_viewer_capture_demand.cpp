// #789: exercise the static idle gate's freshness escapes without a Viewer.
// This intentionally tests private scheduling semantics, not an end-to-end
// RealVNC latency SLO or the RK3588/Qt graphics driver.
#include "detail/viewer_capture_demand.hpp"

#include <chrono>
#include <future>
#include <iostream>
#include <thread>

namespace {

using Gate = HyRemote::detail::ViewerCaptureDemand;
using namespace std::chrono_literals;

bool check(bool condition, const char *message)
{
    if (!condition)
        std::cerr << "FAIL: " << message << '\n';
    return condition;
}

// Zero-viewer waits have no probe deadline, so any wake requires a real
// notification. Avoid fragile sub-250ms assertions under CI descheduling.
template<typename Notify>
bool expectZeroViewerWake(Gate &gate, Notify notify, bool expected, const char *message)
{
    std::promise<bool> completion;
    auto result = completion.get_future();
    std::thread waiter([&] { completion.set_value(gate.waitForViewer()); });
    const bool wasBlocked = result.wait_for(40ms) == std::future_status::timeout;
    notify();
    const bool woke = result.wait_for(2s) == std::future_status::ready;
    if (!woke)
        gate.stop();
    waiter.join();
    return check(wasBlocked && woke && result.get() == expected, message);
}

// Exact synchronization: the test-only hook runs on the scheduler thread
// immediately before its hour-away static wait. The ensuing observed wake
// cannot be a 250ms timer expiry, even if the CI worker is descheduled.
bool expectRemoteInputWakeFromStaticWait(Gate &gate)
{
    std::promise<void> waiting;
    auto entered = waiting.get_future();
    gate.armStaticWaitObserverForTest([&] { waiting.set_value(); });

    std::promise<bool> completion;
    auto result = completion.get_future();
    std::thread waiter([&] { completion.set_value(gate.waitForViewer()); });
    const bool reachedStaticWait = entered.wait_for(2s) == std::future_status::ready;
    if (reachedStaticWait)
        gate.noteRemoteInput();
    else
        gate.stop();
    const bool woke = result.wait_for(2s) == std::future_status::ready;
    if (!woke)
        gate.stop();
    waiter.join();
    return check(reachedStaticWait && woke && result.get(),
                 "remote input did not wake the real blocked static capture");
}

bool expectQtActivityWakeFromStaticWait(Gate &gate)
{
    std::promise<void> waiting;
    auto entered = waiting.get_future();
    gate.armStaticWaitObserverForTest([&] { waiting.set_value(); });
    std::promise<bool> completion;
    auto result = completion.get_future();
    std::thread waiter([&] { completion.set_value(gate.waitForViewer()); });
    const bool armed = entered.wait_for(2s) == std::future_status::ready;
    if (armed)
        gate.noteQtRenderActivity();
    else
        gate.stop();
    const bool woke = result.wait_for(2s) == std::future_status::ready;
    if (!woke)
        gate.stop();
    waiter.join();
    return check(armed && woke && result.get(),
                 "independent Qt GUI activity did not wake a static capture");
}

bool expectOwnGrabDoesNotWakeStaticWait(Gate &gate)
{
    // A public grabToImage itself triggers a QQuickWindow render. The signal
    // must NOT produce a new capture until the existing request completes.
    gate.noteCaptureStarted();
    std::promise<void> waiting;
    auto entered = waiting.get_future();
    gate.armStaticWaitObserverForTest([&] { waiting.set_value(); });
    std::promise<bool> completion;
    auto result = completion.get_future();
    std::thread waiter([&] { completion.set_value(gate.waitForViewer()); });
    const bool armed = entered.wait_for(2s) == std::future_status::ready;
    if (armed)
        gate.noteQtRenderActivity(); // triggered by our own in-flight grab
    const bool suppressed = result.wait_for(60ms) == std::future_status::timeout;
    gate.noteCaptureCompleted();
    gate.noteQtRenderActivity(); // independent next render event
    const bool woke = result.wait_for(2s) == std::future_status::ready;
    if (!woke)
        gate.stop();
    waiter.join();
    return check(armed && suppressed && woke && result.get(),
                 "self-induced Qt grab update retriggered static capture");
}

bool testQtActivityClassification()
{
    // This is the exact semantic distinction absent in #952's first
    // Windows A1/B/A2: most Qt render signals do NOT release idle gating.
    // afterAnimating may originate from grabToImage() itself.
    Gate gate;
    using Reason = Gate::ActivityResult;
    if (!check(gate.noteQtRenderActivity() == Reason::Stopped,
               "stopped session accepted Qt render activity"))
        return false;
    gate.start();
    if (!check(gate.noteQtRenderActivity() == Reason::NoViewer,
               "zero-viewer activity bypassed capture admission"))
        return false;
    gate.frameQueued(true);
    gate.setViewerCount(1);
    if (!check(gate.noteQtRenderActivity() == Reason::AlreadyActive,
               "already-active capture was rearmed unnecessarily"))
        return false;
    for (std::size_t i = 0; i < Gate::kConsecutiveUnchangedFrames; ++i)
        gate.noteFrameChange(false);
    gate.noteCaptureStarted();
    if (!check(gate.noteQtRenderActivity() == Reason::CaptureInFlight,
               "our own in-flight grab unexpectedly rearmed idle capture"))
        return false;
    gate.noteCaptureCompleted();
    gate.noteDeliveryBackpressure(true);
    if (!check(gate.noteQtRenderActivity() == Reason::DeliveryBackpressure,
               "Qt activity bypassed transport backpressure"))
        return false;
    gate.noteDeliveryBackpressure(false);
    if (!check(gate.noteQtRenderActivity() == Reason::ReleasedStaticWait,
               "idle-ready Qt activity failed to release bounded static probe"))
        return false;
    if (!check(gate.noteQtRenderActivity() == Reason::AlreadyActive,
               "same Qt update repeatedly reset active frame counting"))
        return false;
    gate.stop();
    return check(gate.noteQtRenderActivity() == Reason::Stopped,
                 "Qt activity still accepted after stop");
}

void enterStaticGate(Gate &gate)
{
    gate.start();
    gate.frameQueued(true);
    gate.setViewerCount(1);
    for (std::size_t i = 0; i < Gate::kConsecutiveUnchangedFrames; ++i)
        gate.noteFrameChange(false);
}

} // namespace

int main()
{
    if (!testQtActivityClassification())
        return 18;
    Gate gate;
    enterStaticGate(gate);
    if (!check(Gate::kStaticProbeInterval == 250ms,
               "experiment expected a 250ms private static probe"))
        return 1;

    // A remote user action must wake a scheduler already waiting in static
    // mode. It must not be delayed by the 250ms local-only fallback.
    if (!expectRemoteInputWakeFromStaticWait(gate))
        return 2;

    // The input resets the idle counter; entering idle again must be safe.
    for (std::size_t i = 0; i < Gate::kConsecutiveUnchangedFrames; ++i)
        gate.noteFrameChange(false);

    // Source-activity hints are optional and only trigger when the Qt host
    // independently renders while no capture is outstanding. They are NOT
    // trusted as pixel damage and do not change the 60fps Core limit.
    enterStaticGate(gate);
    if (!expectQtActivityWakeFromStaticWait(gate))
        return 13;
    gate.stop();
    enterStaticGate(gate);
    if (!expectOwnGrabDoesNotWakeStaticWait(gate))
        return 14;
    gate.stop();

    // Removing the last viewer must NOT make capture run. It remains sleeping
    // until a subsequent real connection; reconnect then requires full init.
    gate.setViewerCount(0);
    if (!expectZeroViewerWake(gate, [&] { gate.setViewerCount(1); }, true,
                          "reconnecting viewer did not resume capture"))
        return 3;

    // After a fresh viewer leaves, 0V with a previously queued bootstrap
    // must not busy-spin, even if the idle counter was reset.
    gate.setViewerCount(0);
    if (!expectZeroViewerWake(gate, [&] { gate.stop(); }, false,
                          "stop did not release a zero-viewer scheduler"))
        return 4;

    // A non-input, local-only change is discovered by the bounded probe and
    // cannot make the capture scheduler wait forever.
    enterStaticGate(gate);
    std::promise<bool> captured;
    auto future = captured.get_future();
    std::thread probe([&] { captured.set_value(gate.waitForViewer()); });
    const bool probeReady = future.wait_for(800ms) == std::future_status::ready;
    if (!probeReady)
        gate.stop();
    probe.join();
    if (!check(probeReady && future.get(), "bounded local-only probe never fired"))
        return 5;
    gate.stop();

    // A bounded producer must not keep grabbing while its only Normal RFB
    // viewer is draining a previous large FBU. A new connection gets one
    // initial escape; a remote input event gets one more, but neither turns
    // permanent backpressure into an unbounded spin.
    gate.start();
    gate.frameQueued(true);
    gate.setViewerCount(1);
    gate.noteDeliveryBackpressure(true);
    if (!check(gate.waitForViewer(), "first viewer lost one-shot bootstrap escape"))
        return 6;

    std::promise<bool> blockedCompletion;
    auto blockedFuture = blockedCompletion.get_future();
    std::thread blockedWaiter([&] { blockedCompletion.set_value(gate.waitForViewer()); });
    const bool blocked = blockedFuture.wait_for(40ms) == std::future_status::timeout;
    gate.noteRemoteInput();
    const bool inputWoke = blockedFuture.wait_for(2s) == std::future_status::ready;
    if (!inputWoke)
        gate.stop();
    blockedWaiter.join();
    if (!check(blocked && inputWoke && blockedFuture.get(),
               "backpressure ignored or remote input failed to escape once"))
        return 7;

    std::promise<bool> drainedCompletion;
    auto drainedFuture = drainedCompletion.get_future();
    std::thread drainedWaiter([&] { drainedCompletion.set_value(gate.waitForViewer()); });
    const bool blockedAgain = drainedFuture.wait_for(40ms) == std::future_status::timeout;
    gate.noteDeliveryBackpressure(false);
    const bool drainedWoke = drainedFuture.wait_for(2s) == std::future_status::ready;
    if (!drainedWoke)
        gate.stop();
    drainedWaiter.join();
    if (!check(blockedAgain && drainedWoke && drainedFuture.get(),
               "backpressure did not remain bounded until the socket drained"))
        return 8;

    // Even a repeatedly non-incremental Viewer must not be starved forever:
    // the first full FBU request may deliver the cached picture, but a fresh
    // source request is granted once, and saturation resumes thereafter.
    gate.noteDeliveryBackpressure(true);
    gate.noteForcedViewerRefresh();
    if (!check(gate.waitForViewer(), "explicit full FBU request lost its one-shot capture"))
        return 9;
    std::promise<bool> fullBlockedCompletion;
    auto fullBlockedFuture = fullBlockedCompletion.get_future();
    std::thread fullBlockedWaiter(
        [&] { fullBlockedCompletion.set_value(gate.waitForViewer()); });
    const bool fullBlocked = fullBlockedFuture.wait_for(40ms) == std::future_status::timeout;
    gate.noteDeliveryBackpressure(false);
    const bool fullDrainWoke = fullBlockedFuture.wait_for(2s) == std::future_status::ready;
    if (!fullDrainWoke)
        gate.stop();
    fullBlockedWaiter.join();
    if (!check(fullBlocked && fullDrainWoke && fullBlockedFuture.get(),
               "full FBU escape bypassed the next backpressure wait"))
        return 10;
    gate.stop();

    // A client that is not pulling must not consume 60fps captures forever,
    // but a later single full refresh must not be served an arbitrarily old
    // source. Check the independent 250ms fallback without involving Qt timers.
    gate.start();
    gate.frameQueued(true);
    gate.setViewerCount(1);
    gate.noteDeliveryBackpressure(true, true);
    if (!check(gate.waitForViewer(), "first no-demand Viewer bootstrap lost"))
        return 11;
    std::promise<bool> idleProbeCompletion;
    auto idleProbeFuture = idleProbeCompletion.get_future();
    std::thread idleProbe([&] { idleProbeCompletion.set_value(gate.waitForViewer()); });
    const bool probeFired = idleProbeFuture.wait_for(800ms) == std::future_status::ready;
    if (!probeFired)
        gate.stop();
    idleProbe.join();
    if (!check(probeFired && idleProbeFuture.get(),
               "no-request Viewer freshness probe exceeded its bounded interval"))
        return 12;
    gate.stop();

    // Qt render notifications must not escape socket backpressure or
    // wake a 0-viewer Source. This is independent of real Qt render-loop
    // timing and uses strict blocked futures (rather than sleeps as a
    // substitute for acquisition).
    gate.start();
    gate.frameQueued(true);
    gate.setViewerCount(1);
    gate.noteDeliveryBackpressure(true);
    if (!check(gate.waitForViewer(), "initial capture escape was lost"))
        return 15;
    std::promise<bool> activityBlockedPromise;
    auto activityBlocked = activityBlockedPromise.get_future();
    std::thread activityBlockedThread([&] {
        activityBlockedPromise.set_value(gate.waitForViewer());
    });
    const bool wasBlockedForNetwork =
        activityBlocked.wait_for(40ms) == std::future_status::timeout;
    gate.noteQtRenderActivity();
    const bool stayedBlockedForNetwork =
        activityBlocked.wait_for(60ms) == std::future_status::timeout;
    gate.noteDeliveryBackpressure(false);
    const bool networkDrainWoke =
        activityBlocked.wait_for(2s) == std::future_status::ready;
    if (!networkDrainWoke)
        gate.stop();
    activityBlockedThread.join();
    if (!check(wasBlockedForNetwork && stayedBlockedForNetwork
                   && networkDrainWoke && activityBlocked.get(),
               "Qt activity bypassed socket backpressure"))
        return 16;
    gate.stop();

    gate.start();
    gate.frameQueued(true);
    gate.setViewerCount(0);
    std::promise<bool> zeroActivityPromise;
    auto zeroActivity = zeroActivityPromise.get_future();
    std::thread zeroActivityThread([&] {
        zeroActivityPromise.set_value(gate.waitForViewer());
    });
    const bool zeroWasBlocked =
        zeroActivity.wait_for(40ms) == std::future_status::timeout;
    gate.noteQtRenderActivity();
    const bool zeroStayedBlocked =
        zeroActivity.wait_for(60ms) == std::future_status::timeout;
    gate.stop();
    const bool zeroStopWoke =
        zeroActivity.wait_for(2s) == std::future_status::ready;
    zeroActivityThread.join();
    if (!check(zeroWasBlocked && zeroStayedBlocked && zeroStopWoke
                   && !zeroActivity.get(),
               "Qt render activity forced capture with zero viewers"))
        return 17;

    std::cout << "PASS 250ms bounded probe, Qt GUI activity wake and self-grab suppression, "
                 "remote input prompt wake, disconnect/reconnect, delivery backpressure, "
                 "no-demand freshness and stop\n";
    return 0;
}
