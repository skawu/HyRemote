#include <HyRemote/RemoteAccess.h>

#include <QObject>
#include <QPointer>

#include <functional>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "detail/component_factories.hpp"
#include "hyremote/core/session.hpp"

namespace HyRemote {
namespace {

RemoteAccessState mapState(hyremote::SessionState state)
{
    switch (state) {
    case hyremote::SessionState::Stopped:
        return RemoteAccessState::Stopped;
    case hyremote::SessionState::Starting:
        return RemoteAccessState::Starting;
    case hyremote::SessionState::Running:
        return RemoteAccessState::Running;
    case hyremote::SessionState::Stopping:
        return RemoteAccessState::Stopping;
    case hyremote::SessionState::Faulted:
        return RemoteAccessState::Faulted;
    }
    return RemoteAccessState::Faulted;
}

RemoteAccessError mapError(const hyremote::SessionError &error)
{
    RemoteAccessError result;
    result.message = QString::fromStdString(error.message);
    result.recoverable = error.recoverable;

    switch (error.code) {
    case hyremote::SessionErrorCode::InvalidConfiguration:
    case hyremote::SessionErrorCode::IncompatibleFrameCapabilities:
        result.code = RemoteAccessErrorCode::InvalidConfiguration;
        break;
    case hyremote::SessionErrorCode::CaptureStartFailed:
    case hyremote::SessionErrorCode::TransportStartFailed:
        result.code = RemoteAccessErrorCode::StartFailed;
        break;
    case hyremote::SessionErrorCode::StartCancelled:
        result.code = RemoteAccessErrorCode::Cancelled;
        break;
    case hyremote::SessionErrorCode::TargetLost:
    case hyremote::SessionErrorCode::ComponentFailure:
        result.code = RemoteAccessErrorCode::RuntimeFailure;
        break;
    }

    return result;
}

void decrementConnectedClients(const std::shared_ptr<std::atomic<std::size_t>> &count) noexcept
{
    if (!count)
        return;

    std::size_t current = count->load(std::memory_order_relaxed);
    while (current != 0
           && !count->compare_exchange_weak(current,
                                            current - 1,
                                            std::memory_order_relaxed,
                                            std::memory_order_relaxed)) {
    }
}

// Private product diagnostic decorator. It observes only Core's transport-neutral connection
// events, then forwards the exact same event stream into the one Session owned by RemoteAccess.
// Concrete RFB/client/socket types never enter the facade API and no second runtime is created.
class ClientCountingTransport final : public hyremote::Transport
{
public:
    ClientCountingTransport(std::unique_ptr<hyremote::Transport> transport,
                            std::shared_ptr<std::atomic<std::size_t>> connectedClients,
                            std::function<void()> onClientCountChanged)
        : m_transport(std::move(transport))
        , m_connectedClients(std::move(connectedClients))
        , m_onClientCountChanged(std::move(onClientCountChanged))
    {
    }

    hyremote::FrameConsumerCapabilities frameCapabilities() const override
    {
        return m_transport->frameCapabilities();
    }

    bool start(hyremote::InputHandler onInput, hyremote::TransportEventHandler onEvent) override
    {
        m_connectedClients->store(0, std::memory_order_relaxed);
        const auto connectedClients = m_connectedClients;
        const bool started = m_transport->start(
            std::move(onInput),
            [this, connectedClients, onEvent = std::move(onEvent)](const hyremote::TransportEvent &event) mutable {
                // The session sees the event first: a notification that fires from here must let its observer read
                // the post-event state and diagnostic, not the values from before the event.
                if (onEvent)
                    onEvent(event);

                switch (event.code) {
                case hyremote::TransportEventCode::ClientConnected:
                    connectedClients->fetch_add(1, std::memory_order_relaxed);
                    break;
                case hyremote::TransportEventCode::ClientDisconnected:
                    decrementConnectedClients(connectedClients);
                    break;
                case hyremote::TransportEventCode::AuthenticationRejected:
                case hyremote::TransportEventCode::RecoverableFailure:
                case hyremote::TransportEventCode::FatalFailure:
                    break;
                }

                // Every connection event is reported and the facade decides whether the count really moved, so a
                // disconnect for a client that was never counted cannot turn into a signal that reports no change.
                if (m_onClientCountChanged)
                    m_onClientCountChanged();
            });
        if (!started)
            m_connectedClients->store(0, std::memory_order_relaxed);
        return started;
    }

