#include "basic_diagnostics.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHostAddress>

#include <cmath>
#include <utility>

namespace {

bool validEndpointHost(const QString &host)
{
    if (host.isEmpty() || host.size() > 255)
        return false;

    // Display/export only: the operator may opt into showing a *host*,
    // never arbitrary URL, credentials, path or a host:port combination.
    // Colons and zone IDs are legal only for structurally valid IPv6.
    const bool bracketed = host.startsWith(QLatin1Char('['));
    QString literal = host;
    if (bracketed) {
        if (!host.endsWith(QLatin1Char(']')) || host.size() < 4)
            return false;
        literal = host.mid(1, host.size() - 2);
    }

    if (bracketed || literal.contains(QLatin1Char(':'))) {
        const int zonePos = literal.indexOf(QLatin1Char('%'));
        const QString address = zonePos < 0 ? literal : literal.left(zonePos);
        if (zonePos >= 0) {
            const QString scope = literal.mid(zonePos + 1);
            if (scope.isEmpty() || scope.size() > 32)
                return false;
            for (const QChar ch : scope) {
                const ushort c = ch.unicode();
                if (!((c >= '0' && c <= '9')
                      || (c >= 'a' && c <= 'z')
                      || (c >= 'A' && c <= 'Z')
                      || c == '_' || c == '-' || c == '.'))
                    return false;
            }
        }
        QHostAddress ip;
        return ip.setAddress(address)
               && ip.protocol() == QAbstractSocket::IPv6Protocol;
    }

    // DNS/local host labels: no ':' '%', '@', slash, whitespace,
    // query/fragment characters, drive paths or credential syntax.
    // Limit a single label to 63 ASCII characters (including IDNA form).
    const auto labels = host.split(QLatin1Char('.'));
    for (const QString &label : labels) {
        if (label.isEmpty() || label.size() > 63
            || label.startsWith(QLatin1Char('-'))
            || label.endsWith(QLatin1Char('-')))
            return false;
        for (const QChar ch : label) {
            const ushort c = ch.unicode();
            if (!((c >= '0' && c <= '9')
                  || (c >= 'a' && c <= 'z')
                  || (c >= 'A' && c <= 'Z')
                  || c == '-' || c == '_'))
                return false;
        }
    }

    // Dot-decimal lookalikes must also be valid IPv4 literals; do not
    // bless a malformed numeric address as a DNS hostname in a report.
    bool dottedNumeric = host.contains(QLatin1Char('.'));
    for (const QChar ch : host) {
        const ushort c = ch.unicode();
        if (!((c >= '0' && c <= '9') || c == '.')) {
            dottedNumeric = false;
            break;
        }
    }
    if (dottedNumeric) {
        QHostAddress ip;
        return ip.setAddress(host)
               && ip.protocol() == QAbstractSocket::IPv4Protocol;
    }
    return true;
}

QJsonValue finiteNonNegative(double value)
{
    return std::isfinite(value) && value >= 0.0
               ? QJsonValue(value) : QJsonValue(QJsonValue::Null);
}

QJsonValue validFrameDimension(int value)
{
    return value >= 1 && value <= 16384
               ? QJsonValue(value) : QJsonValue(QJsonValue::Null);
}

} // namespace

const char *ToolBasicDiagnostics::stageToken(ToolDiagStage stage) noexcept
{
    switch (stage) {
    case ToolDiagStage::Disconnected: return "disconnected";
    case ToolDiagStage::Connecting: return "connecting";
    case ToolDiagStage::Handshaking: return "handshaking";
    case ToolDiagStage::Connected: return "connected";
    case ToolDiagStage::Failed: return "failed";
    }
    return "unrecognized";
}

const char *ToolBasicDiagnostics::failureToken(ToolDiagFailure failure) noexcept
{
    switch (failure) {
    case ToolDiagFailure::None: return "none";
    case ToolDiagFailure::ConnectionRefused: return "connection-refused";
    case ToolDiagFailure::DnsFailure: return "dns-failure";
    case ToolDiagFailure::Timeout: return "timeout";
    case ToolDiagFailure::SecurityMismatch: return "security-mismatch";
    case ToolDiagFailure::MalformedWire: return "malformed-wire";
    case ToolDiagFailure::RemoteClosed: return "remote-closed";
    case ToolDiagFailure::Other: return "other";
    }
    return "unrecognized";
}

const char *ToolBasicDiagnostics::nextStepToken(ToolDiagFailure failure) noexcept
{
    // Advice tokens are local UI suggestions; do not claim a remote device
    // status or a network/root cause that the client has not observed.
    switch (failure) {
    case ToolDiagFailure::None: return "none";
    case ToolDiagFailure::ConnectionRefused: return "verify-host-port-listener";
    case ToolDiagFailure::DnsFailure: return "verify-hostname-dns";
    case ToolDiagFailure::Timeout: return "verify-network-path-firewall";
    case ToolDiagFailure::SecurityMismatch: return "verify-trusted-lan-and-security-type";
    case ToolDiagFailure::MalformedWire: return "verify-endpoint-protocol";
    case ToolDiagFailure::RemoteClosed: return "check-connection-and-retry";
    case ToolDiagFailure::Other: return "review-local-connection-settings";
    }
    return "review-local-connection-settings";
}

