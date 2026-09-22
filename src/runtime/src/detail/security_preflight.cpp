#include "detail/security_preflight.hpp"

#include <QFile>
#include <QSslCertificate>
#include <QSslKey>
#include <QSslSocket>

#include <algorithm>

#ifdef HYREMOTE_HAS_TRANSPORT_SECURITY
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#endif

namespace HyRemote::detail {
namespace {

constexpr qint64 kMaxCredentialFileBytes = 256 * 1024;

void wipe(QByteArray &bytes) noexcept
{
    if (!bytes.isEmpty())
        std::fill(bytes.begin(), bytes.end(), '\0');
    bytes.clear();
}

#ifdef HYREMOTE_HAS_TRANSPORT_SECURITY

bool loadBoundedFile(const QString &path, const char *label, QByteArray &out, QString &error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("%1 is missing or not readable").arg(QString::fromLatin1(label));
        return false;
    }
    const qint64 size = file.size();
    if (size <= 0 || size > kMaxCredentialFileBytes) {
        error = QStringLiteral("%1 is empty or exceeds the bounded credential-file size")
                    .arg(QString::fromLatin1(label));
        return false;
    }

    out = file.read(kMaxCredentialFileBytes + 1);
    if (out.size() > kMaxCredentialFileBytes) {
        wipe(out);
        error = QStringLiteral("%1 exceeds the bounded credential-file size").arg(QString::fromLatin1(label));
        return false;
    }
    return true;
}

// Proves that the certificate and the private key belong together. Qt's public API cannot answer this:
// QSslCertificate::publicKey() returns a public QSslKey and QSslKey::operator== treats a matching
// public/private pair as different, which the #258 preflight measured before choosing this route. The OpenSSL
// objects live and die inside this function; no OpenSSL type crosses the Runtime-private boundary.
bool credentialPairMatches(const QByteArray &certificatePem, const QByteArray &privateKeyPem, QString &error)
{
    BIO *certificateBio = BIO_new_mem_buf(certificatePem.constData(), certificatePem.size());
    BIO *keyBio = BIO_new_mem_buf(privateKeyPem.constData(), privateKeyPem.size());
    X509 *certificate = certificateBio ? PEM_read_bio_X509(certificateBio, nullptr, nullptr, nullptr) : nullptr;
    EVP_PKEY *key = keyBio ? PEM_read_bio_PrivateKey(keyBio, nullptr, nullptr, nullptr) : nullptr;

    bool ok = false;
    if (!certificate) {
        error = QStringLiteral("certificateFile does not contain a parseable X.509 certificate");
    } else if (!key) {
        error = QStringLiteral("privateKeyFile does not contain a parseable private key");
    } else if (X509_check_private_key(certificate, key) != 1) {
        error = QStringLiteral("privateKeyFile does not match certificateFile");
    } else {
        ok = true;
    }

    if (certificate)
        X509_free(certificate);
    if (key)
        EVP_PKEY_free(key);
    BIO_free(certificateBio);
    BIO_free(keyBio);
    return ok;
}

#endif  // HYREMOTE_HAS_TRANSPORT_SECURITY

}  // namespace

SecureTransportPreparation prepareSecureTransport(RfbSecurityConfig &config)
{
    SecureTransportPreparation result;
    if (config.profile != RfbSecurityProfile::VeNCryptTlsVncAuth) {
        result.ok = true;
        return result;
    }

#ifndef HYREMOTE_HAS_TRANSPORT_SECURITY
    result.unavailable = true;
    result.error = QStringLiteral("the encrypted profile needs the transport-security capability, which is not "
                                  "compiled into this build");
    return result;
#else
    // 1. The backend is chosen explicitly, before any TLS object exists, and the choice is verified. A host that has
    //    already locked the process onto another backend therefore fails closed instead of quietly running on it.
    const QStringList backends = QSslSocket::availableBackends();
    if (!backends.contains(QStringLiteral("openssl"))) {
        result.unavailable = true;
        result.error = QStringLiteral("the encrypted profile requires the OpenSSL TLS backend, which this runtime "
                                      "does not provide");
        return result;
    }
    if (!QSslSocket::setActiveBackend(QStringLiteral("openssl"))
        || QSslSocket::activeBackend() != QStringLiteral("openssl")) {
        result.unavailable = true;
        result.error = QStringLiteral("the encrypted profile requires the OpenSSL TLS backend, which could not be "
                                      "selected in this process");
        return result;
    }

    // 2. The material is read, parsed and matched here, so a listener is never created around unusable credentials.
    QByteArray certificatePem;
    QByteArray privateKeyPem;
    const bool filesRead = loadBoundedFile(config.certificateFile, "certificateFile", certificatePem, result.error)
            && loadBoundedFile(config.privateKeyFile, "privateKeyFile", privateKeyPem, result.error);
    if (!filesRead) {
        wipe(certificatePem);
        wipe(privateKeyPem);
        return result;
    }

    QString pairError;
    const bool pairMatches = credentialPairMatches(certificatePem, privateKeyPem, pairError);

    const QSslCertificate certificate(pairMatches ? certificatePem : QByteArray{}, QSsl::Pem);
    QSslKey key(pairMatches ? privateKeyPem : QByteArray{}, QSsl::Rsa, QSsl::Pem);
    if (pairMatches && key.isNull())
        key = QSslKey(privateKeyPem, QSsl::Ec, QSsl::Pem);

    wipe(certificatePem);
    wipe(privateKeyPem);

    if (!pairMatches) {
        result.error = pairError;
        return result;
    }
    if (certificate.isNull() || key.isNull()) {
        result.error = QStringLiteral("the certificate or the private key could not be loaded for TLS");
        return result;
    }

    config.certificate = certificate;
    config.privateKey = key;
    result.ok = true;
    return result;
#endif
}

}  // namespace HyRemote::detail
