// C5 - transport hand-off.
//
// Proves that the transport is only ever called from the Core dispatch path, that frame ownership
// is stable across that boundary, that a full or stalled transport cannot block the capture path,
// and that incompatible capabilities fail before the Session reaches Running.

#include "fakes.hpp"

using namespace hyremote;
using namespace hyremote::test;

HYR_TEST(enqueue_is_only_called_from_the_dispatch_worker)
{
    RunningSession running;
    HYR_CHECK(running.start());

    RemoteFrame frame = makeFrame(16, 8);
    HYR_CHECK(running.source->deliver(frame));  // completion on this (capture) thread
    HYR_CHECK(running.transport->waitForAccepted(1));

    const std::set<std::thread::id> enqueueThreads = running.transport->enqueueThreads();
    HYR_CHECK_EQ(enqueueThreads.size(), std::size_t{1});
    HYR_CHECK(*enqueueThreads.begin() != std::this_thread::get_id());
    HYR_CHECK_EQ(running.transport->enqueueCalls(), std::size_t{1});
}

HYR_TEST(the_transport_receives_stable_frame_ownership)
{
    RunningSession running;
    HYR_CHECK(running.start());

    TrackedFrame tracked = makeTrackedFrame();
    writePattern(tracked.frame, 0x3C);
    const std::shared_ptr<int> releases = tracked.releases;

    HYR_CHECK(running.source->deliver(tracked.frame));
    HYR_CHECK(running.transport->waitForAccepted(1));

    // The producer drops its own reference: the pixels must stay valid for the transport.
    tracked.frame = RemoteFrame{};
    HYR_CHECK_EQ(*releases, 0);
    {
        const std::vector<RemoteFrame> frames = running.transport->frames();
        HYR_CHECK_EQ(frames.size(), std::size_t{1});
        HYR_CHECK_EQ(readPattern(frames.at(0)), 0x3C);
    }
    HYR_CHECK_EQ(*releases, 0);

    // Only when the transport releases its frame does the storage go away - exactly once.
    running.transport->clearFrames();
    HYR_CHECK_EQ(*releases, 1);
}

HYR_TEST(a_full_transport_runtime_queue_does_not_block_core)
{
    RunningSession running;
    HYR_CHECK(running.start());
    running.transport->setRuntimeQueueCapacity(4);

    // One frame at a time, waiting for each hand-off: every frame reaches the transport, so the
    // test measures the transport's own overflow policy rather than the Core mailbox policy.
    constexpr int kFrames = 6;
    for (int i = 0; i < kFrames; ++i) {
        RemoteFrame frame = makeFrame(16, 8);
        HYR_CHECK(running.source->deliver(frame));
        HYR_CHECK(waitFor([&] {
            return running.session->stats().framesDispatched >= static_cast<std::uint64_t>(i + 1);
        }));
    }

    // The transport's runtime queue is full and it applies its own local policy; Core stays
    // Running and never performs transport work on the capture path.
    HYR_CHECK_EQ(running.transport->enqueueCalls(), std::size_t{kFrames});
    HYR_CHECK_EQ(running.transport->accepted(), std::size_t{4});
    HYR_CHECK_EQ(running.transport->droppedByRuntimeQueue(), std::size_t{2});
    HYR_CHECK_EQ(running.session->state(), SessionState::Running);

    const SessionStats stats = running.session->stats();
    HYR_CHECK_EQ(stats.framesAccepted, std::uint64_t{kFrames});
    HYR_CHECK_EQ(stats.framesDroppedByPolicy, std::uint64_t{0});
    HYR_CHECK_EQ(stats.framesDispatched, stats.framesAccepted);
    HYR_CHECK_EQ(running.transport->accepted() + running.transport->droppedByRuntimeQueue(),
                 stats.framesDispatched);
}

HYR_TEST(a_stalled_transport_cannot_block_the_capture_path)
{
    EnqueueGate gate;
    RunningSession running;
    HYR_CHECK(running.start());
    running.transport->setGate(&gate);

    // Block the transport hand-off deterministically: the dispatch worker is provably stuck inside
    // enqueueFrame() from here on.
    gate.close();
    HYR_CHECK(running.source->deliver(makeFrame(16, 8)));
    HYR_CHECK(gate.waitForEntered(1));

    // Capture keeps being accepted while the dispatch worker cannot progress.
    for (int i = 0; i < 8; ++i) {
        RemoteFrame frame = makeFrame(16, 8);
        HYR_CHECK(running.source->deliver(frame));
    }

    HYR_CHECK(waitFor([&] { return running.session->stats().framesAccepted == 9; }));
    const SessionStats stats = running.session->stats();
    HYR_CHECK_EQ(running.session->state(), SessionState::Running);
    HYR_CHECK(stats.maxMailboxWaitingObserved <= 2);
    HYR_CHECK(stats.maxMailboxOwnedObserved <= 3);
    HYR_CHECK_EQ(running.transport->enqueueCalls(), std::size_t{0});  // still stuck before the post
    HYR_CHECK_EQ(stats.framesDispatched, std::uint64_t{0});
    HYR_CHECK(stats.framesDroppedByPolicy > 0);  // overload is absorbed inside the mailbox bound

    gate.open();
    HYR_CHECK(waitFor([&] { return running.session->stats().framesDispatched >= 1; }));
    running.stop();
    HYR_CHECK_EQ(running.session->state(), SessionState::Stopped);
}

