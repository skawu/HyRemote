// C1 - frame lifetime.
//
// Proves that ownership, not a naked pointer, carries the frame lifetime, and that the final
// release happens exactly once even when the last reference is dropped on another thread.

#include <thread>
#include <vector>

#include "detail/mailbox.hpp"
#include "fakes.hpp"

using namespace hyremote;
using namespace hyremote::test;

HYR_TEST(consumer_can_retain_a_frame_after_the_producer_callback_returns)
{
    TrackedFrame tracked = makeTrackedFrame();
    writePattern(tracked.frame, 0xAB);

    // The "consumer" keeps the frame; the producer then forgets everything it had.
    RemoteFrame consumerFrame = tracked.frame;
    tracked.frame = RemoteFrame{};
    HYR_CHECK_EQ(*tracked.releases, 0);

    // The pixels are still readable through the storage anchor only.
    HYR_CHECK_EQ(readPattern(consumerFrame), 0xAB);
    HYR_CHECK(consumerFrame.storage != nullptr);

    consumerFrame = RemoteFrame{};
    HYR_CHECK_EQ(*tracked.releases, 1);
}

HYR_TEST(storage_stays_valid_until_the_final_owner_releases_it)
{
    TrackedFrame tracked = makeTrackedFrame();
    writePattern(tracked.frame, 0x5A);

    std::vector<RemoteFrame> copies(4, tracked.frame);
    tracked.frame = RemoteFrame{};

    for (const RemoteFrame &frame : copies)
        HYR_CHECK_EQ(readPattern(frame), 0x5A);
    HYR_CHECK_EQ(*tracked.releases, 0);

    copies.clear();
    HYR_CHECK_EQ(*tracked.releases, 1);
}

HYR_TEST(drop_oldest_releases_the_dropped_frame_ownership)
{
    detail::Mailbox mailbox(2, BackpressurePolicy::DropOldest);

    TrackedFrame first = makeTrackedFrame();
    TrackedFrame second = makeTrackedFrame();
    TrackedFrame third = makeTrackedFrame();

    HYR_CHECK(mailbox.push(std::move(first.frame)) == detail::PushResult::Stored);
    HYR_CHECK_EQ(*first.releases, 0);
    HYR_CHECK(mailbox.push(std::move(second.frame)) == detail::PushResult::Stored);
    HYR_CHECK_EQ(*second.releases, 0);

    // The mailbox is full: the oldest waiting frame must lose its ownership immediately.
    HYR_CHECK(mailbox.push(std::move(third.frame)) == detail::PushResult::StoredAfterDroppingOldest);
    HYR_CHECK_EQ(*first.releases, 1);
    HYR_CHECK_EQ(*second.releases, 0);
    HYR_CHECK_EQ(*third.releases, 0);
    HYR_CHECK_EQ(mailbox.stats().droppedOldest, 1);

    // Draining the mailbox releases the frames that survived.
    RemoteFrame out;
    HYR_CHECK(mailbox.tryPop(&out) == detail::PopResult::Frame);
    out = RemoteFrame{};
    mailbox.releaseDispatcherOwnership();
    HYR_CHECK_EQ(*second.releases, 1);

    HYR_CHECK(mailbox.tryPop(&out) == detail::PopResult::Frame);
    out = RemoteFrame{};
    mailbox.releaseDispatcherOwnership();
    HYR_CHECK_EQ(*third.releases, 1);
}

HYR_TEST(the_custom_recycler_receives_exactly_one_final_release)
{
    TrackedFrame tracked = makeTrackedFrame();
    std::vector<RemoteFrame> frames(8, tracked.frame);
    tracked.frame = RemoteFrame{};

    // Release half of the references on another thread: the recycler must still run exactly once,
    // and only when the last owning reference disappears.
    std::thread worker([&frames] {
        for (std::size_t i = 0; i < 4; ++i)
            frames[i] = RemoteFrame{};
    });
    worker.join();
    HYR_CHECK_EQ(*tracked.releases, 0);

    for (std::size_t i = 4; i + 1 < frames.size(); ++i)
        frames[i] = RemoteFrame{};
    HYR_CHECK_EQ(*tracked.releases, 0);

    frames.back() = RemoteFrame{};
    HYR_CHECK_EQ(*tracked.releases, 1);
}

HYR_TEST(a_frame_without_a_storage_anchor_is_never_published)
{
    // Models a producer that hands over a raw borrowed pointer with no lifetime anchor.
    RemoteFrame borrowed;
    borrowed.geometry.size = Size{8, 4};
    borrowed.geometry.pixelFormat = PixelFormat::Bgra8888;
    borrowed.geometry.planeCount = 1;
    borrowed.timing.completionTime = Clock::now();

    HYR_CHECK(!validateFrame(borrowed));
    HYR_CHECK(!validateFrame(borrowed).reason.empty());

    RunningSession running;
    HYR_CHECK(running.start());
    HYR_CHECK(running.source->deliver(borrowed));

    HYR_CHECK(waitFor([&] { return running.session->stats().framesRejectedInvalid == 1; }));
    HYR_CHECK_EQ(running.session->stats().framesAccepted, 0);
    HYR_CHECK_EQ(running.transport->accepted(), 0);
    HYR_CHECK_EQ(running.session->state(), SessionState::Running);
}

HYR_TEST(invalid_geometry_is_rejected_by_the_storage_boundary)
{
    RemoteFrame frame = makeFrame();
    frame.geometry.size = Size{0, 4};
    HYR_CHECK(!validateFrame(frame));

    frame = makeFrame();
    frame.geometry.planeCount = 0;
    HYR_CHECK(!validateFrame(frame));

    frame = makeFrame();
    frame.storage = nullptr;
    HYR_CHECK(!validateFrame(frame));
}

HYR_TEST(cpu_storage_rejects_an_invalid_layout)
{
    HYR_CHECK(CpuFrameStorage::create({}) == nullptr);
    HYR_CHECK(CpuFrameStorage::create({{4, 0}}) == nullptr);
    HYR_CHECK(CpuFrameStorage::createSinglePlane(0, 4) == nullptr);
    HYR_CHECK(CpuFrameStorage::createSinglePlane(16, 0) == nullptr);

    std::shared_ptr<CpuFrameStorage> storage = CpuFrameStorage::createSinglePlane(16, 4);
    HYR_CHECK(storage != nullptr);
    HYR_CHECK_EQ(storage->planeCount(), std::size_t{1});
    HYR_CHECK_EQ(storage->totalBytes(), std::size_t{64});
    HYR_CHECK_EQ(storage->kind(), StorageKind::Cpu);
    HYR_CHECK(storage->mapRead(1) == std::nullopt);
    HYR_CHECK(storage->extension("any.domain") == nullptr);
    HYR_CHECK(storage->externalDomain().empty());
}

HYR_TEST_MAIN()
