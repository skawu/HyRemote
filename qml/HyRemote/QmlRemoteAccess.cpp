#include "QmlRemoteAccess.h"

#include <HyRemote/RemoteAccess.h>

#include <QHostAddress>

#include <optional>
#include <utility>

namespace HyRemote::Qml {
namespace {

QmlRemoteAccess::State mapState(::HyRemote::RemoteAccessState state)
{
    switch (state) {
    case ::HyRemote::RemoteAccessState::Stopped:
        return QmlRemoteAccess::Stopped;
    case ::HyRemote::RemoteAccessState::Starting:
        return QmlRemoteAccess::Starting;
    case ::HyRemote::RemoteAccessState::Running:
        return QmlRemoteAccess::Running;
    case ::HyRemote::RemoteAccessState::Stopping:
        return QmlRemoteAccess::Stopping;
    case ::HyRemote::RemoteAccessState::Faulted:
        return QmlRemoteAccess::Faulted;
    }
    return QmlRemoteAccess::Faulted;
}

QmlRemoteAccess::ErrorCode mapErrorCode(::HyRemote::RemoteAccessErrorCode code)
{
    switch (code) {
    case ::HyRemote::RemoteAccessErrorCode::InvalidConfiguration:
        return QmlRemoteAccess::InvalidConfiguration;
    case ::HyRemote::RemoteAccessErrorCode::TargetAdapterUnavailable:
        return QmlRemoteAccess::TargetAdapterUnavailable;
    case ::HyRemote::RemoteAccessErrorCode::TransportUnavailable:
        return QmlRemoteAccess::TransportUnavailable;
    case ::HyRemote::RemoteAccessErrorCode::RemoteInputUnavailable:
        return QmlRemoteAccess::RemoteInputUnavailable;
    case ::HyRemote::RemoteAccessErrorCode::StartFailed:
        return QmlRemoteAccess::StartFailed;
    case ::HyRemote::RemoteAccessErrorCode::RuntimeFailure:
        return QmlRemoteAccess::RuntimeFailure;
    case ::HyRemote::RemoteAccessErrorCode::Cancelled:
        return QmlRemoteAccess::Cancelled;
    }
    return QmlRemoteAccess::RuntimeFailure;
}

}  // namespace

QmlRemoteAccess::QmlRemoteAccess(QObject *parent)
    : QObject(parent)
    , m_access(std::make_unique<::HyRemote::RemoteAccess>())
{
    // RemoteAccess construction is deliberately inert. QML may request enabled=true during object
    // creation, but the wrapper defers the actual start until componentComplete() so initial target
    // and policy bindings can settle first.
    m_pollTimer.setInterval(100);
    m_pollTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_pollTimer, &QTimer::timeout, this, &QmlRemoteAccess::refreshRuntimeSnapshot);
    refreshRuntimeSnapshot();
}

QmlRemoteAccess::~QmlRemoteAccess()
{
    m_pollTimer.stop();
    if (m_access)
        m_access->stop();
}

void QmlRemoteAccess::classBegin()
{
    // Intentionally inert. Initial QML property setters may run after this callback.
}

void QmlRemoteAccess::componentComplete()
{
    if (m_componentComplete)
        return;

    m_componentComplete = true;
    if (!m_enabled)
        return;

    // m_enabled represents the declarative request while QML is being constructed. Once complete,
    // convert it transactionally into real runtime state. A failed start rolls the property back.
    if (!startRuntime()) {
        m_enabled = false;
        emit enabledChanged();
    }
}

QObject *QmlRemoteAccess::target() const noexcept
{
    return m_access ? m_access->target() : nullptr;
}

void QmlRemoteAccess::setTarget(QObject *targetObject)
{
    if (!m_access || targetObject == target())
        return;
    if (!m_access->setTarget(targetObject)) {
        setLocalError(InvalidConfiguration,
                      QStringLiteral("target can only be changed while remote access is stopped"));
        return;
    }
    clearLocalError();
    emit targetChanged();
}

bool QmlRemoteAccess::enabled() const noexcept
{
    return m_enabled;
}

bool QmlRemoteAccess::startRuntime()
{
    if (!m_access)
        return false;

    clearLocalError();
    if (!m_access->start()) {
        refreshRuntimeSnapshot();
        return false;
    }

    m_pollTimer.start();
    refreshRuntimeSnapshot();
    return true;
}

void QmlRemoteAccess::setEnabled(bool enabledValue)
{
    if (!m_access || enabledValue == m_enabled)
        return;

    if (!m_componentComplete) {
        // During QML construction this is a request only. This preserves inert construction and
        // avoids depending on target/property assignment order.
        m_enabled = enabledValue;
        emit enabledChanged();
        return;
    }

    if (enabledValue) {
        if (!startRuntime()) {
            // Transactional semantics: a failed start leaves enabled=false.
            emit enabledChanged();
            return;
        }
        m_enabled = true;
        emit enabledChanged();
        return;
    }

    m_access->stop();
    m_enabled = false;
    m_pollTimer.stop();
    emit enabledChanged();
    refreshRuntimeSnapshot();
}

