#pragma once

// #609: Runtime-private CPU frame storage backed by the implicitly shared
// QImage returned by QQuickItemGrabResult::image(). The pixel owner must outlive
// any Core/transport consumer, including consumers on non-GUI threads.
//
// Only this exact byte format is published as hyremote::Rgba8888/Premultiplied.
// Other QImage formats are normalized by the Quick capture adapter first.
#include <QImage>

#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "hyremote/core/storage.hpp"

namespace HyRemote::detail {

class QuickImageFrameStorage final : public hyremote::FrameStorage
{
public:
    static std::shared_ptr<const hyremote::FrameStorage> create(const QImage &image)
    {
        if (image.isNull() || image.format() != QImage::Format_RGBA8888_Premultiplied
            || image.width() <= 0 || image.height() <= 0)
            return {};

        const std::size_t width = static_cast<std::size_t>(image.width());
        const std::size_t height = static_cast<std::size_t>(image.height());
        if (width > std::numeric_limits<std::size_t>::max() / 4U)
            return {};

        const auto qtStride = image.bytesPerLine();
        const auto qtBytes = image.sizeInBytes();
        if (qtStride <= 0 || qtBytes <= 0)
            return {};

        const std::size_t stride = static_cast<std::size_t>(qtStride);
        const std::size_t bytes = static_cast<std::size_t>(qtBytes);
        if (stride < width * 4U || stride > std::numeric_limits<std::size_t>::max() / height
            || bytes < stride * height || !image.constBits())
            return {};

        // A QImage value copy retains the same Qt-managed CPU pixel storage.
        // No detached bytes, Qt window, render-context or GPU object is kept.
        return std::shared_ptr<const hyremote::FrameStorage>(
            new QuickImageFrameStorage(image, stride, bytes));
    }

    hyremote::StorageKind kind() const noexcept override { return hyremote::StorageKind::Cpu; }
    std::size_t planeCount() const noexcept override { return 1U; }

    std::optional<hyremote::PlaneView> mapRead(std::size_t plane) const override
    {
        if (plane != 0U)
            return std::nullopt;
        // m_image is const. Never call mutable bits()/scanLine(): those detach
        // an 8 MiB 1080p frame and break the shared-read-only contract.
        return hyremote::PlaneView{
            reinterpret_cast<const std::byte *>(m_image.constBits()),
            m_bytes,
            m_stride,
        };
    }

    std::string_view externalDomain() const noexcept override { return {}; }
    std::shared_ptr<const hyremote::StorageExtension> extension(std::string_view) const override
    {
        return {};
    }

private:
    QuickImageFrameStorage(const QImage &image, std::size_t stride, std::size_t bytes)
        : m_image(image)
        , m_stride(stride)
        , m_bytes(bytes)
    {
    }

    const QImage m_image;
    const std::size_t m_stride;
    const std::size_t m_bytes;
};

}  // namespace HyRemote::detail
