#pragma once

#include "tool_diagnostic_session.hpp"

#include <QDialog>

class QCheckBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;

// Read-only local support view. No network, log ingestion, background I/O or
// clipboard writes. The operator explicitly requests any copy/save action.
class ToolDiagnosticsDialog final : public QDialog
{
public:
    explicit ToolDiagnosticsDialog(const ToolDiagnosticSession &session,
                                   bool chinese, QWidget *parent = nullptr);

    void setChinese(bool chinese);
    void resetEndpointSelection();
    void refresh();

private:
    void retranslate();
    void copyExplicitly();
    void saveExplicitly();
    QString localText(const char *zh, const char *en) const;

    const ToolDiagnosticSession &m_session;
    bool m_chinese = false;
    QLabel *m_heading = nullptr;
    QLabel *m_summary = nullptr;
    QLabel *m_advice = nullptr;
    QLabel *m_privacy = nullptr;
    QCheckBox *m_revealEndpoint = nullptr;
    QPlainTextEdit *m_preview = nullptr;
    QPushButton *m_refresh = nullptr;
    QPushButton *m_copy = nullptr;
    QPushButton *m_save = nullptr;
    QPushButton *m_close = nullptr;
};
