// C4 - bounded completed-frame mailbox, drop policy and ownership accounting.

#include <atomic>
#include <thread>

#include "detail/mailbox.hpp"
#include "fakes.hpp"

using namespace hyremote;
using namespace hyremote::test;

namespace {

std::vector<FrameId> remainingIds(detail::Mailbox &mailbox)
{
    std::vector<FrameId> ids;
    RemoteFrame frame;
    while (mailbox.tryPop(&frame) == detail::PopResult::Frame) {
        ids.push_back(frame.id);
        frame = RemoteFrame{};
        mailbox.releaseDispatcherOwnership();
    }
    return ids;
}

// Used by the concurrency case, which needs them inside lambdas.
constexpr std::size_t kProducers = 4;
constexpr std::size_t kPerProducer = 50;

}  // namespace

HYR_TEST(drop_oldest_never_lets_the_waiting_depth_exceed_the_capacity)
{
    detail::Mailbox mailbox(2, BackpressurePolicy::DropOldest);

    for (int i = 0; i < 10; ++i) {
        RemoteFrame frame = makeFrame(8, 8);
        mailbox.push(std::move(frame));
    }

    const detail::MailboxStats stats = mailbox.stats();
    HYR_CHECK(stats.maxWaitingObserved <= mailbox.capacity());
    HYR_CHECK_EQ(stats.waiting, std::size_t{2});
    HYR_CHECK_EQ(stats.stored, std::uint64_t{10});
    HYR_CHECK_EQ(stats.droppedOldest, std::uint64_t{8});
}

HYR_TEST(newest_frames_survive_overload)
{
    detail::Mailbox mailbox(2, BackpressurePolicy::DropOldest);

    for (int i = 0; i < 5; ++i) {
        RemoteFrame frame = makeFrame(8, 8);
        mailbox.push(std::move(frame));
    }

    // Latest-frame-wins: the two surviving frames are the two newest ones.
    const std::vector<FrameId> ids = remainingIds(mailbox);
    HYR_CHECK_EQ(ids.size(), std::size_t{2});
    HYR_CHECK_EQ(ids[0], FrameId{4});
    HYR_CHECK_EQ(ids[1], FrameId{5});
}

HYR_TEST(a_drop_is_counted_and_is_not_an_error)
{
    detail::Mailbox mailbox(1, BackpressurePolicy::DropOldest);

    for (int i = 0; i < 4; ++i) {
        RemoteFrame frame = makeFrame(8, 8);
        // No exception, no error state: dropping a waiting frame is normal flow control.
        mailbox.push(std::move(frame));
    }

    const detail::MailboxStats stats = mailbox.stats();
    HYR_CHECK_EQ(stats.droppedOldest, std::uint64_t{3});
    HYR_CHECK(!mailbox.isClosed());
    HYR_CHECK_EQ(stats.waiting, std::size_t{1});
}

HYR_TEST(the_dispatcher_owned_frame_is_accounted_in_addition_to_the_queue)
{
    detail::Mailbox mailbox(2, BackpressurePolicy::DropOldest);

    RemoteFrame one = makeFrame(8, 8);
    RemoteFrame two = makeFrame(8, 8);
    mailbox.push(std::move(one));
    mailbox.push(std::move(two));
    HYR_CHECK_EQ(mailbox.stats().maxOwnedObserved, std::size_t{2});

    RemoteFrame popped;
    HYR_CHECK(mailbox.tryPop(&popped) == detail::PopResult::Frame);
    HYR_CHECK_EQ(mailbox.stats().waiting, std::size_t{1});
    HYR_CHECK_EQ(mailbox.stats().dispatcherOwned, std::size_t{1});

    // One more frame arrives while the dispatch worker still owns the popped one: the maximum
    // Core-side ownership is capacity + 1, and the queue itself stays within the capacity.
    RemoteFrame three = makeFrame(8, 8);
    mailbox.push(std::move(three));

    const detail::MailboxStats stats = mailbox.stats();
    HYR_CHECK_EQ(stats.waiting, std::size_t{2});
    HYR_CHECK_EQ(stats.dispatcherOwned, std::size_t{1});
    HYR_CHECK_EQ(stats.maxWaitingObserved, std::size_t{2});
    HYR_CHECK_EQ(stats.maxOwnedObserved, std::size_t{3});

    popped = RemoteFrame{};
    mailbox.releaseDispatcherOwnership();
    HYR_CHECK_EQ(mailbox.stats().dispatcherOwned, std::size_t{0});
    HYR_CHECK_EQ(mailbox.stats().waiting, std::size_t{2});
}

