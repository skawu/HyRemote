#include <HyRemote/RemoteAccess.h>

#include <QObject>
#include <QPointer>

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

}  // namespace

struct RemoteAccess::Impl
{
    QPointer<QObject> target;
    QHostAddress listenAddress = QHostAddress::LocalHost;
    quint16 port = 5900;
    bool remoteInputEnabled = false;
    std::unique_ptr<hyremote::Session> session;
    std::optional<RemoteAccessError> error;

    bool isConfigurable() const
    {
        return !session || session->state() == hyremote::SessionState::Stopped;
    }

    void setError(RemoteAccessErrorCode code, QString message, bool recoverable = false)
    {
        error = RemoteAccessError{code, std::move(message), recoverable};
    }
};

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

    auto session = std::make_unique<hyremote::Session>();
    if (!session->setCaptureSource(std::move(targetComponents.capture))
        || !session->setTransport(std::move(transport.transport))) {
        m_impl->setError(RemoteAccessErrorCode::RuntimeFailure,
                         QStringLiteral("failed to compose the internal HyRemote session"));
        return false;
    }
    if (targetComponents.input)
        session->setInputSink(std::move(targetComponents.input));

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
        return false;
    }

    m_impl->session = std::move(session);
    return true;
}

void RemoteAccess::stop() noexcept
{
    if (!m_impl || !m_impl->session)
        return;

    // `noexcept` is part of the contract: an exception escaping here terminates the host process. The
    // body builds diagnostics (QString/QtError through mapError) and stops the Session, so a bad_alloc
    // is the realistic failure mode and it must not escape.
    try {
        if (const std::optional<hyremote::SessionError> coreError = m_impl->session->lastError())
            m_impl->error = mapError(*coreError);

        m_impl->session->stop();
        m_impl->session.reset();
    } catch (...) {
        // Nothing can be reported from inside a noexcept teardown; the Session has already published
        // whatever terminal transition it reached.
    }
}

RemoteAccessState RemoteAccess::state() const
{
    if (!m_impl || !m_impl->session)
        return RemoteAccessState::Stopped;
    return mapState(m_impl->session->state());
}

std::optional<RemoteAccessError> RemoteAccess::lastError() const
{
    if (!m_impl)
        return std::nullopt;

    if (m_impl->session) {
        if (const std::optional<hyremote::SessionError> coreError = m_impl->session->lastError())
            return mapError(*coreError);
    }
    return m_impl->error;
}

void RemoteAccess::clearError()
{
    if (m_impl)
        m_impl->error.reset();
}

}  // namespace HyRemote
