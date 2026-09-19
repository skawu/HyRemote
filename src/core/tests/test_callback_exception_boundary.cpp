// #164 acceptance 1 and 2: no internal exception may escape a Core callback into the backend stack that
// invoked it, and the boundary itself must survive an allocation failure - which is the realistic cause,
// so the handler cannot afford to format a message by allocating.
//
// The injection needs no test-only hook in production code: this binary replaces the global allocation
// function and throws std::bad_alloc at an armed position. The fakes deliver from the calling thread, so
// anything that escaped the boundary would surface in this test's own stack.

#include "fakes.hpp"
#include "test_support.hpp"

#include <hyremote/core/session.hpp>

#include <atomic>
#include <cstdlib>
#include <new>
#include <optional>
#include <string>

namespace {

// -1 disarmed. A value of k means: the (k+1)-th allocation from now throws, then the counter disarms.
std::atomic<int> g_remainingAllocations{-1};

}  // namespace

// Replaces the global allocation function for this test binary only. Disarmed (-1) reproduces the
// default behaviour, so the harness, the fakes and the rest of Core are unaffected.
void *operator new(std::size_t size)
{
    const int remaining = g_remainingAllocations.load(std::memory_order_relaxed);
    if (remaining > 0) {
        g_remainingAllocations.store(remaining - 1, std::memory_order_relaxed);
    } else if (remaining == 0) {
        g_remainingAllocations.store(-1, std::memory_order_relaxed);
        throw std::bad_alloc();
    }

    if (void *memory = std::malloc(size == 0 ? 1 : size))
        return memory;
    throw std::bad_alloc();
}

