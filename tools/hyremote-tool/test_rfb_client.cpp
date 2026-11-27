#include "rfb_client.h"
#include "tool_diagnostic_session.hpp"

#include <QColor>
#include <QCoreApplication>
#include <QEventLoop>
#include <QHostAddress>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include <functional>
#include <iostream>

namespace {

int failures = 0;

#define CHECK(condition)                                                                 \
    do {                                                                                 \
        if (!(condition)) {                                                              \
            std::cerr << __FILE__ << ':' << __LINE__ << ": failed " #condition << '\n';  \
            ++failures;                                                                  \
        }                                                                                \
    } while (false)

void appendU16(QByteArray &data, quint16 value)
{
    data.append(static_cast<char>((value >> 8U) & 0xffU));
    data.append(static_cast<char>(value & 0xffU));
}

void appendS32(QByteArray &data, qint32 value)
{
    const quint32 raw = static_cast<quint32>(value);
    data.append(static_cast<char>((raw >> 24U) & 0xffU));
    data.append(static_cast<char>((raw >> 16U) & 0xffU));
    data.append(static_cast<char>((raw >> 8U) & 0xffU));
    data.append(static_cast<char>(raw & 0xffU));
}

QByteArray rawPixels(int width, int height)
{
    QByteArray pixels;
    pixels.reserve(width * height * 4);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            pixels.append(static_cast<char>(0x10 + x));
            pixels.append(static_cast<char>(0x20 + y));
            pixels.append(char(0x30));
            pixels.append(char(0));
        }
    }
    return pixels;
}

QByteArray framebufferMessage(int width, int height, bool desktopSizeFirst = false)
{
    QByteArray message;
    message.append(char(0));
    message.append(char(0));
    appendU16(message, desktopSizeFirst ? 2 : 1);
    if (desktopSizeFirst) {
        appendU16(message, 0);
        appendU16(message, 0);
        appendU16(message, static_cast<quint16>(width));
        appendU16(message, static_cast<quint16>(height));
        appendS32(message, -223);
    }
    appendU16(message, 0);
    appendU16(message, 0);
    appendU16(message, static_cast<quint16>(width));
    appendU16(message, static_cast<quint16>(height));
    appendS32(message, 0);
    message += rawPixels(width, height);
    return message;
}

QByteArray cursorMessage()
{
    QByteArray message;
    message.append(char(0));
    message.append(char(0));
    appendU16(message, 1);
    appendU16(message, 1);  // hotspot x
    appendU16(message, 1);  // hotspot y
    appendU16(message, 4);
    appendU16(message, 4);
    appendS32(message, -239);
    for (int i = 0; i < 4 * 4; ++i) {
        message.append(char(255));
        message.append(char(255));
        message.append(char(255));
        message.append(char(0));
    }
    for (int row = 0; row < 4; ++row)
        message.append(char(0xf0));  // four opaque pixels
    return message;
}

class FakeRfbServer final : public QObject
{
public:
    explicit FakeRfbServer(int width = 4, int height = 2, quint8 bitsPerPixel = 32,
                           QByteArray serverName = QByteArray("fake-rfb"))
        : m_width(width)
        , m_height(height)
        , m_bitsPerPixel(bitsPerPixel)
        , m_serverName(serverName)
    {
    }

    std::function<void()> onInputComplete;

    bool listen()
    {
        if (!m_server.listen(QHostAddress::LocalHost, 0))
            return false;
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            m_socket = m_server.nextPendingConnection();
            if (!m_socket)
                return;
            connect(m_socket, &QTcpSocket::readyRead, this, [this] { readClient(); });
            m_socket->write("RFB 003.008\n", 12);
            m_socket->flush();
        });
        return true;
    }

    quint16 port() const { return m_server.serverPort(); }
    bool sawPointer() const { return m_pointer; }
    bool sawKey() const { return m_key; }

