#pragma once

// Shared Qt5.15+ / Qt6 readback CPU-plane normalization.
// The graphics provider owns any GPU resource and only passes CPU bytes.
// No Qt, OS, driver or GPU API types enter this contract.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>

#include "hyremote/core/storage.hpp"

namespace HyRemote::detail {

enum class ReadbackRowOrder {
    TopDown,    // D3D staging and already normalized CPU buffers
    BottomUp   // GL readback origin
};

// Source pitches may exceed the useful output row bytes (e.g. a D3D staging
// texture). Call only after the GPU copy has completed and the source bytes
// are owned for the entire conversion. A failure returns nullptr; no exception
// can escape the conversion worker or be propagated into Qt's render callback.
inline std::shared_ptr<const hyremote::FrameStorage> copyReadbackFrameStorage(
    const std::byte *source, std::size_t sourceBytes,
    std::size_t sourceStride, std::size_t pixelRowBytes,
    std::size_t height, ReadbackRowOrder order) noexcept
{
    if (!source || pixelRowBytes == 0 || height == 0
        || sourceStride < pixelRowBytes
        || pixelRowBytes > std::numeric_limits<std::size_t>::max() / height
        || sourceBytes < pixelRowBytes
        || (height - 1) > (sourceBytes - pixelRowBytes) / sourceStride)
        return {};

    try {
        auto owned = hyremote::CpuFrameStorage::createSinglePlane(
            pixelRowBytes, height);
        std::byte *dest = owned ? owned->mutablePlane(0) : nullptr;
        if (!dest)
            return {};

        if (sourceStride == pixelRowBytes && order == ReadbackRowOrder::TopDown) {
            std::memcpy(dest, source, pixelRowBytes * height);
        } else {
            for (std::size_t y = 0; y < height; ++y) {
                const auto sourceRow = order == ReadbackRowOrder::TopDown
                    ? y : height - y - 1;
                std::memcpy(dest + y * pixelRowBytes,
                            source + sourceRow * sourceStride,
                            pixelRowBytes);
            }
        }
        return owned;
    } catch (...) {
        return {};
    }
}

} // namespace HyRemote::detail
