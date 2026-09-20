#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QString>

namespace HyRemote::detail {

inline constexpr qsizetype kVncAuthChallengeBytes = 16;
inline constexpr qsizetype kVncAuthMaxPasswordBytes = 8;

// Generates one cryptographically random RFB VNC-auth challenge. This is private transport
// machinery; no challenge, password or response is exposed through the product API.
bool generateVncAuthChallenge(QByteArray &challenge, QString &error);

// Computes the 16-byte RFB VNC-auth response expected for `challenge`. VNC authentication uses
// DES with the historical per-byte bit order of the original VNC implementation. Crypto primitives
// come from OpenSSL; HyRemote deliberately does not ship a hand-written DES implementation.
bool computeVncAuthResponse(QByteArrayView password,
                            QByteArrayView challenge,
                            QByteArray &response,
                            QString &error);

// Constant-time response comparison for the authentication boundary.
bool verifyVncAuthResponse(QByteArrayView password,
                           QByteArrayView challenge,
                           QByteArrayView candidate,
                           QString &error);

}  // namespace HyRemote::detail
