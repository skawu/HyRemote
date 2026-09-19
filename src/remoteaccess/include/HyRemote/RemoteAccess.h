#pragma once

#include <QHostAddress>
#include <QString>

#include <cstddef>
#include <memory>
#include <optional>

#include <HyRemote/RemoteAccessExport.h>

class QObject;

namespace HyRemote {

enum class RemoteAccessState {
    Stopped,
    Starting,
    Running,
    Stopping,
    Faulted,
};

enum class RemoteAccessErrorCode {
    InvalidConfiguration,
    TargetAdapterUnavailable,
    TransportUnavailable,
    RemoteInputUnavailable,
    // Authentication was configured but cannot be honoured: either no password was set, or the
    // authenticated transport step is not available in this build. Reported instead of silently
    // opening an unauthenticated listener.
    AuthenticationUnavailable,
    StartFailed,
    RuntimeFailure,
    Cancelled,
};

struct RemoteAccessError
{
    RemoteAccessErrorCode code = RemoteAccessErrorCode::RuntimeFailure;
    QString message;
    bool recoverable = false;
};

// Product-level Embedded C++ API.
//
// Normal applications attach one RemoteAccess instance to a top-level Qt target and explicitly
// start/stop remote access. Session/CaptureSource/Transport/InputSink and concrete VNC backends are
// implementation details and intentionally absent from this header.
//
// Safe defaults:
//   - construction never opens a listener;
//   - listen address defaults to loopback;
//   - remote input defaults to disabled;
//   - configuration is mutable only while Stopped.
class HYREMOTE_REMOTEACCESS_EXPORT RemoteAccess
{
public:
    explicit RemoteAccess(QObject *target = nullptr);
    ~RemoteAccess();

    RemoteAccess(const RemoteAccess &) = delete;
    RemoteAccess &operator=(const RemoteAccess &) = delete;
    RemoteAccess(RemoteAccess &&) noexcept;
    RemoteAccess &operator=(RemoteAccess &&) noexcept;

    QObject *target() const noexcept;
    bool setTarget(QObject *target);

    QHostAddress listenAddress() const;
    bool setListenAddress(const QHostAddress &address);

    quint16 port() const noexcept;
    bool setPort(quint16 port);

    bool remoteInputEnabled() const noexcept;
    bool setRemoteInputEnabled(bool enabled);

    // Authentication configuration. This is configuration only: the authenticated transport step
    // (RFB security type 2) is frozen in docs/security-model.md 10.1 but not implemented yet, so
    // start() currently refuses when authentication is enabled rather than silently serving an
    // unauthenticated SecurityType None listener. The password is never included in errors,
    // diagnostics or logs, and enabling without a password is never accepted at start().
    bool authenticationEnabled() const noexcept;
    bool setAuthenticationEnabled(bool enabled);

    // Stores the password used by the authentication step. Pass an empty string to clear it.
    // Rejected unless the runtime is Stopped, and the value is never echoed back in any error.
    bool setPassword(const QString &password);

    // Explicit lifecycle. start() owns creation of the target adapter, input path and default
    // transport behind the facade. On startup failure, RemoteAccess returns to Stopped and preserves
    // a product-level error for lastError(). A non-recoverable runtime failure is observable as
    // Faulted until the owner calls stop(); configuration becomes mutable again only after Stopped.
    bool start();
    void stop() noexcept;

    RemoteAccessState state() const;

    // Backend-neutral product diagnostic. Running with zero connected clients means the listener is
    // available but no viewer is currently attached. Concrete transport/client objects never cross
    // this API boundary. A Faulted runtime remains owned until explicit stop(), so this diagnostic
    // may remain nonzero until that cleanup completes.
    std::size_t connectedClientCount() const noexcept;

    std::optional<RemoteAccessError> lastError() const;

    // Acknowledge/clear product-level and live recoverable diagnostics. A new occurrence becomes
    // visible again. An active non-recoverable Session fault remains visible while state()==Faulted;
    // call stop() to quiesce the failed runtime, then clearError() if the retained diagnostic has
    // been handled.
    void clearError();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace HyRemote
