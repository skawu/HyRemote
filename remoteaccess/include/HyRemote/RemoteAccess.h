#pragma once

#include <QHostAddress>
#include <QString>

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

    // Explicit lifecycle. start() owns creation of the target adapter, input path and default
    // transport behind the facade. On failure, RemoteAccess returns to Stopped and preserves a
    // product-level error for lastError().
    bool start();
    void stop() noexcept;

    RemoteAccessState state() const;
    std::optional<RemoteAccessError> lastError() const;
    void clearError();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace HyRemote
