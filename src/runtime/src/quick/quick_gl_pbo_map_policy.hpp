#pragma once
// #967 review: ONE geometry-derived admission rule for the experimental GL/PBO
// readback. The Qt render callback sizes and allocates the PBO it will read
// into, and the session retire worker later maps exactly that many bytes into
// an owned CPU frame. Those two decisions used to live in two different
// translation-unit-local expressions, so they could disagree: the submit path
// accepted any overflow-safe geometry while the worker refused to map anything
// above a fixed 40 MiB ceiling. When they disagree the GPU capture succeeds and
// the *delivery* is reported as a readback failure, which is indistinguishable
// from a real driver fault in the trace.
//
// This header is Qt-version neutral: Qt5.15 direct GL and Qt6 RHI share it.
#include <cstddef>
#include <cstdint>
#include <limits>

namespace HyRemote::detail {

// Bytes of one top-down RGBA8 readback, or 0 when the geometry cannot be
// expressed as a CPU frame. Overflow is checked before it can wrap; nothing
// here consults a GPU/OS/chipset name.
inline std::size_t glPboReadbackBytes(std::uint32_t width,
                                     std::uint32_t height) noexcept
{
    if (width == 0 || height == 0)
        return 0;
    constexpr std::size_t bytesPerPixel = 4u; // GL_RGBA / GL_UNSIGNED_BYTE
    if (width > std::numeric_limits<std::size_t>::max() / bytesPerPixel)
        return 0;
    const std::size_t rowBytes = std::size_t(width) * bytesPerPixel;
    if (height > std::numeric_limits<std::size_t>::max() / rowBytes)
        return 0;
    return rowBytes * std::size_t(height);
}

// True when the retire worker can turn a captured frame of this geometry into
// an owned CPU frame. The submit path uses the same predicate, so "accepted for
// readback" and "mappable" cannot diverge again.
//
// #967 review: there is deliberately NO byte ceiling here. A predecessor
// version refused anything above 40 MiB, which is not a product limit:
//  * 1920x1080      =   8_294_400 bytes  (the only #962/#987 fixed-SHA cells)
//  * 3440x1440@150% =  44_582_400 bytes  (5160x2160)
//  * 3840x2160@200% = 132_710_400 bytes  (7680x4320)
// The peer D3D11 staging route copies those same frame sizes without a cap, and
// the PBO itself is already sized from this geometry, so the ceiling expressed
// only an untested assumption about window size. A genuine allocation failure
// is reported by the mapping step instead of being pre-judged here.
inline bool glPboReadbackMappable(std::uint32_t width,
                                 std::uint32_t height) noexcept
{
    return glPboReadbackBytes(width, height) > 0;
}

} // namespace HyRemote::detail
