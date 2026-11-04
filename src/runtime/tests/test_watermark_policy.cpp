// #589 W1: the static text watermark policy is one shared Runtime contract. These cases pin the Owner decision
// (disabled by default, bottom-right single line, 0.35 opacity, tiling only on request), the bounded validation
// of every operator-provided value, and the rule that an explicitly enabled watermark whose policy cannot be
// honoured refuses the remote output instead of quietly streaming an unwatermarked picture.

#include "../src/access_instance.hpp"
#include "../src/detail/watermark_policy.hpp"
#include "../src/detail/watermark_store.hpp"

#if defined(HYREMOTE_TEST_CPP_SURFACE_AVAILABLE)
#include <HyRemote/RemoteAccess.h>
#endif

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QTemporaryDir>
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

#if defined(HYREMOTE_TEST_CPP_SURFACE_AVAILABLE)
// The product C++ surface cannot include this internal header, so `RemoteWatermark` carries its own literal field
// initialisers. That is a drift point: a route that keeps a stale default would satisfy "the operator asked for a
// watermark" while quietly disagreeing with the shared policy, which is exactly what #589 W1 forbids. This case is the
// guard that ties the installed surface back to the one shared policy.
void checkProductSurfaceDefaults()
{
    const HyRemote::RemoteWatermark defaults;
    check(defaults.text.isEmpty(), QStringLiteral("the C++ surface default is 'no watermark'"));
    check(qFuzzyCompare(defaults.opacity, HyRemote::detail::kWatermarkDefaultOpacity),
          QStringLiteral("the C++ surface opacity default equals the shared default"));
    check(defaults.fontPointSize == HyRemote::detail::kWatermarkDefaultFontPointSize,
          QStringLiteral("the C++ surface font size default equals the shared default"));
    check(defaults.colorRgb.compare(QStringLiteral("#FFFFFF"), Qt::CaseInsensitive) == 0,
          QStringLiteral("the C++ surface colour default is the shared default"));
    check(defaults.position == HyRemote::RemoteWatermarkPosition::BottomRight,
          QStringLiteral("the C++ surface position default is bottom-right, not tiled"));
}
#endif  // HYREMOTE_TEST_CPP_SURFACE_AVAILABLE

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

