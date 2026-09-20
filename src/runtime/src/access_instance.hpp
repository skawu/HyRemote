#pragma once

#include "access_types.hpp"

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

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace HyRemote::Runtime
