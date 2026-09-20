#pragma once

#include <QByteArray>
#include <QHostAddress>

#include <memory>

#include <HyRemote/RemoteAccessExport.h>

#include "hyremote/core/transport.hpp"

namespace HyRemote::detail {

// What the RFB handshake must enforce. `password` is the credential read from the security descriptor: it is never
// logged, never copied into a diagnostic, and read only by the challenge/response verification. A build without the
// transport-security capability never reaches this path, because RemoteAccess refuses a secure profile there before
// any listener is opened.
struct RfbSecurityConfig
{
    bool vncAuthenticationRequired = false;
    QByteArray password;
};

// Creates the product's bounded, dependency-light RFB correctness transport. The concrete type is
// deliberately private to RemoteAccess: applications and hyremote-core only see hyremote::Transport.
// The private source-tree declaration is exported only so repository tests can link against the
// shared RemoteAccess DLL on Windows; it is not installed as part of the public SDK surface.
HYREMOTE_REMOTEACCESS_EXPORT std::unique_ptr<hyremote::Transport> createRfbTransport(
    const QHostAddress &listenAddress,
    quint16 port,
    const RfbSecurityConfig &security);

}  // namespace HyRemote::detail
