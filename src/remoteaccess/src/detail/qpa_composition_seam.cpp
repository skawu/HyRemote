// SPDX-License-Identifier: Apache-2.0
#include "detail/qpa_composition_seam.hpp"

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
    if (!frame.storage)
        return result;

    // The capture path publishes CPU storages; anything else legitimately has no writable CPU view.
    auto storage = std::dynamic_pointer_cast<hyremote::CpuFrameStorage>(
        std::const_pointer_cast<hyremote::FrameStorage>(frame.storage));
    if (!storage)
        return result;

    result.data = storage->mutablePlane(0);
    result.bytesPerLine = static_cast<std::size_t>(frame.geometry.size.width) * 4U;
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