    void stop() noexcept override
    {
        m_transport->stop();
        // Transport::stop() is quiescent by contract. Once it returns no late connection callback
        // may race this reset, so Stopped always exposes zero connected clients.
        m_connectedClients->store(0, std::memory_order_relaxed);
        if (m_onClientCountChanged)
            m_onClientCountChanged();
    }

    void enqueueFrame(hyremote::RemoteFrame frame) override
    {
        m_transport->enqueueFrame(std::move(frame));
    }

private:
    std::unique_ptr<hyremote::Transport> m_transport;
    std::shared_ptr<std::atomic<std::size_t>> m_connectedClients;
    std::function<void()> m_onClientCountChanged;
};

}  // namespace

struct RemoteAccess::Impl
{
    struct SessionErrorRevision
    {
        // V1 Core currently publishes a recoverable SessionError only when InputSink::post() throws.
        // Track that occurrence counter directly: unrelated capture/transport activity must not make
        // an already acknowledged recoverable error visible again.
        std::uint64_t inputPostFailures = 0;
    };

    QPointer<QObject> target;
    QHostAddress listenAddress = QHostAddress::LocalHost;
    // The configured product default; see HYREMOTE_DEFAULT_PORT in cmake/HyRemoteProjectOptions.cmake.
    quint16 port = static_cast<quint16>(HYREMOTE_DEFAULT_PORT);
    bool remoteInputEnabled = false;
    RemoteSecurityProfile securityProfile = RemoteSecurityProfile::Insecure;
    QString securityConfigFile;
    std::unique_ptr<hyremote::Session> session;
    std::shared_ptr<hyremote::InputSink> inputSink;
    std::optional<RemoteAccessError> error;
    std::optional<hyremote::SessionError> acknowledgedRecoverableError;
    SessionErrorRevision acknowledgedRecoverableRevision;
    std::shared_ptr<std::atomic<std::size_t>> connectedClients =
        std::make_shared<std::atomic<std::size_t>>(0);

    // Owned by the runtime, so it travels with a move-assigned facade: the destination adopts the source's Impl,
    // notifier included, and the notifier of the runtime it replaced is destroyed after that runtime is quiesced.
    std::unique_ptr<RemoteAccessNotifier> notifier = std::make_unique<RemoteAccessNotifier>();

    // What has already been reported. Everything is compared against a freshly derived value rather than a value
    // cached at write time, so there is one source of truth for what a consumer can observe.
    std::optional<RemoteAccessState> publishedState;
    std::optional<std::size_t> publishedClientCount;
    std::optional<RemoteAccessError> publishedError;

    ~Impl() { shutdownRuntime(); }

    RemoteAccessState derivedState() const
    {
        if (!session)
            return RemoteAccessState::Stopped;
        return mapState(session->state());
    }

    std::optional<RemoteAccessError> derivedError() const
    {
        if (session) {
            if (const std::optional<hyremote::SessionError> coreError = session->lastError()) {
                if (!isAcknowledgedRecoverableError(*coreError))
                    return mapError(*coreError);
            }
        }
        return error;
    }

    static bool sameError(const std::optional<RemoteAccessError> &lhs,
                          const std::optional<RemoteAccessError> &rhs)
    {
        if (lhs.has_value() != rhs.has_value())
            return false;
        if (!lhs)
            return true;
        return lhs->code == rhs->code && lhs->message == rhs->message && lhs->recoverable == rhs->recoverable;
    }

    void publishStateAndError(RemoteAccessState currentState, bool diagnosticWritten,
                              const std::optional<RemoteAccessError> &currentError)
    {
        if (!notifier)
            return;

        if (!publishedState || *publishedState != currentState) {
            publishedState = currentState;
            notifier->stateChanged();
        }

        if (diagnosticWritten || !sameError(publishedError, currentError)) {
            publishedError = currentError;
            notifier->errorChanged();
        }
    }

    // Called by Core while it holds its state lock, with the values it already has. Nothing here may query the
    // Session: every getter takes that same non-recursive lock, so doing so deadlocks. An earlier revision of this
    // change did exactly that and the facade tests hung until their timeouts exposed it.
    void publishFromCore(bool diagnosticWritten, RemoteAccessState currentState,
                         const std::optional<RemoteAccessError> &currentError)
    {
        publishStateAndError(currentState, diagnosticWritten, currentError);
    }

    // Called on a transport connection event. Reads only the count this facade owns, for the same reason.
    void publishClientCount()
    {
        if (!notifier)
            return;

        const std::size_t currentClients = connectedClients ? connectedClients->load(std::memory_order_relaxed) : 0;
        if (!publishedClientCount || *publishedClientCount != currentClients) {
            publishedClientCount = currentClients;
            notifier->clientCountChanged();
        }
    }

