#include "access_instance.hpp"

#include <QObject>
#include <QPointer>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "detail/component_factories.hpp"
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

class ClientCountingTransport final : public hyremote::Transport
{
public:
    ClientCountingTransport(std::unique_ptr<hyremote::Transport> transport,
                            std::shared_ptr<std::atomic<std::size_t>> connectedClients)
        : m_transport(std::move(transport))
        , m_connectedClients(std::move(connectedClients))
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
            [connectedClients, onEvent = std::move(onEvent)](const hyremote::TransportEvent &event) mutable {
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

                if (onEvent)
                    onEvent(event);
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
};

}  // namespace

struct AccessInstance::Impl
{
    struct SessionErrorRevision
    {
        std::uint64_t inputPostFailures = 0;
    };

    QPointer<QObject> target;
    QHostAddress listenAddress = QHostAddress::LocalHost;
    quint16 port = static_cast<quint16>(HYREMOTE_DEFAULT_PORT);
    bool remoteInputEnabled = false;
    SecurityProfile securityProfile = SecurityProfile::Insecure;
    QString securityConfigFile;
    std::unique_ptr<hyremote::Session> session;
    std::shared_ptr<hyremote::InputSink> inputSink;
    std::optional<Error> error;
    std::optional<hyremote::SessionError> acknowledgedRecoverableError;
    SessionErrorRevision acknowledgedRecoverableRevision;
    std::shared_ptr<std::atomic<std::size_t>> connectedClients =
        std::make_shared<std::atomic<std::size_t>>(0);

    ~Impl() { shutdownRuntime(); }

    bool isConfigurable() const
    {
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
        if (!session) {
            inputSink.reset();
            return;
        }

        // Session becomes quiescent before the target input sink discards pending work and balances
        // already-delivered held state. This lifecycle ordering is shared by every integration frontend.
        session->stop();
        if (inputSink)
            inputSink->shutdown();
        session.reset();
        inputSink.reset();
        resetErrorAcknowledgement();
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
    if (address.isNull()) {
        m_impl->setError(ErrorCode::InvalidConfiguration,
                         QStringLiteral("listen address must not be null"));
        return false;
    }

    m_impl->listenAddress = address;
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
        // A version-neutral, factual statement: the rejected value, the accepted range and nothing about a release
        // label. A retired planning label has no place in a user-visible product error.
        m_impl->setError(ErrorCode::InvalidConfiguration,
                         QStringLiteral("port 0 is invalid: a configured listener port must be between 1 and 65535"));
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

    if (!m_impl->isConfigurable()) {
        m_impl->setError(ErrorCode::InvalidConfiguration,
                         QStringLiteral("runtime start requires the Stopped state"));
        return false;
    }

    m_impl->error.reset();
    m_impl->resetErrorAcknowledgement();

    detail::RfbSecurityConfig transportSecurity;
    bool authenticationEnabled = false;
    if (m_impl->securityProfile != SecurityProfile::Insecure) {
        // AuthenticatedEncrypted is a declared product API, but its wire contract (VeNCrypt 0.2 + X509Vnc +
        // TLS >= 1.2) does not exist yet. It therefore has to fail closed deterministically and
        // *independently of the transport-security capability*: a build that can perform VNC Authentication
        // must not accept the encrypted profile and then serve the weaker mechanism, and a readable
        // certificate/private-key descriptor must not make the profile look implemented. Descriptor validity
        // and backend capability are separate facts; this one is about the backend. The refusal happens here,
        // before any listener or transport is composed, and it never falls back to Authenticated or Insecure.
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

        authenticationEnabled = true;
        transportSecurity.vncAuthenticationRequired = true;
        transportSecurity.password = descriptor->password;
#endif
    }

    if (!authenticationEnabled && !m_impl->listenAddress.isLoopback()) {
        m_impl->setError(
            ErrorCode::InvalidConfiguration,
            QStringLiteral("refusing a non-loopback listener while no authentication mode is enabled"));
        return false;
    }

    QObject *targetObject = m_impl->target.data();
    if (targetObject == nullptr) {
        m_impl->setError(ErrorCode::InvalidConfiguration,
                         QStringLiteral("no live Qt target is attached"));
        return false;
    }

    detail::TargetComponents targetComponents =
        detail::createTargetComponents(targetObject, m_impl->remoteInputEnabled);
    if (!targetComponents.supported || !targetComponents.capture) {
        const QString message = targetComponents.error.isEmpty()
                                    ? QStringLiteral("no HyRemote adapter supports the attached Qt target")
                                    : targetComponents.error;
        m_impl->setError(ErrorCode::TargetAdapterUnavailable, message);
        return false;
    }

    if (m_impl->remoteInputEnabled && !targetComponents.input) {
        m_impl->setError(ErrorCode::RemoteInputUnavailable,
                         targetComponents.error.isEmpty()
                             ? QStringLiteral("remote input was enabled but this target adapter has no input sink")
                             : targetComponents.error);
        return false;
    }

    detail::TransportComponent transport =
        detail::createDefaultTransport(m_impl->listenAddress, m_impl->port, transportSecurity);
    if (!transport.transport) {
        const QString message = transport.error.isEmpty()
                                    ? QStringLiteral("no default HyRemote transport is available")
                                    : transport.error;
        m_impl->setError(ErrorCode::TransportUnavailable, message);
        return false;
    }

    transport.transport = std::make_unique<ClientCountingTransport>(
        std::move(transport.transport), m_impl->connectedClients);

    auto session = std::make_unique<hyremote::Session>();
    if (!session->setCaptureSource(std::move(targetComponents.capture))
        || !session->setTransport(std::move(transport.transport))) {
        m_impl->setError(ErrorCode::RuntimeFailure,
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
            m_impl->setError(ErrorCode::StartFailed,
                             QStringLiteral("HyRemote runtime failed to start"));

        session->stop();
        return false;
    }

    m_impl->inputSink = std::move(inputSink);
    m_impl->session = std::move(session);
    return true;
}

void AccessInstance::stop() noexcept
{
    if (!m_impl || !m_impl->session)
        return;

    try {
        if (const std::optional<hyremote::SessionError> coreError = m_impl->session->lastError()) {
            if (!m_impl->isAcknowledgedRecoverableError(*coreError))
                m_impl->error = mapError(*coreError);
        }
    } catch (...) {
        // Diagnostics are best effort during noexcept teardown; quiescence is unconditional.
    }

    m_impl->shutdownRuntime();
}

AccessState AccessInstance::state() const
{
    if (!m_impl || !m_impl->session)
        return AccessState::Stopped;
    return mapState(m_impl->session->state());
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

    if (m_impl->session) {
        if (const std::optional<hyremote::SessionError> coreError = m_impl->session->lastError()) {
            if (!m_impl->isAcknowledgedRecoverableError(*coreError))
                return mapError(*coreError);
        }
    }
    return m_impl->error;
}

void AccessInstance::clearError()
{
    if (!m_impl)
        return;

    m_impl->error.reset();
    m_impl->acknowledgeCurrentRecoverableError();
}

}  // namespace HyRemote::Runtime
