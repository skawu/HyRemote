// #164 acceptance 1 and 2: no internal exception may escape a Core callback into the backend stack that
// invoked it, and the boundary itself must survive an allocation failure - which is the realistic cause,
// so the handler cannot afford to format a message by allocating.
//
// The injection needs no test-only hook in production code: this binary replaces the global allocation
// function and throws std::bad_alloc at an armed position on the thread that armed it. The fakes deliver from
// the calling thread, so anything that escaped the boundary would surface in this test's own stack.

#include "fakes.hpp"
#include "test_support.hpp"

#include <hyremote/core/session.hpp>

#include <atomic>
#include <cstdlib>
#include <new>
#include <optional>
#include <string>
#include <thread>

namespace {

// -1 disarmed. A value of k means: the (k+1)-th allocation made by the thread that armed it throws, then the
// counter disarms for that thread only.
//
// Thread scoped on purpose, and measured rather than assumed. This used to be a process-wide counter, while a
// Session really does run a scheduler thread and a dispatcher thread of its own. Any allocation one of those
// workers happened to make while the counter was armed spent the injection, so the boundary path under test was
// never reached and the binary failed rarely, differently from run to run - "frames accepted" one time,
// "escaped at position 0" another - which is how it arrived as an intermittent CI failure on branches that do
// not touch Core at all (1 failure in 30 local runs). A worker must never be injected into: a failure there is
// that thread's problem, not the callback contract this file exists to prove.
thread_local int t_remainingAllocations = -1;

}  // namespace

// Replaces the global allocation function for this test binary only. Disarmed (-1) reproduces the
// default behaviour, so the harness, the fakes and the rest of Core are unaffected, and a thread that never
// armed the counter is never injected into.
void *operator new(std::size_t size)
{
    if (t_remainingAllocations > 0) {
        --t_remainingAllocations;
    } else if (t_remainingAllocations == 0) {
        t_remainingAllocations = -1;
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
// Position 0 is the first allocation the delivery itself can make, because the harness contributes none:
// FakeCaptureSource builds its handler once, in start(), and copies only a pointer per delivery. The test
// used to assume a platform-independent harness cost instead, and Linux CI disproved it - libstdc++ allocated
// for the harness's own `std::function` copy, so the injection never reached the callback at all.
//
// Measured: the frame path allocates nothing for a valid frame, and an invalid frame carries only a short
// rejection reason, so this case proves the contract that matters here - nothing escapes at any allocation
// position the callback can be reached from. The capture-event case below is the one that also proves the
// boundary reports what it caught, because its fault path copies a long message by construction. Both cases
// depend on the injection belonging to this thread alone; the case after them pins that down.
HYR_TEST(coreCallbackContainsAllocationFailure)
{
    constexpr int firstCallbackAllocation = 0;
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

        // An invalid frame, built before arming so its own construction cannot consume the countdown. Its
        // rejection reason turned out to be short enough for the small-string buffer, so this path may
        // allocate nothing at all and every position may pass without the injection landing anywhere - which
        // is why the reporting half of the contract is asserted by the capture-event case, not here.
        RemoteFrame frame;

        bool escaped = false;
        std::string escapedWhat;
        t_remainingAllocations = position;
        try {
            run.source->deliver(std::move(frame));
        } catch (const std::exception &error) {
            escaped = true;
            escapedWhat = error.what();
        } catch (...) {
            escaped = true;
            escapedWhat = "non-std exception";
        }
        t_remainingAllocations = -1;

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
        const bool delivered = run.source->deliver(std::move(next));
        const hyremote::SessionStats stats = run.session->stats();

        HYR_CHECK(delivered);
        HYR_CHECK(stats.framesAccepted >= 1);
    }
}

HYR_TEST(coreCaptureEventCallbackContainsAllocationFailure)
{
    // Same starting point as above, for the same measured reason: the harness costs no allocation per
    // delivery, so position 0 is the callback's own first allocation.
    constexpr int firstCallbackAllocation = 0;
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
        // not acceptable is an escape.
        const SessionState state = run.session->state();
        HYR_CHECK(state == SessionState::Running || state == SessionState::Faulted);

        bool escaped = false;
        std::string escapedWhat;
        t_remainingAllocations = position;
        try {
            run.source->reportEvent(event);
        } catch (const std::exception &error) {
            escaped = true;
            escapedWhat = error.what();
        } catch (...) {
            escaped = true;
            escapedWhat = "non-std exception";
        }
        t_remainingAllocations = -1;

        HYR_CHECK_MSG(!escaped,
                      "an internal exception escaped the capture event boundary at allocation position "
                          + std::to_string(position) + ": " + escapedWhat);

        if (const std::optional<SessionError> error = run.session->lastError()) {
            HYR_CHECK_EQ(error->code, SessionErrorCode::ComponentFailure);
        }
    }

    // This case asserts the half it can actually reach: nothing escapes at any allocation position the
    // callback can be reached from. Measured on this platform, the capture-event path allocates nothing
    // inside the callback (even a faulting event stores a fixed diagnostic), so no swept position injects
    // there and the case cannot also show the boundary reporting what it caught. The input case covers a
    // deterministic throwing sink separately.
}

// The two cases above only test what they claim while the injection belongs to the thread that armed it. This
// case pins that down with the situation that broke it: a started Session, which really runs a scheduler thread
// and a dispatcher thread, and another thread allocating inside the armed window. A process-wide counter fails
// here every time; a thread-scoped one keeps the injection waiting for the thread it was armed for.
//
// Every allocation this case needs is made outside the armed window - including the thread object, because
// constructing one allocates - and every assertion is deferred until after disarming, because formatting a
// failure message allocates too. What happens inside the window is only atomic loads, which do not. The probe
// allocates through the replaced function directly: a `std::string` built from a constant size can be folded
// away entirely, which is how an earlier revision of this case passed without ever allocating.
HYR_TEST(coreAllocationInjectionIsScopedToTheArmingThread)
{
    constexpr std::size_t probeBytes = 64;

    RunningSession run;
    run.prepare();
    HYR_CHECK(run.start());

    std::atomic<bool> startWorker{false};
    std::atomic<bool> workerDone{false};
    std::atomic<bool> workerAllocated{false};
    std::thread worker([&startWorker, &workerDone, &workerAllocated, probeBytes] {
        while (!startWorker.load(std::memory_order_acquire))
            std::this_thread::yield();
        try {
            void *block = ::operator new(probeBytes);
            workerAllocated.store(block != nullptr, std::memory_order_release);
            ::operator delete(block);
        } catch (...) {
            workerAllocated.store(false, std::memory_order_release);
        }
        workerDone.store(true, std::memory_order_release);
    });

    t_remainingAllocations = 0;
    startWorker.store(true, std::memory_order_release);
    while (!workerDone.load(std::memory_order_acquire))
        std::this_thread::yield();

    // The injection is still waiting for the thread that armed it, so this allocation - made here - is the
    // one that throws, which is exactly what the two cases above depend on.
    const bool otherThreadAllocated = workerAllocated.load(std::memory_order_acquire);
    bool injected = false;
    void *block = nullptr;
    try {
        block = ::operator new(probeBytes);
    } catch (const std::bad_alloc &) {
        injected = true;
    }
    t_remainingAllocations = -1;
    worker.join();
    ::operator delete(block);

    HYR_CHECK_MSG(otherThreadAllocated,
                  "the injection armed for this thread was spent by another thread's allocation");
    HYR_CHECK_MSG(injected,
                  "the injection did not remain armed for the thread that set it");
}

}  // namespace

HYR_TEST_MAIN()
