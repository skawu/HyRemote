#include "access_instance.hpp"

#include <QAbstractSocket>  // QHostAddress::protocol() answers in this enum, used by the IPv4-only contract
#include <QTimer>
#include <QObject>
#include <QPointer>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "detail/component_factories.hpp"
#include "detail/listener_binding.hpp"
#include "detail/security_descriptor.hpp"
#include "hyremote/core/session.hpp"

namespace HyRemote::Runtime {
namespace {

AccessState mapState(hyremote::SessionState state)
{
    switch (state) {
    case hyremote::SessionState::Stopped:
        return AccessState::Stopped;
    case hyremote::SessionState::Starting:
        return AccessState::Starting;
    case hyremote::SessionState::Running:
        return AccessState::Running;
    case hyremote::SessionState::Stopping:
        return AccessState::Stopping;
    case hyremote::SessionState::Faulted:
        return AccessState::Faulted;
    }
    return AccessState::Faulted;
}

Error mapError(const hyremote::SessionError &error)
{
    Error result;
    result.message = QString::fromStdString(error.message);
    result.recoverable = error.recoverable;

    switch (error.code) {
    case hyremote::SessionErrorCode::InvalidConfiguration:
    case hyremote::SessionErrorCode::IncompatibleFrameCapabilities:
        result.code = ErrorCode::InvalidConfiguration;
        break;
    case hyremote::SessionErrorCode::CaptureStartFailed:
    case hyremote::SessionErrorCode::TransportStartFailed:
        result.code = ErrorCode::StartFailed;
        break;
    case hyremote::SessionErrorCode::StartCancelled:
        result.code = ErrorCode::Cancelled;
        break;
    case hyremote::SessionErrorCode::TargetLost:
    case hyremote::SessionErrorCode::ComponentFailure:
        result.code = ErrorCode::RuntimeFailure;
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

// Wraps the transport so the shared Runtime keeps the one authoritative client count and observes the
// *result* of every transport event rather than racing the Core for it: the Core handler runs first,
// and only then does the Runtime read and publish what the Session made of the event.
//
// A run-scoped activity token makes a callback that belongs to a run that has already stopped inert:
// once stop() has entered, a late event can neither change the count nor publish a notification into
// the next run. That is what keeps a queued notification from a previous run from ever overwriting a
// newer run's values - the property #259 freezes instead of a per-notification run generation.
class ClientCountingTransport final : public hyremote::Transport
{
public:
    ClientCountingTransport(std::unique_ptr<hyremote::Transport> transport,
                            std::shared_ptr<std::atomic<std::size_t>> connectedClients,
                            std::shared_ptr<std::atomic<bool>> runActive,
                            std::function<void()> onRuntimeObservation)
        : m_transport(std::move(transport))
        , m_connectedClients(std::move(connectedClients))
        , m_runActive(std::move(runActive))
        , m_onRuntimeObservation(std::move(onRuntimeObservation))
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
        const auto runActive = m_runActive;
        const auto observe = m_onRuntimeObservation;
        const bool started = m_transport->start(
            [runActive, observe, onInput = std::move(onInput)](const hyremote::InputEvent &event) {
                if (onInput)
                    onInput(event);

                if (!runActive->load(std::memory_order_acquire))
                    return;
                if (observe)
                    observe();
            },
            [connectedClients, runActive, observe, onEvent = std::move(onEvent)](
                const hyremote::TransportEvent &event) mutable {
                if (onEvent)
                    onEvent(event);

                if (!runActive->load(std::memory_order_acquire))
                    return;

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

                if (observe)
                    observe();
            });
        if (!started)
            m_connectedClients->store(0, std::memory_order_relaxed);
        return started;
    }

    void stop() noexcept override
    {
        m_transport->stop();
        m_connectedClients->store(0, std::memory_order_relaxed);
    }

    void enqueueFrame(hyremote::RemoteFrame frame) override
    {
        m_transport->enqueueFrame(std::move(frame));
    }

private:
    std::unique_ptr<hyremote::Transport> m_transport;
    std::shared_ptr<std::atomic<std::size_t>> m_connectedClients;
    std::shared_ptr<std::atomic<bool>> m_runActive;
    std::function<void()> m_onRuntimeObservation;
};

class ObservedCaptureSource final : public hyremote::CaptureSource
{
public:
    ObservedCaptureSource(std::unique_ptr<hyremote::CaptureSource> source,
                          std::shared_ptr<std::atomic<bool>> runActive,
                          std::function<void()> onRuntimeObservation)
        : m_source(std::move(source))
        , m_runActive(std::move(runActive))
        , m_onRuntimeObservation(std::move(onRuntimeObservation))
    {
    }

    hyremote::CaptureCapabilities capabilities() const override
    {
        return m_source->capabilities();
    }

    bool start(hyremote::FrameReadyHandler onFrame, hyremote::CaptureEventHandler onEvent) override
    {
        const auto runActive = m_runActive;
        const auto observe = m_onRuntimeObservation;
        return m_source->start(
            std::move(onFrame),
            [runActive, observe, onEvent = std::move(onEvent)](const hyremote::CaptureEvent &event) mutable {
                if (onEvent)
                    onEvent(event);

                if (!runActive->load(std::memory_order_acquire))
                    return;
                if (observe)
                    observe();
            });
    }

    void stop() noexcept override
    {
        m_source->stop();
    }

    bool requestFrame(const hyremote::CaptureRequest &request) override
    {
        return m_source->requestFrame(request);
    }

private:
    std::unique_ptr<hyremote::CaptureSource> m_source;
    std::shared_ptr<std::atomic<bool>> m_runActive;
    std::function<void()> m_onRuntimeObservation;
};

}  // namespace

class InterfaceWatcher : public QObject
{
public:
    explicit InterfaceWatcher(std::function<void()> onTick)
        : m_onTick(std::move(onTick))
    {
        m_timer.setInterval(2000);
        QObject::connect(&m_timer, &QTimer::timeout, this, [this] { m_onTick(); });
    }

