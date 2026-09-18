// SPDX-License-Identifier: Apache-2.0
#include "hyremote/core/types.hpp"

#include "hyremote/core/storage.hpp"

namespace hyremote {

std::string_view pixelFormatName(PixelFormat format)
{
    switch (format) {
    case PixelFormat::Unknown:
        return "Unknown";
    case PixelFormat::Bgra8888:
        return "Bgra8888";
    case PixelFormat::Rgba8888:
        return "Rgba8888";
    case PixelFormat::Bgrx8888:
        return "Bgrx8888";
    case PixelFormat::Rgbx8888:
        return "Rgbx8888";
    }
    return "Unknown";
}

std::string_view storageKindName(StorageKind kind)
{
    switch (kind) {
    case StorageKind::Cpu:
        return "Cpu";
    case StorageKind::External:
        return "External";
    }
    return "External";
}

}  // namespace hyremote
