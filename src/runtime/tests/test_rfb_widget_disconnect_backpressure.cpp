#include <QApplication>
#include <QByteArray>
#include <QColor>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHostAddress>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPalette>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>
#include <QWidget>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <vector>

#include "detail/component_factories.hpp"
#include "hyremote/core/capture_source.hpp"
#include "hyremote/core/frame.hpp"
#include "hyremote/core/session.hpp"
#include "hyremote/core/storage.hpp"
#include "rfb_interaction_latency_stats.hpp"
#include "transport/rfb_transport.hpp"

namespace {

int failures = 0;

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: " #expr << '\n';       \
            ++failures;                                                                            \
        }                                                                                          \
    } while (false)

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

std::uint16_t readU16(const QByteArray &data, qsizetype offset)
{
    const auto byteAt = [&data](qsizetype index) {
        return static_cast<std::uint16_t>(static_cast<unsigned char>(data.at(index)));
    };
    return static_cast<std::uint16_t>((byteAt(offset) << 8U) | byteAt(offset + 1));
}

std::uint32_t readU32(const QByteArray &data, qsizetype offset)
{
    const auto byteAt = [&data](qsizetype index) {
        return static_cast<std::uint32_t>(static_cast<unsigned char>(data.at(index)));
    };
    return (byteAt(offset) << 24U) | (byteAt(offset + 1) << 16U) | (byteAt(offset + 2) << 8U)
           | byteAt(offset + 3);
}

QByteArray readExact(QTcpSocket &socket, qsizetype size, int timeoutMs = 3000)
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

QByteArray readExactPumpingEvents(QTcpSocket &socket, qsizetype size, int timeoutMs = 3000)
{
    QByteArray result;
    QElapsedTimer timer;
    timer.start();
    while (result.size() < size && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        if (socket.bytesAvailable() == 0) {
            const int remaining = std::max(1, timeoutMs - static_cast<int>(timer.elapsed()));
            socket.waitForReadyRead(std::min(5, remaining));
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
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

quint16 freePort()
{
    QTcpServer probe;
    if (!probe.listen(QHostAddress::LocalHost, 0))
        return 0;
    return probe.serverPort();
}

bool connectRawRfb(QTcpSocket &socket,
                   quint16 port,
                   std::uint16_t *framebufferWidth = nullptr,
                   std::uint16_t *framebufferHeight = nullptr)
{
    socket.connectToHost(QHostAddress::LocalHost, port);
    if (!socket.waitForConnected(3000))
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
    if (framebufferWidth)
        *framebufferWidth = readU16(serverInit, 0);
    if (framebufferHeight)
        *framebufferHeight = readU16(serverInit, 2);
    const std::uint32_t nameLength = readU32(serverInit, 20);
    return readExact(socket, static_cast<qsizetype>(nameLength)).size()
           == static_cast<qsizetype>(nameLength);
}

bool sendKey(QTcpSocket &socket, std::uint32_t keysym, bool pressed)
{
    QByteArray message;
    message.append(char(4));
    message.append(pressed ? char(1) : char(0));
    appendU16(message, 0);
    appendU32(message, keysym);
    return writeAll(socket, message);
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

bool sendSetEncodingsRaw(QTcpSocket &socket)
{
    QByteArray message;
    message.append(char(2));
    message.append(char(0));
    appendU16(message, 1);
    appendU32(message, 0);
    return writeAll(socket, message);
}

bool sendUpdateRequest(QTcpSocket &socket,
                       bool incremental,
                       std::uint16_t width,
                       std::uint16_t height)
{
    QByteArray message;
    message.append(char(3));
    message.append(incremental ? char(1) : char(0));
    appendU16(message, 0);
    appendU16(message, 0);
    appendU16(message, width);
    appendU16(message, height);
    return writeAll(socket, message);
}

bool pumpUntil(const std::function<bool()> &predicate, int attempts = 100)
{
    for (int i = 0; i < attempts; ++i) {
        if (predicate())
            return true;
        QEventLoop loop;
        QTimer::singleShot(5, &loop, &QEventLoop::quit);
        loop.exec(QEventLoop::AllEvents);
    }
    return predicate();
}

bool waitStats(hyremote::Session &session,
               const std::function<bool(const hyremote::SessionStats &)> &predicate,
               int timeoutMs = 3000)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (predicate(session.stats()))
            return true;
        QThread::msleep(10);
    }
    return predicate(session.stats());
}

struct RawFramebuffer
{
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::vector<std::uint32_t> pixels;

    void resize(std::uint16_t newWidth, std::uint16_t newHeight)
    {
        width = newWidth;
        height = newHeight;
        pixels.assign(static_cast<std::size_t>(width) * height, 0U);
    }

    void set(std::uint16_t x, std::uint16_t y, std::uint32_t rgb)
    {
        if (x < width && y < height)
            pixels[static_cast<std::size_t>(y) * width + x] = rgb;
    }

    std::uint32_t get(std::uint16_t x, std::uint16_t y) const
    {
        if (x >= width || y >= height)
            return 0U;
        return pixels[static_cast<std::size_t>(y) * width + x];
    }
};

bool readRawFramebufferUpdatePumping(QTcpSocket &socket, RawFramebuffer &frame)
{
    const QByteArray header = readExactPumpingEvents(socket, 4);
    if (header.size() != 4 || header.at(0) != char(0))
        return false;

    const std::uint16_t rectangleCount = readU16(header, 2);
    if (rectangleCount == 0)
        return false;
    for (std::uint16_t index = 0; index < rectangleCount; ++index) {
        const QByteArray rectangle = readExactPumpingEvents(socket, 12);
        if (rectangle.size() != 12)
            return false;
        const std::uint16_t x = readU16(rectangle, 0);
        const std::uint16_t y = readU16(rectangle, 2);
        const std::uint16_t width = readU16(rectangle, 4);
        const std::uint16_t height = readU16(rectangle, 6);
        const std::int32_t encoding = static_cast<std::int32_t>(readU32(rectangle, 8));
        if (encoding != 0)
            return false;

        const qsizetype bytes = static_cast<qsizetype>(width) * height * 4;
        const QByteArray raw = readExactPumpingEvents(socket, bytes);
        if (raw.size() != bytes)
            return false;

        qsizetype offset = 0;
        for (std::uint32_t row = 0; row < height; ++row) {
            for (std::uint32_t column = 0; column < width; ++column) {
                const std::uint32_t rgb
                    = (static_cast<std::uint32_t>(static_cast<unsigned char>(raw.at(offset))) << 16U)
                      | (static_cast<std::uint32_t>(static_cast<unsigned char>(raw.at(offset + 1))) << 8U)
                      | static_cast<std::uint32_t>(static_cast<unsigned char>(raw.at(offset + 2)));
                frame.set(static_cast<std::uint16_t>(x + column),
                          static_cast<std::uint16_t>(y + row),
                          rgb);
                offset += 4;
            }
        }
    }
    return true;
}

class OneFrameCapture final : public hyremote::CaptureSource
{
public:
    hyremote::CaptureCapabilities capabilities() const override
    {
        hyremote::CaptureCapabilities result;
        result.asynchronous = false;
        result.cpuReadable = true;
        result.cpuFormats = {hyremote::PixelFormat::Rgba8888};
        return result;
    }

    bool start(hyremote::FrameReadyHandler onFrame,
               hyremote::CaptureEventHandler onEvent) override
    {
        m_onFrame = std::move(onFrame);
        m_onEvent = std::move(onEvent);
        m_running = static_cast<bool>(m_onFrame);
        m_sent = false;
        return m_running;
    }

    void stop() noexcept override
    {
        m_running = false;
        m_onFrame = {};
        m_onEvent = {};
    }

    bool requestFrame(const hyremote::CaptureRequest &request) override
    {
        if (!m_running || m_sent || !m_onFrame)
            return false;
        m_sent = true;

        constexpr std::uint32_t width = 64;
        constexpr std::uint32_t height = 48;
        auto storage = hyremote::CpuFrameStorage::createSinglePlane(width * 4U, height);
        if (!storage)
            return false;

        hyremote::RemoteFrame frame;
        frame.geometry.size = {width, height};
        frame.geometry.pixelFormat = hyremote::PixelFormat::Rgba8888;
        frame.geometry.alphaMode = hyremote::AlphaMode::Opaque;
        frame.geometry.planeCount = 1;
        frame.storage = std::move(storage);
        frame.timing.pts = hyremote::Clock::now();
        frame.timing.ptsSource = hyremote::PtsSource::Completion;
        frame.timing.requestTime = request.requestTime;
        frame.timing.completionTime = frame.timing.pts;
        frame.damage = hyremote::Damage::fullFrame();
        frame.requestId = request.id;
        m_onFrame(std::move(frame));
        return true;
    }

private:
    bool m_running = false;
    bool m_sent = false;
    hyremote::FrameReadyHandler m_onFrame;
    hyremote::CaptureEventHandler m_onEvent;
};

class ProbeWidget final : public QWidget
{
public:
    int buttonPresses = 0;
    int buttonReleases = 0;
    int keyPresses = 0;
    int keyReleases = 0;

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        ++buttonPresses;
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        ++buttonReleases;
        event->accept();
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        ++keyPresses;
        event->accept();
    }

    void keyReleaseEvent(QKeyEvent *event) override
    {
        ++keyReleases;
        event->accept();
    }
};

class VisualProbeWidget final : public QWidget
{
public:
    static constexpr std::uint32_t darkRgb = 0x183048U;
    static constexpr std::uint32_t lightRgb = 0xd09030U;

    VisualProbeWidget()
    {
        setAutoFillBackground(true);
        applyColor(darkRgb);
    }

    int visualChanges = 0;
    int buttonReleases = 0;

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        toggle();
        event->accept();
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        toggle();
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        ++buttonReleases;
        event->accept();
    }

private:
    void applyColor(std::uint32_t rgb)
    {
        QPalette palette = this->palette();
        palette.setColor(QPalette::Window,
                         QColor(static_cast<int>((rgb >> 16U) & 0xffU),
                                static_cast<int>((rgb >> 8U) & 0xffU),
                                static_cast<int>(rgb & 0xffU)));
        setPalette(palette);
        update();
    }

    void toggle()
    {
        ++visualChanges;
        m_light = !m_light;
        applyColor(m_light ? lightRgb : darkRgb);
    }

    bool m_light = false;
};

void testDisconnectCleanupCrossesSaturatedAdapterMailbox()
{
    HyRemote::detail::resetFactories();

    ProbeWidget target;
    target.resize(100, 60);
    target.setFocusPolicy(Qt::StrongFocus);
    target.show();
    target.setFocus();
    QCoreApplication::processEvents();

    auto targetComponents = HyRemote::detail::createTargetComponents(&target, true);
    CHECK(targetComponents.supported);
    CHECK(targetComponents.input != nullptr);
    if (!targetComponents.input)
        return;

    const quint16 port = freePort();
    CHECK(port != 0);
    if (port == 0)
        return;

    hyremote::Session session;
    CHECK(session.setCaptureSource(std::make_unique<OneFrameCapture>()));
    CHECK(session.setTransport(HyRemote::detail::createRfbTransport(
        QHostAddress::LocalHost, port, HyRemote::detail::RfbSecurityConfig{})));
    session.setInputSink(targetComponents.input);
    CHECK(session.start());
    if (session.state() != hyremote::SessionState::Running) {
        session.stop();
        return;
    }

    QTcpSocket viewer;
    CHECK(connectRawRfb(viewer, port));
    CHECK(waitStats(session, [](const auto &stats) { return stats.transportEvents >= 1; }));

    CHECK(sendKey(viewer, 0xffe1U, true)); // Shift_L
    CHECK(sendPointer(viewer, 0x01U, 20, 20));
    CHECK(pumpUntil([&] { return target.keyPresses >= 1 && target.buttonPresses == 1; }));
    CHECK(target.keyReleases == 0);
    CHECK(target.buttonReleases == 0);

    const auto beforeFlood = session.stats();

    // Keep the GUI thread out of its event loop while the transport worker produces exactly 64
    // ordinary adapter events: each printable A key-down produces one Key plus one Text event.
    for (int i = 0; i < 32; ++i)
        CHECK(sendKey(viewer, static_cast<std::uint32_t>('a'), true));

    CHECK(waitStats(session, [&](const auto &stats) {
        return stats.inputEventsPosted >= beforeFlood.inputEventsPosted + 64U;
    }));
    const auto saturated = session.stats();
    CHECK(saturated.inputPostFailures == beforeFlood.inputPostFailures);

    // Abrupt disconnect owns the final left-button, A and Shift references. Their three normalized
    // releases must cross Session and the protected adapter lane even though the normal 64-event
    // budget is still full and the GUI thread has not drained it yet.
    viewer.abort();
    CHECK(waitStats(session, [&](const auto &stats) {
        return stats.transportEvents >= saturated.transportEvents + 1U
               && stats.inputEventsPosted >= saturated.inputEventsPosted + 3U;
    }));
    const auto afterDisconnect = session.stats();
    CHECK(afterDisconnect.inputPostFailures == saturated.inputPostFailures);

    CHECK(pumpUntil([&] { return target.buttonReleases == 1 && target.keyReleases >= 2; }));
    CHECK(target.buttonReleases == 1);
    CHECK(target.keyReleases >= 2); // printable A then Shift

    session.stop();
    targetComponents.input.reset();
    HyRemote::detail::resetFactories();
}

void testWidgetVisibleResponseLatency()
{
    HyRemote::detail::resetFactories();

    VisualProbeWidget target;
    target.resize(96, 64);
    target.show();
    QCoreApplication::processEvents();

    auto targetComponents = HyRemote::detail::createTargetComponents(&target, true);
    CHECK(targetComponents.supported);
    CHECK(targetComponents.capture != nullptr);
    CHECK(targetComponents.input != nullptr);
    if (!targetComponents.capture || !targetComponents.input)
        return;

    const quint16 port = freePort();
    CHECK(port != 0);
    if (port == 0)
        return;

    hyremote::Session session;
    CHECK(session.setCaptureSource(std::move(targetComponents.capture)));
    CHECK(session.setTransport(HyRemote::detail::createRfbTransport(
        QHostAddress::LocalHost, port, HyRemote::detail::RfbSecurityConfig{})));
    session.setInputSink(targetComponents.input);
    CHECK(session.start());
    if (session.state() != hyremote::SessionState::Running) {
        session.stop();
        return;
    }

    // Let the real asynchronous Widgets capture produce the initial frame before the blocking RFB
    // handshake. This keeps one handshake implementation while still allowing GUI-thread capture.
    CHECK(pumpUntil([&] { return session.stats().framesDispatched >= 1U; }));

    QTcpSocket viewer;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    CHECK(connectRawRfb(viewer, port, &width, &height));
    CHECK(width > 0 && height > 0);
    CHECK(sendSetEncodingsRaw(viewer));
    CHECK(sendUpdateRequest(viewer, false, width, height));

    RawFramebuffer framebuffer;
    framebuffer.resize(width, height);
    CHECK(readRawFramebufferUpdatePumping(viewer, framebuffer));
    const std::uint16_t sampleX = static_cast<std::uint16_t>(width / 2U);
    const std::uint16_t sampleY = static_cast<std::uint16_t>(height / 2U);
    CHECK(framebuffer.get(sampleX, sampleY) == VisualProbeWidget::darkRgb);

    constexpr int sampleCount = 12;
    std::vector<std::chrono::microseconds> samples;
    samples.reserve(sampleCount);

    for (int sample = 0; sample < sampleCount; ++sample) {
        const std::uint32_t expected = (sample % 2) == 0 ? VisualProbeWidget::lightRgb
                                                         : VisualProbeWidget::darkRgb;
        CHECK(sendUpdateRequest(viewer, true, width, height));

        const auto started = std::chrono::steady_clock::now();
        CHECK(sendPointer(viewer, 0x01U, sampleX, sampleY));
        CHECK(readRawFramebufferUpdatePumping(viewer, framebuffer));
        const auto finished = std::chrono::steady_clock::now();

        CHECK(framebuffer.get(sampleX, sampleY) == expected);
        samples.push_back(std::chrono::duration_cast<std::chrono::microseconds>(finished - started));

        CHECK(sendPointer(viewer, 0x00U, sampleX, sampleY));
        CHECK(pumpUntil([&] { return target.buttonReleases >= sample + 1; }));
    }

    CHECK(target.visualChanges == sampleCount);
    CHECK(samples.size() == static_cast<std::size_t>(sampleCount));
    const auto summary = HyRemote::test::summarizeInteractionLatencies(samples);
    CHECK(summary.sampleCount == static_cast<std::size_t>(sampleCount));
    CHECK(summary.minUs <= summary.p50Us);
    CHECK(summary.p50Us <= summary.p95Us);
    CHECK(summary.p95Us <= summary.maxUs);
    std::cout << "HYREMOTE_INTERACTION_LATENCY"
              << " slice=widgets_visible_response"
              << " encoding=raw"
              << " samples=" << summary.sampleCount
              << " min_us=" << summary.minUs
              << " p50_us=" << summary.p50Us
              << " p95_us=" << summary.p95Us
              << " max_us=" << summary.maxUs << '\n';

    viewer.disconnectFromHost();
    viewer.waitForDisconnected(1000);
    session.stop();
    targetComponents.input.reset();
    HyRemote::detail::resetFactories();
}

}  // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    testDisconnectCleanupCrossesSaturatedAdapterMailbox();
    testWidgetVisibleResponseLatency();
    if (failures != 0)
        std::cerr << failures << " RFB/Session/Widgets composition checks failed\n";
    return failures == 0 ? 0 : 1;
}