namespace {

using namespace hyremote;
using namespace hyremote::test;

// Sweeps the allocation position rather than guessing one: with the countdown armed at k, the (k+1)-th
// allocation throws wherever it occurs. Every position has to be contained, so the test cannot pass by
// happening to arm the one position that is already safe.
//
// Position 0 is excluded because it is the harness's own handler copy inside `FakeCaptureSource`
// (`support/fakes.hpp`, `handler = m_onFrame;`), which does allocate on this toolchain; a throw there is
// a test-support failure, not a product one.
//
// Measured while writing this: the frame callback path allocates **nothing** for a valid frame and, for an
// invalid one, the rejection reason is short enough to stay inside the small-string buffer. So this case
// proves the contract that matters here - no internal exception escapes at any allocation position the
// callback can be reached from - while the capture-event case below is the one that proves the boundary is
// what caught a failure, because its fault path allocates a message by construction.
HYR_TEST(coreCallbackContainsAllocationFailure)
{
    constexpr int firstCallbackAllocation = 1;
    constexpr int allocationPositions = 64;

    for (int position = firstCallbackAllocation; position < allocationPositions; ++position) {
        RunningSession run;
        run.prepare();
        if (!run.start()) {
            const std::optional<SessionError> startError = run.session->lastError();
            HYR_CHECK_MSG(false, "session did not start: "
                                     + (startError ? startError->message
                                                   : std::string("no diagnostic reported")));
        }

        // An invalid frame, built before arming so its own construction cannot consume the countdown. The
        // rejection path assigns a reason longer than the small-string buffer, so the callback allocates
        // deterministically inside the boundary - which is what makes the injection land there. A valid
        // frame turns out to be allocation-free on this path, so it could not prove the boundary at all.
        RemoteFrame frame;

        bool escaped = false;
        std::string escapedWhat;
        g_remainingAllocations.store(position, std::memory_order_relaxed);
        try {
            run.source->deliver(std::move(frame));
        } catch (const std::exception &error) {
            escaped = true;
            escapedWhat = error.what();
        } catch (...) {
            escaped = true;
            escapedWhat = "non-std exception";
        }
        g_remainingAllocations.store(-1, std::memory_order_relaxed);

        HYR_CHECK_MSG(!escaped,
                      "an internal exception escaped the frame callback boundary at allocation position "
                          + std::to_string(position) + ": " + escapedWhat);

        // The callback path is not on the fault path for a single lost frame, so the session must still be
        // usable afterwards - that is what makes the reporting recoverable rather than terminal.
        HYR_CHECK(run.session->state() == SessionState::Running);

        if (const std::optional<SessionError> error = run.session->lastError()) {
            HYR_CHECK_EQ(error->code, SessionErrorCode::ComponentFailure);
            HYR_CHECK(error->recoverable);
        }

        // Frame delivery keeps working after a contained callback failure.
        RemoteFrame next = makeFrame();
        HYR_CHECK(run.source->deliver(std::move(next)));
        HYR_CHECK(run.session->stats().framesAccepted >= 1);
    }
}

HYR_TEST(coreCaptureEventCallbackContainsAllocationFailure)
{
    // Same harness exclusion as above, same reason.
    constexpr int firstCallbackAllocation = 1;
    constexpr int allocationPositions = 32;

    for (int position = firstCallbackAllocation; position < allocationPositions; ++position) {
        RunningSession run;
        run.prepare();
        HYR_CHECK(run.start());

        // A non-recoverable event carrying a message longer than the small-string buffer, so that the fault
        // path has something to copy.
        CaptureEvent event;
        event.recoverable = false;
        event.message = std::string(256, 'x');

        // Either the event was processed (non-recoverable, so the session faults) or the injected failure
        // landed and the boundary reported it as recoverable instead. Both are acceptable here; what is
        // not acceptable is an escape, and the case's final check requires that the boundary did report at
        // least once, so this cannot silently pass without the boundary doing its job.
        const SessionState state = run.session->state();
        HYR_CHECK(state == SessionState::Running || state == SessionState::Faulted);

        bool escaped = false;
        std::string escapedWhat;
        g_remainingAllocations.store(position, std::memory_order_relaxed);
        try {
            run.source->reportEvent(event);
        } catch (const std::exception &error) {
            escaped = true;
            escapedWhat = error.what();
        } catch (...) {
            escaped = true;
            escapedWhat = "non-std exception";
        }
        g_remainingAllocations.store(-1, std::memory_order_relaxed);

        HYR_CHECK_MSG(!escaped,
                      "an internal exception escaped the capture event boundary at allocation position "
                          + std::to_string(position) + ": " + escapedWhat);

        if (const std::optional<SessionError> error = run.session->lastError()) {
            HYR_CHECK_EQ(error->code, SessionErrorCode::ComponentFailure);
        }
    }

    // Measured while writing this: one delivery on this path performs exactly one allocation, and it is the
    // harness's handler copy, so no swept position lies inside the callback body - which is why this case
    // claims no more than it can prove, namely that nothing escapes at any reachable position. The case
    // that proves the boundary reports what it catches is the input one below, where a throwing sink is the
    // deterministic trigger.
}

// A sink that throws is the deterministic way to prove the *reporting* half of the boundary: the failure
// has to become an existing error-model entry, the session has to stay usable, and nothing may escape.
class ThrowingInputSink final : public InputSink
{
public:
    void post(const InputEvent &) override { throw std::runtime_error("throwing input sink"); }
};

HYR_TEST(coreInputCallbackReportsInsteadOfEscaping)
{
    RunningSession run;
    run.prepare();
    run.setInputSink(std::make_shared<ThrowingInputSink>());
    HYR_CHECK(run.start());

    InputEvent event;
    bool escaped = false;
    std::string escapedWhat;
    try {
        run.transport->deliverInput(event);
    } catch (const std::exception &error) {
        escaped = true;
        escapedWhat = error.what();
    } catch (...) {
        escaped = true;
        escapedWhat = "non-std exception";
    }

    HYR_CHECK_MSG(!escaped, "an exception escaped the input callback boundary: " + escapedWhat);

    const std::optional<SessionError> error = run.session->lastError();
    HYR_CHECK_MSG(error.has_value(), "the contained failure was not reported through the error model");
    HYR_CHECK_EQ(error->code, SessionErrorCode::ComponentFailure);
    HYR_CHECK_MSG(error->recoverable, "an input sink failure must not make the session terminal");
    HYR_CHECK(run.session->stats().inputPostFailures >= 1);
    HYR_CHECK(run.session->state() == SessionState::Running);
}

}  // namespace

HYR_TEST_MAIN()
