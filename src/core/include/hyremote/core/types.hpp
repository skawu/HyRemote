// SPDX-License-Identifier: Apache-2.0
#pragma once

// Basic value types shared by the whole Core contract.
//
// Design input: docs/proposals/hyremote_core.hpp (ARCH-01). This header is the
// implementation-facing refinement of that proposal; the semantics are frozen by
// docs/adr/0002-remoteframe-lifetime-timestamps.md.
//
// Everything here is an ordinary C++17 value type: no Qt, no protocol and no platform types
// may appear in the Core contract (ADR-0001).

#include <chrono>
#include <cstdint>
#include <string_view>

namespace hyremote {

// Monotonic clock for every Core-visible timestamp. Wall-clock time is never used for frame
// ordering or age comparisons, because a distributed viewer needs a monotonic local domain.
using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

// Monotonic id of a frame accepted by Core. Assigned in Core acceptance order, which is the
// frame ready/completion order, never the capture request order (ADR-0002).
using FrameId = std::uint64_t;

// Diagnostic id of a capture request. It exists to correlate a request with its completion and
// to compute request-to-completion latency. It is never a substitute for a FrameId and never a
// content timestamp.
using CaptureRequestId = std::uint64_t;

struct Size
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

// Rectangle in frame/root coordinates (ADR-0002: damage rectangles preserve frame/root
// coordinates; Core never reinterprets them).
struct Rect
{
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

// v0.1 CPU baselines normalize to packed RGB(A) formats. Optimized/YUV/external formats may be
// added without changing Session semantics.
enum class PixelFormat {
    Unknown,
    Bgra8888,
    Rgba8888,
    Bgrx8888,
    Rgbx8888,
};

enum class AlphaMode {
    Opaque,
    Straight,
    Premultiplied,
};

enum class Rotation {
    Rotate0,
    Rotate90,
    Rotate180,
    Rotate270,
};

struct FrameTransform
{
    Rotation rotation = Rotation::Rotate0;
    bool mirrorX = false;
    bool mirrorY = false;
};

// Stable display name, used in diagnostics and in compatibility failure messages.
std::string_view pixelFormatName(PixelFormat format);

}  // namespace hyremote
