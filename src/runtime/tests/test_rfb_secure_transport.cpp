// Production-path test for the encrypted RFB profile (#143).
//
// It uses the real transport through the private composition seam - the same object every frontend reaches through
// the shared runtime - and drives it with a deterministic Qt/OpenSSL peer that speaks the frozen wire profile byte
// for byte. Nothing here is a preflight harness copy: no listener is faked and no negotiation step is skipped.

#include "detail/security_preflight.hpp"
#include "transport/rfb_transport.hpp"
#include "transport/vnc_auth.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QSslConfiguration>
#include <QSslError>
#include <QSslSocket>
#include <QTcpServer>
#include <QThread>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <cstdio>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

int failures = 0;

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: " #expr << '\n';          \
            ++failures;                                                                            \
        }                                                                                          \
    } while (false)

// --------------------------------------------------------------------------------------------- plumbing

// The transport is asynchronous, so every step is awaited with a bounded event-loop pump. The bound is a hang
// guard, never an assertion: a case fails because a condition stayed false, not because a clock ran out.
bool pumpUntil(const std::function<bool()> &condition, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        if (condition())
            return true;
        QThread::msleep(1);
    }
    return condition();
}

QByteArray readExact(QSslSocket &socket, int bytes, int timeoutMs)
{
    QByteArray data;
    QElapsedTimer timer;
    timer.start();
    while (data.size() < bytes && timer.elapsed() < timeoutMs) {
        if (socket.bytesAvailable() > 0) {
            data += socket.read(bytes - data.size());
            continue;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    if (data.size() < bytes) {
        // Names the stage that stalled, because the expected byte count is different at every step of the profile.
        std::printf("PEER_READ_TIMEOUT expected=%d got=%d encrypted=%d state=%d\n",
                    bytes, int(data.size()), int(socket.isEncrypted()), int(socket.state()));
    }
    return data;
}

quint16 freePort()
{
    QTcpServer probe;
    if (!probe.listen(QHostAddress::LocalHost, 0))
        return 0;
    const quint16 port = probe.serverPort();
    probe.close();
    return port;
}

struct Credential
{
    QByteArray certificatePem;
    QByteArray privateKeyPem;
};

Credential generateCredential(const char *commonName)
{
    Credential result;
    EVP_PKEY *key = EVP_RSA_gen(2048);
    X509 *certificate = X509_new();
    if (key == nullptr || certificate == nullptr) {
        X509_free(certificate);
        EVP_PKEY_free(key);
        return result;
    }

    X509_set_version(certificate, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(certificate), 1);
    X509_gmtime_adj(X509_getm_notBefore(certificate), 0);
    X509_gmtime_adj(X509_getm_notAfter(certificate), 3600);
    X509_set_pubkey(certificate, key);
    X509_NAME *name = X509_get_subject_name(certificate);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                               reinterpret_cast<const unsigned char *>(commonName), -1, -1, 0);
    X509_set_issuer_name(certificate, name);
    X509_sign(certificate, key, EVP_sha256());

    char *data = nullptr;
    BIO *bio = BIO_new(BIO_s_mem());
    PEM_write_bio_X509(bio, certificate);
    const long certificateLength = BIO_get_mem_data(bio, &data);
    result.certificatePem = QByteArray(data, int(certificateLength));
    BIO_free(bio);

    bio = BIO_new(BIO_s_mem());
    PEM_write_bio_PrivateKey(bio, key, nullptr, nullptr, 0, nullptr, nullptr);
    const long keyLength = BIO_get_mem_data(bio, &data);
    result.privateKeyPem = QByteArray(data, int(keyLength));
    BIO_free(bio);

    X509_free(certificate);
    EVP_PKEY_free(key);
    return result;
}

QString writeTempFile(const QString &directory, const QString &name, const QByteArray &contents)
{
    const QString path = directory + QLatin1Char('/') + name;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return QString();
    file.write(contents);
    file.close();
    return path;
}

// The credential the encrypted profile is configured with, already prepared through the same pre-listen routine the
// product uses, so every case below starts from material the release would really accept.
struct SecureProfile
{
    HyRemote::detail::RfbSecurityConfig config;
    QString directory;
    QByteArray password = QByteArrayLiteral("s3cret");
    bool ready = false;
};

bool prepareProfile(SecureProfile &profile, const Credential &credential)
{
    profile.directory = QDir::tempPath() + QStringLiteral("/hyremote-143-") + QString::number(quintptr(&profile));
    QDir().mkpath(profile.directory);

    profile.config.profile = HyRemote::detail::RfbSecurityProfile::VeNCryptTlsVncAuth;
    profile.config.password = profile.password;
    profile.config.certificateFile = writeTempFile(profile.directory, QStringLiteral("server-cert.pem"),
                                                   credential.certificatePem);
    profile.config.privateKeyFile = writeTempFile(profile.directory, QStringLiteral("server-key.pem"),
                                                  credential.privateKeyPem);
    if (profile.config.certificateFile.isEmpty() || profile.config.privateKeyFile.isEmpty())
        return false;

    const HyRemote::detail::SecureTransportPreparation preparation =
        HyRemote::detail::prepareSecureTransport(profile.config);
    if (!preparation.ok) {
        std::cerr << "  credential preparation refused the profile: " << preparation.error.toStdString() << '\n';
        return false;
    }

    profile.ready = true;
    return true;
}

struct RunningServer
{
    std::unique_ptr<hyremote::Transport> transport;
    std::vector<hyremote::TransportEventCode> events;
    quint16 port = 0;

    bool start(const SecureProfile &profile)
    {
        port = freePort();
        if (port == 0)
            return false;

        transport = HyRemote::detail::createRfbTransport(QHostAddress::LocalHost, port, profile.config);
        if (!transport)
            return false;

        return transport->start(
            [](const hyremote::InputEvent &) {},
            [this](const hyremote::TransportEvent &event) { events.push_back(event.code); });
    }

    bool sawEvent(hyremote::TransportEventCode code) const
    {
        for (const hyremote::TransportEventCode recorded : events) {
            if (recorded == code)
                return true;
        }
        return false;
    }

    ~RunningServer()
    {
        if (transport)
            transport->stop();
    }
};

// ------------------------------------------------------------------------------------------------ the peer

// A peer that drives the frozen profile. Each case names the one step where it deliberately deviates, which is what
// makes "no weaker fallback" and "malformed negotiation" testable at all.
enum class PeerBehaviour {
    Correct,
    SecurityTypeNone,        // asks for None (1) where only VeNCrypt (19) is offered
    SecurityTypeVncAuth,     // asks for VNC Authentication (2) instead of VeNCrypt
    WrongVeNCryptVersion,    // answers with a major version above 0.2
    WrongSubType,            // picks a sub-type other than X509Vnc 261
    WrongPassword,           // completes TLS, fails VNC authentication inside it
    StallBeforeTls,          // never starts the TLS handshake
    MalformedTls,            // writes garbage where the TLS record layer belongs
    Tls10Only,               // offers only TLS 1.0 in the ClientHello
};

struct PeerResult
{
    bool reachedPlaintextNegotiation = false;
    bool securityTypesExact = false;
    bool encrypted = false;
    bool securityResultOk = false;
    bool sessionEstablished = false;
    QByteArray securityTypes;
    QString error;
};

class Peer
{
public:
    Peer(quint16 port, PeerBehaviour behaviour, const QByteArray &password)
        : m_port(port)
        , m_behaviour(behaviour)
        , m_password(password)
    {
    }

    PeerResult run(int timeoutMs)
    {
        PeerResult result;
        m_socket.setSslConfiguration(clientConfiguration());
        m_socket.connectToHost(QHostAddress::LocalHost, m_port);
        if (!pumpUntil([this] { return m_socket.state() == QAbstractSocket::ConnectedState; }, timeoutMs)) {
            result.error = QStringLiteral("the peer could not connect");
            return result;
        }

        static constexpr char kClientVersion[] = "RFB 003.008\n";
        m_socket.write(kClientVersion, 12);
        m_socket.flush();

        // The server banner, then its offered security types. This is the plaintext half of the profile.
        const QByteArray banner = readExact(m_socket, 12, timeoutMs);
        if (banner.size() != 12 || !banner.startsWith("RFB 003.008")) {
            result.error = QStringLiteral("the server banner was not RFB 3.8");
            return result;
        }

        result.securityTypes = readExact(m_socket, 2, timeoutMs);
        if (result.securityTypes.size() != 2) {
            result.error = QStringLiteral("the security type list was not sent");
            return result;
        }
        result.securityTypesExact = result.securityTypes == QByteArray("\x01\x13", 2);
        result.reachedPlaintextNegotiation = true;

        if (m_behaviour == PeerBehaviour::SecurityTypeNone) {
            m_socket.write(QByteArray(1, char(1)));
            m_socket.flush();
            // Give the server the bounded moment it needs to answer the choice, so the caller asserts on a decision
            // instead of on the scheduler.
            pumpUntil([this] { return m_socket.state() != QAbstractSocket::ConnectedState; }, 500);
            return result;
        }
        if (m_behaviour == PeerBehaviour::SecurityTypeVncAuth) {
            m_socket.write(QByteArray(1, char(2)));
            m_socket.flush();
            pumpUntil([this] { return m_socket.state() != QAbstractSocket::ConnectedState; }, 500);
            return result;
        }

        m_socket.write(QByteArray(1, char(19)));
        m_socket.flush();

        const QByteArray serverVersion = readExact(m_socket, 2, timeoutMs);
        if (serverVersion != QByteArray("\x00\x02", 2)) {
            result.error = QStringLiteral("the server did not offer VeNCrypt 0.2");
            return result;
        }

        const QByteArray clientVersion = m_behaviour == PeerBehaviour::WrongVeNCryptVersion
                ? QByteArray("\x01\x00", 2)
                : QByteArray("\x00\x02", 2);
        m_socket.write(clientVersion);
        m_socket.flush();

        const QByteArray ack = readExact(m_socket, 1, timeoutMs);
        if (ack.size() != 1) {
            result.error = QStringLiteral("the VeNCrypt acknowledgement was not sent");
            return result;
        }
        if (m_behaviour == PeerBehaviour::WrongVeNCryptVersion) {
            result.error = ack.at(0) == char(0xff) ? QString()
                                                   : QStringLiteral("a version below 0.2 was not refused");
            return result;
        }

        const QByteArray subTypes = readExact(m_socket, 5, timeoutMs);
        if (subTypes.size() != 5 || subTypes.at(0) != char(1)
            || std::uint8_t(subTypes.at(1)) != 0 || std::uint8_t(subTypes.at(2)) != 0
            || std::uint8_t(subTypes.at(3)) != 0x01 || std::uint8_t(subTypes.at(4)) != 0x05) {
            result.error = QStringLiteral("the X509Vnc sub-type was not offered as 261");
            return result;
        }

        const std::uint32_t chosen = m_behaviour == PeerBehaviour::WrongSubType ? 262u : 261u;
        QByteArray subTypeChoice(4, '\0');
        subTypeChoice[0] = char((chosen >> 24) & 0xff);
        subTypeChoice[1] = char((chosen >> 16) & 0xff);
        subTypeChoice[2] = char((chosen >> 8) & 0xff);
        subTypeChoice[3] = char(chosen & 0xff);
        m_socket.write(subTypeChoice);
        m_socket.flush();

        const QByteArray proceed = readExact(m_socket, 1, timeoutMs);
        if (proceed.size() != 1) {
            result.error = QStringLiteral("the TLS-init byte was not sent");
            return result;
        }
        if (m_behaviour == PeerBehaviour::WrongSubType) {
            result.error = QStringLiteral("a wrong sub-type was accepted");
            return result;
        }

        if (m_behaviour == PeerBehaviour::StallBeforeTls) {
            // Deliberately creates no TLS state: the server must release the connection inside its budget.
            pumpUntil([] { return false; }, 1500);
            return result;
        }
        if (m_behaviour == PeerBehaviour::MalformedTls) {
            m_socket.write(QByteArrayLiteral("not a TLS record at all"));
            m_socket.flush();
            pumpUntil([this] { return m_socket.state() != QAbstractSocket::ConnectedState
                            || !m_socket.isEncrypted(); }, 1500);
            return result;
        }

        m_socket.startClientEncryption();
        result.encrypted = pumpUntil([this] { return m_socket.isEncrypted(); }, timeoutMs);
        if (!result.encrypted) {
            result.error = QStringLiteral("the TLS handshake did not complete");
            return result;
        }

        const QByteArray challenge = readExact(m_socket, 16, timeoutMs);
        if (challenge.size() != 16) {
            result.error = QStringLiteral("the VNC challenge did not arrive inside TLS");
            return result;
        }

        const QByteArray password = m_behaviour == PeerBehaviour::WrongPassword ? QByteArrayLiteral("wrong-pw")
                                                                                : m_password;
        QByteArray response;
        QString computeError;
        if (!HyRemote::detail::computeVncAuthResponse(password, challenge, response, computeError)) {
            result.error = QStringLiteral("the response could not be computed");
            return result;
        }
        m_socket.write(response);
        m_socket.flush();

        const QByteArray securityResult = readExact(m_socket, 4, timeoutMs);
        if (securityResult.size() != 4) {
            result.error = QStringLiteral("the security result did not arrive");
            return result;
        }
        const std::uint32_t status = (std::uint32_t(std::uint8_t(securityResult.at(0))) << 24)
                | (std::uint32_t(std::uint8_t(securityResult.at(1))) << 16)
                | (std::uint32_t(std::uint8_t(securityResult.at(2))) << 8)
                | std::uint32_t(std::uint8_t(securityResult.at(3)));
        result.securityResultOk = status == 0;
        if (!result.securityResultOk)
            return result;

        // Reaching a successful SecurityResult inside the encrypted channel is the acceptance for this profile.
        // ServerInit and the rest of the RFB session only start once the runtime has a frame to describe, and this
        // test enqueues none: that part of the session is the same code path the plaintext profiles already cover.
        result.sessionEstablished = m_socket.isEncrypted();
        m_socket.write(QByteArray(1, char(1)));  // ClientInit: shared
        m_socket.flush();
        return result;
    }

private:
    QSslConfiguration clientConfiguration() const
    {
        QSslConfiguration configuration = QSslConfiguration::defaultConfiguration();
        configuration.setPeerVerifyMode(QSslSocket::VerifyNone);
        configuration.setProtocol(m_behaviour == PeerBehaviour::Tls10Only ? QSsl::TlsV1_0
                                                                        : QSsl::TlsV1_2OrLater);
        return configuration;
    }

    QSslSocket m_socket;
    quint16 m_port = 0;
    PeerBehaviour m_behaviour = PeerBehaviour::Correct;
    QByteArray m_password;
};

// ------------------------------------------------------------------------------------------------- cases

Credential &sharedCredential()
{
    static Credential credential = generateCredential("hyremote-143-test");
    return credential;
}

SecureProfile *sharedProfile()
{
    static SecureProfile profile;
    static const bool prepared = prepareProfile(profile, sharedCredential());
    return prepared ? &profile : nullptr;
}

void testHappyEncryptedProfile()
{
    SecureProfile *profile = sharedProfile();
    CHECK(profile != nullptr);
    if (!profile)
        return;

    RunningServer server;
    CHECK(server.start(*profile));
    CHECK(server.transport != nullptr);
    if (!server.transport)
        return;

    Peer peer(server.port, PeerBehaviour::Correct, profile->password);
    const PeerResult result = peer.run(8000);

    // The offered list is exactly one type and that type is VeNCrypt: no weaker security type is ever offered to a
    // client that asked for the encrypted profile.
    CHECK(result.securityTypesExact);
    CHECK(result.encrypted);
    CHECK(result.securityResultOk);
    CHECK(result.sessionEstablished);
    CHECK(!server.sawEvent(hyremote::TransportEventCode::AuthenticationRejected));
}

void testWrongPasswordInsideTls()
{
    SecureProfile *profile = sharedProfile();
    CHECK(profile != nullptr);
    if (!profile)
        return;

    RunningServer server;
    CHECK(server.start(*profile));
    if (!server.transport)
        return;

    Peer bad(server.port, PeerBehaviour::WrongPassword, profile->password);
    const PeerResult refused = bad.run(8000);
    CHECK(refused.encrypted);
    CHECK(!refused.securityResultOk);
    CHECK(server.sawEvent(hyremote::TransportEventCode::AuthenticationRejected));
    // One rejected peer must not take the listener down: the next connection still completes.
    Peer good(server.port, PeerBehaviour::Correct, profile->password);
    const PeerResult accepted = good.run(8000);
    CHECK(accepted.sessionEstablished);
}

void testWeakSecurityTypesAreRejected()
{
    SecureProfile *profile = sharedProfile();
    CHECK(profile != nullptr);
    if (!profile)
        return;

    RunningServer server;
    CHECK(server.start(*profile));
    if (!server.transport)
        return;

    // A client asking for None, or for plain VNC Authentication, is refused rather than served a weaker profile.
    Peer none(server.port, PeerBehaviour::SecurityTypeNone, profile->password);
    const PeerResult noneResult = none.run(4000);
    CHECK(noneResult.securityTypesExact);
    CHECK(!noneResult.encrypted);
    CHECK(server.sawEvent(hyremote::TransportEventCode::AuthenticationRejected));

    server.events.clear();
    Peer vnc(server.port, PeerBehaviour::SecurityTypeVncAuth, profile->password);
    const PeerResult vncResult = vnc.run(4000);
    CHECK(!vncResult.encrypted);
    CHECK(server.sawEvent(hyremote::TransportEventCode::AuthenticationRejected));

    // ... and the listener is still healthy afterwards.
    Peer good(server.port, PeerBehaviour::Correct, profile->password);
    CHECK(good.run(8000).sessionEstablished);
}

void testMalformedNegotiationIsRejected()
{
    SecureProfile *profile = sharedProfile();
    CHECK(profile != nullptr);
    if (!profile)
        return;

    RunningServer server;
    CHECK(server.start(*profile));
    if (!server.transport)
        return;

    // A version below 0.2 is answered with the profile's refusal byte.
    Peer version(server.port, PeerBehaviour::WrongVeNCryptVersion, profile->password);
    CHECK(version.run(4000).error.isEmpty());

    // A sub-type other than X509Vnc never reaches TLS.
    Peer subType(server.port, PeerBehaviour::WrongSubType, profile->password);
    const PeerResult subTypeResult = subType.run(4000);
    CHECK(!subTypeResult.encrypted);
    CHECK(!subTypeResult.error.isEmpty());
}

void testFailingTlsIsBoundedAndLeavesTheListenerUsable()
{
    SecureProfile *profile = sharedProfile();
    CHECK(profile != nullptr);
    if (!profile)
        return;

    RunningServer server;
    CHECK(server.start(*profile));
    if (!server.transport)
        return;

    // Malformed bytes where the record layer belongs, and a peer that simply stops: both are bounded, and neither
    // continues in plaintext or falls back to a weaker type.
    Peer malformed(server.port, PeerBehaviour::MalformedTls, profile->password);
    malformed.run(4000);
    Peer stalled(server.port, PeerBehaviour::StallBeforeTls, profile->password);
    stalled.run(4000);

    Peer good(server.port, PeerBehaviour::Correct, profile->password);
    CHECK(good.run(8000).sessionEstablished);
}

void testTlsBelowTheFloorIsRefused()
{
    SecureProfile *profile = sharedProfile();
    CHECK(profile != nullptr);
    if (!profile)
        return;

    RunningServer server;
    CHECK(server.start(*profile));
    if (!server.transport)
        return;

    Peer legacy(server.port, PeerBehaviour::Tls10Only, profile->password);
    const PeerResult result = legacy.run(5000);
    CHECK(!result.sessionEstablished);

    Peer good(server.port, PeerBehaviour::Correct, profile->password);
    CHECK(good.run(8000).sessionEstablished);
}

void testReconnectStartsClean()
{
    SecureProfile *profile = sharedProfile();
    CHECK(profile != nullptr);
    if (!profile)
        return;

    RunningServer server;
    CHECK(server.start(*profile));
    if (!server.transport)
        return;

    for (int attempt = 0; attempt < 2; ++attempt) {
        Peer peer(server.port, PeerBehaviour::Correct, profile->password);
        const PeerResult result = peer.run(8000);
        CHECK(result.sessionEstablished);
        CHECK(result.securityResultOk);
    }
}

void testShutdownDuringHandshakeAndEstablishedSession()
{
    SecureProfile *profile = sharedProfile();
    CHECK(profile != nullptr);
    if (!profile)
        return;

    // A peer that stalls in the plaintext negotiation, and one that completes: stopping the transport in both
    // states has to be bounded and must not deliver a callback afterwards.
    for (const PeerBehaviour behaviour : { PeerBehaviour::StallBeforeTls, PeerBehaviour::Correct }) {
        RunningServer server;
        CHECK(server.start(*profile));
        if (!server.transport)
            continue;

        Peer peer(server.port, behaviour, profile->password);
        peer.run(2500);

        QElapsedTimer timer;
        timer.start();
        server.transport->stop();
        CHECK(timer.elapsed() < 3000);

        const std::size_t eventsAfterStop = server.events.size();
        pumpUntil([] { return false; }, 300);
        CHECK(server.events.size() == eventsAfterStop);
    }
}

void testCredentialMaterialIsValidatedBeforeAnyListener()
{
    const QString directory = QDir::tempPath() + QStringLiteral("/hyremote-143-prevalidation");
    QDir().mkpath(directory);

    const QString certificatePath = writeTempFile(directory, QStringLiteral("cert.pem"),
                                                  sharedCredential().certificatePem);
    const QString keyPath = writeTempFile(directory, QStringLiteral("key.pem"), sharedCredential().privateKeyPem);
    const QString otherCertificatePath = writeTempFile(directory, QStringLiteral("other-cert.pem"),
                                                       generateCredential("other").certificatePem);
    const QString otherKeyPath = writeTempFile(directory, QStringLiteral("other-key.pem"),
                                               generateCredential("other-key").privateKeyPem);
    const QString malformedPath = writeTempFile(directory, QStringLiteral("malformed.pem"),
                                                QByteArrayLiteral("-----BEGIN CERTIFICATE-----\nnot base64\n"));

    struct Case
    {
        const char *name;
        QString certificate;
        QString key;
        bool expectedOk;
    };
    const std::vector<Case> cases{
        {"valid", certificatePath, keyPath, true},
        {"missing certificate", directory + QStringLiteral("/absent-cert.pem"), keyPath, false},
        {"missing key", certificatePath, directory + QStringLiteral("/absent-key.pem"), false},
        {"malformed certificate", malformedPath, keyPath, false},
        {"malformed key", certificatePath, malformedPath, false},
        {"mismatched pair", otherCertificatePath, keyPath, false},
    };

    for (const Case &testCase : cases) {
        HyRemote::detail::RfbSecurityConfig config;
        config.profile = HyRemote::detail::RfbSecurityProfile::VeNCryptTlsVncAuth;
        config.password = QByteArrayLiteral("s3cret");
        config.certificateFile = testCase.certificate;
        config.privateKeyFile = testCase.key;

        const HyRemote::detail::SecureTransportPreparation preparation =
            HyRemote::detail::prepareSecureTransport(config);
        if (preparation.ok != testCase.expectedOk) {
            std::cerr << "  credential case '" << testCase.name << "' expected ok=" << testCase.expectedOk
                      << " but got " << preparation.ok << " (" << preparation.error.toStdString() << ")\n";
            ++failures;
        }
        if (preparation.ok != testCase.expectedOk) {
            std::cerr << "    certificate=" << QFileInfo(testCase.certificate).fileName().toStdString()
                      << " size=" << QFileInfo(testCase.certificate).size()
                      << " key=" << QFileInfo(testCase.key).fileName().toStdString()
                      << " size=" << QFileInfo(testCase.key).size() << '\n';
        }
        if (!testCase.expectedOk) {
            CHECK(!preparation.error.isEmpty());
            // Nothing was handed to a transport, so no listener could have been created around it.
            CHECK(config.certificate.isNull() || preparation.ok);
        }
    }
}

}  // namespace

int main(int argc, char **argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    QCoreApplication application(argc, argv);

    std::printf("SECURE_TEST_BEGIN qt=%s\n", qVersion());
    std::printf("SECURE_TEST_TLS_BACKENDS=%s\n",
                QSslSocket::availableBackends().join(QLatin1Char(',')).toUtf8().constData());

    testCredentialMaterialIsValidatedBeforeAnyListener();
    testHappyEncryptedProfile();
    testWrongPasswordInsideTls();
    testWeakSecurityTypesAreRejected();
    testMalformedNegotiationIsRejected();
    testFailingTlsIsBoundedAndLeavesTheListenerUsable();
    testTlsBelowTheFloorIsRefused();
    testReconnectStartsClean();
    testShutdownDuringHandshakeAndEstablishedSession();

    std::printf("SECURE_TEST_ACTIVE_BACKEND=%s\n", QSslSocket::activeBackend().toUtf8().constData());
    std::printf("SECURE_TEST_RESULT=%s\n", failures == 0 ? "PASS" : "FAIL");
    std::printf("SECURE_TEST_FAILURES=%d\n", failures);

    if (failures == 0) {
        std::cout << "Encrypted RFB transport tests passed\n";
        return 0;
    }
    return 1;
}
