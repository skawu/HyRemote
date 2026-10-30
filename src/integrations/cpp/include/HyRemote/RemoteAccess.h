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
    // #174: the configured interface has no usable IPv4 address right now, so there is no listener - but the
    // configuration is not yours to change until you stop, and the listener comes back on its own when the
    // interface does. It is not Stopped and it is not Faulted.
    Unavailable,
};

// Stable product-level security intent. Protocol/security-type selection stays private to the
// runtime. The enum states what the application asks for; it is not a capability the build is
// guaranteed to have. Whether a stronger profile is available depends on the actual build and
// runtime capability, a profile that cannot be satisfied fails closed with SecurityUnavailable
// instead of degrading silently, and no stronger profile is promised as a future requirement.
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

// #589: where the server-side text watermark is drawn on the *remote* picture. Every position is a single line
// except `Tiled`, which repeats it across the frame and is only used when explicitly selected.
enum class RemoteWatermarkPosition {
    BottomRight,
    BottomLeft,
    TopRight,
    TopLeft,
    Tiled,
};

// #589: the operator's watermark request. A zero-length `text` means "watermark disabled", which is also the
// default state of every instance; whitespace-only or otherwise unrenderable text is rejected, because treating it
// as "disabled" would silently drop a watermark the caller asked for. Values outside the product bounds (opacity,
// font size, colour form) are rejected, never quietly adjusted; omitted values take the shared Runtime defaults
// (bottom-right, 0.35 opacity, 16 points, white).
//
// The watermark is applied to the outgoing remote framebuffer only. The host application's own painted pixels are
// never modified, and no user name, IP address, time or device identifier is ever inserted automatically.
struct RemoteWatermark
{
    QString text;
    RemoteWatermarkPosition position = RemoteWatermarkPosition::BottomRight;
    // These literals must stay identical to the shared Runtime policy defaults. This installed header cannot include
    // the internal policy header, so the equality is guarded by `hyremote-watermark-policy-test` instead of by a
    // shared constant: a stale value here would silently give this one route a different watermark than QML, Generic
    // and QPA.
    double opacity = 0.35;
    int fontPointSize = 16;
    QString colorRgb = QStringLiteral("#FFFFFF");
};

// Product-level Embedded C++ API.
//
// Normal applications attach one RemoteAccess instance to a top-level Qt target and explicitly
// start/stop remote access. Session/CaptureSource/Transport/InputSink and concrete RFB/TLS backends
// are implementation details and intentionally absent from this header.
//
// Safe defaults:
//   - construction never opens a listener;
//   - listen address defaults to AnyIPv4 (0.0.0.0), the host's IPv4 interfaces;
//   - remote input defaults to disabled;
//   - security defaults to Insecure: unauthenticated and unencrypted, for a trusted LAN only
//     and not Internet-safe;
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

    // The default is 0.0.0.0 (AnyIPv4): the host's IPv4 interfaces, not just loopback (#174).
    QHostAddress listenAddress() const;
    bool setListenAddress(const QHostAddress &address);

    // #174: the listener can be named by network interface instead of by address. The identity is
    // QNetworkInterface::name(), and it is configuration rather than an observation: an interface that changes its
    // IPv4 address keeps reporting the same identity here. A non-empty interface selects interface mode and
    // outranks the address; setting an address clears it again, so the two never describe different modes.
    QString listenInterface() const;
    bool setListenInterface(const QString &identity);

    quint16 port() const noexcept;
    bool setPort(quint16 port);

    bool remoteInputEnabled() const noexcept;
    bool setRemoteInputEnabled(bool enabled);

    RemoteSecurityProfile securityProfile() const noexcept;
    bool setSecurityProfile(RemoteSecurityProfile profile);

    // Path to the non-secret security descriptor/configuration used by the profiles that require it. The descriptor refers to secret material by
    // file path; raw passwords/private keys are never accepted as command-line/QML diagnostics and
    // are never readable back through this API. Relative material paths are resolved by the runtime
    // against the descriptor directory. Configuration changes are accepted only while Stopped.
    QString securityConfigFile() const;
    bool setSecurityConfigFile(const QString &path);

    // #589: server-side static text watermark. Returns the requested policy as the Runtime resolved it (defaults
    // filled in, operator text included). `setWatermark` returns false when the request is not renderable; the
    // reason is part of the diagnostic report (`WATERMARK_POLICY_ERROR`) and start() fails closed with
    // InvalidConfiguration instead of streaming an unwatermarked picture. Configuration changes are accepted only
    // while Stopped. `clearWatermark()` is the explicit "no watermark" state and the default.
    RemoteWatermark watermark() const;
    bool setWatermark(const RemoteWatermark &watermark);
    void clearWatermark();

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

    // #335: bounded support report from the one Shared Runtime diagnostic snapshot/formatter.
    // The report contains product-support facts only and never exposes backend objects, credential
    // material or the contents of a security descriptor. #591: it does name the *selected non-secret
    // locations* (configuration file, state directory, secret reference) together with their provenance,
    // so a support report answers the same question on all four routes.
    QString diagnosticReport() const;

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
