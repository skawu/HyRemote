#include <QByteArray>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
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

bool writeBytewise(QTcpSocket &socket, const QByteArray &data)
{
    for (char byte : data) {
        if (socket.write(&byte, 1) != 1)
            return false;
        socket.flush();
        if (!socket.waitForBytesWritten(3000) && socket.bytesToWrite() != 0)
            return false;
    }
    return true;
}

quint16 freePort()
{
    QTcpServer probe;
    if (!probe.listen(QHostAddress::LocalHost, 0))
        return 0;
    return probe.serverPort();
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
};

bool connectRawRfb(QTcpSocket &socket, quint16 port, bool fragmentedVersion)
{
    socket.connectToHost(QHostAddress::LocalHost, port);
    if (!socket.waitForConnected(3000))
        return false;

    if (readExact(socket, 12) != QByteArray("RFB 003.008\n", 12))
        return false;
    const QByteArray version("RFB 003.008\n", 12);
    if (!(fragmentedVersion ? writeBytewise(socket, version) : writeAll(socket, version)))
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

bool waitForDisconnected(QTcpSocket &socket)
{
    if (socket.state() != QAbstractSocket::UnconnectedState)
        socket.waitForDisconnected(3000);
    return socket.state() == QAbstractSocket::UnconnectedState;
}

bool hasRecoverableFailure(const std::vector<hyremote::TransportEvent> &events,
                           const std::string &needle)
{
    return std::any_of(events.begin(), events.end(), [&](const auto &event) {
        return event.code == hyremote::TransportEventCode::RecoverableFailure
               && event.message.find(needle) != std::string::npos;
    });
}

bool primeInitialFrame(hyremote::Transport &transport)
{
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
    frame.damage = hyremote::Damage::fullFrame();
    transport.enqueueFrame(std::move(frame));
    return true;
}

void testFragmentedHandshakeAndInput()
{
    const quint16 port = freePort();
    CHECK(port != 0);
    if (port == 0)
        return;

    Recorder recorder;
    auto transport = HyRemote::detail::createRfbTransport(QHostAddress::LocalHost, port,
                                                         HyRemote::detail::RfbSecurityConfig{});
    CHECK(transport != nullptr);
    if (!transport)
        return;
    CHECK(transport->start([&](const hyremote::InputEvent &event) { recorder.record(event); },
                           [&](const hyremote::TransportEvent &event) { recorder.record(event); }));
    CHECK(primeInitialFrame(*transport));

    QTcpSocket socket;
    CHECK(connectRawRfb(socket, port, true));
    CHECK(recorder.waitFor([](const auto &, const auto &events) {
        return std::any_of(events.begin(), events.end(), [](const auto &event) {
            return event.code == hyremote::TransportEventCode::ClientConnected;
        });
    }));

    QByteArray keyDown;
    keyDown.append(char(4));
    keyDown.append(char(1));
    appendU16(keyDown, 0);
    appendU32(keyDown, static_cast<std::uint32_t>('a'));
    CHECK(writeBytewise(socket, keyDown));

    QByteArray pointer;
    pointer.append(char(5));
    pointer.append(char(1));
    appendU16(pointer, 17);
    appendU16(pointer, 19);
    CHECK(writeBytewise(socket, pointer));

    CHECK(recorder.waitFor([](const auto &inputs, const auto &) {
        const bool key = std::any_of(inputs.begin(), inputs.end(), [](const auto &event) {
            return event.kind == hyremote::InputEventKind::Key && event.key == hyremote::KeyCode::A
                   && event.pressed;
        });
        const bool button = std::any_of(inputs.begin(), inputs.end(), [](const auto &event) {
            return event.kind == hyremote::InputEventKind::PointerButton
                   && event.button == hyremote::PointerButton::Left && event.pressed;
        });
        return key && button;
    }));

    socket.disconnectFromHost();
    waitForDisconnected(socket);
    transport->stop();
}

void testOversizedSetEncodingsFailsClosed()
{
    const quint16 port = freePort();
    CHECK(port != 0);
    if (port == 0)
        return;

    Recorder recorder;
    auto transport = HyRemote::detail::createRfbTransport(QHostAddress::LocalHost, port,
                                                         HyRemote::detail::RfbSecurityConfig{});
    CHECK(transport != nullptr);
    if (!transport)
        return;
    CHECK(transport->start([&](const hyremote::InputEvent &event) { recorder.record(event); },
                           [&](const hyremote::TransportEvent &event) { recorder.record(event); }));
    CHECK(primeInitialFrame(*transport));

    QTcpSocket socket;
    CHECK(connectRawRfb(socket, port, false));
    QByteArray message;
    message.append(char(2));
    message.append(char(0));
    appendU16(message, 1025);
    CHECK(writeAll(socket, message));
    CHECK(recorder.waitFor([](const auto &, const auto &events) {
        return hasRecoverableFailure(events, "too many encodings");
    }));
    CHECK(waitForDisconnected(socket));
    transport->stop();
}

void testOversizedCutTextFailsClosed()
{
    const quint16 port = freePort();
    CHECK(port != 0);
    if (port == 0)
        return;

    Recorder recorder;
    auto transport = HyRemote::detail::createRfbTransport(QHostAddress::LocalHost, port,
                                                         HyRemote::detail::RfbSecurityConfig{});
    CHECK(transport != nullptr);
    if (!transport)
        return;
    CHECK(transport->start([&](const hyremote::InputEvent &event) { recorder.record(event); },
                           [&](const hyremote::TransportEvent &event) { recorder.record(event); }));
    CHECK(primeInitialFrame(*transport));

    QTcpSocket socket;
    CHECK(connectRawRfb(socket, port, false));
    QByteArray message;
    message.append(char(6));
    message.append("\0\0\0", 3);
    appendU32(message, 64U * 1024U + 1U);
    CHECK(writeAll(socket, message));
    CHECK(recorder.waitFor([](const auto &, const auto &events) {
        return hasRecoverableFailure(events, "cut-text payload exceeded the bounded limit");
    }));
    CHECK(waitForDisconnected(socket));
    transport->stop();
}

void testUnsupportedMessageFailsClosedAndNextClientRecovers()
{
    const quint16 port = freePort();
    CHECK(port != 0);
    if (port == 0)
        return;

    Recorder recorder;
    auto transport = HyRemote::detail::createRfbTransport(QHostAddress::LocalHost, port,
                                                         HyRemote::detail::RfbSecurityConfig{});
    CHECK(transport != nullptr);
    if (!transport)
        return;
    CHECK(transport->start([&](const hyremote::InputEvent &event) { recorder.record(event); },
                           [&](const hyremote::TransportEvent &event) { recorder.record(event); }));
    CHECK(primeInitialFrame(*transport));

    QTcpSocket rejected;
    CHECK(connectRawRfb(rejected, port, false));
    CHECK(writeAll(rejected, QByteArray(1, char(0x7f))));
    CHECK(recorder.waitFor([](const auto &, const auto &events) {
        return hasRecoverableFailure(events, "unsupported protocol message");
    }));
    CHECK(waitForDisconnected(rejected));

    QTcpSocket recovery;
    CHECK(connectRawRfb(recovery, port, true));
    CHECK(recovery.state() == QAbstractSocket::ConnectedState);
    recovery.disconnectFromHost();
    waitForDisconnected(recovery);
    transport->stop();
}

void testBoundedInputBurstFailsClosed()
{
    const quint16 port = freePort();
    CHECK(port != 0);
    if (port == 0)
        return;

    Recorder recorder;
    auto transport = HyRemote::detail::createRfbTransport(QHostAddress::LocalHost, port,
                                                         HyRemote::detail::RfbSecurityConfig{});
    CHECK(transport != nullptr);
    if (!transport)
        return;
    CHECK(transport->start([&](const hyremote::InputEvent &event) { recorder.record(event); },
                           [&](const hyremote::TransportEvent &event) { recorder.record(event); }));

    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, port);
    CHECK(socket.waitForConnected(3000));
    QByteArray burst(256 * 1024 + 1, char('x'));
    const qint64 queued = socket.write(burst);
    CHECK(queued == burst.size());
    socket.flush();
    CHECK(recorder.waitFor([](const auto &, const auto &events) {
        return hasRecoverableFailure(events, "bounded protocol buffer");
    }));
    CHECK(waitForDisconnected(socket));
    transport->stop();
}

}  // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    testFragmentedHandshakeAndInput();
    testOversizedSetEncodingsFailsClosed();
    testOversizedCutTextFailsClosed();
    testUnsupportedMessageFailsClosedAndNextClientRecovers();
    testBoundedInputBurstFailsClosed();

    if (failures != 0)
        std::cerr << failures << " RFB wire-robustness checks failed\n";
    return failures == 0 ? 0 : 1;
}
