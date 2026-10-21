#pragma once

#include <QPointF>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace HyRemote::detail {

// Maps a remote RFB framebuffer coordinate to the target's logical coordinate.
// RFB reports physical pixels; QWidget/QQuickWindow mapping functions expect
// logical pixels. Edge coordinates map edge-to-edge, matching mapPointerToTarget.
inline QPointF mapFramebufferCursorPoint(int x,
                                         int y,
                                         std::uint32_t sourceWidth,
                                         std::uint32_t sourceHeight,
                                         qreal targetWidth,
                                         qreal targetHeight)
{
    if (sourceWidth == 0U || sourceHeight == 0U || !std::isfinite(targetWidth)
        || !std::isfinite(targetHeight) || targetWidth <= 0.0 || targetHeight <= 0.0) {
        return QPointF(-1.0, -1.0);
    }
    if (!std::isfinite(static_cast<double>(x)) || !std::isfinite(static_cast<double>(y)))
        return QPointF(-1.0, -1.0);

    const qreal sourceMaxX = sourceWidth > 1U ? static_cast<qreal>(sourceWidth - 1U) : 0.0;
    const qreal sourceMaxY = sourceHeight > 1U ? static_cast<qreal>(sourceHeight - 1U) : 0.0;
    const qreal targetMaxX = targetWidth > 1.0 ? targetWidth - 1.0 : 0.0;
    const qreal targetMaxY = targetHeight > 1.0 ? targetHeight - 1.0 : 0.0;
    const qreal clampedX = std::clamp(static_cast<qreal>(x), 0.0, sourceMaxX);
    const qreal clampedY = std::clamp(static_cast<qreal>(y), 0.0, sourceMaxY);
    return QPointF(sourceMaxX > 0.0 ? (clampedX / sourceMaxX) * targetMaxX : 0.0,
                   sourceMaxY > 0.0 ? (clampedY / sourceMaxY) * targetMaxY : 0.0);
}

}  // namespace HyRemote::detail
