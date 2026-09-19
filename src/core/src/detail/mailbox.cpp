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

        if (m_queue.size() >= m_capacity) {
            if (m_policy == BackpressurePolicy::ProducerThrottle) {
                // Admission control is supposed to make this unreachable: it counts waiting plus
                // in-flight frames against the capacity. If a backend still over-produces, the
                // frame is refused and counted instead of silently turning into DropOldest.
                ++m_stats.rejectedOverflow;
                updateOwnedLocked();
                return PushResult::RejectedOverflow;
            }

            // DropOldest / LatestFrameWins: the oldest waiting frame loses its ownership and the
            // newest content survives.
            m_queue.pop_front();
            ++m_stats.droppedOldest;
            result = PushResult::StoredAfterDroppingOldest;
        }

        frame.id = m_nextFrameId++;
        m_stats.lastFrameId = frame.id;
        m_queue.push_back(std::move(frame));
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
