// Native Qt6.8 OpenGL-RHI Loopback: actual QuickCaptureSource -> Core
// Session -> production RFB transport -> socket wire / raw pixels.
//
// No external Viewer or site hardware, no product delivery claim.
// Real OpenGL scene update is intentionally driven by application QQuick.
// This uses the existing requestFrame path; it is not an unsolicited push.
#include <QByteArray>
#include <QColor>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QGuiApplication>
#include <QHostAddress>
#include <QPainter>
#include <QQuickPaintedItem>
#include <QQuickWindow>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <vector>

#include "detail/component_factories.hpp"
#include "hyremote/core/session.hpp"
#include "transport/rfb_transport.hpp"

namespace {

int failed = 0;
#define REQUIRE(expr) do { if (!(expr)) { \
    ++failed; std::cerr << "R3_RFB_ERROR line=" << __LINE__ \
                        << " check=" #expr << '\n'; }} while (false)

class FourQuadrants final : public QQuickPaintedItem
{
public:
    explicit FourQuadrants(QQuickItem *parent) : QQuickPaintedItem(parent)
    {
        setAntialiasing(false);
    }

    static std::uint32_t rgb(int phase, int quadrant)
    {
        constexpr std::uint32_t colors[] = {
            0xe0222d, 0x1ec640, 0x2443da, 0xeccd18
        };
        return colors[(phase + quadrant) % 4];
    }

    void setPhase(int phase)
    {
        m_phase = phase;
        update();
    }

    void paint(QPainter *painter) override
    {
        const int phase = m_phase;
        const qreal halfW = width() / 2.0;
        const qreal halfH = height() / 2.0;
        for (int q = 0; q < 4; ++q) {
            const std::uint32_t color = rgb(phase, q);
            painter->fillRect(QRectF((q & 1) ? halfW : 0,
                                     (q & 2) ? halfH : 0,
                                     halfW, halfH),
                QColor(int((color >> 16U) & 0xffU),
                       int((color >> 8U) & 0xffU),
                       int(color & 0xffU)));
        }
    }
private:
    int m_phase = 0;
};

bool pumpUntil(const std::function<bool()> &predicate, int maxMs = 5000)
{
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < maxMs) {
        QEventLoop loop;
        QTimer::singleShot(10, &loop, &QEventLoop::quit);
        loop.exec(QEventLoop::AllEvents);
    }
    return predicate();
}

void u16(QByteArray &out, std::uint16_t value)
{
    out.append(char(value >> 8U));
    out.append(char(value & 255U));
}
void u32(QByteArray &out, std::uint32_t value)
{
    out.append(char(value >> 24U));
    out.append(char(value >> 16U));
    out.append(char(value >> 8U));
    out.append(char(value));
}
std::uint16_t be16(const QByteArray &v, int offset)
{
    return (std::uint16_t(std::uint8_t(v.at(offset))) << 8U)
        | std::uint16_t(std::uint8_t(v.at(offset + 1)));
}
std::uint32_t be32(const QByteArray &v, int offset)
{
    return (std::uint32_t(std::uint8_t(v.at(offset))) << 24U)
        | (std::uint32_t(std::uint8_t(v.at(offset + 1))) << 16U)
        | (std::uint32_t(std::uint8_t(v.at(offset + 2))) << 8U)
        | std::uint32_t(std::uint8_t(v.at(offset + 3)));
}

QByteArray readBytes(QTcpSocket &socket, qsizetype size, int deadlineMs = 6000)
{
    QByteArray data;
    QElapsedTimer timer;
    timer.start();
    while (data.size() < size && timer.elapsed() < deadlineMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        if (!socket.bytesAvailable()) {
            socket.waitForReadyRead(5);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        }
        data += socket.read(size - data.size());
    }
    return data;
}
bool writeBytes(QTcpSocket &socket, const QByteArray &data)
{
    return socket.write(data) == data.size() &&
           (socket.waitForBytesWritten(3000) || socket.bytesToWrite() == 0);
}

quint16 freeLocalPort()
{
    QTcpServer server;
    return server.listen(QHostAddress::LocalHost, 0) ? server.serverPort() : 0;
}

bool handshake(QTcpSocket &client, quint16 port, std::uint16_t *width,
               std::uint16_t *height)
{
    client.connectToHost(QHostAddress::LocalHost, port);
    if (!client.waitForConnected(5000) ||
        readBytes(client, 12) != QByteArray("RFB 003.008\n", 12))
        return false;
    if (!writeBytes(client, QByteArray("RFB 003.008\n", 12)))
        return false;
    const QByteArray number = readBytes(client, 1);
    if (number.size() != 1)
        return false;
    const QByteArray security = readBytes(client, std::uint8_t(number[0]));
    if (!security.contains(char(1)) ||
        !writeBytes(client, QByteArray(1, char(1))))
        return false;
    const QByteArray securityOutcome = readBytes(client, 4);
    if (securityOutcome.size() != 4 || be32(securityOutcome, 0) != 0 ||
        !writeBytes(client, QByteArray(1, char(1))))
        return false;
    const QByteArray init = readBytes(client, 24);
    if (init.size() != 24)
        return false;
    *width = be16(init, 0);
    *height = be16(init, 2);
    return readBytes(client, be32(init, 20)).size() ==
        qsizetype(be32(init, 20));
}

