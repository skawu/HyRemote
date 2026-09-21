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
    QHostAddress listenAddress = QHostAddress::LocalHost;
    quint16 port = static_cast<quint16>(HYREMOTE_DEFAULT_PORT);
    bool remoteInputEnabled = false;
    Runtime::SecurityProfile securityProfile = Runtime::SecurityProfile::Insecure;
    QString securityConfigFile;
};

}  // namespace HyRemote::Runtime::Automatic
