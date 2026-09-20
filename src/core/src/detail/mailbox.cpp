#include "detail/mailbox.hpp"

#include <algorithm>
#include <utility>

namespace hyremote::detail {

Mailbox::Mailbox(std::size_t capacity, BackpressurePolicy policy)
    : m_capacity(capacity == 0 ? 1 : capacity)
    , m_policy(policy)
{
}

void Mailbox::updateOwnedLocked()
{
    m_stats.waiting = m_queue.size();
    m_stats.dispatcherOwned = m_dispatcherOwned;
    m_stats.maxWaitingObserved = std::max(m_stats.maxWaitingObserved, m_stats.waiting);
    m_stats.maxOwnedObserved = std::max(m_stats.maxOwnedObserved, m_stats.waiting + m_dispatcherOwned);
}

PushResult Mailbox::push(RemoteFrame frame)
{
    PushResult result = PushResult::Stored;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_closed) {
            ++m_stats.rejectedClosed;
            return PushResult::RejectedClosed;
        }

        const bool dropOldest = m_queue.size() >= m_capacity;
        if (dropOldest && m_policy == BackpressurePolicy::ProducerThrottle) {
            // Admission control is supposed to make this unreachable: it counts waiting plus
            // in-flight frames against the capacity. If a backend still over-produces, the
            // frame is refused and counted instead of silently turning into DropOldest.
            ++m_stats.rejectedOverflow;
            updateOwnedLocked();
            return PushResult::RejectedOverflow;
        }

        const FrameId assignedId = m_nextFrameId;
        frame.id = assignedId;

        // For DropOldest, store the replacement before releasing the current oldest entry. This gives
        // the operation a strong failure post-state without an allocating rollback path: if deque growth
        // throws, the existing bounded queue is untouched and the exception can be contained by the
        // Session callback boundary. On success the temporary queue depth is at most capacity + 1 inside
        // this critical section; the old entry is removed before statistics are published or waiters are
        // notified. The incoming RemoteFrame storage was already owned by this call, so this does not add a
        // second frame payload to the pipeline merely to recover from allocation failure.
        m_queue.push_back(std::move(frame));

        ++m_nextFrameId;
        m_stats.lastFrameId = assignedId;

        if (dropOldest) {
            m_queue.pop_front();
            ++m_stats.droppedOldest;
            result = PushResult::StoredAfterDroppingOldest;
        }

        ++m_stats.stored;
        updateOwnedLocked();
    }

    m_cv.notify_all();
    return result;
}

PopResult Mailbox::tryPop(RemoteFrame *out)
{
    if (out == nullptr)
        return PopResult::Empty;

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_queue.empty())
        return m_closed ? PopResult::Closed : PopResult::Empty;

    *out = std::move(m_queue.front());
    m_queue.pop_front();
    ++m_dispatcherOwned;
    ++m_stats.popped;
    updateOwnedLocked();
    return PopResult::Frame;
}

void Mailbox::releaseDispatcherOwnership()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_dispatcherOwned > 0)
        --m_dispatcherOwned;
    updateOwnedLocked();
}

void Mailbox::close()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_closed = true;
        updateOwnedLocked();
    }
    m_cv.notify_all();
}

bool Mailbox::isClosed() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_closed;
}

void Mailbox::waitForWork()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this] { return m_closed || !m_queue.empty(); });
}

MailboxStats Mailbox::stats() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

}  // namespace hyremote::detail