private:
    void readClient()
    {
        if (!m_socket)
            return;
        m_in += m_socket->readAll();
        while (true) {
            if (m_phase == Phase::Version) {
                if (m_in.size() < 12)
                    return;
                m_in.remove(0, 12);
                const QByteArray security = QByteArray::fromHex("0101");
                m_socket->write(security);
                m_socket->flush();
                m_phase = Phase::Security;
                continue;
            }
            if (m_phase == Phase::Security) {
                if (m_in.size() < 1)
                    return;
                m_in.remove(0, 1);
                m_socket->write(QByteArray::fromHex("00000000"));
                m_socket->flush();
                m_phase = Phase::ClientInit;
                continue;
            }
            if (m_phase == Phase::ClientInit) {
                if (m_in.size() < 1)
                    return;
                m_in.remove(0, 1);
                sendServerInit();
                m_phase = Phase::Streaming;
                QTimer::singleShot(10, this, [this] {
                    if (m_socket)
                        m_socket->write(cursorMessage());
                });
                QTimer::singleShot(20, this, [this] {
                    if (m_socket)
                        m_socket->write(framebufferMessage(4, 2));
                });
                QTimer::singleShot(80, this, [this] {
                    if (m_socket)
                        m_socket->write(framebufferMessage(8, 4, true));
                });
                continue;
            }

            if (m_phase != Phase::Streaming || m_in.isEmpty())
                return;
            const int type = static_cast<unsigned char>(m_in.at(0));
            qsizetype needed = 0;
            if (type == 2) {
                if (m_in.size() < 4)
                    return;
                const int count = (static_cast<unsigned char>(m_in.at(2)) << 8)
                                  | static_cast<unsigned char>(m_in.at(3));
                needed = 4 + count * 4;
            } else if (type == 3)
                needed = 10;
            else if (type == 5)
                needed = 6;
            else if (type == 4)
                needed = 8;
            else
                return;
            if (m_in.size() < needed)
                return;
            const QByteArray message = m_in.left(needed);
            m_in.remove(0, needed);
            if (type == 5) {
                m_pointer = true;
                CHECK(static_cast<unsigned char>(message.at(1)) == 1);
            } else if (type == 4) {
                m_key = true;
                CHECK(static_cast<unsigned char>(message.at(1)) == 1);
            }
            if (m_pointer && m_key && !m_inputReported) {
                m_inputReported = true;
                if (onInputComplete)
                    onInputComplete();
            }
        }
    }

    void sendServerInit()
    {
        QByteArray init;
        appendU16(init, static_cast<quint16>(m_width));
        appendU16(init, static_cast<quint16>(m_height));
        init.append(char(m_bitsPerPixel));  // bits per pixel
        init.append(char(m_bitsPerPixel == 32 ? 24 : 16));  // depth
        init.append(char(0));   // little endian
        init.append(char(1));   // true colour
        appendU16(init, 255);
        appendU16(init, 255);
        appendU16(init, 255);
        init.append(char(0));   // red shift
        init.append(char(8));   // green shift
        init.append(char(16));  // blue shift
        init.append("\0\0\0", 3);
        appendS32(init, m_serverName.size());
        init += m_serverName;
        m_socket->write(init);
        m_socket->flush();
    }

    enum class Phase { Version, Security, ClientInit, Streaming };

    QTcpServer m_server;
    QTcpSocket *m_socket = nullptr;
    QByteArray m_in;
    Phase m_phase = Phase::Version;
    bool m_pointer = false;
    bool m_key = false;
    bool m_inputReported = false;
    int m_width = 4;
    int m_height = 2;
    quint8 m_bitsPerPixel = 32;
    QByteArray m_serverName;
};


// A malicious but protocol-shaped RFB peer. Its text is not a local error
// string and must never be replayed into the Tool's UI, stderr or reports.
class RejectingRfbServer final : public QObject
{
public:
    enum class Kind {
        SecurityTypes, SecurityResult, OversizedAuthReason,
        OversizedRefusalReason, OversizedServerName
    };

    explicit RejectingRfbServer(Kind kind) : m_kind(kind) {}

    bool listen()
    {
        if (!m_server.listen(QHostAddress::LocalHost, 0))
            return false;
        QObject::connect(&m_server, &QTcpServer::newConnection, this, [this] {
            m_socket = m_server.nextPendingConnection();
            if (!m_socket)
                return;
            QObject::connect(m_socket, &QTcpSocket::readyRead, this,
                             [this] { onReadyRead(); });
            m_socket->write("RFB 003.008\n", 12);
            m_socket->flush();
        });
        return true;
    }

