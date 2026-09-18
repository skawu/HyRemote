// SPDX-License-Identifier: Apache-2.0
#pragma once

// Internal implementation detail of hyremote-core: the bounded completed-frame mailbox.
//
// Not part of the public Core contract, but a self-contained unit so its bounds, drop policy and
// ownership accounting can be tested directly (issue #21, behavior test C4).
//
// Ownership accounting (ADR-0003):
//   waitingCount()          frames waiting for dispatch
//   dispatcherOwnedCount()  frames currently held by the dispatch worker
//   Core-side ownership     waiting + dispatcher-owned <= capacity + 1
//
// Lock order: the Session mutex may be held while calling into the mailbox; the mailbox never
// calls back into the Session while holding its own mutex. Never invert this order.

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>

#include "hyremote/core/frame.hpp"
#include "hyremote/core/session.hpp"

namespace hyremote::detail {

enum class PushResult {
    Stored,                     // accepted and assigned a FrameId
    StoredAfterDroppingOldest,  // DropOldest released the oldest waiting frame to make room
    RejectedOverflow,           // ProducerThrottle: capacity reached, frame not accepted
    RejectedClosed,             // mailbox already closed by Session::stop()
};

enum class PopResult {
    Frame,  // a frame was taken; dispatcher ownership was acquired
    Empty,  // nothing waiting, mailbox still open
    Closed, // mailbox closed and drained: the dispatch worker can exit
};

struct MailboxStats
{
    std::size_t waiting = 0;
    std::size_t dispatcherOwned = 0;
    std::size_t maxWaitingObserved = 0;
    std::size_t maxOwnedObserved = 0;
    std::uint64_t stored = 0;
    std::uint64_t popped = 0;
    std::uint64_t droppedOldest = 0;
    std::uint64_t rejectedOverflow = 0;
    std::uint64_t rejectedClosed = 0;
    std::uint64_t lastFrameId = 0;
};

class Mailbox
{
public:
    Mailbox(std::size_t capacity, BackpressurePolicy policy);

    Mailbox(const Mailbox &) = delete;
    Mailbox &operator=(const Mailbox &) = delete;

    std::size_t capacity() const noexcept { return m_capacity; }
    BackpressurePolicy policy() const noexcept { return m_policy; }

    // Publishes a frame. A FrameId is assigned in acceptance order (this call order), which is
    // the completion order, independently of the capture request order.
    PushResult push(RemoteFrame frame);

    // Takes the oldest waiting frame and marks it as owned by the dispatch worker.
    PopResult tryPop(RemoteFrame *out);

    // Releases the dispatch worker's ownership of the frame it took with tryPop().
    void releaseDispatcherOwnership();

    // Closes the mailbox. Waiting frames stay available so the dispatch worker can drain them.
    void close();

    bool isClosed() const;

    // Blocks until a frame is waiting or the mailbox is closed.
    void waitForWork();

    MailboxStats stats() const;

private:
    void updateOwnedLocked();

    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<RemoteFrame> m_queue;
    const std::size_t m_capacity;
    const BackpressurePolicy m_policy;
    std::uint64_t m_nextFrameId = 1;
    bool m_closed = false;
    std::size_t m_dispatcherOwned = 0;
    MailboxStats m_stats;
};

}  // namespace hyremote::detail