HYR_TEST(accepted_frames_are_drained_before_the_transport_stops)
{
    RunningSession running;
    HYR_CHECK(running.start());

    RemoteFrame one = makeFrame(16, 8);
    RemoteFrame two = makeFrame(16, 8);
    HYR_CHECK(running.source->deliver(one));
    HYR_CHECK(running.source->deliver(two));
    HYR_CHECK(waitFor([&] { return running.session->stats().framesAccepted == 2; }));

    running.stop();

    // Deterministic shutdown: everything Core accepted was handed over before the transport was
    // stopped, and the transport was stopped exactly once, last.
    const SessionStats stats = running.session->stats();
    HYR_CHECK_EQ(stats.framesDispatched, stats.framesAccepted);
    HYR_CHECK_EQ(running.transport->accepted(), std::size_t{2});
    HYR_CHECK_EQ(running.transport->stopCalls(), 1);

    const std::vector<std::string> events = running.transport->events();
    HYR_CHECK(!events.empty());
    HYR_CHECK_EQ(events.front(), std::string{"start"});
    HYR_CHECK_EQ(events.back(), std::string{"stop"});
    HYR_CHECK_EQ(running.session->state(), SessionState::Stopped);
}

HYR_TEST(incompatible_capabilities_fail_before_running)
{
    {
        auto source = std::make_unique<FakeCaptureSource>();
        CaptureCapabilities sourceCaps;
        sourceCaps.asynchronous = true;
        sourceCaps.cpuReadable = true;
        sourceCaps.cpuFormats = {PixelFormat::Rgba8888};
        source->setCapabilities(sourceCaps);

        auto transport = std::make_unique<FakeTransport>();
        FrameConsumerCapabilities consumerCaps;
        consumerCaps.acceptsCpu = true;
        consumerCaps.cpuFormats = {PixelFormat::Bgra8888};
        transport->setCapabilities(consumerCaps);

        Session session;
        session.setCaptureSource(std::move(source));
        session.setTransport(std::move(transport));

        HYR_CHECK(!session.start());
        HYR_CHECK_EQ(session.state(), SessionState::Stopped);
        const std::optional<SessionError> error = session.lastError();
        HYR_CHECK(error.has_value());
        HYR_CHECK(error->code == SessionErrorCode::IncompatibleFrameCapabilities);
        HYR_CHECK(error->message.find("pixel format") != std::string::npos);
    }

    {
        // External-storage-only consumer without any external domain on the source.
        auto source = std::make_unique<FakeCaptureSource>();
        auto transport = std::make_unique<FakeTransport>();
        FrameConsumerCapabilities consumerCaps;
        consumerCaps.acceptsCpu = false;
        transport->setCapabilities(consumerCaps);

        Session session;
        session.setCaptureSource(std::move(source));
        session.setTransport(std::move(transport));

        HYR_CHECK(!session.start());
        HYR_CHECK_EQ(session.state(), SessionState::Stopped);
        HYR_CHECK(session.lastError().value().code == SessionErrorCode::IncompatibleFrameCapabilities);
    }

    {
        // A shared external domain is compatible, and so are unspecified format lists.
        auto source = std::make_unique<FakeCaptureSource>();
        CaptureCapabilities sourceCaps;
        sourceCaps.cpuReadable = false;
        sourceCaps.externalDomains = {"hyremote.test.dmabuf"};
        source->setCapabilities(sourceCaps);

        auto transport = std::make_unique<FakeTransport>();
        FrameConsumerCapabilities consumerCaps;
        consumerCaps.acceptsCpu = false;
        consumerCaps.externalDomains = {"hyremote.test.dmabuf"};
        transport->setCapabilities(consumerCaps);

        Session session;
        session.setCaptureSource(std::move(source));
        session.setTransport(std::move(transport));
        HYR_CHECK(session.start());
        HYR_CHECK_EQ(session.state(), SessionState::Running);
        session.stop();
    }
}

HYR_TEST(capability_check_reports_the_first_applicable_reason)
{
    {
        const CompatibilityResult result = checkFrameCompatibility({}, {});
        HYR_CHECK(result.compatible);
        HYR_CHECK(result.reason.empty());
    }

    {
        CaptureCapabilities source;
        source.cpuReadable = true;
        source.cpuFormats = {PixelFormat::Bgra8888};
        FrameConsumerCapabilities consumer;
        consumer.acceptsCpu = true;
        consumer.cpuFormats = {PixelFormat::Bgra8888, PixelFormat::Rgba8888};
        const CompatibilityResult result = checkFrameCompatibility(source, consumer);
        HYR_CHECK(result.compatible);
    }

    {
        CaptureCapabilities source;
        source.cpuReadable = false;  // produces no CPU frames at all
        FrameConsumerCapabilities consumer;
        consumer.acceptsCpu = true;
        const CompatibilityResult result = checkFrameCompatibility(source, consumer);
        HYR_CHECK(!result.compatible);
        HYR_CHECK(result.reason.find("not CPU-readable") != std::string::npos);
    }

    {
        CaptureCapabilities source;
        source.cpuReadable = false;
        source.externalDomains = {"domain.a"};
        FrameConsumerCapabilities consumer;
        consumer.acceptsCpu = false;
        consumer.externalDomains = {"domain.b", "domain.c"};
        const CompatibilityResult result = checkFrameCompatibility(source, consumer);
        HYR_CHECK(!result.compatible);
        HYR_CHECK(result.reason.find("domain.a") != std::string::npos);
        HYR_CHECK(result.reason.find("domain.b") != std::string::npos);
    }
}

HYR_TEST_MAIN()
