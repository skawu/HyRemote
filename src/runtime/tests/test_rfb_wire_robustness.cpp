#include <QByteArray>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSysInfo>
#include <QTcpServer>
#include <QTcpSocket>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <psapi.h>
#  ifdef _MSC_VER
#    pragma comment(lib, "Psapi.lib")
#  endif
#else
#  include <unistd.h>
#endif

#include "hyremote/core/frame.hpp"
#include "hyremote/core/input.hpp"
#include "../src/transport/rfb_trle.hpp"
#include "hyremote/core/storage.hpp"
#include "hyremote/core/transport.hpp"
#include "rfb_interaction_latency_stats.hpp"
#include "transport/rfb_delivery_state.hpp"
#include "transport/rfb_transport.hpp"
#include "detail/viewer_capture_demand.hpp"

#ifdef HYREMOTE_TEST_QUICK_RFB
#include <QGuiApplication>
#include <QQuickWindow>
#include "detail/component_factories.hpp"
#include "hyremote/core/session.hpp"
#include "staged_quick_probe_item.hpp"
#endif

namespace {

int failures = 0;
qsizetype profileBytesRead = 0;

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
        const QByteArray chunk = socket.read(size - result.size());
        result += chunk;
        profileBytesRead += chunk.size();
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

// #158: a slot is only free after the deferred abort has been delivered, so a reconnect immediately after
// an expiry can still be refused by the bounded client limit. Retry inside a bounded window instead of
// treating that ordering as a product failure.
bool connectRawRfbWithRetry(QTcpSocket &socket, quint16 port, int timeoutMs = 5000)
{
    QElapsedTimer window;
    window.start();
    while (window.elapsed() < timeoutMs) {
        if (connectRawRfb(socket, port, false))
            return true;
        socket.abort();
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
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

std::size_t countRecoverableFailures(Recorder &recorder, const std::string &needle)
{
    std::lock_guard<std::mutex> lock(recorder.mutex);
    return static_cast<std::size_t>(
        std::count_if(recorder.events.begin(), recorder.events.end(), [&](const auto &event) {
            return event.code == hyremote::TransportEventCode::RecoverableFailure
                   && event.message.find(needle) != std::string::npos;
        }));
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

bool sendPixelFormat(QTcpSocket &socket, char bitsPerPixel, char depth, char redMax, char greenMax, char blueMax,
                     char redShift, char greenShift, char blueShift)
{
    QByteArray message;
    message.append(char(0));  // SetPixelFormat
    message.append("\0\0\0", 3);
    message.append(bitsPerPixel);
    message.append(depth);
    message.append(char(0));  // little-endian
    message.append(char(1));  // true colour
    appendU16(message, static_cast<std::uint8_t>(redMax));
    appendU16(message, static_cast<std::uint8_t>(greenMax));
    appendU16(message, static_cast<std::uint8_t>(blueMax));
    message.append(redShift);
    message.append(greenShift);
    message.append(blueShift);
    message.append("\0\0\0", 3);
    return writeAll(socket, message);
}

bool sendNativePixelFormat(QTcpSocket &socket)
{
    return sendPixelFormat(socket, 32, 24, char(255), char(255), char(255), 0, 8, 16);
}

bool sendRgb222PixelFormat(QTcpSocket &socket)
{
    return sendPixelFormat(socket, 8, 6, 3, 3, 3, 4, 2, 0);
}

QByteArray makeFence(std::uint32_t flags, const QByteArray &payload = {})
{
    QByteArray message;
    message.append(char(248));
    message.append("\0\0\0", 3);
    appendU32(message, flags);
    message.append(static_cast<char>(payload.size()));
    message += payload;
    return message;
}

QByteArray readFence(QTcpSocket &socket)
{
    QByteArray header = readExact(socket, 9);
    if (header.size() != 9)
        return {};
    const qsizetype length = static_cast<unsigned char>(header.at(8));
    header += readExact(socket, length);
    return header;
}

bool sendContinuousUpdates(QTcpSocket &socket,
                           bool enable,
                           std::uint16_t x,
                           std::uint16_t y,
                           std::uint16_t width,
                           std::uint16_t height)
{
    QByteArray message;
    message.append(char(150));
    message.append(static_cast<char>(enable ? 1 : 0));
    appendU16(message, x);
    appendU16(message, y);
    appendU16(message, width);
    appendU16(message, height);
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

bool sendPointerButton(QTcpSocket &socket,
                       bool pressed,
                       std::uint16_t x = 17,
                       std::uint16_t y = 19)
{
    QByteArray pointer;
    pointer.append(char(5));
    pointer.append(static_cast<char>(pressed ? 1 : 0));
    appendU16(pointer, x);
    appendU16(pointer, y);
    return writeAll(socket, pointer);
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

bool readFramebufferUpdateBody(QTcpSocket &socket,
                               const QByteArray &header,
                               TestFramebuffer &frame,
                               std::vector<UpdateRect> &rectangles,
                               int timeoutMs = 3000)
{
    rectangles.clear();
    const std::uint16_t count = readU16(header, 2);
    for (std::uint16_t index = 0; index < count; ++index) {
        const QByteArray rectBytes = readExact(socket, 12, timeoutMs);
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
            const QByteArray raw = readExact(socket, size, timeoutMs);
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

bool readFramebufferUpdate(QTcpSocket &socket,
                           TestFramebuffer &frame,
                           std::vector<UpdateRect> &rectangles)
{
    rectangles.clear();
    const QByteArray header = readExact(socket, 4);
    if (header.size() != 4 || header.at(0) != char(0))
        return false;
    return readFramebufferUpdateBody(socket, header, frame, rectangles);
}

// EndOfContinuousUpdates (150) is a server-to-client message, not a promise
// that it is the next byte on the wire. A viewer that already had an
// outstanding update may legitimately receive a queued FramebufferUpdate
// before the acknowledgment, so consume complete updates until the marker
// arrives instead of asserting a particular socket interleaving.
bool readEndOfContinuousUpdates(QTcpSocket &socket, int timeoutMs = 3000)
{
    TestFramebuffer discarded;
    std::vector<UpdateRect> rectangles;
    QElapsedTimer timer;
    timer.start();
    while (true) {
        const int remaining = timeoutMs - static_cast<int>(timer.elapsed());
        if (remaining <= 0)
            return false;
        const QByteArray type = readExact(socket, 1, remaining);
        if (type.size() != 1)
            return false;
        const auto value = static_cast<unsigned char>(type.at(0));
        if (value == 150)
            return true;
        if (value != 0)
            return false;
        const QByteArray rest = readExact(socket, 3, remaining);
        if (rest.size() != 3)
            return false;
        QByteArray header(1, char(0));
        header += rest;
        if (!readFramebufferUpdateBody(socket, header, discarded, rectangles, remaining))
            return false;
    }
}

bool noFramebufferData(QTcpSocket &socket, int timeoutMs = 150)
{
    if (socket.bytesAvailable() != 0)
        return false;
    return !socket.waitForReadyRead(timeoutMs) && socket.bytesAvailable() == 0;
}

bool readCursorUpdate(QTcpSocket &socket,
                      int *widthOut = nullptr,
                      int *heightOut = nullptr,
                      int *hotspotXOut = nullptr,
                      int *hotspotYOut = nullptr,
                      int timeoutMs = 3000)
{
    const QByteArray header = readExact(socket, 4, timeoutMs);
    if (header.size() != 4 || header.at(0) != char(0) || readU16(header, 2) != 1)
        return false;
    const QByteArray rectangle = readExact(socket, 12, timeoutMs);
    if (rectangle.size() != 12 || readS32(rectangle, 8) != -239)
        return false;
    const std::uint16_t width = readU16(rectangle, 4);
    const std::uint16_t height = readU16(rectangle, 6);
    if (width == 0 || height == 0)
        return false;
    if (widthOut)
        *widthOut = width;
    if (heightOut)
        *heightOut = height;
    if (hotspotXOut)
        *hotspotXOut = readU16(rectangle, 0);
    if (hotspotYOut)
        *hotspotYOut = readU16(rectangle, 2);
    const qsizetype pixels = static_cast<qsizetype>(width) * height * 4;
    const qsizetype mask = ((width + 7) / 8) * height;
    return readExact(socket, pixels + mask, timeoutMs).size() == pixels + mask;
}

std::unique_ptr<hyremote::Transport> startTransport(
    quint16 port,
    Recorder &recorder,
    hyremote::RemoteFrame initial,
    HyRemote::detail::RfbCursorProvider cursorProvider = {},
    HyRemote::detail::RfbFrameDecorator frameDecorator = {},
    std::function<void(bool)> frameChangeObserver = {},
    std::function<void(bool, bool)> frameBackpressureObserver = {},
    std::function<void()> frameFullRefreshObserver = {})
{
    HyRemote::detail::RfbSecurityConfig security;
    security.cursorProvider = std::move(cursorProvider);
    security.frameDecorator = std::move(frameDecorator);
    security.frameChangeObserver = std::move(frameChangeObserver);
    security.frameBackpressureObserver = std::move(frameBackpressureObserver);
    security.frameFullRefreshObserver = std::move(frameFullRefreshObserver);
    auto transport = HyRemote::detail::createRfbTransport(QHostAddress::LocalHost, port,
                                                          std::move(security));
    if (!transport)
        return {};
    if (!transport->start([&](const hyremote::InputEvent &event) { recorder.record(event); },
                          [&](const hyremote::TransportEvent &event) { recorder.record(event); })) {
        return {};
    }
    transport->enqueueFrame(std::move(initial));
    return transport;
}

// #609: exercise the real RFB-worker → producer-pressure callback with an
// actual RFB 3.8 socket, rather than assuming ViewerCaptureDemand unit-test
// notifications are wired correctly. Requests and Qt output-drain transitions
// must be reported separately; all observed state is recorded off the worker.
void testRfbConsumerPressureAndFullRequestWake()
{
    struct Trace
    {
        std::mutex mutex;
        std::condition_variable changed;
        std::vector<std::pair<bool, bool>> states;
        int fullRefreshRequests = 0;

        void pressure(bool blocked, bool noDemand)
        {
            {
                std::lock_guard<std::mutex> lock(mutex);
                states.emplace_back(blocked, noDemand);
            }
            changed.notify_all();
        }

        void fullRequest()
        {
            {
                std::lock_guard<std::mutex> lock(mutex);
                ++fullRefreshRequests;
            }
            changed.notify_all();
        }

        int countPressure(bool blocked, bool noDemand)
        {
            std::lock_guard<std::mutex> lock(mutex);
            return static_cast<int>(
                std::count(states.begin(), states.end(),
                           std::make_pair(blocked, noDemand)));
        }

        bool waitForPressure(bool blocked, bool noDemand, int minOccurrences = 1)
        {
            std::unique_lock<std::mutex> lock(mutex);
            return changed.wait_for(lock, std::chrono::seconds(3), [&] {
                return std::count(states.begin(), states.end(),
                                  std::make_pair(blocked, noDemand))
                       >= minOccurrences;
            });
        }
    } trace;

    const quint16 port = freePort();
    CHECK(port != 0);
    if (port == 0)
        return;

    Recorder recorder;
    auto transport = startTransport(
        port, recorder, makeFrame(32, 24, 0x203040U), {}, {}, {},
        [&](bool blocked, bool noDemand) { trace.pressure(blocked, noDemand); },
        [&] { trace.fullRequest(); });
    CHECK(transport != nullptr);
    if (!transport)
        return;

    QTcpSocket socket;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    CHECK(connectRawRfb(socket, port, false, &width, &height));
    CHECK(width == 32 && height == 24);
    // A Normal Viewer with no FBU request must not demand 60fps captures.
    CHECK(trace.waitForPressure(true, true));
    CHECK(sendSetEncodings(socket, {0}));
    CHECK(sendUpdateRequest(socket, false, 0, 0, width, height));

    TestFramebuffer frame;
    frame.resize(width, height);
    std::vector<UpdateRect> rectangles;
    CHECK(readFramebufferUpdate(socket, frame, rectangles));
    CHECK(frame.get(0, 0) == 0x203040U);
    // A fresh explicit full request must unblock the producer even if the
    // pending current pixels can be served immediately from the cache.
    CHECK(trace.waitForPressure(false, false));
    CHECK(trace.waitForPressure(true, false));
    CHECK(trace.waitForPressure(true, true, 2));

    {
        std::lock_guard<std::mutex> lock(trace.mutex);
        CHECK(trace.fullRefreshRequests == 1);
    }

    // Continuous incremental request is a second independent wake source;
    // as no pixels changed, it remains pending while the producer is allowed
    // to capture its next frame. This test does not need to manufacture pixels.
    const int previouslyReady = trace.countPressure(false, false);
    CHECK(sendUpdateRequest(socket, true, 0, 0, width, height));
    CHECK(trace.waitForPressure(false, false, previouslyReady + 1));

    socket.disconnectFromHost();
    waitForDisconnected(socket);
    transport->stop();
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
    CHECK(sendSetEncodings(rawSocket, {0}));
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

// #589 W2 wire-level acceptance: the decorator runs on the transport's own copy before encoding, so every encoding a
// viewer can negotiate has to carry the composed pixels. These cases decode what actually arrived instead of trusting
// the composed buffer, and they check the control direction too: without a decorator nothing may look watermarked.
constexpr std::uint32_t kMarkColor = 0xFF00FFU;

HyRemote::detail::RfbFrameDecorator markDecorator()
{
    return [](const hyremote::RemoteFrame &source, hyremote::RemoteFrame &decorated, QString &error) {
        if (!source.storage) {
            error = QStringLiteral("no source storage");
            return false;
        }
        const auto plane = source.storage->mapRead(0);
        if (!plane || !plane->data) {
            error = QStringLiteral("no readable plane");
            return false;
        }
        const std::uint32_t width = source.geometry.size.width;
        const std::uint32_t height = source.geometry.size.height;
        if (width < 32U || height < 16U) {
            error = QStringLiteral("frame too small for the mark");
            return false;
        }
        auto storage = hyremote::CpuFrameStorage::createSinglePlane(plane->stride, height);
        if (!storage || !storage->mutablePlane(0)) {
            error = QStringLiteral("no target storage");
            return false;
        }
        std::memcpy(storage->mutablePlane(0), plane->data, plane->stride * height);
        auto *bytes = reinterpret_cast<std::uint8_t *>(storage->mutablePlane(0));

        // The mark colour depends on the capture's first pixel, so a later frame can change the *composed* pixels
        // while the capture under the mark stays identical. Without that, an incremental assertion cannot tell
        // "the composed frame was re-sent" from "the mark was already in my copy".
        const auto *firstPixel = bytes;
        const std::uint32_t firstRgb = (static_cast<std::uint32_t>(firstPixel[0]) << 16U)
                                       | (static_cast<std::uint32_t>(firstPixel[1]) << 8U)
                                       | static_cast<std::uint32_t>(firstPixel[2]);
        const std::uint32_t markColor = firstRgb == 0x102030U ? 0x00FF00U : kMarkColor;

        for (std::uint32_t y = height - 16U; y < height; ++y) {
            for (std::uint32_t x = width - 32U; x < width; ++x) {
                auto *pixel = bytes + (static_cast<std::size_t>(y) * width + x) * 4U;
                pixel[0] = static_cast<std::uint8_t>((markColor >> 16U) & 0xffU);
                pixel[1] = static_cast<std::uint8_t>((markColor >> 8U) & 0xffU);
                pixel[2] = static_cast<std::uint8_t>(markColor & 0xffU);
                pixel[3] = 0xffU;
            }
        }
        decorated = source;
        decorated.storage = std::move(storage);
        return true;
    };
}

// #790: the listener remains Running after a viewer leaves. A separate decorated storage
// must be freed by the *last socket* disconnect, while the latest raw bootstrap survives.

// #790: the actual user's configuration has NO watermark. Releasing only
// m_observedFrame/m_servedFrame would keep the raw 8.3MB SharedFrameState::latest
// alive, so exercise the Runtime-opt-in path with a raw storage weak reference.
void testProducerBackedIdleRawFrameRelease()
{
    const quint16 port = freePort();
    CHECK(port != 0);
    if (!port)
        return;

    Recorder recorder;
    HyRemote::detail::RfbSecurityConfig security;
    security.initialFramebufferWidth = 128;
    security.initialFramebufferHeight = 128;
    security.releaseIdleFrameBuffers = true;
    auto transport = HyRemote::detail::createRfbTransport(QHostAddress::LocalHost, port, security);
    CHECK(transport != nullptr);
    if (!transport)
        return;
    CHECK(transport->start([&](const hyremote::InputEvent &event) { recorder.record(event); },
                           [&](const hyremote::TransportEvent &event) { recorder.record(event); }));
    const auto released = [](const std::weak_ptr<const hyremote::FrameStorage> &storage) {
        QElapsedTimer timer;
        timer.start();
        while (!storage.expired() && timer.elapsed() < 2000)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        return storage.expired();
    };

    auto bootstrap = makeFrame(128, 128, 0x203040U);
    const std::weak_ptr<const hyremote::FrameStorage> firstStorage = bootstrap.storage;
    transport->enqueueFrame(std::move(bootstrap));
    CHECK(released(firstStorage)); // idle before a first Viewer needs no cached RGBA pixels

    for (int attempt = 0; attempt < 3; ++attempt) {
        QTcpSocket viewer;
        std::uint16_t width = 0, height = 0;
        const bool joined = connectRawRfb(viewer, port, false, &width, &height);
        CHECK(joined && width == 128 && height == 128);
        if (!joined)
            break;
        CHECK(sendSetEncodings(viewer, {0}));
        CHECK(sendUpdateRequest(viewer, false, 0, 0, width, height));
        CHECK(noFramebufferData(viewer, 75)); // no old raw pixels/invalid fallback
        auto fresh = makeFrame(128, 128, 0x203040U);
        const std::weak_ptr<const hyremote::FrameStorage> currentStorage = fresh.storage;
        transport->enqueueFrame(std::move(fresh)); // simulate Runtime's ClientConnected-triggered new grab
        TestFramebuffer frame;
        frame.resize(width, height);
        std::vector<UpdateRect> rects;
        const bool gotPixels = readFramebufferUpdate(viewer, frame, rects);
        CHECK(gotPixels);
        if (gotPixels)
            CHECK(frame.get(0, 0) == 0x203040U);
        CHECK(!currentStorage.expired());
        viewer.disconnectFromHost();
        CHECK(waitForDisconnected(viewer));
        CHECK(released(currentStorage)); // no retained unmarked raw frame at zero viewers
    }

    transport->stop();
}

// #790: the Runtime releases any zero-Viewer input frame instead of painting
// a full watermark into a second, immediately discarded pixel allocation.
// Standalone RFB keeps its marked bootstrap; the opt-in Runtime must still
// render an accurate fresh watermark after a real Viewer joins.
void testProducerBackedSkipsUnusedZeroViewerWatermark()
{
    const quint16 port = freePort();
    CHECK(port != 0);
    if (!port)
        return;
    Recorder recorder;
    std::atomic<int> decorated{0};
    auto mark = markDecorator();
    HyRemote::detail::RfbSecurityConfig security;
    security.initialFramebufferWidth = 128;
    security.initialFramebufferHeight = 128;
    security.releaseIdleFrameBuffers = true;
    security.frameDecorator =
        [&, mark](const hyremote::RemoteFrame &source,
                  hyremote::RemoteFrame &output, QString &error) {
            decorated.fetch_add(1, std::memory_order_acq_rel);
            return mark(source, output, error);
        };
    auto transport = HyRemote::detail::createRfbTransport(
        QHostAddress::LocalHost, port, security);
    CHECK(transport != nullptr);
    if (!transport)
        return;
    CHECK(transport->start(
        [&](const hyremote::InputEvent &event) { recorder.record(event); },
        [&](const hyremote::TransportEvent &event) { recorder.record(event); }));

    const auto noLongerRetained = [](const std::weak_ptr<const hyremote::FrameStorage> &weak) {
        QElapsedTimer timer;
        timer.start();
        while (!weak.expired() && timer.elapsed() < 3000)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        return weak.expired();
    };
    // Sequential frames cannot be elided by the "latest frame wins" mailbox:
    // wait for the RFB worker to release each captured storage allocation.
    for (int index = 0; index < 4; ++index) {
        auto unused = makeFrame(128, 128, 0x203040U);
        const std::weak_ptr<const hyremote::FrameStorage> weak = unused.storage;
        transport->enqueueFrame(std::move(unused));
        CHECK(noLongerRetained(weak));
        CHECK(decorated.load(std::memory_order_acquire) == 0);
    }

    QTcpSocket viewer;
    std::uint16_t width = 0, height = 0;
    const bool joined = connectRawRfb(viewer, port, false, &width, &height);
    CHECK(joined && width == 128 && height == 128);
    if (!joined) {
        transport->stop();
        return;
    }
    CHECK(sendSetEncodings(viewer, {0}));
    CHECK(sendUpdateRequest(viewer, false, 0, 0, width, height));
    CHECK(noFramebufferData(viewer, 75)); // no unmarked stale bootstrap

    transport->enqueueFrame(makeFrame(128, 128, 0x203040U));
    TestFramebuffer pixels;
    pixels.resize(width, height);
    std::vector<UpdateRect> rects;
    const bool received = readFramebufferUpdate(viewer, pixels, rects);
    CHECK(received);
    if (received) {
        CHECK(pixels.get(0, 0) == 0x203040U);
        CHECK(pixels.get(120, 120) == kMarkColor);
    }
    CHECK(decorated.load(std::memory_order_acquire) == 1);

    viewer.disconnectFromHost();
    CHECK(waitForDisconnected(viewer));
    CHECK(recorder.waitFor([](const auto &, const auto &events) {
        return std::any_of(events.begin(), events.end(), [](const auto &event) {
            return event.code == hyremote::TransportEventCode::ClientDisconnected;
        });
    }));
    const int before = decorated.load(std::memory_order_acquire);
    auto idleAgain = makeFrame(128, 128, 0x102030U);
    const std::weak_ptr<const hyremote::FrameStorage> afterLeave = idleAgain.storage;
    transport->enqueueFrame(std::move(idleAgain));
    CHECK(noLongerRetained(afterLeave));
    CHECK(decorated.load(std::memory_order_acquire) == before);
    transport->stop();
}

void testStandaloneMarkedBootstrapSurvivesReconnects()
{
    const quint16 port = freePort();
    CHECK(port != 0);
    if (!port)
        return;

    Recorder recorder;
    std::mutex guarded;
    std::weak_ptr<const hyremote::FrameStorage> currentDecoration;
    auto mark = markDecorator();
    auto trackedMark = [&](const hyremote::RemoteFrame &source,
                           hyremote::RemoteFrame &output, QString &error) {
        if (!mark(source, output, error))
            return false;
        std::lock_guard<std::mutex> lock(guarded);
        currentDecoration = output.storage;
        return true;
    };
    auto raw = makeFrame(128, 128, 0x203040U);
    const std::weak_ptr<const hyremote::FrameStorage> bootstrapRaw = raw.storage;
    auto transport = startTransport(port, recorder, std::move(raw), {}, trackedMark);
    CHECK(transport != nullptr);
    if (!transport)
        return;

    const auto decoratedStorage = [&] {
        std::lock_guard<std::mutex> lock(guarded);
        return currentDecoration;
    };
    const auto checkDecoratedWire = [&](QTcpSocket &socket) {
        CHECK(sendSetEncodings(socket, {0}));
        CHECK(sendUpdateRequest(socket, false, 0, 0, 128, 128));
        TestFramebuffer framebuffer;
        framebuffer.resize(128, 128);
        std::vector<UpdateRect> rects;
        const bool received = readFramebufferUpdate(socket, framebuffer, rects);
        CHECK(received);
        if (received) {
            CHECK(framebuffer.get(0, 0) == 0x203040U);
            CHECK(framebuffer.get(120, 120) == kMarkColor);
        }
    };
    QTcpSocket first;
    std::uint16_t width = 0, height = 0;
    const bool connected = connectRawRfb(first, port, false, &width, &height);
    CHECK(connected && width == 128 && height == 128);
    if (!connected) {
        transport->stop();
        return;
    }
    // Must deliver already queued/marked bootstrap with no second capture after join.
    checkDecoratedWire(first);
    CHECK(!decoratedStorage().expired());

    QTcpSocket second;
    CHECK(connectRawRfb(second, port, false));
    checkDecoratedWire(second);
    first.disconnectFromHost();
    CHECK(waitForDisconnected(first));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    CHECK(!decoratedStorage().expired()); // 2 -> 1 still retains other viewer's composed frame
    second.disconnectFromHost();
    CHECK(waitForDisconnected(second));
    CHECK(!decoratedStorage().expired()); // standalone retains bootstrap for an immediate reconnect
    CHECK(!bootstrapRaw.expired()); // raw shared bootstrap still enables the next ServerInit

    for (int i = 0; i < 20; ++i) {
        QTcpSocket viewer;
        const bool ready = connectRawRfbWithRetry(viewer, port);
        CHECK(ready);
        if (!ready)
            break;
        // No new enqueueFrame() between disconnect/reconnect: this is the exact
        // bootstrap-decorator contract caught by independent #790 review.
        checkDecoratedWire(viewer);
        CHECK(!decoratedStorage().expired());
        viewer.disconnectFromHost();
        CHECK(waitForDisconnected(viewer));
        CHECK(!decoratedStorage().expired()); // unchanged standalone transport contract
    }
    transport->stop();
    CHECK(decoratedStorage().expired());
    CHECK(bootstrapRaw.expired()); // transport stop releases its pixel storage
}


// #790: producer-backed reconnect must obtain a fresh capture before pixels are
// served; failure of its watermark decoration must close the socket fail-closed.
void testProducerBackedReconnectWatermarkFailsClosed()
{
    const quint16 port = freePort();
    CHECK(port != 0);
    if (!port)
        return;

    Recorder recorder;
    std::atomic<bool> failOnNewFrame{false};
    auto mark = markDecorator();
    HyRemote::detail::RfbSecurityConfig security;
    security.initialFramebufferWidth = 128;
    security.initialFramebufferHeight = 128;
    security.releaseIdleFrameBuffers = true;
    security.frameDecorator = [&](const hyremote::RemoteFrame &source,
                                  hyremote::RemoteFrame &decorated, QString &error) {
        if (failOnNewFrame.load()) {
            error = QStringLiteral("intentional reconnect decorator failure");
            return false;
        }
        return mark(source, decorated, error);
    };
    auto transport = HyRemote::detail::createRfbTransport(QHostAddress::LocalHost, port, security);
    CHECK(transport != nullptr);
    if (!transport)
        return;
    CHECK(transport->start([&](const hyremote::InputEvent &event) { recorder.record(event); },
                           [&](const hyremote::TransportEvent &event) { recorder.record(event); }));
    transport->enqueueFrame(makeFrame(128, 128, 0x203040U));

    QTcpSocket first;
    const bool initiallyConnected = connectRawRfb(first, port, false);
    CHECK(initiallyConnected);
    if (!initiallyConnected) {
        transport->stop();
        return;
    }
    CHECK(sendSetEncodings(first, {0}));
    CHECK(sendUpdateRequest(first, false, 0, 0, 128, 128));
    transport->enqueueFrame(makeFrame(128, 128, 0x203040U));
    TestFramebuffer firstFrame;
    firstFrame.resize(128, 128);
    std::vector<UpdateRect> rects;
    CHECK(readFramebufferUpdate(first, firstFrame, rects));
    CHECK(firstFrame.get(120, 120) == kMarkColor);
    first.disconnectFromHost();
    CHECK(waitForDisconnected(first));
    CHECK(recorder.waitFor([](const auto &, const auto &events) {
        return std::any_of(events.begin(), events.end(), [](const auto &event) {
            return event.code == hyremote::TransportEventCode::ClientDisconnected;
        });
    }));

    QTcpSocket reconnect;
    const bool joined = connectRawRfb(reconnect, port, false);
    CHECK(joined);
    if (joined) {
        CHECK(sendSetEncodings(reconnect, {0}));
        CHECK(sendUpdateRequest(reconnect, false, 0, 0, 128, 128));
        CHECK(noFramebufferData(reconnect, 75)); // no stale unmarked pixels
        failOnNewFrame.store(true);
        transport->enqueueFrame(makeFrame(128, 128, 0x203040U));
        CHECK(waitForDisconnected(reconnect));
    }
    CHECK(recorder.waitFor([](const auto &, const auto &events) {
        return std::any_of(events.begin(), events.end(), [](const auto &event) {
            return event.code == hyremote::TransportEventCode::FatalFailure
                && event.message.find("decorator") != std::string::npos;
        });
    }));
    transport->stop();
}

// #567/#589: a decorator may change far-away served pixels even though
// the source advertises a valid adjacent-ID Regions hint. The test decorator
// changes the bottom-right watermark when the first source pixel changes.
// Both changed areas must reach the actual Raw/TRLE Viewer.
void testDecoratedRegionsCannotHideWatermarkChanges()
{
    for (const std::int32_t preferredEncoding : {0, 15}) {
        const quint16 port = freePort();
        CHECK(port != 0);
        if (!port)
            return;
        Recorder recorder;
        auto first = makeFrame(256, 256, 0x102030U);
        first.id = 10U;
        auto transport = startTransport(port, recorder, std::move(first), {},
                                        markDecorator());
        CHECK(transport != nullptr);
        if (!transport)
            return;
        QTcpSocket socket;
        std::uint16_t width = 0, height = 0;
        const bool connected = connectRawRfb(socket, port, false, &width, &height);
        CHECK(connected && width == 256 && height == 256);
        if (!connected) {
            transport->stop();
            return;
        }
        CHECK(sendSetEncodings(socket, {-312, preferredEncoding, 0}));
        CHECK(readFence(socket) == makeFence(0U));
        TestFramebuffer framebuffer;
        framebuffer.resize(width, height);
        std::vector<UpdateRect> rects;
        CHECK(sendUpdateRequest(socket, false, 0, 0, width, height));
        CHECK(readFramebufferUpdate(socket, framebuffer, rects));
        CHECK(framebuffer.get(0, 0) == 0x102030U);
        CHECK(framebuffer.get(250, 250) == 0x00FF00U);

        // The second source changes ONLY (0,0), but the decorator changes
        // the watermark at the opposite corner based on that same pixel.
        // Source frame IDs are adjacent, so without the transport fix the
        // RFB Regions shortcut incorrectly trusts the 1x1 source hint and
        // the Viewer keeps an old green watermark forever.
        CHECK(sendUpdateRequest(socket, true, 0, 0, width, height));
        const QByteArray barrier("delta-armed", 11);
        CHECK(writeAll(socket, makeFence(0x80000000U, barrier)));
        CHECK(readFence(socket) == makeFence(0U, barrier));
        auto second = makeFrame(256, 256, 0x102030U,
                                {{{0, 0, 1, 1}, 0x203040U}});
        second.id = 11U;
        second.damage.kind = hyremote::DamageKind::Regions;
        second.damage.regions = {hyremote::Rect{0, 0, 1, 1}};
        transport->enqueueFrame(std::move(second));

        CHECK(readFramebufferUpdate(socket, framebuffer, rects));
        CHECK(framebuffer.get(0, 0) == 0x203040U);
        CHECK(framebuffer.get(250, 250) == kMarkColor);
        // For this 256x256 scene, only two separated 64x64 tiles changed:
        // below the 25% whole-page paint threshold. Do not smuggle a
        // full-page update through an overly broad damage fallback.
        std::uint64_t area = 0;
        for (const UpdateRect &rect : rects) {
            if (rect.encoding == 0 || rect.encoding == 15)
                area += static_cast<std::uint64_t>(rect.width) * rect.height;
        }
        CHECK(area <= 2U * 64U * 64U);
        socket.disconnectFromHost();
        waitForDisconnected(socket);
        transport->stop();
    }
}

void testWatermarkReachesEveryEncoding()
{
    const quint16 port = freePort();
    CHECK(port != 0);
    if (port == 0)
        return;

    Recorder recorder;
    auto transport = startTransport(port, recorder, makeFrame(128, 128, 0x203040U), {}, markDecorator());
    CHECK(transport != nullptr);
    if (!transport)
        return;

    // TRLE: the tiles must carry the composed pixels, not the capture.
    QTcpSocket trleSocket;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    CHECK(connectRawRfb(trleSocket, port, false, &width, &height));
    CHECK(sendSetEncodings(trleSocket, {15, 0}));
    CHECK(sendUpdateRequest(trleSocket, false, 0, 0, width, height));
    TestFramebuffer trleFrame;
    trleFrame.resize(width, height);
    std::vector<UpdateRect> rectangles;
    CHECK(readFramebufferUpdate(trleSocket, trleFrame, rectangles));
    CHECK(rectangles.size() == 1U && rectangles.front().encoding == 15);
    CHECK(trleFrame.get(120, 120) == kMarkColor);
    CHECK(trleFrame.get(0, 0) == 0x203040U);

    // Raw: the same pixels through the other encoding.
    QTcpSocket rawSocket;
    CHECK(connectRawRfb(rawSocket, port, false, &width, &height));
    CHECK(sendSetEncodings(rawSocket, {0}));
    CHECK(sendUpdateRequest(rawSocket, false, 0, 0, width, height));
    TestFramebuffer rawFrame;
    rawFrame.resize(width, height);
    CHECK(readFramebufferUpdate(rawSocket, rawFrame, rectangles));
    CHECK(rectangles.size() == 1U && rectangles.front().encoding == 0);
    CHECK(rawFrame.get(120, 120) == kMarkColor);

    // Incremental damage, in the discriminating direction: forget what the viewer already knew, then change the
    // *capture* underneath the mark. A server that encodes the composed frame resends the mark there; one that encodes
    // the capture (or tracks damage only on capture pixels) would put the new background on the wire, and the cleared
    // viewer buffer would show it. Asserting the mark on a buffer that was never told about it is what distinguishes
    // "the mark survived in my own copy" from "the mark was actually delivered again".
    trleFrame.resize(width, height);
    CHECK(trleFrame.get(120, 120) == 0U);
    CHECK(sendUpdateRequest(trleSocket, true, 0, 0, width, height));
    transport->enqueueFrame(makeFrame(128, 128, 0x203040U, {{{0, 0, 4, 4}, 0x102030U}}));
    CHECK(readFramebufferUpdate(trleSocket, trleFrame, rectangles));
    // The trimmed mark colour can only be on the wire if the composed frame was encoded and its damage was tracked
    // between composed frames.
    CHECK(trleFrame.get(120, 120) == 0x00FF00U);
    CHECK(trleFrame.get(0, 0) == 0x102030U);

    trleSocket.disconnectFromHost();
    rawSocket.disconnectFromHost();
    waitForDisconnected(trleSocket);
    waitForDisconnected(rawSocket);
    transport->stop();
}

// A decorator that sizes its mark from the frame's own device pixel ratio, so a wire test can prove the ratio a frame
// carries is the one the composed pixels were produced with.
HyRemote::detail::RfbFrameDecorator dprAwareMarkDecorator()
{
    return [](const hyremote::RemoteFrame &source, hyremote::RemoteFrame &decorated, QString &error) {
        if (!source.storage) {
            error = QStringLiteral("no source storage");
            return false;
        }
        const auto plane = source.storage->mapRead(0);
        if (!plane || !plane->data) {
            error = QStringLiteral("no readable plane");
            return false;
        }
        const auto width = source.geometry.size.width;
        const auto height = source.geometry.size.height;
        const double ratio = source.geometry.devicePixelRatio > 0.0F
                                 ? static_cast<double>(source.geometry.devicePixelRatio)
                                 : 1.0;
        const auto markWidth = static_cast<std::uint32_t>(32.0 * ratio);
        const auto markHeight = static_cast<std::uint32_t>(16.0 * ratio);
        if (width < markWidth || height < markHeight) {
            error = QStringLiteral("frame too small for the mark");
            return false;
        }
        auto storage = hyremote::CpuFrameStorage::createSinglePlane(plane->stride, height);
        if (!storage || !storage->mutablePlane(0)) {
            error = QStringLiteral("no target storage");
            return false;
        }
        std::memcpy(storage->mutablePlane(0), plane->data, plane->stride * height);
        auto *bytes = reinterpret_cast<std::uint8_t *>(storage->mutablePlane(0));
        for (std::uint32_t y = height - markHeight; y < height; ++y) {
            for (std::uint32_t x = width - markWidth; x < width; ++x) {
                auto *pixel = bytes + (static_cast<std::size_t>(y) * width + x) * 4U;
                pixel[0] = 0xffU;
                pixel[1] = 0x00U;
                pixel[2] = 0xffU;
                pixel[3] = 0xffU;
            }
        }
        decorated = source;
        decorated.storage = std::move(storage);
        return true;
    };
}

void testWatermarkSurvivesFramebufferResize()
{
    const quint16 port = freePort();
    CHECK(port != 0);
    if (port == 0)
        return;

    Recorder recorder;
    auto transport = startTransport(port, recorder, makeFrame(128, 128, 0x203040U), {}, markDecorator());
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
    CHECK(frame.get(120, 120) == kMarkColor);

    // The framebuffer shrinks: the mark must be recomposed for the new geometry, not left at the old coordinates.
    transport->enqueueFrame(makeFrame(96, 80, 0x445566U));
    CHECK(sendUpdateRequest(socket, true, 0, 0, 96, 80));
    CHECK(readFramebufferUpdate(socket, frame, rectangles));
    CHECK(frame.width == 96 && frame.height == 80);
    bool desktopSizeSeen = false;
    for (const UpdateRect &rect : rectangles) {
        if (rect.encoding == -223)
            desktopSizeSeen = true;
    }
    CHECK(desktopSizeSeen);
    CHECK(frame.get(95, 79) == kMarkColor);
    CHECK(frame.get(64, 64) == kMarkColor);
    CHECK(frame.get(63, 64) == 0x445566U);
    CHECK(frame.get(64, 63) == 0x445566U);

    socket.disconnectFromHost();
    waitForDisconnected(socket);
    transport->stop();
}

void testWatermarkFollowsDevicePixelRatio()
{
    const quint16 port = freePort();
    CHECK(port != 0);
    if (port == 0)
        return;

    Recorder recorder;
    hyremote::RemoteFrame first = makeFrame(128, 128, 0x203040U);
    first.geometry.devicePixelRatio = 1.0F;

    // The producer publishes to the RFB worker asynchronously. Merely
    // enqueueing the DPR=2 frame and immediately connecting a new viewer
    // races that worker: ServerInit can still legitimately use the prior
    // observed DPR=1 picture. Signal after the new immutable decorated
    // pixels are actually produced on the worker, not after a guessed delay.
    std::mutex composedMutex;
    std::condition_variable composedChanged;
    bool composedDpr2 = false;
    const auto baseDecorator = dprAwareMarkDecorator();
    auto observedDecorator = [&](const hyremote::RemoteFrame &source,
                                 hyremote::RemoteFrame &decorated, QString &error) {
        const bool ok = baseDecorator(source, decorated, error);
        if (ok && source.geometry.devicePixelRatio == 2.0F) {
            {
                std::lock_guard<std::mutex> lock(composedMutex);
                composedDpr2 = true;
            }
            composedChanged.notify_all();
        }
        return ok;
    };
    auto transport = startTransport(port, recorder, std::move(first), {}, observedDecorator);
    CHECK(transport != nullptr);
    if (!transport)
        return;

    QTcpSocket socket;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    CHECK(connectRawRfb(socket, port, false, &width, &height));
    CHECK(sendSetEncodings(socket, {15, 0}));
    CHECK(sendUpdateRequest(socket, false, 0, 0, width, height));
    TestFramebuffer frame;
    frame.resize(width, height);
    std::vector<UpdateRect> rectangles;
    CHECK(readFramebufferUpdate(socket, frame, rectangles));
    CHECK(frame.get(120, 120) == kMarkColor);
    // At ratio 1.0 the mark is 32x16, so this row is still capture.
    CHECK(frame.get(120, 100) == 0x203040U);
    socket.disconnectFromHost();
    waitForDisconnected(socket);

    // The same geometry at ratio 2.0: the mark must double in both directions, which is only possible if the ratio the
    // frame carries is the one the composed pixels were produced with.
    //
    // A *new* viewer reads the ratio-2 frame as its initial picture. Re-requesting on the first viewer instead would
    // race against that viewer's already-queued update for the ratio-1 frame, which is a test bug, not a product
    // defect: the assertion would then be made against pixels that were correctly composed for the older frame.
    hyremote::RemoteFrame second = makeFrame(128, 128, 0x203040U);
    second.geometry.devicePixelRatio = 2.0F;
    transport->enqueueFrame(std::move(second));

    bool secondComposed = false;
    {
        std::unique_lock<std::mutex> lock(composedMutex);
        secondComposed = composedChanged.wait_for(
            lock, std::chrono::seconds(3), [&] { return composedDpr2; });
    }
    CHECK(secondComposed);
    if (!secondComposed) {
        transport->stop();
        return;
    }

    // The worker is single-threaded. A new socket acceptance follows this
    // completed decoration callback, so the new viewer may no longer race
    // the older frame. Do not weaken any on-wire pixel assertion.
    QTcpSocket afterRescale;
    CHECK(connectRawRfb(afterRescale, port, false, &width, &height));
    CHECK(sendSetEncodings(afterRescale, {15, 0}));
    CHECK(sendUpdateRequest(afterRescale, false, 0, 0, width, height));
    TestFramebuffer rescaled;
    rescaled.resize(width, height);
    CHECK(readFramebufferUpdate(afterRescale, rescaled, rectangles));
    CHECK(rescaled.get(120, 100) == kMarkColor);
    CHECK(rescaled.get(70, 110) == kMarkColor);
    CHECK(rescaled.get(60, 110) == 0x203040U);
    CHECK(rescaled.get(120, 95) == 0x203040U);

    afterRescale.disconnectFromHost();
    waitForDisconnected(afterRescale);
    transport->stop();
}

void testNoMarkWithoutDecorator()
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

    QTcpSocket socket;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    CHECK(connectRawRfb(socket, port, false, &width, &height));
    CHECK(sendSetEncodings(socket, {15, 0}));
    CHECK(sendUpdateRequest(socket, false, 0, 0, width, height));
    TestFramebuffer frame;
    frame.resize(width, height);
    std::vector<UpdateRect> rectangles;
    CHECK(readFramebufferUpdate(socket, frame, rectangles));

    bool markFound = false;
    for (std::uint16_t y = 0; y < height; ++y) {
        for (std::uint16_t x = 0; x < width; ++x) {
            if (frame.get(x, y) == kMarkColor)
                markFound = true;
        }
    }
    CHECK(!markFound);

    socket.disconnectFromHost();
    waitForDisconnected(socket);
    transport->stop();
}

void testOpaqueAlphaPolicy()
{
    using hyremote::AlphaMode;
    CHECK(HyRemote::detail::rfbOpaqueChannel(255, 255, AlphaMode::Opaque) == 255);
    CHECK(HyRemote::detail::rfbOpaqueChannel(255, 128, AlphaMode::Premultiplied) == 255);
    CHECK(HyRemote::detail::rfbOpaqueChannel(255, 128, AlphaMode::Straight) == 128);
    CHECK(HyRemote::detail::rfbOpaqueChannel(64, 128, AlphaMode::Straight) == 32);
    CHECK(HyRemote::detail::rfbOpaqueChannel(255, 0, AlphaMode::Straight) == 0);
}

// Native 32bpp Raw must preserve the black-matte RGB policy across all alpha
// variants. Opaque/premultiplied take the row-copy fast path; straight-alpha
// retains the per-channel conversion path. The wire decoder validates the
// exact RFB payload, not merely a private helper's result.
// #789: the RFB wire carries RGB only. A complete RGBA byte comparison
// incorrectly expands changes in invisible alpha into full 64x64 RFB damage
// and keeps the Qt Quick capture gate active forever. Also guard against the
// opposite error: the same immutable source storage with a new alpha *mode*
// is a new on-wire image and cannot use the shared-pointer equality shortcut.
hyremote::RemoteFrame makePresentedRgbTestFrame(std::uint8_t red, std::uint8_t green,
                                               std::uint8_t blue, std::uint8_t alpha,
                                               hyremote::AlphaMode mode)
{
    auto frame = makeFrame(128U, 128U, 0U);
    auto storage = hyremote::CpuFrameStorage::createSinglePlane(128U * 4U, 128U);
    CHECK(storage && storage->mutablePlane(0));
    if (!storage || !storage->mutablePlane(0))
        return {};
    auto *pixels = reinterpret_cast<std::uint8_t *>(storage->mutablePlane(0));
    for (std::size_t pixel = 0; pixel < 128U * 128U; ++pixel) {
        pixels[pixel * 4U + 0U] = red;
        pixels[pixel * 4U + 1U] = green;
        pixels[pixel * 4U + 2U] = blue;
        pixels[pixel * 4U + 3U] = alpha;
    }
    frame.storage = std::move(storage);
    frame.geometry.alphaMode = mode;
    return frame;
}

void testPresentedRgbDamageVsSourceAlpha()
{
    using hyremote::AlphaMode;

    const auto opaque = makePresentedRgbTestFrame(32U, 48U, 64U, 255U, AlphaMode::Opaque);
    const auto alphaOnly = makePresentedRgbTestFrame(32U, 48U, 64U, 128U, AlphaMode::Opaque);
    CHECK(HyRemote::detail::changedRfbTiles(&opaque, alphaOnly).empty());
    const auto pre = makePresentedRgbTestFrame(32U, 48U, 64U, 255U, AlphaMode::Premultiplied);
    const auto preAlphaOnly =
        makePresentedRgbTestFrame(32U, 48U, 64U, 128U, AlphaMode::Premultiplied);
    CHECK(HyRemote::detail::changedRfbTiles(&pre, preAlphaOnly).empty());

    // Fully transparent straight-alpha pixels are invisible despite different
    // underlying RGB values; changing their alpha to opaque must be delivered.
    const auto invisibleA =
        makePresentedRgbTestFrame(32U, 48U, 64U, 0U, AlphaMode::Straight);
    const auto invisibleB =
        makePresentedRgbTestFrame(240U, 8U, 99U, 0U, AlphaMode::Straight);
    CHECK(HyRemote::detail::changedRfbTiles(&invisibleA, invisibleB).empty());
    const auto visible =
        makePresentedRgbTestFrame(240U, 8U, 99U, 255U, AlphaMode::Straight);
    CHECK(!HyRemote::detail::changedRfbTiles(&invisibleB, visible).empty());

    auto sameStorageDifferentMode = preAlphaOnly;
    auto originallyOpaque = preAlphaOnly;
    originallyOpaque.geometry.alphaMode = AlphaMode::Opaque;
    sameStorageDifferentMode.geometry.alphaMode = AlphaMode::Straight;
    // Both frames share identical RGBA storage but have *different* visible
    // RGB on the black-matted RFB wire when alpha=0x80.
    originallyOpaque.id = 10U;
    sameStorageDifferentMode.id = 11U;
    sameStorageDifferentMode.damage.kind = hyremote::DamageKind::Regions;
    sameStorageDifferentMode.damage.regions.clear();
    const auto presentationDelta =
        HyRemote::detail::changedRfbTiles(&originallyOpaque, sameStorageDifferentMode);
    CHECK(presentationDelta.size() == 1U);
    if (presentationDelta.size() == 1U)
        CHECK(presentationDelta.rects().front()
              == (HyRemote::detail::RfbRect{0, 0, 128, 128}));
}

// End-to-end test: alpha-only changes in the CPU source must not cause
// another incremental Raw FBU or a spurious "changed pixels" capture wake.
// #789: a prior confirmed RFB-visible static frame may select the exact
// byte equality fast path, without suppressing alpha-only semantics or new
// localized changes. Verify both choices return identical damage rectangles.
void testStaticCompareFastHintPreservesDamage()
{
    using hyremote::AlphaMode;
    const auto base = makePresentedRgbTestFrame(40, 60, 80, 255, AlphaMode::Opaque);
    const auto unchanged = makePresentedRgbTestFrame(40, 60, 80, 255, AlphaMode::Opaque);
    CHECK(base.storage != unchanged.storage);
    const auto scan = HyRemote::detail::changedRfbTiles(&base, unchanged, false);
    const auto hinted = HyRemote::detail::changedRfbTiles(&base, unchanged, true);
    CHECK(scan.empty());
    CHECK(hinted.rects() == scan.rects());

    const auto alphaOnly = makePresentedRgbTestFrame(40, 60, 80, 128, AlphaMode::Opaque);
    CHECK(HyRemote::detail::changedRfbTiles(&base, alphaOnly, false).empty());
    CHECK(HyRemote::detail::changedRfbTiles(&base, alphaOnly, true).empty());

    const auto changed = makePresentedRgbTestFrame(40, 60, 99, 255, AlphaMode::Opaque);
    const auto usual = HyRemote::detail::changedRfbTiles(&base, changed, false);
    const auto maybeStatic = HyRemote::detail::changedRfbTiles(&base, changed, true);
    CHECK(!usual.empty());
    CHECK(maybeStatic.rects() == usual.rects());

    // A visually equivalent fully-transparent straight-alpha frame is still
    // permitted after an exact raw compare misses: the fallback applies the
    // same black-matte policy as native Raw/TRLE.
    const auto transparentA =
        makePresentedRgbTestFrame(10, 20, 30, 0, AlphaMode::Straight);
    const auto transparentB =
        makePresentedRgbTestFrame(240, 200, 160, 0, AlphaMode::Straight);
    CHECK(HyRemote::detail::changedRfbTiles(&transparentA, transparentB, false).empty());
    CHECK(HyRemote::detail::changedRfbTiles(&transparentA, transparentB, true).empty());

    // Changing the interpretation of identical bytes never takes the bulk
    // shortcut; metadata changes must force the existing full refresh.
    auto modeChanged = alphaOnly;
    modeChanged.geometry.alphaMode = AlphaMode::Straight;
    auto priorOpaque = alphaOnly;
    priorOpaque.geometry.alphaMode = AlphaMode::Opaque;
    const auto full = HyRemote::detail::changedRfbTiles(&priorOpaque, modeChanged, true);
    CHECK(full.size() == 1U);
    if (full.size() == 1U)
        CHECK(full.rects().front() == (HyRemote::detail::RfbRect{0, 0, 128, 128}));
}

void testAlphaOnlyFrameIsNotPublishedToRfb()
{
    using hyremote::AlphaMode;
    const quint16 port = freePort();
    CHECK(port != 0);
    if (port == 0)
        return;

    struct Feedback {
        std::mutex mutex;
        std::condition_variable cv;
        int unchanged = 0;
        int changed = 0;
        void record(bool differs)
        {
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (differs)
                    ++changed;
                else
                    ++unchanged;
            }
            cv.notify_all();
        }
        bool await(bool differs, int expected)
        {
            std::unique_lock<std::mutex> lock(mutex);
            return cv.wait_for(lock, std::chrono::seconds(3),
                               [&] { return (differs ? changed : unchanged) >= expected; });
        }
    } feedback;

    Recorder recorder;
    auto transport = startTransport(
        port, recorder, makePresentedRgbTestFrame(32, 48, 64, 255, AlphaMode::Premultiplied),
        {}, {}, [&](bool differs) { feedback.record(differs); });
    CHECK(transport != nullptr);
    if (!transport)
        return;
    QTcpSocket socket;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    const bool connected = connectRawRfb(socket, port, false, &width, &height);
    CHECK(connected && width == 128 && height == 128);
    if (!connected) {
        transport->stop();
        return;
    }
    CHECK(sendSetEncodings(socket, {0}));
    CHECK(sendUpdateRequest(socket, false, 0, 0, width, height));
    TestFramebuffer actual;
    actual.resize(width, height);
    std::vector<UpdateRect> rects;
    CHECK(readFramebufferUpdate(socket, actual, rects));
    CHECK(actual.get(10, 10) == 0x203040U);

    int expectUnchanged;
    {
        std::lock_guard<std::mutex> lock(feedback.mutex);
        expectUnchanged = feedback.unchanged + 1;
    }
    CHECK(sendUpdateRequest(socket, true, 0, 0, width, height));
    transport->enqueueFrame(
        makePresentedRgbTestFrame(32, 48, 64, 128, AlphaMode::Premultiplied));
    CHECK(feedback.await(false, expectUnchanged));
    CHECK(noFramebufferData(socket, 75)); // no alpha bytes in the RFB picture

    int expectChanged;
    {
        std::lock_guard<std::mutex> lock(feedback.mutex);
        expectChanged = feedback.changed + 1;
    }
    transport->enqueueFrame(
        makePresentedRgbTestFrame(80, 48, 64, 128, AlphaMode::Premultiplied));
    CHECK(feedback.await(true, expectChanged));
    CHECK(readFramebufferUpdate(socket, actual, rects));
    CHECK(actual.get(10, 10) == 0x503040U);

    socket.disconnectFromHost();
    CHECK(waitForDisconnected(socket));
    transport->stop();
}

void testNativeRawFastPathAlphaEquivalence()
{
    using hyremote::AlphaMode;
    const std::array<AlphaMode, 3> modes{
        AlphaMode::Opaque, AlphaMode::Premultiplied, AlphaMode::Straight};
    for (const AlphaMode mode : modes) {
        const quint16 port = freePort();
        CHECK(port != 0);
        if (port == 0)
            return;

        Recorder recorder;
        auto frame = makeFrame(16, 8, 0x203040U);
        auto storage = hyremote::CpuFrameStorage::createSinglePlane(16U * 4U, 8U);
        CHECK(storage && storage->mutablePlane(0));
        if (!storage || !storage->mutablePlane(0))
            return;
        auto *pixels = reinterpret_cast<std::uint8_t *>(storage->mutablePlane(0));
        for (std::size_t pixel = 0; pixel < 16U * 8U; ++pixel) {
            pixels[pixel * 4U + 0U] = 0x20U;
            pixels[pixel * 4U + 1U] = 0x30U;
            pixels[pixel * 4U + 2U] = 0x40U;
            pixels[pixel * 4U + 3U] = 0x80U;
        }
        frame.storage = std::move(storage);
        frame.geometry.alphaMode = mode;

        auto transport = startTransport(port, recorder, std::move(frame));
        CHECK(transport != nullptr);
        if (!transport)
            return;
        QTcpSocket socket;
        std::uint16_t width = 0;
        std::uint16_t height = 0;
        CHECK(connectRawRfb(socket, port, false, &width, &height));
        CHECK(width == 16 && height == 8);
        CHECK(sendSetEncodings(socket, {0}));
        CHECK(sendUpdateRequest(socket, false, 0, 0, width, height));
        TestFramebuffer decoded;
        decoded.resize(width, height);
        std::vector<UpdateRect> rects;
        CHECK(readFramebufferUpdate(socket, decoded, rects));
        CHECK(rects.size() == 1U && rects.front().encoding == 0);
        CHECK(decoded.get(0, 0)
              == (mode == AlphaMode::Straight ? 0x101820U : 0x203040U));
        CHECK(decoded.get(15, 7)
              == (mode == AlphaMode::Straight ? 0x101820U : 0x203040U));
        socket.disconnectFromHost();
        waitForDisconnected(socket);
        transport->stop();
    }
}

// #609: The non-native 8/16/32bpp Raw path and straight-alpha pixels use
// exactly the same protocol bytes after bulk preallocation. Check a real RFB
// 3.8 socket, not just an internal helper. The source RGBA (128,64,32,128)
// has black-matted RGB (64,32,16).
void testRawFallbackBulkPackingPreservesWireBytes()
{
    struct Case
    {
        char bits;
        char depth;
        char redMax;
        char greenMax;
        char blueMax;
        char redShift;
        char greenShift;
        char blueShift;
        std::array<std::uint8_t, 4> wire;
        int bytes;
    };
    const std::array<Case, 3> cases{{
        {8, 6, 3, 3, 3, 4, 2, 0, {0x10, 0, 0, 0}, 1},
        {16, 16, 31, 63, 31, 11, 5, 0, {0x02, 0x41, 0, 0}, 2},
        {32, 24, char(255), char(255), char(255), 16, 8, 0,
         {0x10, 0x20, 0x40, 0x00}, 4},
    }};

    for (const Case &spec : cases) {
        const quint16 port = freePort();
        CHECK(port != 0);
        if (!port)
            return;
        Recorder recorder;
        auto frame = makeFrame(17, 9, 0x804020U);
        auto storage = hyremote::CpuFrameStorage::createSinglePlane(17U * 4U, 9);
        CHECK(storage != nullptr);
        if (!storage)
            return;
        auto *bytes = reinterpret_cast<std::uint8_t *>(storage->mutablePlane(0));
        CHECK(bytes != nullptr);
        if (!bytes)
            return;
        for (std::size_t pixel = 0; pixel < 17U * 9U; ++pixel) {
            bytes[pixel * 4U] = 128U;
            bytes[pixel * 4U + 1U] = 64U;
            bytes[pixel * 4U + 2U] = 32U;
            bytes[pixel * 4U + 3U] = 128U;
        }
        frame.storage = std::move(storage);
        frame.geometry.alphaMode = hyremote::AlphaMode::Straight;
        auto transport = startTransport(port, recorder, std::move(frame));
        CHECK(transport != nullptr);
        if (!transport)
            return;

        QTcpSocket socket;
        std::uint16_t width = 0, height = 0;
        const bool connected = connectRawRfb(socket, port, false, &width, &height);
        CHECK(connected);
        if (connected) {
            CHECK(width == 17 && height == 9);
            CHECK(sendPixelFormat(socket, spec.bits, spec.depth,
                                  spec.redMax, spec.greenMax, spec.blueMax,
                                  spec.redShift, spec.greenShift, spec.blueShift));
            CHECK(sendSetEncodings(socket, {0}));
            CHECK(sendUpdateRequest(socket, false, 0, 0, width, height));
            const QByteArray header = readExact(socket, 4);
            CHECK(header.size() == 4);
            if (header.size() == 4) {
                CHECK(static_cast<std::uint8_t>(header.at(0)) == 0U);
                CHECK(readU16(header, 2) == 1U);
            }
            const QByteArray rect = readExact(socket, 12);
            CHECK(rect.size() == 12);
            if (rect.size() == 12) {
                CHECK(readU16(rect, 0) == 0U && readU16(rect, 2) == 0U);
                CHECK(readU16(rect, 4) == width && readU16(rect, 6) == height);
                CHECK(readS32(rect, 8) == 0);
            }
            const qsizetype payloadSize =
                static_cast<qsizetype>(width) * height * spec.bytes;
            const QByteArray payload = readExact(socket, payloadSize);
            CHECK(payload.size() == payloadSize);
            if (payload.size() == payloadSize) {
                for (qsizetype pixel = 0; pixel < static_cast<qsizetype>(width) * height;
                     ++pixel) {
                    for (int byte = 0; byte < spec.bytes; ++byte) {
                        CHECK(static_cast<std::uint8_t>(
                                  payload.at(pixel * spec.bytes + byte)) == spec.wire[byte]);
                    }
                }
            }
        }
        socket.disconnectFromHost();
        waitForDisconnected(socket);
        transport->stop();
    }
}

// #789: high-entropy TRLE blocks use mode-0 raw CPIXEL, not packed
// palettes. Confirm each byte on native and alpha-matted paths, including
// a non-zero x offset and padded source row stride.
void testHighEntropyTrleRawTileBytesAndAlpha()
{
    using hyremote::AlphaMode;
    constexpr std::uint32_t width = 20U, height = 17U, stride = width * 4U + 16U;
    for (const auto mode : {AlphaMode::Opaque, AlphaMode::Premultiplied,
                            AlphaMode::Straight}) {
        auto frame = makeFrame(width, height, 0U);
        auto storage = hyremote::CpuFrameStorage::createSinglePlane(stride, height);
        CHECK(storage && storage->mutablePlane(0));
        if (!storage || !storage->mutablePlane(0))
            return;
        auto *base = reinterpret_cast<std::uint8_t *>(storage->mutablePlane(0));
        for (std::uint32_t y = 0; y < height; ++y) {
            for (std::uint32_t x = 0; x < width; ++x) {
                const std::uint32_t index = y * width + x;
                auto *pixel = base + static_cast<std::size_t>(y) * stride + x * 4U;
                pixel[0] = static_cast<std::uint8_t>((index * 11U) & 255U);
                pixel[1] = static_cast<std::uint8_t>((index * 47U) & 255U);
                pixel[2] = static_cast<std::uint8_t>((index * 71U) & 255U);
                pixel[3] = 180U;
            }
        }
        frame.storage = std::move(storage);
        frame.geometry.alphaMode = mode;
        for (const std::uint32_t left : {0U, 16U}) {
            const std::uint32_t tileWidth = left == 0U ? 16U : 4U;
            QByteArray actual;
            CHECK(HyRemote::detail::appendNativeTrlePayload(
                actual, frame, left, 0U, tileWidth, 16U));
            CHECK(actual.size() == static_cast<qsizetype>(1U + tileWidth * 16U * 3U));
            if (actual.isEmpty())
                continue;
            CHECK(static_cast<std::uint8_t>(actual.at(0)) == 0U);
            for (std::uint32_t row = 0; row < 16U; ++row) {
                for (std::uint32_t col = 0; col < tileWidth; ++col) {
                    const auto *rgba =
                        base + static_cast<std::size_t>(row) * stride
                             + static_cast<std::size_t>(left + col) * 4U;
                    const auto offset = static_cast<qsizetype>(
                        1U + (row * tileWidth + col) * 3U);
                    for (std::uint32_t channel = 0; channel < 3U; ++channel) {
                        const auto wanted = HyRemote::detail::rfbOpaqueChannel(
                            rgba[channel], rgba[3], mode);
                        CHECK(static_cast<std::uint8_t>(actual.at(
                                  offset + static_cast<qsizetype>(channel))) == wanted);
                    }
                }
            }
        }
    }
}

void testTrlePreferredOverRawOrder()
{
    const quint16 port = freePort();
    CHECK(port != 0);
    if (port == 0)
        return;

    Recorder recorder;
    auto transport = startTransport(port, recorder, makeFrame(64, 48, 0x203040U));
    CHECK(transport != nullptr);
    if (!transport)
        return;

    QTcpSocket socket;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    CHECK(connectRawRfb(socket, port, false, &width, &height));
    // Some clients list Raw first even when they can decode TRLE. The
    // transport must not turn that into a multi-MB/s Raw stream.
    CHECK(sendSetEncodings(socket, {0, 15}));
    CHECK(sendUpdateRequest(socket, false, 0, 0, width, height));
    TestFramebuffer frame;
    frame.resize(width, height);
    std::vector<UpdateRect> rectangles;
    CHECK(readFramebufferUpdate(socket, frame, rectangles));
    CHECK(rectangles.size() == 1U);
    CHECK(rectangles.size() == 1U && rectangles.front().encoding == 15);
    CHECK(frame.get(0, 0) == 0x203040U);

    socket.disconnectFromHost();
    waitForDisconnected(socket);
    transport->stop();
}

void testCursorPseudoEncoding()
{
    const quint16 port = freePort();
    CHECK(port != 0);
    if (port == 0)
        return;

    Recorder recorder;
    auto transport = startTransport(port, recorder, makeFrame(64, 48, 0x203040U));
    CHECK(transport != nullptr);
    if (!transport)
        return;

    QTcpSocket cursorSocket;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    CHECK(connectRawRfb(cursorSocket, port, false, &width, &height));
    CHECK(sendSetEncodings(cursorSocket, {-239, 0}));

    // The cursor is sent with the first update after the pixel format has
    // settled, so request before reading it.
    CHECK(sendUpdateRequest(cursorSocket, false, 0, 0, width, height));
    CHECK(readCursorUpdate(cursorSocket));
    TestFramebuffer frame;
    frame.resize(width, height);
    std::vector<UpdateRect> rectangles;
    CHECK(readFramebufferUpdate(cursorSocket, frame, rectangles));
    CHECK(rectangles.size() == 1U && rectangles.front().encoding == 0);

    // A viewer that does not advertise the pseudo-encoding must not receive it.
    QTcpSocket rawSocket;
    CHECK(connectRawRfb(rawSocket, port, false, &width, &height));
    CHECK(sendSetEncodings(rawSocket, {0}));
    CHECK(sendUpdateRequest(rawSocket, false, 0, 0, width, height));
    CHECK(readFramebufferUpdate(rawSocket, frame, rectangles));
    CHECK(rectangles.size() == 1U && rectangles.front().encoding == 0);

    cursorSocket.disconnectFromHost();
    rawSocket.disconnectFromHost();
    waitForDisconnected(cursorSocket);
    waitForDisconnected(rawSocket);
    transport->stop();
}

void testCursorSentAfterPixelFormatWithoutRequest()
{
    const quint16 port = freePort();
    CHECK(port != 0);
    if (port == 0)
        return;

    Recorder recorder;
    auto transport = startTransport(port, recorder, makeFrame(64, 48, 0x203040U));
    CHECK(transport != nullptr);
    if (!transport)
        return;

    QTcpSocket socket;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    CHECK(connectRawRfb(socket, port, false, &width, &height));
    CHECK(sendSetEncodings(socket, {-239, 0}));

    // RealVNC first requests an 8-bpp rgb222 format and later promotes it to
    // 32 bpp. The reduced format must not consume the cursor throttle.
    CHECK(sendRgb222PixelFormat(socket));
    CHECK(sendNativePixelFormat(socket));

    // No FramebufferUpdateRequest and no frame change: the approved pixel
    // format alone must cause the cursor to be sent.
    CHECK(readCursorUpdate(socket));

    CHECK(sendUpdateRequest(socket, false, 0, 0, width, height));
    TestFramebuffer frame;
    frame.resize(width, height);
    std::vector<UpdateRect> rectangles;
    CHECK(readFramebufferUpdate(socket, frame, rectangles));
    CHECK(rectangles.size() == 1U && rectangles.front().encoding == 0);

    socket.disconnectFromHost();
    waitForDisconnected(socket);
    transport->stop();
}

void testVariableCursorSnapshot()
{
    const quint16 port = freePort();
    CHECK(port != 0);
    if (port == 0)
        return;

    Recorder recorder;
    auto transport = startTransport(
        port,
        recorder,
        makeFrame(64, 48, 0x203040U),
        [](int, int, std::uint32_t, std::uint32_t) {
            HyRemote::detail::RfbCursorSnapshot snapshot;
            snapshot.width = 4;
            snapshot.height = 4;
            snapshot.hotspotX = 2;
            snapshot.hotspotY = 1;
            snapshot.rgba8888 = QByteArray(4 * 4 * 4, char(0));
            for (int index = 0; index < 4 * 4; ++index) {
                snapshot.rgba8888[index * 4] = static_cast<char>(255);
                snapshot.rgba8888[index * 4 + 3] = static_cast<char>(255);
            }
            return snapshot;
        });
    CHECK(transport != nullptr);
    if (!transport)
        return;

    QTcpSocket socket;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    CHECK(connectRawRfb(socket, port, false, &width, &height));
    CHECK(sendSetEncodings(socket, {-239, 0}));

    int cursorWidth = 0;
    int cursorHeight = 0;
    int hotspotX = -1;
    int hotspotY = -1;
    CHECK(sendUpdateRequest(socket, false, 0, 0, width, height));
    CHECK(readCursorUpdate(socket, &cursorWidth, &cursorHeight, &hotspotX, &hotspotY));
    CHECK(cursorWidth == 4);
    CHECK(cursorHeight == 4);
    CHECK(hotspotX == 2);
    CHECK(hotspotY == 1);

    TestFramebuffer frame;
    frame.resize(width, height);
    std::vector<UpdateRect> rectangles;
    CHECK(readFramebufferUpdate(socket, frame, rectangles));
    CHECK(rectangles.size() == 1U && rectangles.front().encoding == 0);

    socket.disconnectFromHost();
    waitForDisconnected(socket);
    transport->stop();
}

void testPartialIncrementalAndResize()
{
    const quint16 port = freePort();
    CHECK(port != 0);
    if (port == 0)
        return;

    Recorder recorder;
    std::mutex observedMutex;
    std::condition_variable observedChanged;
    unsigned int observedRevisions = 0;
    // A transport frame is queued asynchronously. Wait until the existing
    // worker-side served-pixel observer has seen the resize before asserting
    // the wire update, otherwise the request can legitimately race ahead of
    // the new framebuffer and receive the previous dimensions.
    auto transport = startTransport(
        port, recorder, makeFrame(256, 128, 0x101820U), {}, {},
        [&](bool changed) {
            if (!changed)
                return;
            {
                std::lock_guard<std::mutex> lock(observedMutex);
                ++observedRevisions;
            }
            observedChanged.notify_all();
        });
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

    unsigned int priorRevisions = 0;
    {
        std::lock_guard<std::mutex> lock(observedMutex);
        priorRevisions = observedRevisions;
    }
    transport->enqueueFrame(makeFrame(80, 60, 0x445566U));
    {
        std::unique_lock<std::mutex> lock(observedMutex);
        CHECK(observedChanged.wait_for(
            lock, std::chrono::seconds(3),
            [&] { return observedRevisions > priorRevisions; }));
    }
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

void testContinuousUpdatesAndRequestFallback()
{
    const quint16 port = freePort();
    CHECK(port != 0);
    if (port == 0)
        return;
    Recorder recorder;
    constexpr std::uint32_t background = 0x101820U;
    auto transport = startTransport(port, recorder, makeFrame(128, 128, background));
    CHECK(transport != nullptr);
    if (!transport)
        return;

    QTcpSocket continuous;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    CHECK(connectRawRfb(continuous, port, false, &width, &height));
    CHECK(width == 128 && height == 128);
    CHECK(sendSetEncodings(continuous, {-313, 0, -223}));
    CHECK(readExact(continuous, 1) == QByteArray(1, char(150)));

    TestFramebuffer frame;
    frame.resize(width, height);
    std::vector<UpdateRect> rectangles;
    CHECK(sendUpdateRequest(continuous, false, 0, 0, width, height));
    CHECK(readFramebufferUpdate(continuous, frame, rectangles));

    QTcpSocket standard;
    CHECK(connectRawRfb(standard, port, false));
    CHECK(sendSetEncodings(standard, {0}));
    // Only the negotiated viewer may issue ContinuousUpdates.
    QTcpSocket rejected;
    CHECK(connectRawRfb(rejected, port, false));
    CHECK(sendContinuousUpdates(rejected, true, 0, 0, 64, 64));
    CHECK(recorder.waitFor([](const auto &, const auto &events) {
        return hasRecoverableFailure(events, "unnegotiated ContinuousUpdates");
    }));
    CHECK(waitForDisconnected(rejected));

    // A stale request-driven incremental for the RIGHT tile must not
    // prevent enabling continuous updates for the LEFT tile.
    CHECK(sendUpdateRequest(continuous, true, 64, 0, 64, 64));
    CHECK(noFramebufferData(continuous));
    CHECK(sendContinuousUpdates(continuous, true, 0, 0, 64, 64));
    CHECK(noFramebufferData(continuous));
    transport->enqueueFrame(makeFrame(128, 128, background,
                                      {{{8, 8, 8, 8}, 0xaa3300U},
                                       {{80, 8, 8, 8}, 0x00aa66U}}));
    CHECK(readFramebufferUpdate(continuous, frame, rectangles));
    CHECK(rectangles.size() == 1U && rectangles.front().x == 0
          && rectangles.front().width == 64 && rectangles.front().encoding == 0);
    CHECK(frame.get(8, 8) == 0xaa3300U);
    CHECK(frame.get(80, 8) == background);
    CHECK(noFramebufferData(standard));
    CHECK(noFramebufferData(continuous));

    // An incremental request while continuous is enabled must be ignored,
    // even if it names a different region. No out-of-region push is allowed.
    CHECK(sendUpdateRequest(continuous, true, 64, 0, 64, 64));
    transport->enqueueFrame(makeFrame(128, 128, background,
                                      {{{8, 8, 8, 8}, 0xaa3300U},
                                       {{80, 8, 8, 8}, 0xcc8844U}}));
    CHECK(noFramebufferData(continuous));
    // A non-incremental request still works outside the continuous region.
    CHECK(sendUpdateRequest(continuous, false, 64, 0, 64, 64));
    CHECK(readFramebufferUpdate(continuous, frame, rectangles));
    CHECK(rectangles.size() == 1U && rectangles.front().x == 64);
    CHECK(frame.get(80, 8) == 0xcc8844U);

    transport->enqueueFrame(makeFrame(128, 128, background,
                                      {{{8, 8, 8, 8}, 0x0055ddU},
                                       {{80, 8, 8, 8}, 0xcc8844U}}));
    CHECK(readFramebufferUpdate(continuous, frame, rectangles));
    CHECK(frame.get(8, 8) == 0x0055ddU);

    // Disable sends the mandatory one-byte end marker, even if already off.
    CHECK(sendContinuousUpdates(continuous, false, 0, 0, 0, 0));
    CHECK(readEndOfContinuousUpdates(continuous));
    CHECK(sendContinuousUpdates(continuous, false, 0, 0, 0, 0));
    CHECK(readEndOfContinuousUpdates(continuous));
    transport->enqueueFrame(makeFrame(128, 128, background,
                                      {{{8, 8, 8, 8}, 0x22bb33U}}));
    CHECK(noFramebufferData(continuous));
    CHECK(sendUpdateRequest(continuous, true, 0, 0, 64, 64));
    CHECK(readFramebufferUpdate(continuous, frame, rectangles));
    CHECK(frame.get(8, 8) == 0x22bb33U);

    // A non-extension viewer remains fully request-driven and independent.
    TestFramebuffer standardFrame;
    standardFrame.resize(width, height);
    CHECK(sendUpdateRequest(standard, false, 0, 0, width, height));
    CHECK(readFramebufferUpdate(standard, standardFrame, rectangles));
    CHECK(standardFrame.get(8, 8) == 0x22bb33U);

    continuous.disconnectFromHost();
    standard.disconnectFromHost();
    waitForDisconnected(continuous);
    waitForDisconnected(standard);
    transport->stop();
}

void testPerViewerPendingDamageRemainsBounded()
{
    // #371: an idle viewer accumulates damage privately until it requests an
    // update. More than 64 distinct changed regions must not create unbounded
    // pending metadata or lose newer areas when the bound coalesces them.
    // This exercises the transport-private region state, not a slow TCP socket.
    HyRemote::detail::RfbUpdateState paused;
    HyRemote::detail::RfbUpdateState active;
    // Exercise the exact private accumulator in isolation, before
    // selectEligible() can independently coalesce its output.
    HyRemote::detail::RfbDamageRegion pendingRegion;
    const HyRemote::detail::RfbRect viewport{0, 0, 256, 256};
    std::vector<HyRemote::detail::RfbRect> changes;
    constexpr std::size_t count = HyRemote::detail::kRfbMaxDamageRects + 8U;
    changes.reserve(count);

    for (std::size_t index = 0; index < count; ++index) {
        // First 65 regions force coalescing inside the upper 176 rows.
        // Remaining regions are BELOW that merged bound: dropping new
        // damage after first coalescence must make this test fail.
        const bool afterCoalescing
            = index > HyRemote::detail::kRfbMaxDamageRects;
        const HyRemote::detail::RfbRect changed{
            static_cast<std::int32_t>(
                (afterCoalescing
                     ? index - HyRemote::detail::kRfbMaxDamageRects - 1U
                     : index % 9U) * 24U),
            static_cast<std::int32_t>(
                afterCoalescing ? 216U : (index / 9U) * 24U),
            8, 8};
        HyRemote::detail::RfbDamageRegion delta;
        delta.add(changed);
        pendingRegion.add(delta);
        // Bound ORIGINAL pending metadata, not only the region selected
        // later for one request (which separately normalizes its output).
        CHECK(pendingRegion.size() <= HyRemote::detail::kRfbMaxDamageRects);
        paused.accumulate(delta);
        changes.push_back(changed);

        // An independently serviced viewer releases its own damage on each
        // step, while the paused viewer keeps waiting for a single request.
        active.accumulate(delta);
        active.request(true, viewport);
        const auto ready = active.selectEligible();
        CHECK(ready.has_value());
        if (!ready)
            return;
        CHECK(ready->size() == 1U && ready->rects().front() == changed);
        active.commitDelivered(*ready);
        CHECK(!active.hasPendingDamage());
    }

    CHECK(paused.hasPendingDamage());
    CHECK(!paused.hasOutstandingRequest());
    CHECK(pendingRegion.size() <= HyRemote::detail::kRfbMaxDamageRects);
    for (const auto &changed : changes) {
        const bool preserved = std::any_of(
            pendingRegion.rects().begin(), pendingRegion.rects().end(),
            [&changed](const auto &rect) {
                return HyRemote::detail::containsRfbRect(rect, changed);
            });
        CHECK(preserved);
    }
    paused.request(true, viewport);
    const auto catchUp = paused.selectEligible();
    CHECK(catchUp.has_value());
    if (!catchUp)
        return;
    CHECK(!catchUp->empty());
    CHECK(catchUp->size() <= HyRemote::detail::kRfbMaxDamageRects);

    // Coalescing may over-cover a rectangle, but it must retain every changed
    // area and must not resurrect old damage after one eligible delivery.
    for (const auto &changed : changes) {
        const bool covered = std::any_of(
            catchUp->rects().begin(), catchUp->rects().end(),
            [&changed](const auto &rect) {
                return HyRemote::detail::containsRfbRect(rect, changed);
            });
        CHECK(covered);
    }
    paused.commitDelivered(*catchUp);
    CHECK(!paused.hasPendingDamage());
    CHECK(!paused.hasOutstandingRequest());

    // A newer independent event must still be eligible after that catch-up.
    HyRemote::detail::RfbDamageRegion latest;
    const HyRemote::detail::RfbRect next{216, 192, 8, 8};
    latest.add(next);
    paused.accumulate(latest);
    paused.request(true, viewport);
    const auto nextUpdate = paused.selectEligible();
    CHECK(nextUpdate.has_value());
    if (nextUpdate) {
        CHECK(nextUpdate->size() == 1U && nextUpdate->rects().front() == next);
        paused.commitDelivered(*nextUpdate);
    }
    CHECK(!paused.hasPendingDamage());
}

void testRequestPausedViewerDoesNotStallContinuousPeer()
{
    // #371: an otherwise connected request-driven viewer may go idle without
    // issuing another FBU request. Its accumulated damage must remain private;
    // it must neither stall a continuously updating peer nor replay stale
    // intermediate pixels when its own requests resume. This deliberately
    // models request inactivity, NOT socket backpressure or a shaped LAN.
    const quint16 port = freePort();
    CHECK(port != 0);
    if (port == 0)
        return;

    Recorder recorder;
    constexpr std::uint32_t background = 0x182838U;
    const hyremote::Rect leftRect{8, 8, 8, 8};
    const hyremote::Rect rightRect{80, 8, 8, 8};
    auto transport = startTransport(port, recorder, makeFrame(128, 128, background));
    CHECK(transport != nullptr);
    if (!transport)
        return;

    QTcpSocket fast;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    const bool fastConnected = connectRawRfb(fast, port, false, &width, &height);
    CHECK(fastConnected);
    if (!fastConnected) {
        transport->stop();
        return;
    }
    CHECK(width == 128 && height == 128);
    CHECK(sendSetEncodings(fast, {-313, 0}));
    CHECK(readExact(fast, 1) == QByteArray(1, char(150)));

    TestFramebuffer fastFrame;
    fastFrame.resize(width, height);
    std::vector<UpdateRect> rectangles;
    CHECK(sendUpdateRequest(fast, false, 0, 0, width, height));
    CHECK(readFramebufferUpdate(fast, fastFrame, rectangles));
    CHECK(sendContinuousUpdates(fast, true, 0, 0, width, height));
    CHECK(noFramebufferData(fast));

    // The second real socket uses ordinary request-driven Raw only.
    QTcpSocket paused;
    std::uint16_t pausedWidth = 0;
    std::uint16_t pausedHeight = 0;
    const bool pausedConnected = connectRawRfb(paused, port, false, &pausedWidth, &pausedHeight);
    CHECK(pausedConnected);
    if (!pausedConnected) {
        fast.disconnectFromHost();
        waitForDisconnected(fast);
        transport->stop();
        return;
    }
    CHECK(pausedWidth == width && pausedHeight == height);
    // Fence is used only as a per-socket request-PARSE barrier below:
    // no BlockBefore/After transport-drain semantics are assumed.
    CHECK(sendSetEncodings(paused, {-312, 0}));
    CHECK(readFence(paused) == makeFence(0U));
    TestFramebuffer pausedFrame;
    pausedFrame.resize(pausedWidth, pausedHeight);
    CHECK(sendUpdateRequest(paused, false, 0, 0, pausedWidth, pausedHeight));
    CHECK(readFramebufferUpdate(paused, pausedFrame, rectangles));

    std::uint32_t leftColor = background;
    std::uint32_t rightColor = background;
    for (std::uint32_t step = 0; step < 16; ++step) {
        const bool leftChanged = (step % 2U) == 0U;
        if (leftChanged)
            leftColor = 0x880000U + step;
        else
            rightColor = 0x008800U + step;

        transport->enqueueFrame(makeFrame(128, 128, background,
                                          {{leftRect, leftColor}, {rightRect, rightColor}}));
        // Each iteration is acknowledged by the fast real socket before the
        // next frame. No timing-based assumption about worker coalescing.
        CHECK(readFramebufferUpdate(fast, fastFrame, rectangles));
        CHECK(rectangles.size() == 1U && rectangles.front().encoding == 0);
        CHECK(rectangles.size() == 1U
              && rectangles.front().x == (leftChanged ? 0 : 64));
        CHECK(fastFrame.get(8, 8) == leftColor);
        CHECK(fastFrame.get(80, 8) == rightColor);
    }

    CHECK(noFramebufferData(paused));  // no request, no unsolicited payload
    CHECK(noFramebufferData(fast));    // static scene, no duplicate payload

    // The paused viewer must catch up in one incremental reply with the
    // latest state of BOTH independent tiles, not 16 intermediate updates.
    // The two broad tiles are coalesced into one paint rectangle so a viewer
    // does not render them as a scanline sequence.
    CHECK(sendUpdateRequest(paused, true, 0, 0, pausedWidth, pausedHeight));
    CHECK(readFramebufferUpdate(paused, pausedFrame, rectangles));
    CHECK(rectangles.size() == 1U);
    CHECK(pausedFrame.get(8, 8) == leftColor);
    CHECK(pausedFrame.get(80, 8) == rightColor);

    CHECK(sendUpdateRequest(paused, true, 0, 0, pausedWidth, pausedHeight));
    // A Fence round trip confirms the server parsed this pending request
    // before the next changed frame is queued. No TCP-drain/order flags are
    // claimed. The separate static-scene tests own zero-damage handling;
    // this case proves request-paused peer isolation and later recovery.
    const QByteArray requestBarrier("next", 4);
    CHECK(writeAll(paused, makeFence(0x80000000U, requestBarrier)));
    CHECK(readFence(paused) == makeFence(0U, requestBarrier));
    CHECK(noFramebufferData(paused));
    CHECK(noFramebufferData(fast));

    // Both peers remain live after catch-up, independently receiving one
    // later changed tile without a new request from the continuous viewer.
    leftColor = 0xaa44ccU;
    transport->enqueueFrame(makeFrame(128, 128, background,
                                      {{leftRect, leftColor}, {rightRect, rightColor}}));
    CHECK(readFramebufferUpdate(fast, fastFrame, rectangles));
    CHECK(fastFrame.get(8, 8) == leftColor);
    CHECK(readFramebufferUpdate(paused, pausedFrame, rectangles));
    CHECK(rectangles.size() == 1U && rectangles.front().x == 0);
    CHECK(pausedFrame.get(8, 8) == leftColor);
    CHECK(pausedFrame.get(80, 8) == rightColor);

    paused.disconnectFromHost();
    fast.disconnectFromHost();
    waitForDisconnected(paused);
    waitForDisconnected(fast);
    transport->stop();
}

void testForcedRefreshHonorsExplicitRegion()
{
    // Deterministic regression for a non-incremental request parked behind
    // queued socket bytes while a geometry invalidation arrives. This exercises
    // the same transport-private RfbUpdateState without timing assumptions.
    HyRemote::detail::RfbUpdateState delivery;
    delivery.request(false, {64, 8, 64, 48});
    delivery.invalidateFull(96, 80);

    const auto eligible = delivery.selectEligible();
    CHECK(eligible.has_value());
    if (eligible) {
        CHECK(eligible->size() == 1U);
        CHECK(eligible->rects().front() == (HyRemote::detail::RfbRect{64, 8, 32, 48}));
        delivery.commitDelivered(*eligible);
    }

    // A subsequent forced refresh must also obey an incremental request's
    // requested area. Empty intersection means a resize can send DesktopSize
    // without inventing an out-of-region pixel rectangle.
    delivery.request(true, {110, 110, 8, 8});
    delivery.invalidateFull(96, 80);
    const auto empty = delivery.selectEligible();
    CHECK(empty.has_value());
    CHECK(empty && empty->empty());

    // Neither a resize nor an initial forced refresh may enlarge the normal
    // request-driven path when no ContinuousUpdates capability was advertised.
    const quint16 port = freePort();
    CHECK(port != 0);
    if (port == 0)
        return;
    Recorder recorder;
    auto transport = startTransport(port, recorder, makeFrame(128, 128, 0x314159U));
    CHECK(transport != nullptr);
    if (!transport)
        return;
    QTcpSocket socket;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    CHECK(connectRawRfb(socket, port, false, &width, &height));
    CHECK(sendSetEncodings(socket, {-223, 0}));
    CHECK(sendUpdateRequest(socket, false, 32, 16, 24, 12));
    TestFramebuffer frame;
    frame.resize(width, height);
    std::vector<UpdateRect> rectangles;
    CHECK(readFramebufferUpdate(socket, frame, rectangles));
    CHECK(rectangles.size() == 1U);
    CHECK(rectangles.size() == 1U && rectangles.front().x == 32
          && rectangles.front().y == 16 && rectangles.front().width == 24
          && rectangles.front().height == 12);
    CHECK(frame.get(40, 20) == 0x314159U);
    CHECK(frame.get(0, 0) == 0U);
    socket.disconnectFromHost();
    waitForDisconnected(socket);
    transport->stop();
}

void testContinuousUpdatesForcedRefreshBounds()
{
    const quint16 port = freePort();
    CHECK(port != 0);
    if (port == 0)
        return;
    Recorder recorder;
    constexpr std::uint32_t background = 0x182838U;
    auto transport = startTransport(port, recorder, makeFrame(128, 128, background));
    CHECK(transport != nullptr);
    if (!transport)
        return;

    QTcpSocket socket;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    CHECK(connectRawRfb(socket, port, false, &width, &height));
    CHECK(sendSetEncodings(socket, {-313, -223, 0}));
    CHECK(readExact(socket, 1) == QByteArray(1, char(150)));

    // The first update is still a forced refresh from ServerInit. It may not
    // leak the entire framebuffer outside the 32x32 enabled region.
    CHECK(sendContinuousUpdates(socket, true, 32, 32, 32, 32));
    TestFramebuffer frame;
    frame.resize(width, height);
    std::vector<UpdateRect> rectangles;
    CHECK(readFramebufferUpdate(socket, frame, rectangles));
    CHECK(rectangles.size() == 1U && rectangles.front().x == 32
          && rectangles.front().y == 32 && rectangles.front().width == 32
          && rectangles.front().height == 32 && rectangles.front().encoding == 0);
    CHECK(frame.get(40, 40) == background);
    CHECK(frame.get(0, 0) == 0U);

    // A resize must still communicate DesktopSize, but automatic pixel
    // updates remain clipped to the enabled region.
    transport->enqueueFrame(makeFrame(96, 96, 0x224466U));
    CHECK(readFramebufferUpdate(socket, frame, rectangles));
    CHECK(rectangles.size() == 2U);
    CHECK(rectangles.size() == 2U && rectangles.front().encoding == -223);
    CHECK(rectangles.size() == 2U && rectangles.back().x == 32
          && rectangles.back().y == 32 && rectangles.back().width == 32
          && rectangles.back().height == 32);
    CHECK(frame.width == 96 && frame.height == 96);
    CHECK(frame.get(40, 40) == 0x224466U);
    CHECK(frame.get(0, 0) == 0U);

    CHECK(sendContinuousUpdates(socket, false, 0, 0, 0, 0));
    CHECK(readEndOfContinuousUpdates(socket));
    socket.disconnectFromHost();
    waitForDisconnected(socket);
    transport->stop();
}

// #567: receiving a new producer framebuffer cannot change pointer mapping
// before the Viewer has received DesktopSize. The request-driven client keeps
// displaying its earlier size even after the RFB worker processes the resize.
void testPointerViewportTracksAdvertisedFramebuffer()
{
    const quint16 port = freePort();
    CHECK(port != 0);
    if (!port)
        return;
    Recorder recorder;
    std::atomic<int> compared{0};
    auto transport = startTransport(
        port, recorder, makeFrame(64, 48, 0x203040U), {}, {},
        [&](bool) { compared.fetch_add(1); });
    CHECK(transport != nullptr);
    if (!transport)
        return;

    QTcpSocket socket;
    std::uint16_t width = 0, height = 0;
    const bool connected = connectRawRfb(socket, port, false, &width, &height);
    CHECK(connected && width == 64 && height == 48);
    if (!connected) {
        transport->stop();
        return;
    }
    CHECK(sendSetEncodings(socket, {-312, -223, 0}));
    CHECK(readFence(socket) == makeFence(0U));
    CHECK(sendUpdateRequest(socket, false, 0, 0, width, height));
    TestFramebuffer shown;
    shown.resize(width, height);
    std::vector<UpdateRect> rectangles;
    CHECK(readFramebufferUpdate(socket, shown, rectangles));

    const int before = compared.load();
    transport->enqueueFrame(makeFrame(96, 80, 0x508060U));
    QElapsedTimer timeout;
    timeout.start();
    while (compared.load() == before && timeout.elapsed() < 3000)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    CHECK(compared.load() > before);
    // The fence proves the worker processed that latest frame, but there
    // is still no pending FBU request capable of delivering DesktopSize.
    const QByteArray barrier("resize-pending", 14);
    CHECK(writeAll(socket, makeFence(0x80000000U, barrier)));
    CHECK(readFence(socket) == makeFence(0U, barrier));
    CHECK(noFramebufferData(socket));

    CHECK(sendPointerButton(socket, false, 17, 19));
    CHECK(recorder.waitFor([](const auto &inputs, const auto &) {
        return std::any_of(inputs.begin(), inputs.end(), [](const auto &e) {
            return e.kind == hyremote::InputEventKind::PointerMove
                && e.x == 17.0F && e.y == 19.0F;
        });
    }));
    {
        std::lock_guard<std::mutex> lock(recorder.mutex);
        const auto it = std::find_if(recorder.inputs.rbegin(), recorder.inputs.rend(),
            [](const auto &e) {
                return e.kind == hyremote::InputEventKind::PointerMove
                    && e.x == 17.0F && e.y == 19.0F;
            });
        CHECK(it != recorder.inputs.rend());
        if (it != recorder.inputs.rend()) {
            CHECK(it->sourceViewport.width == 64U);
            CHECK(it->sourceViewport.height == 48U);
        }
    }

    CHECK(sendUpdateRequest(socket, true, 0, 0, width, height));
    CHECK(readFramebufferUpdate(socket, shown, rectangles));
    CHECK(shown.width == 96 && shown.height == 80);
    CHECK(shown.get(15, 15) == 0x508060U);
    CHECK(sendPointerButton(socket, false, 18, 19));
    CHECK(recorder.waitFor([](const auto &inputs, const auto &) {
        return std::any_of(inputs.begin(), inputs.end(), [](const auto &e) {
            return e.kind == hyremote::InputEventKind::PointerMove
                && e.x == 18.0F && e.y == 19.0F;
        });
    }));
    {
        std::lock_guard<std::mutex> lock(recorder.mutex);
        const auto it = std::find_if(recorder.inputs.rbegin(), recorder.inputs.rend(),
            [](const auto &e) {
                return e.kind == hyremote::InputEventKind::PointerMove
                    && e.x == 18.0F && e.y == 19.0F;
            });
        CHECK(it != recorder.inputs.rend());
        if (it != recorder.inputs.rend()) {
            CHECK(it->sourceViewport.width == 96U);
            CHECK(it->sourceViewport.height == 80U);
        }
    }
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

void testNegotiatedFenceAndRequestFallback()
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

    QTcpSocket negotiated;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    CHECK(connectRawRfb(negotiated, port, false, &width, &height));
    CHECK(sendSetEncodings(negotiated, {-312, 0}));
    CHECK(readFence(negotiated) == makeFence(0U));

    // Fragmented Request + BlockBefore/After/SyncNext/reserved bits. None
    // of the ordering modes is claimed until Qt socket/Runtime barriers exist.
    const QByteArray payload("fence", 5);
    CHECK(writeBytewise(negotiated, makeFence(0xc0000007U, payload)));
    CHECK(readFence(negotiated) == makeFence(0U, payload));

    CHECK(sendUpdateRequest(negotiated, false, 0, 0, width, height));
    TestFramebuffer frame;
    frame.resize(width, height);
    std::vector<UpdateRect> rectangles;
    CHECK(readFramebufferUpdate(negotiated, frame, rectangles));
    CHECK(rectangles.size() == 1U && rectangles.front().encoding == 0);

    // One client's negotiation cannot enable the extension for another.
    QTcpSocket unnegotiated;
    CHECK(connectRawRfb(unnegotiated, port, false));
    CHECK(writeAll(unnegotiated, makeFence(0x80000000U)));
    CHECK(recorder.waitFor([](const auto &, const auto &events) {
        return hasRecoverableFailure(events, "unnegotiated Fence");
    }));
    CHECK(waitForDisconnected(unnegotiated));

    CHECK(writeAll(negotiated, makeFence(0x80000001U, QByteArray("ok", 2))));
    CHECK(readFence(negotiated) == makeFence(0U, QByteArray("ok", 2)));

    // One TCP write can pipeline Fence + a state-changing input message.
    // We must still clear BlockAfter because QTcpSocket::write is not a drain
    // barrier, regardless of how the server dispatches the following input.
    QByteArray pipelined = makeFence(0x80000002U, QByteArray("pipe", 4));
    pipelined.append(char(5));  // PointerEvent, left button down
    pipelined.append(char(1));
    appendU16(pipelined, 17);
    appendU16(pipelined, 19);
    CHECK(writeAll(negotiated, pipelined));
    CHECK(readFence(negotiated) == makeFence(0U, QByteArray("pipe", 4)));

    // Reject the 65-byte claim from the fixed header; do not await the data.
    QTcpSocket oversized;
    CHECK(connectRawRfb(oversized, port, false));
    CHECK(sendSetEncodings(oversized, {-312, 0}));
    CHECK(readFence(oversized) == makeFence(0U));
    QByteArray invalid = makeFence(0x80000000U);
    invalid[8] = char(65);
    CHECK(writeAll(oversized, invalid));
    CHECK(recorder.waitFor([](const auto &, const auto &events) {
        return hasRecoverableFailure(events, "Fence payload exceeded");
    }));
    CHECK(waitForDisconnected(oversized));

    negotiated.disconnectFromHost();
    waitForDisconnected(negotiated);
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

// #158: V1 speaks RFB 3.8 exactly. A 3.7 (or any other) reply must be rejected before the server writes a
// security-type list, and the rejection must leave the transport able to serve the next client.
void testExactRfb38VersionIsRequired()
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

    const auto expectRejected = [&](const QByteArray &version) {
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, port);
        CHECK(socket.waitForConnected(3000));
        CHECK(readExact(socket, 12) == QByteArray("RFB 003.008\n", 12));
        CHECK(writeAll(socket, version));
        // A version the product does not speak must not be followed by a security-type list.
        CHECK(readExact(socket, 2, 1000).isEmpty());
        CHECK(waitForDisconnected(socket));
    };
    expectRejected(QByteArray("RFB 003.007\n", 12));
    expectRejected(QByteArray("RFB 003.005\n", 12));
    expectRejected(QByteArray("RFB 004.001\n", 12));
    CHECK(recorder.waitFor([](const auto &, const auto &events) {
        return std::count_if(events.begin(), events.end(), [](const hyremote::TransportEvent &event) {
                   return event.code == hyremote::TransportEventCode::RecoverableFailure
                          && event.message.find("exact RFB 3.8") != std::string::npos;
               }) == 3;
    }));

    CHECK(primeInitialFrame(*transport));
    QTcpSocket accepted;
    CHECK(connectRawRfb(accepted, port, false));
    accepted.disconnectFromHost();
    waitForDisconnected(accepted);
    transport->stop();
}

// #158/#603: physical acceptance measured RealVNC Viewer 7.15.1 delaying ClientInit by ~29 s while it waits
// for the user to dismiss its unencrypted-connection notice. The bounded handshake must still serve such a
// client instead of closing the socket under it.
void testSlowViewerClientInitStillServed()
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
    CHECK(negotiateThroughSecurity(socket, port, false));

    // The viewer is still showing its notice: keep the socket idle past the old 3 s bound.
    QElapsedTimer pause;
    pause.start();
    while (pause.elapsed() < 4000) {
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    CHECK(socket.state() == QAbstractSocket::ConnectedState);

    CHECK(writeAll(socket, QByteArray(1, char(1))));
    const QByteArray serverInit = readExact(socket, 24);
    CHECK(serverInit.size() == 24);
    CHECK(readU16(serverInit, 0) == 64 && readU16(serverInit, 2) == 48);
    const std::uint32_t nameLength = readU32(serverInit, 20);
    CHECK(readExact(socket, static_cast<qsizetype>(nameLength)).size()
          == static_cast<qsizetype>(nameLength));

    // The session must be fully usable from here on: a Normal-phase message is processed, which is what the
    // long ClientInit wait could otherwise have cost (the slot would have been aborted before this point).
    QByteArray keyEvent;
    keyEvent.append(char(4));
    keyEvent.append(char(1));
    appendU16(keyEvent, 0);
    appendU32(keyEvent, static_cast<std::uint32_t>('a'));
    CHECK(writeAll(socket, keyEvent));

    QElapsedTimer delivery;
    delivery.start();
    bool delivered = false;
    while (delivery.elapsed() < 4000) {
        {
            std::lock_guard<std::mutex> lock(recorder.mutex);
            delivered = !recorder.inputs.empty();
        }
        if (delivered)
            break;
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    CHECK(delivered);

    socket.disconnectFromHost();
    waitForDisconnected(socket);
    transport->stop();
}

// #158: bounded Normal-phase occupancy. Eight viewers that stall inside a message must expire and free
// their bounded slots; a ninth legitimate viewer then gets a usable session on a clean per-client state.
void testStalledPartialMessagesReleaseTheirSlots()
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

    std::array<QTcpSocket, 8> stalled;
    for (auto &socket : stalled) {
        CHECK(connectRawRfb(socket, port, false));
        // The first four bytes of a ten-byte FramebufferUpdateRequest: an incomplete buffered message.
        CHECK(writeAll(socket, QByteArray("\x03\x00\x00\x00", 4)));
    }
    CHECK(recorder.waitFor([](const auto &, const auto &events) {
        return std::count_if(events.begin(), events.end(), [](const hyremote::TransportEvent &event) {
                   return event.code == hyremote::TransportEventCode::RecoverableFailure
                          && event.message.find("bounded progress deadline") != std::string::npos;
               }) >= 8;
    }, 9000));

    QTcpSocket recovery;
    CHECK(connectRawRfbWithRetry(recovery, port));
    CHECK(sendUpdateRequest(recovery, true, 0, 0, 64, 48));
    CHECK(recovery.state() == QAbstractSocket::ConnectedState);
    recovery.disconnectFromHost();
    waitForDisconnected(recovery);
    transport->stop();
}

// #158: the post-init deadline only expires a viewer that never produced a useful message. A viewer with
// an outstanding incremental request on a static frame must survive it and stay connected.
void testPostInitUsefulnessDeadlineKeepsStaticViewer()
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

    QTcpSocket idle;
    CHECK(connectRawRfb(idle, port, false));
    QTcpSocket staticViewer;
    CHECK(connectRawRfb(staticViewer, port, false));
    // The first incremental request is answered by the forced refresh that ServerInit scheduled, so consume
    // it first; only the second request on an unchanged frame stays outstanding across the deadline.
    TestFramebuffer staticFrame;
    staticFrame.resize(64, 48);
    std::vector<UpdateRect> staticRects;
    CHECK(sendUpdateRequest(staticViewer, true, 0, 0, 64, 48));
    CHECK(readFramebufferUpdate(staticViewer, staticFrame, staticRects));
    CHECK(sendUpdateRequest(staticViewer, true, 0, 0, 64, 48));
    CHECK(noFramebufferData(staticViewer));

    CHECK(recorder.waitFor([](const auto &, const auto &events) {
        return std::any_of(events.begin(), events.end(), [](const hyremote::TransportEvent &event) {
            return event.code == hyremote::TransportEventCode::RecoverableFailure
                   && event.message.find("bounded post-init deadline") != std::string::npos;
        });
    }, 13000));
    CHECK(waitForDisconnected(idle));
    CHECK(staticViewer.state() == QAbstractSocket::ConnectedState);
    // A further settlement window proves the useful viewer is not expired late either.
    QElapsedTimer settle;
    settle.start();
    while (settle.elapsed() < 2000) {
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    CHECK(countRecoverableFailures(recorder, "bounded post-init deadline") == 1);
    CHECK(staticViewer.state() == QAbstractSocket::ConnectedState);
    staticViewer.disconnectFromHost();
    waitForDisconnected(staticViewer);
    transport->stop();
}

// #158: a client expired while holding a recognized key must release it exactly once, through the same
// disconnected path as any other abort, and a reconnect must start with clean state.
void testStalledClientReleasesHeldKeyOnce()
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
    QByteArray keyDown;
    keyDown.append(char(4));
    keyDown.append(char(1));
    appendU16(keyDown, 0);
    appendU32(keyDown, static_cast<std::uint32_t>('a'));
    CHECK(writeAll(socket, keyDown));
    CHECK(writeAll(socket, QByteArray("\x03\x00\x00\x00", 4)));
    CHECK(recorder.waitFor([](const auto &, const auto &events) {
        return std::any_of(events.begin(), events.end(), [](const hyremote::TransportEvent &event) {
            return event.code == hyremote::TransportEventCode::RecoverableFailure
                   && event.message.find("bounded progress deadline") != std::string::npos;
        });
    }, 9000));
    CHECK(waitForDisconnected(socket));

    // The release is published from the transport's worker thread, so a fully loaded suite can observe the
    // client-side disconnect a moment before the recorder sees it. Wait for the balanced pair instead of
    // racing the publication; the assertion below still requires exactly one press and one release.
    int presses = 0;
    int releases = 0;
    QElapsedTimer balance;
    balance.start();
    while (balance.elapsed() < 5000) {
        presses = 0;
        releases = 0;
        {
            std::lock_guard<std::mutex> lock(recorder.mutex);
            for (const auto &event : recorder.inputs) {
                if (event.kind != hyremote::InputEventKind::Key || event.key != hyremote::KeyCode::A)
                    continue;
                if (event.pressed)
                    ++presses;
                else
                    ++releases;
            }
        }
        if (presses == 1 && releases == 1)
            break;
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    CHECK(presses == 1);
    CHECK(releases == 1);

    QTcpSocket reconnect;
    CHECK(connectRawRfbWithRetry(reconnect, port));
    CHECK(reconnect.state() == QAbstractSocket::ConnectedState);
    reconnect.disconnectFromHost();
    waitForDisconnected(reconnect);
    transport->stop();
}

// #158: stopping with armed occupancy timers must be quiescent - no late expiry event and no resurrection
// of a client that was already torn down.
void testStopWithArmedOccupancySweepIsQuiescent()
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

    QTcpSocket first;
    QTcpSocket second;
    CHECK(connectRawRfb(first, port, false));
    CHECK(connectRawRfb(second, port, false));
    CHECK(writeAll(first, QByteArray("\x03\x00\x00\x00", 4)));
    CHECK(writeAll(second, QByteArray("\x04\x00\x00\x00", 4)));

    const std::size_t deadlineFailuresBefore = countRecoverableFailures(recorder, "bounded");
    transport->stop();

    QElapsedTimer settle;
    settle.start();
    while (settle.elapsed() < 800) {
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    CHECK(countRecoverableFailures(recorder, "bounded") == deadlineFailuresBefore);
}

void testInitialTargetGeometrySendsServerInitBeforeFrame()
{
    const quint16 port = freePort();
    CHECK(port != 0);
    if (port == 0)
        return;

    Recorder recorder;
    HyRemote::detail::RfbSecurityConfig security;
    security.initialFramebufferWidth = 640;
    security.initialFramebufferHeight = 480;
    auto transport = HyRemote::detail::createRfbTransport(QHostAddress::LocalHost, port,
                                                          std::move(security));
    CHECK(transport != nullptr);
    if (!transport)
        return;
    CHECK(transport->start([&](const hyremote::InputEvent &event) { recorder.record(event); },
                           [&](const hyremote::TransportEvent &event) { recorder.record(event); }));

    QTcpSocket socket;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    CHECK(connectRawRfb(socket, port, false, &width, &height));
    CHECK(width == 640);
    CHECK(height == 480);
    CHECK(socket.state() == QAbstractSocket::ConnectedState);
    socket.abort();
    transport->stop();
}

void runInteractionLatencyProbe(const std::vector<std::int32_t> &encodings,
                                std::int32_t expectedEncoding,
                                const char *encodingLabel,
                                bool continuousUpdates = false)
{
    constexpr std::size_t sampleCount = 12;
    constexpr std::uint32_t background = 0x203040U;
    constexpr std::uint32_t oddColor = 0xd04030U;
    constexpr std::uint32_t evenColor = 0x30a060U;
    constexpr hyremote::Rect markerRect{8, 8, 8, 8};

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

    hyremote::Transport *transportPtr = transport.get();
    std::size_t pressCount = 0;
    CHECK(transport->start(
        [&](const hyremote::InputEvent &event) {
            recorder.record(event);
            if (event.kind != hyremote::InputEventKind::PointerButton
                || event.button != hyremote::PointerButton::Left || !event.pressed) {
                return;
            }

            ++pressCount;
            const std::uint32_t color = (pressCount % 2U) == 1U ? oddColor : evenColor;
            transportPtr->enqueueFrame(makeFrame(64, 48, background, {{markerRect, color}}));
        },
        [&](const hyremote::TransportEvent &event) { recorder.record(event); }));
    transport->enqueueFrame(makeFrame(64, 48, background));

    QTcpSocket socket;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    CHECK(connectRawRfb(socket, port, false, &width, &height));
    CHECK(width == 64 && height == 48);
    std::vector<std::int32_t> negotiated = encodings;
    if (continuousUpdates)
        negotiated.insert(negotiated.begin(), -313);
    CHECK(sendSetEncodings(socket, negotiated));
    if (continuousUpdates) {
        // A client explicitly advertises ContinuousUpdates (-313); the
        // server confirms it with one EndOfContinuousUpdates byte (150).
        CHECK(readExact(socket, 1) == QByteArray(1, char(150)));
    }
    CHECK(sendUpdateRequest(socket, false, 0, 0, width, height));

    TestFramebuffer frame;
    frame.resize(width, height);
    std::vector<UpdateRect> rectangles;
    CHECK(readFramebufferUpdate(socket, frame, rectangles));
    CHECK(rectangles.size() == 1U && rectangles.front().encoding == expectedEncoding);
    CHECK(frame.get(markerRect.x, markerRect.y) == background);

    if (continuousUpdates) {
        CHECK(sendContinuousUpdates(socket, true, 0, 0, width, height));
        // A static scene may not generate redundant updates just because
        // ContinuousUpdates is armed; subsequent pixels require new damage.
        CHECK(noFramebufferData(socket));
    }

    std::vector<std::chrono::microseconds> samples;
    samples.reserve(sampleCount);

    for (std::size_t index = 0; index < sampleCount; ++index) {
        // The legacy reference path requests one incremental update per
        // interaction. ContinuousUpdates must deliver all later changes
        // without any additional FramebufferUpdateRequest messages.
        if (!continuousUpdates)
            CHECK(sendUpdateRequest(socket, true, 0, 0, width, height));

        const auto started = std::chrono::steady_clock::now();
        CHECK(sendPointerButton(socket, true));
        CHECK(readFramebufferUpdate(socket, frame, rectangles));
        const auto completed = std::chrono::steady_clock::now();

        CHECK(rectangles.size() == 1U && rectangles.front().encoding == expectedEncoding);
        const std::uint32_t expected = (index % 2U) == 0U ? oddColor : evenColor;
        CHECK(frame.get(markerRect.x, markerRect.y) == expected);
        samples.push_back(
            std::chrono::duration_cast<std::chrono::microseconds>(completed - started));

        CHECK(sendPointerButton(socket, false));
    }

    CHECK(samples.size() == sampleCount);
    const auto summary = HyRemote::test::summarizeInteractionLatencies(samples);
    CHECK(summary.sampleCount == sampleCount);
    CHECK(summary.minUs >= 0);
    CHECK(summary.p50Us >= summary.minUs);
    CHECK(summary.p95Us >= summary.p50Us);
    CHECK(summary.maxUs >= summary.p95Us);
    HyRemote::test::printInteractionLatencySummary(std::cout, encodingLabel, summary);

    if (continuousUpdates) {
        CHECK(sendContinuousUpdates(socket, false, 0, 0, 0, 0));
        CHECK(readEndOfContinuousUpdates(socket));
        CHECK(noFramebufferData(socket));
    }

    socket.disconnectFromHost();
    waitForDisconnected(socket);
    transport->stop();
}

double processCpuMs()
{
#ifdef Q_OS_WIN
    // MSVC std::clock() reports wall time, NOT process CPU time.
    // Kernel and user times reported by GetProcessTimes are in 100ns units.
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user))
        return -1.0;
    ULARGE_INTEGER kernelTicks{}, userTicks{};
    kernelTicks.LowPart = kernel.dwLowDateTime;
    kernelTicks.HighPart = kernel.dwHighDateTime;
    userTicks.LowPart = user.dwLowDateTime;
    userTicks.HighPart = user.dwHighDateTime;
    return static_cast<double>(kernelTicks.QuadPart + userTicks.QuadPart) / 10000.0;
#else
    const std::clock_t ticks = std::clock();
    return ticks == static_cast<std::clock_t>(-1)
               ? -1.0
               : 1000.0 * static_cast<double>(ticks) / static_cast<double>(CLOCKS_PER_SEC);
#endif
}

qint64 processWorkingSetKb()
{
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
        return static_cast<qint64>(counters.WorkingSetSize / 1024U);
#else
    QFile statm(QStringLiteral("/proc/self/statm"));
    if (statm.open(QIODevice::ReadOnly)) {
        const QList<QByteArray> fields = statm.readAll().simplified().split(' ');
        if (fields.size() >= 2) {
            const long pageSize = sysconf(_SC_PAGESIZE);
            return static_cast<qint64>(fields.at(1).toLongLong()) * pageSize / 1024;
        }
    }
#endif
    return 0;
}

struct ProfileSample
{
    QString encoding;
    QString workload;
    int frames = 0;
    qint64 bytes = 0;
    double p50Ms = 0.0;
    double p95Ms = 0.0;
    double cpuMs = 0.0;
    double wallMs = 0.0;
    double cpuOneCorePercent = 0.0;
    qint64 workingSetKb = 0;
};

std::vector<ProfileSample> profileSamples;
constexpr int kProfileWidth = 256;
constexpr int kProfileHeight = 256;

double profilePercentile(std::vector<double> values, double percentile)
{
    if (values.empty())
        return 0.0;
    std::sort(values.begin(), values.end());
    const double position = (values.size() - 1) * percentile;
    const auto lower = static_cast<std::size_t>(position);
    const auto upper = std::min(lower + 1, values.size() - 1);
    const double fraction = position - static_cast<double>(lower);
    return values[lower] * (1.0 - fraction) + values[upper] * fraction;
}

void recordProfileSample(const QString &encoding,
                         const QString &workload,
                         int frames,
                         qint64 bytes,
                         std::vector<double> latenciesMs,
                         double cpuMs,
                         double wallMs,
                         qint64 workingSetKb)
{
    ProfileSample sample;
    sample.encoding = encoding;
    sample.workload = workload;
    sample.frames = frames;
    sample.bytes = bytes;
    sample.p50Ms = profilePercentile(latenciesMs, 0.50);
    sample.p95Ms = profilePercentile(latenciesMs, 0.95);
    sample.cpuMs = cpuMs;
    sample.wallMs = wallMs;
    // 100% is one fully used logical CPU; multi-threaded work can exceed 100%.
    sample.cpuOneCorePercent = (cpuMs >= 0.0 && wallMs > 0.0)
                                   ? (100.0 * cpuMs / wallMs)
                                   : -1.0;
    sample.workingSetKb = workingSetKb;
    profileSamples.push_back(sample);
    std::cerr << "PROFILE " << encoding.toStdString() << ' ' << workload.toStdString()
              << " frames=" << frames << " bytes=" << bytes << " p50_ms=" << sample.p50Ms
              << " p95_ms=" << sample.p95Ms << " cpu_ms=" << sample.cpuMs
              << " wall_ms=" << sample.wallMs
              << " cpu_one_core_percent=" << sample.cpuOneCorePercent
              << " working_set_kb=" << sample.workingSetKb << '\n';
}

void runProfileWorkload(const std::vector<std::int32_t> &encodings,
                        const QString &encodingLabel,
                        const QString &workload)
{
    constexpr std::uint16_t kWidth = 256;
    constexpr std::uint16_t kHeight = 256;
    constexpr int kSamples = 16;
    constexpr hyremote::Rect kMarker{32, 24, 8, 8};

    const quint16 port = freePort();
    if (port == 0)
        return;
    QElapsedTimer wallTimer;
    wallTimer.start();
    const double cpuStartMs = processCpuMs();
    Recorder recorder;
    auto transport = startTransport(port, recorder, makeFrame(kWidth, kHeight, 0x203040U));
    if (!transport)
        return;

    QTcpSocket socket;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    if (!connectRawRfb(socket, port, false, &width, &height)
        || !sendSetEncodings(socket, encodings)
        || !sendUpdateRequest(socket, false, 0, 0, width, height)) {
        transport->stop();
        return;
    }

    TestFramebuffer frame;
    frame.resize(width, height);
    std::vector<UpdateRect> rectangles;
    if (!readFramebufferUpdate(socket, frame, rectangles)) {
        transport->stop();
        return;
    }

    std::vector<double> latenciesMs;
    qint64 measuredBytes = 0;
    int measuredFrames = 0;

    if (workload == QStringLiteral("static")) {
        profileBytesRead = 0;
        if (!sendUpdateRequest(socket, true, 0, 0, width, height)) {
            transport->stop();
            return;
        }
        noFramebufferData(socket, 250);
        measuredBytes = profileBytesRead;
        measuredFrames = 1;
        latenciesMs.push_back(0.0);
    } else if (workload == QStringLiteral("localized") || workload == QStringLiteral("high-motion")) {
        for (int index = 0; index < kSamples; ++index) {
            const std::uint32_t color = 0xd0a030U + static_cast<std::uint32_t>(index) * 0x000101U;
            if (workload == QStringLiteral("localized")) {
                auto changed = makeFrame(kWidth, kHeight, 0x203040U, {{kMarker, color}});
                changed.damage = hyremote::Damage::fromRegions({kMarker});
                transport->enqueueFrame(std::move(changed));
            } else {
                transport->enqueueFrame(makeFrame(kWidth, kHeight, color));
            }

            profileBytesRead = 0;
            const auto started = std::chrono::steady_clock::now();
            if (!sendUpdateRequest(socket, true, 0, 0, width, height)
                || !readFramebufferUpdate(socket, frame, rectangles)) {
                break;
            }
            const auto completed = std::chrono::steady_clock::now();
            latenciesMs.push_back(
                std::chrono::duration<double, std::milli>(completed - started).count());
            measuredBytes += profileBytesRead;
            ++measuredFrames;
        }
    } else if (workload == QStringLiteral("full-refresh")) {
        profileBytesRead = 0;
        const auto started = std::chrono::steady_clock::now();
        if (sendUpdateRequest(socket, false, 0, 0, width, height)
            && readFramebufferUpdate(socket, frame, rectangles)) {
            const auto completed = std::chrono::steady_clock::now();
            latenciesMs.push_back(
                std::chrono::duration<double, std::milli>(completed - started).count());
            measuredBytes += profileBytesRead;
            measuredFrames = 1;
        }
    }

    const double cpuEndMs = processCpuMs();
    recordProfileSample(encodingLabel,
                        workload,
                        measuredFrames,
                        measuredBytes,
                        latenciesMs,
                        (cpuStartMs >= 0.0 && cpuEndMs >= cpuStartMs)
                            ? (cpuEndMs - cpuStartMs)
                            : -1.0,
                        static_cast<double>(wallTimer.nsecsElapsed()) / 1000000.0,
                        processWorkingSetKb());
    socket.disconnectFromHost();
    waitForDisconnected(socket);
    transport->stop();
}

void runDeliveryProfile(const QString &outputPath)
{
    runProfileWorkload({0}, QStringLiteral("raw"), QStringLiteral("static"));
    runProfileWorkload({0}, QStringLiteral("raw"), QStringLiteral("localized"));
    runProfileWorkload({0}, QStringLiteral("raw"), QStringLiteral("high-motion"));
    runProfileWorkload({0}, QStringLiteral("raw"), QStringLiteral("full-refresh"));
    runProfileWorkload({15, 0}, QStringLiteral("trle"), QStringLiteral("static"));
    runProfileWorkload({15, 0}, QStringLiteral("trle"), QStringLiteral("localized"));
    runProfileWorkload({15, 0}, QStringLiteral("trle"), QStringLiteral("high-motion"));
    runProfileWorkload({15, 0}, QStringLiteral("trle"), QStringLiteral("full-refresh"));

    QJsonArray samples;
    for (const ProfileSample &sample : profileSamples) {
        QJsonObject entry;
        entry.insert(QStringLiteral("encoding"), sample.encoding);
        entry.insert(QStringLiteral("workload"), sample.workload);
        entry.insert(QStringLiteral("frames"), sample.frames);
        entry.insert(QStringLiteral("bytes"), static_cast<double>(sample.bytes));
        entry.insert(QStringLiteral("p50_ms"), sample.p50Ms);
        entry.insert(QStringLiteral("p95_ms"), sample.p95Ms);
        entry.insert(QStringLiteral("cpu_ms"), sample.cpuMs);
        entry.insert(QStringLiteral("wall_ms"), sample.wallMs);
        entry.insert(QStringLiteral("cpu_one_core_percent"), sample.cpuOneCorePercent);
        entry.insert(QStringLiteral("working_set_kb"), static_cast<double>(sample.workingSetKb));
        samples.append(entry);
    }

    QJsonObject root;
    root.insert(QStringLiteral("schema"), QStringLiteral("hyremote-rfb-delivery-profile/1"));
    root.insert(QStringLiteral("cpu_measurement_scope"),
                QStringLiteral("whole test process (fixture + loopback peer); not Host, Viewer or encoder-only"));
    root.insert(QStringLiteral("cpu_one_core_percent_unit"),
                QStringLiteral("100 = one logical CPU busy; can exceed 100 with multiple threads"));
    root.insert(QStringLiteral("cpu_unavailable_value"), -1);
    root.insert(QStringLiteral("platform"), QSysInfo::prettyProductName());
    root.insert(QStringLiteral("qt"), QString::fromLatin1(qVersion()));
    root.insert(QStringLiteral("width"), kProfileWidth);
    root.insert(QStringLiteral("height"), kProfileHeight);
    root.insert(QStringLiteral("samples"), samples);

    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        std::cerr << "PROFILE failed to open " << outputPath.toStdString() << '\n';
        ++failures;
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    std::cerr << "PROFILE wrote " << profileSamples.size() << " samples to "
              << outputPath.toStdString() << '\n';
}

// #789: A full-viewport incremental FBU usually consumes ALL accumulated
// damage. With 64 scattered rectangles, the old commit path rebuilt/split
// the pending vector for every one of 64 delivered rectangles, despite the
// mathematically empty final difference. The fast path may only clear when
// each pending rectangle is contained in a single delivered rectangle.
void testFullyDeliveredSparseDamageHasNoPhantomPendingUpdates()
{
    using HyRemote::detail::RfbDamageRegion;
    using HyRemote::detail::RfbRect;
    using HyRemote::detail::RfbUpdateState;
    constexpr RfbRect viewport{0, 0, 1920, 1080};
    RfbUpdateState viewer;
    std::vector<RfbRect> tiles;
    tiles.reserve(HyRemote::detail::kRfbMaxDamageRects);
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            const RfbRect tile{col * 128, row * 120, 16, 16};
            RfbDamageRegion delta;
            delta.add(tile);
            viewer.accumulate(delta);
            tiles.push_back(tile);
        }
    }
    CHECK(tiles.size() == HyRemote::detail::kRfbMaxDamageRects);
    viewer.request(true, viewport);
    const auto first = viewer.selectEligible();
    CHECK(first.has_value());
    if (!first)
        return;
    CHECK(first->size() == HyRemote::detail::kRfbMaxDamageRects);
    for (const RfbRect &tile : tiles) {
        CHECK(std::any_of(first->rects().begin(), first->rects().end(),
                          [&tile](const RfbRect &r) { return r == tile; }));
    }
    viewer.commitDelivered(*first);
    CHECK(!viewer.hasPendingDamage());
    CHECK(!viewer.hasOutstandingRequest());
    viewer.request(true, viewport);
    CHECK(!viewer.selectEligible().has_value());
    viewer.cancelRequest();

    // A single larger transmitted rectangle is also sufficient to consume
    // all pending fragments. It is NOT necessary to match the exact list.
    RfbDamageRegion pending;
    for (const RfbRect &tile : tiles)
        pending.add(tile);
    RfbDamageRegion sent;
    sent.add(viewport);
    pending.subtract(sent);
    CHECK(pending.empty());
}

void testDeliveryCoverageIsConservativeForPartialAndOverlappingCuts()
{
    using HyRemote::detail::RfbDamageRegion;
    using HyRemote::detail::RfbRect;
    // Partial FBU: the left half is delivered, the right half MUST remain.
    RfbDamageRegion pending;
    pending.add({0, 0, 100, 100});
    RfbDamageRegion sent;
    sent.add({0, 0, 50, 100});
    pending.subtract(sent);
    CHECK(pending.size() == 1U);
    if (pending.size() == 1U)
        CHECK(pending.rects().front() == (RfbRect{50, 0, 50, 100}));
    RfbDamageRegion second;
    second.add({50, 0, 50, 100});
    pending.subtract(second);
    CHECK(pending.empty());

    // A pending region may be covered by the UNION of two delivered
    // overlapping rectangles while neither single rectangle contains it.
    // This must take the exact multi-cut path, not falsely shortcut.
    RfbDamageRegion unionCovered;
    unionCovered.add({0, 0, 100, 100});
    RfbDamageRegion overlappingCuts;
    overlappingCuts.add({0, 0, 60, 100});
    overlappingCuts.add({40, 0, 60, 100});
    CHECK(overlappingCuts.size() == 2U);
    unionCovered.subtract(overlappingCuts);
    CHECK(unionCovered.empty());

    // Multiple pending regions, but a delivery of only one of them: no
    // collateral clear of a later independently owned change.
    RfbDamageRegion twoRegions;
    twoRegions.add({0, 0, 20, 20});
    twoRegions.add({80, 80, 20, 20});
    RfbDamageRegion oneSent;
    oneSent.add({0, 0, 20, 20});
    twoRegions.subtract(oneSent);
    CHECK(twoRegions.size() == 1U);
    if (twoRegions.size() == 1U)
        CHECK(twoRegions.rects().front() == (RfbRect{80, 80, 20, 20}));
}

// An over-capacity sparse pending set may contain overlapping bounding boxes.
// When a Viewer consumes the entire selected update, commitDelivered must
// remove ALL pending change, not resurrect an earlier cut because an
// intermediate subtraction re-applied the lossy 64-rectangle bound.
// A 200-region workload is deliberately kept below the 25% large-paint policy
// so the test exercises many distinct cuts instead of one full-page clear.
void testSparseDeliveredCutsDoNotResurrectPendingDamage()
{
    using HyRemote::detail::RfbDamageRegion;
    using HyRemote::detail::RfbRect;
    constexpr RfbRect viewport{0, 0, 3840, 2160};
    std::vector<RfbRect> patches;
    patches.reserve(200U);
    HyRemote::detail::RfbUpdateState viewer;
    for (int y = 0; y < 10; ++y) {
        for (int x = 0; x < 20; ++x) {
            const RfbRect rect{x * 192, y * 192, 16, 16};
            patches.push_back(rect);
            RfbDamageRegion delta;
            delta.add(rect);
            viewer.accumulate(delta);
        }
    }
    CHECK(patches.size() == 200U);
    viewer.request(true, viewport);
    const auto selected = viewer.selectEligible();
    CHECK(selected.has_value());
    if (!selected)
        return;
    CHECK(selected->size() > 1U);
    CHECK(selected->size() <= HyRemote::detail::kRfbMaxDamageRects);
    for (const RfbRect &tile : patches) {
        CHECK(std::any_of(selected->rects().begin(), selected->rects().end(),
                          [&tile](const RfbRect &box) {
                              return HyRemote::detail::containsRfbRect(box, tile);
                          }));
    }
    viewer.commitDelivered(*selected);
    CHECK(!viewer.hasPendingDamage());
    CHECK(!viewer.hasOutstandingRequest());
    // No new frame: a fresh incremental FBU must not re-send the same pixels.
    viewer.request(true, viewport);
    CHECK(!viewer.selectEligible().has_value());
    viewer.cancelRequest();
}

void testPendingDamageCoalescesAtDelivery()
{
    HyRemote::detail::RfbUpdateState state;
    const HyRemote::detail::RfbRect viewport{0, 0, 256, 256};
    HyRemote::detail::RfbDamageRegion damage;
    damage.add({0, 0, 128, 128});
    damage.add({128, 0, 128, 128});
    damage.add({0, 128, 128, 128});
    damage.add({128, 128, 128, 128});
    state.accumulate(damage);
    state.request(true, viewport);
    const auto selected = state.selectEligible();
    CHECK(selected.has_value());
    if (selected) {
        const HyRemote::detail::RfbRect expectedFull{0, 0, 256, 256};
        CHECK(selected->size() == 1U);
        if (selected->size() == 1U)
            CHECK(selected->rects().front() == expectedFull);
    }
}

// #789: 65 sparse-but-row-contiguous 64px changed tiles are only ~13% of
// a 1920x1080 page. Previously the hard 64-rect bound joined the 65th tile
// to a near/full-page rectangle before the 25% paint policy could even run.
// Only exact unions are allowed: no unchanged pixel may be invented.
void testExactTileUnionPreventsSparseFullPageInflation()
{
    using HyRemote::detail::RfbDamageRegion;
    using HyRemote::detail::RfbRect;
    constexpr std::uint32_t width = 1920;
    constexpr std::uint32_t height = 1080;
    RfbDamageRegion actual;
    std::uint64_t tileArea = 0;
    std::size_t tileCount = 0;
    for (std::uint32_t row = 0; row < 17U; ++row) {
        const std::uint32_t y = row * 64U;
        const std::uint32_t tileHeight = std::min<std::uint32_t>(64U, height - y);
        const std::uint32_t firstX = (row % 2U == 0U) ? 0U : (width - 256U);
        const std::uint32_t tilesInRow = row == 16U ? 1U : 4U;
        for (std::uint32_t col = 0; col < tilesInRow; ++col) {
            actual.add({static_cast<std::int32_t>(firstX + col * 64U),
                        static_cast<std::int32_t>(y), 64,
                        static_cast<std::int32_t>(tileHeight)});
            tileArea += 64U * tileHeight;
            ++tileCount;
        }
    }
    CHECK(tileCount == 65U);
    CHECK(tileArea * 100U < static_cast<std::uint64_t>(width) * height * 25U);
    CHECK(actual.size() == 17U); // no global 64-rect bounding fallback

    std::uint64_t deliveredArea = 0;
    for (const RfbRect &rect : actual.rects())
        deliveredArea += static_cast<std::uint64_t>(rect.width) * rect.height;
    CHECK(deliveredArea == tileArea); // no unchanged bytes added by exact union

    const auto painted = HyRemote::detail::coalesceRfbDamageForPaint(actual, width, height);
    CHECK(painted.size() == 17U); // below intentional 25% large-paint threshold

    // A genuine 25% spread-out page transition must still collapse to one
    // paint rectangle. Avoid reintroducing the top-to-bottom banding complaint.
    RfbDamageRegion quarter;
    quarter.add({0, 0, 64, 64});
    quarter.add({192, 0, 64, 64});
    quarter.add({0, 192, 64, 64});
    quarter.add({192, 192, 64, 64});
    const auto wide = HyRemote::detail::coalesceRfbDamageForPaint(quarter, 256U, 256U);
    CHECK(wide.size() == 1U);
    if (wide.size() == 1U)
        CHECK(wide.rects().front() == (RfbRect{0, 0, 256, 256}));

    // Only full-edge adjacency is exact. Diagonal touching is not a union.
    RfbDamageRegion diagonal;
    diagonal.add({0, 0, 64, 64});
    diagonal.add({64, 64, 64, 64});
    CHECK(diagonal.size() == 2U);
}

// #789/#567: compare the REAL RGB changes before applying lossy 64-rect
// bounding. A 1080p checkerboard has >200 disjoint dirty tiles and covers
// roughly half the screen: scan should collapse once, not re-run the greedy
// 64-pair search after each tile. This is the same single-rect paint rule
// already required for an actual large page transition.
void testDenseChangedTilesAvoidSparseBoundingAndPaintAsOne()
{
    using HyRemote::detail::RfbRect;
    constexpr std::uint32_t width = 1920U, height = 1080U;
    std::vector<std::pair<hyremote::Rect, std::uint32_t>> patches;
    std::uint64_t actualChangedTileArea = 0;
    for (std::uint32_t row = 0; row < 17U; ++row) {
        const std::uint32_t tileHeight = std::min<std::uint32_t>(64U, height - row * 64U);
        for (std::uint32_t col = 0; col < 30U; ++col) {
            if ((row + col) % 2U != 0U)
                continue;
            patches.push_back({{static_cast<std::int32_t>(col * 64U),
                                static_cast<std::int32_t>(row * 64U),
                                64U, tileHeight}, 0xD22C13U});
            actualChangedTileArea += 64U * tileHeight;
        }
    }
    CHECK(patches.size() > HyRemote::detail::kRfbMaxDamageRects * 3U);
    CHECK(actualChangedTileArea * 4U >= static_cast<std::uint64_t>(width) * height);
    const auto oldFrame = makeFrame(width, height, 0x102030U);
    const auto newFrame = makeFrame(width, height, 0x102030U, patches);
    CHECK(oldFrame.storage && newFrame.storage);
    if (!oldFrame.storage || !newFrame.storage)
        return;

    const auto changed = HyRemote::detail::changedRfbTiles(&oldFrame, newFrame);
    CHECK(changed.size() == 1U);
    if (changed.size() == 1U)
        CHECK(changed.rects().front() == (RfbRect{0, 0, 1920, 1080}));
    HyRemote::detail::RfbUpdateState viewer;
    viewer.accumulate(changed);
    viewer.request(true, {0, 0, 1920, 1080});
    const auto eligible = viewer.selectEligible();
    CHECK(eligible.has_value());
    if (eligible) {
        CHECK(eligible->size() == 1U);
        CHECK(eligible->rects().front() == (RfbRect{0, 0, 1920, 1080}));
        viewer.commitDelivered(*eligible);
        CHECK(!viewer.hasPendingDamage());
    }
}

// Exact threshold, one tile below, and clipped far-edge tiles must retain
// the original 64x64 RFB granularity. The large-paint decision uses the
// ACTUAL changed tile area, not the sum of intermediate greedy boxes.
void testChangedTileAreaThresholdAndClippedEdges()
{
    using HyRemote::detail::RfbRect;
    using Patch = std::pair<hyremote::Rect, std::uint32_t>;
    const auto old256 = makeFrame(256, 256, 0x123456U);
    const std::vector<Patch> threeCorners = {
        {{0, 0, 64, 64}, 0xE13223U},
        {{192, 0, 64, 64}, 0xE13223U},
        {{0, 192, 64, 64}, 0xE13223U}
    };
    const auto sparse = makeFrame(256, 256, 0x123456U, threeCorners);
    const auto stillSparse = HyRemote::detail::changedRfbTiles(&old256, sparse);
    CHECK(stillSparse.size() == 3U); // 18.75%, not a full-screen paint
    if (stillSparse.size() == 3U) {
        for (const auto &patch : threeCorners) {
            const RfbRect expected{patch.first.x, patch.first.y, 64, 64};
            CHECK(std::find(stillSparse.rects().begin(), stillSparse.rects().end(),
                            expected) != stillSparse.rects().end());
        }
    }

    auto fourCorners = threeCorners;
    fourCorners.push_back({{192, 192, 64, 64}, 0xE13223U});
    const auto quarter = makeFrame(256, 256, 0x123456U, fourCorners);
    const auto large = HyRemote::detail::changedRfbTiles(&old256, quarter);
    CHECK(large.size() == 1U); // exactly 25%, intentional paint collapse
    if (large.size() == 1U)
        CHECK(large.rects().front() == (RfbRect{0, 0, 256, 256}));

    const auto oldEdge = makeFrame(130, 130, 0x123456U);
    const auto newEdge = makeFrame(130, 130, 0x123456U,
                                 std::vector<Patch>{{{128, 128, 2, 2}, 0xE13223U}});
    const auto edgeDamage = HyRemote::detail::changedRfbTiles(&oldEdge, newEdge);
    CHECK(edgeDamage.size() == 1U);
    if (edgeDamage.size() == 1U)
        CHECK(edgeDamage.rects().front() == (RfbRect{128, 128, 2, 2}));
}

// #789: 65 fully DISJOINT 64x64 tiles spread across a 1080p page must
// still fit the 64-region bound WITHOUT turning ~13% real changes into an
// almost full-screen rectangle. #888's exact-edge unions cannot help here.
// Use the actual last-frame RGBA comparison and the per-viewer delivery state,
// rather than asserting on an artificial list of rectangles alone.
void testDisjointSparseOverflowDoesNotInflateFullPage()
{
    using HyRemote::detail::RfbDamageRegion;
    using HyRemote::detail::RfbRect;
    constexpr std::uint32_t width = 1920U;
    constexpr std::uint32_t height = 1080U;
    constexpr int tile = 64;
    constexpr std::uint64_t trueArea = 65ULL * 64ULL * 64ULL;
    constexpr std::uint64_t maxCoalescedArea = trueArea + 64ULL * 64ULL;

    std::vector<std::pair<hyremote::Rect, std::uint32_t>> patches;
    std::vector<RfbRect> expected;
    patches.reserve(65U);
    expected.reserve(65U);
    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col < 13; ++col) {
            // Separate tile columns by one full unchanged tile, and rows
            // by two. No pair shares a full edge; any coalescing is bounded.
            const RfbRect rect{col * 128, row * 192, tile, tile};
            expected.push_back(rect);
            patches.push_back({{rect.x, rect.y, tile, tile}, 0xa91b43U});
        }
    }
    CHECK(expected.size() == 65U);
    const auto oldFrame = makeFrame(width, height, 0x112233U);
    const auto newFrame = makeFrame(width, height, 0x112233U, patches);
    CHECK(oldFrame.storage && newFrame.storage);
    if (!oldFrame.storage || !newFrame.storage)
        return;
    const RfbDamageRegion changed =
        HyRemote::detail::changedRfbTiles(&oldFrame, newFrame);
    CHECK(changed.size() == HyRemote::detail::kRfbMaxDamageRects);

    std::uint64_t transmittedArea = 0;
    for (const auto &rect : changed.rects())
        transmittedArea += static_cast<std::uint64_t>(rect.width) * rect.height;
    CHECK(transmittedArea >= trueArea);
    // Exactly one small-gap merge is enough. Old hard-bound fallback creates
    // a ~1600x832 rectangle, ~5x this conservative upper bound.
    CHECK(transmittedArea <= maxCoalescedArea);
    CHECK(transmittedArea * 100U < static_cast<std::uint64_t>(width) * height * 25U);
    for (const RfbRect &tileRect : expected) {
        CHECK(std::any_of(changed.rects().begin(), changed.rects().end(),
                          [&tileRect](const RfbRect &rect) {
                              return HyRemote::detail::containsRfbRect(rect, tileRect);
                          }));
    }
    const auto paint =
        HyRemote::detail::coalesceRfbDamageForPaint(changed, width, height);
    CHECK(paint.size() == HyRemote::detail::kRfbMaxDamageRects);

    HyRemote::detail::RfbUpdateState viewer;
    viewer.accumulate(changed);
    viewer.request(true, {0, 0, static_cast<int>(width), static_cast<int>(height)});
    const auto eligible = viewer.selectEligible();
    CHECK(eligible.has_value());
    if (!eligible)
        return;
    CHECK(eligible->size() == HyRemote::detail::kRfbMaxDamageRects);
    for (const RfbRect &tileRect : expected) {
        CHECK(std::any_of(eligible->rects().begin(), eligible->rects().end(),
                          [&tileRect](const RfbRect &rect) {
                              return HyRemote::detail::containsRfbRect(rect, tileRect);
                          }));
    }
    viewer.commitDelivered(*eligible);
    CHECK(!viewer.hasPendingDamage());
    CHECK(!viewer.hasOutstandingRequest());
}

void testLargeDamageCoalescesForPaint()
{
    HyRemote::detail::RfbDamageRegion large;
    for (int y = 0; y < 256; y += 64) {
        for (int x = 0; x < 256; x += 64)
            large.add({x, y, 64, 64});
    }
    const auto painted = HyRemote::detail::coalesceRfbDamageForPaint(large, 256U, 256U);
    const HyRemote::detail::RfbRect expectedFull{0, 0, 256, 256};
    CHECK(painted.size() == 1U);
    if (painted.size() == 1U)
        CHECK(painted.rects().front() == expectedFull);

    HyRemote::detail::RfbDamageRegion smallDamage;
    smallDamage.add({0, 0, 8, 8});
    smallDamage.add({100, 100, 8, 8});
    CHECK(HyRemote::detail::coalesceRfbDamageForPaint(smallDamage, 256U, 256U).size() == 2U);
}

}  // namespace



