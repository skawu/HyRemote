#include <QByteArray>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
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

    if (!writeAll(socket, QByteArray(1, char(1)))) // ClientInit shared=true
        return false;
    const QByteArray serverInit = readExact(socket, 24);
    if (serverInit.size() != 24)
        return false;
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

struct Recorder
{
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<hyremote::InputEvent> inputs;
    std::vector<hyremote::TransportEvent> events;

    void record(const hyremote::InputEvent &event)
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            inputs.push_back(event);
        }
        changed.notify_all();
    }

    void record(const hyremote::TransportEvent &event)
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            events.push_back(event);
        }
        changed.notify_all();
    }

    template <typename Predicate>
    bool waitFor(Predicate predicate, int timeoutMs = 3000)
    {
        std::unique_lock<std::mutex> lock(mutex);
        return changed.wait_for(lock,
                                std::chrono::milliseconds(timeoutMs),
                                [&] { return predicate(inputs, events); });
    }

    std::vector<hyremote::InputEvent> inputSnapshot()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return inputs;
    }
};

std::size_t countKey(const std::vector<hyremote::InputEvent> &events,
                     hyremote::KeyCode key,
                     bool pressed)
{
    return static_cast<std::size_t>(std::count_if(events.begin(), events.end(), [&](const auto &event) {
        return event.kind == hyremote::InputEventKind::Key && event.key == key
               && event.pressed == pressed;
    }));
}

std::size_t countButton(const std::vector<hyremote::InputEvent> &events,
                        hyremote::PointerButton button,
                        bool pressed)
{
    return static_cast<std::size_t>(std::count_if(events.begin(), events.end(), [&](const auto &event) {
        return event.kind == hyremote::InputEventKind::PointerButton && event.button == button
               && event.pressed == pressed;
    }));
}

