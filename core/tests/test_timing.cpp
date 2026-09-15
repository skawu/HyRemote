// C3 - timestamp model.
//
// The rule under test: a capture request time is never implicitly promoted to a content PTS,
// completion time is the safe v0.1 baseline, a backend-supplied Render/Presentation PTS wins, and
// FrameId follows Core acceptance order rather than request order.

#include "fakes.hpp"

using namespace hyremote;
using namespace hyremote::test;

HYR_TEST(request_time_is_never_promoted_to_a_content_pts)
{
    RemoteFrame frame = makeFrame();
    frame.timing.requestTime = Clock::now();
    frame.timing.completionTime.reset();
    frame.timing.pts = TimePoint{};
    frame.timing.ptsSource = PtsSource::Completion;

    std::string reason;
    HYR_CHECK(!normalizeFrameTiming(frame, &reason));
    HYR_CHECK(reason.find("request") != std::string::npos);

    RunningSession running;
    HYR_CHECK(running.start());
    HYR_CHECK(running.source->deliver(frame));

    HYR_CHECK(waitFor([&] { return running.session->stats().framesRejectedTiming == 1; }));
    HYR_CHECK_EQ(running.session->stats().framesAccepted, 0);
    HYR_CHECK_EQ(running.session->stats().framesRejectedInvalid, 0);
    HYR_CHECK_EQ(running.transport->accepted(), 0);

    // A frame without a timestamp is a recoverable capture problem, not a Session fault.
    HYR_CHECK_EQ(running.session->state(), SessionState::Running);
}

HYR_TEST(completion_time_is_the_v01_pts_baseline)
{
    RunningSession running;
    HYR_CHECK(running.start());

    const TimePoint requestTime = Clock::now();
    const TimePoint completionTime = requestTime + std::chrono::milliseconds(4);

    RemoteFrame frame = makeFrame();
    frame.timing.requestTime = requestTime;
    frame.timing.completionTime = completionTime;
    frame.timing.pts = TimePoint{};  // backend observed no render/presentation time
    frame.timing.ptsSource = PtsSource::Completion;

    HYR_CHECK(running.source->deliver(frame));
    HYR_CHECK(running.transport->waitForAccepted(1));

    const RemoteFrame dispatched = running.transport->frames().at(0);
    HYR_CHECK(dispatched.timing.pts == completionTime);
    HYR_CHECK(dispatched.timing.ptsSource == PtsSource::Completion);
    HYR_CHECK(dispatched.timing.completionTime.has_value());
    HYR_CHECK_EQ(dispatched.timing.completionTime.value(), completionTime);
    HYR_CHECK(dispatched.timing.pts != requestTime);
}

HYR_TEST(backend_render_and_presentation_pts_are_preserved)
{
    RunningSession running;
    HYR_CHECK(running.start());

    const TimePoint requestTime = Clock::now();
    const TimePoint renderTime = requestTime + std::chrono::milliseconds(2);
    const TimePoint completionTime = requestTime + std::chrono::milliseconds(30);

    RemoteFrame renderFrame = makeFrame();
    renderFrame.timing.requestTime = requestTime;
    renderFrame.timing.completionTime = completionTime;
    renderFrame.timing.pts = renderTime;
    renderFrame.timing.ptsSource = PtsSource::Render;
    HYR_CHECK(running.source->deliver(renderFrame));
    HYR_CHECK(running.transport->waitForAccepted(1));

    const RemoteFrame renderDispatched = running.transport->frames().at(0);
    // The more precise backend timestamp wins over completion time ...
    HYR_CHECK(renderDispatched.timing.pts == renderTime);
    HYR_CHECK(renderDispatched.timing.ptsSource == PtsSource::Render);
    // ... and the diagnostic completion time is still recorded.
    HYR_CHECK_EQ(renderDispatched.timing.completionTime.value(), completionTime);

    const TimePoint presentationTime = requestTime + std::chrono::milliseconds(28);
    RemoteFrame presentationFrame = makeFrame();
    presentationFrame.timing.completionTime = completionTime;
    presentationFrame.timing.pts = presentationTime;
    presentationFrame.timing.ptsSource = PtsSource::Presentation;
    HYR_CHECK(running.source->deliver(presentationFrame));
    HYR_CHECK(running.transport->waitForAccepted(2));

    const RemoteFrame presentationDispatched = running.transport->frames().at(1);
    HYR_CHECK(presentationDispatched.timing.pts == presentationTime);
    HYR_CHECK(presentationDispatched.timing.ptsSource == PtsSource::Presentation);
}

