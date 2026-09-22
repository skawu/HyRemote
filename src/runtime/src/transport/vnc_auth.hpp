#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QString>

#include <HyRemote/RemoteAccessExport.h>

namespace HyRemote::detail {

inline constexpr qsizetype kVncAuthChallengeBytes = 16;
inline constexpr qsizetype kVncAuthMaxPasswordBytes = 8;

// The private source-tree declarations are exported only in a test-enabled build, so repository tests can link
// against the shared runtime DLL on Windows and reuse the product's own primitive instead of reimplementing the
// historical DES bit order. They are not installed and not part of any SDK surface.
#if defined(HYREMOTE_ENABLE_PRIVATE_TEST_EXPORTS)
#  define HYREMOTE_VNC_AUTH_EXPORT HYREMOTE_REMOTEACCESS_EXPORT
#else
#  define HYREMOTE_VNC_AUTH_EXPORT
#endif

// Generates one cryptographically random RFB VNC-auth challenge. This is private transport
// machinery; no challenge, password or response is exposed through the product API.
HYREMOTE_VNC_AUTH_EXPORT bool generateVncAuthChallenge(QByteArray &challenge, QString &error);

// Computes the 16-byte RFB VNC-auth response expected for `challenge`. VNC authentication uses
// DES with the historical per-byte bit order of the original VNC implementation. Crypto primitives
// come from OpenSSL; HyRemote deliberately does not ship a hand-written DES implementation.
HYREMOTE_VNC_AUTH_EXPORT bool computeVncAuthResponse(QByteArrayView password,
                                                     QByteArrayView challenge,
                                                     QByteArray &response,
                                                     QString &error);

// Constant-time response comparison for the authentication boundary.
HYREMOTE_VNC_AUTH_EXPORT bool verifyVncAuthResponse(QByteArrayView password,
                                                    QByteArrayView challenge,
                                                    QByteArrayView candidate,
                                                    QString &error);

#undef HYREMOTE_VNC_AUTH_EXPORT

}  // namespace HyRemote::detail