void chooseRaw(QTcpSocket &client)
{
    QByteArray enc;
    enc.append(char(2));
    enc.append(char(0));
    u16(enc, 1);
    u32(enc, 0); // RFB EncodingRaw
    REQUIRE(writeBytes(client, enc));
}
void requestUpdate(QTcpSocket &client, std::uint16_t width,
                   std::uint16_t height, bool incremental)
{
    QByteArray req;
    req.append(char(3));
    req.append(incremental ? char(1) : char(0));
    u16(req, 0);
    u16(req, 0);
    u16(req, width);
    u16(req, height);
    REQUIRE(writeBytes(client, req));
}

struct WireImage
{
    int width = 0, height = 0;
    std::vector<std::uint32_t> pixels;
    explicit WireImage(int w, int h) : width(w), height(h),
        pixels(static_cast<std::size_t>(w) * h, 0) {}
    std::uint32_t at(int x, int y) const
    {
        return pixels[static_cast<std::size_t>(y) * width + x];
    }
};

bool receiveRfbUpdate(QTcpSocket &client, WireImage &image)
{
    const QByteArray header = readBytes(client, 4);
    if (header.size() != 4 || header.at(0) != char(0))
        return false;
    const unsigned rectangles = be16(header, 2);
    if (!rectangles || rectangles > 500)
        return false;
    for (unsigned i = 0; i < rectangles; ++i) {
        const QByteArray rect = readBytes(client, 12);
        if (rect.size() != 12 || be32(rect, 8) != 0)
            return false;
        const int x = be16(rect, 0), y = be16(rect, 2);
        const int w = be16(rect, 4), h = be16(rect, 6);
        if (w <= 0 || h <= 0 || x + w > image.width || y + h > image.height)
            return false;
        const QByteArray pixels = readBytes(client, qsizetype(w) * h * 4);
        if (pixels.size() != qsizetype(w) * h * 4)
            return false;
        qsizetype at = 0;
        for (int row = 0; row < h; ++row) {
            for (int col = 0; col < w; ++col, at += 4) {
                const auto red = std::uint32_t(std::uint8_t(pixels[at]));
                const auto green = std::uint32_t(std::uint8_t(pixels[at + 1]));
                const auto blue = std::uint32_t(std::uint8_t(pixels[at + 2]));
                image.pixels[static_cast<std::size_t>(y + row) * image.width + x + col] =
                    (red << 16U) | (green << 8U) | blue;
            }
        }
    }
    return true;
}

bool matchesQuadrants(const WireImage &image, int phase)
{
    for (int q = 0; q < 4; ++q) {
        const int x = (q & 1) ? image.width * 3 / 4 : image.width / 4;
        const int y = (q & 2) ? image.height * 3 / 4 : image.height / 4;
        const std::uint32_t actual = image.at(x, y);
        if (actual != FourQuadrants::rgb(phase, q)) {
            std::cerr << "R3_RFB_MISMATCH phase=" << phase
                      << " quadrant=" << q << " received=0x"
                      << std::hex << actual << " expected=0x"
                      << FourQuadrants::rgb(phase, q) << std::dec << '\n';
            return false;
        }
    }
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    constexpr int width = 320, height = 180;
    QQuickWindow window;
    window.resize(width, height);
    FourQuadrants source(window.contentItem());
    source.setSize(QSizeF(width, height));
    window.show();

    // The real production transport is used, including its RFB exact update
    // protocol and canonical RGBA storage mapping. No mocked encoder.
    auto components = HyRemote::detail::createTargetComponents(&window, false);
    REQUIRE(components.supported && components.capture);
    if (!components.capture)
        return 10;
    hyremote::Session session;
    REQUIRE(session.setCaptureSource(std::move(components.capture)));
    const quint16 port = freeLocalPort();
    REQUIRE(port);
    REQUIRE(session.setTransport(HyRemote::detail::createRfbTransport(
        QHostAddress::LocalHost, port, HyRemote::detail::RfbSecurityConfig{})));
    REQUIRE(session.start());
    if (!pumpUntil([&] { return session.stats().framesDispatched > 0; })) {
        std::cerr << "R3_RFB=BLOCKED_NO_INITIAL_CORE_FRAME\n";
        session.stop();
        return 12;
    }

    QTcpSocket client;
    std::uint16_t actualW = 0, actualH = 0;
    if (!handshake(client, port, &actualW, &actualH)) {
        std::cerr << "R3_RFB=BLOCKED_RFB_HANDSHAKE\n";
        session.stop();
        return 13;
    }
    REQUIRE(actualW == width && actualH == height);
    chooseRaw(client);
    WireImage visible(actualW, actualH);
    requestUpdate(client, actualW, actualH, false);
    REQUIRE(receiveRfbUpdate(client, visible));
    REQUIRE(matchesQuadrants(visible, 0));

    // Change the source BEFORE issuing incremental update. The real
    // scheduler/transport must produce a new capture rather than reuse the
    // old RGBA storage under a new frame ID or return only a false delta.
    for (int phase = 1; phase <= 4; ++phase) {
        source.setPhase(phase);
        requestUpdate(client, actualW, actualH, true);
        REQUIRE(receiveRfbUpdate(client, visible));
        REQUIRE(matchesQuadrants(visible, phase));
    }

    client.disconnectFromHost();
    client.waitForDisconnected(2000);
    session.stop();
    REQUIRE(session.state() == hyremote::SessionState::Stopped);
    if (failed) {
        std::cerr << "R3_RFB=FAIL checks=" << failed << '\n';
        return 20;
    }
    std::cout << "R3_RFB_LOOPBACK=PASS_SCOPED"
              << " SCENE_STAGES=5"
              << " QUADRANTS=4"
              << " PORT=ephemeral"
              << " CORE_TO_RAW_RFB=WIRE_BYTES_VERIFIED\n";
    return 0;
}
