#pragma once

#include <QObject>
#include <QQmlParserStatus>
#include <QTimer>
#include <QtQmlIntegration/qqmlintegration.h>

#include <memory>

namespace HyRemote {
class RemoteAccess;
}

namespace HyRemote::Qml {

// Thin declarative wrapper over the same HyRemote::RemoteAccess product runtime used by C++ apps.
// It owns no capture, transport, input or Session implementation of its own.
//
// QQmlParserStatus lets `enabled: true` remain a simple declarative request without racing QML's
// initial target/property construction order. The shared runtime is started only after componentComplete().
//
// Do not mark this QObject final: Qt's generated QML registration layer derives an internal
// QQmlElement<T> wrapper from creatable QML element types.
class QmlRemoteAccess : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RemoteAccess)
    Q_INTERFACES(QQmlParserStatus)

    Q_PROPERTY(QObject *target READ target WRITE setTarget NOTIFY targetChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QString listenAddress READ listenAddress WRITE setListenAddress NOTIFY listenAddressChanged)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged)
    Q_PROPERTY(bool remoteInputEnabled READ remoteInputEnabled WRITE setRemoteInputEnabled NOTIFY remoteInputEnabledChanged)
    Q_PROPERTY(SecurityProfile securityProfile READ securityProfile WRITE setSecurityProfile NOTIFY securityProfileChanged)
    // This is a descriptor path, not secret material. Password/private-key contents are deliberately
    // absent from the QML surface and can never be read back through a binding.
    Q_PROPERTY(QString securityConfigFile READ securityConfigFile WRITE setSecurityConfigFile NOTIFY securityConfigFileChanged)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(quint64 connectedClientCount READ connectedClientCount NOTIFY connectedClientCountChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)
    Q_PROPERTY(ErrorCode errorCode READ errorCode NOTIFY errorChanged)
    Q_PROPERTY(bool recoverableError READ recoverableError NOTIFY errorChanged)

public:
    enum State {
        Stopped,
        Starting,
        Running,
        Stopping,
        Faulted,
    };
    Q_ENUM(State)

    enum SecurityProfile {
        Insecure,
        Authenticated,
        AuthenticatedEncrypted,
    };
    Q_ENUM(SecurityProfile)

    enum ErrorCode {
        NoError,
        InvalidConfiguration,
        TargetAdapterUnavailable,
        TransportUnavailable,
        RemoteInputUnavailable,
        SecurityUnavailable,
        StartFailed,
        RuntimeFailure,
        Cancelled,
    };
    Q_ENUM(ErrorCode)

    explicit QmlRemoteAccess(QObject *parent = nullptr);
    ~QmlRemoteAccess() override;

    void classBegin() override;
    void componentComplete() override;

    QObject *target() const noexcept;
    void setTarget(QObject *target);

    bool enabled() const noexcept;
    void setEnabled(bool enabled);

    QString listenAddress() const;
    void setListenAddress(const QString &address);

    int port() const noexcept;
    void setPort(int port);

    bool remoteInputEnabled() const noexcept;
    void setRemoteInputEnabled(bool enabled);

    SecurityProfile securityProfile() const noexcept;
    void setSecurityProfile(SecurityProfile profile);

    QString securityConfigFile() const;
    void setSecurityConfigFile(const QString &path);

    State state() const noexcept;
    quint64 connectedClientCount() const noexcept;
    QString errorString() const;
    ErrorCode errorCode() const noexcept;
    bool recoverableError() const noexcept;

    Q_INVOKABLE void clearError();

signals:
    void targetChanged();
    void enabledChanged();
    void listenAddressChanged();
    void portChanged();
    void remoteInputEnabledChanged();
    void securityProfileChanged();
    void securityConfigFileChanged();
    void stateChanged();
    void connectedClientCountChanged();
    void errorChanged();

private:
    bool startRuntime();
    void refreshRuntimeSnapshot();
    void setLocalError(ErrorCode code, QString message, bool recoverable = false);
    void clearLocalError();

    std::unique_ptr<::HyRemote::RemoteAccess> m_access;
    QTimer m_pollTimer;
    QMetaObject::Connection m_targetDestroyedConnection;
    bool m_componentComplete = false;
    bool m_enabled = false;
    State m_state = Stopped;
    quint64 m_connectedClientCount = 0;
    ErrorCode m_errorCode = NoError;
    QString m_errorString;
    bool m_recoverableError = false;
};

}  // namespace HyRemote::Qml
