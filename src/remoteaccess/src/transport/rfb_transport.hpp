#pragma once

#include <QHostAddress>

#include <memory>

#include <HyRemote/RemoteAccessExport.h>

#include "hyremote/core/transport.hpp"

namespace HyRemote::detail {

// Creates the product's bounded, dependency-light RFB correctness transport. The concrete type is
// deliberately private to RemoteAccess: applications and hyremote-core only see hyremote::Transport.
// The private source-tree declaration is exported only so repository tests can link against the
// shared RemoteAccess DLL on Windows; it is not installed as part of the public SDK surface.
HYREMOTE_REMOTEACCESS_EXPORT std::unique_ptr<hyremote::Transport> createRfbTransport(
    const QHostAddress &listenAddress,
    quint16 port);

}  // namespace HyRemote::detail
