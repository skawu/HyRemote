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

// This is a SOURCE-owned invalidation, not the Qt grab/afterAnimating signal.
// A timer set one hour into the future makes a successful capture wake
// unambiguously notification-driven rather than a periodic probe.
bool expectSourceInvalidationWakeFromStaticWait(Gate &gate)
{
    std::promise<void> waiting;
    auto entered = waiting.get_future();
    gate.armStaticWaitObserverForTest([&] { waiting.set_value(); });

    std::promise<bool> completion;
    auto result = completion.get_future();
    std::thread waiter([&] { completion.set_value(gate.waitForViewer()); });
    const bool reachedStaticWait = entered.wait_for(2s) == std::future_status::ready;
    if (reachedStaticWait)
        gate.noteSourceInvalidation();
    else
        gate.stop();
    const bool woke = result.wait_for(2s) == std::future_status::ready;
    if (!woke)
        gate.stop();
    waiter.join();
    return check(reachedStaticWait && woke && result.get(),
                 "source-owned change did not wake the dormant static capture");
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

    // Reset after previous tests. The source-change seam has no producer
    // connection until a future, separately qualified experiment.
    enterStaticGate(gate);
    if (!expectSourceInvalidationWakeFromStaticWait(gate))
        return 13;
    gate.stop();

    // A source change in 0-viewer mode must never trigger background captures.
    // Only a real client connection can release this indefinite sleep.
    gate.start();
    gate.frameQueued(true);
    gate.noteSourceInvalidation(); // no viewer, no implicit wake
    if (!expectZeroViewerWake(
            gate, [&] { gate.setViewerCount(1); }, true,
            "source invalidation bypassed zero-viewer sleep or blocked reconnect"))
        return 14;
    gate.stop();

    // Local scene change must NOT override transport backpressure. The
    // downstream Viewer cannot consume an additional FBU until it drains.
    gate.start();
    gate.frameQueued(true);
    gate.setViewerCount(1);
    gate.noteDeliveryBackpressure(true);
    if (!check(gate.waitForViewer(), "first Viewer backpressure escape unavailable"))
        return 15;
    std::promise<bool> remainedBlocked;
    auto backpressured = remainedBlocked.get_future();
    std::thread blockedSourceWaiter([&] {
        remainedBlocked.set_value(gate.waitForViewer());
    });
    const bool enteredBlockedWait = backpressured.wait_for(40ms)
                                    == std::future_status::timeout;
    gate.noteSourceInvalidation();
    const bool stayedBlocked = backpressured.wait_for(50ms)
                               == std::future_status::timeout;
    gate.noteDeliveryBackpressure(false);
    const bool finallyWoke = backpressured.wait_for(2s)
                             == std::future_status::ready;
    if (!finallyWoke)
        gate.stop();
    blockedSourceWaiter.join();
    if (!check(enteredBlockedWait && stayedBlocked && finallyWoke
               && backpressured.get(),
               "source invalidation escaped existing transport backpressure"))
        return 16;
    gate.stop();

    std::cout << "PASS 250ms bounded probe, remote input prompt wake, "
                 "disconnect/reconnect, delivery backpressure, no-demand freshness, source-only wake, backpressure and stop\n";
    return 0;
}
