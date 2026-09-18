// SPDX-License-Identifier: Apache-2.0
// C6 - Session lifecycle, failure escalation and deterministic stop.
//
// The state sequence under test is:
//   Stopped -> Starting -> Running -> Stopping -> Stopped
//   Starting -> Faulted, and Faulted -> Stopping -> Stopped for cleanup.

#include "fakes.hpp"

using namespace hyremote;
using namespace hyremote::test;

HYR_TEST(the_session_follows_the_documented_state_sequence)
{
    RunningSession running;
    running.prepare();
    HYR_CHECK(running.session->state() == SessionState::Stopped);

    // The capture source observes the state from inside start(): it must still be Starting, and
    // the transport must not have been started yet.
    SessionState stateDuringCaptureStart = SessionState::Stopped;
    SessionState stateDuringTransportStart = SessionState::Faulted;
    SessionState stateDuringStop = SessionState::Stopped;
    running.source->onStart = [&] { stateDuringCaptureStart = running.session->state(); };
    running.transport->onStart = [&] { stateDuringTransportStart = running.session->state(); };
    running.transport->onStop = [&] { stateDuringStop = running.session->state(); };

    HYR_CHECK(running.session->start());
    HYR_CHECK(stateDuringCaptureStart == SessionState::Starting);
    HYR_CHECK(stateDuringTransportStart == SessionState::Starting);
    HYR_CHECK(running.session->state() == SessionState::Running);

    running.session->stop();
    HYR_CHECK(stateDuringStop == SessionState::Stopping);
    HYR_CHECK(running.session->state() == SessionState::Stopped);
}

HYR_TEST(repeated_start_stop_is_deterministic)
{
    RunningSession running;
    running.prepare();

    for (int cycle = 0; cycle < 3; ++cycle) {
        HYR_CHECK(running.session->start());
        HYR_CHECK(running.session->state() == SessionState::Running);

        // Per-run observability is reset with each start().
        const SessionStats initial = running.session->stats();
        HYR_CHECK_EQ(initial.framesAccepted, std::uint64_t{0});
        HYR_CHECK_EQ(initial.framesDispatched, std::uint64_t{0});
        HYR_CHECK_EQ(initial.framesDroppedByPolicy, std::uint64_t{0});

        RemoteFrame frame = makeFrame(16, 8);
        HYR_CHECK(running.source->deliver(frame));
        HYR_CHECK(running.transport->waitForAccepted(static_cast<std::size_t>(cycle) + 1));

        running.session->stop();
        HYR_CHECK(running.session->state() == SessionState::Stopped);
        HYR_CHECK_EQ(running.session->stats().framesDispatched, std::uint64_t{1});
    }

    // Every cycle stopped the capture source exactly once; nothing leaked across runs.
    HYR_CHECK_EQ(running.source->stopCalls(), 3);
    HYR_CHECK_EQ(running.transport->stopCalls(), 3);
}

HYR_TEST(a_capture_start_failure_faults_before_running)
{
    RunningSession running;
    running.prepare();
    running.source->startResult = false;

    HYR_CHECK(!running.session->start());
    HYR_CHECK(running.session->state() == SessionState::Faulted);

    const std::optional<SessionError> error = running.session->lastError();
    HYR_CHECK(error.has_value());
    HYR_CHECK(error->code == SessionErrorCode::CaptureStartFailed);
    HYR_CHECK(!error->recoverable);

    // The transport was never started, and cleanup still reaches Stopped.
    HYR_CHECK(running.transport->events().empty());
    HYR_CHECK_EQ(running.transport->stopCalls(), 0);

    running.session->stop();
    HYR_CHECK(running.session->state() == SessionState::Stopped);
}

HYR_TEST(a_transport_start_failure_faults_and_cleans_the_capture_source)
{
    RunningSession running;
    running.prepare();
    running.transport->startResult = false;

    HYR_CHECK(!running.session->start());
    HYR_CHECK(running.session->state() == SessionState::Faulted);

    const std::optional<SessionError> error = running.session->lastError();
    HYR_CHECK(error.has_value());
    HYR_CHECK(error->code == SessionErrorCode::TransportStartFailed);

    // Partial-start cleanup stopped the capture source that had started, and the transport that
    // failed to start is not asked to stop.
    HYR_CHECK_EQ(running.source->stopCalls(), 1);
    HYR_CHECK_EQ(running.transport->stopCalls(), 0);

    running.session->stop();
    HYR_CHECK(running.session->state() == SessionState::Stopped);
}

