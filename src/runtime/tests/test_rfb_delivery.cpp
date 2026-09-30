#include <QByteArray>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "hyremote/core/frame.hpp"
#include "hyremote/core/storage.hpp"
#include "hyremote/core/transport.hpp"
#include "transport/rfb_delivery.hpp"
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

constexpr std::int32_t kEncodingRaw = 0;
constexpr std::int32_t kEncodingTrle = 15;
constexpr std::int32_t kEncodingDesktopSize = -223;

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
    const auto byte = [&data](qsizetype index) {
        return static_cast<std::uint16_t>(static_cast<unsigned char>(data.at(index)));
    };
    return static_cast<std::uint16_t>((byte(offset) << 8U) | byte(offset + 1));
}

std::uint32_t readU32(const QByteArray &data, qsizetype offset)
{
    const auto byte = [&data](qsizetype index) {
        return static_cast<std::uint32_t>(static_cast<unsigned char>(data.at(index)));
    };
    return (byte(offset) << 24U) | (byte(offset + 1) << 16U) | (byte(offset + 2) << 8U)
           | byte(offset + 3);
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

quint16 freePort()
{
    QTcpServer probe;
    if (!probe.listen(QHostAddress::LocalHost, 0))
        return 0;
    return probe.serverPort();
}

struct PixelSpec
{
    std::uint8_t bitsPerPixel = 32;
    std::uint8_t depth = 24;
    bool bigEndian = false;
    bool trueColor = true;
    std::uint16_t redMax = 255;
    std::uint16_t greenMax = 255;
    std::uint16_t blueMax = 255;
    std::uint8_t redShift = 0;
    std::uint8_t greenShift = 8;
    std::uint8_t blueShift = 16;
};

PixelSpec parsePixelSpec(const QByteArray &data, qsizetype offset)
{
    PixelSpec spec;
    spec.bitsPerPixel = static_cast<std::uint8_t>(data.at(offset));
    spec.depth = static_cast<std::uint8_t>(data.at(offset + 1));
    spec.bigEndian = data.at(offset + 2) != 0;
    spec.trueColor = data.at(offset + 3) != 0;
    spec.redMax = readU16(data, offset + 4);
    spec.greenMax = readU16(data, offset + 6);
    spec.blueMax = readU16(data, offset + 8);
    spec.redShift = static_cast<std::uint8_t>(data.at(offset + 10));
    spec.greenShift = static_cast<std::uint8_t>(data.at(offset + 11));
    spec.blueShift = static_cast<std::uint8_t>(data.at(offset + 12));
    return spec;
}

QByteArray setPixelFormat(const PixelSpec &spec)
{
    QByteArray message;
    message.append(char(0));
    message.append("\0\0\0", 3);
    message.append(static_cast<char>(spec.bitsPerPixel));
    message.append(static_cast<char>(spec.depth));
    message.append(static_cast<char>(spec.bigEndian ? 1 : 0));
    message.append(static_cast<char>(spec.trueColor ? 1 : 0));
    appendU16(message, spec.redMax);
    appendU16(message, spec.greenMax);
    appendU16(message, spec.blueMax);
    message.append(static_cast<char>(spec.redShift));
    message.append(static_cast<char>(spec.greenShift));
    message.append(static_cast<char>(spec.blueShift));
    message.append("\0\0\0", 3);
    return message;
}

QByteArray setEncodings(const std::vector<std::int32_t> &encodings)
{
    QByteArray message;
    message.append(char(2));
    message.append(char(0));
    appendU16(message, static_cast<std::uint16_t>(encodings.size()));
    for (const auto encoding : encodings)
        appendS32(message, encoding);
    return message;
}

QByteArray updateRequest(bool incremental, int x, int y, int width, int height)
{
    QByteArray message;
    message.append(char(3));
    message.append(static_cast<char>(incremental ? 1 : 0));
    appendU16(message, static_cast<std::uint16_t>(x));
    appendU16(message, static_cast<std::uint16_t>(y));
    appendU16(message, static_cast<std::uint16_t>(width));
    appendU16(message, static_cast<std::uint16_t>(height));
    return message;
}

struct Image
{
    int width = 0;
    int height = 0;
    std::vector<std::uint32_t> pixels;
};

Image solidImage(int width, int height, std::uint32_t rgb)
{
    return {width, height, std::vector<std::uint32_t>(static_cast<std::size_t>(width * height), rgb)};
}

void fillRect(Image &image, int x, int y, int width, int height, std::uint32_t rgb)
{
    const int x0 = std::max(0, x);
    const int y0 = std::max(0, y);
    const int x1 = std::min(image.width, x + width);
    const int y1 = std::min(image.height, y + height);
    for (int row = y0; row < y1; ++row) {
        std::fill(image.pixels.begin() + static_cast<std::ptrdiff_t>(row * image.width + x0),
                  image.pixels.begin() + static_cast<std::ptrdiff_t>(row * image.width + x1),
                  rgb);
    }
}

hyremote::RemoteFrame toFrame(const Image &image)
{
    auto storage = hyremote::CpuFrameStorage::createSinglePlane(
        static_cast<std::size_t>(image.width) * 4U, static_cast<std::size_t>(image.height));
    if (!storage)
        return {};

    auto *bytes = reinterpret_cast<std::uint8_t *>(storage->mutablePlane(0));
    if (!bytes)
        return {};
    for (std::size_t i = 0; i < image.pixels.size(); ++i) {
        const std::uint32_t rgb = image.pixels[i];
        bytes[i * 4U] = static_cast<std::uint8_t>((rgb >> 16U) & 0xffU);
        bytes[i * 4U + 1U] = static_cast<std::uint8_t>((rgb >> 8U) & 0xffU);
        bytes[i * 4U + 2U] = static_cast<std::uint8_t>(rgb & 0xffU);
        bytes[i * 4U + 3U] = 0xffU;
    }

    hyremote::RemoteFrame frame;
    frame.geometry.size = {static_cast<std::uint32_t>(image.width),
                           static_cast<std::uint32_t>(image.height)};
    frame.geometry.pixelFormat = hyremote::PixelFormat::Rgba8888;
    frame.geometry.alphaMode = hyremote::AlphaMode::Opaque;
    frame.geometry.planeCount = 1;
    frame.storage = std::move(storage);
    frame.timing.pts = hyremote::Clock::now();
    frame.timing.ptsSource = hyremote::PtsSource::Completion;
    frame.damage = hyremote::Damage::fullFrame();
    return frame;
}

struct Viewer
{
    QTcpSocket socket;
    PixelSpec pixels;
    Image framebuffer;
};

bool connectViewer(Viewer &viewer, quint16 port)
{
    viewer.socket.connectToHost(QHostAddress::LocalHost, port);
    if (!viewer.socket.waitForConnected(3000))
        return false;
    if (readExact(viewer.socket, 12) != QByteArray("RFB 003.008\n", 12))
        return false;
    if (!writeAll(viewer.socket, QByteArray("RFB 003.008\n", 12)))
        return false;

    const QByteArray countBytes = readExact(viewer.socket, 1);
    if (countBytes.size() != 1)
        return false;
    const int count = static_cast<unsigned char>(countBytes.at(0));
    const QByteArray security = readExact(viewer.socket, count);
    if (security.size() != count || !security.contains(char(1)))
        return false;
    if (!writeAll(viewer.socket, QByteArray(1, char(1))))
        return false;
    const QByteArray securityResult = readExact(viewer.socket, 4);
    if (securityResult.size() != 4 || readU32(securityResult, 0) != 0U)
        return false;

    if (!writeAll(viewer.socket, QByteArray(1, char(1))))
        return false;
    const QByteArray init = readExact(viewer.socket, 24);
    if (init.size() != 24)
        return false;
    viewer.framebuffer.width = readU16(init, 0);
    viewer.framebuffer.height = readU16(init, 2);
    viewer.framebuffer.pixels.assign(
        static_cast<std::size_t>(viewer.framebuffer.width * viewer.framebuffer.height), 0U);
    viewer.pixels = parsePixelSpec(init, 4);
    const std::uint32_t nameLength = readU32(init, 20);
    return readExact(viewer.socket, static_cast<qsizetype>(nameLength)).size()
           == static_cast<qsizetype>(nameLength);
}

std::uint32_t pixelValue(const QByteArray &bytes, const PixelSpec &spec)
{
    std::uint32_t value = 0;
    const int count = spec.bitsPerPixel / 8;
    if (spec.bigEndian) {
        for (int i = 0; i < count; ++i)
            value = (value << 8U) | static_cast<unsigned char>(bytes.at(i));
    } else {
        for (int i = count - 1; i >= 0; --i)
            value = (value << 8U) | static_cast<unsigned char>(bytes.at(i));
    }
    return value;
}

std::uint8_t expandChannel(std::uint32_t value, std::uint16_t max)
{
    return static_cast<std::uint8_t>((value * 255U + max / 2U) / max);
}

std::uint32_t rgbFromValue(std::uint32_t value, const PixelSpec &spec)
{
    const std::uint32_t r = (value >> spec.redShift) & spec.redMax;
    const std::uint32_t g = (value >> spec.greenShift) & spec.greenMax;
    const std::uint32_t b = (value >> spec.blueShift) & spec.blueMax;
    return (static_cast<std::uint32_t>(expandChannel(r, spec.redMax)) << 16U)
           | (static_cast<std::uint32_t>(expandChannel(g, spec.greenMax)) << 8U)
           | static_cast<std::uint32_t>(expandChannel(b, spec.blueMax));
}

int bytesPerCpixel(const PixelSpec &spec)
{
    const int bytesPerPixel = spec.bitsPerPixel / 8;
    if (!spec.trueColor || spec.bitsPerPixel != 32 || spec.depth > 24)
        return bytesPerPixel;
    const std::uint32_t usedMask = (static_cast<std::uint32_t>(spec.redMax) << spec.redShift)
                                   | (static_cast<std::uint32_t>(spec.greenMax) << spec.greenShift)
                                   | (static_cast<std::uint32_t>(spec.blueMax) << spec.blueShift);
    return ((usedMask & 0xff000000U) == 0U || (usedMask & 0x000000ffU) == 0U) ? 3
                                                                                : bytesPerPixel;
}

std::uint32_t decodeCpixel(const QByteArray &cpixel, const PixelSpec &spec)
{
    const int fullBytes = spec.bitsPerPixel / 8;
    if (cpixel.size() == fullBytes)
        return rgbFromValue(pixelValue(cpixel, spec), spec);

    CHECK(fullBytes == 4 && cpixel.size() == 3);
    if (fullBytes != 4 || cpixel.size() != 3)
        return 0;

    const std::uint32_t usedMask = (static_cast<std::uint32_t>(spec.redMax) << spec.redShift)
                                   | (static_cast<std::uint32_t>(spec.greenMax) << spec.greenShift)
                                   | (static_cast<std::uint32_t>(spec.blueMax) << spec.blueShift);
    const bool fitsLowThree = (usedMask & 0xff000000U) == 0U;
    QByteArray full(4, char(0));
    const int first = fitsLowThree ? (spec.bigEndian ? 1 : 0) : (spec.bigEndian ? 0 : 1);
    for (int i = 0; i < 3; ++i)
        full[first + i] = cpixel.at(i);
    return rgbFromValue(pixelValue(full, spec), spec);
}

bool applyRaw(Viewer &viewer, int x, int y, int width, int height)
{
    const int bpp = viewer.pixels.bitsPerPixel / 8;
    const QByteArray payload
        = readExact(viewer.socket, static_cast<qsizetype>(width) * height * bpp);
    if (payload.size() != static_cast<qsizetype>(width) * height * bpp)
        return false;
    qsizetype offset = 0;
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            const QByteArray pixel = payload.mid(offset, bpp);
            offset += bpp;
            viewer.framebuffer.pixels[static_cast<std::size_t>((y + row) * viewer.framebuffer.width
                                                               + x + col)]
                = rgbFromValue(pixelValue(pixel, viewer.pixels), viewer.pixels);
        }
    }
    return true;
}