void testConcurrentViewerHeldStateIsolation()
{
    const quint16 port = freePort();
    CHECK(port != 0);
    if (port == 0)
        return;

    Recorder recorder;
    auto transport = HyRemote::detail::createRfbTransport(QHostAddress::LocalHost, port);
    CHECK(transport != nullptr);
    if (!transport)
        return;

    CHECK(transport->start([&](const hyremote::InputEvent &event) { recorder.record(event); },
                           [&](const hyremote::TransportEvent &event) { recorder.record(event); }));

    constexpr std::uint32_t width = 64;
    constexpr std::uint32_t height = 48;
    auto storage = hyremote::CpuFrameStorage::createSinglePlane(width * 4U, height);
    CHECK(storage != nullptr);
    if (!storage) {
        transport->stop();
        return;
    }

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

    QTcpSocket first;
    QTcpSocket second;
    CHECK(connectRawRfb(first, port));
    CHECK(connectRawRfb(second, port));
    CHECK(recorder.waitFor([](const auto &, const auto &events) {
        return std::count_if(events.begin(), events.end(), [](const auto &event) {
                   return event.code == hyremote::TransportEventCode::ClientConnected;
               })
               >= 2;
    }));

    CHECK(sendKey(first, 0xffe1U, true));  // Shift_L
    CHECK(sendKey(second, 0xffe1U, true));
    CHECK(sendKey(first, static_cast<std::uint32_t>('b'), true));
    CHECK(sendKey(second, static_cast<std::uint32_t>('b'), true));
    CHECK(sendPointer(first, 0x01U, 10, 10));
    CHECK(sendPointer(second, 0x01U, 20, 20));

    CHECK(recorder.waitFor([](const auto &inputs, const auto &) {
        return countKey(inputs, hyremote::KeyCode::Shift, true) >= 1
               && countKey(inputs, hyremote::KeyCode::B, true) >= 1
               && countButton(inputs, hyremote::PointerButton::Left, true) >= 1;
    }));
    QThread::msleep(100);

    auto inputs = recorder.inputSnapshot();
    CHECK(countKey(inputs, hyremote::KeyCode::Shift, true) == 1);
    CHECK(countKey(inputs, hyremote::KeyCode::B, true) == 1);
    CHECK(countButton(inputs, hyremote::PointerButton::Left, true) == 1);

    // A release from a viewer that never held the logical key must not subtract another viewer's
    // contribution. This explicitly covers malformed/out-of-order clients, not just disconnect.
    CHECK(sendKey(second, static_cast<std::uint32_t>('c'), true));
    CHECK(recorder.waitFor([](const auto &events, const auto &) {
        return countKey(events, hyremote::KeyCode::C, true) >= 1;
    }));
    CHECK(sendKey(first, static_cast<std::uint32_t>('c'), false));
    QThread::msleep(100);
    inputs = recorder.inputSnapshot();
    CHECK(countKey(inputs, hyremote::KeyCode::C, true) == 1);
    CHECK(countKey(inputs, hyremote::KeyCode::C, false) == 0);

    // First viewer disappears while both viewers still contribute the same logical holds. Its
    // cleanup must decrement only its own references and must not release the shared Qt target.
    first.abort();
    CHECK(recorder.waitFor([](const auto &, const auto &events) {
        return std::count_if(events.begin(), events.end(), [](const auto &event) {
                   return event.code == hyremote::TransportEventCode::ClientDisconnected;
               })
               >= 1;
    }));
    QThread::msleep(100);

    inputs = recorder.inputSnapshot();
    CHECK(countKey(inputs, hyremote::KeyCode::Shift, false) == 0);
    CHECK(countKey(inputs, hyremote::KeyCode::B, false) == 0);
    CHECK(countKey(inputs, hyremote::KeyCode::C, false) == 0);
    CHECK(countButton(inputs, hyremote::PointerButton::Left, false) == 0);

    // Aggregating distinct viewer holds must not suppress normal repeat input from the surviving
    // viewer. Its repeated B-down is a repeat while it is already the remaining holder, not a new
    // global held-state transition from another client.
    CHECK(sendKey(second, static_cast<std::uint32_t>('b'), true));
    CHECK(recorder.waitFor([](const auto &events, const auto &) {
        return countKey(events, hyremote::KeyCode::B, true) >= 2;
    }));
    inputs = recorder.inputSnapshot();
    CHECK(countKey(inputs, hyremote::KeyCode::B, true) == 2);
    CHECK(countKey(inputs, hyremote::KeyCode::B, false) == 0);

    // The surviving viewer owns the final references. Only its releases may transition the shared
    // target back to up, and ordinary releases must still observe Shift as held.
    CHECK(sendKey(second, static_cast<std::uint32_t>('c'), false));
    CHECK(sendKey(second, static_cast<std::uint32_t>('b'), false));
    CHECK(sendPointer(second, 0x00U, 22, 21));
    CHECK(sendKey(second, 0xffe1U, false));

    CHECK(recorder.waitFor([](const auto &inputs, const auto &) {
        return countKey(inputs, hyremote::KeyCode::Shift, false) >= 1
               && countKey(inputs, hyremote::KeyCode::B, false) >= 1
               && countKey(inputs, hyremote::KeyCode::C, false) >= 1
               && countButton(inputs, hyremote::PointerButton::Left, false) >= 1;
    }));
    QThread::msleep(100);

    inputs = recorder.inputSnapshot();
    CHECK(countKey(inputs, hyremote::KeyCode::Shift, false) == 1);
    CHECK(countKey(inputs, hyremote::KeyCode::B, false) == 1);
    CHECK(countKey(inputs, hyremote::KeyCode::C, false) == 1);
    CHECK(countButton(inputs, hyremote::PointerButton::Left, false) == 1);

    const auto bRelease = std::find_if(inputs.begin(), inputs.end(), [](const auto &event) {
        return event.kind == hyremote::InputEventKind::Key && event.key == hyremote::KeyCode::B
               && !event.pressed;
    });
    CHECK(bRelease != inputs.end());
    if (bRelease != inputs.end())
        CHECK(hyremote::hasModifier(bRelease->modifiers, hyremote::InputModifier::Shift));

    const auto cRelease = std::find_if(inputs.begin(), inputs.end(), [](const auto &event) {
        return event.kind == hyremote::InputEventKind::Key && event.key == hyremote::KeyCode::C
               && !event.pressed;
    });
    CHECK(cRelease != inputs.end());
    if (cRelease != inputs.end())
        CHECK(hyremote::hasModifier(cRelease->modifiers, hyremote::InputModifier::Shift));

    const auto leftRelease = std::find_if(inputs.begin(), inputs.end(), [](const auto &event) {
        return event.kind == hyremote::InputEventKind::PointerButton
               && event.button == hyremote::PointerButton::Left && !event.pressed;
    });
    CHECK(leftRelease != inputs.end());
    if (leftRelease != inputs.end()) {
        CHECK(leftRelease->x == 22.0F);
        CHECK(leftRelease->y == 21.0F);
        CHECK(hyremote::hasModifier(leftRelease->modifiers, hyremote::InputModifier::Shift));
    }

    const auto shiftRelease = std::find_if(inputs.begin(), inputs.end(), [](const auto &event) {
        return event.kind == hyremote::InputEventKind::Key && event.key == hyremote::KeyCode::Shift
               && !event.pressed;
    });
    CHECK(shiftRelease != inputs.end());
    if (shiftRelease != inputs.end())
        CHECK(!hyremote::hasModifier(shiftRelease->modifiers, hyremote::InputModifier::Shift));

    second.disconnectFromHost();
    if (second.state() != QAbstractSocket::UnconnectedState)
        second.waitForDisconnected(1000);
    transport->stop();
}

}  // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    testConcurrentViewerHeldStateIsolation();
    if (failures != 0)
        std::cerr << failures << " concurrent-viewer RFB input checks failed\n";
    return failures == 0 ? 0 : 1;
}