HYR_TEST(recoverable_capture_events_do_not_fault_the_session)
{
    RunningSession running;
    HYR_CHECK(running.start());

    CaptureEvent event;
    event.code = CaptureEventCode::TemporarilyUnavailable;
    event.message = "target window is not visible";
    event.recoverable = true;
    running.source->reportEvent(event);

    HYR_CHECK(waitFor([&] { return running.session->stats().captureEvents == 1; }));
    HYR_CHECK_EQ(running.session->state(), SessionState::Running);
    HYR_CHECK_EQ(running.session->stats().captureEventsNonRecoverable, std::uint64_t{0});
    HYR_CHECK(!running.session->lastError().has_value());
}

HYR_TEST(a_non_recoverable_capture_event_faults_the_session)
{
    RunningSession running;
    HYR_CHECK(running.start());

    CaptureEvent event;
    event.code = CaptureEventCode::TargetLost;
    event.message = "target permanently gone";
    event.recoverable = false;
    running.source->reportEvent(event);

    HYR_CHECK(waitFor([&] { return running.session->state() == SessionState::Faulted; }));
    const std::optional<SessionError> error = running.session->lastError();
    HYR_CHECK(error.has_value());
    HYR_CHECK(error->code == SessionErrorCode::TargetLost);

    running.session->stop();
    HYR_CHECK(running.session->state() == SessionState::Stopped);
}

HYR_TEST(transport_events_escalate_only_when_fatal)
{
    RunningSession running;
    HYR_CHECK(running.start());

    TransportEvent recoverable;
    recoverable.code = TransportEventCode::ClientDisconnected;
    recoverable.message = "client left";
    running.transport->reportEvent(recoverable);

    HYR_CHECK(waitFor([&] { return running.session->stats().transportEvents == 1; }));
    HYR_CHECK_EQ(running.session->state(), SessionState::Running);

    TransportEvent fatal;
    fatal.code = TransportEventCode::FatalFailure;
    fatal.message = "runtime cannot listen";
    running.transport->reportEvent(fatal);

    HYR_CHECK(waitFor([&] { return running.session->state() == SessionState::Faulted; }));
    HYR_CHECK_EQ(running.session->stats().transportEventsFatal, std::uint64_t{1});
}

HYR_TEST(configuration_and_component_errors_fail_before_starting)
{
    {
        SessionConfig config;
        config.frameQueue.capacity = 0;

        RunningSession running;
        running.prepare(config);
        HYR_CHECK(!running.session->start());
        HYR_CHECK(running.session->state() == SessionState::Stopped);
        HYR_CHECK(running.session->lastError().value().code == SessionErrorCode::InvalidConfiguration);
    }

    {
        SessionConfig config;
        config.capture.maxInFlight = 0;

        RunningSession running;
        running.prepare(config);
        HYR_CHECK(!running.session->start());
        HYR_CHECK(running.session->state() == SessionState::Stopped);
        HYR_CHECK(running.session->lastError().value().code == SessionErrorCode::InvalidConfiguration);
    }

    {
        // No components installed at all.
        Session session;
        HYR_CHECK(!session.start());
        HYR_CHECK(session.state() == SessionState::Stopped);
        HYR_CHECK(session.lastError().value().code == SessionErrorCode::InvalidConfiguration);
    }

    // The default configuration is valid; an unusable one explains itself.
    HYR_CHECK(validateSessionConfig(SessionConfig{}).empty());
    SessionConfig invalid;
    invalid.capture.targetFramesPerSecond = 0.0;
    HYR_CHECK(!validateSessionConfig(invalid).empty());
}

HYR_TEST(start_requires_the_stopped_state_and_stop_is_idempotent)
{
    RunningSession running;
    HYR_CHECK(running.start());

    HYR_CHECK(!running.session->start());
    HYR_CHECK(running.session->state() == SessionState::Running);
    HYR_CHECK(running.session->lastError().has_value());

    running.session->stop();
    HYR_CHECK(running.session->state() == SessionState::Stopped);

    running.session->stop();  // idempotent
    HYR_CHECK(running.session->state() == SessionState::Stopped);
    HYR_CHECK_EQ(running.source->stopCalls(), 1);
}

