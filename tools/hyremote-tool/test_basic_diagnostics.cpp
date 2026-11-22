#include "basic_diagnostics.hpp"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

#include <iostream>
#include <limits>

namespace {
int failures = 0;
#define CHECK(expr) do { if (!(expr)) { \
    std::cerr << "TOOL_BASIC_DIAG_FAIL line " << __LINE__ << ": " #expr << '\n'; \
    ++failures; } } while (false)

QJsonObject parseReport(const QByteArray &bytes)
{
    const auto document = QJsonDocument::fromJson(bytes);
    CHECK(document.isObject());
    return document.object();
}

void testDefaultRedactionAndEvidenceBoundary()
{
    ToolBasicDiagnostics report;
    ToolDiagSnapshot snapshot;
    snapshot.host = QStringLiteral("192.168.100.21");
    snapshot.port = 5921;
    snapshot.stage = ToolDiagStage::Connecting;
    snapshot.lastFailure = ToolDiagFailure::Timeout;
    snapshot.localFps = 12.5;
    snapshot.localMbitPerSecond = 2.25;
    snapshot.receivedFrames = 123;
    snapshot.receivedBytes = 12345;
    snapshot.localFrameWidth = 1920;
    snapshot.localFrameHeight = 1080;
    snapshot.observedUtcMs = 1790000000000LL;
    report.observeSnapshot(snapshot);
    const auto encoded = report.reportJson();
    CHECK(encoded.size() <= ToolBasicDiagnostics::kMaxReportBytes);
    CHECK(!encoded.contains("192.168.100.21"));
    CHECK(!encoded.contains("password"));
    CHECK(!encoded.contains("Bearer"));
    const auto json = parseReport(encoded);
    CHECK(json.value(QStringLiteral("schemaVersion")).toInt() == 1);
    CHECK(json.value(QStringLiteral("reportType")).toString()
          == QStringLiteral("hyremote-tool-client-basic"));
    CHECK(json.value(QStringLiteral("provenance")).toString()
          == QStringLiteral("local-client-only"));
    CHECK(json.value(QStringLiteral("endpointRedacted")).toBool());
    CHECK(json.value(QStringLiteral("endpoint")).toString()
          == QStringLiteral("[redacted]"));
    CHECK(json.value(QStringLiteral("state")).toString()
          == QStringLiteral("connecting"));
    CHECK(json.value(QStringLiteral("lastFailure")).toString()
          == QStringLiteral("timeout"));
    CHECK(json.value(QStringLiteral("nextStep")).toString()
          == QStringLiteral("verify-network-path-firewall"));
    CHECK(json.value(QStringLiteral("wireSecurity")).toString()
          == QStringLiteral("not-observed"));
    CHECK(json.value(QStringLiteral("remoteDeviceIdentity")).toString()
          == QStringLiteral("not-available"));
    CHECK(json.value(QStringLiteral("remoteCpu")).toString()
          == QStringLiteral("not-available"));
    CHECK(json.value(QStringLiteral("remoteRuntimeVersion")).toString()
          == QStringLiteral("not-available"));
    const auto metrics = json.value(QStringLiteral("clientMeasurements")).toObject();
    CHECK(metrics.value(QStringLiteral("fps")).toDouble() == 12.5);
    CHECK(metrics.value(QStringLiteral("mbitPerSecond")).toDouble() == 2.25);
    CHECK(metrics.value(QStringLiteral("frames")).toString()
          == QStringLiteral("123"));
    CHECK(metrics.value(QStringLiteral("receivedBytes")).toString()
          == QStringLiteral("12345"));
    CHECK(metrics.value(QStringLiteral("frameWidth")).toInt() == 1920);
    CHECK(metrics.value(QStringLiteral("frameHeight")).toInt() == 1080);
}

