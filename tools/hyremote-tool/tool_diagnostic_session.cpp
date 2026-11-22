#include "tool_diagnostic_session.hpp"

#include <QDateTime>

#include <utility>

namespace {
qint64 utcMillis()
{
    return QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
}
} // namespace

ToolDiagStage ToolDiagnosticSession::stageFor(RfbClient::State state) noexcept
{
    switch (state) {
    case RfbClient::State::Connecting: return ToolDiagStage::Connecting;
    case RfbClient::State::Handshaking: return ToolDiagStage::Handshaking;
    case RfbClient::State::Connected: return ToolDiagStage::Connected;
    case RfbClient::State::Failed: return ToolDiagStage::Failed;
    case RfbClient::State::Disconnected: return ToolDiagStage::Disconnected;
    }
    return ToolDiagStage::Disconnected;
}

ToolDiagFailure ToolDiagnosticSession::classifySocketError(
    QAbstractSocket::SocketError error)
{
    switch (error) {
    case QAbstractSocket::ConnectionRefusedError:
        return ToolDiagFailure::ConnectionRefused;
    case QAbstractSocket::HostNotFoundError:
        return ToolDiagFailure::DnsFailure;
    case QAbstractSocket::SocketTimeoutError:
        return ToolDiagFailure::Timeout;
    case QAbstractSocket::RemoteHostClosedError:
        return ToolDiagFailure::RemoteClosed;
    default:
        return ToolDiagFailure::Other;
    }
}

ToolDiagFailure ToolDiagnosticSession::classifyClientError(const QString &error)
{
    // No substring search of untrusted peer text or localized QTcpSocket
    // errorString: failure types below are fixed strings authored by Tool.
    if (error == QStringLiteral("remote authentication failed")
        || error.startsWith(QStringLiteral("the remote endpoint requires security type(s) ")))
        return ToolDiagFailure::SecurityMismatch;

    if (error == QStringLiteral("remote endpoint sent an oversized authentication reason")
        || error == QStringLiteral("remote endpoint sent an oversized refusal reason")
        || error == QStringLiteral("remote endpoint sent an oversized server name")
        || error == QStringLiteral("remote endpoint exceeded the client receive budget")
        || error == QStringLiteral("the remote endpoint announced an empty framebuffer")
        || error == QStringLiteral("remote cut text exceeds the client budget")
        || error == QStringLiteral("remote framebuffer update exceeds the client payload budget")
        || error == QStringLiteral("remote resize exceeds the client allocation budget")
        || error == QStringLiteral("remote cursor has an invalid size")
        || error == QStringLiteral("not an RFB 3.x endpoint")
        || error == QStringLiteral("remote framebuffer update has too many rectangles")
        || error == QStringLiteral("remote raw rectangle is too large")
        || error == QStringLiteral("remote pixel format is not supported by this client")
        || error == QStringLiteral("remote framebuffer exceeds the client allocation budget")
        || error.startsWith(QStringLiteral("unsupported rectangle encoding"))
        || error.startsWith(QStringLiteral("unsupported server message type"))
        || error.startsWith(QStringLiteral("remote rectangle is outside the framebuffer")))
        return ToolDiagFailure::MalformedWire;

    // A protocol-level refusal does NOT prove TCP ConnectionRefusedError.
    return ToolDiagFailure::Other;
}

void ToolDiagnosticSession::publish(bool recordEvent)
{
    m_snapshot.observedUtcMs = utcMillis();
    m_report.observeSnapshot(m_snapshot);
    if (recordEvent)
        m_report.observeEvent(
            ToolDiagEvent{m_snapshot.observedUtcMs, m_snapshot.stage, m_snapshot.lastFailure});
}

void ToolDiagnosticSession::beginAttempt(QString host, quint16 port)
{
    // A new explicit attempt never inherits another endpoint's screen,
    // counters, failure, control intent or event history.
    m_report = ToolBasicDiagnostics{};
    m_snapshot = ToolDiagSnapshot{};
    m_snapshot.host = std::move(host);
    m_snapshot.port = port;
    m_snapshot.stage = ToolDiagStage::Connecting;
    m_socketFailure = ToolDiagFailure::None;
    publish(true);
}

void ToolDiagnosticSession::observeState(RfbClient::State state)
{
    const ToolDiagStage mapped = stageFor(state);
    if (m_snapshot.stage == mapped)
        return;
    m_snapshot.stage = mapped;
    if (mapped == ToolDiagStage::Connected)
        m_snapshot.lastFailure = ToolDiagFailure::None;
    if (mapped == ToolDiagStage::Disconnected || mapped == ToolDiagStage::Failed)
        m_snapshot.remoteControlRequested = false;
    publish(true);
}

void ToolDiagnosticSession::observeSocketError(QAbstractSocket::SocketError error)
{
    m_socketFailure = classifySocketError(error);
    if (m_socketFailure != ToolDiagFailure::None)
        m_snapshot.lastFailure = m_socketFailure;
    publish(false);
}

void ToolDiagnosticSession::observeError(const QString &clientError)
{
    // Structured socket reason (if present) is stronger evidence than a
    // translated or unrecognized free-form client errorString.
    if (m_socketFailure != ToolDiagFailure::None)
        m_snapshot.lastFailure = m_socketFailure;
    else
        m_snapshot.lastFailure = classifyClientError(clientError);
    publish(false);
}

void ToolDiagnosticSession::observeStats(const RfbClient::Stats &stats)
{
    m_snapshot.localFps = stats.fps;
    m_snapshot.localMbitPerSecond = stats.mbitPerSecond;
    m_snapshot.receivedFrames = stats.frames;
    m_snapshot.receivedBytes = stats.bytes;
    if (stats.framebufferSize.isValid() && !stats.framebufferSize.isEmpty()) {
        m_snapshot.localFrameWidth = stats.framebufferSize.width();
        m_snapshot.localFrameHeight = stats.framebufferSize.height();
    }
    publish(false);
}

void ToolDiagnosticSession::observeFrame(const QSize &size)
{
    if (size.isValid() && !size.isEmpty()) {
        m_snapshot.localFrameWidth = size.width();
        m_snapshot.localFrameHeight = size.height();
    }
    publish(false);
}

void ToolDiagnosticSession::setControlRequested(bool requested)
{
    m_snapshot.remoteControlRequested = requested;
    publish(false);
}

QByteArray ToolDiagnosticSession::previewJson(bool includeEndpoint) const
{
    ToolDiagReportOptions options;
    options.includeEndpoint = includeEndpoint;
    return m_report.reportJson(options);
}
