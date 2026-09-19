#pragma once

#include <QHostAddress>
#include <QObject>
#include <QString>

#include <cstddef>
#include <memory>
#include <optional>

#include <HyRemote/RemoteAccessExport.h>

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

// Change notifications for Embedded C++ consumers.
//
// RemoteAccess itself stays a plain, movable value type - docs/v1-api-stability.md freezes that - so its
// notifications live on a separate object reached through RemoteAccess::notifier():
//
//     QObject::connect(remote.notifier(), &HyRemote::RemoteAccessNotifier::clientCountChanged, ...);
//
// Every signal reports something that actually happened, and none of it is produced by polling: stateChanged() is
// emitted for the transitions the facade causes (start/stop) and for the ones it is told about (a runtime fault,
// including a capture target disappearing), clientCountChanged() comes from the transport's own connection events,
// and errorChanged() is emitted whenever the diagnostic behind lastError() is written or cleared - including a
// recurrence of the same error, which is why an observer should compare what it cares about rather than assume
// silence means "nothing new".
//
// Signals are emitted on the thread that caused the change; for an asynchronous failure that is a Core worker or a
// transport callback thread, which is where the facade's own calls already run. The notifier is owned by the facade
// and destroyed with it, after the runtime has been quiesced, so Qt tears down the connections for you.
class HYREMOTE_REMOTEACCESS_EXPORT RemoteAccessNotifier : public QObject
{
    Q_OBJECT

public:
    explicit RemoteAccessNotifier(QObject *parent = nullptr);

Q_SIGNALS:
    // state() changed.
    void stateChanged();
    // connectedClientCount() changed.
    void clientCountChanged();
    // The diagnostic reported by lastError() was written or cleared.
    void errorChanged();
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

    // Change notifications for consumers that would otherwise poll; see RemoteAccessNotifier. Returns nullptr for a
    // moved-from facade. The object lives as long as the facade (or the move target that adopted its runtime).
    RemoteAccessNotifier *notifier() const noexcept;

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
