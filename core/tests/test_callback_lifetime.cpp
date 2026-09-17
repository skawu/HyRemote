// B3/B4 - callback lifetime and the input boundary.
//
// Proves the frozen rule: `CaptureSource::stop()` / `Transport::stop()` must not return until their
// callbacks can no longer be invoked, and Core's own lifetime gate makes a late callback harmless
// even when an adapter violates that rule. Also covers the `InputSink::post()` failure policy.

#include "fakes.hpp"

using namespace hyremote;
using namespace hyremote::test;

HYR_TEST(conforming_adapters_stop_calling_back_once_stopped)
{
    RunningSession running;
    HYR_CHECK(running.start());

    running.session->stop();
    HYR_CHECK_EQ(running.session->state(), SessionState::Stopped);

    // The fakes model the quiescence rule: after stop() they do not call back any more.
    HYR_CHECK(!running.source->deliver(makeFrame(8, 8)));
    HYR_CHECK(!running.source->deliver(makeFrame(8, 8)));
    HYR_CHECK_EQ(running.session->stats().callbacksIgnoredAfterStop, std::uint64_t{0});
    HYR_CHECK_EQ(running.session->stats().framesArrivedAfterStop, std::uint64_t{0});
}

HYR_TEST(late_frame_and_event_callbacks_are_ignored)
{
    RunningSession running;
    HYR_CHECK(running.start());

    RemoteFrame frame = makeFrame(16, 8);
    HYR_CHECK(running.source->deliver(frame));
    HYR_CHECK(waitFor([&] { return running.session->stats().framesAccepted == 1; }));

    running.session->stop();
    const SessionStats before = running.session->stats();

    // An adapter that violates the quiescence rule: Core must ignore the callback instead of
    // touching a torn-down run.
    HYR_CHECK(running.source->forceDeliver(makeFrame(16, 8)));
    CaptureEvent event;
    event.code = CaptureEventCode::TargetLost;
    event.message = "late";
    event.recoverable = false;
    running.source->forceReportEvent(event);

    const SessionStats after = running.session->stats();
    HYR_CHECK_EQ(after.callbacksIgnoredAfterStop, std::uint64_t{2});
    // Nothing else moved: no frame was accepted, dropped or dispatched, and no event was counted
    // and no fault was raised.
    HYR_CHECK_EQ(after.framesAccepted, before.framesAccepted);
    HYR_CHECK_EQ(after.framesDroppedByPolicy, before.framesDroppedByPolicy);
    HYR_CHECK_EQ(after.framesDispatched, before.framesDispatched);
    HYR_CHECK_EQ(after.captureEvents, before.captureEvents);
    HYR_CHECK_EQ(running.session->state(), SessionState::Stopped);
}

HYR_TEST(late_input_callbacks_are_ignored)
{
    auto sink = std::make_shared<FakeInputSink>();

    RunningSession running;
    HYR_CHECK(running.start());
    running.setInputSink(sink);

    running.session->stop();

    InputEvent event;
    event.kind = InputEventKind::PointerMove;
    event.sourceViewport = InputViewport{16, 8, 1.0F};
    running.transport->forceDeliverInput(event);
    running.transport->forceReportEvent(TransportEvent{TransportEventCode::FatalFailure, "late"});

    const SessionStats after = running.session->stats();
    HYR_CHECK_EQ(after.callbacksIgnoredAfterStop, std::uint64_t{2});
    HYR_CHECK_EQ(sink->count(), std::size_t{0});
    HYR_CHECK_EQ(after.inputEventsPosted, std::uint64_t{0});
    HYR_CHECK_EQ(after.transportEvents, std::uint64_t{0});
    HYR_CHECK_EQ(running.session->state(), SessionState::Stopped);
}

