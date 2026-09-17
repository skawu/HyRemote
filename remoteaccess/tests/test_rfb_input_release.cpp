// Deterministic transport evidence for issue #90 (release held remote input state on viewer
// disconnect). The tests drive the real bounded RFB transport over loopback with a minimal client and
// assert on the normalized InputEvents the transport publishes.
//
// Evidence covered here:
//   1. a held key followed by an abrupt disconnect produces the matching normalized key release;
//   2. held pointer buttons followed by a disconnect produce releases at the last known location;
//   3. input the viewer already released normally is not released a second time;
//   4. a viewer that disconnects before reaching the input phase receives no synthetic releases;
//   5. one viewer's disconnect does not touch another viewer's held state;
//   6. stopping the transport publishes no late input after the input callback is disabled.

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "hyremote/core/frame.hpp"
#include "hyremote/core/input.hpp"
#include "hyremote/core/storage.hpp"
#include "hyremote/core/transport.hpp"
#include "transport/rfb_transport.hpp"

namespace {

int g_failures = 0;

#define HYR_CHECK(condition)                                                                      \
    do {                                                                                          \
        if (!(condition)) {                                                                       \
            ++g_failures;                                                                         \
            std::cerr << "FAIL line " << __LINE__ << ": " #condition << std::endl;                 \
        }                                                                                         \
    } while (false)

template <typename Predicate> bool pumpUntil(Predicate predicate, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    for (;;) {
        if (predicate())
            return true;
        if (timer.elapsed() > timeoutMs)
            return false;
        QCoreApplication::processEvents();
        QThread::msleep(2);
    }
}

void appendU16(QByteArray &message, std::uint16_t value)
{
    message.append(static_cast<char>((value >> 8U) & 0xFFU));
    message.append(static_cast<char>(value & 0xFFU));
}

void appendU32(QByteArray &message, std::uint32_t value)
{
    message.append(static_cast<char>((value >> 24U) & 0xFFU));
    message.append(static_cast<char>((value >> 16U) & 0xFFU));
    message.append(static_cast<char>((value >> 8U) & 0xFFU));
    message.append(static_cast<char>(value & 0xFFU));
}

// ---------------------------------------------------------------------------------------------
// Minimal RFB 3.8 client: version handshake, SecurityType None, ClientInit, then raw KeyEvent /
// PointerEvent messages. Only what these tests need is implemented.
// ---------------------------------------------------------------------------------------------
class TestClient
{
public:
    bool connectToServer(quint16 port)
    {
        m_socket.connectToHost(QHostAddress::LocalHost, port);
        return pumpUntil([this] { return m_socket.state() == QAbstractSocket::ConnectedState; }, 3000);
    }

    bool handshake()
    {
        if (!waitForBytes(12))
            return false;
        m_socket.readAll();  // server version banner
        if (m_socket.write("RFB 003.008\n", 12) != 12 || !flush())
            return false;
        if (!waitForBytes(2))
            return false;
        const QByteArray security = m_socket.readAll();
        if (security.size() < 2 || static_cast<unsigned char>(security.at(0)) != 1
            || static_cast<unsigned char>(security.at(1)) != 1) {
            return false;
        }
        const QByteArray choice(1, static_cast<char>(1));  // SecurityType None
        if (m_socket.write(choice, 1) != 1 || !flush())
            return false;
        if (!waitForBytes(4))
            return false;
        m_socket.readAll();  // SecurityResult OK
        const QByteArray clientInit(1, static_cast<char>(1));  // shared
        if (m_socket.write(clientInit, 1) != 1 || !flush())
            return false;
        return waitForBytes(24);  // ServerInit; contents are not needed for input-phase tests
    }

    bool sendKey(std::uint32_t keysym, bool pressed)
    {
        QByteArray message;
        message.append(static_cast<char>(4));
        message.append(static_cast<char>(pressed ? 1 : 0));
        appendU16(message, 0);
        appendU32(message, keysym);
        return writeMessage(message);
    }

    bool sendPointer(std::uint8_t mask, std::uint16_t x, std::uint16_t y)
    {
        QByteArray message;
        message.append(static_cast<char>(5));
        message.append(static_cast<char>(mask));
        appendU16(message, x);
        appendU16(message, y);
        return writeMessage(message);
    }

    void abruptDisconnect() { m_socket.abort(); }

    void gracefulDisconnect()
    {
        m_socket.disconnectFromHost();
        pumpUntil([this] { return m_socket.state() == QAbstractSocket::UnconnectedState; }, 3000);
    }

private:
    bool writeMessage(const QByteArray &message)
    {
        if (m_socket.write(message) != message.size())
            return false;
        return flush();
    }