    // Report whatever really changed. `diagnosticWritten` is set when Core says the diagnostic was written rather
    // than merely re-read, so a recurrence of the same error is still reported even though the derived value is
    // unchanged - an observer that acknowledged the first occurrence has to see the second one.
    void publishChanges(bool diagnosticWritten = false)
    {
        if (!notifier)
            return;

        // Safe here and only here: these run from a call the owner made, where the Session is not being held by Core.
        publishStateAndError(derivedState(), diagnosticWritten, derivedError());
        publishClientCount();
    }

    bool isConfigurable() const
    {
        return !session || session->state() == hyremote::SessionState::Stopped;
    }

    void setError(RemoteAccessErrorCode code, QString message, bool recoverable = false)
    {
        error = RemoteAccessError{code, std::move(message), recoverable};
        publishChanges(true);
    }

    SessionErrorRevision currentSessionErrorRevision() const
    {
        if (!session)
            return {};
        const hyremote::SessionStats stats = session->stats();
        return SessionErrorRevision{stats.inputPostFailures};
    }

    bool isAcknowledgedRecoverableError(const hyremote::SessionError &candidate) const
    {
        if (!candidate.recoverable || !acknowledgedRecoverableError || !session)
            return false;

        const hyremote::SessionError &acknowledged = *acknowledgedRecoverableError;
        if (acknowledged.code != candidate.code || acknowledged.message != candidate.message
            || acknowledged.recoverable != candidate.recoverable) {
            return false;
        }

        const SessionErrorRevision current = currentSessionErrorRevision();
        return current.inputPostFailures == acknowledgedRecoverableRevision.inputPostFailures;
    }

    void resetErrorAcknowledgement() noexcept
    {
        acknowledgedRecoverableError.reset();
        acknowledgedRecoverableRevision = {};
    }

    void acknowledgeCurrentRecoverableError()
    {
        resetErrorAcknowledgement();
        if (!session)
            return;

        const std::optional<hyremote::SessionError> coreError = session->lastError();
        if (!coreError || !coreError->recoverable)
            return;

        acknowledgedRecoverableError = *coreError;
        acknowledgedRecoverableRevision = currentSessionErrorRevision();
    }

