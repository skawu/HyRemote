#include "runtime_notifications.hpp"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <utility>
#include <vector>

namespace HyRemote::Runtime {

struct RuntimeNotificationSink::Impl
{
    struct Subscriber
    {
        std::uint64_t identity = 0;
        RuntimeNotificationHandlers handlers;
    };

    mutable std::mutex mutex;
    std::vector<Subscriber> subscribers;
    std::uint64_t nextIdentity = 1;

    // Last delivered value of each fact. `errorPublished` distinguishes "never delivered anything"
    // from "delivered the value that is still current", which is what makes the first snapshot of a
    // session reach a subscriber while a repeated one does not.
    std::optional<AccessState> lastState;
    std::optional<std::size_t> lastConnectedClientCount;
    std::optional<Error> lastError;
    bool errorPublished = false;

    std::atomic<std::size_t> containedExceptions{0};
};

namespace {

// Handlers are copied under the lock and invoked after it is released, so a consumer may subscribe or
// unsubscribe from inside its own callback without deadlocking the seam.
template <typename Member, typename Argument>
void deliver(const std::vector<RuntimeNotificationHandlers> &handlers,
             Member RuntimeNotificationHandlers::*member,
             const Argument &argument,
             std::atomic<std::size_t> &containedExceptions)
{
    for (const RuntimeNotificationHandlers &bundle : handlers) {
        const Member &handler = bundle.*member;
        if (!handler) {
            continue;
        }

        try {
            handler(argument);
        } catch (...) {
            // A consumer callback never escapes the seam: it must not abort a transport worker
            // thread, change a Session state or terminate the process.
            containedExceptions.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

}  // namespace

RuntimeNotificationSink::RuntimeNotificationSink()
    : m_impl(std::make_unique<Impl>())
{
}

RuntimeNotificationSink::~RuntimeNotificationSink() = default;

RuntimeNotificationToken RuntimeNotificationSink::subscribe(RuntimeNotificationHandlers handlers)
{
    const std::lock_guard<std::mutex> lock(m_impl->mutex);
    if (m_impl->subscribers.size() >= maxSubscribers) {
        return RuntimeNotificationToken{};
    }

    const std::uint64_t identity = m_impl->nextIdentity++;
    m_impl->subscribers.push_back(Impl::Subscriber{identity, std::move(handlers)});
    return RuntimeNotificationToken(identity);
}

void RuntimeNotificationSink::unsubscribe(RuntimeNotificationToken token) noexcept
{
    if (!token.isValid()) {
        return;
    }

    try {
        const std::lock_guard<std::mutex> lock(m_impl->mutex);
        const auto removed = std::remove_if(m_impl->subscribers.begin(),
                                            m_impl->subscribers.end(),
                                            [token](const Impl::Subscriber &subscriber) {
                                                return subscriber.identity == token.identity();
                                            });
        m_impl->subscribers.erase(removed, m_impl->subscribers.end());
    } catch (...) {
        // Unsubscribing is a best-effort cleanup on a path that must not throw.
    }
}

std::size_t RuntimeNotificationSink::subscriberCount() const noexcept
{
    const std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->subscribers.size();
}

std::size_t RuntimeNotificationSink::containedCallbackExceptions() const noexcept
{
    return m_impl->containedExceptions.load(std::memory_order_relaxed);
}

void RuntimeNotificationSink::publishState(AccessState state)
{
    std::vector<RuntimeNotificationHandlers> targets;
    {
        const std::lock_guard<std::mutex> lock(m_impl->mutex);
        if (m_impl->lastState && *m_impl->lastState == state) {
            return;  // the same state published twice delivers once
        }
        m_impl->lastState = state;
        targets.reserve(m_impl->subscribers.size());
        for (const Impl::Subscriber &subscriber : m_impl->subscribers) {
            targets.push_back(subscriber.handlers);
        }
    }

    deliver(targets, &RuntimeNotificationHandlers::stateChanged, state, m_impl->containedExceptions);
}

void RuntimeNotificationSink::publishConnectedClientCount(std::size_t count)
{
    std::vector<RuntimeNotificationHandlers> targets;
    {
        const std::lock_guard<std::mutex> lock(m_impl->mutex);
        if (m_impl->lastConnectedClientCount && *m_impl->lastConnectedClientCount == count) {
            return;
        }
        m_impl->lastConnectedClientCount = count;
        targets.reserve(m_impl->subscribers.size());
        for (const Impl::Subscriber &subscriber : m_impl->subscribers) {
            targets.push_back(subscriber.handlers);
        }
    }

    deliver(targets,
            &RuntimeNotificationHandlers::connectedClientCountChanged,
            count,
            m_impl->containedExceptions);
}

void RuntimeNotificationSink::publishError(const std::optional<Error> &error, bool newOccurrence)
{
    std::vector<RuntimeNotificationHandlers> targets;
    {
        const std::lock_guard<std::mutex> lock(m_impl->mutex);
        if (error) {
            const bool unchanged = m_impl->errorPublished && m_impl->lastError
                && m_impl->lastError->code == error->code
                && m_impl->lastError->message == error->message
                && m_impl->lastError->recoverable == error->recoverable;
            if (unchanged && !newOccurrence) {
                return;
            }
            m_impl->lastError = *error;
        } else {
            // "No error" is only news while the consumers still hold an error. Publishing it to a
            // subscriber that has never been told about an error, or publishing it twice, would be a
            // notification that carries no change.
            if (!m_impl->errorPublished || !m_impl->lastError) {
                return;
            }
            m_impl->lastError.reset();
        }

        m_impl->errorPublished = true;
        targets.reserve(m_impl->subscribers.size());
        for (const Impl::Subscriber &subscriber : m_impl->subscribers) {
            targets.push_back(subscriber.handlers);
        }
    }

    deliver(targets, &RuntimeNotificationHandlers::errorChanged, error, m_impl->containedExceptions);
}

}  // namespace HyRemote::Runtime