void ToolBasicDiagnostics::observeSnapshot(ToolDiagSnapshot snapshot)
{
    m_snapshot = std::move(snapshot);
}

void ToolBasicDiagnostics::observeEvent(const ToolDiagEvent &event)
{
    if (event.observedUtcMs < 0)
        return;
    m_events.append(event);
    if (m_events.size() > kMaxEvents)
        m_events.remove(0, m_events.size() - kMaxEvents);
}

QByteArray ToolBasicDiagnostics::reportJson(const ToolDiagReportOptions &options) const
{
    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), 1);
    root.insert(QStringLiteral("reportType"), QStringLiteral("hyremote-tool-client-basic"));
    root.insert(QStringLiteral("provenance"), QStringLiteral("local-client-only"));
    root.insert(QStringLiteral("generatedFromObservedUtcMs"),
                QString::number(m_snapshot.observedUtcMs));
    root.insert(QStringLiteral("state"), QString::fromLatin1(stageToken(m_snapshot.stage)));
    root.insert(QStringLiteral("lastFailure"),
                QString::fromLatin1(failureToken(m_snapshot.lastFailure)));
    root.insert(QStringLiteral("nextStep"),
                QString::fromLatin1(nextStepToken(m_snapshot.lastFailure)));
    root.insert(QStringLiteral("remoteControlRequested"),
                m_snapshot.remoteControlRequested);

    // SecurityType None is the only implemented protocol for this Tool, but
    // do not claim that any RFB handshake happened until local Connected.
    root.insert(QStringLiteral("wireSecurity"),
                m_snapshot.stage == ToolDiagStage::Connected
                    ? QStringLiteral("rfb-none-observed-UNENCRYPTED")
                    : QStringLiteral("not-observed"));
    root.insert(QStringLiteral("remoteDeviceIdentity"), QStringLiteral("not-available"));
    root.insert(QStringLiteral("remoteCpu"), QStringLiteral("not-available"));
    root.insert(QStringLiteral("remoteRuntimeVersion"), QStringLiteral("not-available"));

    const bool showHost = options.includeEndpoint && validEndpointHost(m_snapshot.host);
    root.insert(QStringLiteral("endpoint"),
                showHost ? m_snapshot.host : QStringLiteral("[redacted]"));
    root.insert(QStringLiteral("endpointRedacted"), !showHost);
    root.insert(QStringLiteral("port"),
                m_snapshot.port == 0 ? QJsonValue(QJsonValue::Null)
                                     : QJsonValue(static_cast<int>(m_snapshot.port)));

    QJsonObject clientMeasurements;
    clientMeasurements.insert(QStringLiteral("frames"),
                              QString::number(m_snapshot.receivedFrames));
    clientMeasurements.insert(QStringLiteral("receivedBytes"),
                              QString::number(m_snapshot.receivedBytes));
    clientMeasurements.insert(QStringLiteral("fps"), finiteNonNegative(m_snapshot.localFps));
    clientMeasurements.insert(QStringLiteral("mbitPerSecond"),
                              finiteNonNegative(m_snapshot.localMbitPerSecond));
    clientMeasurements.insert(QStringLiteral("frameWidth"),
                              validFrameDimension(m_snapshot.localFrameWidth));
    clientMeasurements.insert(QStringLiteral("frameHeight"),
                              validFrameDimension(m_snapshot.localFrameHeight));
    root.insert(QStringLiteral("clientMeasurements"), clientMeasurements);

    QJsonArray events;
    for (const auto &event : m_events) {
        QJsonObject item;
        item.insert(QStringLiteral("observedUtcMs"),
                    QString::number(event.observedUtcMs));
        item.insert(QStringLiteral("state"),
                    QString::fromLatin1(stageToken(event.stage)));
        item.insert(QStringLiteral("failure"),
                    QString::fromLatin1(failureToken(event.reason)));
        events.append(item);
    }
    root.insert(QStringLiteral("recentClientEvents"), events);
    auto encoded = QJsonDocument(root).toJson(QJsonDocument::Compact);
    if (encoded.size() > kMaxReportBytes) {
        // Never truncate JSON in the middle or silently drop provenance.
        return QByteArrayLiteral(
            "{\"schemaVersion\":1,\"reportType\":\"hyremote-tool-client-basic\","
            "\"result\":\"report-too-large\",\"provenance\":\"local-client-only\"}");
    }
    return encoded;
}