// #591: an explicitly configured policy has to survive a restart. The state directory is the contract's place for
// mutable state, so this is asserted through a second AccessInstance that never selects anything itself - exactly the
// "process restarted" situation - instead of trusting that a write happened.
void checkPersistedPolicySurvivesRestart()
{
    QTemporaryDir state;
    check(state.isValid(), QStringLiteral("a temporary state directory exists"));
    if (!state.isValid())
        return;

    const QByteArray previousState = qgetenv("HYREMOTE_STATE_DIR");
    qputenv("HYREMOTE_STATE_DIR", state.path().toUtf8());
    const QString storedPath = QDir(state.path()).filePath(QStringLiteral("watermark.policy"));

    // Built from code points on purpose: this case must not depend on how a compiler decodes the source file. The Qt 5
    // lane compiles without /utf-8, so a literal CJK string in the source became cp1252 mojibake there, which the policy
    // then rejected as control characters - a test-encoding artefact that looked like a persistence defect.
    QString cjkText;
    cjkText += QChar(static_cast<ushort>(0x6301));
    cjkText += QChar(static_cast<ushort>(0x4E45));
    cjkText += QLatin1Char(' ');
    cjkText += QStringLiteral("CONFIDENTIAL");

    {
        WatermarkPolicyRequest request;
        request.enabled = true;
        request.text = cjkText;
        request.position = WatermarkPosition::Tiled;
        request.opacity = 0.5;
        request.fontPointSize = 24;
        request.color = QStringLiteral("#00FF00");

        HyRemote::Runtime::AccessInstance writer;
        check(writer.setWatermarkPolicy(request), QStringLiteral("an explicit policy is accepted"));
        // Persistence happens at start(): only then is the configuration selection final, so a descriptor path chosen
        // after the policy cannot send the file to the wrong directory.
        check(!writer.start(), QStringLiteral("a target-less start still fails"));
        check(QFile::exists(storedPath),
              QStringLiteral("the explicit policy is persisted into the state directory"));
    }

    {
        // A fresh instance is the restart: it selects nothing of its own.
        HyRemote::Runtime::AccessInstance restarted;
        check(!restarted.start(), QStringLiteral("a target-less start still fails"));
        const auto restored = restarted.watermarkPolicy().policy;
        check(restored.enabled && restored.text == cjkText,
              QStringLiteral("the persisted policy is restored on the next start"));
        check(restored.position == WatermarkPosition::Tiled && qFuzzyCompare(restored.opacity, 0.5)
                  && restored.fontPointSize == 24,
              QStringLiteral("the restored policy keeps its position, opacity and size"));
        const QString report
            = HyRemote::Runtime::formatDiagnosticReport(restarted.diagnosticSnapshot());
        if (!restored.enabled) {
            // A failing restore must say why, otherwise it is indistinguishable from "the store is empty". The public
            // report already carries the resolved host paths and the policy state, so no internal symbol is needed.
            QFile stored(storedPath);
            QByteArray storedBytes;
            if (stored.open(QIODevice::ReadOnly))
                storedBytes = stored.readAll();
            QString codePoints;
            const QString storedText = QString::fromUtf8(storedBytes);
            for (const QString &line : storedText.split(QLatin1Char('\n'))) {
                if (!line.startsWith(QStringLiteral("text=")))
                    continue;
                const QString value = line.mid(5);
                for (const uint codePoint : value.toUcs4())
                    codePoints += QStringLiteral("U+%1 ").arg(codePoint, 4, 16, QLatin1Char('0'));
            }
            QTextStream(stdout) << "  [diag] size=" << storedBytes.size() << " textCps=" << codePoints << '\n'
                                << "  [diag] " << report << '\n';
        }
        check(report.contains(QStringLiteral("WATERMARK_ENABLED=true")),
              QStringLiteral("the restored policy is visible as enabled in the report"));
        check(!report.contains(cjkText),
              QStringLiteral("the restored policy still keeps the operator text out of the report"));
    }

    {
        // An application's own selection always wins over the stored one.
        WatermarkPolicyRequest request;
        request.enabled = true;
        request.text = QStringLiteral("EXPLICIT");
        HyRemote::Runtime::AccessInstance override;
        check(override.setWatermarkPolicy(request), QStringLiteral("an explicit selection is accepted"));
        check(override.watermarkPolicy().policy.text == QStringLiteral("EXPLICIT"),
              QStringLiteral("the explicit selection wins over the stored policy"));
    }

    {
        // A request the resolver refuses must never reach the store: a half-resolved policy (text accepted, colour
        // quietly reset to white) would come back on the next launch as a watermark the operator never asked for.
        WatermarkPolicyRequest good;
        good.enabled = true;
        good.text = QStringLiteral("KEEP-ME");
        HyRemote::Runtime::AccessInstance seeded;
        check(seeded.setWatermarkPolicy(good), QStringLiteral("a valid policy is accepted for the seed"));
        check(!seeded.start(), QStringLiteral("the seed start still fails for the missing target"));

        WatermarkPolicyRequest invalid = good;
        invalid.text = QStringLiteral("OVERWRITE-ATTEMPT");
        invalid.opacity = 4.0;
        HyRemote::Runtime::AccessInstance rejected;
        check(rejected.setWatermarkPolicy(invalid), QStringLiteral("the invalid request is recorded for reporting"));
        check(!rejected.start(), QStringLiteral("start refuses an enabled policy that cannot be honoured"));

        QFile stored(storedPath);
        const bool readable = stored.open(QIODevice::ReadOnly);
        check(readable, QStringLiteral("the stored policy is still readable"));
        const QString storedText = readable ? QString::fromUtf8(stored.readAll()) : QString{};
        check(storedText.contains(QStringLiteral("KEEP-ME")),
              QStringLiteral("an invalid request leaves the stored policy untouched"));
        check(!storedText.contains(QStringLiteral("OVERWRITE-ATTEMPT")),
              QStringLiteral("an invalid request is not written to the state directory"));
    }

    {
        // Disabling is an explicit selection too: it must clear the store, or the mark would come back on restart.
        HyRemote::Runtime::AccessInstance clearer;
        check(clearer.setWatermarkPolicy(WatermarkPolicyRequest{}), QStringLiteral("disabling is accepted"));
        check(!clearer.start(), QStringLiteral("a target-less start still fails"));
        check(!QFile::exists(storedPath), QStringLiteral("disabling removes the stored policy"));
    }

    {
        // A stored file that cannot be trusted is reported, never silently treated as "no watermark".
        QFile file(storedPath);
        check(file.open(QIODevice::WriteOnly), QStringLiteral("the stored policy can be overwritten for the test"));
        file.write("version=9\ntext=OUTSIDE-THE-BOUNDS\n");
        file.close();

        HyRemote::Runtime::AccessInstance unusable;
        check(!unusable.start(), QStringLiteral("corrupted persisted watermark must refuse remote start"));
        // #812: this regression MUST identify the real restore failure. The previous test used a missing
        // Qt target and accepted an unrelated start() failure even when corrupt state would stream unmarked.
        const auto restoreFailure = unusable.lastError();
        check(restoreFailure.has_value()
                  && restoreFailure->code == HyRemote::Runtime::ErrorCode::InvalidConfiguration
                  && restoreFailure->message.contains(QStringLiteral("stored watermark policy was not applied"))
                  && restoreFailure->message.contains(QStringLiteral("remote output is refused")),
              QStringLiteral("corrupt restored watermark refuses before target validation"));
        check(!unusable.diagnosticSnapshot().effectivePort.has_value(),
              QStringLiteral("a corrupt restored mark cannot open a remote listener"));
        // An unusable stored policy must not be restored: keep disabled, but reject with a durable
        // error instead of interpreting disabled as permission to stream an unwatermarked frame.
        check(!unusable.watermarkPolicy().policy.enabled && !unusable.watermarkPolicy().ok(),
              QStringLiteral("a corrupt stored mark is disabled but retains its fatal restore error"));

        // An explicit operator OFF is different from an implicit failed restore: it must clear
        // the corrupt store and never be rejected as an uncleared previous watermark error.
        HyRemote::Runtime::AccessInstance explicitClear;
        check(explicitClear.setWatermarkPolicy(WatermarkPolicyRequest{}),
              QStringLiteral("explicit OFF may override the corrupt persisted mark"));
        check(!explicitClear.start(), QStringLiteral("explicit OFF without a target still cannot start"));
        check(!QFile::exists(storedPath),
              QStringLiteral("explicit OFF clears the corrupted stored policy"));
        const auto clearedFailure = explicitClear.lastError();
        check(!clearedFailure.has_value()
                  || !clearedFailure->message.contains(QStringLiteral("stored watermark policy was not applied")),
              QStringLiteral("explicit OFF does not inherit a prior restore failure"));
    }

    // An oversized state file must never be materialized via an unbounded read, and its text
    // must not enter an ordinary diagnostic. The test intentionally exceeds the format cap.
    {
        QFile oversized(storedPath);
        check(oversized.open(QIODevice::WriteOnly | QIODevice::Truncate),
              QStringLiteral("an oversized stored policy can be created for the negative test"));
        if (oversized.isOpen()) {
            oversized.write(QByteArray(8192, 'x'));
            oversized.close();
            // The store and path resolvers are private to the Runtime DLL and intentionally
            // not exported as a test/SDK ABI. Exercise the observable Runtime contract instead.
            HyRemote::Runtime::AccessInstance oversizedReader;
            const auto policy = oversizedReader.watermarkPolicy();
            check(!policy.policy.enabled
                      && policy.error.contains(QStringLiteral("exceeds the permitted size")),
                  QStringLiteral("oversized persisted policy is rejected before an unbounded read"));
            check(!policy.error.contains(QStringLiteral("xxxx")),
                  QStringLiteral("oversized policy content is not echoed into diagnostics"));
            check(!oversizedReader.start(),
                  QStringLiteral("an oversized stored watermark prevents remote startup"));
            const auto oversizedError = oversizedReader.lastError();
            check(oversizedError.has_value()
                      && oversizedError->code == HyRemote::Runtime::ErrorCode::InvalidConfiguration
                      && oversizedError->message.contains(QStringLiteral("exceeds the permitted size"))
                      && oversizedError->message.contains(QStringLiteral("remote output is refused")),
                  QStringLiteral("oversized stored watermark fails closed before target validation"));
            check(!oversizedReader.diagnosticSnapshot().effectivePort.has_value(),
                  QStringLiteral("an oversized restored mark leaves no remote listener"));
        }
    }

    // A selected state path that is actually a *file* is unusable. The user must see an
    // explicit persistence failure instead of a false success followed by a missing watermark
    // on restart. This deterministic fixture works on Windows without manipulating ACLs.
    {
        const QString obstructionPath = QDir(state.path()).filePath(QStringLiteral("state-obstruction"));
        QFile obstruction(obstructionPath);
        check(obstruction.open(QIODevice::WriteOnly),
              QStringLiteral("an obstructing regular file is available for the state path probe"));
        obstruction.close();
        qputenv("HYREMOTE_STATE_DIR", obstructionPath.toUtf8());

        WatermarkPolicyRequest request;
        request.enabled = true;
        request.text = QStringLiteral("DO-NOT-ECHO-THIS-MARK");
        HyRemote::Runtime::AccessInstance unsaved;
        check(unsaved.setWatermarkPolicy(request), QStringLiteral("an explicit valid policy is selected"));
        check(!unsaved.start(), QStringLiteral("a missing writable state directory refuses remote startup"));
        const auto error = unsaved.lastError();
        check(error.has_value() && error->message.contains(QStringLiteral("could not persist")),
              QStringLiteral("start reports why the explicitly requested watermark could not be saved"));
        check(!error.has_value() || !error->message.contains(request.text),
              QStringLiteral("persistence diagnostics never log the operator watermark text"));

        HyRemote::Runtime::AccessInstance uncleared;
        check(uncleared.setWatermarkPolicy(WatermarkPolicyRequest{}),
              QStringLiteral("explicit disable is accepted even with an unusable state directory"));
        check(!uncleared.start(), QStringLiteral("failed persistence of explicit disable is not silently successful"));

        qputenv("HYREMOTE_STATE_DIR", state.path().toUtf8());
    }

    if (previousState.isEmpty())
        qunsetenv("HYREMOTE_STATE_DIR");
    else
        qputenv("HYREMOTE_STATE_DIR", previousState);
}


