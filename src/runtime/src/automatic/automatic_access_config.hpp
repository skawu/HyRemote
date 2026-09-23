#pragma once

#include "access_types.hpp"

#include <QHostAddress>
#include <QString>

namespace HyRemote::Runtime::Automatic {

// Frontend-neutral configuration for application-level zero-code access. Generic Plugin and QPA
// parse their own launch syntax into this common runtime shape; no frontend-specific vocabulary is
// retained after bootstrap.
struct AccessConfig
{
    // #174: exactly one of address/interface selects the binding. An empty interface means the address decides,
    // which is why the wildcard default still means "all IPv4 interfaces" with no interface configured.
    QString listenInterface;
    QHostAddress listenAddress = QHostAddress::AnyIPv4;
    quint16 port = static_cast<quint16>(HYREMOTE_DEFAULT_PORT);
    bool remoteInputEnabled = false;
    Runtime::SecurityProfile securityProfile = Runtime::SecurityProfile::Insecure;
    QString securityConfigFile;
};

}  // namespace HyRemote::Runtime::Automatic