void testRfbFeedbackUsesActualServedPixels()
{
    const quint16 port = freePort();
    CHECK(port != 0);
    if (!port)
        return;
    Recorder recorder;
    std::atomic<int> unchanged{0}, changed{0};
    auto transport = startTransport(
        port, recorder, makeFrame(128, 128, 0x203040U), {}, {},
        [&](bool differs) {
            if (differs)
                ++changed;
            else
                ++unchanged;
        });
    CHECK(transport != nullptr);
    if (!transport)
        return;

    QTcpSocket socket;
    std::uint16_t width = 0, height = 0;
    const bool connected = connectRawRfb(socket, port, false, &width, &height);
    CHECK(connected && width == 128 && height == 128);
    if (!connected) {
        transport->stop();
        return;
    }
    CHECK(sendSetEncodings(socket, {0}));
    CHECK(sendUpdateRequest(socket, false, 0, 0, width, height));
    TestFramebuffer actual;
    actual.resize(width, height);
    std::vector<UpdateRect> rectangles;
    CHECK(readFramebufferUpdate(socket, actual, rectangles));

    const auto reached = [](const std::atomic<int> &counter, int expected) {
        QElapsedTimer timer;
        timer.start();
        while (counter.load() < expected && timer.elapsed() < 2000)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        return counter.load() >= expected;
    };
    const int beforeUnchanged = unchanged.load();
    transport->enqueueFrame(makeFrame(128, 128, 0x203040U));
    CHECK(reached(unchanged, beforeUnchanged + 1)); // fullFrame hint is NOT proof of changed pixels

    const int beforeChanged = changed.load();
    CHECK(sendUpdateRequest(socket, true, 0, 0, width, height));
    transport->enqueueFrame(makeFrame(128, 128, 0x203040U, {{{70, 10, 8, 8}, 0xe0a030U}}));
    CHECK(reached(changed, beforeChanged + 1));
    CHECK(readFramebufferUpdate(socket, actual, rectangles));
    CHECK(actual.get(70, 10) == 0xe0a030U); // matching feedback is also what viewer received

    socket.disconnectFromHost();
    CHECK(waitForDisconnected(socket));
    transport->stop();
}

