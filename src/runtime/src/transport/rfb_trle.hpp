#pragma once

#include <QByteArray>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "hyremote/core/frame.hpp"
#include "rfb_pixel_policy.hpp"

namespace HyRemote::detail {

constexpr std::int32_t kRfbEncodingTrle = 15;
constexpr std::uint32_t kRfbTrleTile = 16;

inline std::uint32_t rfbRgb(std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept
{
    return (static_cast<std::uint32_t>(r) << 16U) | (static_cast<std::uint32_t>(g) << 8U)
           | static_cast<std::uint32_t>(b);
}

inline void appendNativeCpixel(QByteArray &out, std::uint32_t rgb)
{
    // Native HyRemote RFB pixels are 32bpp/depth-24, little-endian, shifts 0/8/16. RFC 6143
    // CPIXEL therefore omits the unused most-significant byte; the wire bytes are R,G,B.
    out.append(static_cast<char>((rgb >> 16U) & 0xffU));
    out.append(static_cast<char>((rgb >> 8U) & 0xffU));
    out.append(static_cast<char>(rgb & 0xffU));
}

inline int rfbPackedPaletteBits(std::size_t paletteSize) noexcept
{
    if (paletteSize == 2U)
        return 1;
    if (paletteSize <= 4U)
        return 2;
    return 4;
}

inline bool appendNativeTrlePayload(QByteArray &out,
                                    const hyremote::RemoteFrame &frame,
                                    std::uint32_t x,
                                    std::uint32_t y,
                                    std::uint32_t width,
                                    std::uint32_t height)
{
    if (frame.geometry.pixelFormat != hyremote::PixelFormat::Rgba8888 || !frame.storage
        || frame.geometry.planeCount != 1 || width == 0 || height == 0
        || x + width > frame.geometry.size.width || y + height > frame.geometry.size.height) {
        return false;
    }

    const auto plane = frame.storage->mapRead(0);
    if (!plane || !plane->data)
        return false;
    const std::size_t frameWidth = frame.geometry.size.width;
    const std::size_t frameHeight = frame.geometry.size.height;
    if (plane->stride < frameWidth * 4U || plane->bytes < plane->stride * frameHeight)
        return false;

    const auto *base = reinterpret_cast<const std::uint8_t *>(plane->data);
    const auto pixelAt = [&](std::uint32_t px, std::uint32_t py) {
        const auto *source = base + static_cast<std::size_t>(py) * plane->stride
                             + static_cast<std::size_t>(px) * 4U;
        return rfbRgb(rfbOpaqueChannel(source[0], source[3], frame.geometry.alphaMode),
                      rfbOpaqueChannel(source[1], source[3], frame.geometry.alphaMode),
                      rfbOpaqueChannel(source[2], source[3], frame.geometry.alphaMode));
    };

    // The old unconditional reserve allocated 1920*1080*3 = 5.93 MiB even
    // when the actual TRLE payload was just ~32 KiB of solid tiles. That
    // inflated temporary encoder allocations/allocator high-water for large
    // flat HMI pages. Defer the raw-sized reservation until the first tile
    // has already been classified by the existing encoder. On an all-raw
    // (high-entropy) page the very first tile still reserves the full buffer,
    // preserving its one-allocation fast path without a second pixel scan.
    // Mixed pages may grow the QByteArray normally; no wire bytes or tile
    // classification thresholds change.
    const std::uint64_t rawCpixelBytes = static_cast<std::uint64_t>(width) * height * 3U;
    const std::uint64_t tileCount =
        ((static_cast<std::uint64_t>(width) + kRfbTrleTile - 1U) / kRfbTrleTile)
        * ((static_cast<std::uint64_t>(height) + kRfbTrleTile - 1U) / kRfbTrleTile);
    bool firstTile = true;

    for (std::uint32_t tileY = y; tileY < y + height; tileY += kRfbTrleTile) {
        const std::uint32_t tileHeight = std::min(kRfbTrleTile, y + height - tileY);
        for (std::uint32_t tileX = x; tileX < x + width; tileX += kRfbTrleTile) {
            const std::uint32_t tileWidth = std::min(kRfbTrleTile, x + width - tileX);

            std::array<std::uint32_t, 16> palette{};
            std::array<std::uint8_t, kRfbTrleTile * kRfbTrleTile> indices{};
            std::size_t paletteSize = 0;
            bool paletteOverflow = false;

            for (std::uint32_t row = 0; row < tileHeight && !paletteOverflow; ++row) {
                for (std::uint32_t col = 0; col < tileWidth; ++col) {
                    const std::uint32_t pixel = pixelAt(tileX + col, tileY + row);
                    std::size_t index = 0;
                    while (index < paletteSize && palette[index] != pixel)
                        ++index;
                    if (index == paletteSize) {
                        if (paletteSize == palette.size()) {
                            paletteOverflow = true;
                            break;
                        }
                        palette[paletteSize++] = pixel;
                    }
                    indices[static_cast<std::size_t>(row) * tileWidth + col]
                        = static_cast<std::uint8_t>(index);
                }
            }

            if (firstTile) {
                firstTile = false;
                if (paletteOverflow) {
                    // Only an actually raw first tile earns a whole-rectangle
                    // reserve. Include each raw tile's mode byte as well as
                    // CPIXELs to avoid a tail reallocation on noisy pages.
                    // Check the combined size before narrowing to qsizetype.
                    const std::uint64_t maxSize = static_cast<std::uint64_t>(
                        std::numeric_limits<qsizetype>::max());
                    const std::uint64_t prefix = static_cast<std::uint64_t>(out.size());
                    if (rawCpixelBytes <= maxSize - prefix
                        && tileCount <= maxSize - prefix - rawCpixelBytes) {
                        out.reserve(out.size()
                                    + static_cast<qsizetype>(rawCpixelBytes + tileCount));
                    }
                }
            }

            if (paletteOverflow) {
                // High-entropy TRLE tile: no palette can encode 17+ colours,
                // so emit the exact raw CPIXEL bytes. The old path performed
                // three QByteArray::append calls per pixel; a 1080p noisy page
                // means millions of tiny appends plus repeated bounds checks.
                // Resize the complete tile once, then write R,G,B directly.
                // Tile size is <=16x16, and the wire remains RFC 6143 mode 0.
                const qsizetype tileBytes =
                    static_cast<qsizetype>(tileWidth * tileHeight * 3U);
                if (out.size() > std::numeric_limits<qsizetype>::max()
                                     - tileBytes - 1)
                    return false;
                out.append(char(0));
                const qsizetype offset = out.size();
                out.resize(offset + tileBytes);
                auto *packed = reinterpret_cast<std::uint8_t *>(out.data() + offset);
                for (std::uint32_t row = 0; row < tileHeight; ++row) {
                    const auto *source = base
                        + static_cast<std::size_t>(tileY + row) * plane->stride
                        + static_cast<std::size_t>(tileX) * 4U;
                    for (std::uint32_t col = 0; col < tileWidth; ++col) {
                        const auto *rgba = source + static_cast<std::size_t>(col) * 4U;
                        const std::size_t i =
                            (static_cast<std::size_t>(row) * tileWidth + col) * 3U;
                        packed[i] = rfbOpaqueChannel(
                            rgba[0], rgba[3], frame.geometry.alphaMode);
                        packed[i + 1U] = rfbOpaqueChannel(
                            rgba[1], rgba[3], frame.geometry.alphaMode);
                        packed[i + 2U] = rfbOpaqueChannel(
                            rgba[2], rgba[3], frame.geometry.alphaMode);
                    }
                }
                continue;
            }

            if (paletteSize == 1U) {
                out.append(char(1));  // Solid tile.
                appendNativeCpixel(out, palette[0]);
                continue;
            }

            out.append(static_cast<char>(paletteSize));  // Packed palette 2..16.
            for (std::size_t index = 0; index < paletteSize; ++index)
                appendNativeCpixel(out, palette[index]);

            const int bits = rfbPackedPaletteBits(paletteSize);
            for (std::uint32_t row = 0; row < tileHeight; ++row) {
                std::uint8_t packed = 0;
                int used = 0;
                for (std::uint32_t col = 0; col < tileWidth; ++col) {
                    packed = static_cast<std::uint8_t>(
                        (packed << bits)
                        | indices[static_cast<std::size_t>(row) * tileWidth + col]);
                    used += bits;
                    if (used == 8) {
                        out.append(static_cast<char>(packed));
                        packed = 0;
                        used = 0;
                    }
                }
                if (used != 0) {
                    packed = static_cast<std::uint8_t>(packed << (8 - used));
                    out.append(static_cast<char>(packed));
                }
            }
        }
    }
    return true;
}

}  // namespace HyRemote::detail