    void start() { m_timer.start(); }
    void stop() { m_timer.stop(); }

private:
    QTimer m_timer;
    std::function<void()> m_onTick;
};

struct AccessInstance::Impl
{
    struct SessionErrorRevision
    {
        std::uint64_t inputPostFailures = 0;
    };

    QPointer<QObject> target;
    QHostAddress listenAddress = QHostAddress::AnyIPv4;
    quint16 port = static_cast<quint16>(HYREMOTE_DEFAULT_PORT);
    bool remoteInputEnabled = false;
    SecurityProfile securityProfile = SecurityProfile::Insecure;
    QString securityConfigFile;
    QString listenInterface;
    std::unique_ptr<hyremote::Session> session;
    std::shared_ptr<hyremote::InputSink> inputSink;
    std::optional<Error> error;
    std::optional<hyremote::SessionError> acknowledgedRecoverableError;
    SessionErrorRevision acknowledgedRecoverableRevision;
    std::shared_ptr<std::atomic<std::size_t>> connectedClients =
        std::make_shared<std::atomic<std::size_t>>(0);

    RuntimeNotificationSink notifications;
    std::optional<AccessState> transitionState;
    std::shared_ptr<std::atomic<bool>> runActive;
    std::uint64_t lastFailureRevision = 0;

    bool desiredActive = false;
    std::optional<QHostAddress> effectiveAddress;
    std::optional<detail::RfbSecurityConfig> activeTransportSecurity;
    std::unique_ptr<InterfaceWatcher> watcher;

    bool composeAndStart(const QHostAddress &address, const detail::RfbSecurityConfig &security);
    bool rebindTo(const QHostAddress &address);
    void reconcileBinding();
    void startInterfaceWatcher();
    void stopInterfaceWatcher();

    ~Impl() { shutdownRuntime(); }

    bool isConfigurable() const
    {
        if (desiredActive)
            return false;
        return !session || session->state() == hyremote::SessionState::Stopped;
    }

    void setError(ErrorCode code, QString message, bool recoverable = false)
    {
        error = Error{code, std::move(message), recoverable};
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
        if (runActive)
            runActive->store(false, std::memory_order_release);

        if (!session) {
            inputSink.reset();
            return;
        }

        session->stop();
        if (inputSink)
            inputSink->shutdown();
        session.reset();
        inputSink.reset();
        resetErrorAcknowledgement();
    }

