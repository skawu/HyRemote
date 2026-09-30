#pragma once

#include <QByteArray>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include "hyremote/core/frame.hpp"

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
        return rfbRgb(source[0], source[1], source[2]);
    };

    const std::uint64_t rawCpixelBytes = static_cast<std::uint64_t>(width) * height * 3U;
    if (rawCpixelBytes <= static_cast<std::uint64_t>(std::numeric_limits<qsizetype>::max()))
        out.reserve(out.size() + static_cast<qsizetype>(rawCpixelBytes));

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

            if (paletteOverflow) {
                out.append(char(0));  // TRLE raw CPIXEL tile.
                for (std::uint32_t row = 0; row < tileHeight; ++row) {
                    for (std::uint32_t col = 0; col < tileWidth; ++col)
                        appendNativeCpixel(out, pixelAt(tileX + col, tileY + row));
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
