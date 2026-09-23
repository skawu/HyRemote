#pragma once

// Private Runtime notification seam (#259).
//
// The three callbacks below are the whole notification vocabulary of the shared runtime: typed by
// construction, with no topic string, no QVariant payload, no observer base class, no generic event
// bus and no global registry. A frontend registers the handlers it needs and keeps the returned
// token so it can unregister again.
//
// **Delivery thread.** A notification is delivered on the thread that produced the Runtime-side fact:
// the lifecycle caller thread for start/stop, a transport worker thread for client connect and
// disconnect, a capture/backend thread for target loss. The seam therefore promises **no** thread
// affinity and none for the GUI: a frontend that must touch its own QObject state marshals the
// notification onto its own thread (QmlRemoteAccess does exactly that, and a frontend that ignores
// this rule is the only way to break it).
//
// **Lifetime.** The sink stores handler copies only - never a raw owner pointer - so a destroyed
// subscriber cannot be called through a dangling registration. Unsubscribing stops further delivery.
// A handler that is executing while its own subscription is removed finishes that one call, so a
// consumer that can be destroyed concurrently guards its captured state (QmlRemoteAccess captures a
// QPointer) in addition to unsubscribing.
//
// **No duplicate publications.** The sink remembers the last delivered value of each of the three
// facts, so publishing the truth twice delivers once. The one deliberate exception is an error: a
// genuinely new occurrence of the same code/message is delivered again, which is what keeps a
// repeated recoverable failure observable after it was acknowledged.

#include "../access_types.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace HyRemote::Runtime {

struct RuntimeNotificationHandlers
{
    std::function<void(AccessState)> stateChanged;
    std::function<void(std::size_t)> connectedClientCountChanged;
    // std::nullopt means "the effective Runtime error is gone" - never "no news".
    std::function<void(std::optional<Error>)> errorChanged;
};

// Opaque, copyable subscription identity. A token is only meaningful for the sink that produced it;
// unsubscribing an unknown, already-removed or default-constructed token is a no-op.
class RuntimeNotificationToken
{
public:
    RuntimeNotificationToken() noexcept = default;

    bool isValid() const noexcept { return m_identity != 0; }
    std::uint64_t identity() const noexcept { return m_identity; }

private:
    friend class RuntimeNotificationSink;
    explicit RuntimeNotificationToken(std::uint64_t identity) noexcept
        : m_identity(identity)
    {
    }

    std::uint64_t m_identity = 0;
};

class RuntimeNotificationSink
{
public:
    // Bounded: this seam exists for the repository's own frontends, not for an unbounded observer
    // population, so registration beyond the bound is refused rather than allowed to grow.
    static constexpr std::size_t maxSubscribers = 8;

    RuntimeNotificationSink();
    ~RuntimeNotificationSink();

    RuntimeNotificationSink(const RuntimeNotificationSink &) = delete;
    RuntimeNotificationSink &operator=(const RuntimeNotificationSink &) = delete;

    RuntimeNotificationToken subscribe(RuntimeNotificationHandlers handlers);
    void unsubscribe(RuntimeNotificationToken token) noexcept;

    std::size_t subscriberCount() const noexcept;

    // Diagnostics: a consumer callback that threw and was contained by the seam. Such a callback must
    // never escape into the runtime, abort a transport worker or change a Session state.
    std::size_t containedCallbackExceptions() const noexcept;

    // Producer side. Only the shared Runtime publishes here; a frontend can subscribe but cannot
    // inject a notification, so there is exactly one source of truth for every value.
    void publishState(AccessState state);
    void publishConnectedClientCount(std::size_t count);
    // `newOccurrence` forces delivery of an unchanged error, and is only meaningful when `error` is
    // set. It is how a second real occurrence of the same recoverable failure stays observable.
    void publishError(const std::optional<Error> &error, bool newOccurrence = false);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace HyRemote::Runtime
