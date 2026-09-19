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

// Stable product-level security intent. Protocol/security-type selection stays private to the
// runtime. The final V1 GA secure profile is AuthenticatedEncrypted; Insecure remains an explicit
// loopback/trusted-test compatibility mode and is never silently selected as a downgrade.
enum class RemoteSecurityProfile {
    Insecure,
    Authenticated,
    AuthenticatedEncrypted,
};

enum class RemoteAccessErrorCode {
    InvalidConfiguration,
    TargetAdapterUnavailable,
    TransportUnavailable,
    RemoteInputUnavailable,
    // A non-insecure profile was configured but its descriptor/material or implementation cannot
    // be honoured. Reported instead of silently opening a weaker listener.
    SecurityUnavailable,
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
// start/stop remote access. Session/CaptureSource/Transport/InputSink and concrete RFB/TLS backends
// are implementation details and intentionally absent from this header.
//
// Safe defaults:
//   - construction never opens a listener;
//   - listen address defaults to loopback;
//   - remote input defaults to disabled;
//   - security defaults to explicit Insecure compatibility mode;
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

    RemoteSecurityProfile securityProfile() const noexcept;
    bool setSecurityProfile(RemoteSecurityProfile profile);

    // Path to the non-secret V1 security descriptor. The descriptor refers to secret material by
    // file path; raw passwords/private keys are never accepted as command-line/QML diagnostics and
    // are never readable back through this API. Relative material paths are resolved by the runtime
    // against the descriptor directory. Configuration changes are accepted only while Stopped.
    QString securityConfigFile() const;
    bool setSecurityConfigFile(const QString &path);

    // Explicit lifecycle. start() owns creation of the target adapter, input path and default
    // transport behind the facade. On startup failure, RemoteAccess returns to Stopped and preserves
    // a product-level error for lastError(). A non-recoverable runtime failure is observable as
    // Faulted until the owner calls stop(); configuration becomes mutable again only after Stopped.
    bool start();
    void stop() noexcept;

    RemoteAccessState state() const;

    // Backend-neutral product diagnostic. Running with zero connected clients means the listener is
    // available but no viewer is currently attached. This is aggregate diagnostics only: it is not
    // identity, authentication or authorization. Concrete transport/client objects never cross this
    // API boundary.
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
