#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <utility>
#include <vector>

#include "hyremote/core/frame.hpp"

namespace HyRemote::detail {

constexpr std::uint32_t kRfbDamageTile = 64;
constexpr std::size_t kRfbMaxDamageRects = 64;

struct RfbRect
{
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;

    bool empty() const noexcept { return width <= 0 || height <= 0; }
    std::int32_t right() const noexcept { return x + width; }
    std::int32_t bottom() const noexcept { return y + height; }
};

inline bool operator==(const RfbRect &a, const RfbRect &b) noexcept
{
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

inline RfbRect intersectRfbRect(const RfbRect &a, const RfbRect &b) noexcept
{
    const auto x0 = std::max(a.x, b.x);
    const auto y0 = std::max(a.y, b.y);
    const auto x1 = std::min(a.right(), b.right());
    const auto y1 = std::min(a.bottom(), b.bottom());
    return {x0, y0, std::max<std::int32_t>(0, x1 - x0), std::max<std::int32_t>(0, y1 - y0)};
}

inline RfbRect boundRfbRects(const RfbRect &a, const RfbRect &b) noexcept
{
    if (a.empty())
        return b;
    if (b.empty())
        return a;
    const auto x0 = std::min(a.x, b.x);
    const auto y0 = std::min(a.y, b.y);
    const auto x1 = std::max(a.right(), b.right());
    const auto y1 = std::max(a.bottom(), b.bottom());
    return {x0, y0, x1 - x0, y1 - y0};
}

inline bool containsRfbRect(const RfbRect &outer, const RfbRect &inner) noexcept
{
    return !inner.empty() && outer.x <= inner.x && outer.y <= inner.y
           && outer.right() >= inner.right() && outer.bottom() >= inner.bottom();
}

inline std::vector<RfbRect> subtractRfbRect(const RfbRect &source, const RfbRect &cut)
{
    const RfbRect hit = intersectRfbRect(source, cut);
    if (hit.empty())
        return {source};

    std::vector<RfbRect> result;
    for (const RfbRect &candidate : {
             RfbRect{source.x, source.y, source.width, hit.y - source.y},
             RfbRect{source.x, hit.bottom(), source.width, source.bottom() - hit.bottom()},
             RfbRect{source.x, hit.y, hit.x - source.x, hit.height},
             RfbRect{hit.right(), hit.y, source.right() - hit.right(), hit.height},
         }) {
        if (!candidate.empty())
            result.push_back(candidate);
    }
    return result;
}

class RfbDamageRegion
{
public:
    bool empty() const noexcept { return m_rects.empty(); }
    std::size_t size() const noexcept { return m_rects.size(); }
    const std::vector<RfbRect> &rects() const noexcept { return m_rects; }

    void clear() { m_rects.clear(); }

    void add(RfbRect rect)
    {
        if (rect.empty())
            return;
        for (auto it = m_rects.begin(); it != m_rects.end();) {
            if (containsRfbRect(*it, rect))
                return;
            if (containsRfbRect(rect, *it)) {
                it = m_rects.erase(it);
                continue;
            }
            ++it;
        }
        m_rects.push_back(rect);
        enforceBound();
    }

    void add(const RfbDamageRegion &other)
    {
        for (const RfbRect &rect : other.m_rects)
            add(rect);
    }

    RfbDamageRegion intersected(RfbRect requested) const
    {
        RfbDamageRegion result;
        for (const RfbRect &rect : m_rects)
            result.add(intersectRfbRect(rect, requested));
        return result;
    }

    void subtract(const RfbDamageRegion &delivered)
    {
        for (const RfbRect &cut : delivered.m_rects)
            subtractOne(cut);
    }

private:
    void subtractOne(RfbRect cut)
    {
        if (cut.empty())
            return;
        std::vector<RfbRect> next;
        for (const RfbRect &rect : m_rects) {
            auto pieces = subtractRfbRect(rect, cut);
            next.insert(next.end(), pieces.begin(), pieces.end());
        }
        m_rects = std::move(next);
        enforceBound();
    }

    void enforceBound()
    {
        if (m_rects.size() <= kRfbMaxDamageRects)
            return;
        RfbRect all;
        for (const RfbRect &rect : m_rects)
            all = boundRfbRects(all, rect);
        m_rects.clear();
        if (!all.empty())
            m_rects.push_back(all);
    }

    std::vector<RfbRect> m_rects;
};

class RfbUpdateState
{
public:
    void accumulate(const RfbDamageRegion &damage) { m_pending.add(damage); }
    bool hasPendingDamage() const noexcept { return !m_pending.empty(); }
    bool hasOutstandingRequest() const noexcept { return m_request.has_value(); }

    void request(bool incremental, RfbRect requested)
    {
        m_request = Request{incremental, requested};
    }

    void cancelRequest() noexcept { m_request.reset(); }

    std::optional<RfbDamageRegion> selectEligible() const
    {
        if (!m_request)
            return std::nullopt;

        RfbDamageRegion selected;
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

    void commitDelivered(const RfbDamageRegion &delivered)
    {
        if (!m_request)
            return;
        m_pending.subtract(delivered);
        m_forcedRefresh.reset();
        m_request.reset();
    }

    void invalidateFull(std::uint32_t width, std::uint32_t height)
    {
        const RfbRect full{0,
                           0,
                           static_cast<std::int32_t>(width),
                           static_cast<std::int32_t>(height)};
        m_pending.clear();
        m_pending.add(full);
        m_forcedRefresh = full;
    }

private:
    struct Request
    {
        bool incremental = false;
        RfbRect requested;
    };

    RfbDamageRegion m_pending;
    std::optional<Request> m_request;
    std::optional<RfbRect> m_forcedRefresh;
};

inline RfbDamageRegion fullRfbDamage(std::uint32_t width, std::uint32_t height)
{
    RfbDamageRegion result;
    result.add({0, 0, static_cast<std::int32_t>(width), static_cast<std::int32_t>(height)});
    return result;
}

constexpr RfbRect clipCoreDamageRect(const hyremote::Rect &rect,
                                     std::uint32_t frameWidth,
                                     std::uint32_t frameHeight) noexcept
{
    const std::int64_t fw = static_cast<std::int64_t>(frameWidth);
    const std::int64_t fh = static_cast<std::int64_t>(frameHeight);
    const std::int64_t left = std::max<std::int64_t>(0, static_cast<std::int64_t>(rect.x));
    const std::int64_t top = std::max<std::int64_t>(0, static_cast<std::int64_t>(rect.y));
    const std::int64_t right = std::min<std::int64_t>(
        fw, static_cast<std::int64_t>(rect.x) + static_cast<std::int64_t>(rect.width));
    const std::int64_t bottom = std::min<std::int64_t>(
        fh, static_cast<std::int64_t>(rect.y) + static_cast<std::int64_t>(rect.height));
    if (right <= left || bottom <= top)
        return {};
    return {static_cast<std::int32_t>(left),
            static_cast<std::int32_t>(top),
            static_cast<std::int32_t>(right - left),
            static_cast<std::int32_t>(bottom - top)};
}

constexpr auto kWideDamageClipRegression = clipCoreDamageRect(
    hyremote::Rect{-10, -5, UINT32_MAX, UINT32_MAX}, 100U, 80U);
static_assert(kWideDamageClipRegression.x == 0 && kWideDamageClipRegression.y == 0
              && kWideDamageClipRegression.width == 100 && kWideDamageClipRegression.height == 80);

inline RfbDamageRegion regionsFromFrameDamage(const hyremote::RemoteFrame &frame)
{
    RfbDamageRegion result;
    if (frame.damage.kind != hyremote::DamageKind::Regions)
        return result;

    for (const hyremote::Rect &rect : frame.damage.regions)
        result.add(clipCoreDamageRect(rect, frame.geometry.size.width, frame.geometry.size.height));
    return result;
}

constexpr bool rfbFrameIdsAreAdjacent(hyremote::FrameId previous, hyremote::FrameId current) noexcept
{
    return previous != 0 && current != 0 && current > previous && current - previous == 1U;
}

static_assert(rfbFrameIdsAreAdjacent(10U, 11U));
static_assert(!rfbFrameIdsAreAdjacent(10U, 12U));
static_assert(!rfbFrameIdsAreAdjacent(0U, 1U));
static_assert(!rfbFrameIdsAreAdjacent(11U, 10U));

inline RfbDamageRegion changedRfbTiles(const hyremote::RemoteFrame *previous,
                                       const hyremote::RemoteFrame &current)
{
    const auto width = current.geometry.size.width;
    const auto height = current.geometry.size.height;
    if (width == 0 || height == 0)
        return {};

    // A Regions list describes the delta from the immediately preceding accepted frame. The
    // RFB transport mailbox is latest-frame-wins, so more than one accepted frame may be
    // coalesced before the worker observes it. Only trust Regions when FrameId proves that no
    // accepted frame was skipped; otherwise diff the actual last-observed pixels against the
    // newest frame so a delta from a discarded intermediate frame cannot be lost.
    const bool adjacentAcceptedFrame
        = previous && rfbFrameIdsAreAdjacent(previous->id, current.id);
    if (current.damage.kind == hyremote::DamageKind::Regions && adjacentAcceptedFrame)
        return regionsFromFrameDamage(current);

    if (!previous || previous->geometry.size.width != width
        || previous->geometry.size.height != height
        || previous->geometry.pixelFormat != current.geometry.pixelFormat
        || previous->geometry.planeCount != current.geometry.planeCount) {
        return fullRfbDamage(width, height);
    }

    if (previous->storage == current.storage)
        return {};

    if (current.geometry.pixelFormat != hyremote::PixelFormat::Rgba8888
        || current.geometry.planeCount != 1 || !previous->storage || !current.storage) {
        return fullRfbDamage(width, height);
    }

    const auto before = previous->storage->mapRead(0);
    const auto after = current.storage->mapRead(0);
    const std::size_t rowBytes = static_cast<std::size_t>(width) * 4U;
    if (!before || !after || !before->data || !after->data || before->stride < rowBytes
        || after->stride < rowBytes
        || before->bytes < before->stride * static_cast<std::size_t>(height)
        || after->bytes < after->stride * static_cast<std::size_t>(height)) {
        return fullRfbDamage(width, height);
    }

    RfbDamageRegion result;
    const auto *beforeBase = reinterpret_cast<const std::uint8_t *>(before->data);
    const auto *afterBase = reinterpret_cast<const std::uint8_t *>(after->data);
    for (std::uint32_t y0 = 0; y0 < height; y0 += kRfbDamageTile) {
        const std::uint32_t tileHeight = std::min(kRfbDamageTile, height - y0);
        for (std::uint32_t x0 = 0; x0 < width; x0 += kRfbDamageTile) {
            const std::uint32_t tileWidth = std::min(kRfbDamageTile, width - x0);
            bool changed = false;
            for (std::uint32_t row = 0; row < tileHeight; ++row) {
                const auto beforeOffset = static_cast<std::size_t>(y0 + row) * before->stride
                                          + static_cast<std::size_t>(x0) * 4U;
                const auto afterOffset = static_cast<std::size_t>(y0 + row) * after->stride
                                         + static_cast<std::size_t>(x0) * 4U;
                if (std::memcmp(beforeBase + beforeOffset,
                                afterBase + afterOffset,
                                static_cast<std::size_t>(tileWidth) * 4U)
                    != 0) {
                    changed = true;
                    break;
                }
            }
            if (changed) {
                result.add({static_cast<std::int32_t>(x0),
                            static_cast<std::int32_t>(y0),
                            static_cast<std::int32_t>(tileWidth),
                            static_cast<std::int32_t>(tileHeight)});
            }
        }
    }
    return result;
}

}  // namespace HyRemote::detail