HYR_TEST(a_declared_backend_pts_source_without_a_value_is_rejected)
{
    RemoteFrame frame = makeFrame();
    frame.timing.ptsSource = PtsSource::Render;
    frame.timing.pts = TimePoint{};
    frame.timing.completionTime = Clock::now();

    std::string reason;
    HYR_CHECK(!normalizeFrameTiming(frame, &reason));
    HYR_CHECK(reason.find("Render") != std::string::npos);

    // Core refuses rather than silently relabelling the source as Completion.
    HYR_CHECK(frame.timing.ptsSource == PtsSource::Render);

    RunningSession running;
    HYR_CHECK(running.start());
    HYR_CHECK(running.source->deliver(frame));
    HYR_CHECK(waitFor([&] { return running.session->stats().framesRejectedTiming == 1; }));
    HYR_CHECK_EQ(running.session->stats().framesAccepted, 0);
}

HYR_TEST(request_and_completion_diagnostics_stay_separately_observable)
{
    RunningSession running;
    HYR_CHECK(running.start());

    const TimePoint requestTime = Clock::now();
    const TimePoint completionTime = requestTime + std::chrono::milliseconds(25);

    RemoteFrame frame = makeFrame();
    frame.timing.requestTime = requestTime;
    frame.timing.completionTime = completionTime;
    frame.timing.ptsSource = PtsSource::Completion;

    HYR_CHECK(running.source->deliver(frame));
    HYR_CHECK(running.transport->waitForAccepted(1));

    const RemoteFrame dispatched = running.transport->frames().at(0);
    HYR_CHECK(dispatched.timing.requestTime.has_value());
    HYR_CHECK_EQ(dispatched.timing.requestTime.value(), requestTime);

    const std::optional<std::chrono::nanoseconds> latency = requestToCompletionLatency(dispatched);
    HYR_CHECK(latency.has_value());
    HYR_CHECK_EQ(latency->count(),
                 std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(25)).count());

    // The PTS is the completion time; the request time stays a diagnostic.
    HYR_CHECK(dispatched.timing.pts == completionTime);
    HYR_CHECK(dispatched.timing.pts != requestTime);
}

HYR_TEST(latency_diagnostics_are_absent_when_not_supplied)
{
    RemoteFrame withoutRequest = makeFrame();
    HYR_CHECK(!requestToCompletionLatency(withoutRequest).has_value());

    RemoteFrame withoutCompletion = makeFrame();
    withoutCompletion.timing.completionTime.reset();
    withoutCompletion.timing.requestTime = Clock::now();
    HYR_CHECK(!requestToCompletionLatency(withoutCompletion).has_value());
}

HYR_TEST(frame_id_follows_acceptance_order_not_request_order)
{
    RunningSession running;
    HYR_CHECK(running.start());

    // Two completions arrive in the opposite order of the requests that produced them.
    RemoteFrame firstAccepted = makeFrame(8, 8);
    firstAccepted.requestId = 7;
    firstAccepted.timing.requestTime = Clock::now();
    firstAccepted.timing.completionTime = Clock::now();
    HYR_CHECK(running.source->deliver(firstAccepted));
    HYR_CHECK(running.transport->waitForAccepted(1));

    RemoteFrame secondAccepted = makeFrame(8, 8);
    secondAccepted.requestId = 3;
    secondAccepted.timing.requestTime = Clock::now();
    secondAccepted.timing.completionTime = Clock::now();
    HYR_CHECK(running.source->deliver(secondAccepted));
    HYR_CHECK(running.transport->waitForAccepted(2));

    const std::vector<RemoteFrame> frames = running.transport->frames();
    HYR_CHECK_EQ(frames.size(), std::size_t{2});

    // Acceptance order, not request order: ids are monotonic in the order Core accepted them.
    HYR_CHECK_EQ(frames[0].id, FrameId{1});
    HYR_CHECK_EQ(frames[1].id, FrameId{2});
    HYR_CHECK(frames[1].id > frames[0].id);
    HYR_CHECK_EQ(frames[0].requestId.value(), CaptureRequestId{7});
    HYR_CHECK_EQ(frames[1].requestId.value(), CaptureRequestId{3});
    HYR_CHECK_EQ(running.session->stats().lastFrameId, FrameId{2});
}

HYR_TEST_MAIN()
