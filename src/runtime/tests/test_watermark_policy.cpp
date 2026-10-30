// #589 W1: the static text watermark policy is one shared Runtime contract. These cases pin the Owner decision
// (disabled by default, bottom-right single line, 0.35 opacity, tiling only on request), the bounded validation
// of every operator-provided value, and the rule that an explicitly enabled watermark whose policy cannot be
// honoured refuses the remote output instead of quietly streaming an unwatermarked picture.

#include "../src/access_instance.hpp"
#include "../src/detail/watermark_policy.hpp"

#include <QCoreApplication>
#include <QString>
#include <QTextStream>

namespace {

int failures = 0;

void check(bool condition, const QString &message)
{
    QTextStream out(stdout);
    if (condition) {
        out << "ok: " << message << '\n';
        return;
    }
    out << "FAIL: " << message << '\n';
    ++failures;
}

using HyRemote::detail::WatermarkPolicyRequest;
using HyRemote::detail::WatermarkPosition;
using HyRemote::detail::parseWatermarkPosition;
using HyRemote::detail::resolveWatermarkPolicy;
using HyRemote::detail::watermarkFontFamilyCandidates;
using HyRemote::detail::watermarkPositionName;

void checkSharedDefaults()
{
    const auto result = resolveWatermarkPolicy(WatermarkPolicyRequest{});
    check(result.ok(), QStringLiteral("an empty request resolves"));
    check(!result.policy.enabled, QStringLiteral("the watermark is disabled by default"));
    check(result.policy.position == WatermarkPosition::BottomRight,
          QStringLiteral("the default position is the bottom-right corner"));
    check(result.policy.opacity == 0.35, QStringLiteral("the default opacity is 0.35"));
    check(result.policy.fontPointSize == 16, QStringLiteral("the default font size is 16 points"));
    check(result.policy.colorRgb == 0xFFFFFFU, QStringLiteral("the default colour is white"));
    check(result.policy.text.isEmpty(), QStringLiteral("no text is invented"));
}

void checkEnabledTextAndDefaults()
{
    WatermarkPolicyRequest request;
    request.enabled = true;
    request.text = QStringLiteral("机密 内部 CONFIDENTIAL");

    const auto result = resolveWatermarkPolicy(request);
    check(result.ok(), QStringLiteral("UTF-8 Chinese/English text is accepted"));
    check(result.policy.enabled, QStringLiteral("the operator intent is preserved"));
    check(result.policy.text == request.text, QStringLiteral("the text is preserved unchanged"));
    check(result.policy.position == WatermarkPosition::BottomRight,
          QStringLiteral("enabling without a position keeps the single bottom-right default"));
    check(result.policy.opacity == 0.35, QStringLiteral("enabling without opacity keeps 0.35"));
}

void checkRejectedText()
{
    {
        WatermarkPolicyRequest request;
        request.enabled = true;
        const auto result = resolveWatermarkPolicy(request);
        check(!result.ok() && result.error.contains(QStringLiteral("no watermark text")),
              QStringLiteral("enabling without text is refused"));
        check(result.policy.enabled, QStringLiteral("the refusal still reports the operator intent"));
    }
    {
        WatermarkPolicyRequest request;
        request.enabled = true;
        request.text = QStringLiteral("   \t  ");
        const auto result = resolveWatermarkPolicy(request);
        check(!result.ok(), QStringLiteral("whitespace-only text is not a watermark"));
    }
    {
        WatermarkPolicyRequest request;
        request.text = QString(129, QLatin1Char('x'));
        const auto result = resolveWatermarkPolicy(request);
        check(!result.ok() && result.error.contains(QStringLiteral("129"))
                  && result.error.contains(QStringLiteral("128")),
              QStringLiteral("over-long text is refused with both lengths named"));
    }
    {
        WatermarkPolicyRequest request;
        request.enabled = true;
        request.text = QStringLiteral("first line\nsecond line");
        const auto result = resolveWatermarkPolicy(request);
        check(!result.ok() && result.error.contains(QStringLiteral("single visible line")),
              QStringLiteral("multi-line text is refused"));
    }
    {
        WatermarkPolicyRequest request;
        request.enabled = true;
        request.text = QString(128, QChar(0x4E2D));  // 128 CJK code units
        check(resolveWatermarkPolicy(request).ok(), QStringLiteral("text at the 128 limit is accepted"));
    }
    {
        // A zero-width character would satisfy "non-empty text" while rendering as nothing, which is exactly the
        // silent degradation the Owner decision forbids.
        WatermarkPolicyRequest request;
        request.enabled = true;
        request.text = QString(QChar(0x200B)) + QStringLiteral("invisible");
        const auto result = resolveWatermarkPolicy(request);
        check(!result.ok() && result.error.contains(QStringLiteral("zero-width")),
              QStringLiteral("zero-width characters are refused instead of rendering nothing"));
    }
    {
        // A bidi override would let the visible order differ from the configured text.
        WatermarkPolicyRequest request;
        request.enabled = true;
        request.text = QChar(0x202E) + QStringLiteral("reordered");
        const auto result = resolveWatermarkPolicy(request);
        check(!result.ok() && result.error.contains(QStringLiteral("bidi")),
              QStringLiteral("bidi override characters are refused"));
    }
    {
        WatermarkPolicyRequest request;
        request.enabled = true;
        request.text = QStringLiteral("C1 control") + QChar(0x0085);
        check(!resolveWatermarkPolicy(request).ok(),
              QStringLiteral("C1 control characters are refused"));
    }
    {
        // Characters outside the BMP are surrogate pairs in UTF-16. They are one visible code point, so a
        // code-point-aware check must accept them instead of rejecting legitimate text.
        WatermarkPolicyRequest request;
        request.enabled = true;
        request.text = QString::fromUtf8("\xF0\x9F\x94\x92 \xE4\xBF\x9D\xE5\xAF\x86 \xF0\xA0\x80\x80");
        const auto result = resolveWatermarkPolicy(request);
        check(result.ok(), QStringLiteral("emoji and non-BMP ideographs are accepted"));
    }
}

void checkExplicitValues()
{
    WatermarkPolicyRequest request;
    request.enabled = true;
    request.text = QStringLiteral("内部资料");
    request.position = WatermarkPosition::Tiled;
    request.opacity = 0.8;
    request.fontPointSize = 24;
    request.color = QStringLiteral("#00ff7f");

    const auto result = resolveWatermarkPolicy(request);
    check(result.ok(), QStringLiteral("an explicit bounded request resolves"));
    check(result.policy.position == WatermarkPosition::Tiled, QStringLiteral("tiling is explicit, not default"));
    check(result.policy.opacity == 0.8, QStringLiteral("explicit opacity wins over the default"));
    check(result.policy.fontPointSize == 24, QStringLiteral("explicit font size wins over the default"));
    check(result.policy.colorRgb == 0x00FF7FU, QStringLiteral("lower-case hex colour is accepted"));
}

void checkRejectedValues()
{
    {
        WatermarkPolicyRequest request;
        request.opacity = 0.0;
        const auto result = resolveWatermarkPolicy(request);
        check(!result.ok() && result.error.contains(QStringLiteral("opacity")),
              QStringLiteral("an invisible opacity is refused instead of clamped"));
    }
    {
        WatermarkPolicyRequest request;
        request.opacity = 1.5;
        check(!resolveWatermarkPolicy(request).ok(), QStringLiteral("opacity above 1 is refused"));
    }
    {
        WatermarkPolicyRequest request;
        request.opacity = 1.0;
        check(resolveWatermarkPolicy(request).ok(), QStringLiteral("opacity 1.0 is the accepted upper bound"));
    }
    {
        WatermarkPolicyRequest request;
        request.fontPointSize = 7;
        check(!resolveWatermarkPolicy(request).ok(), QStringLiteral("font size below 8 points is refused"));
    }
    {
        WatermarkPolicyRequest request;
        request.fontPointSize = 97;
        check(!resolveWatermarkPolicy(request).ok(), QStringLiteral("font size above 96 points is refused"));
    }
    {
        WatermarkPolicyRequest request;
        request.color = QStringLiteral("green");
        const auto result = resolveWatermarkPolicy(request);
        check(!result.ok() && result.error.contains(QStringLiteral("RRGGBB")),
              QStringLiteral("a colour that is not #RRGGBB is refused with the expected form"));
        check(!result.error.contains(QStringLiteral("green")),
              QStringLiteral("the rejected value is not echoed into the diagnostic"));
    }
    {
        WatermarkPolicyRequest request;
        request.color = QStringLiteral("#12345");
        check(!resolveWatermarkPolicy(request).ok(), QStringLiteral("a truncated hex colour is refused"));
    }
    {
        WatermarkPolicyRequest request;
        request.color = QStringLiteral("  #00ff00  ");
        const auto result = resolveWatermarkPolicy(request);
        check(result.ok() && result.policy.colorRgb == 0x00FF00U,
              QStringLiteral("surrounding whitespace around a colour is tolerated like other text input"));
    }
}

void checkPositionTokenValidation()
{
    WatermarkPolicyRequest request;
    request.enabled = true;
    request.text = QStringLiteral("TOKEN");
    request.positionToken = QStringLiteral("centre");
    const auto rejected = resolveWatermarkPolicy(request);
    check(!rejected.ok(), QStringLiteral("an unknown position token fails the policy"));
    check(rejected.error.contains(QStringLiteral("bottom-right")),
          QStringLiteral("the refusal names the accepted vocabulary"));
    check(!rejected.error.contains(QStringLiteral("centre")),
          QStringLiteral("the rejected token is not echoed"));
    check(rejected.policy.enabled, QStringLiteral("the operator intent stays visible"));

    request.positionToken = QStringLiteral("top-left");
    const auto accepted = resolveWatermarkPolicy(request);
    check(accepted.ok() && accepted.policy.position == WatermarkPosition::TopLeft,
          QStringLiteral("a valid token is applied by the policy"));
}

void checkPositionTokens()
{
    struct Case
    {
        const char *token;
        WatermarkPosition position;
    };
    const Case cases[] = {
        {"bottom-right", WatermarkPosition::BottomRight},
        {"bottom-left", WatermarkPosition::BottomLeft},
        {"top-right", WatermarkPosition::TopRight},
        {"top-left", WatermarkPosition::TopLeft},
        {"tiled", WatermarkPosition::Tiled},
    };

    for (const Case &entry : cases) {
        WatermarkPosition parsed = WatermarkPosition::Tiled;
        check(parseWatermarkPosition(QString::fromLatin1(entry.token), parsed) && parsed == entry.position,
              QStringLiteral("position token '%1' parses").arg(QString::fromLatin1(entry.token)));
        check(watermarkPositionName(entry.position) == QString::fromLatin1(entry.token),
              QStringLiteral("position token '%1' round-trips").arg(QString::fromLatin1(entry.token)));
    }

    WatermarkPosition parsed = WatermarkPosition::BottomRight;
    check(!parseWatermarkPosition(QStringLiteral("centre"), parsed),
          QStringLiteral("an unknown position token is refused"));
    check(watermarkPositionName(WatermarkPosition::TopLeft) == QStringLiteral("top-left"),
          QStringLiteral("position names are stable launch-option tokens"));
}

void checkFontFallback()
{
    const QStringList families = watermarkFontFamilyCandidates();
    check(!families.isEmpty(), QStringLiteral("a font family chain is published"));
    check(families.first() == QStringLiteral("Microsoft YaHei UI"),
          QStringLiteral("the Windows CJK-capable family is preferred first"));
    check(families.contains(QStringLiteral("Segoe UI")), QStringLiteral("a Latin fallback is present"));
    check(families.contains(QStringLiteral("Noto Sans CJK SC")),
          QStringLiteral("a cross-platform CJK fallback is present"));
    check(families.last() == QStringLiteral("sans-serif"),
          QStringLiteral("a generic family terminates the chain"));
}

void checkDiagnosticsAndStartGate(QCoreApplication &application)
{
    Q_UNUSED(application);

    {
        HyRemote::Runtime::AccessInstance instance;
        const QString report = HyRemote::Runtime::formatDiagnosticReport(instance.diagnosticSnapshot());
        check(report.contains(QStringLiteral("WATERMARK_ENABLED=false\n")),
              QStringLiteral("a default instance reports the watermark as disabled"));
        check(report.contains(QStringLiteral("WATERMARK_POSITION=bottom-right\n")),
              QStringLiteral("the report names the shared default position"));
        check(report.contains(QStringLiteral("WATERMARK_OPACITY=0.35\n")),
              QStringLiteral("the report names the shared default opacity"));
        check(report.contains(QStringLiteral("WATERMARK_FONT_SIZE=16\n")),
              QStringLiteral("the report names the shared default font size"));
        check(report.contains(QStringLiteral("WATERMARK_COLOR=#FFFFFF\n")),
              QStringLiteral("the report names the shared default colour"));
        check(report.contains(QStringLiteral("WATERMARK_POLICY_ERROR=none\n")),
              QStringLiteral("a default policy has no error"));
        check(report.contains(QStringLiteral("WATERMARK_TEXT_CODE_UNITS=0\n")),
              QStringLiteral("a default policy reports no text"));
    }

    {
        // The operator text must never appear in a report, and the intent must stay visible either way.
        const QString text = QStringLiteral("机密 内部 CONFIDENTIAL");
        WatermarkPolicyRequest request;
        request.enabled = true;
        request.text = text;

        HyRemote::Runtime::AccessInstance instance;
        check(instance.setWatermarkPolicy(request), QStringLiteral("the policy is accepted while stopped"));

        const QString report = HyRemote::Runtime::formatDiagnosticReport(instance.diagnosticSnapshot());
        check(report.contains(QStringLiteral("WATERMARK_ENABLED=true\n")),
              QStringLiteral("the report keeps the operator intent visible"));
        check(report.contains(QStringLiteral("WATERMARK_POLICY_ERROR=none\n")),
              QStringLiteral("a valid policy reports no policy error"));
        check(!report.contains(text), QStringLiteral("the operator text never appears in the report"));
        check(report.contains(QStringLiteral("WATERMARK_TEXT_CODE_UNITS=%1\n").arg(text.size())),
              QStringLiteral("only the text length is reported"));

#if HYREMOTE_HAS_WATERMARK_RENDERING
        // W2: this build contains the compositor, so the capability gate accepts an enabled policy and the only
        // remaining refusal here is the missing Qt target - never a watermark-free stream.
        check(HyRemote::Runtime::AccessInstance::watermarkRenderingAvailable(),
              QStringLiteral("this build contains the server-side compositor"));
        check(!instance.start(), QStringLiteral("a target-less start still fails"));
        check(instance.lastError().has_value()
                  && instance.lastError()->code == HyRemote::Runtime::ErrorCode::InvalidConfiguration,
              QStringLiteral("the failure is the missing target, not the watermark"));
        check(instance.state() == HyRemote::Runtime::AccessState::Stopped,
              QStringLiteral("the host runtime returns to Stopped, it is not terminated"));
#else
        // A build without the compositor must refuse the remote output rather than stream an unwatermarked picture.
        check(!HyRemote::Runtime::AccessInstance::watermarkRenderingAvailable(),
              QStringLiteral("this build has no compositor, and says so"));
        check(!instance.start(), QStringLiteral("an enabled watermark without a compositor refuses to start"));
        const auto error = instance.lastError();
        check(error.has_value() && error->code == HyRemote::Runtime::ErrorCode::StartFailed,
              QStringLiteral("the missing capability is a StartFailed, not an invalid configuration"));
        check(error.has_value() && error->message.contains(QStringLiteral("watermark")),
              QStringLiteral("the refusal names the watermark"));
        check(instance.state() == HyRemote::Runtime::AccessState::Stopped,
              QStringLiteral("the host runtime returns to Stopped, it is not terminated"));

        // A refused start must not leave a half-composed runtime behind: the configuration stays editable, no
        // effective listener facts appear, and stop() stays the idempotent quiescence boundary.
        check(instance.setWatermarkPolicy(WatermarkPolicyRequest{}),
              QStringLiteral("the refused start leaves the configuration editable"));
        check(!instance.diagnosticSnapshot().effectivePort.has_value(),
              QStringLiteral("a refused start publishes no effective port"));
        instance.stop();
        check(instance.state() == HyRemote::Runtime::AccessState::Stopped,
              QStringLiteral("stop() after a refused start still ends in Stopped"));
        check(instance.lastError().has_value(),
              QStringLiteral("the retained diagnostic survives the quiescence stop"));
#endif
    }

    {
        // An invalid enabled policy is refused for its own reason, and the report explains it.
        WatermarkPolicyRequest request;
        request.enabled = true;
        request.text = QStringLiteral("valid text");
        request.opacity = 4.0;

        HyRemote::Runtime::AccessInstance instance;
        check(instance.setWatermarkPolicy(request), QStringLiteral("an invalid policy is stored, not thrown"));
        check(!instance.watermarkPolicy().ok(), QStringLiteral("the stored policy keeps its error"));
        check(!instance.start(), QStringLiteral("an invalid enabled policy refuses to start"));
        const auto error = instance.lastError();
        check(error.has_value() && error->message.contains(QStringLiteral("opacity")),
              QStringLiteral("the refusal names the offending value"));

        const QString report = HyRemote::Runtime::formatDiagnosticReport(instance.diagnosticSnapshot());
        check(report.contains(QStringLiteral("WATERMARK_POLICY_ERROR=the watermark opacity")),
              QStringLiteral("the report explains the invalid policy"));
    }
}

}  // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);

    checkSharedDefaults();
    checkEnabledTextAndDefaults();
    checkRejectedText();
    checkExplicitValues();
    checkRejectedValues();
    checkPositionTokens();
    checkPositionTokenValidation();
    checkFontFallback();
    checkDiagnosticsAndStartGate(application);

    QTextStream out(stdout);
    out << (failures == 0 ? "watermark policy checks passed\n" : "watermark policy checks failed\n");
    return failures == 0 ? 0 : 2;
}
