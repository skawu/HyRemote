#pragma once

// #589 W2: the server-side text watermark compositor. It renders the shared policy onto a *copy* of the frame the
// transport is about to encode, so the captured buffer is never mutated: disabling the watermark, changing the
// policy or attaching a second viewer must always fall back to the untouched capture.

#include "watermark_policy.hpp"

#include <QByteArray>
#include <QString>

#include <cstddef>

namespace HyRemote::detail {

// A read-only RGBA8888 view of the frame plane. `stride` is bytes per row as produced by the capture backend.
struct WatermarkFrameView {
    const uchar *rgba = nullptr;
    qsizetype stride = 0;
    int width = 0;
    int height = 0;
    bool premultiplied = true;
    // The frame's device pixel ratio, so a policy expressed in points stays the same physical size on a 1x and a
    // 2x target instead of halving on the high-DPI one.
    double devicePixelRatio = 1.0;
};

// Composites `policy` onto a fresh copy of the frame.
//
// Returns false and leaves `destination` untouched when there is nothing to do (policy disabled) or when the view
// itself is unusable; in the unusable case `error` explains why. On success `destination` holds
// `stride * height` bytes of RGBA8888 in the same premultiplication mode as the source.
bool composeWatermark(const WatermarkPolicy &policy, const WatermarkFrameView &frame,
                      QByteArray &destination, QString &error);

}  // namespace HyRemote::detail
