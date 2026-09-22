#pragma once

#include <QByteArray>
#include <QHostAddress>
#include <QSslCertificate>
#include <QSslKey>
#include <QString>

#include <algorithm>
#include <memory>

#include <HyRemote/RemoteAccessExport.h>

#include "hyremote/core/transport.hpp"

namespace HyRemote::detail {

// The closed set of wire profiles the RFB transport can be configured for. It is one enum rather than a
// group of booleans on purpose: `tls = true, authentication = false` is not a profile this product offers,
// and a closed type is the only way to make that combination unrepresentable.
//
//   None                 RFB 3.8 -> SecurityType None (1)
//   VncAuth              RFB 3.8 -> VNC Authentication (2), plaintext on the wire
//   VeNCryptTlsVncAuth   RFB 3.8 -> VeNCrypt (19) -> VeNCrypt 0.2 -> X509Vnc (261) -> TLS >= 1.2
//                        -> VNC Authentication inside the encrypted channel
enum class RfbSecurityProfile {
    None,
    VncAuth,
    VeNCryptTlsVncAuth,
};

// What the RFB handshake must enforce. `password` is the credential read from the security descriptor: it is never
// logged, never copied into a diagnostic, and read only by the challenge/response verification. The certificate and
// key are material a pre-listen preparation step already loaded and validated; the transport never re-reads a file
// and never accepts an unvalidated pair. A build without the transport-security capability never reaches any of this,
// because RemoteAccess refuses a secure profile there before any listener is opened.
//
// V0.2 freezes exactly one TLS policy for `VeNCryptTlsVncAuth`: TLS >= 1.2. It is deliberately not a field, because a
// configurable minimum is how a weaker policy would arrive.
struct RfbSecurityConfig
{
    RfbSecurityProfile profile = RfbSecurityProfile::None;
    QByteArray password;
    QString certificateFile;
    QString privateKeyFile;
    QSslCertificate certificate;
    QSslKey privateKey;

    RfbSecurityConfig() = default;
    RfbSecurityConfig(const RfbSecurityConfig &) = default;
    RfbSecurityConfig &operator=(const RfbSecurityConfig &) = default;
    RfbSecurityConfig(RfbSecurityConfig &&) noexcept = default;
    RfbSecurityConfig &operator=(RfbSecurityConfig &&) noexcept = default;

    // The password buffer is bounded and overwritten before release. Copies are allowed because the worker owns
    // one for the run; every instance wipes its own buffer when it is destroyed. The destructor is inline on
    // purpose: an out-of-line one would add an unreleased symbol to the shared runtime's private ABI surface for
    // every consumer that merely destroys a configuration object.
    ~RfbSecurityConfig()
    {
        if (!password.isEmpty())
            std::fill(password.begin(), password.end(), '\0');
        password.clear();
    }
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