    bool flush() { return m_socket.waitForBytesWritten(3000); }

    bool waitForBytes(int count)
    {
        return pumpUntil([this, count] { return m_socket.bytesAvailable() >= count; }, 3000);
    }

    QTcpSocket m_socket;
};

// ---------------------------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------------------------
struct EventSink
{
    void append(const hyremote::InputEvent &event)
    {
        std::lock_guard<std::mutex> lock(mutex);
        events.push_back(event);
    }

    std::vector<hyremote::InputEvent> snapshot()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return events;
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(mutex);
        events.clear();
    }

    std::mutex mutex;
    std::vector<hyremote::InputEvent> events;
};

quint16 findFreePort()
{
    QTcpServer probe;
    if (!probe.listen(QHostAddress::LocalHost, 0))
        return 0;
    const quint16 port = probe.serverPort();
    probe.close();
    return port;
}

// The transport needs a frame with a valid viewport before it accepts pointer input.
hyremote::RemoteFrame makeFrame()
{
    constexpr std::uint32_t width = 64;
    constexpr std::uint32_t height = 48;
    auto storage = hyremote::CpuFrameStorage::createSinglePlane(width * 4U, height);
    hyremote::RemoteFrame frame;
    if (!storage)
        return frame;
    frame.id = 1;
    frame.geometry.size = {width, height};
    frame.geometry.pixelFormat = hyremote::PixelFormat::Rgba8888;
    frame.geometry.alphaMode = hyremote::AlphaMode::Opaque;
    frame.geometry.planeCount = 1;
    frame.storage = std::move(storage);
    frame.timing.pts = hyremote::Clock::now();
    frame.timing.ptsSource = hyremote::PtsSource::Completion;
    frame.damage = hyremote::Damage::fullFrame();
    return frame;
}

class TransportFixture
{
public:
    bool start()
    {
        const quint16 port = findFreePort();
        if (port == 0)
            return false;
        m_transport = HyRemote::detail::createRfbTransport(QHostAddress::LocalHost, port);
        if (!m_transport)
            return false;
        EventSink *sink = &m_sink;
        const bool started = m_transport->start(
            [sink](const hyremote::InputEvent &event) { sink->append(event); },
            [](const hyremote::TransportEvent &) {});
        if (!started)
            return false;
        m_port = port;
        m_transport->enqueueFrame(makeFrame());
        // Give the worker thread a moment to accept the frame so the viewport is valid.
        return pumpUntil([this] { return m_sink.snapshot().empty(); }, 200);
    }

    void stop() { m_transport->stop(); }

    quint16 port() const { return m_port; }
    EventSink &sink() { return m_sink; }

private:
    std::unique_ptr<hyremote::Transport> m_transport;
    EventSink m_sink;
    quint16 m_port = 0;
};

std::size_t countKeyEvents(const std::vector<hyremote::InputEvent> &events,
                           hyremote::KeyCode key,
                           bool pressed)
{
    return static_cast<std::size_t>(std::count_if(
        events.begin(), events.end(), [key, pressed](const hyremote::InputEvent &event) {
            return event.kind == hyremote::InputEventKind::Key && event.key == key
                   && event.pressed == pressed;
        }));
}

std::size_t countButtonEvents(const std::vector<hyremote::InputEvent> &events,
                              hyremote::PointerButton button,
                              bool pressed)
{
    return static_cast<std::size_t>(std::count_if(
        events.begin(), events.end(), [button, pressed](const hyremote::InputEvent &event) {
            return event.kind == hyremote::InputEventKind::PointerButton && event.button == button
                   && event.pressed == pressed;
        }));
}

const hyremote::InputEvent *findButtonEvent(const std::vector<hyremote::InputEvent> &events,
                                            hyremote::PointerButton button,
                                            bool pressed)
{
    const auto it = std::find_if(events.begin(), events.end(),
                                 [button, pressed](const hyremote::InputEvent &event) {
                                     return event.kind == hyremote::InputEventKind::PointerButton
                                            && event.button == button && event.pressed == pressed;
                                 });
    return it == events.end() ? nullptr : &*it;
}