void testExplicitEndpointAndInvalidUntrustedInput()
{
    ToolBasicDiagnostics report;
    ToolDiagSnapshot snapshot;
    snapshot.host = QStringLiteral("hmi-factory.local");
    snapshot.port = 5931;
    snapshot.stage = ToolDiagStage::Connected;
    snapshot.remoteControlRequested = true;
    report.observeSnapshot(snapshot);
    ToolDiagReportOptions reveal;
    reveal.includeEndpoint = true;
    auto obj = parseReport(report.reportJson(reveal));
    CHECK(!obj.value(QStringLiteral("endpointRedacted")).toBool(true));
    CHECK(obj.value(QStringLiteral("endpoint")).toString()
          == QStringLiteral("hmi-factory.local"));
    CHECK(obj.value(QStringLiteral("wireSecurity")).toString()
          == QStringLiteral("rfb-none-observed-UNENCRYPTED"));
    CHECK(obj.value(QStringLiteral("remoteControlRequested")).toBool());
    // This flag reports client intent, never a granted device identity.
    CHECK(obj.value(QStringLiteral("remoteDeviceIdentity")).toString()
          == QStringLiteral("not-available"));

    snapshot.host = QStringLiteral("user:password@host.internal");
    report.observeSnapshot(snapshot);
    const auto compromised = report.reportJson(reveal);
    CHECK(!compromised.contains("password"));
    CHECK(!compromised.contains("host.internal"));
    CHECK(parseReport(compromised).value(QStringLiteral("endpointRedacted")).toBool());

    snapshot.host = QStringLiteral("https://host.local/path?token=secret");
    report.observeSnapshot(snapshot);
    CHECK(!report.reportJson(reveal).contains("secret"));

    // Independent P2 negative: a character whitelist must not mistake
    // drive-relative paths, credential text or encoded URLs for hostnames.
    const QStringList unsafeHosts = {
        QStringLiteral("C:private.txt"),
        QStringLiteral("alice:secret"),
        QStringLiteral("https:%2F%2Fhost.local%2Fpath%3Ftoken%3Dsecret"),
        QStringLiteral("host:5921"),
        QStringLiteral("[fe80::1"),
        QStringLiteral("[factory.local]"),
        QStringLiteral("fe80::1%bad/scope"),
        QStringLiteral("factory..local"),
        QStringLiteral("-bad.local"),
        QStringLiteral("bad-.local"),
        QStringLiteral("999.999.999.999"),
        QStringLiteral("user@host")
    };
    for (const QString &unsafeHost : unsafeHosts) {
        snapshot.host = unsafeHost;
        report.observeSnapshot(snapshot);
        const auto masked = report.reportJson(reveal);
        CHECK(!masked.contains(unsafeHost.toUtf8()));
        const auto maskedJson = parseReport(masked);
        CHECK(maskedJson.value(QStringLiteral("endpointRedacted")).toBool());
        CHECK(maskedJson.value(QStringLiteral("endpoint")).toString()
              == QStringLiteral("[redacted]"));
    }

    snapshot.host = QStringLiteral("[fe80::1%eth0]");
    report.observeSnapshot(snapshot);
    CHECK(parseReport(report.reportJson(reveal))
          .value(QStringLiteral("endpoint")).toString()
          == QStringLiteral("[fe80::1%eth0]"));

    snapshot.host = QString(256, QLatin1Char('x'));
    report.observeSnapshot(snapshot);
    CHECK(parseReport(report.reportJson(reveal))
              .value(QStringLiteral("endpoint")).toString() == QStringLiteral("[redacted]"));

    snapshot.host = QStringLiteral("fe80::1%2");
    report.observeSnapshot(snapshot);
    CHECK(parseReport(report.reportJson(reveal))
              .value(QStringLiteral("endpoint")).toString() == QStringLiteral("fe80::1%2"));
}

