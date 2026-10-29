#include "watermark_compositor.hpp"

#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QImage>
#include <QPainter>
#include <QRectF>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace HyRemote::detail {

namespace {

qreal devicePixelRatioOf(const WatermarkFrameView &frame)
{
    return std::isfinite(frame.devicePixelRatio) && frame.devicePixelRatio > 0.0 ? frame.devicePixelRatio : 1.0;
}

int watermarkPixelSize(const WatermarkPolicy &policy, qreal devicePixelRatio)
{
    // The policy is expressed in points, the frame in device pixels: 96 dpi is the reference, so a 16 pt
    // watermark keeps the same physical size on a 1x and a 2x target instead of shrinking on the high-DPI one.
    const qreal pixels = static_cast<qreal>(policy.fontPointSize) * (96.0 / 72.0) * devicePixelRatio;
    return std::max(1, static_cast<int>(std::lround(pixels)));
}

QFont watermarkFont(const WatermarkPolicy &policy, qreal devicePixelRatio)
{
    QFont font;
    font.setFamilies(watermarkFontFamilyCandidates());
    font.setPixelSize(watermarkPixelSize(policy, devicePixelRatio));
    font.setBold(true);
    return font;
}

QColor watermarkColor(const WatermarkPolicy &policy)
{
    return QColor(static_cast<int>((policy.colorRgb >> 16) & 0xFFU),
                  static_cast<int>((policy.colorRgb >> 8) & 0xFFU),
                  static_cast<int>(policy.colorRgb & 0xFFU));
}

void drawCornerWatermark(QPainter &painter, const QSizeF &size, const QString &text, WatermarkPosition position)
{
    const QFontMetricsF metrics = painter.fontMetrics();
    const qreal advance = metrics.horizontalAdvance(text);
    const qreal margin = std::max<qreal>(8.0, std::min(size.width(), size.height()) * 0.02);

    qreal x = 0.0;
    qreal baseline = 0.0;
    switch (position) {
    case WatermarkPosition::BottomRight:
        x = size.width() - margin - advance;
        baseline = size.height() - margin - metrics.descent();
        break;
    case WatermarkPosition::BottomLeft:
        x = margin;
        baseline = size.height() - margin - metrics.descent();
        break;
    case WatermarkPosition::TopRight:
        x = size.width() - margin - advance;
        baseline = margin + metrics.ascent();
        break;
    case WatermarkPosition::TopLeft:
        x = margin;
        baseline = margin + metrics.ascent();
        break;
    case WatermarkPosition::Tiled:
        // Rendered by the tiled pass below; a single pass is never asked for this position.
        return;
    }

    painter.drawText(QPointF(std::max<qreal>(0.0, x), baseline), text);
}

void drawTiledWatermark(QPainter &painter, const QSizeF &size, const QString &text)
{
    const QFontMetricsF metrics = painter.fontMetrics();
    const qreal advance = metrics.horizontalAdvance(text);
    if (advance <= 0.0)
        return;

    // A regular grid: the operator asked for a visible repeating watermark, not for a decorative rotation, and a
    // predictable layout is what the pixel tests can pin.
    const qreal stepX = std::max<qreal>(advance * 1.6, 24.0);
    const qreal stepY = std::max<qreal>(metrics.height() * 2.5, 24.0);
    for (qreal y = stepY * 0.5; y < size.height() + stepY * 0.5; y += stepY) {
        for (qreal x = 0.0; x < size.width(); x += stepX)
            painter.drawText(QPointF(x, y), text);
    }
}

}  // namespace

bool composeWatermark(const WatermarkPolicy &policy, const WatermarkFrameView &frame,
                      QByteArray &destination, QString &error)
{
    if (!policy.enabled)
        return false;

    if (!frame.rgba || frame.width <= 0 || frame.height <= 0 || frame.stride < frame.width * 4) {
        error = QStringLiteral("the frame view cannot be watermarked (missing or malformed RGBA plane)");
        return false;
    }
    if (frame.stride > static_cast<qsizetype>(1) << 24 || frame.height > (1 << 16)) {
        error = QStringLiteral("the frame view is too large to watermark within the protocol baseline");
        return false;
    }

    const qsizetype total = frame.stride * frame.height;
    destination.resize(total);
    if (destination.size() != total) {
        error = QStringLiteral("the compositor could not allocate the watermarked frame");
        return false;
    }
    for (int row = 0; row < frame.height; ++row) {
        std::memcpy(destination.data() + static_cast<qsizetype>(row) * frame.stride,
                    frame.rgba + static_cast<qsizetype>(row) * frame.stride,
                    static_cast<std::size_t>(frame.stride));
    }

    QImage image(reinterpret_cast<uchar *>(destination.data()), frame.width, frame.height, frame.stride,
                 frame.premultiplied ? QImage::Format_RGBA8888_Premultiplied : QImage::Format_RGBA8888);
    if (image.isNull()) {
        destination.clear();
        error = QStringLiteral("the compositor could not wrap the frame buffer");
        return false;
    }

    QPainter painter(&image);
    if (!painter.isActive()) {
        destination.clear();
        error = QStringLiteral("the compositor could not start painting");
        return false;
    }
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setFont(watermarkFont(policy, devicePixelRatioOf(frame)));
    painter.setPen(watermarkColor(policy));
    painter.setOpacity(policy.opacity);

    // A clipped watermark is a silently degraded watermark: part of the operator's text would be missing without
    // anyone being told. Refuse instead of eliding, scaling or truncating, and let the caller surface the reason.
    const QSizeF size(image.width(), image.height());
    const QFontMetricsF metrics = painter.fontMetrics();
    if (metrics.horizontalAdvance(policy.text) > size.width()
        || metrics.height() > size.height()) {
        painter.end();
        destination.clear();
        error = QStringLiteral("the watermark text does not fit a %1x%2 frame at %3 points")
                    .arg(image.width())
                    .arg(image.height())
                    .arg(policy.fontPointSize);
        return false;
    }

    if (policy.position == WatermarkPosition::Tiled)
        drawTiledWatermark(painter, size, policy.text);
    else
        drawCornerWatermark(painter, size, policy.text, policy.position);

    painter.end();
    return true;
}

}  // namespace HyRemote::detail