void checkRestoredPolicyFollowsDescriptorStatePath()
{
    // #793: with no HYREMOTE_STATE_DIR override, the security descriptor anchors a different
    // state directory. A pre-start getter must not permanently pin the platform-default policy.
    QTemporaryDir fixture;
    check(fixture.isValid(), QStringLiteral("the descriptor-cache fixture has an isolated root"));
    if (!fixture.isValid())
        return;

    const bool hadState = qEnvironmentVariableIsSet("HYREMOTE_STATE_DIR");
    const QByteArray oldState = qgetenv("HYREMOTE_STATE_DIR");
    const bool hadConfig = qEnvironmentVariableIsSet("HYREMOTE_CONFIG_FILE");
    const QByteArray oldConfig = qgetenv("HYREMOTE_CONFIG_FILE");
    qunsetenv("HYREMOTE_STATE_DIR");
    qunsetenv("HYREMOTE_CONFIG_FILE");

    const QDir root(fixture.path());
    const QString firstDir = root.filePath(QStringLiteral("first"));
    const QString secondDir = root.filePath(QStringLiteral("second"));
    const QString emptyDir = root.filePath(QStringLiteral("no-policy"));
    const QString firstConfig = QDir(firstDir).filePath(QStringLiteral("security.conf"));
    const QString secondConfig = QDir(secondDir).filePath(QStringLiteral("security.conf"));
    const QString emptyConfig = QDir(emptyDir).filePath(QStringLiteral("security.conf"));

    for (const QString &directory : {firstDir, secondDir, emptyDir}) {
        check(QDir().mkpath(QDir(directory).filePath(QStringLiteral("state"))),
              QStringLiteral("a descriptor-relative state directory exists"));
        QFile file(QDir(directory).filePath(QStringLiteral("security.conf")));
        const bool writable = file.open(QIODevice::WriteOnly);
        check(writable, QStringLiteral("a readable security descriptor fixture exists"));
        if (writable) {
            file.write("# isolated path-selection fixture\n");
            file.close();
        }
    }

    auto storeFile = [](const QString &configFile) {
        return QDir(QFileInfo(configFile).absolutePath())
            .filePath(QStringLiteral("state/watermark.policy"));
    };
    auto seed = [&storeFile](const QString &configFile, const QString &text) {
        // Use the exported AccessInstance to write the real policy at start(). Private store/path
        // helpers have hidden visibility in the shared Runtime and cannot be linked from this test.
        HyRemote::Runtime::AccessInstance writer;
        WatermarkPolicyRequest request;
        request.enabled = true;
        request.text = text;
        check(writer.setSecurityConfigFile(configFile),
              QStringLiteral("the Runtime accepts the descriptor selector"));
        check(writer.setWatermarkPolicy(request),
              QStringLiteral("the Runtime accepts the explicitly enabled seed watermark"));
        check(!writer.start(), QStringLiteral("the target-less seed cannot establish a session"));
        QFile file(storeFile(configFile));
        const bool readable = file.open(QIODevice::ReadOnly);
        check(readable, QStringLiteral("the descriptor-relative state contains a persisted policy"));
        check(readable && QString::fromUtf8(file.readAll()).contains(text),
              QStringLiteral("the persisted policy belongs to the selected descriptor directory"));
    };
    seed(firstConfig, QStringLiteral("FIRST-STORE"));
    seed(secondConfig, QStringLiteral("SECOND-STORE"));

    {
        HyRemote::Runtime::AccessInstance access;
        (void)access.watermarkPolicy(); // default-store peek used to poison the one-shot restored flag
        check(access.setSecurityConfigFile(firstConfig),
              QStringLiteral("first descriptor can be selected after a getter"));
        check(access.watermarkPolicy().policy.text == QStringLiteral("FIRST-STORE"),
              QStringLiteral("the getter reloads the first descriptor-relative store"));
        check(access.setSecurityConfigFile(secondConfig),
              QStringLiteral("second descriptor can replace the first while stopped"));
        check(access.watermarkPolicy().policy.text == QStringLiteral("SECOND-STORE"),
              QStringLiteral("the getter no longer reports a stale first-store mark"));
        check(!access.start(), QStringLiteral("a target-less test Runtime still cannot start"));
        check(access.watermarkPolicy().policy.text == QStringLiteral("SECOND-STORE"),
              QStringLiteral("start retains the correctly selected second-store mark"));
        access.stop();

        check(access.setSecurityConfigFile(firstConfig),
              QStringLiteral("a stopped Runtime can select the first descriptor again"));
        check(access.watermarkPolicy().policy.text == QStringLiteral("FIRST-STORE"),
              QStringLiteral("a stopped Runtime reloads the first store, not the previous run"));
        check(access.setSecurityConfigFile(emptyConfig),
              QStringLiteral("a stopped Runtime can select a descriptor without a saved mark"));
        check(!access.watermarkPolicy().policy.enabled,
              QStringLiteral("a missing policy resets the previous store's mark to disabled"));
    }

    {
        HyRemote::Runtime::AccessInstance access;
        check(access.setSecurityConfigFile(firstConfig),
              QStringLiteral("start-only test chooses the first descriptor"));
        check(!access.start(), QStringLiteral("target-less initial start returns false"));
        check(access.watermarkPolicy().policy.text == QStringLiteral("FIRST-STORE"),
              QStringLiteral("start-only path restores first-store policy"));
        access.stop();
        check(access.setSecurityConfigFile(secondConfig),
              QStringLiteral("the next run can choose the other descriptor"));
        check(!access.start(), QStringLiteral("target-less second start returns false"));
        check(access.watermarkPolicy().policy.text == QStringLiteral("SECOND-STORE"),
              QStringLiteral("a restart without an intervening getter restores the new state store"));
        access.stop();
    }

    {
        HyRemote::Runtime::AccessInstance access;
        check(access.setSecurityConfigFile(firstConfig),
              QStringLiteral("explicit override test chooses the first descriptor"));
        (void)access.watermarkPolicy();
        WatermarkPolicyRequest operatorChoice;
        operatorChoice.enabled = true;
        operatorChoice.text = QStringLiteral("EXPLICIT-OPERATOR");
        check(access.setWatermarkPolicy(operatorChoice),
              QStringLiteral("the operator can explicitly select a different policy"));
        check(access.setSecurityConfigFile(secondConfig),
              QStringLiteral("the descriptor is still changeable after an explicit choice"));
        check(access.watermarkPolicy().policy.text == operatorChoice.text,
              QStringLiteral("an explicit selection outranks any other descriptor's stored mark"));
        check(!access.start(), QStringLiteral("a target-less explicit test still cannot start"));
        check(access.watermarkPolicy().policy.text == operatorChoice.text,
              QStringLiteral("start cannot erase the explicit selection"));
        QFile storedFile(storeFile(secondConfig));
        const bool readable = storedFile.open(QIODevice::ReadOnly);
        check(readable && QString::fromUtf8(storedFile.readAll()).contains(operatorChoice.text),
              QStringLiteral("the explicit operator mark is persisted in the newly selected state store"));
        QFile previousFile(storeFile(firstConfig));
        const bool priorReadable = previousFile.open(QIODevice::ReadOnly);
        check(priorReadable && QString::fromUtf8(previousFile.readAll()).contains(QStringLiteral("FIRST-STORE")),
              QStringLiteral("the newly selected explicit policy does not alter the prior state store"));
    }

    if (hadState)
        qputenv("HYREMOTE_STATE_DIR", oldState);
    else
        qunsetenv("HYREMOTE_STATE_DIR");
    if (hadConfig)
        qputenv("HYREMOTE_CONFIG_FILE", oldConfig);
    else
        qunsetenv("HYREMOTE_CONFIG_FILE");
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);

    checkSharedDefaults();
#if defined(HYREMOTE_TEST_CPP_SURFACE_AVAILABLE)
    checkProductSurfaceDefaults();
#endif
    checkEnabledTextAndDefaults();
    checkRejectedText();
    checkExplicitValues();
    checkRejectedValues();
    checkPositionTokens();
    checkPositionTokenValidation();
    checkFontFallback();
    checkDiagnosticsAndStartGate(application);
    checkPersistedPolicySurvivesRestart();
    checkRestoredPolicyFollowsDescriptorStatePath();

    QTextStream out(stdout);
    out << (failures == 0 ? "watermark policy checks passed\n" : "watermark policy checks failed\n");
    return failures == 0 ? 0 : 2;
}