    AccessState projectedState() const
    {
        if (transitionState)
            return *transitionState;
        if (!session)
            return desiredActive ? AccessState::Unavailable : AccessState::Stopped;
        return mapState(session->state());
    }

    std::optional<Error> effectiveError() const
    {
        if (session) {
            if (const std::optional<hyremote::SessionError> coreError = session->lastError()) {
                if (!isAcknowledgedRecoverableError(*coreError))
                    return mapError(*coreError);
            }
        }
        return error;
    }

    std::uint64_t failureRevision() const
    {
        if (!session)
            return 0;
        const hyremote::SessionStats stats = session->stats();
        return stats.inputPostFailures + stats.captureEventsNonRecoverable + stats.transportEventsFatal;
    }

    void publishSnapshot()
    {
        const std::uint64_t revision = failureRevision();
        const bool newErrorOccurrence = revision != lastFailureRevision;
        lastFailureRevision = revision;

        notifications.publishConnectedClientCount(connectedClients->load(std::memory_order_relaxed));
        notifications.publishError(effectiveError(), newErrorOccurrence);
        notifications.publishState(projectedState());
    }

    void publishSnapshotNoexcept() noexcept
    {
        try {
            publishSnapshot();
        } catch (...) {
        }
    }
};

AccessInstance::AccessInstance(QObject *target)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->target = target;
}

AccessInstance::~AccessInstance()
{
    stop();
}

AccessInstance::AccessInstance(AccessInstance &&) noexcept = default;
AccessInstance &AccessInstance::operator=(AccessInstance &&) noexcept = default;

QObject *AccessInstance::target() const noexcept
{
    return m_impl ? m_impl->target.data() : nullptr;
}

bool AccessInstance::setTarget(QObject *target)
{
    if (!m_impl || !m_impl->isConfigurable())
        return false;
    m_impl->target = target;
    return true;
}

QHostAddress AccessInstance::listenAddress() const
{
    return m_impl ? m_impl->listenAddress : QHostAddress{};
}

bool AccessInstance::setListenAddress(const QHostAddress &address)
{
    if (!m_impl || !m_impl->isConfigurable())
        return false;
    if (address.isNull() || address.protocol() != QAbstractSocket::IPv4Protocol) {
        m_impl->setError(ErrorCode::InvalidConfiguration,
                         address.isNull()
                             ? QStringLiteral("listen address must not be null")
                             : QStringLiteral("listen address must be an IPv4 address"));
        m_impl->publishSnapshotNoexcept();
        return false;
    }

    m_impl->listenAddress = address;
    m_impl->listenInterface.clear();
    return true;
}

QString AccessInstance::listenInterface() const
{
    return m_impl ? m_impl->listenInterface : QString{};
}

bool AccessInstance::setListenInterface(const QString &identity)
{
    if (!m_impl || !m_impl->isConfigurable())
        return false;
    if (identity.isEmpty()) {
        m_impl->setError(ErrorCode::InvalidConfiguration,
                         QStringLiteral("listen interface must not be empty; set an address to use the address modes"));
        m_impl->publishSnapshotNoexcept();
        return false;
    }

    m_impl->listenInterface = identity;
    return true;
}

quint16 AccessInstance::port() const noexcept
{
    return m_impl ? m_impl->port : 0;
}

bool AccessInstance::setPort(quint16 port)
{
    if (!m_impl || !m_impl->isConfigurable())
        return false;
    if (port == 0) {
        m_impl->setError(ErrorCode::InvalidConfiguration,
                         QStringLiteral("port 0 is invalid: a configured listener port must be between 1 and 65535"));
        m_impl->publishSnapshotNoexcept();
        return false;
    }

    m_impl->port = port;
    return true;
}

bool AccessInstance::remoteInputEnabled() const noexcept
{
    return m_impl && m_impl->remoteInputEnabled;
}

bool AccessInstance::setRemoteInputEnabled(bool enabled)
{
    if (!m_impl || !m_impl->isConfigurable())
        return false;
    m_impl->remoteInputEnabled = enabled;
    return true;
}

SecurityProfile AccessInstance::securityProfile() const noexcept
{
    return m_impl ? m_impl->securityProfile : SecurityProfile::Insecure;
}

bool AccessInstance::setSecurityProfile(SecurityProfile profile)
{
    if (!m_impl || !m_impl->isConfigurable())
        return false;
    m_impl->securityProfile = profile;
    return true;
}

QString AccessInstance::securityConfigFile() const
{
    return m_impl ? m_impl->securityConfigFile : QString{};
}

bool AccessInstance::setSecurityConfigFile(const QString &path)
{
    if (!m_impl || !m_impl->isConfigurable())
        return false;
    m_impl->securityConfigFile = path;
    return true;
}

bool AccessInstance::start()
{
    if (!m_impl)
        return false;

    struct ExitPublication
    {
        Impl *impl;

        ~ExitPublication()
        {
            impl->transitionState.reset();
            impl->publishSnapshotNoexcept();
        }
    } exitPublication{m_impl.get()};

    if (!m_impl->isConfigurable()) {
        m_impl->setError(ErrorCode::InvalidConfiguration,
                         QStringLiteral("runtime start requires the Stopped state"));
        return false;
    }

    m_impl->transitionState = AccessState::Starting;
    m_impl->lastFailureRevision = 0;
    m_impl->error.reset();
    m_impl->resetErrorAcknowledgement();
    m_impl->publishSnapshotNoexcept();

    QHostAddress effectiveAddress = m_impl->listenAddress;
    const detail::ListenerMode bindingMode =
        detail::listenerModeFor(m_impl->listenAddress, m_impl->listenInterface);
    if (bindingMode == detail::ListenerMode::Address
        && !detail::isLocalIpv4Address(m_impl->listenAddress)) {
        m_impl->setError(ErrorCode::InvalidConfiguration,
                         QStringLiteral("listen address %1 does not belong to this host")
                             .arg(m_impl->listenAddress.toString()));
        return false;
    }
    if (bindingMode == detail::ListenerMode::Interface) {
        const detail::InterfaceResolutionResult resolution =
            detail::resolveInterfaceAddress(m_impl->listenInterface);
        if (resolution.status != detail::InterfaceResolution::Found) {
            m_impl->setError(ErrorCode::TransportUnavailable, resolution.error);
            return false;
        }
        effectiveAddress = resolution.address;
    }

    detail::RfbSecurityConfig transportSecurity;
    if (m_impl->securityProfile != SecurityProfile::Insecure) {
        if (m_impl->securityProfile == SecurityProfile::AuthenticatedEncrypted) {
            m_impl->setError(
                ErrorCode::SecurityUnavailable,
                QStringLiteral("AuthenticatedEncrypted requires the final VeNCrypt/TLS transport, which this "
                               "release does not provide; refusing to start instead of falling back to a weaker "
                               "listener"));
            return false;
        }

        if (m_impl->securityConfigFile.trimmed().isEmpty()) {
            m_impl->setError(ErrorCode::SecurityUnavailable,
                             QStringLiteral("the selected security profile requires a security descriptor"));
            return false;
        }

#ifndef HYREMOTE_HAS_TRANSPORT_SECURITY
        m_impl->setError(ErrorCode::SecurityUnavailable,
                         QStringLiteral("the selected security profile needs the transport-security capability, "
                                        "which is not compiled into this build"));
        return false;
#else
        QString descriptorError;
        std::optional<detail::SecurityDescriptor> descriptor =
            detail::loadSecurityDescriptor(m_impl->securityConfigFile, m_impl->securityProfile, descriptorError);
        if (!descriptor) {
            m_impl->setError(ErrorCode::InvalidConfiguration,
                             descriptorError.isEmpty()
                                 ? QStringLiteral("the security descriptor could not be loaded")
                                 : descriptorError);
            return false;
        }

        transportSecurity.vncAuthenticationRequired = true;
        transportSecurity.password = descriptor->password;
#endif
    }

    m_impl->activeTransportSecurity = transportSecurity;

    if (!m_impl->composeAndStart(effectiveAddress, transportSecurity))
        return false;

    m_impl->desiredActive = true;
    m_impl->effectiveAddress = effectiveAddress;
    if (bindingMode == detail::ListenerMode::Interface)
        m_impl->startInterfaceWatcher();
    return true;
}

void AccessInstance::reconcileInterfaceBinding()
{
    if (m_impl)
        m_impl->reconcileBinding();
}

void AccessInstance::stop() noexcept
{
    if (!m_impl)
        return;

    m_impl->desiredActive = false;
    m_impl->stopInterfaceWatcher();
    m_impl->effectiveAddress.reset();
    m_impl->activeTransportSecurity.reset();

    if (!m_impl->session) {
        m_impl->transitionState.reset();
        m_impl->publishSnapshotNoexcept();
        return;
    }

    m_impl->transitionState = AccessState::Stopping;
    m_impl->publishSnapshotNoexcept();

    try {
        if (const std::optional<hyremote::SessionError> coreError = m_impl->session->lastError()) {
            if (!m_impl->isAcknowledgedRecoverableError(*coreError))
                m_impl->error = mapError(*coreError);
        }
    } catch (...) {
    }

    m_impl->shutdownRuntime();

    m_impl->transitionState.reset();
    m_impl->publishSnapshotNoexcept();
}

AccessState AccessInstance::state() const
{
    if (!m_impl)
        return AccessState::Stopped;
    return m_impl->projectedState();
}

std::size_t AccessInstance::connectedClientCount() const noexcept
{
    if (!m_impl || !m_impl->connectedClients)
        return 0;
    return m_impl->connectedClients->load(std::memory_order_relaxed);
}

std::optional<Error> AccessInstance::lastError() const
{
    if (!m_impl)
        return std::nullopt;

    return m_impl->effectiveError();
}

DiagnosticSnapshot AccessInstance::diagnosticSnapshot() const
{
    DiagnosticSnapshot result;
    if (!m_impl)
        return result;

    result.state = m_impl->projectedState();
    result.configuredListenAddress = m_impl->listenAddress;
    result.configuredListenInterface = m_impl->listenInterface;
    result.configuredPort = m_impl->port;
    result.configuredSecurityProfile = m_impl->securityProfile;
    result.remoteInputEnabled = m_impl->remoteInputEnabled;
    result.connectedClientCount = m_impl->connectedClients
                                      ? m_impl->connectedClients->load(std::memory_order_relaxed)
                                      : 0;
    result.lastError = m_impl->effectiveError();

    // Effective facts are observations, never configuration guesses. The runtime is fail-closed, so a
    // successful Running state proves the configured security profile is the one actually satisfied.
    if (result.state == AccessState::Running && m_impl->effectiveAddress) {
        result.effectiveListenAddress = m_impl->effectiveAddress;
        result.effectivePort = m_impl->port;
        result.effectiveSecurityProfile = m_impl->securityProfile;
    }

    return result;
}

void AccessInstance::clearError()
{
    if (!m_impl)
        return;

    m_impl->error.reset();
    m_impl->acknowledgeCurrentRecoverableError();
    m_impl->publishSnapshotNoexcept();
}

RuntimeNotificationToken AccessInstance::subscribeNotifications(RuntimeNotificationHandlers handlers)
{
    if (!m_impl)
        return RuntimeNotificationToken{};
    return m_impl->notifications.subscribe(std::move(handlers));
}

void AccessInstance::unsubscribeNotifications(RuntimeNotificationToken token) noexcept
{
    if (!m_impl)
        return;
    m_impl->notifications.unsubscribe(token);
}

bool AccessInstance::Impl::composeAndStart(const QHostAddress &address, const detail::RfbSecurityConfig &security)
{
    shutdownRuntime();

    QObject *targetObject = target.data();
    if (targetObject == nullptr) {
        setError(ErrorCode::InvalidConfiguration, QStringLiteral("no live Qt target is attached"));
        return false;
    }

    detail::TargetComponents targetComponents = detail::createTargetComponents(targetObject, remoteInputEnabled);
    if (!targetComponents.supported || !targetComponents.capture) {
        const QString message = targetComponents.error.isEmpty()
                                    ? QStringLiteral("no HyRemote adapter supports the attached Qt target")
                                    : targetComponents.error;
        setError(ErrorCode::TargetAdapterUnavailable, message);
        return false;
    }

    if (remoteInputEnabled && !targetComponents.input) {
        setError(ErrorCode::RemoteInputUnavailable,
                 targetComponents.error.isEmpty()
                     ? QStringLiteral("remote input was enabled but this target adapter has no input sink")
                     : targetComponents.error);
        return false;
    }

    detail::TransportComponent transport = detail::createDefaultTransport(address, port, security);
    if (!transport.transport) {
        const QString message = transport.error.isEmpty()
                                    ? QStringLiteral("no default HyRemote transport is available")
                                    : transport.error;
        setError(ErrorCode::TransportUnavailable, message);
        return false;
    }

    runActive = std::make_shared<std::atomic<bool>>(false);
    const auto observeRuntime = [impl = this] { impl->publishSnapshotNoexcept(); };

    transport.transport = std::make_unique<ClientCountingTransport>(std::move(transport.transport),
                                                                    connectedClients,
                                                                    runActive,
                                                                    observeRuntime);

    auto freshSession = std::make_unique<hyremote::Session>();
    auto observedCapture =
        std::make_unique<ObservedCaptureSource>(std::move(targetComponents.capture), runActive, observeRuntime);
    if (!freshSession->setCaptureSource(std::move(observedCapture))
        || !freshSession->setTransport(std::move(transport.transport))) {
        setError(ErrorCode::RuntimeFailure, QStringLiteral("failed to compose the internal HyRemote session"));
        return false;
    }

    std::shared_ptr<hyremote::InputSink> freshInputSink = targetComponents.input;
    if (freshInputSink)
        freshSession->setInputSink(freshInputSink);

    runActive->store(true, std::memory_order_release);
    if (!freshSession->start()) {
        runActive->store(false, std::memory_order_release);
        if (const std::optional<hyremote::SessionError> coreError = freshSession->lastError())
            error = mapError(*coreError);
        else
            setError(ErrorCode::StartFailed, QStringLiteral("HyRemote runtime failed to start"));

        freshSession->stop();
        return false;
    }

    inputSink = std::move(freshInputSink);
    session = std::move(freshSession);
    return true;
}

bool AccessInstance::Impl::rebindTo(const QHostAddress &address)
{
    transitionState = AccessState::Starting;
    lastFailureRevision = 0;
    error.reset();
    resetErrorAcknowledgement();
    publishSnapshotNoexcept();

    const detail::RfbSecurityConfig security =
        activeTransportSecurity ? *activeTransportSecurity : detail::RfbSecurityConfig{};

    if (composeAndStart(address, security)) {
        effectiveAddress = address;
        error.reset();
    } else {
        effectiveAddress.reset();
    }

    transitionState.reset();
    publishSnapshotNoexcept();
    return effectiveAddress.has_value() && *effectiveAddress == address;
}

void AccessInstance::Impl::reconcileBinding()
{
    if (!desiredActive)
        return;
    if (detail::listenerModeFor(listenAddress, listenInterface) != detail::ListenerMode::Interface)
        return;

    const detail::InterfaceResolutionResult resolution = detail::resolveInterfaceAddress(listenInterface);

    if (resolution.status == detail::InterfaceResolution::Found) {
        if (effectiveAddress && *effectiveAddress == resolution.address)
            return;
        rebindTo(resolution.address);
        return;
    }

    shutdownRuntime();
    effectiveAddress.reset();
    setError(ErrorCode::TransportUnavailable, resolution.error);
    publishSnapshotNoexcept();
}

void AccessInstance::Impl::startInterfaceWatcher()
{
    if (!watcher)
        watcher = std::make_unique<InterfaceWatcher>([this] { reconcileBinding(); });
    watcher->start();
}

void AccessInstance::Impl::stopInterfaceWatcher()
{
    if (watcher)
        watcher->stop();
}

}  // namespace HyRemote::Runtime