// ---------------------------------------------------------------------------------------------
// 1. Held key + abrupt disconnect -> the matching normalized key release.
// ---------------------------------------------------------------------------------------------
void testHeldKeyIsReleasedOnAbruptDisconnect()
{
    TransportFixture fixture;
    HYR_CHECK(fixture.start());

    TestClient client;
    HYR_CHECK(client.connectToServer(fixture.port()));
    HYR_CHECK(client.handshake());

    constexpr std::uint32_t kKeysymA = 0x61;
    HYR_CHECK(client.sendKey(kKeysymA, true));
    HYR_CHECK(pumpUntil([&] { return countKeyEvents(fixture.sink().snapshot(),
                                                    hyremote::KeyCode::A, true) == 1; },
                        2000));

    client.abruptDisconnect();
    HYR_CHECK(pumpUntil([&] { return countKeyEvents(fixture.sink().snapshot(),
                                                    hyremote::KeyCode::A, false) == 1; },
                        2000));
    const auto events = fixture.sink().snapshot();
    HYR_CHECK(countKeyEvents(events, hyremote::KeyCode::A, true) == 1);
    HYR_CHECK(countKeyEvents(events, hyremote::KeyCode::A, false) == 1);

    fixture.stop();
}

// ---------------------------------------------------------------------------------------------
// 2. Held buttons + disconnect -> releases at the last known pointer location.
// ---------------------------------------------------------------------------------------------
void testHeldButtonsAreReleasedAtTheLastLocation()
{
    TransportFixture fixture;
    HYR_CHECK(fixture.start());

    TestClient client;
    HYR_CHECK(client.connectToServer(fixture.port()));
    HYR_CHECK(client.handshake());

    constexpr std::uint16_t kX = 30;
    constexpr std::uint16_t kY = 20;
    HYR_CHECK(client.sendPointer(0x05, kX, kY));  // left + right held
    HYR_CHECK(pumpUntil([&] { return countButtonEvents(fixture.sink().snapshot(),
                                                       hyremote::PointerButton::Left, true) == 1; },
                        2000));

    client.abruptDisconnect();
    HYR_CHECK(pumpUntil([&] { return countButtonEvents(fixture.sink().snapshot(),
                                                       hyremote::PointerButton::Left, false) == 1; },
                        2000));
    const auto events = fixture.sink().snapshot();
    HYR_CHECK(countButtonEvents(events, hyremote::PointerButton::Right, true) == 1);
    HYR_CHECK(countButtonEvents(events, hyremote::PointerButton::Right, false) == 1);
    HYR_CHECK(countButtonEvents(events, hyremote::PointerButton::Middle, true) == 0);
    HYR_CHECK(countButtonEvents(events, hyremote::PointerButton::Middle, false) == 0);

    const hyremote::InputEvent *release =
        findButtonEvent(events, hyremote::PointerButton::Left, false);
    HYR_CHECK(release != nullptr);
    if (release != nullptr) {
        HYR_CHECK(release->x == static_cast<float>(kX));
        HYR_CHECK(release->y == static_cast<float>(kY));
        HYR_CHECK(hyremote::isValidInputViewport(release->sourceViewport));
    }

    fixture.stop();
}

// ---------------------------------------------------------------------------------------------
// 3. A normal release is never duplicated by the disconnect path.
// ---------------------------------------------------------------------------------------------
void testNormalReleasesAreNotDuplicated()
{
    TransportFixture fixture;
    HYR_CHECK(fixture.start());

    TestClient client;
    HYR_CHECK(client.connectToServer(fixture.port()));
    HYR_CHECK(client.handshake());

    constexpr std::uint32_t kKeysymA = 0x61;
    HYR_CHECK(client.sendPointer(0x01, 10, 12));
    HYR_CHECK(client.sendKey(kKeysymA, true));
    HYR_CHECK(client.sendKey(kKeysymA, false));
    HYR_CHECK(client.sendPointer(0x00, 10, 12));
    HYR_CHECK(pumpUntil([&] { return countButtonEvents(fixture.sink().snapshot(),
                                                       hyremote::PointerButton::Left, false) == 1; },
                        2000));

    client.gracefulDisconnect();
    QThread::msleep(200);
    QCoreApplication::processEvents();

    const auto events = fixture.sink().snapshot();
    HYR_CHECK(countKeyEvents(events, hyremote::KeyCode::A, false) == 1);
    HYR_CHECK(countButtonEvents(events, hyremote::PointerButton::Left, false) == 1);
    HYR_CHECK(countButtonEvents(events, hyremote::PointerButton::Left, true) == 1);

    fixture.stop();
}

// ---------------------------------------------------------------------------------------------
// 4. A viewer that never reached the input phase receives no synthetic releases.
// ---------------------------------------------------------------------------------------------
void testUnannouncedViewerGetsNoSyntheticRelease()
{
    TransportFixture fixture;
    HYR_CHECK(fixture.start());

    TestClient client;
    HYR_CHECK(client.connectToServer(fixture.port()));
    HYR_CHECK(client.sendKey(0x61, true));  // sent before the handshake completes
    client.abruptDisconnect();
    QThread::msleep(200);
    QCoreApplication::processEvents();

    const auto events = fixture.sink().snapshot();
    HYR_CHECK(countKeyEvents(events, hyremote::KeyCode::A, false) == 0);

    fixture.stop();
}

