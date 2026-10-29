#include "watermark_policy.hpp"

#include <QChar>
#include <QRegularExpression>

#include <cmath>

namespace HyRemote::detail {

namespace {

bool isSingleLinePrintable(const QString &text)
{
    for (const QChar character : text) {
        if (character == QLatin1Char(' '))
            continue;
        if (!character.isPrint())
            return false;
    }
    return true;
}

std::optional<quint32> parseColor(const QString &value, QString &error)
{
    static const QRegularExpression pattern(QStringLiteral("^#[0-9A-Fa-f]{6}$"));
    if (!pattern.match(value).hasMatch()) {
        error = QStringLiteral("the watermark colour '%1' is not a #RRGGBB value").arg(value);
        return std::nullopt;
    }
    return static_cast<quint32>(value.mid(1).toUInt(nullptr, 16));
}

}  // namespace

WatermarkPolicyResult resolveWatermarkPolicy(const WatermarkPolicyRequest &request)
{
    WatermarkPolicyResult result;
    WatermarkPolicy &policy = result.policy;

    // Defaults first: every field the route left unset keeps the one shared default, and the operator's intent
    // stays visible in the policy even when validation fails below.
    policy.enabled = request.enabled;
    policy.text = request.text;
    policy.position = request.position.value_or(WatermarkPosition::BottomRight);
    policy.opacity = request.opacity.value_or(kWatermarkDefaultOpacity);
    policy.fontPointSize = request.fontPointSize.value_or(kWatermarkDefaultFontPointSize);
    policy.colorRgb = kWatermarkDefaultColorRgb;

    if (policy.text.size() > kWatermarkMaxTextLength) {
        result.error = QStringLiteral("the watermark text is %1 characters long, but the limit is %2")
                           .arg(policy.text.size())
                           .arg(kWatermarkMaxTextLength);
        return result;
    }

    if (!isSingleLinePrintable(policy.text)) {
        result.error = QStringLiteral("the watermark text must be a single line without control characters");
        return result;
    }

    if (policy.enabled && policy.text.trimmed().isEmpty()) {
        result.error = QStringLiteral("the watermark is enabled, but no watermark text was provided; "
                                      "HyRemote never invents one");
        return result;
    }

    if (!std::isfinite(policy.opacity) || policy.opacity < kWatermarkMinOpacity
        || policy.opacity > kWatermarkMaxOpacity) {
        result.error = QStringLiteral("the watermark opacity %1 is outside %2..%3")
                           .arg(policy.opacity, 0, 'g', 4)
                           .arg(kWatermarkMinOpacity, 0, 'g', 2)
                           .arg(kWatermarkMaxOpacity, 0, 'g', 2);
        return result;
    }

    if (policy.fontPointSize < kWatermarkMinFontPointSize
        || policy.fontPointSize > kWatermarkMaxFontPointSize) {
        result.error = QStringLiteral("the watermark font size %1 is outside %2..%3 points")
                           .arg(policy.fontPointSize)
                           .arg(kWatermarkMinFontPointSize)
                           .arg(kWatermarkMaxFontPointSize);
        return result;
    }

    if (request.color) {
        QString colorError;
        const std::optional<quint32> rgb = parseColor(*request.color, colorError);
        if (!rgb) {
            result.error = colorError;
            return result;
        }
        policy.colorRgb = *rgb;
    }

    return result;
}

QString watermarkPositionName(WatermarkPosition position)
{
    switch (position) {
    case WatermarkPosition::BottomRight:
        return QStringLiteral("bottom-right");
    case WatermarkPosition::BottomLeft:
        return QStringLiteral("bottom-left");
    case WatermarkPosition::TopRight:
        return QStringLiteral("top-right");
    case WatermarkPosition::TopLeft:
        return QStringLiteral("top-left");
    case WatermarkPosition::Tiled:
        return QStringLiteral("tiled");
    }
    return QStringLiteral("bottom-right");
}

bool parseWatermarkPosition(const QString &token, WatermarkPosition &position)
{
    const QString normalized = token.trimmed().toLower();
    const struct {
        const char *token;
        WatermarkPosition position;
    } known[] = {
        {"bottom-right", WatermarkPosition::BottomRight},
        {"bottom-left", WatermarkPosition::BottomLeft},
        {"top-right", WatermarkPosition::TopRight},
        {"top-left", WatermarkPosition::TopLeft},
        {"tiled", WatermarkPosition::Tiled},
    };

    for (const auto &entry : known) {
        if (normalized == QLatin1String(entry.token)) {
            position = entry.position;
            return true;
        }
    }
    return false;
}

QStringList watermarkFontFamilies()
{
    // Preference order only: the renderer asks Qt for the first available family and lets Qt fall back through
    // the rest, so Latin and CJK text keep a real glyph source instead of a placeholder box.
    static const QStringList families = {
        QStringLiteral("Microsoft YaHei UI"),
        QStringLiteral("Microsoft YaHei"),
        QStringLiteral("Segoe UI"),
        QStringLiteral("Noto Sans CJK SC"),
        QStringLiteral("Noto Sans SC"),
        QStringLiteral("DejaVu Sans"),
        QStringLiteral("sans-serif"),
    };
    return families;
}

}  // namespace HyRemote::detail
