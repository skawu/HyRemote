#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QDialog>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHostAddress>
#include <QTcpSocket>
#include <QTimer>
#include <QWidget>

#include <cstdint>
#include <iostream>

namespace {

constexpr quint16 kPort = 5997;
constexpr int kTimeoutMs = 5000;
constexpr std::int32_t kEncodingRaw = 0;
constexpr std::int32_t kEncodingDesktopSize = -223;

std::uint8_t byteAt(const QByteArray &data, qsizetype offset)
{
    return static_cast<std::uint8_t>(static_cast<unsigned char>(data.at(offset)));
}

std::uint16_t readU16(const QByteArray &data, qsizetype offset)
{
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(byteAt(data, offset)) << 8U)
                                      | byteAt(data, offset + 1));
}

std::uint32_t readU32(const QByteArray &data, qsizetype offset)
{
    return (static_cast<std::uint32_t>(byteAt(data, offset)) << 24U)
           | (static_cast<std::uint32_t>(byteAt(data, offset + 1)) << 16U)
           | (static_cast<std::uint32_t>(byteAt(data, offset + 2)) << 8U)
           | static_cast<std::uint32_t>(byteAt(data, offset + 3));
}

std::int32_t readS32(const QByteArray &data, qsizetype offset)
{
    return static_cast<std::int32_t>(readU32(data, offset));
}

void appendU16(QByteArray &data, std::uint16_t value)
{
    data.append(static_cast<char>((value >> 8U) & 0xffU));
    data.append(static_cast<char>(value & 0xffU));
}

void appendU32(QByteArray &data, std::uint32_t value)
{
    data.append(static_cast<char>((value >> 24U) & 0xffU));
    data.append(static_cast<char>((value >> 16U) & 0xffU));
    data.append(static_cast<char>((value >> 8U) & 0xffU));
    data.append(static_cast<char>(value & 0xffU));
}

void appendS32(QByteArray &data, std::int32_t value)
{
    appendU32(data, static_cast<std::uint32_t>(value));
}

void pumpEvents(int milliseconds)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < milliseconds)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
}

bool waitForConnected(QTcpSocket &socket, int timeoutMs = kTimeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (socket.state() != QAbstractSocket::ConnectedState && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        socket.waitForConnected(20);
    }
    return socket.state() == QAbstractSocket::ConnectedState;
}

QByteArray readExact(QTcpSocket &socket, qsizetype count, int timeoutMs = kTimeoutMs)
{
    QByteArray result;
    QElapsedTimer timer;
    timer.start();
    while (result.size() < count && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        const qsizetype needed = count - result.size();
        if (socket.bytesAvailable() > 0)
            result += socket.read(needed);
        else
            socket.waitForReadyRead(20);
        if (socket.state() == QAbstractSocket::UnconnectedState && socket.bytesAvailable() == 0)
            break;
    }
    return result;
}

bool writeMessage(QTcpSocket &socket, const QByteArray &message)
{
    if (socket.state() != QAbstractSocket::ConnectedState)
        return false;
    if (socket.write(message) != message.size())
        return false;
    socket.flush();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return socket.state() == QAbstractSocket::ConnectedState;
}

struct FramebufferGeometry
{
    std::uint16_t width = 0;
    std::uint16_t height = 0;
};

bool completeHandshake(QTcpSocket &socket, FramebufferGeometry &geometry)
{
    socket.connectToHost(QHostAddress::LocalHost, kPort);
    if (!waitForConnected(socket))
        return false;

    const QByteArray serverVersion = readExact(socket, 12);
    if (serverVersion != QByteArray("RFB 003.008\n", 12))
        return false;
    if (!writeMessage(socket, QByteArray("RFB 003.008\n", 12)))
        return false;

    const QByteArray securityTypes = readExact(socket, 2);
    if (securityTypes.size() != 2 || byteAt(securityTypes, 0) != 1 || byteAt(securityTypes, 1) != 1)
        return false;
    if (!writeMessage(socket, QByteArray(1, char(1))))
        return false;

    const QByteArray securityResult = readExact(socket, 4);
    if (securityResult.size() != 4 || readU32(securityResult, 0) != 0)
        return false;

    if (!writeMessage(socket, QByteArray(1, char(1))))  // ClientInit: shared
        return false;

    const QByteArray init = readExact(socket, 24);
    if (init.size() != 24)
        return false;
    geometry.width = readU16(init, 0);
    geometry.height = readU16(init, 2);
    const std::uint32_t nameLength = readU32(init, 20);
    if (geometry.width == 0 || geometry.height == 0 || nameLength > 1024)
        return false;
    const QByteArray name = readExact(socket, static_cast<qsizetype>(nameLength));
    if (name.size() != static_cast<qsizetype>(nameLength))
        return false;

    QByteArray encodings;
    encodings.append(char(2));
    encodings.append(char(0));
    appendU16(encodings, 2);
    appendS32(encodings, kEncodingRaw);
    appendS32(encodings, kEncodingDesktopSize);
    return writeMessage(socket, encodings);
}