// ---------------------------------------------------------------------------------------------
// 5. One viewer's disconnect does not mutate another viewer's held state.
// ---------------------------------------------------------------------------------------------
void testViewersDoNotShareHeldState()
{
    TransportFixture fixture;
    HYR_CHECK(fixture.start());

    TestClient first;
    TestClient second;
    HYR_CHECK(first.connectToServer(fixture.port()));
    HYR_CHECK(first.handshake());
    HYR_CHECK(second.connectToServer(fixture.port()));
    HYR_CHECK(second.handshake());

    constexpr std::uint32_t kKeysymA = 0x61;
    constexpr std::uint32_t kKeysymB = 0x62;
    HYR_CHECK(first.sendKey(kKeysymA, true));
    HYR_CHECK(second.sendKey(kKeysymB, true));
    HYR_CHECK(pumpUntil([&] { return countKeyEvents(fixture.sink().snapshot(),
                                                    hyremote::KeyCode::B, true) == 1; },
                        2000));

    first.abruptDisconnect();
    HYR_CHECK(pumpUntil([&] { return countKeyEvents(fixture.sink().snapshot(),
                                                    hyremote::KeyCode::A, false) == 1; },
                        2000));

    // The second viewer's held key is untouched: exactly one press and no release for it yet.
    const auto afterFirst = fixture.sink().snapshot();
    HYR_CHECK(countKeyEvents(afterFirst, hyremote::KeyCode::A, false) == 1);
    HYR_CHECK(countKeyEvents(afterFirst, hyremote::KeyCode::B, true) == 1);
    HYR_CHECK(countKeyEvents(afterFirst, hyremote::KeyCode::B, false) == 0);

    // A later viewer therefore starts clean, and its own presses are tracked normally.
    TestClient third;
    HYR_CHECK(third.connectToServer(fixture.port()));
    HYR_CHECK(third.handshake());
    HYR_CHECK(third.sendKey(kKeysymB, true));
    HYR_CHECK(third.sendKey(kKeysymB, false));
    HYR_CHECK(pumpUntil([&] { return countKeyEvents(fixture.sink().snapshot(),
                                                    hyremote::KeyCode::B, false) == 1; },
                        2000));
    const auto afterThird = fixture.sink().snapshot();
    HYR_CHECK(countKeyEvents(afterThird, hyremote::KeyCode::B, true) == 2);
    HYR_CHECK(countKeyEvents(afterThird, hyremote::KeyCode::B, false) == 1);

    second.abruptDisconnect();
    HYR_CHECK(pumpUntil([&] { return countKeyEvents(fixture.sink().snapshot(),
                                                    hyremote::KeyCode::B, false) == 2; },
                        2000));

    fixture.stop();
}

// ---------------------------------------------------------------------------------------------
// 6. Stopping the transport publishes no late input.
// ---------------------------------------------------------------------------------------------
void testStopDoesNotPublishLateInput()
{
    TransportFixture fixture;
    HYR_CHECK(fixture.start());

    TestClient client;
    HYR_CHECK(client.connectToServer(fixture.port()));
    HYR_CHECK(client.handshake());
    HYR_CHECK(client.sendKey(0x61, true));
    HYR_CHECK(client.sendPointer(0x01, 5, 6));
    HYR_CHECK(pumpUntil([&] { return countButtonEvents(fixture.sink().snapshot(),
                                                       hyremote::PointerButton::Left, true) == 1; },
                        2000));

    fixture.stop();
    const std::size_t afterStop = fixture.sink().snapshot().size();

    QThread::msleep(200);
    QCoreApplication::processEvents();
    HYR_CHECK(fixture.sink().snapshot().size() == afterStop);
    HYR_CHECK(countKeyEvents(fixture.sink().snapshot(), hyremote::KeyCode::A, false) == 0);
    HYR_CHECK(countButtonEvents(fixture.sink().snapshot(), hyremote::PointerButton::Left, false) == 0);

    client.abruptDisconnect();
}

}  // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    testHeldKeyIsReleasedOnAbruptDisconnect();
    testHeldButtonsAreReleasedAtTheLastLocation();
    testNormalReleasesAreNotDuplicated();
    testUnannouncedViewerGetsNoSyntheticRelease();
    testViewersDoNotShareHeldState();
    testStopDoesNotPublishLateInput();

    if (g_failures == 0) {
        std::cout << "rfb input-release evidence: 6 case(s), 0 failure(s)" << std::endl;
        return 0;
    }
    std::cerr << "rfb input-release evidence: " << g_failures << " failure(s)" << std::endl;
    return 1;
}