HYR_TEST(the_capture_in_flight_bound_is_respected)
{
    SessionConfig config;
    config.capture.maxInFlight = 2;
    config.frameQueue.capacity = 2;

    RunningSession running;
    HYR_CHECK(running.start(config));

    // The fake source never completes on its own, so the scheduler must stop at the bound.
    HYR_CHECK(waitFor([&] { return running.source->requestCalls() >= 2; }));
    HYR_CHECK(waitFor([&] { return running.session->stats().maxInFlightObserved >= 2; }));

    const std::size_t observed = running.source->maxOutstanding();
    HYR_CHECK_MSG(observed <= 2, "source saw more concurrent requests than the in-flight bound: "
                                     + std::to_string(observed));
    HYR_CHECK_EQ(running.source->requestCalls(), std::size_t{2});
    HYR_CHECK_EQ(running.session->stats().maxInFlightObserved, std::size_t{2});

    // Completing one request frees exactly one slot.
    RemoteFrame frame = makeFrame(16, 8);
    HYR_CHECK(running.source->deliver(frame));
    HYR_CHECK(waitFor([&] { return running.source->requestCalls() >= 3; }));
    HYR_CHECK(running.source->maxOutstanding() <= 2);
}

HYR_TEST(a_rejected_request_is_counted_and_retried)
{
    SessionConfig config;
    config.capture.maxInFlight = 1;
    config.capture.rejectedRequestRetryDelay = std::chrono::milliseconds(1);

    RunningSession running;
    HYR_CHECK(running.start(config));
    HYR_CHECK(waitFor([&] { return running.source->requestCalls() >= 1; }));

    // The backend starts refusing requests while one is still outstanding, then the free slot
    // makes the scheduler retry - and get refused.
    running.source->setRejectRequests(true);
    RemoteFrame frame = makeFrame(16, 8);
    HYR_CHECK(running.source->deliver(frame));
    HYR_CHECK(waitFor([&] { return running.session->stats().captureRequestsRejected >= 1; }));
    HYR_CHECK_EQ(running.session->state(), SessionState::Running);

    // The retry loop keeps trying, so the Session recovers as soon as the backend accepts again.
    const std::size_t successfulBefore = running.source->requestCalls();
    running.source->setRejectRequests(false);
    HYR_CHECK(waitFor([&] { return running.source->requestCalls() > successfulBefore; }));
    HYR_CHECK_EQ(running.source->maxOutstanding(), std::size_t{1});
    HYR_CHECK_EQ(running.session->state(), SessionState::Running);
}

HYR_TEST(a_backend_exception_does_not_terminate_the_host_process)
{
    SessionConfig config;
    config.capture.maxInFlight = 1;
    config.capture.rejectedRequestRetryDelay = std::chrono::milliseconds(1);

    RunningSession running;
    HYR_CHECK(running.start(config));
    HYR_CHECK(waitFor([&] { return running.source->requestCalls() >= 1; }));

    // A backend bug on the capture request path must surface as a fault, never as a doomed
    // Core-owned thread.
    running.source->throwFromRequestFrame = true;
    RemoteFrame frame = makeFrame(8, 8);
    HYR_CHECK(running.source->deliver(frame));

    HYR_CHECK(waitFor([&] { return running.session->state() == SessionState::Faulted; }));
    const std::optional<SessionError> error = running.session->lastError();
    HYR_CHECK(error.has_value());
    HYR_CHECK(error->message.find("requestFrame") != std::string::npos);
    HYR_CHECK(running.session->stats().captureRequestsRejected >= 1);

    running.session->stop();
    HYR_CHECK_EQ(running.session->state(), SessionState::Stopped);
}

HYR_TEST(a_transport_exception_does_not_terminate_the_host_process)
{
    RunningSession running;
    HYR_CHECK(running.start());

    running.transport->throwFromEnqueue = true;
    RemoteFrame frame = makeFrame(8, 8);
    HYR_CHECK(running.source->deliver(frame));

    HYR_CHECK(waitFor([&] { return running.session->stats().transportEnqueueFailures == 1; }));
    HYR_CHECK(waitFor([&] { return running.session->state() == SessionState::Faulted; }));
    const std::optional<SessionError> error = running.session->lastError();
    HYR_CHECK(error.has_value());
    HYR_CHECK(error->message.find("enqueueFrame") != std::string::npos);

    running.session->stop();
    HYR_CHECK_EQ(running.session->state(), SessionState::Stopped);
    HYR_CHECK_EQ(running.transport->stopCalls(), 1);
}

// --- R2-B1: teardown ownership ---------------------------------------------------------------