    void shutdownRuntime() noexcept
    {
        if (!session) {
            inputSink.reset();
            return;
        }

        // Session::stop() first closes/drains Core's callback gate and makes the transport
        // quiescent. Only then may the target adapter discard queued remote input and balance state
        // that was already delivered to the still-running local Qt application. This is deliberately
        // below the public API and shared by C++, QML and Transparent QPA through RemoteAccess.
        session->stop();
        // Once the runtime is being torn down no further Core notification is wanted; the facade publishes the final
        // state itself, from the call that asked for the teardown.
        session->setChangeCallback({});
        if (inputSink)
            inputSink->shutdown();
        session.reset();
        inputSink.reset();
        resetErrorAcknowledgement();
    }
};

RemoteAccessNotifier::RemoteAccessNotifier(QObject *parent)
    : QObject(parent)
{
}

RemoteAccessNotifier *RemoteAccess::notifier() const noexcept
{
    return m_impl ? m_impl->notifier.get() : nullptr;
}

RemoteAccess::RemoteAccess(QObject *target)
    : m_impl(std::make_unique<Impl>())
{
    // Deliberately no runtime/backend construction here. Creating a RemoteAccess object must not
    // open a listener, allocate a transport runtime, or affect the application's local UI path.
    m_impl->target = target;
}

RemoteAccess::~RemoteAccess()
{
    stop();
}

RemoteAccess::RemoteAccess(RemoteAccess &&) noexcept = default;
RemoteAccess &RemoteAccess::operator=(RemoteAccess &&) noexcept = default;

QObject *RemoteAccess::target() const noexcept
{
    return m_impl ? m_impl->target.data() : nullptr;
}

bool RemoteAccess::setTarget(QObject *target)
{
    if (!m_impl || !m_impl->isConfigurable())
        return false;
    m_impl->target = target;
    return true;
}

QHostAddress RemoteAccess::listenAddress() const
{
    return m_impl ? m_impl->listenAddress : QHostAddress{};
}

bool RemoteAccess::setListenAddress(const QHostAddress &address)
{
    if (!m_impl || !m_impl->isConfigurable())
        return false;
    if (address.isNull()) {
        m_impl->setError(RemoteAccessErrorCode::InvalidConfiguration,
                         QStringLiteral("listen address must not be null"));
        return false;
    }

    m_impl->listenAddress = address;
    return true;
}

quint16 RemoteAccess::port() const noexcept
{
    return m_impl ? m_impl->port : 0;
}

bool RemoteAccess::setPort(quint16 port)
{
    if (!m_impl || !m_impl->isConfigurable())
        return false;
    if (port == 0) {
        m_impl->setError(RemoteAccessErrorCode::InvalidConfiguration,
                         QStringLiteral("port 0 is not part of the stable V0.0.1.0 contract"));
        return false;
    }

    m_impl->port = port;
    return true;
}

bool RemoteAccess::remoteInputEnabled() const noexcept
{
    return m_impl && m_impl->remoteInputEnabled;
}

bool RemoteAccess::setRemoteInputEnabled(bool enabled)
{
    if (!m_impl || !m_impl->isConfigurable())
        return false;
    m_impl->remoteInputEnabled = enabled;
    return true;
}

RemoteSecurityProfile RemoteAccess::securityProfile() const noexcept
{
    return m_impl ? m_impl->securityProfile : RemoteSecurityProfile::Insecure;
}

bool RemoteAccess::setSecurityProfile(RemoteSecurityProfile profile)
{
    if (!m_impl || !m_impl->isConfigurable())
        return false;
    m_impl->securityProfile = profile;
    return true;
}

QString RemoteAccess::securityConfigFile() const
{
    return m_impl ? m_impl->securityConfigFile : QString{};
}

bool RemoteAccess::setSecurityConfigFile(const QString &path)
{
    if (!m_impl || !m_impl->isConfigurable())
        return false;
    m_impl->securityConfigFile = path;
    return true;
}

bool RemoteAccess::start()
{
    if (!m_impl)
        return false;

    if (!m_impl->isConfigurable()) {
        m_impl->setError(RemoteAccessErrorCode::InvalidConfiguration,
                         QStringLiteral("RemoteAccess::start() requires the Stopped state"));
        return false;
    }

    m_impl->error.reset();
    m_impl->resetErrorAcknowledgement();

    // The explicit insecure compatibility profile must never publish a SecurityType None listener
    // beyond loopback. This absorbs the #153 guard while keeping the normal default path unchanged.
    // A secure profile must never silently downgrade if its descriptor/capability is unavailable.
    if (m_impl->securityProfile == RemoteSecurityProfile::Insecure) {
        if (!m_impl->listenAddress.isLoopback()) {
            m_impl->setError(
                RemoteAccessErrorCode::InvalidConfiguration,
                QStringLiteral("refusing a non-loopback listener with the Insecure security profile"));
            return false;
        }
    } else {
        if (m_impl->securityConfigFile.trimmed().isEmpty()) {
            m_impl->setError(RemoteAccessErrorCode::SecurityUnavailable,
                             QStringLiteral("the selected security profile requires a security descriptor"));
            return false;
        }

        // Configuration is frozen before the authenticated transport implementation lands. Refuse
        // before target/transport composition so no listener can be opened and no weaker security
        // mode can be substituted. The next #143 increment replaces this guard with descriptor
        // validation and the real authenticated/encrypted transport path.
        m_impl->setError(RemoteAccessErrorCode::SecurityUnavailable,
                         QStringLiteral("the selected security profile is not available yet; refusing to open a weaker listener"));
        return false;
    }

    QObject *targetObject = m_impl->target.data();
    if (targetObject == nullptr) {
        m_impl->setError(RemoteAccessErrorCode::InvalidConfiguration,
                         QStringLiteral("no live Qt target is attached"));
        return false;
    }

    detail::TargetComponents targetComponents =
        detail::createTargetComponents(targetObject, m_impl->remoteInputEnabled);
    if (!targetComponents.supported || !targetComponents.capture) {
        const QString message = targetComponents.error.isEmpty()
                                    ? QStringLiteral("no HyRemote adapter supports the attached Qt target")
                                    : targetComponents.error;
        m_impl->setError(RemoteAccessErrorCode::TargetAdapterUnavailable, message);
        return false;
    }

    if (m_impl->remoteInputEnabled && !targetComponents.input) {
        m_impl->setError(RemoteAccessErrorCode::RemoteInputUnavailable,
                         targetComponents.error.isEmpty()
                             ? QStringLiteral("remote input was enabled but this target adapter has no input sink")
                             : targetComponents.error);
        return false;
    }

    detail::TransportComponent transport =
        detail::createDefaultTransport(m_impl->listenAddress, m_impl->port);
    if (!transport.transport) {
        const QString message = transport.error.isEmpty()
                                    ? QStringLiteral("no default HyRemote transport is available")
                                    : transport.error;
        m_impl->setError(RemoteAccessErrorCode::TransportUnavailable, message);
        return false;
    }

    // The decorator only observes connection events and the notifier travels with the runtime, so capturing the Impl
    // here is sound: the transport lives inside the session this Impl owns and is torn down before the Impl is.
    Impl *const impl = m_impl.get();
    transport.transport = std::make_unique<ClientCountingTransport>(
        std::move(transport.transport), m_impl->connectedClients, [impl] { impl->publishChanges(); });

    auto session = std::make_unique<hyremote::Session>();
    if (!session->setCaptureSource(std::move(targetComponents.capture))
        || !session->setTransport(std::move(transport.transport))) {
        m_impl->setError(RemoteAccessErrorCode::RuntimeFailure,
                         QStringLiteral("failed to compose the internal HyRemote session"));
        return false;
    }

    std::shared_ptr<hyremote::InputSink> inputSink = targetComponents.input;
    if (inputSink)
        session->setInputSink(inputSink);

    if (!session->start()) {
        const std::optional<hyremote::SessionError> coreError = session->lastError();
        if (coreError)
            m_impl->error = mapError(*coreError);
        else
            m_impl->setError(RemoteAccessErrorCode::StartFailed,
                             QStringLiteral("HyRemote runtime failed to start"));

        // Product-level semantics are simpler than Core's partial-start observability: a failed
        // public start() cleans itself up and returns to Stopped. The error remains queryable.
        session->stop();
        // Published here rather than only on the success path: this start() wrote the diagnostic itself and no Core
        // change callback has been installed yet, so nothing else would report it.
        m_impl->publishChanges(true);
        return false;
    }

    m_impl->inputSink = std::move(inputSink);
    m_impl->session = std::move(session);

    // Core pushes the changes this facade cannot infer from its own calls: a fault raised by a worker, a transport
    // event, a capture target disappearing. They arrive on the thread that caused them with Core's lock held, so the
    // callback takes the values Core hands it and only maps and emits. Everything the facade causes itself is
    // published after the call that caused it.
    m_impl->session->setChangeCallback(
        [impl](hyremote::Session::Change change, hyremote::SessionState state,
               std::optional<hyremote::SessionError> coreError) {
            const std::optional<RemoteAccessError> mappedError =
                coreError ? std::make_optional(mapError(*coreError)) : std::nullopt;
            impl->publishFromCore(change == hyremote::Session::Change::Diagnostic, mapState(state), mappedError);
        });

    m_impl->publishChanges();
    return true;
}

void RemoteAccess::stop() noexcept
{
    if (!m_impl || !m_impl->session)
        return;

    if (const std::optional<hyremote::SessionError> coreError = m_impl->session->lastError()) {
        if (!m_impl->isAcknowledgedRecoverableError(*coreError))
            m_impl->error = mapError(*coreError);
    }

    m_impl->shutdownRuntime();

    // Publish after the teardown, not before: the values a consumer reads from the slot are the final ones.
    m_impl->publishChanges();
}

RemoteAccessState RemoteAccess::state() const
{
    if (!m_impl || !m_impl->session)
        return RemoteAccessState::Stopped;
    return mapState(m_impl->session->state());
}

std::size_t RemoteAccess::connectedClientCount() const noexcept
{
    if (!m_impl || !m_impl->connectedClients)
        return 0;
    return m_impl->connectedClients->load(std::memory_order_relaxed);
}

std::optional<RemoteAccessError> RemoteAccess::lastError() const
{
    if (!m_impl)
        return std::nullopt;

    if (m_impl->session) {
        if (const std::optional<hyremote::SessionError> coreError = m_impl->session->lastError()) {
            if (!m_impl->isAcknowledgedRecoverableError(*coreError))
                return mapError(*coreError);
        }
    }
    return m_impl->error;
}

void RemoteAccess::clearError()
{
    if (!m_impl)
        return;

    m_impl->error.reset();
    // A live non-recoverable Core fault remains the reason the Session is Faulted and cannot be
    // hidden by a UI acknowledgement. Recoverable runtime diagnostics may be acknowledged; only a
    // later occurrence of that recoverable Core error advances its occurrence counter and makes it
    // visible again. Unrelated viewer/capture/transport activity must not resurrect the old error.
    m_impl->acknowledgeCurrentRecoverableError();
    m_impl->publishChanges(true);
}

}  // namespace HyRemote
