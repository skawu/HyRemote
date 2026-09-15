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

HYR_TEST_MAIN()
