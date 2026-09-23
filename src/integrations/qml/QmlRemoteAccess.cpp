#include "QmlRemoteAccess.h"

#include "access_instance.hpp"

#include <QHostAddress>
#include <QMetaObject>
#include <QPointer>
#include <QThread>

#include <optional>
#include <utility>

namespace HyRemote::Qml {
namespace {

QmlRemoteAccess::State mapState(::HyRemote::Runtime::AccessState state)
{
    switch (state) {
    case ::HyRemote::Runtime::AccessState::Stopped:
        return QmlRemoteAccess::Stopped;
    case ::HyRemote::Runtime::AccessState::Starting:
        return QmlRemoteAccess::Starting;
    case ::HyRemote::Runtime::AccessState::Running:
        return QmlRemoteAccess::Running;
    case ::HyRemote::Runtime::AccessState::Stopping:
        return QmlRemoteAccess::Stopping;
    case ::HyRemote::Runtime::AccessState::Faulted:
        return QmlRemoteAccess::Faulted;
    case ::HyRemote::Runtime::AccessState::Unavailable:
        return QmlRemoteAccess::Unavailable;
    }
    return QmlRemoteAccess::Faulted;
}

::HyRemote::Runtime::SecurityProfile mapSecurityProfile(QmlRemoteAccess::SecurityProfile profile)
{
    switch (profile) {
    case QmlRemoteAccess::Insecure:
        return ::HyRemote::Runtime::SecurityProfile::Insecure;
    case QmlRemoteAccess::Authenticated:
        return ::HyRemote::Runtime::SecurityProfile::Authenticated;
    case QmlRemoteAccess::AuthenticatedEncrypted:
        return ::HyRemote::Runtime::SecurityProfile::AuthenticatedEncrypted;
    }
    return ::HyRemote::Runtime::SecurityProfile::Insecure;
}

QmlRemoteAccess::SecurityProfile mapSecurityProfile(::HyRemote::Runtime::SecurityProfile profile)
{
    switch (profile) {
    case ::HyRemote::Runtime::SecurityProfile::Insecure:
        return QmlRemoteAccess::Insecure;
    case ::HyRemote::Runtime::SecurityProfile::Authenticated:
        return QmlRemoteAccess::Authenticated;
    case ::HyRemote::Runtime::SecurityProfile::AuthenticatedEncrypted:
        return QmlRemoteAccess::AuthenticatedEncrypted;
    }
    return QmlRemoteAccess::Insecure;
}

QmlRemoteAccess::ErrorCode mapErrorCode(::HyRemote::Runtime::ErrorCode code)
{
    switch (code) {
    case ::HyRemote::Runtime::ErrorCode::InvalidConfiguration:
        return QmlRemoteAccess::InvalidConfiguration;
    case ::HyRemote::Runtime::ErrorCode::TargetAdapterUnavailable:
        return QmlRemoteAccess::TargetAdapterUnavailable;
    case ::HyRemote::Runtime::ErrorCode::TransportUnavailable:
        return QmlRemoteAccess::TransportUnavailable;
    case ::HyRemote::Runtime::ErrorCode::RemoteInputUnavailable:
        return QmlRemoteAccess::RemoteInputUnavailable;
    case ::HyRemote::Runtime::ErrorCode::SecurityUnavailable:
        return QmlRemoteAccess::SecurityUnavailable;
    case ::HyRemote::Runtime::ErrorCode::StartFailed:
        return QmlRemoteAccess::StartFailed;
    case ::HyRemote::Runtime::ErrorCode::RuntimeFailure:
        return QmlRemoteAccess::RuntimeFailure;
    case ::HyRemote::Runtime::ErrorCode::Cancelled:
        return QmlRemoteAccess::Cancelled;
    }
    return QmlRemoteAccess::RuntimeFailure;
}

}  // namespace

QmlRemoteAccess::QmlRemoteAccess(QObject *parent)
    : QObject(parent)
    , m_access(std::make_unique<::HyRemote::Runtime::AccessInstance>())
{
    // Runtime construction is deliberately inert. QML may request enabled=true during object
    // creation, but the wrapper defers the actual start until componentComplete() so initial target
    // and policy bindings can settle first.
    subscribeToRuntimeNotifications();
    syncRuntimeSnapshot();
}

QmlRemoteAccess::~QmlRemoteAccess()
{
    // Unsubscribe first, quiesce the shared runtime second: once AccessInstance::stop() has returned,
    // the transport and capture wrappers can no longer publish, so no notification can arrive while
    // this object is being destroyed - not even from a worker thread.
    unsubscribeFromRuntimeNotifications();
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

    if (m_targetDestroyedConnection)
        QObject::disconnect(m_targetDestroyedConnection);
    m_targetDestroyedConnection = {};
    if (targetObject) {
        m_targetDestroyedConnection =
            connect(targetObject, &QObject::destroyed, this, [this](QObject *) {
                m_targetDestroyedConnection = {};
                emit targetChanged();
            });
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
        // The runtime has already published the failure (its error, then Stopped). This read is the
        // one-shot local synchronization, not a poll: it exists so the wrapper is correct even if a
        // notification arrived before it had subscribed.
        syncRuntimeSnapshot();
        return false;
    }

    syncRuntimeSnapshot();
    return true;
}

void QmlRemoteAccess::setEnabled(bool enabledValue)
{
    if (!m_access || enabledValue == m_enabled)
        return;

    if (!m_componentComplete) {
        m_enabled = enabledValue;
        emit enabledChanged();
        return;
    }

    if (enabledValue) {
        if (!startRuntime()) {
            emit enabledChanged();
            return;
        }
        m_enabled = true;
        emit enabledChanged();
        return;
    }

    m_access->stop();
    m_enabled = false;
    emit enabledChanged();
    syncRuntimeSnapshot();
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
    // Assigning an address clears any interface selection in the shared runtime, so the property that reported the
    // interface has to report that it is gone - otherwise a QML binding would keep showing a binding that no longer
    // applies.
    if (m_access->listenInterface().isEmpty())
        emit listenInterfaceChanged();
}

QString QmlRemoteAccess::listenInterface() const
{
    return m_access ? m_access->listenInterface() : QString{};
}

void QmlRemoteAccess::setListenInterface(const QString &identity)
{
    if (!m_access)
        return;

    const QString trimmed = identity.trimmed();
    if (trimmed.isEmpty()) {
        setLocalError(InvalidConfiguration,
                      QStringLiteral("listenInterface must name a network interface; assign listenAddress to use an "
                                     "address instead"));
        return;
    }
    if (trimmed == m_access->listenInterface())
        return;
    if (!m_access->setListenInterface(trimmed)) {
        setLocalError(InvalidConfiguration,
                      QStringLiteral("listenInterface can only be changed while remote access is stopped"));
        return;
    }
    clearLocalError();
    emit listenInterfaceChanged();
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

QmlRemoteAccess::SecurityProfile QmlRemoteAccess::securityProfile() const noexcept
{
    return m_access ? mapSecurityProfile(m_access->securityProfile()) : Insecure;
}

void QmlRemoteAccess::setSecurityProfile(SecurityProfile profile)
{
    if (!m_access || profile == securityProfile())
        return;
    if (!m_access->setSecurityProfile(mapSecurityProfile(profile))) {
        setLocalError(InvalidConfiguration,
                      QStringLiteral("securityProfile can only be changed while remote access is stopped"));
        return;
    }
    clearLocalError();
    emit securityProfileChanged();
}

QString QmlRemoteAccess::securityConfigFile() const
{
    return m_access ? m_access->securityConfigFile() : QString{};
}

void QmlRemoteAccess::setSecurityConfigFile(const QString &path)
{
    if (!m_access || path == securityConfigFile())
        return;
    if (!m_access->setSecurityConfigFile(path)) {
        setLocalError(InvalidConfiguration,
                      QStringLiteral("securityConfigFile can only be changed while remote access is stopped"));
        return;
    }
    clearLocalError();
    emit securityConfigFileChanged();
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
    m_localError = FrontendError{};
    applyRuntimeError();
}

void QmlRemoteAccess::subscribeToRuntimeNotifications()
{
    if (!m_access || m_notificationToken.isValid())
        return;

    // The runtime calls a handler on whatever thread produced the fact. These handlers only marshal,
    // and they hold a QPointer so a notification that is already in flight cannot touch a destroyed
    // wrapper even before the queued invocation is cancelled.
    const QPointer<QmlRemoteAccess> guard(this);
    const auto marshal = [guard](void (QmlRemoteAccess::*apply)()) {
        if (!guard)
            return;
        guard->dispatchRuntimeNotification(apply);
    };

    ::HyRemote::Runtime::RuntimeNotificationHandlers handlers;
    handlers.stateChanged = [marshal](::HyRemote::Runtime::AccessState) {
        marshal(&QmlRemoteAccess::applyRuntimeState);
    };
    handlers.connectedClientCountChanged = [marshal](std::size_t) {
        marshal(&QmlRemoteAccess::applyRuntimeClientCount);
    };
    handlers.errorChanged = [marshal](std::optional<::HyRemote::Runtime::Error>) {
        marshal(&QmlRemoteAccess::applyRuntimeError);
    };

    m_notificationToken = m_access->subscribeNotifications(std::move(handlers));
}

void QmlRemoteAccess::unsubscribeFromRuntimeNotifications()
{
    if (!m_access || !m_notificationToken.isValid())
        return;

    m_access->unsubscribeNotifications(m_notificationToken);
    m_notificationToken = ::HyRemote::Runtime::RuntimeNotificationToken{};
}

void QmlRemoteAccess::dispatchRuntimeNotification(void (QmlRemoteAccess::*apply)())
{
    if (QThread::currentThread() == thread()) {
        // Already on the owner thread: apply directly, so a notification caused by this thread's own
        // lifecycle call is observable before that call returns.
        (this->*apply)();
        return;
    }

    // A transport or capture worker thread: marshal onto this object's thread. Qt cancels a queued
    // invocation whose context object is destroyed, and the QPointer check covers the remainder.
    const QPointer<QmlRemoteAccess> guard(this);
    QMetaObject::invokeMethod(
        this,
        [guard, apply] {
            if (guard)
                (guard.data()->*apply)();
        },
        Qt::QueuedConnection);
}

void QmlRemoteAccess::applyRuntimeState()
{
    if (!m_access)
        return;

    const State nextState = mapState(m_access->state());
    if (nextState == m_state)
        return;

    m_state = nextState;
    emit stateChanged();
}

void QmlRemoteAccess::applyRuntimeClientCount()
{
    if (!m_access)
        return;

    const quint64 nextConnectedClientCount = static_cast<quint64>(m_access->connectedClientCount());
    if (nextConnectedClientCount == m_connectedClientCount)
        return;

    m_connectedClientCount = nextConnectedClientCount;
    emit connectedClientCountChanged();
}

void QmlRemoteAccess::applyRuntimeError()
{
    if (!m_access)
        return;

    // Effective error: this frontend's own validation error when it has one, otherwise the runtime's
    // effective error. A runtime "no error" notification therefore cannot silently erase a local
    // validation error, and the rule is deterministic in both directions: clearing the local error
    // falls back to whatever the runtime currently reports, and clearError() clears both.
    const std::optional<::HyRemote::Runtime::Error> runtimeError = m_access->lastError();
    const bool localIsSet = m_localError.code != NoError || !m_localError.message.isEmpty();

    const ErrorCode nextCode = localIsSet ? m_localError.code
                                          : (runtimeError ? mapErrorCode(runtimeError->code) : NoError);
    const QString nextMessage = localIsSet ? m_localError.message
                                           : (runtimeError ? runtimeError->message : QString{});
    const bool nextRecoverable = localIsSet ? m_localError.recoverable
                                            : (runtimeError ? runtimeError->recoverable : false);

    if (nextCode == m_errorCode && nextMessage == m_errorString && nextRecoverable == m_recoverableError)
        return;

    m_errorCode = nextCode;
    m_errorString = nextMessage;
    m_recoverableError = nextRecoverable;
    emit errorChanged();
}

void QmlRemoteAccess::syncRuntimeSnapshot()
{
    // Explicit, one-shot local synchronization - never a poll. It runs only when this wrapper is
    // constructed, when a lifecycle call it made has just returned, and when a local error changes.
    // Every product state that changes while the runtime runs arrives as a typed notification.
    applyRuntimeState();
    applyRuntimeClientCount();
    applyRuntimeError();
}

void QmlRemoteAccess::setLocalError(ErrorCode code, QString message, bool recoverable)
{
    m_localError.code = code;
    m_localError.message = std::move(message);
    m_localError.recoverable = recoverable;
    applyRuntimeError();
}

void QmlRemoteAccess::clearLocalError()
{
    if (m_localError.code == NoError && m_localError.message.isEmpty() && !m_localError.recoverable)
        return;

    m_localError = FrontendError{};
    applyRuntimeError();
}

}  // namespace HyRemote::Qml
