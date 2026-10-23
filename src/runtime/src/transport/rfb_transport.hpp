#pragma once

#include <QByteArray>
#include <QHostAddress>

#include <functional>
#include <memory>

#include <HyRemote/RemoteAccessExport.h>

#include "hyremote/core/transport.hpp"

namespace HyRemote::detail {

// What the RFB handshake must enforce. `password` is the credential read from the security descriptor: it is never
// logged, never copied into a diagnostic, and read only by the challenge/response verification. A build without the
// transport-security capability never reaches this path, because RemoteAccess refuses a secure profile there before
// any listener is opened.
// Transport-private cursor snapshot. Keeping this QtGui-free lets the
// QtCore-only wire tests include this header; the Runtime converts a QCursor
// into RGBA8888 before handing it to the worker thread.
struct RfbCursorSnapshot
{
    int width = 0;
    int height = 0;
    int hotspotX = 0;
    int hotspotY = 0;
    QByteArray rgba8888;
};

// x/y are the last remote pointer position in target framebuffer coordinates,
// or -1 when the viewer has not sent a pointer event yet. sourceWidth/sourceHeight
// are the remembered framebuffer dimensions, so callers can map physical RFB
// pixels to logical target coordinates. This lets the target resolve the cursor
// shape under the remote pointer instead of the local one.
using RfbCursorProvider =
    std::function<RfbCursorSnapshot(int x, int y, std::uint32_t sourceWidth, std::uint32_t sourceHeight)>;

struct RfbSecurityConfig
{
    bool vncAuthenticationRequired = false;
    bool remoteInputEnabled = false;
    QByteArray password;
    // Optional product hook for the current target cursor. Tests and custom
    // compositions may leave it empty and receive the portable arrow fallback.
    RfbCursorProvider cursorProvider;
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
