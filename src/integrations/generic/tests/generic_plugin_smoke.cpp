#include <QCoreApplication>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QHostAddress>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSizeF>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <thread>

namespace {

std::uint32_t readU32(const QByteArray &data, qsizetype offset)
{
    const auto byteAt = [&data](qsizetype index) {
        return static_cast<std::uint32_t>(static_cast<unsigned char>(data.at(index)));
    };
    return (byteAt(offset) << 24U) | (byteAt(offset + 1) << 16U) | (byteAt(offset + 2) << 8U)
           | byteAt(offset + 3);
}

void appendU16(QByteArray &data, std::uint16_t value)
{
    data.append(static_cast<char>((value >> 8U) & 0xffU));
    data.append(static_cast<char>(value & 0xffU));
}

QByteArray readExact(QTcpSocket &socket, qsizetype size, int timeoutMs = 5000)
{
    QByteArray result;
    QElapsedTimer timer;
    timer.start();
    while (result.size() < size && timer.elapsed() < timeoutMs) {
        if (socket.bytesAvailable() == 0) {
            const int remaining = std::max(1, timeoutMs - static_cast<int>(timer.elapsed()));
            if (!socket.waitForReadyRead(remaining))
                break;
        }
        result += socket.read(size - result.size());
    }
    return result;
}

bool writeAll(QTcpSocket &socket, const QByteArray &data)
{
    if (socket.write(data) != data.size())
        return false;
    socket.flush();
    return socket.waitForBytesWritten(3000) || socket.bytesToWrite() == 0;
}

bool connectAndHandshake(QTcpSocket &socket, quint16 port)
{
    for (int attempt = 0; attempt < 60; ++attempt) {
        socket.abort();
        socket.connectToHost(QHostAddress::LocalHost, port);
        if (socket.waitForConnected(50))
            break;
        QThread::msleep(50);
    }
    if (socket.state() != QAbstractSocket::ConnectedState)
        return false;

    if (readExact(socket, 12) != QByteArray("RFB 003.008\n", 12))
        return false;
    if (!writeAll(socket, QByteArray("RFB 003.008\n", 12)))
        return false;

    const QByteArray securityCount = readExact(socket, 1);
    if (securityCount.size() != 1)
        return false;
    const int count = static_cast<unsigned char>(securityCount.at(0));
    const QByteArray securityTypes = readExact(socket, count);
    if (securityTypes.size() != count || !securityTypes.contains(char(1)))
        return false;
    if (!writeAll(socket, QByteArray(1, char(1))))
        return false;

    const QByteArray securityResult = readExact(socket, 4);
    if (securityResult.size() != 4 || readU32(securityResult, 0) != 0U)
        return false;

    if (!writeAll(socket, QByteArray(1, char(1))))
        return false;
    const QByteArray serverInit = readExact(socket, 24);
    if (serverInit.size() != 24)
        return false;
    const std::uint32_t nameLength = readU32(serverInit, 20);
    return nameLength <= 1024U
           && readExact(socket, static_cast<qsizetype>(nameLength)).size()
                  == static_cast<qsizetype>(nameLength);
}

bool sendPointer(QTcpSocket &socket, std::uint8_t mask, std::uint16_t x, std::uint16_t y)
{
    QByteArray message;
    message.append(char(5));
    message.append(static_cast<char>(mask));
    appendU16(message, x);
    appendU16(message, y);
    return writeAll(socket, message);
}

class ClickItem final : public QQuickItem
{
public:
    explicit ClickItem(QQuickItem *parent = nullptr)
        : QQuickItem(parent)
    {
        setAcceptedMouseButtons(Qt::LeftButton);
    }

    bool clicked() const noexcept { return m_clicked; }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton) {
            event->ignore();
            return;
        }
        m_pressed = true;
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (!m_pressed || event->button() != Qt::LeftButton) {
            event->ignore();
            return;
        }
        m_pressed = false;
        m_clicked = true;
        event->accept();
        QCoreApplication::quit();
    }

private:
    bool m_pressed = false;
    bool m_clicked = false;
};

}  // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    if (QGuiApplication::platformName() != QStringLiteral("offscreen")) {
        std::fprintf(stderr,
                     "FAIL: Generic Plugin changed native platform identity to '%s'\n",
                     qPrintable(QGuiApplication::platformName()));
        return 2;
    }

    QQuickWindow window;
    window.resize(320, 200);
    window.setTitle(QStringLiteral("HyRemote Generic plugin smoke"));
    ClickItem target(window.contentItem());
    target.setSize(QSizeF(320, 200));
    window.show();

    constexpr quint16 port = 5992;
    std::atomic<bool> clientOk{false};
    std::thread viewer([&clientOk] {
        QTcpSocket socket;
        if (!connectAndHandshake(socket, port))
            return;
        if (!sendPointer(socket, 0x00U, 160, 100))
            return;
        if (!sendPointer(socket, 0x01U, 160, 100))
            return;
        if (!sendPointer(socket, 0x00U, 160, 100))
            return;
        clientOk.store(true, std::memory_order_release);
        socket.disconnectFromHost();
        if (socket.state() != QAbstractSocket::UnconnectedState)
            socket.waitForDisconnected(1000);
    });

    QTimer::singleShot(7000, &app, &QCoreApplication::quit);
    app.exec();
    viewer.join();

    if (!clientOk.load(std::memory_order_acquire)) {
        std::fprintf(stderr, "FAIL: Generic Plugin RFB client did not complete the input sequence\n");
        return 3;
    }
    if (!target.clicked()) {
        std::fprintf(stderr, "FAIL: Generic Quick target did not receive the remote click\n");
        return 4;
    }

    std::printf("PASS: Generic Plugin preserved native QPA and delivered remote Quick input\n");
    return 0;
}