HYR_TEST(producer_throttle_is_behaviorally_distinct_from_drop_oldest)
{
    detail::Mailbox mailbox(2, BackpressurePolicy::ProducerThrottle);

    RemoteFrame one = makeFrame(8, 8);
    RemoteFrame two = makeFrame(8, 8);
    RemoteFrame three = makeFrame(8, 8);

    HYR_CHECK(mailbox.push(std::move(one)) == detail::PushResult::Stored);
    HYR_CHECK(mailbox.push(std::move(two)) == detail::PushResult::Stored);
    HYR_CHECK(mailbox.push(std::move(three)) == detail::PushResult::RejectedOverflow);

    const detail::MailboxStats stats = mailbox.stats();
    // It must not silently become DropOldest: nothing is dropped, and the overflow is visible.
    HYR_CHECK_EQ(stats.droppedOldest, std::uint64_t{0});
    HYR_CHECK_EQ(stats.rejectedOverflow, std::uint64_t{1});
    HYR_CHECK_EQ(stats.waiting, std::size_t{2});
    HYR_CHECK_EQ(stats.stored, std::uint64_t{2});

    const std::vector<FrameId> ids = remainingIds(mailbox);
    HYR_CHECK_EQ(ids.size(), std::size_t{2});
    HYR_CHECK_EQ(ids[0], FrameId{1});
    HYR_CHECK_EQ(ids[1], FrameId{2});
}

HYR_TEST(close_drains_waiting_frames_then_reports_closed)
{
    detail::Mailbox mailbox(4, BackpressurePolicy::DropOldest);

    RemoteFrame one = makeFrame(8, 8);
    RemoteFrame two = makeFrame(8, 8);
    mailbox.push(std::move(one));
    mailbox.push(std::move(two));

    mailbox.close();
    HYR_CHECK(mailbox.isClosed());

    RemoteFrame popped;
    HYR_CHECK(mailbox.tryPop(&popped) == detail::PopResult::Frame);
    popped = RemoteFrame{};
    mailbox.releaseDispatcherOwnership();
    HYR_CHECK(mailbox.tryPop(&popped) == detail::PopResult::Frame);
    popped = RemoteFrame{};
    mailbox.releaseDispatcherOwnership();

    // Closed and drained: the dispatch worker may exit.
    HYR_CHECK(mailbox.tryPop(&popped) == detail::PopResult::Closed);

    RemoteFrame late = makeFrame(8, 8);
    HYR_CHECK(mailbox.push(std::move(late)) == detail::PushResult::RejectedClosed);
    HYR_CHECK_EQ(mailbox.stats().rejectedClosed, std::uint64_t{1});
}

HYR_TEST(frame_ids_are_assigned_in_push_order)
{
    detail::Mailbox mailbox(8, BackpressurePolicy::DropOldest);

    for (int i = 0; i < 3; ++i) {
        RemoteFrame frame = makeFrame(8, 8);
        frame.requestId = static_cast<CaptureRequestId>(100 - i);  // unrelated to acceptance order
        mailbox.push(std::move(frame));
    }

    const detail::MailboxStats stats = mailbox.stats();
    HYR_CHECK_EQ(stats.lastFrameId, FrameId{3});

    const std::vector<FrameId> ids = remainingIds(mailbox);
    HYR_CHECK_EQ(ids[0], FrameId{1});
    HYR_CHECK_EQ(ids[1], FrameId{2});
    HYR_CHECK_EQ(ids[2], FrameId{3});
}

HYR_TEST(concurrent_producing_and_consuming_keep_the_bounds)
{
    detail::Mailbox mailbox(2, BackpressurePolicy::DropOldest);
    std::atomic<bool> producersDone{false};
    std::atomic<std::uint64_t> popped{0};

    std::vector<std::thread> producers;
    producers.reserve(kProducers);
    for (std::size_t producer = 0; producer < kProducers; ++producer) {
        producers.emplace_back([&mailbox] {
            for (std::size_t i = 0; i < kPerProducer; ++i) {
                RemoteFrame frame = makeFrame(8, 8);
                mailbox.push(std::move(frame));
            }
        });
    }

    std::thread consumer([&mailbox, &producersDone, &popped] {
        RemoteFrame frame;
        while (!producersDone.load(std::memory_order_acquire)) {
            if (mailbox.tryPop(&frame) == detail::PopResult::Frame) {
                frame = RemoteFrame{};
                mailbox.releaseDispatcherOwnership();
                popped.fetch_add(1, std::memory_order_relaxed);
            } else {
                std::this_thread::yield();
            }
        }
        while (mailbox.tryPop(&frame) == detail::PopResult::Frame) {
            frame = RemoteFrame{};
            mailbox.releaseDispatcherOwnership();
            popped.fetch_add(1, std::memory_order_relaxed);
        }
    });

    for (std::thread &producer : producers)
        producer.join();
    producersDone.store(true, std::memory_order_release);
    consumer.join();

    const detail::MailboxStats stats = mailbox.stats();
    HYR_CHECK(stats.maxWaitingObserved <= mailbox.capacity());
    HYR_CHECK(stats.maxOwnedObserved <= mailbox.capacity() + 1);
    HYR_CHECK_EQ(stats.stored, std::uint64_t{kProducers * kPerProducer});
    // Every stored frame is either still waiting, dropped, or popped.
    HYR_CHECK_EQ(stats.stored, popped.load() + stats.droppedOldest);
    HYR_CHECK_EQ(stats.waiting, std::size_t{0});
    HYR_CHECK_EQ(stats.dispatcherOwned, std::size_t{0});
    HYR_CHECK_EQ(stats.lastFrameId, stats.stored);
}

HYR_TEST_MAIN()
