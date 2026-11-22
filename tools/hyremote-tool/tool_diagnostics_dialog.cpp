#include "tool_diagnostics_dialog.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QVBoxLayout>

namespace {
QString stageLabel(ToolDiagStage stage, bool zh)
{
    switch (stage) {
    case ToolDiagStage::Disconnected: return zh ? QStringLiteral("未连接") : QStringLiteral("Disconnected");
    case ToolDiagStage::Connecting: return zh ? QStringLiteral("连接中") : QStringLiteral("Connecting");
    case ToolDiagStage::Handshaking: return zh ? QStringLiteral("握手中") : QStringLiteral("Handshaking");
    case ToolDiagStage::Connected: return zh ? QStringLiteral("已连接（仅客户端观测）") : QStringLiteral("Connected (client-observed)");
    case ToolDiagStage::Failed: return zh ? QStringLiteral("失败") : QStringLiteral("Failed");
    }
    return zh ? QStringLiteral("未知") : QStringLiteral("Unknown");
}

QString adviceLabel(ToolDiagFailure failure, bool zh)
{
    switch (failure) {
    case ToolDiagFailure::None: return zh ? QStringLiteral("暂无已观察到的故障。") : QStringLiteral("No observed failure.");
    case ToolDiagFailure::ConnectionRefused: return zh ? QStringLiteral("检查远程地址、端口及监听服务。") : QStringLiteral("Verify endpoint, port and listener.");
    case ToolDiagFailure::DnsFailure: return zh ? QStringLiteral("检查主机名和 DNS。") : QStringLiteral("Verify hostname and DNS.");
    case ToolDiagFailure::Timeout: return zh ? QStringLiteral("检查网络路径和防火墙。") : QStringLiteral("Verify network path and firewall.");
    case ToolDiagFailure::SecurityMismatch: return zh ? QStringLiteral("检查可信局域网及 RFB 安全类型。当前仅支持未加密的 None。") : QStringLiteral("Check trusted LAN and RFB security type. Only unencrypted None is supported.");
    case ToolDiagFailure::MalformedWire: return zh ? QStringLiteral("确认端点是否为兼容的 RFB 服务。") : QStringLiteral("Verify that the endpoint provides compatible RFB.");
    case ToolDiagFailure::RemoteClosed: return zh ? QStringLiteral("检查会话和网络连接后重试。") : QStringLiteral("Check the connection and retry.");
    case ToolDiagFailure::Other: return zh ? QStringLiteral("检查本机连接设置及受信任的网络。") : QStringLiteral("Review local connection settings and trusted network.");
    }
    return zh ? QStringLiteral("没有足够证据判断。") : QStringLiteral("Insufficient evidence.");
}
} // namespace

ToolDiagnosticsDialog::ToolDiagnosticsDialog(
    const ToolDiagnosticSession &session, bool chinese, QWidget *parent)
    : QDialog(parent), m_session(session), m_chinese(chinese)
{
    setObjectName(QStringLiteral("tool-diagnostics-dialog"));
    setModal(false);
    resize(570, 450);
    setMinimumSize(420, 340);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(8);

    m_heading = new QLabel(this);
    m_heading->setWordWrap(true);
    layout->addWidget(m_heading);

    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);
    layout->addWidget(m_summary);
    m_advice = new QLabel(this);
    m_advice->setWordWrap(true);
    layout->addWidget(m_advice);

    m_revealEndpoint = new QCheckBox(this);
    m_revealEndpoint->setObjectName(QStringLiteral("tool-diagnostics-reveal-endpoint"));
    m_revealEndpoint->setChecked(false);
    layout->addWidget(m_revealEndpoint);

    m_preview = new QPlainTextEdit(this);
    m_preview->setObjectName(QStringLiteral("tool-diagnostics-preview"));
    m_preview->setReadOnly(true);
    m_preview->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    layout->addWidget(m_preview, 1);

    m_privacy = new QLabel(this);
    m_privacy->setWordWrap(true);
    layout->addWidget(m_privacy);

    auto *actions = new QHBoxLayout;
    m_refresh = new QPushButton(this);
    m_refresh->setObjectName(QStringLiteral("tool-diagnostics-refresh"));
    m_copy = new QPushButton(this);
    m_copy->setObjectName(QStringLiteral("tool-diagnostics-copy"));
    m_save = new QPushButton(this);
    m_save->setObjectName(QStringLiteral("tool-diagnostics-save"));
    m_close = new QPushButton(this);
    actions->addWidget(m_refresh);
    actions->addStretch(1);
    actions->addWidget(m_copy);
    actions->addWidget(m_save);
    actions->addWidget(m_close);
    layout->addLayout(actions);

    connect(m_revealEndpoint, &QCheckBox::toggled, this, [this] { refresh(); });
    connect(m_refresh, &QPushButton::clicked, this, [this] { refresh(); });
    connect(m_copy, &QPushButton::clicked, this, [this] { copyExplicitly(); });
    connect(m_save, &QPushButton::clicked, this, [this] { saveExplicitly(); });
    connect(m_close, &QPushButton::clicked, this, &QDialog::close);
    retranslate();
    refresh();
}

