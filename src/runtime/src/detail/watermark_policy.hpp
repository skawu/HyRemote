#pragma once

// #589: one static text watermark policy shared by every integration route. The route only supplies what the
// operator asked for; defaults, bounds and error wording live here so C++/QML/Generic/QPA cannot drift apart.
//
// Owner UX decision (2026-10-29, recorded on #589/#594):
//   * the watermark is disabled by default;
//   * when the operator enables it with a valid non-empty text and does not choose a position, the single line
//     is drawn in the bottom-right corner of the remote picture;
//   * the default opacity is 0.35;
//   * tiling is only used when the operator asks for it;
//   * no text is generated and no user name, IP, time or device identifier is ever inserted;
//   * only the remote output is affected, never the host application's own painted pixels.
//
// The policy is deliberately Qt-Gui free (QRgb/hex text instead of QColor) so a runtime-only configuration
// without any capture adapter still compiles; the renderer converts it at the point where Qt Gui exists.

// The two token helpers are exported so the peer frontend modules (separate shared objects) validate a launch token
// with this one implementation instead of growing their own copy of the vocabulary. Translation units that compile
// this header outside the runtime library - the standalone contract tests - have no generated export header, and get
// an empty decoration, which is exactly what a local build needs.
#if defined(__has_include)
#  if __has_include(<HyRemote/RemoteAccessExport.h>)
#    include <HyRemote/RemoteAccessExport.h>
#  endif
#endif
#ifndef HYREMOTE_REMOTEACCESS_EXPORT
#  define HYREMOTE_REMOTEACCESS_EXPORT
#endif

#include <QString>
#include <QStringList>

#include <optional>

namespace HyRemote::detail {

enum class WatermarkPosition {
    BottomRight,
    BottomLeft,
    TopRight,
    TopLeft,
    Tiled,
};

// Bounds. A supplied value outside its bound is rejected with a diagnostic; it is never clamped, because a
// silently adjusted watermark is a different watermark than the operator asked for.
//
// The length bound counts UTF-16 code units, which is what QString::size() reports and what the diagnostic
// reports. It bounds the *input*, not the rendered width: fitting the composed text into the frame is the
// renderer's job and must never be resolved by clipping the operator's text behind their back (#589 W2).
constexpr int kWatermarkMaxTextCodeUnits = 128;
constexpr double kWatermarkMinOpacity = 0.05;
constexpr double kWatermarkMaxOpacity = 1.0;
constexpr double kWatermarkDefaultOpacity = 0.35;
constexpr int kWatermarkMinFontPointSize = 8;
constexpr int kWatermarkMaxFontPointSize = 96;
constexpr int kWatermarkDefaultFontPointSize = 16;
constexpr quint32 kWatermarkDefaultColorRgb = 0xFFFFFF;  // white

struct WatermarkPolicy {
    bool enabled = false;
    QString text;
    WatermarkPosition position = WatermarkPosition::BottomRight;
    double opacity = kWatermarkDefaultOpacity;
    quint32 colorRgb = kWatermarkDefaultColorRgb;
    int fontPointSize = kWatermarkDefaultFontPointSize;
};

// Everything a route may say. An unset optional means "use the shared default", which is what keeps the four
// routes from inventing their own defaults.
struct WatermarkPolicyRequest {
    bool enabled = false;
    QString text;
    std::optional<WatermarkPosition> position;
    // A token a textual route (QML property, launch parameter) received verbatim. It is validated here so an unknown
    // token fails the policy - and therefore the start - instead of being dropped at a frontend boundary.
    std::optional<QString> positionToken;
    std::optional<double> opacity;
    std::optional<QString> color;  // "#RRGGBB"
    std::optional<int> fontPointSize;
};

struct WatermarkPolicyResult {
    WatermarkPolicy policy;
    // Non-empty when the request is invalid. `policy.enabled` still reflects the operator's intent, so a
    // support report can say "enabled, but invalid" instead of pretending the feature was never asked for.
    QString error;

    bool ok() const { return error.isEmpty(); }
};

// Resolves a request into the effective policy. Defaults fill every field the route left unset; invalid
// input yields an error instead of a downgraded policy.
HYREMOTE_REMOTEACCESS_EXPORT WatermarkPolicyResult resolveWatermarkPolicy(const WatermarkPolicyRequest &request);

// Stable tokens used by launch options, QML string properties and the diagnostic report. Exported only so the peer
// frontend modules (which are separate shared objects) can validate a launch token with this one implementation;
// the header stays uninstalled and is not an application SDK promise.
HYREMOTE_REMOTEACCESS_EXPORT QString watermarkPositionName(WatermarkPosition position);
HYREMOTE_REMOTEACCESS_EXPORT bool parseWatermarkPosition(const QString &token, WatermarkPosition &position);

// Candidate font families in preference order, so CJK text keeps a real glyph source where the first choice is
// not installed. This is a candidate list, not a Qt fallback chain: the renderer must probe each family
// (QFontInfo) and pick the first one that actually resolves, then let Qt fall back within that family.
HYREMOTE_REMOTEACCESS_EXPORT QStringList watermarkFontFamilyCandidates();

}  // namespace HyRemote::detail