HYR_TEST(r2b1_concurrent_stop_callers_produce_exactly_one_teardown)
{
    constexpr std::size_t kCallers = 4;

    RunningSession running;
    HYR_CHECK(running.start());
    HYR_CHECK(waitFor([&] { return running.session->stats().workersStarted == 2; }));

    TestBarrier barrier(kCallers);
    std::atomic<int> returned{0};
    std::vector<std::thread> callers;
    callers.reserve(kCallers);
    for (std::size_t i = 0; i < kCallers; ++i) {
        callers.emplace_back([&] {
            barrier.wait();  // release every stop() caller at the same instant
            running.session->stop();
            returned.fetch_add(1);
        });
    }
    for (std::thread &caller : callers)
        caller.join();

    // Every caller returned, and the ordered teardown ran exactly once: the claim is taken
    // atomically under the mutex, so the other callers wait for it instead of racing on the same
    // component stop / worker join sequence.
    HYR_CHECK_EQ(returned.load(), int{kCallers});
    HYR_CHECK_EQ(running.session->stats().teardownsPerformed, std::uint64_t{1});
    HYR_CHECK_EQ(running.session->state(), SessionState::Stopped);
    HYR_CHECK_EQ(running.source->stopCalls(), 1);
    HYR_CHECK_EQ(running.transport->stopCalls(), 1);

    // Nothing was destroyed, closed twice or joined twice while the callers ran.
    HYR_CHECK_EQ(*running.source->destroyed, 0);
    HYR_CHECK_EQ(*running.transport->destroyed, 0);

    // And the Session is still usable afterwards.
    HYR_CHECK(running.session->setCaptureSource(std::make_unique<FakeCaptureSource>()));
    HYR_CHECK(running.session->setTransport(std::make_unique<FakeTransport>()));
    HYR_CHECK(running.session->start());
    HYR_CHECK_EQ(running.session->state(), SessionState::Running);
    running.session->stop();
    HYR_CHECK_EQ(running.session->state(), SessionState::Stopped);
}

// --- R2-B2: stop() during Starting cancels the in-progress start -----------------------------

HYR_TEST(r2b2_stop_while_the_capture_source_start_is_in_progress)
{
    EnqueueGate gate;
    gate.close();

    RunningSession running;
    running.prepare();
    running.source->startGate = &gate;

    bool started = true;
    std::thread starter([&] { started = running.session->start(); });

    HYR_CHECK(gate.waitForEntered(1));
    HYR_CHECK_EQ(running.session->state(), SessionState::Starting);

    // stop() must be able to win while the external start is still in progress.
    running.session->stop();
    HYR_CHECK_EQ(running.session->state(), SessionState::Stopped);

    gate.open();
    starter.join();

    // The in-progress start() noticed the cancellation: it returns false, it never publishes
    // `Running`, and it stops the component that finished starting after the cancellation.
    HYR_CHECK(!started);
    HYR_CHECK_EQ(running.session->state(), SessionState::Stopped);
    HYR_CHECK_EQ(running.source->stopCalls(), 1);
    HYR_CHECK_EQ(running.transport->stopCalls(), 0);
    HYR_CHECK(running.transport->events().empty());
    HYR_CHECK_EQ(running.source->requestCalls(), 0);
    HYR_CHECK_EQ(running.session->stats().workersStarted, 0);
    HYR_CHECK_EQ(running.session->stats().teardownsPerformed, 1);
    HYR_CHECK(running.session->lastError().has_value());
    HYR_CHECK(running.session->lastError().value().code == SessionErrorCode::StartCancelled);

    // The Session remains usable: a later start/stop cycle works normally.
    HYR_CHECK(running.session->start());
    HYR_CHECK_EQ(running.session->state(), SessionState::Running);
    running.session->stop();
    HYR_CHECK_EQ(running.session->state(), SessionState::Stopped);
    HYR_CHECK_EQ(running.session->stats().teardownsPerformed, 1);
}