void testBoundedEventsAndMetrics()
{
    ToolBasicDiagnostics report;
    ToolDiagSnapshot snapshot;
    snapshot.stage = ToolDiagStage::Failed;
    snapshot.lastFailure = ToolDiagFailure::SecurityMismatch;
    snapshot.localFps = std::numeric_limits<double>::quiet_NaN();
    snapshot.localMbitPerSecond = std::numeric_limits<double>::infinity();
    snapshot.localFrameWidth = -2;
    snapshot.localFrameHeight = 100000;
    snapshot.receivedBytes = std::numeric_limits<quint64>::max();
    report.observeSnapshot(snapshot);

    for (int i = 0; i < 1200; ++i) {
        report.observeEvent(ToolDiagEvent{
            static_cast<qint64>(i), ToolDiagStage::Handshaking,
            ToolDiagFailure::SecurityMismatch});
    }
    report.observeEvent(ToolDiagEvent{-1, ToolDiagStage::Failed, ToolDiagFailure::Other});
    CHECK(report.recentEvents().size() == ToolBasicDiagnostics::kMaxEvents);
    CHECK(report.recentEvents().front().observedUtcMs == 1176);
    CHECK(report.recentEvents().back().observedUtcMs == 1199);
    const auto bytes = report.reportJson();
    CHECK(bytes.size() <= ToolBasicDiagnostics::kMaxReportBytes);
    const auto json = parseReport(bytes);
    const auto events = json.value(QStringLiteral("recentClientEvents")).toArray();
    CHECK(events.size() == ToolBasicDiagnostics::kMaxEvents);
    CHECK(events.at(0).toObject().value(QStringLiteral("observedUtcMs")).toString()
          == QStringLiteral("1176"));
    CHECK(json.value(QStringLiteral("nextStep")).toString()
          == QStringLiteral("verify-trusted-lan-and-security-type"));
    CHECK(json.value(QStringLiteral("wireSecurity")).toString()
          == QStringLiteral("not-observed"));
    const auto metrics = json.value(QStringLiteral("clientMeasurements")).toObject();
    CHECK(metrics.value(QStringLiteral("fps")).isNull());
    CHECK(metrics.value(QStringLiteral("mbitPerSecond")).isNull());
    CHECK(metrics.value(QStringLiteral("frameWidth")).isNull());
    CHECK(metrics.value(QStringLiteral("frameHeight")).isNull());
    CHECK(metrics.value(QStringLiteral("receivedBytes")).toString()
          == QStringLiteral("18446744073709551615"));
}

void testStableDiagnosticAdvice()
{
    CHECK(QString::fromLatin1(ToolBasicDiagnostics::nextStepToken(
        ToolDiagFailure::ConnectionRefused)) == QStringLiteral("verify-host-port-listener"));
    CHECK(QString::fromLatin1(ToolBasicDiagnostics::nextStepToken(
        ToolDiagFailure::DnsFailure)) == QStringLiteral("verify-hostname-dns"));
    CHECK(QString::fromLatin1(ToolBasicDiagnostics::nextStepToken(
        ToolDiagFailure::Timeout)) == QStringLiteral("verify-network-path-firewall"));
    CHECK(QString::fromLatin1(ToolBasicDiagnostics::nextStepToken(
        ToolDiagFailure::MalformedWire)) == QStringLiteral("verify-endpoint-protocol"));
    CHECK(QString::fromLatin1(ToolBasicDiagnostics::nextStepToken(
        ToolDiagFailure::RemoteClosed)) == QStringLiteral("check-connection-and-retry"));
    CHECK(QString::fromLatin1(ToolBasicDiagnostics::nextStepToken(
        ToolDiagFailure::Other)) == QStringLiteral("review-local-connection-settings"));
}
} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    testDefaultRedactionAndEvidenceBoundary();
    testExplicitEndpointAndInvalidUntrustedInput();
    testBoundedEventsAndMetrics();
    testStableDiagnosticAdvice();
    if (failures) {
        std::cerr << "TOOL_BASIC_DIAG_FAILED=" << failures << '\n';
        return 1;
    }
    std::cout << "TOOL_BASIC_DIAG_PASS" << std::endl;
    return 0;
}
