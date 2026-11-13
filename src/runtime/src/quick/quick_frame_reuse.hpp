#pragma once

// #967 P1: one common, Qt-free pixel-equivalence contract for the public Qt
// Quick CaptureSource. Qt returns a NEW QImage (and therefore a NEW
// FrameStorage) for every grabToImage() even when the scene did not change a
// single byte. Downstream, that new identity forces a whole-frame watermark
// recomposition, a whole-frame visible-pixel comparison and a whole-frame
// encode/send. This is repeated CPU work on the DEFAULT public path shared by
// every Qt graphics backend, not a property of one RHI or one device.
//
// Frozen rule (docs/internal/967-p1-design-freeze-2026-11-13.md): a new
// RemoteFrame keeps its own request identity, PTS and completion time; only the
// immutable pixels may be shared, and only when they are byte-for-byte equal
// under the same geometry. Everything downstream keeps using the existing
// storage identity fast path, H2 served-frame snapshot and per-viewer damage.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <utility>

#include "hyremote/core/frame.hpp"
#include "hyremote/core/storage.hpp"

namespace HyRemote::detail {

inline bool quickFrameGeometryEquivalent(const hyremote::FrameGeometry &a,
                                         const hyremote::FrameGeometry &b) noexcept
{
    return a.size.width == b.size.width && a.size.height == b.size.height
           && a.pixelFormat == b.pixelFormat && a.alphaMode == b.alphaMode
           && a.planeCount == b.planeCount
           && a.devicePixelRatio == b.devicePixelRatio
           && a.transform.rotation == b.transform.rotation
           && a.transform.mirrorX == b.transform.mirrorX
           && a.transform.mirrorY == b.transform.mirrorY;
}

// Compare only the visible RGBA bytes. Padding, stride and owner identity never
// reach the RFB wire, so two differently padded planes with identical visible
// pixels are equivalent. Any malformed plane fails closed (no reuse).
inline bool quickFramePixelsEqual(const hyremote::FrameStorage &previous,
                                  const hyremote::FrameStorage &current,
                                  const hyremote::FrameGeometry &geometry)
{
    if (geometry.pixelFormat != hyremote::PixelFormat::Rgba8888
        || geometry.planeCount != 1U || geometry.size.width == 0U
        || geometry.size.height == 0U)
        return false;

    const std::size_t width = static_cast<std::size_t>(geometry.size.width);
    const std::size_t height = static_cast<std::size_t>(geometry.size.height);
    if (width > std::numeric_limits<std::size_t>::max() / 4U)
        return false;
    const std::size_t rowBytes = width * 4U;

    try {
        const auto before = previous.mapRead(0);
        const auto after = current.mapRead(0);
        if (!before || !after || !before->data || !after->data)
            return false;

        const auto readable = [&](const hyremote::PlaneView &plane) noexcept {
            if (plane.stride < rowBytes || height == 0U)
                return false;
            if (height > 1U
                && plane.stride
                       > (std::numeric_limits<std::size_t>::max() - rowBytes) / (height - 1U))
                return false;
            return plane.bytes >= plane.stride * (height - 1U) + rowBytes;
        };
        if (!readable(*before) || !readable(*after))
            return false;

        const auto *beforeBase = reinterpret_cast<const std::uint8_t *>(before->data);
        const auto *afterBase = reinterpret_cast<const std::uint8_t *>(after->data);
        if (before->stride == rowBytes && after->stride == rowBytes) {
            return std::memcmp(beforeBase, afterBase, rowBytes * height) == 0;
        }
        for (std::size_t row = 0; row < height; ++row) {
            if (std::memcmp(beforeBase + row * before->stride,
                            afterBase + row * after->stride, rowBytes)
                != 0)
                return false;
        }
        return true;
    } catch (...) {
        // mapRead() is a virtual backend surface. A failure must never drop a
        // real frame: just take the unchanged full-composition path instead.
        return false;
    }
}

// One-slot cache owned by ONE CaptureSource instance. It is not global and not
// shared between sessions, policies or Qt backends. The strong reference is the
// SDK-owned pixel allocation that #996's single zero-viewer edge must release.
class QuickFrameReuseCache final
{
public:
    // Returns the immutable storage this completion must publish. The caller
    // must still build a fresh RemoteFrame around it.
    std::shared_ptr<const hyremote::FrameStorage> publish(
        std::shared_ptr<const hyremote::FrameStorage> candidate,
        const hyremote::FrameGeometry &geometry,
        std::uint64_t generation)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_generation == generation && m_previous && candidate
            && quickFrameGeometryEquivalent(m_geometry, geometry)
            && quickFramePixelsEqual(*m_previous, *candidate, geometry)) {
            return m_previous;
        }
        m_geometry = geometry;
        m_generation = generation;
        m_previous = candidate;
        return std::move(candidate);
    }

    // CaptureSource::releaseIdleResources() (last Viewer left) and every
    // start()/stop()/generation boundary. Never retain pixels across them.
    void reset(std::uint64_t generation) noexcept
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_previous.reset();
        m_geometry = hyremote::FrameGeometry{};
        m_generation = generation;
    }

    void release() noexcept { reset(0U); }

private:
    std::mutex m_mutex;
    std::shared_ptr<const hyremote::FrameStorage> m_previous;
    hyremote::FrameGeometry m_geometry{};
    std::uint64_t m_generation = 0U;
};

} // namespace HyRemote::detail
