#include "detail/composition_support.hpp"

#include <limits>
#include <utility>

namespace HyRemote::detail {

WritableCpuFrame createWritableCpuFrame(std::size_t bytesPerLine, std::size_t height)
{
    WritableCpuFrame result;
    auto storage = hyremote::CpuFrameStorage::createSinglePlane(bytesPerLine, height);
    if (storage) {
        result.data = storage->mutablePlane(0);
        result.bytesPerLine = bytesPerLine;
        result.storage = std::move(storage);
    }
    return result;
}

WritableCpuFrame writableCpuFrameOf(const hyremote::RemoteFrame &frame)
{
    WritableCpuFrame result;
    if (!frame.storage
        || frame.geometry.pixelFormat != hyremote::PixelFormat::Rgba8888
        || frame.geometry.planeCount != 1
        || frame.geometry.size.width == 0
        || frame.geometry.size.height == 0) {
        return result;
    }

    auto storage = std::dynamic_pointer_cast<hyremote::CpuFrameStorage>(
        std::const_pointer_cast<hyremote::FrameStorage>(frame.storage));
    if (!storage || storage->planeCount() != 1)
        return result;

    const auto &layout = storage->layout();
    if (layout.size() != 1)
        return result;

    constexpr std::size_t bytesPerPixel = 4;
    const std::size_t width = static_cast<std::size_t>(frame.geometry.size.width);
    const std::size_t height = static_cast<std::size_t>(frame.geometry.size.height);
    if (width > std::numeric_limits<std::size_t>::max() / bytesPerPixel)
        return result;

    const std::size_t minimumStride = width * bytesPerPixel;
    const std::size_t actualStride = layout[0].bytesPerLine;
    if (actualStride < minimumStride || actualStride == 0)
        return result;
    if (height > std::numeric_limits<std::size_t>::max() / actualStride)
        return result;

    const std::size_t requiredBytes = actualStride * height;
    if (layout[0].bytes < requiredBytes)
        return result;

    std::byte *data = storage->mutablePlane(0);
    if (!data)
        return result;

    result.data = data;
    result.bytesPerLine = actualStride;
    result.storage = std::move(storage);
    return result;
}

std::optional<hyremote::MappedInputPoint> mapPointerToNormalizedTarget(const hyremote::InputEvent &event,
                                                                      float targetWidth,
                                                                      float targetHeight)
{
    return hyremote::mapPointerToTarget(event, targetWidth, targetHeight);
}

} // namespace HyRemote::detail
