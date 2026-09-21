#pragma once

#include <HyRemote/RemoteAccessExport.h>

#include <cstddef>
#include <memory>
#include <optional>

#include "hyremote/core/frame.hpp"
#include "hyremote/core/input.hpp"
#include "hyremote/core/storage.hpp"

namespace HyRemote::detail {

// Runtime-private helpers used by the application-level automatic composition path.
// They are owned by Common Runtime rather than any integration frontend and are not
// installed as application-facing SDK API.
struct WritableCpuFrame
{
    std::shared_ptr<hyremote::FrameStorage> storage;
    std::byte *data = nullptr;
    std::size_t bytesPerLine = 0;
};

HYREMOTE_REMOTEACCESS_EXPORT WritableCpuFrame createWritableCpuFrame(std::size_t bytesPerLine,
                                                                     std::size_t height);

HYREMOTE_REMOTEACCESS_EXPORT WritableCpuFrame writableCpuFrameOf(const hyremote::RemoteFrame &frame);

HYREMOTE_REMOTEACCESS_EXPORT std::optional<hyremote::MappedInputPoint>
mapPointerToNormalizedTarget(const hyremote::InputEvent &event, float targetWidth, float targetHeight);

} // namespace HyRemote::detail
