// Production RFB -> Runtime -> Qt window-system ingress acceptance for #400.
// RFB has no double-click concept: it carries pointer button-mask transitions. This fixture proves
// the parser emits raw facts and Qt, not HyRemote, classifies the resulting pointer sequence.

#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QEventLoop>
#include <QHostAddress>
#include <QMouseEvent>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QWidget>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <vector>

#include "detail/component_factories.hpp"
#include "hyremote/core/frame.hpp"
#include "hyremote/core/input.hpp"
#include "hyremote/core/storage.hpp"
#include "hyremote/core/transport.hpp"
#include "transport/rfb_transport.hpp"

namespace {

int failures = 0;

void check(bool ok, const char *what)
{
    if (!ok) {
        std::cerr << "CHECK failed: " << what << '\n';
        ++failures;
    }
    std::cout << (ok ? "ok   " : "FAIL ") << what << '\n';
}

void pumpFor(int milliseconds)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < milliseconds)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

class Probe final : public QWidget
{
public:
    explicit Probe(QWidget *parent = nullptr)
        : QWidget(parent)
    {
    }

    int presses = 0;
    int releases = 0;
    int doubleClicks = 0;

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        ++presses;
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        ++releases;
        event->accept();
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        ++doubleClicks;
        event->accept();
    }
};

void appendU16(QByteArray &data, std::uint16_t value)
{
    data.append(static_cast<char>((value >> 8U) & 0xffU));
    data.append(static_cast<char>(value & 0xffU));
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

bool connectRawRfb(QTcpSocket &socket, quint16 port)
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
    const std::uint32_t nameLength = readU32(serverInit, 20);
    return readExact(socket, static_cast<qsizetype>(nameLength)).size()
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

class FactLog
{
public:
    void record(const hyremote::InputEvent &event)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_events.push_back(event);
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_events.clear();
    }

    std::size_t countButton(hyremote::PointerButton button, bool pressed)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return static_cast<std::size_t>(std::count_if(m_events.begin(), m_events.end(), [&](const auto &e) {
            return e.kind == hyremote::InputEventKind::PointerButton && e.button == button
                   && e.pressed == pressed;
        }));
    }

private:
    std::mutex m_mutex;
    std::vector<hyremote::InputEvent> m_events;
};

struct Harness
{
    quint16 port = 0;
    std::unique_ptr<hyremote::Transport> transport;
    HyRemote::detail::TargetComponents components;

    bool start(QWidget &root, FactLog &facts)
    {
        port = freePort();
        if (port == 0)
            return false;

        components = HyRemote::detail::createTargetComponents(&root, true);
        if (!components.input)
            return false;

        transport = HyRemote::detail::createRfbTransport(QHostAddress::LocalHost, port,
                                                         HyRemote::detail::RfbSecurityConfig{});
        if (!transport)
            return false;
        if (!transport->start(
                [&facts, this](const hyremote::InputEvent &event) {
                    facts.record(event);
                    components.input->post(event);
                },
                [](const hyremote::TransportEvent &) {}))
            return false;

        constexpr std::uint32_t width = 500;
        constexpr std::uint32_t height = 500;
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
        frame.damage = hyremote::Damage::fullFrame();
        transport->enqueueFrame(std::move(frame));
        return true;
    }

    void stop()
    {
        if (transport)
            transport->stop();
        transport.reset();
        components.input.reset();
    }
};

void sendClick(QTcpSocket &viewer, std::uint16_t x, std::uint16_t y)
{
    check(sendPointer(viewer, 0x01U, x, y), "RFB left-down is written");
    check(sendPointer(viewer, 0x00U, x, y), "RFB left-up is written");
}

void runAcceptance()
{
    HyRemote::detail::resetFactories();

    QWidget root;
    root.resize(500, 500);
    Probe probe(&root);
    probe.setGeometry(0, 0, 500, 500);
    root.show();
    pumpFor(50);

    FactLog facts;
    Harness harness;
    check(harness.start(root, facts), "RFB production harness starts");
    if (!harness.transport)
        return;

    QTcpSocket viewer;
    check(connectRawRfb(viewer, harness.port), "raw RFB viewer connects");
    if (viewer.state() != QAbstractSocket::ConnectedState) {
        harness.stop();
        return;
    }

    facts.clear();
    probe.presses = probe.releases = probe.doubleClicks = 0;
    check(sendPointer(viewer, 0x00U, 100, 100), "RFB move is written");
    sendClick(viewer, 100, 100);
    QThread::msleep(8);
    sendClick(viewer, 100, 100);
    pumpFor(250);

    check(facts.countButton(hyremote::PointerButton::Left, true) == 2,
          "RFB parser emits two raw left-press facts");
    check(facts.countButton(hyremote::PointerButton::Left, false) == 2,
          "RFB parser emits two raw left-release facts");
    check(probe.doubleClicks == 1,
          "Qt classifies the production RFB pointer sequence as one double click");

    QThread::msleep(static_cast<unsigned long>(QApplication::doubleClickInterval()) + 20UL);
    facts.clear();
    probe.presses = probe.releases = probe.doubleClicks = 0;
    sendClick(viewer, 100, 100);
    pumpFor(QApplication::doubleClickInterval() + 50);
    sendClick(viewer, 100, 100);
    pumpFor(150);
    check(probe.doubleClicks == 0,
          "RFB clicks outside Qt's interval remain two single clicks");

    QThread::msleep(static_cast<unsigned long>(QApplication::doubleClickInterval()) + 20UL);
    probe.presses = probe.releases = probe.doubleClicks = 0;
    sendClick(viewer, 40, 40);
    QThread::msleep(8);
    sendClick(viewer, 360, 260);
    pumpFor(150);
    check(probe.doubleClicks == 0,
          "RFB clicks outside Qt's distance remain two single clicks");

    viewer.disconnectFromHost();
    viewer.waitForDisconnected(1000);
    harness.stop();
}

}  // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    runAcceptance();
    HyRemote::detail::resetFactories();

    std::cout << (failures == 0 ? "PASS: RFB window-system input acceptance"
                                : "FAIL: RFB window-system input acceptance")
              << " (" << failures << " checks failed)\n";
    return failures == 0 ? 0 : 1;
}