bool applyTrle(Viewer &viewer, int x, int y, int width, int height)
{
    const int cpixelBytes = bytesPerCpixel(viewer.pixels);
    for (int tileY = y; tileY < y + height; tileY += 16) {
        const int tileHeight = std::min(16, y + height - tileY);
        for (int tileX = x; tileX < x + width; tileX += 16) {
            const int tileWidth = std::min(16, x + width - tileX);
            const QByteArray sub = readExact(viewer.socket, 1);
            if (sub.size() != 1)
                return false;
            const int subencoding = static_cast<unsigned char>(sub.at(0));

            if (subencoding == 0) {
                const QByteArray pixels = readExact(
                    viewer.socket, static_cast<qsizetype>(tileWidth * tileHeight * cpixelBytes));
                if (pixels.size() != tileWidth * tileHeight * cpixelBytes)
                    return false;
                qsizetype offset = 0;
                for (int row = 0; row < tileHeight; ++row) {
                    for (int col = 0; col < tileWidth; ++col) {
                        const std::uint32_t rgb
                            = decodeCpixel(pixels.mid(offset, cpixelBytes), viewer.pixels);
                        offset += cpixelBytes;
                        viewer.framebuffer.pixels[static_cast<std::size_t>(
                            (tileY + row) * viewer.framebuffer.width + tileX + col)] = rgb;
                    }
                }
                continue;
            }

            if (subencoding == 1) {
                const QByteArray pixel = readExact(viewer.socket, cpixelBytes);
                if (pixel.size() != cpixelBytes)
                    return false;
                const std::uint32_t rgb = decodeCpixel(pixel, viewer.pixels);
                for (int row = 0; row < tileHeight; ++row) {
                    for (int col = 0; col < tileWidth; ++col) {
                        viewer.framebuffer.pixels[static_cast<std::size_t>(
                            (tileY + row) * viewer.framebuffer.width + tileX + col)] = rgb;
                    }
                }
                continue;
            }

            if (subencoding < 2 || subencoding > 16)
                return false;
            const int paletteSize = subencoding;
            const QByteArray paletteBytes = readExact(viewer.socket, paletteSize * cpixelBytes);
            if (paletteBytes.size() != paletteSize * cpixelBytes)
                return false;
            std::vector<std::uint32_t> palette(static_cast<std::size_t>(paletteSize));
            for (int i = 0; i < paletteSize; ++i)
                palette[static_cast<std::size_t>(i)]
                    = decodeCpixel(paletteBytes.mid(i * cpixelBytes, cpixelBytes), viewer.pixels);

            const int bits = paletteSize == 2 ? 1 : (paletteSize <= 4 ? 2 : 4);
            const int bytesPerRow = (tileWidth * bits + 7) / 8;
            const QByteArray packed = readExact(viewer.socket, bytesPerRow * tileHeight);
            if (packed.size() != bytesPerRow * tileHeight)
                return false;
            const std::uint8_t mask = static_cast<std::uint8_t>((1U << bits) - 1U);
            for (int row = 0; row < tileHeight; ++row) {
                int bitOffset = 0;
                for (int col = 0; col < tileWidth; ++col) {
                    const int byteIndex = row * bytesPerRow + bitOffset / 8;
                    const int within = bitOffset % 8;
                    const int shift = 8 - bits - within;
                    const auto index = static_cast<std::uint8_t>(
                        (static_cast<unsigned char>(packed.at(byteIndex)) >> shift) & mask);
                    if (index >= palette.size())
                        return false;
                    viewer.framebuffer.pixels[static_cast<std::size_t>(
                        (tileY + row) * viewer.framebuffer.width + tileX + col)] = palette[index];
                    bitOffset += bits;
                }
            }
        }
    }
    return true;
}

