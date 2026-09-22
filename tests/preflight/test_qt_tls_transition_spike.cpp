// V0.2 technical preflight (issue #258, Preflight B): the Qt TLS transition seam.
//
// The risk this spike removes is not "Qt has TLS". It is that RFB's first half - the version banner and the VeNCrypt
// negotiation - is plaintext on a connection that must later become TLS without being re-accepted, re-connected or
// handed to a different socket object with an unread buffer still in it.
//
// The shape under test is the one the preflight selected: the accepted descriptor is owned by a QSslSocket from the
// moment it is accepted, through QTcpServer::incomingConnection(qintptr). The connection behaves as a plain stream
// until the server decides to upgrade it, and from then on the same object carries TLS. No QTcpSocket is used first
// and no descriptor is ever migrated between objects.
//
// Preflight evidence only: this is not the #143 implementation, and it speaks just enough of the profile to cross the
// seam. Credentials are supplied by the harness as file paths, so the spike carries no key material.

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QHostAddress>
#include <QSslCertificate>
#include <QSslCipher>
#include <QSslConfiguration>
#include <QSslError>
#include <QSslKey>
#include <QSslSocket>
#include <QTcpServer>
#include <QtGlobal>

#include <cstdio>
#include <functional>
#include <vector>

namespace {

int failures = 0;

void require(bool condition, const char *what)
{
    if (condition) {
        std::printf("case ok: %s\n", what);
    } else {
        std::printf("CASE FAILED: %s\n", what);
        ++failures;
    }
}

QByteArray rfbBanner()
{
    return QByteArrayLiteral("RFB 003.008\n");
}

// The server and the client of this spike live in one process, so a blocking waitFor*() on the client would stop the
// same event loop that has to serve the server side and deadlock the pair. Every wait therefore pumps events until its
// condition holds or the budget runs out.
bool pumpUntil(const std::function<bool()> &done, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (!done() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    return done();
}

int pumpForReadyRead(QIODevice *device, int timeoutMs)
{
    pumpUntil([device] { return device->bytesAvailable() > 0; }, timeoutMs);
    return int(device->bytesAvailable());
}

bool pumpForConnected(QSslSocket *client, int timeoutMs)
{
    return pumpUntil([client] { return client->state() == QAbstractSocket::ConnectedState; }, timeoutMs);
}

bool pumpForEncrypted(QSslSocket *client, int timeoutMs)
{
    return pumpUntil([client] { return client->isEncrypted(); }, timeoutMs);
}

bool pumpForDisconnected(QSslSocket *client, int timeoutMs)
{
    return pumpUntil([client] { return client->state() == QAbstractSocket::UnconnectedState; }, timeoutMs);
}

constexpr quint8 kSecurityTypeVeNCrypt = 19;
constexpr quint32 kSubTypeX509Vnc = 261;

enum class Mode { Upgrade, NeverUpgrade, EmptyKey };

// The accepted descriptor belongs to this socket from the first byte.
class TransitionServer : public QTcpServer
{
public:
    TransitionServer(const QSslCertificate &certificate, const QSslKey &key, Mode mode)
        : m_certificate(certificate), m_key(key), m_mode(mode)
    {
    }

    int handshakesCompleted = 0;
    int plaintextPreambleBytes = 0;
    bool sawSubtypeChoice = false;
    bool descriptorStable = true;
    bool refusedToStartEncryption = false;

protected:
    void incomingConnection(qintptr descriptor) override
    {
        auto *socket = new QSslSocket(this);
        QSslConfiguration configuration = QSslConfiguration::defaultConfiguration();
        configuration.setProtocol(QSsl::TlsV1_2OrLater);
        configuration.setLocalCertificate(m_certificate);
        configuration.setPrivateKey(m_key);
        configuration.setPeerVerifyMode(QSslSocket::VerifyNone);
        socket->setSslConfiguration(configuration);
        if (!socket->setSocketDescriptor(descriptor)) {
            socket->deleteLater();
            return;
        }
        const qintptr acceptedDescriptor = socket->socketDescriptor();
        m_sockets.push_back(socket);

        connect(socket, &QSslSocket::readyRead, this, [this, socket, acceptedDescriptor] {
            m_buffer.append(socket->readAll());
            plaintextPreambleBytes = int(m_buffer.size());
            // Explicit per-step accounting: the plaintext half has a fixed byte budget, and counting it removes the
            // guesswork that made the first version of this spike never reach the upgrade step at all.
            for (;;) {
                const int needed = m_expect - m_consumed;
                if (needed <= 0 || m_buffer.size() < m_consumed + needed) {
                    return;
                }
                m_consumed += needed;
                switch (m_step) {
                case 0:   // client banner (12 bytes) -> offer VeNCrypt
                    if (!m_buffer.startsWith(rfbBanner())) {
                        socket->abort();
                        return;
                    }
                    socket->write(QByteArray(1, char(1)) + QByteArray(1, char(kSecurityTypeVeNCrypt)));
                    m_step = 1;
                    m_expect = 1;
                    break;
                case 1:   // security type choice (1 byte) -> VeNCrypt version 0.2
                    socket->write(QByteArray(1, char(0)) + QByteArray(1, char(2)));
                    m_step = 2;
                    m_expect = 2;
                    break;
                case 2:   // client version (2 bytes) -> ack, sub-type list
                    socket->write(QByteArray(1, char(0)));                        // ack: 0.2 accepted
                    socket->write(QByteArray(1, char(1)));                        // one sub-type offered
                    {
                        QByteArray subType(4, '\0');
                        subType[2] = char((kSubTypeX509Vnc >> 8) & 0xff);
                        subType[3] = char(kSubTypeX509Vnc & 0xff);
                        socket->write(subType);
                    }
                    m_step = 3;
                    m_expect = 4;
                    break;
                case 3:   // sub-type choice (4 bytes) -> TLS may start
                    sawSubtypeChoice = true;
                    // Observed against a real viewer: the client reads exactly one byte before it will start TLS.
                    socket->write(QByteArray(1, char(1)));
                    socket->flush();
                    if (m_mode == Mode::NeverUpgrade) {
                        return;
                    }
                    if (m_mode == Mode::EmptyKey) {
                        refusedToStartEncryption = m_key.isNull();
                        socket->abort();
                        return;
                    }
                    socket->startServerEncryption();
                    descriptorStable = descriptorStable && (socket->socketDescriptor() == acceptedDescriptor);
                    m_step = 4;
                    m_expect = 0;
                    return;
                default:
                    return;
                }
                socket->flush();
            }
        });

        connect(socket, &QSslSocket::encrypted, this, [this, socket, acceptedDescriptor] {
            ++handshakesCompleted;
            descriptorStable = descriptorStable && (socket->socketDescriptor() == acceptedDescriptor);
            socket->write(QByteArrayLiteral("hyremote-preflight-tls"));
            socket->flush();
        });
        connect(socket, &QSslSocket::sslErrors, this, [](const QList<QSslError> &) {});
    }

private:
    QSslCertificate m_certificate;
    QSslKey m_key;
    Mode m_mode;
    QByteArray m_buffer;
    int m_step = 0;
    int m_expect = rfbBanner().size();   // the client banner arrives first
    int m_consumed = 0;
    std::vector<QSslSocket *> m_sockets;
};

bool loadCredentials(QSslCertificate *certificate, QSslKey *key)
{
    QFile certificateFile(qEnvironmentVariable("HYREMOTE_SPIKE_CERT_FILE"));
    QFile keyFile(qEnvironmentVariable("HYREMOTE_SPIKE_KEY_FILE"));
    if (!certificateFile.open(QIODevice::ReadOnly) || !keyFile.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray keyBytes = keyFile.readAll();
    *certificate = QSslCertificate(certificateFile.readAll(), QSsl::Pem);
    *key = QSslKey(keyBytes, QSsl::Rsa, QSsl::Pem);
    if (key->isNull()) {
        *key = QSslKey(keyBytes, QSsl::Ec, QSsl::Pem);
    }
    return !certificate->isNull() && !key->isNull();
}

QByteArray subTypeBytes()
{
    QByteArray subType(4, '\0');
    subType[2] = char((kSubTypeX509Vnc >> 8) & 0xff);
    subType[3] = char(kSubTypeX509Vnc & 0xff);
    return subType;
}

// Drives the plaintext half of the profile, then hands the socket to TLS.
void drivePlaintextThenUpgrade(QSslSocket *client, const std::function<void()> &afterSubtype)
{
    client->write(rfbBanner());
    client->write(QByteArray(1, char(kSecurityTypeVeNCrypt)));
    client->flush();
    pumpForReadyRead(client, 5000);
    client->readAll();
    client->write(QByteArray(1, char(0)) + QByteArray(1, char(2)));
    client->flush();
    pumpForReadyRead(client, 5000);
    client->readAll();
    {
        QByteArray subType(4, '\0');
        subType[2] = char((kSubTypeX509Vnc >> 8) & 0xff);
        subType[3] = char(kSubTypeX509Vnc & 0xff);
        client->write(subType);
    }
    client->flush();
    afterSubtype();
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);

    QSslCertificate certificate;
    QSslKey key;
    if (!loadCredentials(&certificate, &key)) {
        std::printf("SPIKE_RESULT=SKIPPED credential material not supplied\n");
        return 0;
    }

    std::printf("SPIKE_INFO qt=%s ssl_supported=%d active_backend=%s\n", qVersion(),
                int(QSslSocket::supportsSsl()), qPrintable(QSslSocket::activeBackend()));

    // 1. The transition itself: plaintext preamble, then TLS on the same descriptor.
    {
        TransitionServer server(certificate, key, Mode::Upgrade);
        require(server.listen(QHostAddress::LocalHost, 0), "spike listener accepts connections");

        QSslSocket client;
        QSslConfiguration configuration = QSslConfiguration::defaultConfiguration();
        configuration.setProtocol(QSsl::TlsV1_2OrLater);
        configuration.setPeerVerifyMode(QSslSocket::VerifyNone);
        client.setSslConfiguration(configuration);
        client.connectToHost(QHostAddress::LocalHost, server.serverPort());
        require(pumpForConnected(&client, 5000), "client connected to the spike server");
        drivePlaintextThenUpgrade(&client, [&client] {
            pumpForReadyRead(&client, 5000);
            client.readAll();
            client.startClientEncryption();
        });
        const bool encrypted = pumpForEncrypted(&client, 8000);
        require(encrypted, "TLS handshake completed after the plaintext preamble on the same connection");
        if (encrypted) {
            require(client.isEncrypted(), "the same socket object reports itself encrypted");
            require(client.sessionProtocol() >= QSsl::TlsV1_2, "negotiated protocol is at least TLS 1.2");
            std::printf("SPIKE_INFO negotiated_protocol=%d cipher=%s\n", int(client.sessionProtocol()),
                        qPrintable(client.sessionCipher().name()));
            require(server.sawSubtypeChoice, "server saw the plaintext VeNCrypt sub-type choice before TLS");
            require(server.plaintextPreambleBytes > 0, "plaintext preamble bytes were exchanged before TLS");
            require(server.handshakesCompleted == 1, "server completed exactly one handshake");
            require(server.descriptorStable, "the accepted descriptor was never migrated to another socket");
        }
        client.disconnectFromHost();
        require(client.state() == QAbstractSocket::UnconnectedState || pumpForDisconnected(&client, 3000),
                "shutdown is bounded");
    }

    // 2. Reconnect: the seam is repeatable on a fresh connection.
    {
        TransitionServer server(certificate, key, Mode::Upgrade);
        require(server.listen(QHostAddress::LocalHost, 0), "second listener started");
        for (int attempt = 1; attempt <= 2; ++attempt) {
            QSslSocket client;
            QSslConfiguration configuration = QSslConfiguration::defaultConfiguration();
            configuration.setProtocol(QSsl::TlsV1_2OrLater);
            configuration.setPeerVerifyMode(QSslSocket::VerifyNone);
            client.setSslConfiguration(configuration);
            client.connectToHost(QHostAddress::LocalHost, server.serverPort());
            if (!pumpForConnected(&client, 5000)) {
                require(false, "reconnect attempt connected");
                break;
            }
            drivePlaintextThenUpgrade(&client, [&client] {
                client.waitForReadyRead(5000);
                client.readAll();
                client.startClientEncryption();
            });
            require(pumpForEncrypted(&client, 8000), attempt == 1 ? "first connection upgraded to TLS"
                                                               : "second connection upgraded to TLS again");
            client.disconnectFromHost();
            pumpForDisconnected(&client, 3000);
        }
        require(server.handshakesCompleted == 2, "both connections completed their own handshake");
    }

    // 3. A connection that never reaches the upgrade step starts no TLS and receives no product traffic.
    {
        TransitionServer server(certificate, key, Mode::NeverUpgrade);
        require(server.listen(QHostAddress::LocalHost, 0), "third listener started");
        QSslSocket client;
        client.connectToHost(QHostAddress::LocalHost, server.serverPort());
        if (pumpForConnected(&client, 5000)) {
            client.write(rfbBanner());
            client.write(QByteArray(1, char(kSecurityTypeVeNCrypt)));
            client.flush();
            client.waitForReadyRead(3000);
            client.abort();
        }
        require(server.handshakesCompleted == 0, "no handshake happened on a connection that never chose the seam");
    }

    // 4. Credentials that cannot be used produce no session: the client cannot reach an encrypted state, and the
    //    server's refusal is what stops the weaker plaintext service from continuing past the seam.
    {
        TransitionServer server(certificate, QSslKey(), Mode::EmptyKey);
        require(server.listen(QHostAddress::LocalHost, 0), "fourth listener started");
        QSslSocket client;
        QSslConfiguration configuration = QSslConfiguration::defaultConfiguration();
        configuration.setProtocol(QSsl::TlsV1_2OrLater);
        configuration.setPeerVerifyMode(QSslSocket::VerifyNone);
        client.setSslConfiguration(configuration);
        client.connectToHost(QHostAddress::LocalHost, server.serverPort());
        if (pumpForConnected(&client, 5000)) {
            drivePlaintextThenUpgrade(&client, [&client] {
                client.waitForReadyRead(3000);
                client.readAll();
                client.startClientEncryption();
            });
            require(!pumpForEncrypted(&client, 4000),
                    "with unusable credentials no TLS session can be established at all");
        }
        require(server.refusedToStartEncryption, "the server refused to start encryption with an empty key");
        require(server.handshakesCompleted == 0, "no session was reported as established");
    }

    std::printf("SPIKE_RESULT=%s\n", failures == 0 ? "PASS" : "FAIL");
    std::printf("SPIKE_FAILURES=%d\n", failures);
    return failures == 0 ? 0 : 1;
}