HYR_TEST(r2b2_stop_while_the_transport_start_is_in_progress)
{
    EnqueueGate gate;
    gate.close();

    RunningSession running;
    running.prepare();
    running.transport->startGate = &gate;

    bool started = true;
    std::thread starter([&] { started = running.session->start(); });

    HYR_CHECK(gate.waitForEntered(1));
    HYR_CHECK_EQ(running.session->state(), SessionState::Starting);

    // The workers exist and are waiting in the startup handshake; no request may be issued yet.
    HYR_CHECK(waitFor([&] { return running.session->stats().workersStarted == 2; }));
    HYR_CHECK_EQ(running.source->requestCalls(), 0);

    // stop() wins while the transport start is still in progress: it stops the capture source and
    // joins the workers it already owns.
    running.session->stop();
    HYR_CHECK_EQ(running.session->state(), SessionState::Stopped);

    gate.open();
    starter.join();

    HYR_CHECK(!started);
    HYR_CHECK_EQ(running.session->state(), SessionState::Stopped);
    // The source was stopped once (by the teardown); the transport, which finished starting after
    // the cancellation, was stopped once by the cancelled start().
    HYR_CHECK_EQ(running.source->stopCalls(), 1);
    HYR_CHECK_EQ(running.transport->stopCalls(), 1);
    HYR_CHECK_EQ(running.session->stats().teardownsPerformed, 1);
    HYR_CHECK_EQ(running.source->requestCalls(), 0);
    HYR_CHECK_EQ(running.session->stats().framesDispatched, std::uint64_t{0});
    HYR_CHECK(running.session->lastError().value().code == SessionErrorCode::StartCancelled);
    HYR_CHECK_EQ(*running.source->destroyed, 0);
    HYR_CHECK_EQ(*running.transport->destroyed, 0);

    // A later cycle works, and the cancelled run left no worker behind.
    HYR_CHECK(running.session->start());
    HYR_CHECK_EQ(running.session->state(), SessionState::Running);
    HYR_CHECK(waitFor([&] { return running.source->requestCalls() >= 1; }));
    running.session->stop();
    HYR_CHECK_EQ(running.session->state(), SessionState::Stopped);
}

// --- R3-B1: no mutex re-entry while a start() reports its cancellation -------------------------

namespace {

// Records whether the Session was ever observed `Running`, so "start() never publishes `Running`
// after a stop() won" is checked by observation and not only inferred from the final state.
class RunningWatcher
{
public:
    explicit RunningWatcher(Session &session)
        : m_session(session)
        , m_thread([this] {
            while (!m_stop) {
                if (m_session.state() == SessionState::Running)
                    m_sawRunning = true;
                std::this_thread::sleep_for(std::chrono::microseconds(200));
            }
        })
    {
    }

    ~RunningWatcher() { stop(); }

    void stop()
    {
        m_stop = true;
        if (m_thread.joinable())
            m_thread.join();
    }

    bool sawRunning()
    {
        stop();
        return m_sawRunning.load();
    }

private:
    Session &m_session;
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_sawRunning{false};
    std::thread m_thread;
};

// Runs `action` on a separate thread and returns only once it has completed, so a test can inject a
// stop() at a deterministic point from inside a gated component start.
bool stopOnSeparateThreadAndWait(Session &session, std::thread &thread)
{
    std::atomic<bool> returned{false};
    thread = std::thread([&session, &returned] {
        session.stop();
        returned.store(true);
    });
    return waitFor([&returned] { return returned.load(); });
}

}  // namespace

HYR_TEST(r3b1_stop_wins_at_the_earliest_moment_after_starting_is_published)
{
    // Shape (a) of the R3 review: the stop wins at the earliest moment an external thread can act,
    // i.e. immediately after `Starting` is published. The callback gate is installed in the same
    // critical section as the run claim, so "while the gate installation is committed" is no longer
    // a separate window (there is no helper that could re-enter the mutex there either); this test
    // pins the observable consequences of a stop() winning at that point.
    RunningSession running;
    running.prepare();

    std::atomic<bool> rendezvousOk{false};
    std::thread stopper;
    running.source->onStart = [&] {
        // Runs on the Session's start() thread, inside CaptureSource::start(), with no Session lock
        // held. The stop() is performed by a normal external thread and this hook waits for the whole
        // teardown to complete before the component reports "started", so the interleaving is
        // deterministic: the stop has definitely won before the source start returns.
        rendezvousOk.store(stopOnSeparateThreadAndWait(*running.session, stopper));
    };

    RunningWatcher watcher(*running.session);
    const bool started = running.session->start();
    stopper.join();

    HYR_CHECK(rendezvousOk.load());
    HYR_CHECK(!started);
    HYR_CHECK(!watcher.sawRunning());  // no later transition to `Running`
    HYR_CHECK_EQ(running.session->state(), SessionState::Stopped);
    HYR_CHECK_EQ(running.session->stats().teardownsPerformed, std::uint64_t{1});
    HYR_CHECK_EQ(running.source->stopCalls(), 1);
    HYR_CHECK_EQ(running.transport->stopCalls(), 0);
    HYR_CHECK(running.transport->events().empty());
    HYR_CHECK_EQ(running.source->requestCalls(), 0);
    HYR_CHECK_EQ(running.session->stats().workersStarted, 0);
    HYR_CHECK(running.session->lastError().has_value());
    HYR_CHECK(running.session->lastError().value().code == SessionErrorCode::StartCancelled);

    // A subsequent fresh start/stop still works: the cancelled run left no lock held and no state
    // behind.
    running.source->onStart = nullptr;
    HYR_CHECK(running.session->start());
    HYR_CHECK_EQ(running.session->state(), SessionState::Running);
    running.session->stop();
    HYR_CHECK_EQ(running.session->state(), SessionState::Stopped);
    HYR_CHECK_EQ(running.session->stats().teardownsPerformed, std::uint64_t{1});
}

