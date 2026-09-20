#include "transport/vnc_auth.hpp"

#include <openssl/crypto.h>
#include <openssl/des.h>
#include <openssl/rand.h>

#include <array>
#include <cstring>

namespace HyRemote::detail {
namespace {

unsigned char reverseBits(unsigned char value) noexcept
{
    value = static_cast<unsigned char>(((value & 0x55U) << 1U) | ((value >> 1U) & 0x55U));
    value = static_cast<unsigned char>(((value & 0x33U) << 2U) | ((value >> 2U) & 0x33U));
    return static_cast<unsigned char>((value << 4U) | (value >> 4U));
}

}  // namespace

bool generateVncAuthChallenge(QByteArray &challenge, QString &error)
{
    challenge = QByteArray(kVncAuthChallengeBytes, '\0');
    if (RAND_bytes(reinterpret_cast<unsigned char *>(challenge.data()),
                   static_cast<int>(challenge.size())) != 1) {
        challenge.clear();
        error = QStringLiteral("OpenSSL failed to generate the VNC authentication challenge");
        return false;
    }
    error.clear();
    return true;
}

bool computeVncAuthResponse(QByteArrayView password,
                            QByteArrayView challenge,
                            QByteArray &response,
                            QString &error)
{
    response.clear();
    if (password.isEmpty() || password.size() > kVncAuthMaxPasswordBytes) {
        error = QStringLiteral("VNC authentication password must contain 1 to 8 bytes");
        return false;
    }
    if (challenge.size() != kVncAuthChallengeBytes) {
        error = QStringLiteral("VNC authentication challenge must contain exactly 16 bytes");
        return false;
    }

    DES_cblock key{};
    for (qsizetype index = 0; index < password.size(); ++index) {
        key[static_cast<std::size_t>(index)] =
            reverseBits(static_cast<unsigned char>(password.at(index)));
    }

    DES_key_schedule schedule{};
    DES_set_key_unchecked(&key, &schedule);

    response = QByteArray(kVncAuthChallengeBytes, '\0');
    for (qsizetype offset = 0; offset < kVncAuthChallengeBytes; offset += 8) {
        DES_cblock input{};
        DES_cblock output{};
        std::memcpy(input,
                    challenge.data() + offset,
                    static_cast<std::size_t>(sizeof(DES_cblock)));
        DES_ecb_encrypt(&input, &output, &schedule, DES_ENCRYPT);
        std::memcpy(response.data() + offset,
                    output,
                    static_cast<std::size_t>(sizeof(DES_cblock)));
        OPENSSL_cleanse(input, sizeof(input));
        OPENSSL_cleanse(output, sizeof(output));
    }

    OPENSSL_cleanse(key, sizeof(key));
    OPENSSL_cleanse(&schedule, sizeof(schedule));
    error.clear();
    return true;
}

bool verifyVncAuthResponse(QByteArrayView password,
                           QByteArrayView challenge,
                           QByteArrayView candidate,
                           QString &error)
{
    if (candidate.size() != kVncAuthChallengeBytes) {
        error = QStringLiteral("VNC authentication response must contain exactly 16 bytes");
        return false;
    }

    QByteArray expected;
    if (!computeVncAuthResponse(password, challenge, expected, error))
        return false;

    const bool matches = CRYPTO_memcmp(expected.constData(),
                                       candidate.data(),
                                       static_cast<std::size_t>(expected.size())) == 0;
    OPENSSL_cleanse(expected.data(), static_cast<std::size_t>(expected.size()));
    expected.clear();
    if (!matches)
        error = QStringLiteral("VNC authentication response did not match");
    else
        error.clear();
    return matches;
}

}  // namespace HyRemote::detail
