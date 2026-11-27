#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>
#include <QtGlobal>

// S1 #585: local-client observations only. The report is a *preview* for
// explicit user action, not an upload, a Host API or security attestation.
enum class ToolDiagStage {
    Disconnected, Connecting, Handshaking, Connected, Failed
};

enum class ToolDiagFailure {
    None, ConnectionRefused, DnsFailure, Timeout, SecurityMismatch,
    MalformedWire, RemoteClosed, Other
};

struct ToolDiagEvent {
    qint64 observedUtcMs = 0;
    ToolDiagStage stage = ToolDiagStage::Disconnected;
    ToolDiagFailure reason = ToolDiagFailure::None;
};

struct ToolDiagSnapshot {
    QString host; // in-memory display input, redacted by default on export
    quint16 port = 0;
    ToolDiagStage stage = ToolDiagStage::Disconnected;
    ToolDiagFailure lastFailure = ToolDiagFailure::None;
    bool remoteControlRequested = false;
    // Counts observed on the local client, not measured on the remote Host.
    double localFps = 0;
    double localMbitPerSecond = 0;
    quint64 receivedFrames = 0;
    quint64 receivedBytes = 0;
    int localFrameWidth = 0;
    int localFrameHeight = 0;
    qint64 observedUtcMs = 0;
};

struct ToolDiagReportOptions {
    // The default always removes the operator's endpoint. Opt-in only after
    // preview: the receiving app must not silently copy/upload this output.
    bool includeEndpoint = false;
};

class ToolBasicDiagnostics final
{
public:
    static constexpr int kMaxEvents = 24;
    static constexpr int kMaxReportBytes = 8192;

    void observeSnapshot(ToolDiagSnapshot snapshot);
    void observeEvent(const ToolDiagEvent &event);
    const ToolDiagSnapshot &snapshot() const noexcept { return m_snapshot; }
    const QVector<ToolDiagEvent> &recentEvents() const noexcept { return m_events; }

    // A safe, bounded, deterministic report preview with explicit provenance.
    // No raw socket error / credential / log text is ever serialized.
    QByteArray reportJson(const ToolDiagReportOptions &options = {}) const;

    static const char *stageToken(ToolDiagStage stage) noexcept;
    static const char *failureToken(ToolDiagFailure failure) noexcept;
    static const char *nextStepToken(ToolDiagFailure failure) noexcept;

private:
    ToolDiagSnapshot m_snapshot;
    QVector<ToolDiagEvent> m_events;
};
