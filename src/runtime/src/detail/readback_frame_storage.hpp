#pragma once

// Shared Qt5.15+ / Qt6 readback CPU-plane normalization.
// The graphics provider owns any GPU resource and only passes CPU bytes.
// No Qt, OS, driver or GPU API types enter this contract.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <new>

#include "hyremote/core/storage.hpp"

namespace HyRemote::detail {

enum class ReadbackRowOrder {
    TopDown,    // D3D staging and already normalized CPU buffers
    BottomUp   // GL readback origin
};

enum class ReadbackPixelOrder {
    Rgba8888,  // GL and some native staging textures
    Bgra8888   // D3D11 staging (typical DXGI_FORMAT_B8G8R8A8_UNORM)
};

// Source pitches may exceed the useful output row bytes (e.g. a D3D staging
// texture). Call only after the GPU copy has completed and the source bytes
// are owned for the entire conversion. A failure returns nullptr; no exception
// can escape the conversion worker or be propagated into Qt's render callback.
inline std::shared_ptr<const hyremote::FrameStorage> copyReadbackFrameStorage(
    const std::byte *source, std::size_t sourceBytes,
    std::size_t sourceStride, std::size_t pixelRowBytes,
    std::size_t height, ReadbackRowOrder order,
    ReadbackPixelOrder pixels = ReadbackPixelOrder::Rgba8888) noexcept
{
    if (!source || pixelRowBytes == 0 || height == 0
        || sourceStride < pixelRowBytes
        || (pixels == ReadbackPixelOrder::Bgra8888 && pixelRowBytes % 4 != 0)
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
        if (pixels == ReadbackPixelOrder::Bgra8888) {
            for (std::size_t pos = 0; pos < pixelRowBytes * height; pos += 4) {
                const std::byte b = dest[pos];
                dest[pos] = dest[pos + 2];
                dest[pos + 2] = b;
            }
        }
        return owned;
    } catch (...) {
        return {};
    }
}

// The GPU mapping was already copied into this source-owned CPU allocation.
// Keep that exact allocation as the immutable FrameStorage lifetime anchor,
// rather than allocating a second full frame and copying all pixels again.
// For bottom-up GL, reverse rows IN PLACE using only one stride-sized scratch
// buffer; for D3D staging, preserve RowPitch and swap BGRA channels in place.
// No GL/D3D handle escapes its renderer thread or is retained here.
class AdoptedReadbackFrameStorage final : public hyremote::FrameStorage
{
public:
    AdoptedReadbackFrameStorage(std::shared_ptr<std::byte> data,
                               std::size_t bytes, std::size_t stride) noexcept
        : m_data(std::move(data)), m_bytes(bytes), m_stride(stride) {}

    hyremote::StorageKind kind() const noexcept override
    {
        return hyremote::StorageKind::Cpu;
    }
    std::size_t planeCount() const noexcept override { return 1; }
    std::optional<hyremote::PlaneView> mapRead(std::size_t plane) const override
    {
        if (plane != 0 || !m_data)
            return std::nullopt;
        return hyremote::PlaneView{m_data.get(), m_bytes, m_stride};
    }
    std::string_view externalDomain() const noexcept override { return {}; }
    std::shared_ptr<const hyremote::StorageExtension>
    extension(std::string_view) const override { return {}; }

private:
    const std::shared_ptr<std::byte> m_data;
    const std::size_t m_bytes;
    const std::size_t m_stride;
};

inline std::shared_ptr<const hyremote::FrameStorage> adoptReadbackFrameStorage(
    std::shared_ptr<std::byte> source,
    std::size_t sourceBytes,
    std::size_t sourceStride,
    std::size_t pixelRowBytes,
    std::size_t height,
    ReadbackRowOrder order,
    ReadbackPixelOrder pixels = ReadbackPixelOrder::Rgba8888) noexcept
{
    if (!source || pixelRowBytes == 0 || height == 0
        || pixelRowBytes % 4 != 0
        || sourceStride < pixelRowBytes
        || sourceStride > std::numeric_limits<std::size_t>::max() / height
        || sourceBytes < sourceStride * height)
        return {};
    try {
        std::byte *data = source.get();
        if (order == ReadbackRowOrder::BottomUp && height > 1) {
            std::unique_ptr<std::byte[]> scratch(
                new (std::nothrow) std::byte[sourceStride]);
            if (!scratch)
                return {};
            for (std::size_t y = 0; y < height / 2; ++y) {
                auto *top = data + y * sourceStride;
                auto *bottom = data + (height - y - 1) * sourceStride;
                std::memcpy(scratch.get(), top, sourceStride);
                std::memcpy(top, bottom, sourceStride);
                std::memcpy(bottom, scratch.get(), sourceStride);
            }
        }
        if (pixels == ReadbackPixelOrder::Bgra8888) {
            for (std::size_t y = 0; y < height; ++y) {
                auto *row = data + y * sourceStride;
                for (std::size_t x = 0; x < pixelRowBytes; x += 4) {
                    const std::byte blue = row[x];
                    row[x] = row[x + 2];
                    row[x + 2] = blue;
                }
            }
        }
        return std::make_shared<AdoptedReadbackFrameStorage>(
            std::move(source), sourceBytes, sourceStride);
    } catch (...) {
        return {};
    }
}

} // namespace HyRemote::detail
