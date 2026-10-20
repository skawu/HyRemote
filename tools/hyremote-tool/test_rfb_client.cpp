#include "rfb_client.h"

#include <QColor>
#include <QCoreApplication>
#include <QHostAddress>
#include <QImage>
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

class FakeRfbServer final : public QObject
{
public:
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
            if (type == 2)
                needed = 12;
            else if (type == 3)
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
        appendU16(init, 4);
        appendU16(init, 2);
        init.append(char(32));  // bits per pixel
        init.append(char(24));  // depth
        init.append(char(0));   // little endian
        init.append(char(1));   // true colour
        appendU16(init, 255);
        appendU16(init, 255);
        appendU16(init, 255);
        init.append(char(0));   // red shift
        init.append(char(8));   // green shift
        init.append(char(16));  // blue shift
        init.append("\0\0\0", 3);
        const QByteArray name("fake-rfb");
        appendS32(init, name.size());
        init += name;
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
};

}  // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    FakeRfbServer server;
    if (!server.listen()) {
        std::cerr << "fake server could not listen\n";
        return 1;
    }

    RfbClient client;
    int frames = 0;
    bool inputComplete = false;
    bool statsSeen = false;

    const auto maybeQuit = [&] {
        if (frames >= 2 && inputComplete && statsSeen)
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
    CHECK(inputComplete);
    CHECK(statsSeen);
    CHECK(client.state() != RfbClient::State::Failed);
    return failures == 0 ? 0 : 1;
}
