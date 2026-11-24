#pragma once

#include <QSize>
#include <QtGlobal>

#include <cmath>

// Explicit Qt5/Qt6 capture geometry contract, derived from tagged upstream
// qquickitemgrabresult.cpp implementations (not API-document assumptions):
//   qtdeclarative v5.15.2: setSize(d->textureSize), lines 259-268
//   qtdeclarative v6.8.3: setSize(d->textureSize * devicePixelRatio), 249-263
// The public grabToImage(targetSize) parameter is therefore a PHYSICAL texture
// size in Qt5.15, but a LOGICAL target that Qt6 multiplies by window DPR.
// Keep this in Shared Runtime, never in Core or a platform-specific provider.
namespace HyRemote::detail {

inline QSize quickGrabTargetForOutput(const QSize &outputPixels,
                                      const QSize &windowLogicalSize,
                                      qreal windowDpr,
                                      bool logicalPixelsForComposite) noexcept
{
    if (outputPixels.isEmpty() || windowLogicalSize.isEmpty()
        || !std::isfinite(windowDpr) || windowDpr <= 0.0)
        return {};
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (!logicalPixelsForComposite)
        return windowLogicalSize; // Qt6 automatically multiplies by DPR

    // Automatic's framebuffer contract is one pixel per logical canvas unit.
    // Request fewer offscreen pixels on high-DPI Qt6; Qt will scale the
    // targetSize back by DPR. Rounding may differ by 1 pixel for fractional
    // scale factors; the Composite scales the child into the logical canvas.
    return QSize(qMax(1, qRound(outputPixels.width() / windowDpr)),
                 qMax(1, qRound(outputPixels.height() / windowDpr)));
#else
    Q_UNUSED(logicalPixelsForComposite);
    return outputPixels; // Qt5.15 uses the targetSize without DPR scaling
#endif
}

} // namespace HyRemote::detail
