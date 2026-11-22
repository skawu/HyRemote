#pragma once

#include "basic_diagnostics.hpp"
#include "rfb_client.h"

#include <QAbstractSocket>
#include <QByteArray>
#include <QString>

// The RfbClient is the only protocol/network observer. This small local
// session adapter never parses peer messages and never records raw socket,
// server-name, exception or user-entered password text in diagnostic reports.
class ToolDiagnosticSession final
{
public:
    void beginAttempt(QString host, quint16 port);
    void observeState(RfbClient::State state);
    void observeSocketError(QAbstractSocket::SocketError error);
    void observeError(const QString &clientError);
    void observeStats(const RfbClient::Stats &stats);
    void observeFrame(const QSize &size);
    void setControlRequested(bool requested);

    const ToolBasicDiagnostics &report() const noexcept { return m_report; }
    QByteArray previewJson(bool includeEndpoint = false) const;

private:
    void publish(bool recordEvent);
    static ToolDiagStage stageFor(RfbClient::State state) noexcept;
    static ToolDiagFailure classifyClientError(const QString &error);
    static ToolDiagFailure classifySocketError(QAbstractSocket::SocketError error);

    ToolBasicDiagnostics m_report;
    ToolDiagSnapshot m_snapshot;
    ToolDiagFailure m_socketFailure = ToolDiagFailure::None;
};