struct UpdateResult
{
    bool ok = false;
    bool sawRaw = false;
    bool sawTrle = false;
    bool sawDesktopSize = false;
    int pixelRectangles = 0;
};

UpdateResult readUpdate(Viewer &viewer)
{
    UpdateResult result;
    const QByteArray header = readExact(viewer.socket, 4);
    if (header.size() != 4 || header.at(0) != 0)
        return result;
    const int count = readU16(header, 2);

    for (int i = 0; i < count; ++i) {
        const QByteArray rectHeader = readExact(viewer.socket, 12);
        if (rectHeader.size() != 12)
            return result;
        const int x = readU16(rectHeader, 0);
        const int y = readU16(rectHeader, 2);
        const int width = readU16(rectHeader, 4);
        const int height = readU16(rectHeader, 6);
        const std::int32_t encoding = readS32(rectHeader, 8);

        if (encoding == kEncodingDesktopSize) {
            result.sawDesktopSize = true;
            viewer.framebuffer.width = width;
            viewer.framebuffer.height = height;
            viewer.framebuffer.pixels.assign(static_cast<std::size_t>(width * height), 0U);
            continue;
        }

        if (x + width > viewer.framebuffer.width || y + height > viewer.framebuffer.height)
            return result;
        ++result.pixelRectangles;
        if (encoding == kEncodingRaw) {
            result.sawRaw = true;
            if (!applyRaw(viewer, x, y, width, height))
                return result;
        } else if (encoding == kEncodingTrle) {
            result.sawTrle = true;
            if (!applyTrle(viewer, x, y, width, height))
                return result;
        } else {
            return result;
        }
    }

    result.ok = true;
    return result;
}

