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
#include <utility>
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

void appendS32(QByteArray &data, std::int32_t value)
{
    appendU32(data, static_cast<std::uint32_t>(value));
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

std::int32_t readS32(const QByteArray &data, qsizetype offset)
{
    return static_cast<std::int32_t>(readU32(data, offset));
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

bool negotiateThroughSecurity(QTcpSocket &socket, quint16 port, bool fragmentedVersion)
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
    return securityResult.size() == 4 && readU32(securityResult, 0) == 0U;
}

bool connectRawRfb(QTcpSocket &socket,
                   quint16 port,
                   bool fragmentedVersion,
                   std::uint16_t *framebufferWidth = nullptr,
                   std::uint16_t *framebufferHeight = nullptr)
{
    if (!negotiateThroughSecurity(socket, port, fragmentedVersion))
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

bool enterAwaitInitialFrame(QTcpSocket &socket, quint16 port)
{
    if (!negotiateThroughSecurity(socket, port, false))
        return false;
    // ClientInit moves the worker into AwaitInitialFrame. With no frame enqueued in this test,
    // subsequent bytes are deliberately retained instead of interpreted as normal messages.
    return writeAll(socket, QByteArray(1, char(1)));
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

hyremote::RemoteFrame makeFrame(std::uint32_t width,
                                std::uint32_t height,
                                std::uint32_t background,
                                const std::vector<std::pair<hyremote::Rect, std::uint32_t>> &overlays = {})
{
    auto storage = hyremote::CpuFrameStorage::createSinglePlane(width * 4U, height);
    if (!storage)
        return {};
    auto *bytes = reinterpret_cast<std::uint8_t *>(storage->mutablePlane(0));
    if (!bytes)
        return {};

    const auto putPixel = [&](std::uint32_t x, std::uint32_t y, std::uint32_t rgb) {
        auto *pixel = bytes + (static_cast<std::size_t>(y) * width + x) * 4U;
        pixel[0] = static_cast<std::uint8_t>((rgb >> 16U) & 0xffU);
        pixel[1] = static_cast<std::uint8_t>((rgb >> 8U) & 0xffU);
        pixel[2] = static_cast<std::uint8_t>(rgb & 0xffU);
        pixel[3] = 0xffU;
    };
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x)
            putPixel(x, y, background);
    }
    for (const auto &[rect, color] : overlays) {
        const std::uint32_t x0 = rect.x < 0 ? 0U : static_cast<std::uint32_t>(rect.x);
        const std::uint32_t y0 = rect.y < 0 ? 0U : static_cast<std::uint32_t>(rect.y);
        const std::uint32_t x1 = std::min(width, x0 + rect.width);
        const std::uint32_t y1 = std::min(height, y0 + rect.height);
        for (std::uint32_t y = y0; y < y1; ++y) {
            for (std::uint32_t x = x0; x < x1; ++x)
                putPixel(x, y, color);
        }
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
    return frame;
}

bool primeInitialFrame(hyremote::Transport &transport)
{
    auto frame = makeFrame(64, 48, 0x203040U);
    if (!frame.storage)
        return false;
    transport.enqueueFrame(std::move(frame));
    return true;
}

bool sendSetEncodings(QTcpSocket &socket, const std::vector<std::int32_t> &encodings)
{
    QByteArray message;
    message.append(char(2));
    message.append(char(0));
    appendU16(message, static_cast<std::uint16_t>(encodings.size()));
    for (std::int32_t encoding : encodings)
        appendS32(message, encoding);
    return writeAll(socket, message);
}

bool sendUpdateRequest(QTcpSocket &socket,
                       bool incremental,
                       std::uint16_t x,
                       std::uint16_t y,
                       std::uint16_t width,
                       std::uint16_t height)
{
    QByteArray message;
    message.append(char(3));
    message.append(static_cast<char>(incremental ? 1 : 0));
    appendU16(message, x);
    appendU16(message, y);
    appendU16(message, width);
    appendU16(message, height);
    return writeAll(socket, message);
}

struct TestFramebuffer
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

struct UpdateRect
{
    std::uint16_t x = 0;
    std::uint16_t y = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::int32_t encoding = 0;
};

std::uint32_t readCpixel(const QByteArray &bytes, qsizetype offset)
{
    return (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes.at(offset))) << 16U)
           | (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes.at(offset + 1))) << 8U)
           | static_cast<std::uint32_t>(static_cast<unsigned char>(bytes.at(offset + 2)));
}