QString QmlRemoteAccess::listenAddress() const
{
    return m_access ? m_access->listenAddress().toString() : QString{};
}

void QmlRemoteAccess::setListenAddress(const QString &addressText)
{
    if (!m_access)
        return;

    const QHostAddress address(addressText.trimmed());
    if (address.isNull()) {
        setLocalError(InvalidConfiguration,
                      QStringLiteral("listenAddress must be a valid IP address"));
        return;
    }
    if (address == m_access->listenAddress())
        return;
    if (!m_access->setListenAddress(address)) {
        setLocalError(InvalidConfiguration,
                      QStringLiteral("listenAddress can only be changed while remote access is stopped"));
        return;
    }
    clearLocalError();
    emit listenAddressChanged();
}

int QmlRemoteAccess::port() const noexcept
{
    return m_access ? static_cast<int>(m_access->port()) : 0;
}

void QmlRemoteAccess::setPort(int portValue)
{
    if (!m_access)
        return;
    if (portValue < 1 || portValue > 65535) {
        setLocalError(InvalidConfiguration,
                      QStringLiteral("port must be between 1 and 65535"));
        return;
    }
    if (portValue == port())
        return;
    if (!m_access->setPort(static_cast<quint16>(portValue))) {
        setLocalError(InvalidConfiguration,
                      QStringLiteral("port can only be changed while remote access is stopped"));
        return;
    }
    clearLocalError();
    emit portChanged();
}

bool QmlRemoteAccess::remoteInputEnabled() const noexcept
{
    return m_access && m_access->remoteInputEnabled();
}

void QmlRemoteAccess::setRemoteInputEnabled(bool enabledValue)
{
    if (!m_access || enabledValue == remoteInputEnabled())
        return;
    if (!m_access->setRemoteInputEnabled(enabledValue)) {
        setLocalError(
            InvalidConfiguration,
            QStringLiteral("remoteInputEnabled can only be changed while remote access is stopped"));
        return;
    }
    clearLocalError();
    emit remoteInputEnabledChanged();
}

QmlRemoteAccess::State QmlRemoteAccess::state() const noexcept
{
    return m_state;
}

quint64 QmlRemoteAccess::connectedClientCount() const noexcept
{
    return m_connectedClientCount;
}

QString QmlRemoteAccess::errorString() const
{
    return m_errorString;
}

QmlRemoteAccess::ErrorCode QmlRemoteAccess::errorCode() const noexcept
{
    return m_errorCode;
}

bool QmlRemoteAccess::recoverableError() const noexcept
{
    return m_recoverableError;
}

void QmlRemoteAccess::clearError()
{
    if (m_access)
        m_access->clearError();
    clearLocalError();
    refreshRuntimeSnapshot();
}

void QmlRemoteAccess::refreshRuntimeSnapshot()
{
    if (!m_access)
        return;

    const State nextState = mapState(m_access->state());
    if (nextState != m_state) {
        m_state = nextState;
        emit stateChanged();
    }

    const quint64 nextConnectedClientCount =
        static_cast<quint64>(m_access->connectedClientCount());
    if (nextConnectedClientCount != m_connectedClientCount) {
        m_connectedClientCount = nextConnectedClientCount;
        emit connectedClientCountChanged();
    }

    const std::optional<::HyRemote::RemoteAccessError> runtimeError = m_access->lastError();
    if (!runtimeError)
        return;

    const ErrorCode nextCode = mapErrorCode(runtimeError->code);
    if (nextCode == m_errorCode && runtimeError->message == m_errorString
        && runtimeError->recoverable == m_recoverableError) {
        return;
    }

    m_errorCode = nextCode;
    m_errorString = runtimeError->message;
    m_recoverableError = runtimeError->recoverable;
    emit errorChanged();
}

void QmlRemoteAccess::setLocalError(ErrorCode code, QString message, bool recoverable)
{
    if (m_errorCode == code && m_errorString == message && m_recoverableError == recoverable)
        return;
    m_errorCode = code;
    m_errorString = std::move(message);
    m_recoverableError = recoverable;
    emit errorChanged();
}

void QmlRemoteAccess::clearLocalError()
{
    if (m_errorCode == NoError && m_errorString.isEmpty() && !m_recoverableError)
        return;
    m_errorCode = NoError;
    m_errorString.clear();
    m_recoverableError = false;
    emit errorChanged();
}

}  // namespace HyRemote::Qml