HYR_TEST(late_callbacks_after_session_destruction_are_safe)
{
    // The adapter outlives the Session and keeps calling back: the gate makes this a no-op instead
    // of a use-after-free of the destroyed Session implementation.
    auto source = std::make_unique<FakeCaptureSource>();
    auto transport = std::make_unique<FakeTransport>();
    FakeCaptureSource *rawSource = source.get();
    FakeTransport *rawTransport = transport.get();

    {
        Session session;
        session.setCaptureSource(std::make_unique<LoaningCaptureSource>(rawSource));
        session.setTransport(std::make_unique<LoaningTransport>(rawTransport));
        HYR_CHECK(session.start());
        session.stop();

        // The gate is closed by stop(), so a delivery after stop() must not reach the mailbox. This is
        // observable, unlike the destruction case below.
        HYR_CHECK(rawSource->forceDeliver(makeFrame(8, 8)));
        HYR_CHECK_EQ(session.stats().framesDispatched, std::uint64_t{0});
    }  // ~Session destroys the implementation here; the backends are still owned by this test

    HYR_CHECK(rawSource->forceDeliver(makeFrame(8, 8)));
    rawSource->forceReportEvent(CaptureEvent{CaptureEventCode::TargetLost, "late", false});
    rawTransport->forceDeliverInput(InputEvent{});
    rawTransport->forceReportEvent(TransportEvent{TransportEventCode::FatalFailure, "late"});

    // Reaching this point without a crash is the primary assertion: the closed gate guarantees the late
    // callbacks never dereferenced the destroyed implementation. Nothing may have been started as a side
    // effect either, so the backend saw no further capture request.
    HYR_CHECK_EQ(rawSource->requestCalls(), 0);
}

HYR_TEST(a_throwing_input_sink_is_reported_but_not_fatal)
{
    auto sink = std::make_shared<FakeInputSink>();
    sink->throwFromPost = true;

    RunningSession running;
    HYR_CHECK(running.start());
    running.setInputSink(sink);

    InputEvent event;
    event.kind = InputEventKind::Key;
    event.key = KeyCode::A;
    event.pressed = true;

    // The exception must not unwind through the transport runtime.
    running.transport->deliverInput(event);

    const SessionStats stats = running.session->stats();
    HYR_CHECK_EQ(stats.inputPostFailures, std::uint64_t{1});
    HYR_CHECK_EQ(stats.inputEventsPosted, std::uint64_t{0});

    // Remote input is not on the capture path: the Session keeps running and reports the failure
    // as a recoverable error.
    HYR_CHECK_EQ(running.session->state(), SessionState::Running);
    const std::optional<SessionError> error = running.session->lastError();
    HYR_CHECK(error.has_value());
    HYR_CHECK(error->recoverable);

    // Frame delivery is unaffected.
    RemoteFrame frame = makeFrame(16, 8);
    HYR_CHECK(running.source->deliver(frame));
    HYR_CHECK(running.transport->waitForAccepted(1));
    HYR_CHECK_EQ(running.session->state(), SessionState::Running);
}

HYR_TEST(the_input_sink_may_be_replaced_at_runtime)
{
    auto first = std::make_shared<FakeInputSink>();
    auto second = std::make_shared<FakeInputSink>();
    const std::shared_ptr<int> firstDestroyed = first->destroyed;

    RunningSession running;
    HYR_CHECK(running.start());
    running.setInputSink(first);

    InputEvent event;
    event.kind = InputEventKind::PointerMove;
    event.sourceViewport = InputViewport{16, 8, 1.0F};
    event.x = 4.0F;
    event.y = 2.0F;
    running.transport->deliverInput(event);
    HYR_CHECK_EQ(first->count(), std::size_t{1});
    HYR_CHECK_EQ(*firstDestroyed, 0);

    // Documented contract: the sink is held through shared_ptr, so it may be swapped while running.
    running.setInputSink(second);
    running.transport->deliverInput(event);
    HYR_CHECK_EQ(second->count(), std::size_t{1});
    HYR_CHECK_EQ(first->count(), std::size_t{1});

    // The Session released its reference; this test still holds one, so nothing is destroyed yet.
    HYR_CHECK_EQ(*firstDestroyed, 0);
    first.reset();
    HYR_CHECK_EQ(*firstDestroyed, 1);
    HYR_CHECK_EQ(running.session->state(), SessionState::Running);
}

HYR_TEST_MAIN()