bool decodeTrle(QTcpSocket &socket, TestFramebuffer &frame, const UpdateRect &rect)
{
    for (std::uint32_t tileY = 0; tileY < rect.height; tileY += 16U) {
        const std::uint32_t tileHeight = std::min<std::uint32_t>(16U, rect.height - tileY);
        for (std::uint32_t tileX = 0; tileX < rect.width; tileX += 16U) {
            const std::uint32_t tileWidth = std::min<std::uint32_t>(16U, rect.width - tileX);
            const QByteArray subencodingBytes = readExact(socket, 1);
            if (subencodingBytes.size() != 1)
                return false;
            const std::uint8_t subencoding
                = static_cast<std::uint8_t>(static_cast<unsigned char>(subencodingBytes.at(0)));

            if (subencoding == 0U) {
                const QByteArray raw = readExact(
                    socket, static_cast<qsizetype>(tileWidth * tileHeight * 3U));
                if (raw.size() != static_cast<qsizetype>(tileWidth * tileHeight * 3U))
                    return false;
                qsizetype offset = 0;
                for (std::uint32_t y = 0; y < tileHeight; ++y) {
                    for (std::uint32_t x = 0; x < tileWidth; ++x) {
                        frame.set(static_cast<std::uint16_t>(rect.x + tileX + x),
                                  static_cast<std::uint16_t>(rect.y + tileY + y),
                                  readCpixel(raw, offset));
                        offset += 3;
                    }
                }
                continue;
            }

            if (subencoding == 1U) {
                const QByteArray color = readExact(socket, 3);
                if (color.size() != 3)
                    return false;
                const std::uint32_t rgb = readCpixel(color, 0);
                for (std::uint32_t y = 0; y < tileHeight; ++y) {
                    for (std::uint32_t x = 0; x < tileWidth; ++x)
                        frame.set(static_cast<std::uint16_t>(rect.x + tileX + x),
                                  static_cast<std::uint16_t>(rect.y + tileY + y),
                                  rgb);
                }
                continue;
            }

            if (subencoding < 2U || subencoding > 16U)
                return false;
            const std::size_t paletteSize = subencoding;
            const QByteArray paletteBytes = readExact(
                socket, static_cast<qsizetype>(paletteSize * 3U));
            if (paletteBytes.size() != static_cast<qsizetype>(paletteSize * 3U))
                return false;
            std::vector<std::uint32_t> palette(paletteSize);
            for (std::size_t i = 0; i < paletteSize; ++i)
                palette[i] = readCpixel(paletteBytes, static_cast<qsizetype>(i * 3U));

            const int bits = paletteSize == 2U ? 1 : (paletteSize <= 4U ? 2 : 4);
            const std::uint32_t rowBytes = (tileWidth * static_cast<std::uint32_t>(bits) + 7U) / 8U;
            for (std::uint32_t y = 0; y < tileHeight; ++y) {
                const QByteArray packed = readExact(socket, static_cast<qsizetype>(rowBytes));
                if (packed.size() != static_cast<qsizetype>(rowBytes))
                    return false;
                for (std::uint32_t x = 0; x < tileWidth; ++x) {
                    const std::uint32_t bit = x * static_cast<std::uint32_t>(bits);
                    const std::uint32_t byteIndex = bit / 8U;
                    const int shift = 8 - bits - static_cast<int>(bit % 8U);
                    const std::uint8_t byte
                        = static_cast<std::uint8_t>(static_cast<unsigned char>(packed.at(byteIndex)));
                    const std::uint8_t index
                        = static_cast<std::uint8_t>((byte >> shift) & ((1U << bits) - 1U));
                    if (index >= palette.size())
                        return false;
                    frame.set(static_cast<std::uint16_t>(rect.x + tileX + x),
                              static_cast<std::uint16_t>(rect.y + tileY + y),
                              palette[index]);
                }
            }
        }
    }
    return true;
}

