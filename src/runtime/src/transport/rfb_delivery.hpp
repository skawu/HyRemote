#pragma once

#include <QByteArray>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <vector>

#include "hyremote/core/frame.hpp"
#include "hyremote/core/storage.hpp"

namespace HyRemote::detail::rfb_delivery {

constexpr int kDamageTile = 64;
constexpr int kTrleTile = 16;
constexpr std::size_t kMaxDamageRects = 64;

struct Rect
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool empty() const noexcept { return width <= 0 || height <= 0; }
    int right() const noexcept { return x + width; }
    int bottom() const noexcept { return y + height; }
};

inline bool operator==(const Rect &a, const Rect &b) noexcept
{
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

inline Rect intersection(const Rect &a, const Rect &b) noexcept
{
    const int x0 = std::max(a.x, b.x);
    const int y0 = std::max(a.y, b.y);
    const int x1 = std::min(a.right(), b.right());
    const int y1 = std::min(a.bottom(), b.bottom());
    return {x0, y0, std::max(0, x1 - x0), std::max(0, y1 - y0)};
}

inline Rect bounds(const Rect &a, const Rect &b) noexcept
{
    if (a.empty())
        return b;
    if (b.empty())
        return a;
    const int x0 = std::min(a.x, b.x);
    const int y0 = std::min(a.y, b.y);
    const int x1 = std::max(a.right(), b.right());
    const int y1 = std::max(a.bottom(), b.bottom());
    return {x0, y0, x1 - x0, y1 - y0};
}

inline bool contains(const Rect &outer, const Rect &inner) noexcept
{
    return !inner.empty() && outer.x <= inner.x && outer.y <= inner.y
           && outer.right() >= inner.right() && outer.bottom() >= inner.bottom();
}

inline std::vector<Rect> subtractRect(const Rect &source, const Rect &cut)
{
    const Rect hit = intersection(source, cut);
    if (hit.empty())
        return {source};

    std::vector<Rect> out;
    out.reserve(4);
    for (const Rect &piece : {
             Rect{source.x, source.y, source.width, hit.y - source.y},
             Rect{source.x, hit.bottom(), source.width, source.bottom() - hit.bottom()},
             Rect{source.x, hit.y, hit.x - source.x, hit.height},
             Rect{hit.right(), hit.y, source.right() - hit.right(), hit.height},
         }) {
        if (!piece.empty())
            out.push_back(piece);
    }
    return out;
}

class BoundedRegion
{
public:
    void clear() { m_rects.clear(); }
    bool empty() const noexcept { return m_rects.empty(); }
    std::size_t rectCount() const noexcept { return m_rects.size(); }
    const std::vector<Rect> &rects() const noexcept { return m_rects; }

    void add(Rect rect)
    {
        if (rect.empty())
            return;
        for (auto it = m_rects.begin(); it != m_rects.end();) {
            if (contains(*it, rect))
                return;
            if (contains(rect, *it)) {
                it = m_rects.erase(it);
                continue;
            }
            ++it;
        }
        m_rects.push_back(rect);
        enforceBound();
    }

    void add(const BoundedRegion &other)
    {
        for (const Rect &rect : other.m_rects)
            add(rect);
    }

    BoundedRegion intersected(Rect request) const
    {
        BoundedRegion out;
        for (const Rect &rect : m_rects)
            out.add(intersection(rect, request));
        return out;
    }

    void subtract(const BoundedRegion &delivered)
    {
        for (const Rect &cut : delivered.m_rects)
            subtract(cut);
    }

private:
    void subtract(Rect cut)
    {
        if (cut.empty())
            return;
        std::vector<Rect> next;
        for (const Rect &rect : m_rects) {
            const auto pieces = subtractRect(rect, cut);
            next.insert(next.end(), pieces.begin(), pieces.end());
        }
        m_rects = std::move(next);
        enforceBound();
    }

    void enforceBound()
    {
        if (m_rects.size() <= kMaxDamageRects)
            return;
        Rect all;
        for (const Rect &rect : m_rects)
            all = bounds(all, rect);
        m_rects.clear();
        if (!all.empty())
            m_rects.push_back(all);
    }

    std::vector<Rect> m_rects;
};

class ViewerState
{
public:
    void accumulate(const BoundedRegion &damage) { m_pending.add(damage); }

    void request(bool incremental, Rect requested)
    {
        m_request = Request{incremental, requested};
    }

    bool hasOutstandingRequest() const noexcept { return m_request.has_value(); }

    std::optional<BoundedRegion> selectEligible() const
    {
        if (!m_request)
            return std::nullopt;

        BoundedRegion selected;
        if (m_forcedRefresh) {
            selected.add(*m_forcedRefresh);
            return selected;
        }

        if (m_request->incremental) {
            selected = m_pending.intersected(m_request->requested);
            if (selected.empty())
                return std::nullopt;
        } else {
            selected.add(m_request->requested);
        }
        return selected;
    }

    void commitDelivered(const BoundedRegion &delivered)
    {
        if (!m_request)
            return;
        m_pending.subtract(delivered);
        m_forcedRefresh.reset();
        m_request.reset();
    }

    void markInitialFull(int width, int height)
    {
        m_pending.clear();
        m_pending.add({0, 0, width, height});
        m_forcedRefresh.reset();
    }

    void invalidateFull(int width, int height)
    {
        const Rect full{0, 0, width, height};
        m_pending.clear();
        m_pending.add(full);
        m_forcedRefresh = full;
    }

private:
    struct Request
    {
        bool incremental = false;
        Rect requested;
    };

    BoundedRegion m_pending;
    std::optional<Request> m_request;
    std::optional<Rect> m_forcedRefresh;
};

struct MappedRgbaFrame
{
    const std::uint8_t *data = nullptr;
    std::size_t stride = 0;
    int width = 0;
    int height = 0;
};

inline std::optional<MappedRgbaFrame> mapRgbaFrame(const hyremote::RemoteFrame &frame)
{
    if (frame.geometry.pixelFormat != hyremote::PixelFormat::Rgba8888 || !frame.storage
        || frame.geometry.size.width == 0 || frame.geometry.size.height == 0
        || frame.geometry.size.width > static_cast<std::uint32_t>(std::numeric_limits<int>::max())
        || frame.geometry.size.height > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
        return std::nullopt;
    }

    const auto plane = frame.storage->mapRead(0);
    if (!plane || !plane->data)
        return std::nullopt;

    const std::size_t width = frame.geometry.size.width;
    const std::size_t height = frame.geometry.size.height;
    if (plane->stride < width * 4U || plane->bytes < plane->stride * height)
        return std::nullopt;

    return MappedRgbaFrame{
        reinterpret_cast<const std::uint8_t *>(plane->data),
        plane->stride,
        static_cast<int>(width),
        static_cast<int>(height),
    };
}

inline Rect fullRect(const hyremote::RemoteFrame &frame) noexcept
{
    if (frame.geometry.size.width > static_cast<std::uint32_t>(std::numeric_limits<int>::max())
        || frame.geometry.size.height > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
        return {};
    }
    return {0,
            0,
            static_cast<int>(frame.geometry.size.width),
            static_cast<int>(frame.geometry.size.height)};
}

inline bool sameDiffGeometry(const hyremote::RemoteFrame &a, const hyremote::RemoteFrame &b) noexcept
{
    return a.geometry.size.width == b.geometry.size.width
           && a.geometry.size.height == b.geometry.size.height
           && a.geometry.pixelFormat == b.geometry.pixelFormat;
}

inline BoundedRegion changedTiles(const hyremote::RemoteFrame *before,
                                  const hyremote::RemoteFrame &after)
{
    BoundedRegion result;
    const Rect full = fullRect(after);
    if (full.empty())
        return result;

    if (!before || !sameDiffGeometry(*before, after)) {
        result.add(full);
        return result;
    }

    const auto beforeMap = mapRgbaFrame(*before);
    const auto afterMap = mapRgbaFrame(after);
    if (!beforeMap || !afterMap) {
        result.add(full);
        return result;
    }

    for (int y0 = 0; y0 < afterMap->height; y0 += kDamageTile) {
        const int height = std::min(kDamageTile, afterMap->height - y0);
        for (int x0 = 0; x0 < afterMap->width; x0 += kDamageTile) {
            const int width = std::min(kDamageTile, afterMap->width - x0);
            bool changed = false;
            for (int row = 0; row < height; ++row) {
                const auto *a = beforeMap->data + static_cast<std::size_t>(y0 + row) * beforeMap->stride
                                + static_cast<std::size_t>(x0) * 4U;
                const auto *b = afterMap->data + static_cast<std::size_t>(y0 + row) * afterMap->stride
                                + static_cast<std::size_t>(x0) * 4U;
                if (std::memcmp(a, b, static_cast<std::size_t>(width) * 4U) != 0) {
                    changed = true;
                    break;
                }
            }
            if (changed)
                result.add({x0, y0, width, height});
        }
    }
    return result;
}

inline std::uint32_t rgbPixel(const MappedRgbaFrame &frame, int x, int y) noexcept
{
    const auto *pixel = frame.data + static_cast<std::size_t>(y) * frame.stride
                        + static_cast<std::size_t>(x) * 4U;
    return (static_cast<std::uint32_t>(pixel[0]) << 16U)
           | (static_cast<std::uint32_t>(pixel[1]) << 8U)
           | static_cast<std::uint32_t>(pixel[2]);
}

inline void appendCpixel(QByteArray &out, std::uint32_t pixel)
{
    out.append(static_cast<char>((pixel >> 16U) & 0xffU));
    out.append(static_cast<char>((pixel >> 8U) & 0xffU));
    out.append(static_cast<char>(pixel & 0xffU));
}

inline int packedBits(std::size_t paletteSize) noexcept
{
    if (paletteSize == 2U)
        return 1;
    if (paletteSize <= 4U)
        return 2;
    return 4;
}

inline bool appendTrlePayload(QByteArray &out,
                              const hyremote::RemoteFrame &frame,
                              Rect rectangle)
{
    const auto mapped = mapRgbaFrame(frame);
    if (!mapped)
        return false;
    rectangle = intersection(rectangle, {0, 0, mapped->width, mapped->height});
    if (rectangle.empty())
        return false;

    const std::uint64_t rawUpperBound = static_cast<std::uint64_t>(rectangle.width)
                                        * static_cast<std::uint64_t>(rectangle.height) * 3U;
    if (rawUpperBound > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
        return false;
    out.reserve(out.size() + static_cast<qsizetype>(rawUpperBound));

    for (int tileY = rectangle.y; tileY < rectangle.bottom(); tileY += kTrleTile) {
        const int tileHeight = std::min(kTrleTile, rectangle.bottom() - tileY);
        for (int tileX = rectangle.x; tileX < rectangle.right(); tileX += kTrleTile) {
            const int tileWidth = std::min(kTrleTile, rectangle.right() - tileX);
            std::array<std::uint32_t, 16> palette{};
            std::array<std::uint8_t, kTrleTile * kTrleTile> indices{};
            std::size_t paletteSize = 0;
            bool paletteOverflow = false;

            for (int y = 0; y < tileHeight && !paletteOverflow; ++y) {
                for (int x = 0; x < tileWidth; ++x) {
                    const std::uint32_t pixel = rgbPixel(*mapped, tileX + x, tileY + y);
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
                    indices[static_cast<std::size_t>(y * tileWidth + x)]
                        = static_cast<std::uint8_t>(index);
                }
            }

            if (paletteOverflow) {
                out.append(char(0));
                for (int y = 0; y < tileHeight; ++y) {
                    for (int x = 0; x < tileWidth; ++x)
                        appendCpixel(out, rgbPixel(*mapped, tileX + x, tileY + y));
                }
                continue;
            }

            if (paletteSize == 1U) {
                out.append(char(1));
                appendCpixel(out, palette[0]);
                continue;
            }

            out.append(static_cast<char>(paletteSize));
            for (std::size_t i = 0; i < paletteSize; ++i)
                appendCpixel(out, palette[i]);

            const int bits = packedBits(paletteSize);
            for (int y = 0; y < tileHeight; ++y) {
                std::uint8_t byte = 0;
                int used = 0;
                for (int x = 0; x < tileWidth; ++x) {
                    const auto index = indices[static_cast<std::size_t>(y * tileWidth + x)];
                    byte = static_cast<std::uint8_t>((byte << bits) | index);
                    used += bits;
                    if (used == 8) {
                        out.append(static_cast<char>(byte));
                        byte = 0;
                        used = 0;
                    }
                }
                if (used != 0) {
                    byte = static_cast<std::uint8_t>(byte << (8 - used));
                    out.append(static_cast<char>(byte));
                }
            }
        }
    }
    return true;
}

}  // namespace HyRemote::detail::rfb_delivery