bool noFramebufferPayload(QTcpSocket &socket, int timeoutMs = 150)
{
    if (socket.bytesAvailable() != 0)
        return false;
    return !socket.waitForReadyRead(timeoutMs);
}

void testCommitBoundaryModel()
{
    using HyRemote::detail::rfb_delivery::BoundedRegion;
    using HyRemote::detail::rfb_delivery::Rect;
    using HyRemote::detail::rfb_delivery::ViewerState;

    ViewerState state;
    state.markInitialFull(128, 96);
    state.request(true, Rect{0, 0, 128, 96});
    const auto first = state.selectEligible();
    CHECK(first.has_value() && !first->empty());
    const auto retry = state.selectEligible();
    CHECK(retry.has_value() && retry->rects() == first->rects());
    if (retry)
        state.commitDelivered(*retry);
    state.request(true, Rect{0, 0, 128, 96});
    CHECK(!state.selectEligible().has_value());
    std::cout << "DELIVERY_COMMIT_BOUNDARY=PASS\n";
}

void testProductionDelivery()
{
    const quint16 port = freePort();
    CHECK(port != 0);
    if (port == 0)
        return;

    auto transport = HyRemote::detail::createRfbTransport(QHostAddress::LocalHost, port,
                                                          HyRemote::detail::RfbSecurityConfig{});
    CHECK(transport != nullptr);
    if (!transport)
        return;
    CHECK(transport->start({}, {}));

    Image base = solidImage(128, 96, 0x182028U);
    fillRect(base, 0, 0, 32, 32, 0x31506dU);
    fillRect(base, 64, 32, 32, 32, 0xe0a030U);
    transport->enqueueFrame(toFrame(base));

    Viewer trle;
    CHECK(connectViewer(trle, port));
    PixelSpec conventional = trle.pixels;
    conventional.redShift = 16;
    conventional.greenShift = 8;
    conventional.blueShift = 0;
    CHECK(writeAll(trle.socket, setPixelFormat(conventional)));
    trle.pixels = conventional;
    CHECK(writeAll(trle.socket, setEncodings({kEncodingTrle, kEncodingRaw, kEncodingDesktopSize})));
    CHECK(writeAll(trle.socket, updateRequest(false, 0, 0, base.width, base.height)));
    const UpdateResult initial = readUpdate(trle);
    CHECK(initial.ok && initial.sawTrle && !initial.sawRaw);
    CHECK(trle.framebuffer.pixels == base.pixels);
    std::cout << "TRLE_NEGOTIATION_ROUNDTRIP=PASS\n";

    CHECK(writeAll(trle.socket, updateRequest(true, 0, 0, base.width, base.height)));
    CHECK(noFramebufferPayload(trle.socket));

    Image changed = base;
    fillRect(changed, 4, 4, 20, 20, 0x3b8f60U);
    transport->enqueueFrame(toFrame(changed));
    const UpdateResult wake = readUpdate(trle);
    CHECK(wake.ok && wake.sawTrle && wake.pixelRectangles >= 1);
    CHECK(trle.framebuffer.pixels == changed.pixels);
    std::cout << "STATIC_PENDING_REQUEST_WAKE=PASS\n";

    Image split = changed;
    fillRect(split, 8, 70, 16, 16, 0xb8c4ccU);
    fillRect(split, 88, 70, 16, 16, 0xd05050U);
    transport->enqueueFrame(toFrame(split));
    CHECK(writeAll(trle.socket, updateRequest(true, 0, 64, 64, 32)));
    const UpdateResult left = readUpdate(trle);
    CHECK(left.ok && left.sawTrle);
    CHECK(trle.framebuffer.pixels != split.pixels);

    CHECK(writeAll(trle.socket, updateRequest(true, 0, 64, 64, 32)));
    CHECK(noFramebufferPayload(trle.socket));
    CHECK(writeAll(trle.socket, updateRequest(true, 64, 64, 64, 32)));
    const UpdateResult right = readUpdate(trle);
    CHECK(right.ok && right.sawTrle);
    CHECK(trle.framebuffer.pixels == split.pixels);
    std::cout << "PARTIAL_DAMAGE_PRESERVED=PASS\n";

    Image a = split;
    Image b = a;
    fillRect(b, 4, 40, 12, 12, 0x104080U);
    Image c = b;
    fillRect(c, 68, 4, 12, 12, 0x801040U);
    Image d = c;
    fillRect(d, 100, 44, 12, 12, 0x408010U);
    transport->enqueueFrame(toFrame(b));
    transport->enqueueFrame(toFrame(c));
    transport->enqueueFrame(toFrame(d));
    CHECK(writeAll(trle.socket, updateRequest(true, 0, 0, d.width, d.height)));
    const UpdateResult accumulated = readUpdate(trle);
    CHECK(accumulated.ok && accumulated.sawTrle);
    CHECK(trle.framebuffer.pixels == d.pixels);
    std::cout << "SLOW_VIEWER_ACCUMULATION=PASS\n";

    Image resized = solidImage(160, 112, 0x25313cU);
    fillRect(resized, 40, 30, 64, 48, 0xe0a030U);
    transport->enqueueFrame(toFrame(resized));
    CHECK(writeAll(trle.socket, updateRequest(true, 0, 0, resized.width, resized.height)));
    const UpdateResult resize = readUpdate(trle);
    CHECK(resize.ok && resize.sawDesktopSize && resize.sawTrle);
    CHECK(trle.framebuffer.width == resized.width && trle.framebuffer.height == resized.height);
    CHECK(trle.framebuffer.pixels == resized.pixels);
    std::cout << "RESIZE_FULL_REFRESH=PASS\n";

    Viewer raw;
    CHECK(connectViewer(raw, port));
    CHECK(writeAll(raw.socket, setEncodings({kEncodingRaw, kEncodingDesktopSize})));
    CHECK(writeAll(raw.socket, updateRequest(false, 0, 0, resized.width, resized.height)));
    const UpdateResult fallback = readUpdate(raw);
    CHECK(fallback.ok && fallback.sawRaw && !fallback.sawTrle);
    CHECK(raw.framebuffer.pixels == resized.pixels);
    std::cout << "RAW_FALLBACK=PASS\n";

    trle.socket.disconnectFromHost();
    raw.socket.disconnectFromHost();
    trle.socket.waitForDisconnected(1000);
    raw.socket.waitForDisconnected(1000);
    transport->stop();
}

}  // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    testCommitBoundaryModel();
    testProductionDelivery();

    if (failures != 0)
        std::cerr << failures << " RFB delivery checks failed\n";
    return failures == 0 ? 0 : 1;
}