HYR_TEST(r3b1_stop_wins_after_the_transport_startup_commits_before_running)
{
    // Shape (b) of the R3 review: the transport has already started but has not returned to the
    // Session yet. The transport commit and the `Running` publication are one critical section, so
    // this is the deterministic injection point: the teardown completes before `Transport::start()`
    // returns, which guarantees that the Session's commit section sees a cancelled run and therefore
    // must not publish `Running`.
    RunningSession running;
    running.prepare();

    std::atomic<bool> rendezvousOk{false};
    std::thread stopper;
    running.transport->onStart = [&] {
        // Runs on the Session's start() thread, inside Transport::start(), after the transport has
        // been marked started and with no Session lock held.
        rendezvousOk.store(stopOnSeparateThreadAndWait(*running.session, stopper));
    };

    RunningWatcher watcher(*running.session);
    const bool started = running.session->start();
    stopper.join();

    HYR_CHECK(rendezvousOk.load());
    HYR_CHECK(!started);
    HYR_CHECK(!watcher.sawRunning());  // no later transition to `Running`
    HYR_CHECK_EQ(running.session->state(), SessionState::Stopped);
    HYR_CHECK_EQ(running.session->stats().teardownsPerformed, std::uint64_t{1});
    // The teardown stopped the capture source (it had been committed); the transport finished
    // starting after the cancellation, so the cancelled start() stops it. Each component exactly
    // once, and no component is destroyed while it is still live.
    HYR_CHECK_EQ(running.source->stopCalls(), 1);
    HYR_CHECK_EQ(running.transport->stopCalls(), 1);
    HYR_CHECK_EQ(running.source->requestCalls(), 0);
    HYR_CHECK_EQ(running.session->stats().framesDispatched, std::uint64_t{0});
    HYR_CHECK(running.session->lastError().value().code == SessionErrorCode::StartCancelled);
    HYR_CHECK_EQ(*running.source->destroyed, 0);
    HYR_CHECK_EQ(*running.transport->destroyed, 0);

    running.transport->onStart = nullptr;
    HYR_CHECK(running.session->start());
    HYR_CHECK_EQ(running.session->state(), SessionState::Running);
    running.session->stop();
    HYR_CHECK_EQ(running.session->state(), SessionState::Stopped);
    HYR_CHECK_EQ(running.session->stats().teardownsPerformed, std::uint64_t{1});
}

HYR_TEST(r3b1_concurrent_start_and_stop_complete_without_deadlock)
{
    // Bounded race over the narrow windows a deterministic test cannot pin: `start()` and `stop()`
    // are released through a barrier while the start is between two critical sections, which is where
    // a stop() claims the run while the start() thread is about to re-acquire the mutex. A watchdog
    // turns a hypothetical deadlock (the R3-B1 failure mode) into a test failure instead of a hung
    // binary, and the fixture is intentionally leaked in that case because its destructor would block
    // on the mutex the deadlocked thread holds.
    constexpr int kIterations = 200;

    int stoppedByTheRacer = 0;
    int stoppedByTheTest = 0;
    for (int iteration = 0; iteration < kIterations; ++iteration) {
        auto running = std::make_unique<RunningSession>();
        running->prepare();

        TestBarrier barrier(2);
        std::atomic<bool> startReturned{false};
        std::atomic<bool> stopReturned{false};
        std::thread starter([&] {
            barrier.wait();
            running->session->start();
            startReturned.store(true);
        });
        std::thread stopper([&] {
            barrier.wait();
            running->session->stop();
            stopReturned.store(true);
        });

        const bool completed = waitFor([&] { return startReturned.load() && stopReturned.load(); },
                                       std::chrono::milliseconds(10000));
        if (!completed) {
            (void)running.release();
            starter.detach();
            stopper.detach();
            HYR_CHECK_MSG(false,
                          "concurrent start()/stop() did not complete within the watchdog: deadlock");
            return;
        }
        starter.join();
        stopper.join();

        if (running->session->state() == SessionState::Running) {
            // The start won the race and published `Running`; the owner then stops normally.
            ++stoppedByTheTest;
            running->session->stop();
        } else {
            // The stop won during startup and cancelled it, or it completed before the run was
            // claimed and the start then failed/never started.
            ++stoppedByTheRacer;
        }

        HYR_CHECK_EQ(running->session->state(), SessionState::Stopped);
        // Exactly one ordered teardown per run, whichever side won.
        HYR_CHECK_EQ(running->session->stats().teardownsPerformed, std::uint64_t{1});
    }

    // Both outcomes are legal; the assertion that matters is that every iteration completed.
    HYR_CHECK_EQ(stoppedByTheRacer + stoppedByTheTest, kIterations);
}

