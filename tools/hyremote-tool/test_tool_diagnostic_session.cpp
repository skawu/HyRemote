#include "tool_diagnostic_session.hpp"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>

#include <iostream>
#include <limits>

namespace {
int failures = 0;
#define CHECK(x) do { if (!(x)) { \
    std::cerr << "TOOL_DIAG_SESSION_FAIL line " << __LINE__ << ": " #x << '\n'; \
    ++failures; } } while (false)

QJsonObject snapshot(const ToolDiagnosticSession &session, bool reveal = false)
{
    const auto raw = session.previewJson(reveal);
    CHECK(raw.size() <= ToolBasicDiagnostics::kMaxReportBytes);
    const auto doc = QJsonDocument::fromJson(raw);
    CHECK(doc.isObject());
    return doc.object();
}

void verifyLiveFactsAndPrivacy()
{
    ToolDiagnosticSession observer;
    CHECK(snapshot(observer).value(QStringLiteral("endpointRedacted")).toBool());
    observer.beginAttempt(QStringLiteral("10.10.12.8"), 5921);
    auto state = snapshot(observer);
    CHECK(state.value(QStringLiteral("state")).toString() == QStringLiteral("connecting"));
    CHECK(state.value(QStringLiteral("endpoint")).toString() == QStringLiteral("[redacted]"));
    CHECK(state.value(QStringLiteral("port")).toInt() == 5921);
    observer.observeState(RfbClient::State::Handshaking);
    CHECK(snapshot(observer).value(QStringLiteral("wireSecurity")).toString()
          == QStringLiteral("not-observed"));
    observer.observeState(RfbClient::State::Connected);
    observer.setControlRequested(true);
    RfbClient::Stats stats;
    stats.frames = 13;
    stats.bytes = 84521;
    stats.fps = 11.25;
    stats.mbitPerSecond = 3.5;
    stats.framebufferSize = QSize(1920, 1080);
    observer.observeStats(stats);
    observer.observeFrame(QSize(1280, 720));
    state = snapshot(observer);
    CHECK(state.value(QStringLiteral("state")).toString() == QStringLiteral("connected"));
    CHECK(state.value(QStringLiteral("wireSecurity")).toString()
          == QStringLiteral("rfb-none-observed-UNENCRYPTED"));
    CHECK(state.value(QStringLiteral("remoteDeviceIdentity")).toString()
          == QStringLiteral("not-available"));
    CHECK(state.value(QStringLiteral("remoteControlRequested")).toBool());
    auto metrics = state.value(QStringLiteral("clientMeasurements")).toObject();
    CHECK(metrics.value(QStringLiteral("frames")).toString() == QStringLiteral("13"));
    CHECK(metrics.value(QStringLiteral("receivedBytes")).toString() == QStringLiteral("84521"));
    CHECK(metrics.value(QStringLiteral("frameWidth")).toInt() == 1280);
    CHECK(metrics.value(QStringLiteral("frameHeight")).toInt() == 720);
    CHECK(snapshot(observer, true).value(QStringLiteral("endpoint")).toString()
          == QStringLiteral("10.10.12.8"));
    observer.observeState(RfbClient::State::Disconnected);
    CHECK(!snapshot(observer).value(QStringLiteral("remoteControlRequested")).toBool());
    observer.setControlRequested(true);
    observer.observeState(RfbClient::State::Failed);
    CHECK(!snapshot(observer).value(QStringLiteral("remoteControlRequested")).toBool());

    observer.beginAttempt(QStringLiteral("C:private.txt"), 5500);
    CHECK(snapshot(observer, true).value(QStringLiteral("endpoint")).toString()
          == QStringLiteral("[redacted]"));
    CHECK(snapshot(observer).value(QStringLiteral("recentClientEvents")).toArray().size() == 1);
    CHECK(snapshot(observer).value(QStringLiteral("clientMeasurements"))
          .toObject().value(QStringLiteral("frames")).toString() == QStringLiteral("0"));
    CHECK(!snapshot(observer).value(QStringLiteral("remoteControlRequested")).toBool());
}