bool readFramebufferUpdate(QTcpSocket &socket,
                           TestFramebuffer &frame,
                           std::vector<UpdateRect> &rectangles)
{
    rectangles.clear();
    const QByteArray header = readExact(socket, 4);
    if (header.size() != 4 || header.at(0) != char(0))
        return false;
    const std::uint16_t count = readU16(header, 2);
    for (std::uint16_t index = 0; index < count; ++index) {
        const QByteArray rectBytes = readExact(socket, 12);
        if (rectBytes.size() != 12)
            return false;
        UpdateRect rect;
        rect.x = readU16(rectBytes, 0);
        rect.y = readU16(rectBytes, 2);
        rect.width = readU16(rectBytes, 4);
        rect.height = readU16(rectBytes, 6);
        rect.encoding = readS32(rectBytes, 8);
        rectangles.push_back(rect);

        if (rect.encoding == -223) {
            frame.resize(rect.width, rect.height);
            continue;
        }
        if (rect.encoding == 0) {
            const qsizetype size = static_cast<qsizetype>(rect.width) * rect.height * 4;
            const QByteArray raw = readExact(socket, size);
            if (raw.size() != size)
                return false;
            qsizetype offset = 0;
            for (std::uint32_t y = 0; y < rect.height; ++y) {
                for (std::uint32_t x = 0; x < rect.width; ++x) {
                    const std::uint32_t rgb
                        = (static_cast<std::uint32_t>(static_cast<unsigned char>(raw.at(offset))) << 16U)
                          | (static_cast<std::uint32_t>(static_cast<unsigned char>(raw.at(offset + 1))) << 8U)
                          | static_cast<std::uint32_t>(static_cast<unsigned char>(raw.at(offset + 2)));
                    frame.set(static_cast<std::uint16_t>(rect.x + x),
                              static_cast<std::uint16_t>(rect.y + y),
                              rgb);
                    offset += 4;
                }
            }
            continue;
        }
        if (rect.encoding == 15) {
            if (!decodeTrle(socket, frame, rect))
                return false;
            continue;
        }
        return false;
    }
    return true;
}

bool noFramebufferData(QTcpSocket &socket, int timeoutMs = 150)
{
    if (socket.bytesAvailable() != 0)
        return false;
    return !socket.waitForReadyRead(timeoutMs) && socket.bytesAvailable() == 0;
}

std::unique_ptr<hyremote::Transport> startTransport(quint16 port,
                                                    Recorder &recorder,
                                                    hyremote::RemoteFrame initial)
{
    auto transport = HyRemote::detail::createRfbTransport(QHostAddress::LocalHost, port,
                                                          HyRemote::detail::RfbSecurityConfig{});
    if (!transport)
        return {};
    if (!transport->start([&](const hyremote::InputEvent &event) { recorder.record(event); },
                          [&](const hyremote::TransportEvent &event) { recorder.record(event); })) {
        return {};
    }
    transport->enqueueFrame(std::move(initial));
    return transport;
}

void testTrleRawAndIncrementalDelivery()
{
    const quint16 port = freePort();
    CHECK(port != 0);
    if (port == 0)
        return;

    Recorder recorder;
    auto transport = startTransport(port, recorder, makeFrame(128, 128, 0x203040U));
    CHECK(transport != nullptr);
    if (!transport)
        return;

    QTcpSocket trleSocket;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    CHECK(connectRawRfb(trleSocket, port, false, &width, &height));
    CHECK(width == 128 && height == 128);
    CHECK(sendSetEncodings(trleSocket, {15, 0}));
    CHECK(sendUpdateRequest(trleSocket, false, 0, 0, width, height));
    TestFramebuffer trleFrame;
    trleFrame.resize(width, height);
    std::vector<UpdateRect> rectangles;
    CHECK(readFramebufferUpdate(trleSocket, trleFrame, rectangles));
    CHECK(rectangles.size() == 1U);
    CHECK(rectangles.size() == 1U && rectangles.front().encoding == 15);
    CHECK(trleFrame.get(0, 0) == 0x203040U);

    // A static incremental request remains pending without sending duplicate pixels.
    CHECK(sendUpdateRequest(trleSocket, true, 0, 0, width, height));
    CHECK(noFramebufferData(trleSocket));
    transport->enqueueFrame(makeFrame(128,
                                      128,
                                      0x203040U,
                                      {{{70, 10, 8, 8}, 0xe0a030U}}));
    CHECK(readFramebufferUpdate(trleSocket, trleFrame, rectangles));
    CHECK(rectangles.size() == 1U);
    CHECK(rectangles.size() == 1U && rectangles.front().encoding == 15);
    CHECK(rectangles.size() == 1U && rectangles.front().x == 64 && rectangles.front().y == 0
          && rectangles.front().width == 64 && rectangles.front().height == 64);
    CHECK(trleFrame.get(70, 10) == 0xe0a030U);
    CHECK(trleFrame.get(0, 0) == 0x203040U);

    QTcpSocket rawSocket;
    CHECK(connectRawRfb(rawSocket, port, false, &width, &height));
    CHECK(sendSetEncodings(rawSocket, {0, 15}));
    CHECK(sendUpdateRequest(rawSocket, false, 0, 0, width, height));
    TestFramebuffer rawFrame;
    rawFrame.resize(width, height);
    CHECK(readFramebufferUpdate(rawSocket, rawFrame, rectangles));
    CHECK(rectangles.size() == 1U);
    CHECK(rectangles.size() == 1U && rectangles.front().encoding == 0);
    CHECK(rawFrame.get(70, 10) == 0xe0a030U);

    trleSocket.disconnectFromHost();
    rawSocket.disconnectFromHost();
    waitForDisconnected(trleSocket);
    waitForDisconnected(rawSocket);
    transport->stop();
}

