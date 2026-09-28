// #401 deterministic reproduction through the PRODUCTION RFB pointer parser.
//
// The viewer side of #400 is a real RFB client: double clicks arrive as ordinary PointerEvent
// button-mask transitions. This fixture drives the real RFB transport over a real socket, records
// the transport-neutral facts the parser produced (to prove the first divergence is NOT in
// RFB_PARSE), feeds them into the real Widgets adapter, and classifies each timing/receiver case.
//
// Cases required by #401: move / left-down / left-up / left-down / left-up envelope, outside
// interval, outside distance, different receiver.
//
// Assertions pin CURRENT observed behaviour; DIVERGENCE lines are the measured evidence.

#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QEventLoop>
#include <QGuiApplication>
#include <QHostAddress>
#include <QMouseEvent>
#include <QStyleHints>
#include <QTcpServer>
#include <QThread>
#include <QTcpSocket>
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
int divergences = 0;

void check(bool ok, const char *what)
{
    if (!ok) {
        std::cerr << "CHECK failed: " << what << '\n';
        ++failures;
    }
    std::cout << (ok ? "ok   " : "FAIL ") << what << '\n';
}

void divergence(const char *layer, const char *scenario, const char *detail)
{
    ++divergences;
    std::cout << "DIVERGENCE[" << layer << "] " << scenario << ": " << detail << '\n';
}

void noDivergence(const char *scenario, const char *detail)
{
    std::cout << "NO_DEFECT " << scenario << ": " << detail << '\n';
}

void pump()
{
    for (int i = 0; i < 30; ++i)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
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
        setMouseTracking(true);
    }

    int presses = 0;
    int releases = 0;
    int doubleClicks = 0;
    QVector<QEvent::Type> order;

