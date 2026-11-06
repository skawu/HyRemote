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

// With a 250ms idle probe, this confirms a blocked capture request resumes
// on the input/Viewer notification rather than merely on the next timer tick.
template<typename Notify>
bool expectPromptWake(Gate &gate, Notify notify, bool expected,
                      const char *message, std::chrono::milliseconds precheck = 35ms)
{
    std::promise<bool> completion;
    auto result = completion.get_future();
    std::thread waiter([&] { completion.set_value(gate.waitForViewer()); });
    const bool wasBlocked = result.wait_for(precheck) == std::future_status::timeout;

    notify();
    // The bound is deliberately shorter than the 250ms static probe. It does
    // not claim an application/render/transport latency bound.
    const bool wokePromptly = result.wait_for(120ms) == std::future_status::ready;
    if (!wokePromptly)
        gate.stop(); // unblock on failure so a test never leaks a worker
    waiter.join();

    return check(wasBlocked && wokePromptly && result.get() == expected, message);
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
    if (!expectPromptWake(gate, [&] { gate.noteRemoteInput(); }, true,
                          "remote input did not promptly wake static capture"))
        return 2;

    // The input resets the idle counter; entering idle again must be safe.
    for (std::size_t i = 0; i < Gate::kConsecutiveUnchangedFrames; ++i)
        gate.noteFrameChange(false);

    // Removing the last viewer must NOT make capture run. It remains sleeping
    // until a subsequent real connection; reconnect then requires full init.
    gate.setViewerCount(0);
    if (!expectPromptWake(gate, [&] { gate.setViewerCount(1); }, true,
                          "reconnecting viewer did not resume capture"))
        return 3;

    // After a fresh viewer leaves, 0V with a previously queued bootstrap
    // must not busy-spin, even if the idle counter was reset.
    gate.setViewerCount(0);
    if (!expectPromptWake(gate, [&] { gate.stop(); }, false,
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

    std::cout << "PASS 250ms bounded probe, remote input prompt wake, "
                 "disconnect/reconnect and stop\n";
    return 0;
}