bool requestFramebuffer(QTcpSocket &socket, const FramebufferGeometry &known)
{
    QByteArray request;
    request.append(char(3));
    request.append(char(0));  // full update
    appendU16(request, 0);
    appendU16(request, 0);
    appendU16(request, known.width);
    appendU16(request, known.height);
    return writeMessage(socket, request);
}

bool readFramebufferUpdate(QTcpSocket &socket, FramebufferGeometry &geometry)
{
    const QByteArray header = readExact(socket, 4);
    if (header.size() != 4 || byteAt(header, 0) != 0)
        return false;

    const std::uint16_t rectangleCount = readU16(header, 2);
    if (rectangleCount == 0 || rectangleCount > 4)
        return false;

    FramebufferGeometry announced = geometry;
    for (std::uint16_t i = 0; i < rectangleCount; ++i) {
        const QByteArray rect = readExact(socket, 12);
        if (rect.size() != 12)
            return false;
        const std::uint16_t width = readU16(rect, 4);
        const std::uint16_t height = readU16(rect, 6);
        const std::int32_t encoding = readS32(rect, 8);

        if (encoding == kEncodingDesktopSize) {
            announced.width = width;
            announced.height = height;
            continue;
        }
        if (encoding != kEncodingRaw)
            return false;

        const std::uint64_t bytes = static_cast<std::uint64_t>(width) * height * 4U;
        if (bytes > 16U * 1024U * 1024U)
            return false;
        const QByteArray pixels = readExact(socket, static_cast<qsizetype>(bytes));
        if (pixels.size() != static_cast<qsizetype>(bytes))
            return false;
    }

    geometry = announced;
    return socket.state() == QAbstractSocket::ConnectedState;
}

bool requestAndRead(QTcpSocket &socket, FramebufferGeometry &geometry)
{
    return requestFramebuffer(socket, geometry) && readFramebufferUpdate(socket, geometry);
}

}  // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    QWidget primary;
    primary.setWindowTitle(QStringLiteral("HyRemote QPA multi-surface primary"));
    primary.resize(320, 180);
    primary.move(100, 100);
    primary.show();

    QDialog dialog;
    dialog.setWindowTitle(QStringLiteral("HyRemote QPA independent dialog"));
    dialog.resize(160, 100);
    dialog.move(470, 100);

    int result = 1;
    QTimer::singleShot(350, &app, [&] {
        QTcpSocket viewer;
        FramebufferGeometry geometry;
        if (!completeHandshake(viewer, geometry) || !requestAndRead(viewer, geometry)) {
            std::cerr << "FAIL: could not establish initial RFB viewer session\n";
            app.quit();
            return;
        }
        const FramebufferGeometry primaryGeometry = geometry;

        dialog.show();
        dialog.raise();
        dialog.activateWindow();
        pumpEvents(350);

        if (!requestAndRead(viewer, geometry)) {
            std::cerr << "FAIL: existing viewer connection did not survive dialog creation\n";
            app.quit();
            return;
        }
        if (geometry.width <= primaryGeometry.width) {
            std::cerr << "FAIL: independent dialog did not expand the composite application canvas\n";
            app.quit();
            return;
        }

        primary.hide();
        pumpEvents(350);
        if (!requestAndRead(viewer, geometry)) {
            std::cerr << "FAIL: existing viewer connection did not survive primary-window hide\n";
            app.quit();
            return;
        }
        if (viewer.state() != QAbstractSocket::ConnectedState || geometry.width == 0 || geometry.height == 0) {
            std::cerr << "FAIL: dialog-only application surface lost the live viewer session\n";
            app.quit();
            return;
        }

        primary.show();
        primary.raise();
        primary.activateWindow();
        dialog.hide();
        pumpEvents(350);
        if (!requestAndRead(viewer, geometry)) {
            std::cerr << "FAIL: existing viewer connection did not survive surface replacement\n";
            app.quit();
            return;
        }

        viewer.disconnectFromHost();
        std::cout << "PASS: one RFB viewer connection survived primary/dialog surface churn\n";
        result = 0;
        app.quit();
    });

    QTimer::singleShot(15000, &app, [&] {
        std::cerr << "FAIL: QPA multi-surface connection smoke timed out\n";
        app.quit();
    });

    app.exec();
    return result;
}