protected:
    bool event(QEvent *event) override
    {
        switch (event->type()) {
        case QEvent::MouseButtonPress:
            ++presses;
            order.append(event->type());
            event->accept();
            return true;
        case QEvent::MouseButtonRelease:
            ++releases;
            order.append(event->type());
            event->accept();
            return true;
        case QEvent::MouseButtonDblClick:
            ++doubleClicks;
            order.append(event->type());
            event->accept();
            return true;
        default:
            return QWidget::event(event);
        }
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

    if (!writeAll(socket, QByteArray(1, char(1))))  // ClientInit shared=true
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

// Records the transport-neutral facts emitted by the production parser, before any target routing.
class FactLog
{
public:
    void record(const hyremote::InputEvent &event)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_events.push_back(event);
    }

    std::vector<hyremote::InputEvent> snapshot()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_events;
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_events.clear();
    }

    std::size_t countButton(hyremote::PointerButton button, bool pressed)
    {
        const std::vector<hyremote::InputEvent> events = snapshot();
        return static_cast<std::size_t>(std::count_if(events.begin(), events.end(), [&](const auto &e) {
            return e.kind == hyremote::InputEventKind::PointerButton && e.button == button
                   && e.pressed == pressed;
        }));
    }

    std::size_t countMoves()
    {
        const std::vector<hyremote::InputEvent> events = snapshot();
        return static_cast<std::size_t>(std::count_if(events.begin(), events.end(), [](const auto &e) {
            return e.kind == hyremote::InputEventKind::PointerMove;
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
        // The real production Widgets adapter for this root: the transport's input handler posts
        // into exactly the sink the product installs (Session::onInput -> sink->post).
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

void runPreflight()
{
    HyRemote::detail::resetFactories();

    QWidget root;
    root.resize(500, 500);

    FactLog facts;
    Harness harness;
    if (!harness.start(root, facts)) {
        check(false, "rfb harness starts");
        return;
    }

    Probe probe(&root);
    probe.setGeometry(0, 0, 500, 500);

    // Second receiver, used for the different-receiver case.
    Probe other(&root);
    other.setGeometry(0, 0, 0, 0);  // configured per-scenario via geometry below
    pump();

    QTcpSocket viewer;
    if (!connectRawRfb(viewer, harness.port)) {
        check(false, "raw RFB viewer connects");
        harness.stop();
        return;
    }
    pump();

    // --- Case 1: valid double-click envelope -------------------------------------------------
    facts.clear();
    probe.presses = probe.releases = probe.doubleClicks = 0;
    probe.order.clear();
    sendPointer(viewer, 0x00U, 100, 100);  // move
    sendPointer(viewer, 0x01U, 100, 100);  // left-down
    sendPointer(viewer, 0x00U, 100, 100);  // left-up
    sendPointer(viewer, 0x01U, 100, 100);  // left-down
    sendPointer(viewer, 0x00U, 100, 100);  // left-up
    pumpFor(250);

    check(facts.countButton(hyremote::PointerButton::Left, true) == 2
              && facts.countButton(hyremote::PointerButton::Left, false) == 2,
          "RFB_PARSE: the wire sequence yields two left press and two left release facts");
    check(probe.doubleClicks == 1,
          "#400 production RFB: a viewer double click on the wire yields one MouseButtonDblClick");
    check(probe.presses == 2,
          "#400 production RFB: the wire yields two ordinary presses plus one added DblClick");
    check(probe.releases == 2, "#400 production RFB: both releases are delivered");
    if (probe.doubleClicks == 1)
        noDivergence("RFB valid double-click envelope",
                     "the production parser path reaches Qt's double-click semantic without any RFB "
                     "protocol concept");

    // --- Case 2: outside interval ------------------------------------------------------------
    facts.clear();
    probe.presses = probe.releases = probe.doubleClicks = 0;
    probe.order.clear();
    sendPointer(viewer, 0x01U, 100, 100);
    sendPointer(viewer, 0x00U, 100, 100);
    pumpFor(250);
    pumpFor(650);
    sendPointer(viewer, 0x01U, 100, 100);
    sendPointer(viewer, 0x00U, 100, 100);
    pumpFor(250);
    check(probe.presses == 2 && probe.doubleClicks == 0,
          "outside interval: two single clicks");
    noDivergence("RFB outside interval",
                 "two singles is the correct platform answer; #400 must keep this case unchanged");

    // --- Case 3: outside distance ------------------------------------------------------------
    facts.clear();
    probe.presses = probe.releases = probe.doubleClicks = 0;
    probe.order.clear();
    sendPointer(viewer, 0x01U, 40, 40);
    sendPointer(viewer, 0x00U, 40, 40);
    sendPointer(viewer, 0x01U, 360, 260);
    sendPointer(viewer, 0x00U, 360, 260);
    pumpFor(250);
    check(probe.presses == 2 && probe.doubleClicks == 0,
          "outside distance: two single clicks");
    noDivergence("RFB outside distance",
                 "two singles is the correct platform answer; #400 must keep this case unchanged");

    // --- Case 4: different receiver ----------------------------------------------------------
    facts.clear();
    probe.presses = probe.releases = probe.doubleClicks = 0;
    probe.order.clear();
    probe.setGeometry(0, 0, 200, 200);
    other.setGeometry(250, 250, 200, 200);
    pump();
    sendPointer(viewer, 0x01U, 100, 100);  // inside probe
    sendPointer(viewer, 0x00U, 100, 100);
    sendPointer(viewer, 0x01U, 300, 300);  // inside other, immediately after
    sendPointer(viewer, 0x00U, 300, 300);
    pumpFor(250);
    check(probe.presses == 1 && other.presses == 1,
          "different receiver: each widget receives its own press");
    if (probe.doubleClicks == 0 && other.doubleClicks == 0) {
        noDivergence("RFB different receiver",
                     "no cross-receiver double click exists today; #400 must preserve this negative when it "
                     "introduces classification (receiver identity is a hard condition, not a refinement)");
    } else {
        divergence("QT_SEMANTIC_CLASSIFICATION",
                   "RFB different receiver",
                   "a cross-receiver double click was synthesized");
    }

    // --- Case 5: far excursion that coalescing replaces --------------------------------
    facts.clear();
    probe.presses = probe.releases = probe.doubleClicks = 0;
    probe.setGeometry(0, 0, 500, 600);
    other.setGeometry(0, 0, 0, 0);
    pump();
    sendPointer(viewer, 0x01U, 150, 150);
    sendPointer(viewer, 0x00U, 150, 150);
    pumpFor(150);
    sendPointer(viewer, 0x00U, 420, 520);  // far excursion, still queued
    sendPointer(viewer, 0x00U, 151, 151);  // return: ambient coalescing keeps only this move
    sendPointer(viewer, 0x01U, 150, 150);
    sendPointer(viewer, 0x00U, 150, 150);
    pumpFor(250);
    std::cout << "     observed[rfb far-return]: presses=" << probe.presses
              << " dblClicks=" << probe.doubleClicks << '\n';
    check(probe.doubleClicks == 0,
          "#400 production RFB far-return: a far excursion followed by a return does NOT form a "
          "double click");
    check(probe.presses == 2, "#400 production RFB far-return: both presses are ordinary presses");

    viewer.abort();
    harness.stop();
}

}  // namespace

int main(int argc, char **argv)
{
    // Widgets targets need a QApplication, not a bare QGuiApplication.
    QApplication app(argc, argv);
    if (QStyleHints *hints = QGuiApplication::styleHints())
        hints->setMouseDoubleClickInterval(400);

    runPreflight();

    std::cout << (failures == 0 ? "PASS: rfb input preflight reproduction"
                                : "FAIL: rfb input preflight reproduction")
              << " (checks failed: " << failures << ", divergence scenarios: " << divergences << ")\n";
    return failures == 0 ? 0 : 1;
}