void verifyFailureFactPrecedence()
{
    ToolDiagnosticSession observer;
    observer.beginAttempt(QStringLiteral("host.local"), 5900);
    observer.observeSocketError(QAbstractSocket::HostNotFoundError);
    observer.observeError(QStringLiteral("locale-dependent socket text"));
    observer.observeState(RfbClient::State::Failed);
    auto state = snapshot(observer);
    CHECK(state.value(QStringLiteral("state")).toString() == QStringLiteral("failed"));
    CHECK(state.value(QStringLiteral("lastFailure")).toString() == QStringLiteral("dns-failure"));
    CHECK(state.value(QStringLiteral("nextStep")).toString()
          == QStringLiteral("verify-hostname-dns"));
    CHECK(!observer.previewJson().contains("locale-dependent"));

    observer.beginAttempt(QStringLiteral("host.local"), 5900);
    observer.observeSocketError(QAbstractSocket::ConnectionRefusedError);
    observer.observeError(QStringLiteral("Connection refused"));
    observer.observeState(RfbClient::State::Failed);
    CHECK(snapshot(observer).value(QStringLiteral("lastFailure")).toString()
          == QStringLiteral("connection-refused"));
    observer.beginAttempt(QStringLiteral("host.local"), 5900);
    observer.observeSocketError(QAbstractSocket::SocketTimeoutError);
    observer.observeError(QStringLiteral("some custom locale timeout"));
    observer.observeState(RfbClient::State::Failed);
    CHECK(snapshot(observer).value(QStringLiteral("lastFailure")).toString()
          == QStringLiteral("timeout"));
    observer.beginAttempt(QStringLiteral("host.local"), 5900);
    observer.observeSocketError(QAbstractSocket::RemoteHostClosedError);
    observer.observeError(QStringLiteral("remote reset"));
    observer.observeState(RfbClient::State::Failed);
    CHECK(snapshot(observer).value(QStringLiteral("lastFailure")).toString()
          == QStringLiteral("remote-closed"));

    observer.beginAttempt(QStringLiteral("host.local"), 5900);
    observer.observeError(QStringLiteral("the remote endpoint requires security type(s) 2; this version supports None only"));
    observer.observeState(RfbClient::State::Failed);
    CHECK(snapshot(observer).value(QStringLiteral("lastFailure")).toString()
          == QStringLiteral("security-mismatch"));
    observer.beginAttempt(QStringLiteral("host.local"), 5900);
    observer.observeError(QStringLiteral("remote endpoint sent an oversized authentication reason"));
    observer.observeState(RfbClient::State::Failed);
    CHECK(snapshot(observer).value(QStringLiteral("lastFailure")).toString()
          == QStringLiteral("malformed-wire"));
    const QStringList malformedProtectors = {
        QStringLiteral("remote endpoint sent an oversized refusal reason"),
        QStringLiteral("remote endpoint sent an oversized server name"),
        QStringLiteral("remote endpoint exceeded the client receive budget"),
        QStringLiteral("remote cut text exceeds the client budget"),
        QStringLiteral("remote framebuffer update exceeds the client payload budget"),
        QStringLiteral("remote resize exceeds the client allocation budget"),
        QStringLiteral("the remote endpoint announced an empty framebuffer"),
        QStringLiteral("remote cursor has an invalid size")
    };
    for (const QString &reason : malformedProtectors) {
        observer.beginAttempt(QStringLiteral("host.local"), 5900);
        observer.observeError(reason);
        observer.observeState(RfbClient::State::Failed);
        CHECK(snapshot(observer).value(QStringLiteral("lastFailure")).toString()
              == QStringLiteral("malformed-wire"));
    }
    observer.beginAttempt(QStringLiteral("host.local"), 5900);
    observer.observeError(QStringLiteral("could not allocate the remote framebuffer"));
    observer.observeState(RfbClient::State::Failed);
    CHECK(snapshot(observer).value(QStringLiteral("lastFailure")).toString()
          == QStringLiteral("other"));
    observer.beginAttempt(QStringLiteral("host.local"), 5900);
    observer.observeError(QStringLiteral("not an RFB 3.x endpoint"));
    observer.observeState(RfbClient::State::Failed);
    CHECK(snapshot(observer).value(QStringLiteral("lastFailure")).toString()
          == QStringLiteral("malformed-wire"));
    observer.beginAttempt(QStringLiteral("host.local"), 5900);
    observer.observeError(QStringLiteral("untrusted\nfake-log token=secret"));
    observer.observeState(RfbClient::State::Failed);
    CHECK(snapshot(observer).value(QStringLiteral("lastFailure")).toString()
          == QStringLiteral("other"));
    CHECK(!observer.previewJson().contains("fake-log"));
    CHECK(!observer.previewJson().contains("token=secret"));
    CHECK(!observer.previewJson().contains("host.local"));
    observer.beginAttempt(QStringLiteral("host.local"), 5900);
    CHECK(snapshot(observer).value(QStringLiteral("lastFailure")).toString()
          == QStringLiteral("none"));

    for (int i = 0; i < 1000; ++i)
        observer.observeState((i % 2) ? RfbClient::State::Connected
                                      : RfbClient::State::Handshaking);
    CHECK(observer.report().recentEvents().size() == ToolBasicDiagnostics::kMaxEvents);
    CHECK(snapshot(observer).value(QStringLiteral("recentClientEvents")).toArray().size()
          == ToolBasicDiagnostics::kMaxEvents);
}
} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    verifyLiveFactsAndPrivacy();
    verifyFailureFactPrecedence();
    if (failures)
        return 1;
    std::cout << "TOOL_DIAG_SESSION_PASS" << std::endl;
    return 0;
}