    quint16 port() const { return m_server.serverPort(); }

private:
    void onReadyRead()
    {
        if (!m_socket)
            return;
        m_in += m_socket->readAll();
        if (m_stage == 0 && m_in.size() >= 12) {
            m_in.remove(0, 12); // Client RFB 3.8 version
            if (m_kind == Kind::SecurityTypes
                || m_kind == Kind::OversizedRefusalReason) {
                QByteArray refusal(1, char(0)); // SecurityTypes count=0
                if (m_kind == Kind::OversizedRefusalReason) {
                    appendS32(refusal, 4097);
                    // Keep socket open, proving no body wait for a bad length.
                } else {
                    appendS32(refusal, maliciousReason().size());
                    refusal += maliciousReason();
                }
                m_socket->write(refusal);
                m_socket->flush();
                m_stage = 2;
            } else {
                m_socket->write(QByteArray::fromHex("0101"));
                m_socket->flush();
                m_stage = 1;
            }
        }
        if (m_stage == 1 && m_in.size() >= 1) {
            m_in.remove(0, 1); // Client selects SecurityType None
            QByteArray failure = QByteArray::fromHex("00000001");
            if (m_kind == Kind::OversizedServerName) {
                m_socket->write(QByteArray::fromHex("00000000"));
                m_socket->flush();
                m_stage = 3;
                return; // wait for ClientInit, then send malformed ServerInit
            }
            if (m_kind == Kind::OversizedAuthReason) {
                appendS32(failure, 4097);
                // Deliberately keep the socket open without a body: the
                // client must fail on the oversized length alone.
            } else {
                appendS32(failure, maliciousReason().size());
                failure += maliciousReason();
            }
            m_socket->write(failure);
            m_socket->flush();
            m_stage = 2;
        }
        if (m_stage == 3 && m_in.size() >= 1) {
            m_in.remove(0, 1); // ClientInit: shared session
            QByteArray init;
            appendU16(init, 4);
            appendU16(init, 2);
            init.append(char(32)); // bits per pixel
            init.append(char(24)); // depth
            init.append(char(0));  // little endian
            init.append(char(1));  // true color
            appendU16(init, 255);
            appendU16(init, 255);
            appendU16(init, 255);
            init.append(char(0));
            init.append(char(8));
            init.append(char(16));
            init.append("\0\0\0", 3);
            appendS32(init, 4097); // illegal nameLength, body intentionally absent
            m_socket->write(init);
            m_socket->flush();
            m_stage = 2;
        }
    }

    static QByteArray maliciousReason()
    {
        QByteArray reason("secrets=remote-should-not-log");
        reason.append("\nTOOL_CLIENT_STATE Connected");
        reason.append(char(27));
        reason.append("[31m");
        return reason;
    }

    Kind m_kind;
    QTcpServer m_server;
    QTcpSocket *m_socket = nullptr;
    QByteArray m_in;
    int m_stage = 0;
};


void verifyServerNameIsNotControlGrant()
{
    const QByteArray names[] = {
        QByteArray("view-only"),
        QByteArray("control-enabled"),
        QByteArray("operator\nTOOL_CLIENT_STATE Connected\nSERVERNAME_PRIVATE_MARKER\x1b[31m")
    };
    for (const QByteArray &remoteName : names) {
        FakeRfbServer server(4, 2, 32, remoteName);
        if (!server.listen()) {
            CHECK(false && "fake metadata server could not listen");
            continue;
        }
        RfbClient client;
        QString seenName;
        bool connected = false;
        QEventLoop handshake;
        QObject::connect(&client, &RfbClient::serverNameChanged, &handshake,
                         [&](const QString &name) { seenName = name; });
        QObject::connect(&client, &RfbClient::stateChanged, &handshake,
                         [&](RfbClient::State state) {
                             if (state == RfbClient::State::Connected) {
                                 connected = true;
                                 handshake.quit();
                             }
                         });
        QTimer::singleShot(2000, &handshake, &QEventLoop::quit);
        client.connectToHost(QStringLiteral("127.0.0.1"), server.port());
        handshake.exec();
        CHECK(connected);
        CHECK(seenName == QString::fromUtf8(remoteName));
        CHECK(!client.remoteControlEnabled());

        client.sendPointer(1, 1, 1);
        client.sendKey(0x41, true);
        QEventLoop observe;
        QTimer::singleShot(100, &observe, &QEventLoop::quit);
        observe.exec();
        CHECK(!server.sawPointer());
        CHECK(!server.sawKey());

        bool inputDelivered = false;
        QEventLoop control;
        server.onInputComplete = [&] {
            inputDelivered = true;
            control.quit();
        };
        client.setRemoteControlEnabled(true); // explicit local request
        client.sendPointer(1, 1, 1);
        client.sendKey(0x41, true);
        QTimer::singleShot(2000, &control, &QEventLoop::quit);
        control.exec();
        CHECK(inputDelivered);
        CHECK(server.sawPointer() && server.sawKey());
        client.disconnectFromHost();
        CHECK(!client.remoteControlEnabled());
    }

    RfbClient recycled;
    recycled.setRemoteControlEnabled(true);
    recycled.disconnectFromHost();
    CHECK(!recycled.remoteControlEnabled());
    recycled.setRemoteControlEnabled(true);
    recycled.connectToHost(QStringLiteral("127.0.0.1"), 1);
    CHECK(!recycled.remoteControlEnabled());
    recycled.disconnectFromHost();
}

