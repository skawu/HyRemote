#pragma once

#include <QObject>
#include <QQmlParserStatus>
#include <QtQmlIntegration/qqmlintegration.h>

#include "detail/runtime_notifications.hpp"
#include "detail/watermark_policy.hpp"

#include <memory>
#include <optional>

namespace HyRemote::Runtime {
class AccessInstance;
}

namespace HyRemote::Qml {

// Thin declarative frontend over the shared HyRemote runtime. It owns no capture, transport, input
// or Session implementation of its own and does not depend on the Embedded C++ frontend.
//
// QQmlParserStatus lets `enabled: true` remain a simple declarative request without racing QML's
// initial target/property construction order. The shared runtime is started only after componentComplete().
//
// The three product-visible properties (state, connectedClientCount, errorString/errorCode/
// recoverableError) are updated from the shared runtime's typed notifications, not from a timer: the
// runtime publishes when its own event boundaries produce a change, and this wrapper reads the snapshot
// those notifications point at. A notification may arrive on a transport worker thread, so delivery is
// marshalled onto this object's thread before any member here is touched.
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
    // #174: the interface identity (QNetworkInterface::name()). Non-empty selects interface mode; assigning
    // listenAddress clears it. The frontend maps the property and never resolves an adapter itself.
    Q_PROPERTY(QString listenInterface READ listenInterface WRITE setListenInterface NOTIFY listenInterfaceChanged)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged)
    Q_PROPERTY(bool remoteInputEnabled READ remoteInputEnabled WRITE setRemoteInputEnabled NOTIFY remoteInputEnabledChanged)
    Q_PROPERTY(SecurityProfile securityProfile READ securityProfile WRITE setSecurityProfile NOTIFY securityProfileChanged)
    Q_PROPERTY(QString securityConfigFile READ securityConfigFile WRITE setSecurityConfigFile NOTIFY securityConfigFileChanged)
    // #589: server-side static text watermark. Empty watermarkText means disabled; the rest default to the shared
    // Runtime policy (bottom-right, 0.35 opacity, 16 points, white). A value the policy refuses is reported through
    // errorCode/errorString and refuses the start instead of streaming an unmarked picture.
    Q_PROPERTY(QString watermarkText READ watermarkText WRITE setWatermarkText NOTIFY watermarkChanged)
    Q_PROPERTY(QString watermarkPosition READ watermarkPosition WRITE setWatermarkPosition NOTIFY watermarkChanged)
    Q_PROPERTY(double watermarkOpacity READ watermarkOpacity WRITE setWatermarkOpacity NOTIFY watermarkChanged)
    Q_PROPERTY(int watermarkFontPointSize READ watermarkFontPointSize WRITE setWatermarkFontPointSize NOTIFY watermarkChanged)
    Q_PROPERTY(QString watermarkColor READ watermarkColor WRITE setWatermarkColor NOTIFY watermarkChanged)
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
        // #174: no listener right now because the configured interface has no usable IPv4 address; the runtime
        // keeps following that same interface and returns to Running by itself.
        Unavailable,
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

    QString listenInterface() const;
    void setListenInterface(const QString &identity);

    int port() const noexcept;
    void setPort(int port);

    bool remoteInputEnabled() const noexcept;
    void setRemoteInputEnabled(bool enabled);

    SecurityProfile securityProfile() const noexcept;
    void setSecurityProfile(SecurityProfile profile);

    QString securityConfigFile() const;
    void setSecurityConfigFile(const QString &path);

    QString watermarkText() const;
    void setWatermarkText(const QString &text);
    // Stable token: bottom-right (default), bottom-left, top-right, top-left or tiled.
    QString watermarkPosition() const;
    void setWatermarkPosition(const QString &position);
    double watermarkOpacity() const;
    void setWatermarkOpacity(double opacity);
    int watermarkFontPointSize() const;
    void setWatermarkFontPointSize(int points);
    QString watermarkColor() const;
    void setWatermarkColor(const QString &colorRgb);

    State state() const noexcept;
    quint64 connectedClientCount() const noexcept;
    QString errorString() const;
    ErrorCode errorCode() const noexcept;
    bool recoverableError() const noexcept;

    // #335: same bounded Shared Runtime support report as the C++ facade.
    Q_INVOKABLE QString diagnosticReport() const;
    Q_INVOKABLE void clearError();

signals:
    void targetChanged();
    void enabledChanged();
    void listenAddressChanged();
    void listenInterfaceChanged();
    void portChanged();
    void remoteInputEnabledChanged();
    void securityProfileChanged();
    void securityConfigFileChanged();
    // One notification for the five watermark properties: the policy is resolved as a whole, so a change to any
    // field can change the effective policy, its error state and the report.
    void watermarkChanged();
    void stateChanged();
    void connectedClientCountChanged();
    void errorChanged();

private:
    void applyWatermark(const HyRemote::detail::WatermarkPolicyRequest &request);

    // A frontend-local validation error (a rejected listenAddress, a property changed while running).
    // It is kept apart from the runtime's effective error so a runtime "no error" notification cannot
    // silently erase what this frontend itself refused.
    struct FrontendError
    {
        ErrorCode code = NoError;
        QString message;
        bool recoverable = false;
    };

    bool startRuntime();
    void subscribeToRuntimeNotifications();
    void unsubscribeFromRuntimeNotifications();
    void dispatchRuntimeNotification(void (QmlRemoteAccess::*apply)());
    void applyRuntimeState();
    void applyRuntimeClientCount();
    void applyRuntimeError();
    void syncRuntimeSnapshot();
    void setLocalError(ErrorCode code, QString message, bool recoverable = false);
    void clearLocalError();

    std::unique_ptr<::HyRemote::Runtime::AccessInstance> m_access;
    ::HyRemote::Runtime::RuntimeNotificationToken m_notificationToken;
    QMetaObject::Connection m_targetDestroyedConnection;
    bool m_componentComplete = false;
    bool m_enabled = false;
    State m_state = Stopped;
    quint64 m_connectedClientCount = 0;
    ErrorCode m_errorCode = NoError;
    QString m_errorString;
    bool m_recoverableError = false;
    FrontendError m_localError;
};

}  // namespace HyRemote::Qml