// --- B1: startup race regression -------------------------------------------------------------

HYR_TEST(b1_workers_created_during_starting_survive_the_running_publication)
{
    // Hold the transport startup: at that moment the Core workers already exist and the Session is
    // still `Starting`, which is exactly the window in which a fast worker used to exit for good.
    EnqueueGate gate;
    gate.close();

    RunningSession running;
    running.prepare();
    running.transport->startGate = &gate;

    bool started = false;
    std::thread starter([&] { started = running.session->start(); });

    HYR_CHECK(gate.waitForEntered(1));
    HYR_CHECK_EQ(running.session->state(), SessionState::Starting);

    // Both workers have provably begun while the Session is still `Starting`: this is the racy
    // window. With the previous implementation the scheduler decided to exit right here.
    HYR_CHECK(waitFor([&] { return running.session->stats().workersStarted == 2; }));
    HYR_CHECK_EQ(running.session->state(), SessionState::Starting);

    // No capture request is issued before `Running` is published.
    HYR_CHECK_EQ(running.source->requestCalls(), std::size_t{0});

    gate.open();
    starter.join();

    HYR_CHECK(started);
    HYR_CHECK_EQ(running.session->state(), SessionState::Running);

    // The scheduler is still alive: it issues requests now that `Running` is published. With the
    // racy implementation this never happens and the wait fails deterministically.
    HYR_CHECK(waitFor([&] { return running.source->requestCalls() >= 1; }));
    HYR_CHECK(waitFor([&] { return running.session->stats().maxInFlightObserved >= 1; }));

    running.stop();
    HYR_CHECK_EQ(running.session->state(), SessionState::Stopped);
}

// --- B2: component mutation rules ------------------------------------------------------------

HYR_TEST(b2_component_replacement_is_rejected_while_active)
{
    RunningSession running;
    HYR_CHECK(running.start());
    HYR_CHECK(waitFor([&] { return running.source->requestCalls() >= 1; }));

    const std::shared_ptr<int> sourceDestroyed = running.source->destroyed;
    const std::shared_ptr<int> transportDestroyed = running.transport->destroyed;

    auto replacementSource = std::make_unique<FakeCaptureSource>();
    const std::shared_ptr<int> replacementDestroyed = replacementSource->destroyed;
    HYR_CHECK(!running.session->setCaptureSource(std::move(replacementSource)));
    HYR_CHECK(!running.session->setTransport(std::make_unique<FakeTransport>()));

    // The active components are alive and unchanged; the refused replacement was released.
    HYR_CHECK_EQ(*sourceDestroyed, 0);
    HYR_CHECK_EQ(*transportDestroyed, 0);
    HYR_CHECK_EQ(*replacementDestroyed, 1);

    // The run keeps working with the original components.
    const std::size_t requestsBefore = running.source->requestCalls();
    RemoteFrame frame = makeFrame(16, 8);
    HYR_CHECK(running.source->deliver(frame));
    HYR_CHECK(running.transport->waitForAccepted(1));
    HYR_CHECK(waitFor([&] { return running.source->requestCalls() > requestsBefore; }));
    HYR_CHECK_EQ(running.session->state(), SessionState::Running);
}

HYR_TEST(b2_replacement_is_rejected_while_starting_and_while_faulted)
{
    {
        EnqueueGate gate;
        gate.close();

        RunningSession running;
        running.prepare();
        running.transport->startGate = &gate;

        bool started = false;
        std::thread starter([&] { started = running.session->start(); });
        HYR_CHECK(gate.waitForEntered(1));
        HYR_CHECK_EQ(running.session->state(), SessionState::Starting);

        HYR_CHECK(!running.session->setCaptureSource(std::make_unique<FakeCaptureSource>()));
        HYR_CHECK(!running.session->setTransport(std::make_unique<FakeTransport>()));
        HYR_CHECK_EQ(*running.source->destroyed, 0);
        HYR_CHECK_EQ(*running.transport->destroyed, 0);

        gate.open();
        starter.join();
        HYR_CHECK(started);
        running.stop();
    }

    {
        RunningSession running;
        HYR_CHECK(running.start());

        CaptureEvent event;
        event.code = CaptureEventCode::TargetLost;
        event.message = "gone";
        event.recoverable = false;
        running.source->reportEvent(event);
        HYR_CHECK(waitFor([&] { return running.session->state() == SessionState::Faulted; }));

        HYR_CHECK(!running.session->setCaptureSource(std::make_unique<FakeCaptureSource>()));
        HYR_CHECK(!running.session->setTransport(std::make_unique<FakeTransport>()));
        HYR_CHECK_EQ(*running.source->destroyed, 0);
        HYR_CHECK_EQ(*running.transport->destroyed, 0);

        running.session->stop();
        HYR_CHECK_EQ(running.session->state(), SessionState::Stopped);
    }
}