void verifyUntrustedRefusalReasons()
{
    struct Case {
        RejectingRfbServer::Kind kind;
        const char *expected;
    };
    const Case cases[] = {
        {RejectingRfbServer::Kind::SecurityTypes,
         "the remote endpoint refused the connection"},
        {RejectingRfbServer::Kind::SecurityResult,
         "remote authentication failed"},
        {RejectingRfbServer::Kind::OversizedAuthReason,
         "remote endpoint sent an oversized authentication reason"},
        {RejectingRfbServer::Kind::OversizedRefusalReason,
         "remote endpoint sent an oversized refusal reason"},
        {RejectingRfbServer::Kind::OversizedServerName,
         "remote endpoint sent an oversized server name"}
    };
    for (const auto &test : cases) {
        RejectingRfbServer server(test.kind);
        if (!server.listen()) {
            CHECK(false && "refusing fake server could not listen");
            continue;
        }
        RfbClient client;
        ToolDiagnosticSession evidence;
        evidence.beginAttempt(QStringLiteral("127.0.0.1"), server.port());
        QString reported;
        bool failed = false;
        QEventLoop loop;
        QObject::connect(&client, &RfbClient::errorOccurred, &loop,
                         [&](const QString &message) {
                             reported = message;
                             evidence.observeError(message);
                         });
        QObject::connect(&client, &RfbClient::socketErrorObserved, &loop,
                         [&](QAbstractSocket::SocketError error) {
                             evidence.observeSocketError(error);
                         });
        QObject::connect(&client, &RfbClient::stateChanged, &loop,
                         [&](RfbClient::State state) {
                             evidence.observeState(state);
                             if (state == RfbClient::State::Failed) {
                                 failed = true;
                                 loop.quit();
                             }
                         });
        QTimer::singleShot(2000, &loop, &QEventLoop::quit);
        client.connectToHost(QStringLiteral("127.0.0.1"), server.port());
        loop.exec();
        CHECK(failed);
        CHECK(client.state() == RfbClient::State::Failed);
        CHECK(reported == QString::fromLatin1(test.expected));
        CHECK(client.lastError() == reported);
        CHECK(!reported.contains(QStringLiteral("secrets=")));
        CHECK(!reported.contains(QStringLiteral("TOOL_CLIENT_STATE")));
        CHECK(!reported.contains(QLatin1Char('\n')));
        CHECK(!reported.contains(QChar(27)));
        const QByteArray report = evidence.previewJson();
        const auto diagnostic = QJsonDocument::fromJson(report).object();
        CHECK(diagnostic.value(QStringLiteral("state")).toString()
              == QStringLiteral("failed"));
        CHECK(diagnostic.value(QStringLiteral("endpoint")).toString()
              == QStringLiteral("[redacted]"));
        CHECK(!report.contains("secrets="));
        CHECK(!report.contains("TOOL_CLIENT_STATE"));
        if (test.kind == RejectingRfbServer::Kind::OversizedAuthReason
            || test.kind == RejectingRfbServer::Kind::OversizedRefusalReason
            || test.kind == RejectingRfbServer::Kind::OversizedServerName) {
            // True RFB 3.8 wire: an advertised 4097 length violates the
            // protocol field budget, regardless of which handshake stage.
            CHECK(diagnostic.value(QStringLiteral("lastFailure")).toString()
                  == QStringLiteral("malformed-wire"));
        } else if (test.kind == RejectingRfbServer::Kind::SecurityResult) {
            CHECK(diagnostic.value(QStringLiteral("lastFailure")).toString()
                  == QStringLiteral("security-mismatch"));
        }
    }
}

}  // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    CHECK(rfbWheelMaskForDelta(120) == 0x08);
    CHECK(rfbWheelMaskForDelta(-120) == 0x10);

    verifyServerNameIsNotControlGrant();
    verifyUntrustedRefusalReasons();

    {
        FakeRfbServer oversized(65535, 65535);
        if (!oversized.listen()) {
            std::cerr << "oversized fake server could not listen\n";
            return 1;
        }
        RfbClient guarded;
        bool failed = false;
        QEventLoop loop;
        QObject::connect(&guarded, &RfbClient::stateChanged, &loop,
                         [&](RfbClient::State state) {
                             if (state == RfbClient::State::Failed) {
                                 failed = true;
                                 loop.quit();
                             }
                         });
        QTimer::singleShot(2000, &loop, &QEventLoop::quit);
        guarded.connectToHost(QStringLiteral("127.0.0.1"), oversized.port());
        loop.exec();
        CHECK(failed);
    }

    {
        FakeRfbServer unsupportedPixelFormat(4, 2, 16);
        if (!unsupportedPixelFormat.listen()) {
            std::cerr << "unsupported-format fake server could not listen\n";
            return 1;
        }
        RfbClient guarded;
        bool failed = false;
        QEventLoop loop;
        QObject::connect(&guarded, &RfbClient::stateChanged, &loop,
                         [&](RfbClient::State state) {
                             if (state == RfbClient::State::Failed) {
                                 failed = true;
                                 loop.quit();
                             }
                         });
        QTimer::singleShot(2000, &loop, &QEventLoop::quit);
        guarded.connectToHost(QStringLiteral("127.0.0.1"), unsupportedPixelFormat.port());
        loop.exec();
        CHECK(failed);
    }

    FakeRfbServer server;
    if (!server.listen()) {
        std::cerr << "fake server could not listen\n";
        return 1;
    }

    RfbClient client;
    int frames = 0;
    bool cursorSeen = false;
    bool inputComplete = false;
    bool statsSeen = false;

    const auto maybeQuit = [&] {
        if (frames >= 2 && cursorSeen && inputComplete && statsSeen)
            QTimer::singleShot(50, &app, &QCoreApplication::quit);
    };

    QObject::connect(&client, &RfbClient::stateChanged, &app, [&](RfbClient::State state) {
        if (state == RfbClient::State::Connected) {
            client.setRemoteControlEnabled(true);
            client.sendPointer(1, 10, 10);
            client.sendKey(0x41, true);
        }
    });

    QObject::connect(&client, &RfbClient::frameUpdated, &app, [&](const QImage &image) {
        ++frames;
        if (frames == 1) {
            CHECK(image.size() == QSize(4, 2));
            CHECK(image.pixelColor(0, 0).rgb() == qRgb(0x10, 0x20, 0x30));
        } else if (frames == 2) {
            CHECK(image.size() == QSize(8, 4));
            CHECK(image.pixelColor(1, 1).rgb() == qRgb(0x11, 0x21, 0x30));
        }
        maybeQuit();
    });

    QObject::connect(&client, &RfbClient::cursorUpdated, &app, [&](const QImage &cursor, const QPoint &hotspot) {
        if (!cursor.isNull()) {
            CHECK(cursor.size() == QSize(4, 4));
            CHECK(hotspot == QPoint(1, 1));
            cursorSeen = true;
            maybeQuit();
        }
    });

    QObject::connect(&client, &RfbClient::statsUpdated, &app, [&](const RfbClient::Stats &stats) {
        if (stats.frames >= 2 && stats.bytes > 0) {
            statsSeen = true;
            maybeQuit();
        }
    });

    server.onInputComplete = [&] {
        inputComplete = true;
        maybeQuit();
    };

    QTimer::singleShot(5000, &app, [&] {
        std::cerr << "timeout: frames=" << frames << " input=" << inputComplete
                  << " stats=" << statsSeen << '\n';
        ++failures;
        app.quit();
    });

    client.connectToHost(QStringLiteral("127.0.0.1"), server.port());
    app.exec();

    CHECK(frames >= 2);
    CHECK(cursorSeen);
    CHECK(inputComplete);
    CHECK(statsSeen);
    CHECK(client.state() != RfbClient::State::Failed);
    return failures == 0 ? 0 : 1;
}