void testPartialIncrementalAndResize()
{
    const quint16 port = freePort();
    CHECK(port != 0);
    if (port == 0)
        return;

    Recorder recorder;
    auto transport = startTransport(port, recorder, makeFrame(256, 128, 0x101820U));
    CHECK(transport != nullptr);
    if (!transport)
        return;

    QTcpSocket socket;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    CHECK(connectRawRfb(socket, port, false, &width, &height));
    CHECK(sendSetEncodings(socket, {-223, 15, 0}));
    CHECK(sendUpdateRequest(socket, false, 0, 0, width, height));
    TestFramebuffer frame;
    frame.resize(width, height);
    std::vector<UpdateRect> rectangles;
    CHECK(readFramebufferUpdate(socket, frame, rectangles));

    transport->enqueueFrame(makeFrame(256,
                                      128,
                                      0x101820U,
                                      {{{8, 8, 8, 8}, 0xaa3300U},
                                       {{200, 8, 8, 8}, 0x00aa66U}}));
    CHECK(sendUpdateRequest(socket, true, 0, 0, 64, 64));
    CHECK(readFramebufferUpdate(socket, frame, rectangles));
    CHECK(rectangles.size() == 1U && rectangles.front().x == 0
          && rectangles.front().width == 64);
    CHECK(frame.get(8, 8) == 0xaa3300U);
    CHECK(frame.get(200, 8) != 0x00aa66U);

    CHECK(sendUpdateRequest(socket, true, 192, 0, 64, 64));
    CHECK(readFramebufferUpdate(socket, frame, rectangles));
    CHECK(rectangles.size() == 1U && rectangles.front().x == 192
          && rectangles.front().width == 64);
    CHECK(frame.get(200, 8) == 0x00aa66U);

    transport->enqueueFrame(makeFrame(80, 60, 0x445566U));
    CHECK(sendUpdateRequest(socket, true, 0, 0, 80, 60));
    CHECK(readFramebufferUpdate(socket, frame, rectangles));
    CHECK(rectangles.size() == 2U);
    CHECK(rectangles.size() == 2U && rectangles.front().encoding == -223);
    CHECK(rectangles.size() == 2U && rectangles.back().encoding == 15);
    CHECK(frame.width == 80 && frame.height == 60);
    CHECK(frame.get(0, 0) == 0x445566U);

    socket.disconnectFromHost();
    waitForDisconnected(socket);
    transport->stop();
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
    CHECK(enterAwaitInitialFrame(socket, port));
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

    testTrleRawAndIncrementalDelivery();
    testPartialIncrementalAndResize();
    testFragmentedHandshakeAndInput();
    testOversizedSetEncodingsFailsClosed();
    testOversizedCutTextFailsClosed();
    testUnsupportedMessageFailsClosedAndNextClientRecovers();
    testBoundedInputBurstFailsClosed();

    if (failures != 0)
        std::cerr << failures << " RFB wire-robustness checks failed\n";
    return failures == 0 ? 0 : 1;
}
