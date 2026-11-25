#pragma once

#include <cstdint>

#include "hyremote/core/frame.hpp"

namespace HyRemote::detail {

// Shared by Raw/TRLE wire encoding and visible-pixel damage comparison.
// RFB has no alpha channel: straight RGB is composited on black; premultiplied
// RGB is already black-composited, and opaque pixels are sent unchanged.
// Keep this transport presentation contract independent of Qt and render Kit.
inline std::uint8_t rfbOpaqueChannel(std::uint8_t channel,
                                     std::uint8_t alpha,
                                     hyremote::AlphaMode mode) noexcept
{
    if (mode == hyremote::AlphaMode::Straight) {
        return static_cast<std::uint8_t>((static_cast<std::uint32_t>(channel) * alpha + 127U)
                                         / 255U);
    }
    return channel;
}

}  // namespace HyRemote::detail
