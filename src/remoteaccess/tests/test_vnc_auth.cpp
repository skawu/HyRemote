#include "transport/vnc_auth.hpp"

#include <QByteArray>
#include <QString>

#include <cstdio>

namespace {

bool check(bool condition, const char *message)
{
    if (!condition)
        std::fprintf(stderr, "FAIL: %s\n", message);
    return condition;
}

QByteArray fromHex(const char *hex)
{
    return QByteArray::fromHex(QByteArray(hex));
}

}  // namespace

int main()
{
    using HyRemote::detail::computeVncAuthResponse;
    using HyRemote::detail::generateVncAuthChallenge;
    using HyRemote::detail::kVncAuthChallengeBytes;
    using HyRemote::detail::verifyVncAuthResponse;

    // Interoperability vector from a real VNC challenge/response exchange. It also locks the
    // historical VNC per-byte DES key bit order; ordinary DES with the literal password produces a
    // different response and is not RFB VNC authentication.
    const QByteArray password("testingg");
    const QByteArray challenge = fromHex("b4a7257a443426527dd9d987fa6b099f");
    const QByteArray expected = fromHex("4838c102d8cbb1decd38ecdbec533bc7");

    QString error;
    QByteArray response;
    if (!check(computeVncAuthResponse(password, challenge, response, error),
               "known VNC auth vector computes")
        || !check(error.isEmpty(), "known vector has no error")
        || !check(response == expected, "known VNC auth response matches interoperability vector")) {
        return 1;
    }

    if (!check(verifyVncAuthResponse(password, challenge, expected, error),
               "correct response verifies")
        || !check(error.isEmpty(), "successful verification has no error")) {
        return 2;
    }

    QByteArray wrong = expected;
    wrong[0] = static_cast<char>(wrong.at(0) ^ 0x01);
    if (!check(!verifyVncAuthResponse(password, challenge, wrong, error),
               "wrong response is rejected")
        || !check(!error.contains(QString::fromLatin1(password)),
                  "verification error never contains the password")) {
        return 3;
    }

    QByteArray randomChallenge;
    error.clear();
    if (!check(generateVncAuthChallenge(randomChallenge, error),
               "OpenSSL challenge generation succeeds")
        || !check(randomChallenge.size() == kVncAuthChallengeBytes,
                  "generated challenge is exactly 16 bytes")) {
        return 4;
    }

    error.clear();
    if (!check(!computeVncAuthResponse(QByteArray("123456789"), challenge, response, error),
               "password longer than eight bytes is rejected rather than truncated")) {
        return 5;
    }

    error.clear();
    if (!check(!computeVncAuthResponse(password, QByteArray("short"), response, error),
               "non-16-byte challenge is rejected")) {
        return 6;
    }

    return 0;
}
