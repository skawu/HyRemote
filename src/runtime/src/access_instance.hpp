#pragma once

#include "access_types.hpp"
#include "detail/runtime_notifications.hpp"

#include <HyRemote/RemoteAccessExport.h>

#include <QHostAddress>
#include <QString>

#include <cstddef>
#include <memory>
#include <optional>

class QObject;

namespace HyRemote::Runtime {

// Private cross-frontend runtime service. The class is exported from the one shared
// HyRemoteRemoteAccess binary because QML/Generic/QPA payloads are separate modules, but this header
// is never installed and therefore is not an application SDK/ABI promise.
class HYREMOTE_REMOTEACCESS_EXPORT AccessInstance
{
public:
    explicit AccessInstance(QObject *target = nullptr);
    ~AccessInstance();

    AccessInstance(const AccessInstance &) = delete;
    AccessInstance &operator=(const AccessInstance &) = delete;
    AccessInstance(AccessInstance &&) noexcept;
    AccessInstance &operator=(AccessInstance &&) noexcept;

    QObject *target() const noexcept;
    bool setTarget(QObject *target);

    QHostAddress listenAddress() const;
    bool setListenAddress(const QHostAddress &address);

    // #174: the interface identity of the listener, by QNetworkInterface::name(). Non-empty selects interface
    // mode and outranks the address; setting an address clears it again. The identity is configuration, never a
    // resolved address, so an interface that changes IPv4 keeps reporting the same identity here.
    QString listenInterface() const;
    bool setListenInterface(const QString &identity);

    // The single reconciliation entry point for an interface binding (#174). The runtime's own watcher calls it,
    // and repository tests call it directly so that no test ever has to sleep waiting for a timer. It is a no-op
    // unless an interface listener is actually desired, which is what makes a late tick harmless.
    void reconcileInterfaceBinding();

    quint16 port() const noexcept;
    bool setPort(quint16 port);

    bool remoteInputEnabled() const noexcept;
    bool setRemoteInputEnabled(bool enabled);

    SecurityProfile securityProfile() const noexcept;
    bool setSecurityProfile(SecurityProfile profile);

    QString securityConfigFile() const;
    bool setSecurityConfigFile(const QString &path);

    bool start();
    void stop() noexcept;

    AccessState state() const;
    std::size_t connectedClientCount() const noexcept;
    std::optional<Error> lastError() const;
    void clearError();

    // Private Runtime notification seam (#259). A frontend subscribes instead of polling: the handlers
    // are typed (see detail/runtime_notifications.hpp), the returned token unregisters them, and the
    // values are published from the event boundaries that already exist in this class - the start/stop
    // lifecycle, the transport wrapper and the capture wrapper - never from a timer.
    //
    // Frontends consume these notifications and read the snapshot they already read today; there is no
    // second state machine and no second copy of the client count. Like the rest of AccessInstance this
    // is not an application SDK promise: the header is never installed and no Core or public facade
    // type appears here.
    RuntimeNotificationToken subscribeNotifications(RuntimeNotificationHandlers handlers);
    void unsubscribeNotifications(RuntimeNotificationToken token) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace HyRemote::Runtime