QString ToolDiagnosticsDialog::localText(const char *zh, const char *en) const
{
    return QString::fromUtf8(m_chinese ? zh : en);
}

void ToolDiagnosticsDialog::setChinese(bool chinese)
{
    // Re-translation is called on every frame/stat update by the existing
    // Tool window. Avoid re-serializing report JSON when locale is unchanged.
    if (m_chinese == chinese)
        return;
    m_chinese = chinese;
    retranslate();
    refresh();
}

void ToolDiagnosticsDialog::retranslate()
{
    setWindowTitle(localText("本机基础诊断", "Local basic diagnostics"));
    m_heading->setText(localText("本机客户端连接诊断（只读）",
                                 "Local client connection diagnostics (read-only)"));
    m_revealEndpoint->setText(localText(
        "在预览和导出中显示合法设备地址（手动选择；默认隐藏）",
        "Include a valid endpoint in preview/export (opt-in; hidden by default)"));
    m_privacy->setText(localText(
        "报告仅包含本机已观察到的信息，不代表远端身份、CPU 或业务结果。"
        "RFB None 未加密；复制/保存仅在点击后执行。",
        "Client-observed facts only, not remote identity, CPU or business outcome. "
        "RFB None is unencrypted. Copy/save occurs only when clicked."));
    m_refresh->setText(localText("刷新", "Refresh"));
    m_copy->setText(localText("复制报告", "Copy report"));
    m_save->setText(localText("保存报告", "Save report"));
    m_close->setText(localText("关闭", "Close"));
}

void ToolDiagnosticsDialog::resetEndpointSelection()
{
    m_revealEndpoint->setChecked(false);
    refresh();
}

void ToolDiagnosticsDialog::refresh()
{
    const auto &facts = m_session.report().snapshot();
    m_summary->setText(localText("状态：", "State: ")
                       + stageLabel(facts.stage, m_chinese));
    m_advice->setText(localText("排查建议：", "Next step: ")
                      + adviceLabel(facts.lastFailure, m_chinese));
    const QByteArray raw = m_session.previewJson(m_revealEndpoint->isChecked());
    m_preview->setPlainText(QString::fromUtf8(raw));
}

void ToolDiagnosticsDialog::copyExplicitly()
{
    // Regenerate from the current observed state; a stale preview must not
    // leak a different endpoint after a reconnect or manual selection.
    refresh();
    if (auto *clipboard = QApplication::clipboard())
        clipboard->setText(m_preview->toPlainText());
}

void ToolDiagnosticsDialog::saveExplicitly()
{
    refresh();
    const QString path = QFileDialog::getSaveFileName(
        this, localText("保存本机诊断报告", "Save local diagnostic report"),
        QStringLiteral("hyremote-tool-diagnostics.json"),
        QStringLiteral("JSON (*.json)"));
    if (path.isEmpty())
        return; // cancel is side-effect free

    const QByteArray bytes = m_session.previewJson(m_revealEndpoint->isChecked());
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly)
        || output.write(bytes) != bytes.size() || !output.commit()) {
        output.cancelWriting();
        QMessageBox::warning(this, localText("保存失败", "Save failed"),
                             localText("无法保存报告；请检查本机文件权限。",
                                       "Could not save the report. Check local file permissions."));
    }
}
