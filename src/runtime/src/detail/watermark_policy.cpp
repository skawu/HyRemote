#include "watermark_policy.hpp"

#include <QChar>
#include <QRegularExpression>

#include <cmath>

namespace HyRemote::detail {

namespace {

// A watermark whose glyphs cannot be seen or read is not a watermark, so the text has to be one visible line.
// Beyond control characters this also rejects format characters: zero-width characters would let an "enabled"
// watermark render as nothing, and bidi overrides (U+202A..U+202E, U+2066..U+2069) would let the visible order
// differ from the configured text. Private-use code points stay allowed because icon fonts legitimately use them.
bool isSingleLineVisible(const QString &text)
{
    for (const QChar character : text) {
        switch (character.category()) {
        case QChar::Other_Control:
        case QChar::Other_Format:
        case QChar::Other_Surrogate:
        case QChar::Other_NotAssigned:
        case QChar::LineSeparator:
        case QChar::ParagraphSeparator:
            return false;
        default:
            break;
        }
    }
    return true;
}

std::optional<quint32> parseColor(const QString &value, QString &error)
{
    static const QRegularExpression pattern(QStringLiteral("^#[0-9A-Fa-f]{6}$"));
    // Surrounding whitespace is tolerated like every other textual input; the value is never echoed back in a
    // diagnostic, because a paste error can put unrelated operator text into this field.
    const QString normalized = value.trimmed();
    if (!pattern.match(normalized).hasMatch()) {
        error = QStringLiteral("the watermark colour must be a #RRGGBB value");
        return std::nullopt;
    }
    return static_cast<quint32>(normalized.mid(1).toUInt(nullptr, 16));
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

    if (policy.text.size() > kWatermarkMaxTextCodeUnits) {
        result.error = QStringLiteral("the watermark text is %1 UTF-16 code units long, but the limit is %2")
                           .arg(policy.text.size())
                           .arg(kWatermarkMaxTextCodeUnits);
        return result;
    }

    if (!isSingleLineVisible(policy.text)) {
        result.error = QStringLiteral("the watermark text must be a single visible line without control, "
                                      "zero-width or bidi formatting characters");
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

QStringList watermarkFontFamilyCandidates()
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