void testStaticViewerCaptureDemandFeedback()
{
    HyRemote::detail::ViewerCaptureDemand demand;
    demand.start();
    demand.frameQueued(true);
    std::atomic<bool> finished{false};
    std::atomic<bool> permitted{false};
    std::thread bootstrapWaiter([&] {
        permitted.store(demand.waitForViewer());
        finished.store(true);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    CHECK(!finished.load()); // no viewer -> no repeated full grabs
    demand.setViewerCount(1);
    bootstrapWaiter.join();
    CHECK(finished.load() && permitted.load());

    // Existing transport did the exact tile diff. Three genuinely identical served frames
    // enter the bounded background probe; one changed frame or input resumes active captures.
    for (int n = 0; n < 3; ++n)
        demand.noteFrameChange(false);
    const auto began = std::chrono::steady_clock::now();
    CHECK(demand.waitForViewer()); // must wait for a bounded local-only update probe
    const auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - began);
    CHECK(waited.count() >= 30);
    CHECK(waited.count() < 1000);
    demand.noteRemoteInput();
    const auto fast = std::chrono::steady_clock::now();
    CHECK(demand.waitForViewer());
    CHECK(std::chrono::steady_clock::now() - fast < std::chrono::milliseconds(100));

    for (int n = 0; n < 3; ++n)
        demand.noteFrameChange(false);
    std::atomic<bool> woke{false};
    std::thread blocked([&] {
        (void)demand.waitForViewer();
        woke.store(true);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    demand.noteFrameChange(true);
    blocked.join();
    CHECK(woke.load()); // changed rendered pixels immediately cancel the idle gate

    demand.setViewerCount(0);
    std::atomic<bool> released{false};
    std::thread zero([&] {
        (void)demand.waitForViewer();
        released.store(true);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    CHECK(!released.load());
    demand.stop();
    zero.join();
    CHECK(released.load()); // stop does not wait for another Viewer
    demand.start();
    demand.frameQueued(false);
    CHECK(demand.waitForViewer()); // new run can bootstrap without a Viewer
    demand.stop();
}


// #567: exact 1080p full-screen profiling is opt-in only. Keep this out of the
// default CI suite: it measures a synthetic loopback peer, not RealVNC or RK3588.
hyremote::RemoteFrame makeTexturedPageFrame(std::uint32_t width,
                                            std::uint32_t height,
                                            std::uint32_t seed)
{
    auto storage = hyremote::CpuFrameStorage::createSinglePlane(width * 4U, height);
    if (!storage || !storage->mutablePlane(0))
        return {};
    auto *pixels = reinterpret_cast<std::uint8_t *>(storage->mutablePlane(0));
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::size_t offset = (static_cast<std::size_t>(y) * width + x) * 4U;
            pixels[offset] = static_cast<std::uint8_t>((17U * x + 31U * y + 53U * seed) & 0xffU);
            pixels[offset + 1] = static_cast<std::uint8_t>((7U * x + 97U * y + 29U * seed) & 0xffU);
            pixels[offset + 2] = static_cast<std::uint8_t>((151U * x + 23U * y + 11U * seed) & 0xffU);
            pixels[offset + 3] = 0xffU;
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

void testFullPageTransitionSingleWireRectangle()
{
    constexpr std::uint16_t width = 512;
    constexpr std::uint16_t height = 256;
    constexpr std::uint32_t newPageColor = 0xa040f0U;
    for (const bool trle : {false, true}) {
        const quint16 port = freePort();
        CHECK(port != 0);
        if (!port)
            return;
        Recorder recorder;
        auto transport = startTransport(port, recorder, makeFrame(width, height, 0x203040U));
        CHECK(transport != nullptr);
        if (!transport)
            return;
        QTcpSocket viewer;
        std::uint16_t actualWidth = 0, actualHeight = 0;
        const bool joined = connectRawRfb(viewer, port, false, &actualWidth, &actualHeight);
        CHECK(joined && actualWidth == width && actualHeight == height);
        if (!joined) {
            transport->stop();
            continue;
        }
        CHECK(sendSetEncodings(viewer, trle ? std::vector<std::int32_t>{15, 0}
                                           : std::vector<std::int32_t>{0}));
        CHECK(sendUpdateRequest(viewer, false, 0, 0, width, height));
        TestFramebuffer received;
        received.resize(width, height);
        std::vector<UpdateRect> rects;
        CHECK(readFramebufferUpdate(viewer, received, rects));
        CHECK(received.get(0, 0) == 0x203040U);

        // A complete changed page should reach the wire as ONE coherent RFB
        // FramebufferUpdate with ONE full-frame rectangle, not 64px row bands.
        CHECK(sendUpdateRequest(viewer, true, 0, 0, width, height));
        transport->enqueueFrame(makeFrame(width, height, newPageColor));
        const bool gotPage = readFramebufferUpdate(viewer, received, rects);
        CHECK(gotPage);
        if (gotPage) {
            CHECK(rects.size() == 1U);
            if (rects.size() == 1U) {
                CHECK(rects.front().x == 0 && rects.front().y == 0);
                CHECK(rects.front().width == width && rects.front().height == height);
                CHECK(rects.front().encoding == (trle ? 15 : 0));
            }
            CHECK(received.get(0, 0) == newPageColor);
            CHECK(received.get(width / 2, height / 2) == newPageColor);
            CHECK(received.get(width - 1, height - 1) == newPageColor);
        }
        viewer.disconnectFromHost();
        waitForDisconnected(viewer);
        transport->stop();
    }
}

// A deliberately expensive, manually invoked test fixture for exact 1920x1080
// wire delivery. Values include synthetic loopback decoding; NOT board capture,
// network bitrate/RTT, RealVNC painting time, or a product performance SLA.
void runLargePageTransitionProfile(const QString &outputPath)
{
    constexpr std::uint16_t width = 1920;
    constexpr std::uint16_t height = 1080;
    QJsonArray samples;
    for (const bool trle : {false, true}) {
        const quint16 port = freePort();
        CHECK(port != 0);
        if (!port)
            return;
        Recorder recorder;
        auto transport = startTransport(port, recorder, makeFrame(width, height, 0x203040U));
        CHECK(transport != nullptr);
        if (!transport)
            return;

        QTcpSocket viewer;
        std::uint16_t rw = 0, rh = 0;
        const bool joined = connectRawRfb(viewer, port, false, &rw, &rh);
        if (!joined || rw != width || rh != height
            || !sendSetEncodings(viewer, trle ? std::vector<std::int32_t>{15, 0}
                                              : std::vector<std::int32_t>{0})
            || !sendUpdateRequest(viewer, false, 0, 0, width, height)) {
            CHECK(false);
            viewer.abort();
            transport->stop();
            continue;
        }
        TestFramebuffer received;
        received.resize(width, height);
        std::vector<UpdateRect> rects;
        if (!readFramebufferUpdate(viewer, received, rects)) {
            CHECK(false);
            viewer.abort();
            transport->stop();
            continue;
        }
        auto changed = makeTexturedPageFrame(width, height, 13U);
        if (!changed.storage) {
            CHECK(false);
            viewer.abort();
            transport->stop();
            continue;
        }
        profileBytesRead = 0;
        QElapsedTimer timer;
        timer.start();
        const bool sentRequest = sendUpdateRequest(viewer, true, 0, 0, width, height);
        if (sentRequest)
            transport->enqueueFrame(std::move(changed));
        const bool gotPage = sentRequest && readFramebufferUpdate(viewer, received, rects);
        const double completeMs = static_cast<double>(timer.nsecsElapsed()) / 1000000.0;
        const qint64 wireBytesRead = profileBytesRead;

        QJsonObject record;
        record.insert(QStringLiteral("encoding"), trle ? QStringLiteral("trle") : QStringLiteral("raw"));
        record.insert(QStringLiteral("received"), gotPage);
        record.insert(QStringLiteral("wire_bytes_read"), static_cast<double>(wireBytesRead));
        record.insert(QStringLiteral("request_to_full_decode_ms"), completeMs);
        record.insert(QStringLiteral("fbu_count"), gotPage ? 1 : 0);
        record.insert(QStringLiteral("rectangle_count"), static_cast<int>(rects.size()));
        QJsonArray rectangleList;
        if (gotPage) {
            for (const UpdateRect &rect : rects) {
                QJsonObject bounds;
                bounds.insert(QStringLiteral("x"), static_cast<int>(rect.x));
                bounds.insert(QStringLiteral("y"), static_cast<int>(rect.y));
                bounds.insert(QStringLiteral("width"), static_cast<int>(rect.width));
                bounds.insert(QStringLiteral("height"), static_cast<int>(rect.height));
                bounds.insert(QStringLiteral("encoding"), rect.encoding);
                rectangleList.append(bounds);
            }
            CHECK(rects.size() == 1U);
            if (rects.size() == 1U) {
                CHECK(rects.front().x == 0 && rects.front().y == 0
                      && rects.front().width == width && rects.front().height == height
                      && rects.front().encoding == (trle ? 15 : 0));
            }
            const auto expectedColor = [](std::uint32_t x, std::uint32_t y) {
                constexpr std::uint32_t seed = 13U;
                const std::uint32_t red = (17U * x + 31U * y + 53U * seed) & 0xffU;
                const std::uint32_t green = (7U * x + 97U * y + 29U * seed) & 0xffU;
                const std::uint32_t blue = (151U * x + 23U * y + 11U * seed) & 0xffU;
                return (red << 16U) | (green << 8U) | blue;
            };
            CHECK(received.get(0, 0) == expectedColor(0, 0));
            CHECK(received.get(width / 2, height / 2) == expectedColor(width / 2, height / 2));
            CHECK(received.get(width - 1, height - 1) == expectedColor(width - 1, height - 1));
        }
        record.insert(QStringLiteral("rectangles"), rectangleList);
        samples.append(record);
        std::cerr << "LARGE_PAGE_PROFILE " << (trle ? "trle" : "raw")
                  << " bytes=" << wireBytesRead << " complete_ms=" << completeMs
                  << " rects=" << rects.size() << " received=" << gotPage << '\n';
        viewer.disconnectFromHost();
        waitForDisconnected(viewer);
        transport->stop();
    }
    QJsonObject root;
    root.insert(QStringLiteral("schema"), QStringLiteral("hyremote-rfb-large-page-profile/1"));
    root.insert(QStringLiteral("platform"), QSysInfo::prettyProductName());
    root.insert(QStringLiteral("qt"), QString::fromLatin1(qVersion()));
    root.insert(QStringLiteral("width"), width);
    root.insert(QStringLiteral("height"), height);
    root.insert(QStringLiteral("measurement_scope"),
                QStringLiteral("synthetic loopback full FBU decode incl client; not Host capture, link throughput, or RealVNC painting"));
    root.insert(QStringLiteral("samples"), samples);
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        std::cerr << "LARGE_PAGE_PROFILE unable to open output " << outputPath.toStdString() << '\n';
        ++failures;
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
}

// #567: damage and encoded pixels must come from the same frame snapshot. The
// observer runs inside frameAvailable() after the damage diff and before the
// client loop, so enqueueing B there deterministically creates the A-damage /
// B-pixel interleaving without sleeps or a second producer thread.
void testDamageAndEncodedFrameShareSnapshot()
{
    const quint16 port = freePort();
    CHECK(port != 0);
    if (port == 0)
        return;

    Recorder recorder;
    std::atomic<int> phase{0};
    std::atomic<bool> injected{false};
    hyremote::Transport *transportPtr = nullptr;

    auto transport = startTransport(
        port, recorder, makeFrame(128, 128, 0x101010U), {}, {},
        [&](bool) {
            if (phase.load() != 1 || injected.exchange(true))
                return;
            auto b = makeFrame(128, 128, 0x101010U, {{{68, 68, 8, 8}, 0x0000ffU}});
            hyremote::Damage damage;
            damage.kind = hyremote::DamageKind::Regions;
            damage.regions.push_back({68, 68, 8, 8});
            b.damage = damage;
            if (transportPtr)
                transportPtr->enqueueFrame(std::move(b));
        });
    CHECK(transport != nullptr);
    if (!transport)
        return;
    transportPtr = transport.get();

    QTcpSocket socket;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    CHECK(connectRawRfb(socket, port, false, &width, &height));
    CHECK(width == 128 && height == 128);
    // Negotiate Fence as an ordered request-PARSE barrier (not a TCP drain).
    // This ensures the worker has armed the incremental request before frame A
    // is queued, independently of readyRead/frameAvailable event ordering.
    CHECK(sendSetEncodings(socket, {-312, 0}));
    CHECK(readFence(socket) == makeFence(0U));

    TestFramebuffer framebuffer;
    framebuffer.resize(width, height);
    std::vector<UpdateRect> rectangles;
    CHECK(sendUpdateRequest(socket, false, 0, 0, width, height));
    CHECK(readFramebufferUpdate(socket, framebuffer, rectangles));
    CHECK(framebuffer.get(4, 4) == 0x101010U);

    phase.store(1);
    CHECK(sendUpdateRequest(socket, true, 0, 0, width, height));
    const QByteArray requestBarrier("armed", 5);
    CHECK(writeAll(socket, makeFence(0x80000000U, requestBarrier)));
    CHECK(readFence(socket) == makeFence(0U, requestBarrier));
    auto a = makeFrame(128, 128, 0x101010U, {{{4, 4, 8, 8}, 0xff0000U}});
    hyremote::Damage aDamage;
    aDamage.kind = hyremote::DamageKind::Regions;
    aDamage.regions.push_back({4, 4, 8, 8});
    a.damage = aDamage;
    transport->enqueueFrame(std::move(a));

    // First update must carry A (red), not the newer B pixels.
    CHECK(readFramebufferUpdate(socket, framebuffer, rectangles));
    CHECK(framebuffer.get(4, 4) == 0xff0000U);
    CHECK(framebuffer.get(68, 68) == 0x101010U);

    // B is delivered by the following update; its blue region must arrive.
    CHECK(sendUpdateRequest(socket, true, 0, 0, width, height));
    CHECK(readFramebufferUpdate(socket, framebuffer, rectangles));
    CHECK(framebuffer.get(68, 68) == 0x0000ffU);

    socket.disconnectFromHost();
    transport->stop();
}

// Developer-only comparison of the existing exact-tile scan and the optional
// static-hinted fast equality. The *same* executable, CPU and two independent
// frame allocations are used for both. No fixed runner CPU/ratio is asserted.
void runRfbStaticCompareProfile(const QString &outputPath)
{
    const auto before = makeFrame(1920U, 1080U, 0x203040U);
    const auto identical = makeFrame(1920U, 1080U, 0x203040U);
    const auto changed = makeFrame(
        1920U, 1080U, 0x203040U,
        {{hyremote::Rect{896, 512, 64, 64}, 0xd02030U}});
    CHECK(before.storage && identical.storage && changed.storage);
    if (!before.storage || !identical.storage || !changed.storage)
        return;
    CHECK(before.storage != identical.storage);

    constexpr int repetitions = 120;
    const auto sample = [&](const hyremote::RemoteFrame &after, bool preferStatic) {
        QElapsedTimer elapsed;
        volatile std::size_t total = 0;
        elapsed.start();
        for (int i = 0; i < repetitions; ++i) {
            const auto diff = HyRemote::detail::changedRfbTiles(
                &before, after, preferStatic);
            total = total + diff.size();
        }
        const double nsPerCall =
            static_cast<double>(elapsed.nsecsElapsed()) / repetitions;
        return std::make_pair(nsPerCall, static_cast<std::size_t>(total));
    };

    // Alternating order limits warm-cache and runner placement bias; only
    // two figures at the same synthetic size are advisory, not RK3588 data.
    const auto staticOldA = sample(identical, false);
    const auto staticFast = sample(identical, true);
    const auto staticOldB = sample(identical, false);
    const auto sparseOld = sample(changed, false);
    const auto sparseFirstAfterStatic = sample(changed, true);
    CHECK(staticOldA.second == 0U && staticOldB.second == 0U);
    CHECK(staticFast.second == 0U);
    CHECK(sparseOld.second == sparseFirstAfterStatic.second);
    CHECK(sparseOld.second == repetitions);

    const double oldMean = (staticOldA.first + staticOldB.first) / 2.0;
    const QString report = QStringLiteral(
        "PROFILE_KIND=RFB_STATIC_COMPARE_HINT\n"
        "PROFILE_CONTEXT=HOSTED_SYNTHETIC_NOT_DEVICE_PROOF\n"
        "FRAMEBUFFER=1920x1080\n"
        "REPETITIONS=%1\n"
        "STATIC_OLD_A_NS_PER_FRAME=%2\n"
        "STATIC_FAST_NS_PER_FRAME=%3\n"
        "STATIC_OLD_B_NS_PER_FRAME=%4\n"
        "STATIC_FAST_OVER_OLD_RATIO=%5\n"
        "SPARSE_DYNAMIC_OLD_NS_PER_FRAME=%6\n"
        "SPARSE_DYNAMIC_FIRST_AFTER_STATIC_NS_PER_FRAME=%7\n"
        "SPARSE_DYNAMIC_FIRST_HINT_OVER_OLD_RATIO=%8\n"
        "PROFILE_QUALIFICATION=ADVISORY_ONLY\n")
        .arg(repetitions)
        .arg(staticOldA.first, 0, 'f', 0)
        .arg(staticFast.first, 0, 'f', 0)
        .arg(staticOldB.first, 0, 'f', 0)
        .arg(oldMean > 0.0 ? staticFast.first / oldMean : -1.0, 0, 'f', 4)
        .arg(sparseOld.first, 0, 'f', 0)
        .arg(sparseFirstAfterStatic.first, 0, 'f', 0)
        .arg(sparseOld.first > 0.0
             ? sparseFirstAfterStatic.first / sparseOld.first : -1.0, 0, 'f', 4);
    std::cout << report.toStdString();
    QFile file(outputPath);
    CHECK(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    if (file.isOpen()) {
        const QByteArray bytes = report.toUtf8();
        CHECK(file.write(bytes) == bytes.size());
    }
}

// Exact pre-optimization raw-tile encoding for the synthetic workload in
// which every tile has 17+ colours. This reference never ships in Runtime.
QByteArray legacyHighEntropyTrleRawTiles(const hyremote::RemoteFrame &frame)
{
    QByteArray out;
    const auto plane = frame.storage->mapRead(0);
    if (!plane || !plane->data)
        return {};
    const auto *base = reinterpret_cast<const std::uint8_t *>(plane->data);
    const auto pixelAt = [&](std::uint32_t x, std::uint32_t y) {
        const auto *p = base + static_cast<std::size_t>(y) * plane->stride + x * 4U;
        return HyRemote::detail::rfbRgb(p[0], p[1], p[2]);
    };
    const auto width = frame.geometry.size.width;
    const auto height = frame.geometry.size.height;
    out.reserve(static_cast<qsizetype>(static_cast<std::uint64_t>(width) * height * 3U));
    for (std::uint32_t ty = 0; ty < height; ty += 16U) {
        const auto th = std::min(16U, height - ty);
        for (std::uint32_t tx = 0; tx < width; tx += 16U) {
            const auto tw = std::min(16U, width - tx);
            std::array<std::uint32_t, 16> palette{};
            std::array<std::uint8_t, 256> indices{};
            std::size_t count = 0;
            bool overflow = false;
            for (std::uint32_t row = 0; row < th && !overflow; ++row) {
                for (std::uint32_t col = 0; col < tw; ++col) {
                    const auto p = pixelAt(tx + col, ty + row);
                    std::size_t i = 0;
                    while (i < count && palette[i] != p)
                        ++i;
                    if (i == count) {
                        if (count == palette.size()) {
                            overflow = true;
                            break;
                        }
                        palette[count++] = p;
                    }
                    indices[static_cast<std::size_t>(row) * tw + col]
                        = static_cast<std::uint8_t>(i);
                }
            }
            if (!overflow) // explicit scope: all tiles must be raw in this test
                return {};
            out.append(char(0));
            for (std::uint32_t row = 0; row < th; ++row)
                for (std::uint32_t col = 0; col < tw; ++col)
                    HyRemote::detail::appendNativeCpixel(
                        out, pixelAt(tx + col, ty + row));
        }
    }
    return out;
}

// Same executable / same 1080p source bytes / same old-style palette scan:
// measure the exact raw-packing replacement without any OS/GPU assumptions.
void runHighEntropyTrleRawPackingProfile(const QString &filePath)
{
    constexpr std::uint32_t width = 1920U, height = 1080U;
    auto frame = makeFrame(width, height, 0U);
    auto storage = hyremote::CpuFrameStorage::createSinglePlane(width * 4U, height);
    CHECK(storage && storage->mutablePlane(0));
    if (!storage || !storage->mutablePlane(0))
        return;
    auto *p = reinterpret_cast<std::uint8_t *>(storage->mutablePlane(0));
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const auto off = (static_cast<std::size_t>(y) * width + x) * 4U;
            p[off + 0U] = static_cast<std::uint8_t>((x * 13U + y * 7U) & 255U);
            p[off + 1U] = static_cast<std::uint8_t>((x * 29U + y * 11U) & 255U);
            p[off + 2U] = static_cast<std::uint8_t>((x * 37U + y * 19U) & 255U);
            p[off + 3U] = 255U;
        }
    }
    frame.storage = std::move(storage);
    frame.geometry.alphaMode = hyremote::AlphaMode::Opaque;
    const QByteArray reference = legacyHighEntropyTrleRawTiles(frame);
    QByteArray optimized;
    CHECK(HyRemote::detail::appendNativeTrlePayload(optimized, frame, 0, 0, width, height));
    CHECK(!reference.isEmpty() && reference == optimized);
    if (reference.isEmpty() || reference != optimized)
        return;

    constexpr int iterations = 10;
    const auto measure = [&](bool fast) {
        QElapsedTimer clock;
        std::uint64_t bytes = 0;
        clock.start();
        for (int i = 0; i < iterations; ++i) {
            QByteArray encoded;
            if (fast)
                CHECK(HyRemote::detail::appendNativeTrlePayload(
                    encoded, frame, 0, 0, width, height));
            else
                encoded = legacyHighEntropyTrleRawTiles(frame);
            CHECK(encoded.size() == reference.size());
            bytes += static_cast<std::uint64_t>(encoded.size());
        }
        CHECK(bytes == static_cast<std::uint64_t>(reference.size()) * iterations);
        return static_cast<double>(clock.nsecsElapsed()) / iterations;
    };
    (void)measure(false);
    (void)measure(true);
    const double oldA = measure(false);
    const double fast = measure(true);
    const double oldB = measure(false);
    const double baseline = (oldA + oldB) / 2.0;
    const QString report = QStringLiteral(
        "PROFILE_KIND=TRLE_HIGH_ENTROPY_RAW_TILES\n"
        "PROFILE_CONTEXT=HOSTED_SYNTHETIC_NOT_DEVICE_PROOF\n"
        "FRAMEBUFFER=1920x1080\n"
        "WIRE_BYTES_PER_FRAME=%1\n"
        "ITERATIONS_PER_CASE=%2\n"
        "OLD_A_NS_PER_FRAME=%3\n"
        "NEW_NS_PER_FRAME=%4\n"
        "OLD_B_NS_PER_FRAME=%5\n"
        "NEW_OVER_OLD_RATIO=%6\n"
        "WIRE_EQUALITY=PASS\n"
        "PROFILE_QUALIFICATION=ADVISORY_ONLY\n")
        .arg(reference.size()).arg(iterations)
        .arg(oldA, 0, 'f', 0).arg(fast, 0, 'f', 0)
        .arg(oldB, 0, 'f', 0)
        .arg(baseline > 0.0 ? fast / baseline : -1.0, 0, 'f', 4);
    std::cout << report.toStdString();
    QFile output(filePath);
    CHECK(output.open(QIODevice::WriteOnly | QIODevice::Truncate));
    if (output.isOpen()) {
        const QByteArray bytes = report.toUtf8();
        CHECK(output.write(bytes) == bytes.size());
    }
}

#ifdef HYREMOTE_TEST_QUICK_RFB
void testQuickRfbPageConsistency(std::int32_t encoding, bool staged)
{
    constexpr std::uint16_t width = 640, height = 360;
    QQuickWindow window;
    window.resize(width, height);
    StagedQuickProbeItem item(window.contentItem());
    item.setWidth(width);
    item.setHeight(height);
    std::atomic<int> rendered{-1};
    QObject::connect(&window, &QQuickWindow::afterRendering, &window, [&] {
        rendered.store(item.paintedStage());
    }, Qt::DirectConnection);
    window.show();

    const auto waitFor = [](const auto &predicate) {
        QElapsedTimer timer;
        timer.start();
        while (!predicate() && timer.elapsed() < 4000) {
            QCoreApplication::processEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return predicate();
    };
    const auto matches = [](const auto &pixel, int stage) {
        for (std::uint16_t y = 0; y < height; ++y)
            for (std::uint16_t x = 0; x < width; ++x) {
                const bool newPage = stage == 2 || (stage == 1 && x < width / 2);
                if (pixel(x, y) != (newPage ? StagedQuickProbeItem::B : StagedQuickProbeItem::A))
                    return false;
            }
        return true;
    };
    auto target = HyRemote::detail::createTargetComponents(&window, false);
    const quint16 port = freePort();
    if (!target.capture || !port) {
        CHECK(false);
        return;
    }
    HyRemote::detail::RfbSecurityConfig security;
    security.initialFramebufferWidth = width;
    security.initialFramebufferHeight = height;
    std::atomic<unsigned> observedStages{0};
    std::atomic<bool> invalidSource{false};
    // Pass through unchanged. Observe the actual frame delivered by Core.
    security.frameDecorator = [&](const hyremote::RemoteFrame &frame,
                                  hyremote::RemoteFrame &output, QString &) {
        const auto plane = frame.storage ? frame.storage->mapRead(0) : std::nullopt;
        const int lastRendered = rendered.load();
        if (frame.requestId && *frame.requestId && frame.id && plane && plane->data
            && frame.geometry.size.width == width && frame.geometry.size.height == height
            && frame.geometry.pixelFormat == hyremote::PixelFormat::Rgba8888
            && plane->stride >= width * 4U) {
            const auto pixel = [&](unsigned x, unsigned y) {
                const auto *p = reinterpret_cast<const unsigned char *>(plane->data)
                    + y * plane->stride + x * 4U;
                return (unsigned(p[0]) << 16) | (unsigned(p[1]) << 8) | unsigned(p[2]);
            };
            for (int stage = 0; stage <= lastRendered; ++stage) {
                if ((stage != 1 || staged) && matches(pixel, stage)) {
                    const auto bit = 1U << stage;
                    if (!(observedStages.fetch_or(bit) & bit))
                        std::cout << "QUICK_RFB_SOURCE stage=" << stage << " request=" << *frame.requestId
                                  << " frame=" << frame.id << '\n';
                    output = frame;
                    return true;
                }
            }
        }
        invalidSource.store(true);
        output = frame;
        return true;
    };
    hyremote::SessionConfig config;
    config.capture.targetFramesPerSecond = 30;
    hyremote::Session session(config);
    if (!session.setCaptureSource(std::move(target.capture))
        || !session.setTransport(HyRemote::detail::createRfbTransport(
            QHostAddress::LocalHost, port, security))
        || !session.start()) {
        CHECK(false);
        return;
    }

    QTcpSocket socket;
    std::uint16_t actualWidth = 0, actualHeight = 0;
    if (!connectRawRfb(socket, port, false, &actualWidth, &actualHeight)
        || actualWidth != width || actualHeight != height
        || !sendSetEncodings(socket, {encoding})) {
        CHECK(false);
        return;
    }
    TestFramebuffer framebuffer;
    framebuffer.resize(width, height);
    std::vector<UpdateRect> rectangles;
    const auto decode = [&] {
        return waitFor([&] { return socket.bytesAvailable() > 0; })
            && readFramebufferUpdate(socket, framebuffer, rectangles)
            && !rectangles.empty()
            && std::all_of(rectangles.begin(), rectangles.end(),
                           [=](const UpdateRect &rect) { return rect.encoding == encoding; });
    };
    const auto decodedStage = [&] {
        for (int stage = 0; stage <= 2; ++stage) {
            if ((observedStages.load() & (1U << stage))
                && matches([&](unsigned x, unsigned y) { return framebuffer.get(x, y); }, stage))
                return stage;
        }
        return -1;
    };
    if (!waitFor([&] { return observedStages.load() & 1U; })
        || !sendUpdateRequest(socket, false, 0, 0, width, height)
        || !decode() || decodedStage() != 0
        || !sendUpdateRequest(socket, true, 0, 0, width, height)) {
        CHECK(false);
        return;
    }
    for (int stage = staged ? 1 : 2; stage <= 2; ++stage) {
        item.setStage(stage);
        if (!waitFor([&] { return rendered.load() == stage; })) {
            CHECK(false);
            return;
        }
    }
    if (!waitFor([&] { return observedStages.load() & (1U << 2); }) || !decode()) {
        CHECK(false);
        return;
    }
    const int incrementalStage = decodedStage();
    CHECK(incrementalStage >= 0);
    if (incrementalStage >= 0 && incrementalStage < 2)
        CHECK(sendUpdateRequest(socket, true, 0, 0, width, height) && decode() && decodedStage() == 2);
    CHECK(sendUpdateRequest(socket, false, 0, 0, width, height) && decode() && decodedStage() == 2);
    CHECK(!invalidSource.load());
    std::cout << "QUICK_RFB_PAGE encoding=" << encoding << " staged=" << staged
              << " incremental_stage=" << incrementalStage
              << " source_mask=" << observedStages.load() << '\n';
}
#endif

int main(int argc, char **argv)
{
#ifdef HYREMOTE_TEST_QUICK_RFB
    if (argc == 2 && std::string(argv[1]) == "--quick-rfb-page-consistency") {
        QGuiApplication app(argc, argv);
        for (const auto encoding : {0, 15}) {
            testQuickRfbPageConsistency(encoding, false);
            testQuickRfbPageConsistency(encoding, true);
        }
        return failures == 0 ? 0 : 1;
    }
#endif
    QCoreApplication app(argc, argv);

    const QByteArray trleRawPath = qgetenv("HYREMOTE_RFB_TRLE_RAW_PROFILE_FILE");
    if (!trleRawPath.isEmpty()) {
        runHighEntropyTrleRawPackingProfile(QString::fromLocal8Bit(trleRawPath));
        return failures == 0 ? 0 : 1;
    }

    const QByteArray staticComparePath = qgetenv("HYREMOTE_RFB_COMPARE_PROFILE_FILE");
    if (!staticComparePath.isEmpty()) {
        runRfbStaticCompareProfile(QString::fromLocal8Bit(staticComparePath));
        return failures == 0 ? 0 : 1;
    }

    const QByteArray largeProfilePath = qgetenv("HYREMOTE_RFB_LARGE_PROFILE_FILE");
    if (!largeProfilePath.isEmpty()) {
        runLargePageTransitionProfile(QString::fromLocal8Bit(largeProfilePath));
        return failures == 0 ? 0 : 1;
    }

    const QByteArray profilePath = qgetenv("HYREMOTE_RFB_PROFILE_FILE");
    if (!profilePath.isEmpty()) {
        runDeliveryProfile(QString::fromLocal8Bit(profilePath));
        return failures == 0 ? 0 : 1;
    }

    testTrleRawAndIncrementalDelivery();
    testRfbConsumerPressureAndFullRequestWake();
    testDamageAndEncodedFrameShareSnapshot();
    testPointerViewportTracksAdvertisedFramebuffer();
    testDecoratedRegionsCannotHideWatermarkChanges();
    testWatermarkReachesEveryEncoding();
    testStandaloneMarkedBootstrapSurvivesReconnects();
    testProducerBackedIdleRawFrameRelease();
    testProducerBackedSkipsUnusedZeroViewerWatermark();
    testProducerBackedReconnectWatermarkFailsClosed();
    testWatermarkSurvivesFramebufferResize();
    testWatermarkFollowsDevicePixelRatio();
    testNoMarkWithoutDecorator();
    testOpaqueAlphaPolicy();
    testPresentedRgbDamageVsSourceAlpha();
    testStaticCompareFastHintPreservesDamage();
    testAlphaOnlyFrameIsNotPublishedToRfb();
    testNativeRawFastPathAlphaEquivalence();
    testRawFallbackBulkPackingPreservesWireBytes();
    testHighEntropyTrleRawTileBytesAndAlpha();
    testTrlePreferredOverRawOrder();
    testCursorPseudoEncoding();
    testCursorSentAfterPixelFormatWithoutRequest();
    testVariableCursorSnapshot();
    testPartialIncrementalAndResize();
    testContinuousUpdatesAndRequestFallback();
    testPerViewerPendingDamageRemainsBounded();
    testFullyDeliveredSparseDamageHasNoPhantomPendingUpdates();
    testDeliveryCoverageIsConservativeForPartialAndOverlappingCuts();
    testSparseDeliveredCutsDoNotResurrectPendingDamage();
    testPendingDamageCoalescesAtDelivery();
    testLargeDamageCoalescesForPaint();
    testExactTileUnionPreventsSparseFullPageInflation();
    testDenseChangedTilesAvoidSparseBoundingAndPaintAsOne();
    testChangedTileAreaThresholdAndClippedEdges();
    testDisjointSparseOverflowDoesNotInflateFullPage();
    testFullPageTransitionSingleWireRectangle();
    testStaticViewerCaptureDemandFeedback();
    testRfbFeedbackUsesActualServedPixels();
    testRequestPausedViewerDoesNotStallContinuousPeer();
    testContinuousUpdatesForcedRefreshBounds();
    testForcedRefreshHonorsExplicitRegion();
    testFragmentedHandshakeAndInput();
    testNegotiatedFenceAndRequestFallback();
    testOversizedSetEncodingsFailsClosed();
    testOversizedCutTextFailsClosed();
    testUnsupportedMessageFailsClosedAndNextClientRecovers();
    testBoundedInputBurstFailsClosed();
    testExactRfb38VersionIsRequired();
    testSlowViewerClientInitStillServed();
    testStalledPartialMessagesReleaseTheirSlots();
    testPostInitUsefulnessDeadlineKeepsStaticViewer();
    testStalledClientReleasesHeldKeyOnce();
    testStopWithArmedOccupancySweepIsQuiescent();
    testInitialTargetGeometrySendsServerInitBeforeFrame();
    runInteractionLatencyProbe({15, 0}, 15, "trle");
    runInteractionLatencyProbe({0}, 0, "raw");
    runInteractionLatencyProbe({15, 0}, 15, "trle-continuous", true);
    runInteractionLatencyProbe({0}, 0, "raw-continuous", true);

    if (failures != 0)
        std::cerr << failures << " RFB wire-robustness checks failed\n";
    return failures == 0 ? 0 : 1;
}