HYR_TEST(b2_replacement_is_rejected_while_stopping)
{
    RunningSession running;
    HYR_CHECK(running.start());

    SessionState stateDuringSourceStop = SessionState::Stopped;
    bool sourceRejected = false;
    bool transportRejected = false;

    running.source->onStop = [&] {
        stateDuringSourceStop = running.session->state();
        sourceRejected = !running.session->setCaptureSource(std::make_unique<FakeCaptureSource>());
    };
    running.transport->onStop = [&] {
        transportRejected = !running.session->setTransport(std::make_unique<FakeTransport>());
    };

    running.session->stop();

    HYR_CHECK(stateDuringSourceStop == SessionState::Stopping);
    HYR_CHECK(sourceRejected);
    HYR_CHECK(transportRejected);
    HYR_CHECK_EQ(running.session->state(), SessionState::Stopped);
}

// --- B4: plugin-boundary exceptions ----------------------------------------------------------

HYR_TEST(b4_a_throwing_capture_start_faults_and_cleans_up)
{
    RunningSession running;
    running.prepare();
    running.source->throwFromStart = true;

    HYR_CHECK(!running.session->start());
    HYR_CHECK_EQ(running.session->state(), SessionState::Faulted);

    const std::optional<SessionError> error = running.session->lastError();
    HYR_CHECK(error.has_value());
    HYR_CHECK(error->code == SessionErrorCode::CaptureStartFailed);
    HYR_CHECK(error->message.find("threw") != std::string::npos);

    // A throwing start may have left partial state, so the component is stopped again; the
    // transport was never started.
    HYR_CHECK_EQ(running.source->stopCalls(), 1);
    HYR_CHECK(running.transport->events().empty());

    running.session->stop();
    HYR_CHECK_EQ(running.session->state(), SessionState::Stopped);
}

HYR_TEST(b4_a_throwing_transport_start_stops_the_capture_source)
{
    RunningSession running;
    running.prepare();
    running.transport->throwFromStart = true;

    HYR_CHECK(!running.session->start());
    HYR_CHECK_EQ(running.session->state(), SessionState::Faulted);

    const std::optional<SessionError> error = running.session->lastError();
    HYR_CHECK(error.has_value());
    HYR_CHECK(error->code == SessionErrorCode::TransportStartFailed);
    HYR_CHECK(error->message.find("threw") != std::string::npos);

    // The capture source that did start is stopped again, and the throwing transport is stopped
    // once as well.
    HYR_CHECK_EQ(running.source->stopCalls(), 1);
    HYR_CHECK_EQ(running.transport->stopCalls(), 1);

    running.session->stop();
    HYR_CHECK_EQ(running.session->state(), SessionState::Stopped);
}

HYR_TEST(b4_a_throwing_capability_query_fails_before_starting)
{
    {
        RunningSession running;
        running.prepare();
        running.source->throwFromCapabilities = true;

        HYR_CHECK(!running.session->start());
        HYR_CHECK_EQ(running.session->state(), SessionState::Stopped);
        HYR_CHECK_EQ(running.source->stopCalls(), 0);

        const std::optional<SessionError> error = running.session->lastError();
        HYR_CHECK(error.has_value());
        HYR_CHECK(error->code == SessionErrorCode::ComponentFailure);
        HYR_CHECK(error->message.find("capability query") != std::string::npos);
        HYR_CHECK(running.transport->events().empty());
    }

    {
        RunningSession running;
        running.prepare();
        running.transport->throwFromCapabilities = true;

        HYR_CHECK(!running.session->start());
        HYR_CHECK_EQ(running.session->state(), SessionState::Stopped);
        HYR_CHECK_EQ(running.source->stopCalls(), 0);
        HYR_CHECK(running.session->lastError().value().code == SessionErrorCode::ComponentFailure);
    }
}

HYR_TEST_MAIN()
