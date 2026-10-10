#include <HyRemote/RemoteAccess.h>

#include <QAbstractSocket>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHostAddress>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkInterface>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSizePolicy>
#include <QSaveFile>
#include <QSlider>
#include <QSpinBox>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <iostream>

namespace {

QString stateName(HyRemote::RemoteAccessState state)
{
    switch (state) {
    case HyRemote::RemoteAccessState::Stopped:
        return QStringLiteral("Stopped");
    case HyRemote::RemoteAccessState::Starting:
        return QStringLiteral("Starting");
    case HyRemote::RemoteAccessState::Running:
        return QStringLiteral("Running");
    case HyRemote::RemoteAccessState::Stopping:
        return QStringLiteral("Stopping");
    case HyRemote::RemoteAccessState::Faulted:
        return QStringLiteral("Faulted");
    }
    return QStringLiteral("Unknown");
}

int readPositiveInt(const QCommandLineParser &parser,
                    const QCommandLineOption &option,
                    int fallback)
{
    bool ok = false;
    const int value = parser.value(option).toInt(&ok);
    return ok && value > 0 ? value : fallback;
}

QString diagnosticValue(const QString &report, const QString &key)
{
    const QString prefix = key + QLatin1Char('=');
    for (const QString &line : report.split(QLatin1Char('\n'))) {
        if (line.startsWith(prefix))
            return line.mid(prefix.size());
    }
    return QStringLiteral("unknown");
}

QStringList viewerEndpoints(quint16 port)
{
    QStringList endpoints;
    for (const QNetworkInterface &interface : QNetworkInterface::allInterfaces()) {
        const auto flags = interface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp)
            || !flags.testFlag(QNetworkInterface::IsRunning))
            continue;

        for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
            const QHostAddress address = entry.ip();
            if (address.protocol() != QAbstractSocket::IPv4Protocol || address.isLoopback()
                || address.isLinkLocal() || address.isNull())
                continue;

            const QString endpoint = QStringLiteral("%1:%2").arg(address.toString()).arg(port);
            if (!endpoints.contains(endpoint))
                endpoints.push_back(endpoint);
        }
    }
    endpoints.sort(Qt::CaseInsensitive);
    return endpoints;
}

class SupportWindow final : public QWidget
{
public:
    explicit SupportWindow(quint16 port, bool initialInputEnabled, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_remote(this)
        , m_port(port)
    {
        setWindowTitle(QStringLiteral("HyRemoteTool"));
        // Keep the native window within the usable desktop height even at
        // high DPI and on compact displays. Overflow belongs to the content
        // scroller below, not to the top-level window's minimum height.
        const QScreen *screen = QApplication::primaryScreen();
        const QRect available = screen ? screen->availableGeometry() : QRect(0, 0, 780, 720);
        resize(qMin(780, qMax(360, available.width() - 48)),
               qMin(700, qMax(280, available.height() - 80)));

        m_remote.setPort(port);
        m_remote.setRemoteInputEnabled(initialInputEnabled);

        // The operator panel is taller than many desktop viewports. Without
        // this scroll area, the nested forms and diagnostics editors force a
        // native minimum height larger than the screen and hide bottom actions.
        auto *windowLayout = new QVBoxLayout(this);
        windowLayout->setContentsMargins(0, 0, 0, 0);
        m_scrollArea = new QScrollArea(this);
        m_scrollArea->setWidgetResizable(true);
        m_scrollArea->setFrameShape(QFrame::NoFrame);
        m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        windowLayout->addWidget(m_scrollArea);
        auto *content = new QWidget(m_scrollArea);
        auto *root = new QVBoxLayout(content);
        m_scrollArea->setWidget(content);

        auto *heading = new QLabel(QStringLiteral("HyRemoteTool"), content);
        QFont headingFont = heading->font();
        headingFont.setPointSize(16);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        root->addWidget(heading);

        auto *intro = new QLabel(
            QStringLiteral("The local application remains authoritative. Remote access is off until "
                           "the local operator explicitly starts it."),
            content);
        intro->setWordWrap(true);
        root->addWidget(intro);

        auto *remoteBox = new QGroupBox(QStringLiteral("Remote support"), content);
        auto *remoteLayout = new QVBoxLayout(remoteBox);

        auto *statusForm = new QFormLayout;
        // On narrow/high-DPI desktops, break labels onto their own row
        // instead of forcing the content widget wider than the viewport.
        statusForm->setRowWrapPolicy(QFormLayout::WrapLongRows);
        m_state = new QLabel(remoteBox);
        m_buildIdentity = new QLabel(remoteBox);
        m_buildIdentity->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_buildIdentity->setWordWrap(true);
        auto buildIdentityPolicy = m_buildIdentity->sizePolicy();
        buildIdentityPolicy.setHorizontalPolicy(QSizePolicy::Ignored);
        buildIdentityPolicy.setHeightForWidth(true);
        m_buildIdentity->setSizePolicy(buildIdentityPolicy);
        m_deploymentIdentity = new QLabel(remoteBox);
        m_deploymentIdentity->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_deploymentIdentity->setWordWrap(true);
        auto deploymentIdentityPolicy = m_deploymentIdentity->sizePolicy();
        deploymentIdentityPolicy.setHorizontalPolicy(QSizePolicy::Ignored);
        deploymentIdentityPolicy.setHeightForWidth(true);
        m_deploymentIdentity->setSizePolicy(deploymentIdentityPolicy);
        m_platform = new QLabel(remoteBox);
        m_security = new QLabel(remoteBox);
        m_configuredListener = new QLabel(remoteBox);
        m_endpoint = new QLabel(remoteBox);
        m_viewerEndpoints = new QLabel(remoteBox);
        m_viewerEndpoints->setWordWrap(true);
        m_viewerEndpoints->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_connection = new QLabel(remoteBox);
        m_clients = new QLabel(QStringLiteral("0"), remoteBox);
        m_policy = new QLabel(remoteBox);
        m_focus = new QLabel(remoteBox);
        m_error = new QLabel(QStringLiteral("None"), remoteBox);
        m_error->setWordWrap(true);
        statusForm->addRow(QStringLiteral("State"), m_state);
        statusForm->addRow(QStringLiteral("Build identity"), m_buildIdentity);
        statusForm->addRow(QStringLiteral("Deployment identity"), m_deploymentIdentity);
        statusForm->addRow(QStringLiteral("Platform"), m_platform);
        statusForm->addRow(QStringLiteral("Security"), m_security);
        statusForm->addRow(QStringLiteral("Configured listener"), m_configuredListener);
        statusForm->addRow(QStringLiteral("Effective listener"), m_endpoint);
        statusForm->addRow(QStringLiteral("Viewer endpoints"), m_viewerEndpoints);
        statusForm->addRow(QStringLiteral("Connection"), m_connection);
        statusForm->addRow(QStringLiteral("Connected clients"), m_clients);
        statusForm->addRow(QStringLiteral("Policy"), m_policy);
        statusForm->addRow(QStringLiteral("Target window"), m_focus);
        statusForm->addRow(QStringLiteral("Last error"), m_error);
        remoteLayout->addLayout(statusForm);

        auto *actions = new QHBoxLayout;
        m_startStop = new QPushButton(QStringLiteral("Start remote access"), remoteBox);
        m_input = new QCheckBox(QStringLiteral("Allow remote control"), remoteBox);
        m_input->setChecked(initialInputEnabled);
        actions->addWidget(m_startStop);
        actions->addWidget(m_input);
        actions->addStretch(1);
        remoteLayout->addLayout(actions);

        auto *connectionActions = new QHBoxLayout;
        auto *refreshEndpoints = new QPushButton(QStringLiteral("Refresh viewer endpoints"), remoteBox);
        m_copyEndpoints = new QPushButton(QStringLiteral("Copy viewer endpoints"), remoteBox);
        connectionActions->addWidget(refreshEndpoints);
        connectionActions->addWidget(m_copyEndpoints);
        connectionActions->addStretch(1);
        remoteLayout->addLayout(connectionActions);

        auto *policyNote = new QLabel(
            QStringLiteral("Changing remote-control policy while running performs an explicit "
                           "stop/apply/start cycle on the same RemoteAccess instance."),
            remoteBox);
        policyNote->setWordWrap(true);
        remoteLayout->addWidget(policyNote);

        m_focusWarning = new QLabel(
            QStringLiteral("Remote input is application-scoped. This HyRemoteTool window is not "
                           "the active OS window, so remote pointer/text input may not reactivate it. "
                           "Activate HyRemoteTool locally to restore reliable remote input."),
            remoteBox);
        m_focusWarning->setWordWrap(true);
        m_focusWarning->setVisible(false);
        remoteLayout->addWidget(m_focusWarning);

        auto *security = new QLabel(
            QStringLiteral("Security boundary: the current RFB correctness baseline uses "
                           "SecurityType None. The listener is unencrypted, so keep it on this LAN or another trusted, "
                           "explicitly protected network path; this example does not simulate "
                           "authentication or encryption."),
            remoteBox);
        security->setWordWrap(true);
        remoteLayout->addWidget(security);
        root->addWidget(remoteBox);

        auto *diagnosticsBox = new QGroupBox(QStringLiteral("Diagnostics"), content);
        auto *diagnosticsLayout = new QVBoxLayout(diagnosticsBox);
        m_diagnostics = new QPlainTextEdit(diagnosticsBox);
        m_diagnostics->setReadOnly(true);
        m_diagnostics->setLineWrapMode(QPlainTextEdit::NoWrap);
        m_diagnostics->setMaximumBlockCount(64);
        m_diagnostics->setMinimumHeight(150);
        diagnosticsLayout->addWidget(m_diagnostics);
        auto *diagnosticActions = new QHBoxLayout;
        auto *copyDiagnostics = new QPushButton(QStringLiteral("Copy diagnostic report"), diagnosticsBox);
        auto *saveDiagnostics = new QPushButton(QStringLiteral("Save diagnostic report"), diagnosticsBox);
        diagnosticActions->addWidget(copyDiagnostics);
        diagnosticActions->addWidget(saveDiagnostics);
        diagnosticActions->addStretch(1);
        diagnosticsLayout->addLayout(diagnosticActions);
        root->addWidget(diagnosticsBox);

        auto *activityBox = new QGroupBox(QStringLiteral("Activity"), content);
        auto *activityLayout = new QVBoxLayout(activityBox);
        m_activity = new QPlainTextEdit(activityBox);
        m_activity->setReadOnly(true);
        m_activity->setLineWrapMode(QPlainTextEdit::NoWrap);
        m_activity->setMaximumBlockCount(100);
        m_activity->setMinimumHeight(110);
        activityLayout->addWidget(m_activity);
        auto *clearActivity = new QPushButton(QStringLiteral("Clear activity"), activityBox);
        activityLayout->addWidget(clearActivity, 0, Qt::AlignLeft);
        root->addWidget(activityBox);

        auto *workBox = new QGroupBox(QStringLiteral("Local operator controls"), content);
        m_operatorControls = workBox;
        auto *workLayout = new QFormLayout(workBox);
        workLayout->setRowWrapPolicy(QFormLayout::WrapLongRows);
        auto *asset = new QLineEdit(QStringLiteral("Conveyor-01"), workBox);
        auto *speed = new QSpinBox(workBox);
        speed->setRange(0, 100);
        speed->setValue(42);
        auto *load = new QSlider(Qt::Horizontal, workBox);
        load->setRange(0, 100);
        load->setValue(58);
        auto *loadValue = new QProgressBar(workBox);
        loadValue->setRange(0, 100);
        loadValue->setValue(load->value());
        auto *maintenance = new QCheckBox(QStringLiteral("Maintenance mode"), workBox);
        auto *notes = new QPlainTextEdit(workBox);
        m_operatorNotes = notes;
        // QTextEdit's preferred width is only a hint: let the field shrink
        // when the form wraps at a compact native window width.
        auto notesPolicy = notes->sizePolicy();
        notesPolicy.setHorizontalPolicy(QSizePolicy::Ignored);
        notes->setSizePolicy(notesPolicy);
        notes->setPlainText(QStringLiteral("Operator notes remain editable locally while remote support is active."));
        notes->setMaximumBlockCount(20);

        workLayout->addRow(QStringLiteral("Asset"), asset);
        workLayout->addRow(QStringLiteral("Command setpoint"), speed);
        workLayout->addRow(QStringLiteral("Process load"), load);
        workLayout->addRow(QStringLiteral("Load indicator"), loadValue);
        workLayout->addRow(QString(), maintenance);
        workLayout->addRow(QStringLiteral("Notes"), notes);
        root->addWidget(workBox, 1);

        QObject::connect(load, &QSlider::valueChanged, loadValue, &QProgressBar::setValue);
        QObject::connect(m_startStop, &QPushButton::clicked, this, [this] { toggleRemoteAccess(); });
        QObject::connect(m_input, &QCheckBox::toggled, this, [this](bool enabled) {
            applyRemoteInputPolicy(enabled);
        });
        QObject::connect(refreshEndpoints, &QPushButton::clicked, this, [this] {
            refreshViewerEndpoints();
        });
        QObject::connect(m_copyEndpoints, &QPushButton::clicked, this, [this] {
            QApplication::clipboard()->setText(m_viewerEndpointList.join(QLatin1Char('\n')));
        });
        QObject::connect(copyDiagnostics, &QPushButton::clicked, this, [this] {
            QApplication::clipboard()->setText(m_remote.diagnosticReport());
        });
        QObject::connect(saveDiagnostics, &QPushButton::clicked, this, [this] {
            const QString path = QFileDialog::getSaveFileName(
                this,
                QStringLiteral("Save HyRemote diagnostic report"),
                QStringLiteral("hyremote-diagnostic.txt"),
                QStringLiteral("Text files (*.txt);;All files (*)"));
            if (!path.isEmpty() && !saveDiagnosticReport(path))
                m_error->setText(QStringLiteral("Could not save diagnostic report"));
        });
        QObject::connect(clearActivity, &QPushButton::clicked, m_activity, &QPlainTextEdit::clear);

        auto *timer = new QTimer(this);
        timer->setInterval(100);
        timer->setTimerType(Qt::CoarseTimer);
        QObject::connect(timer, &QTimer::timeout, this, [this] { refreshStatus(); });
        timer->start();
        appendActivity(QStringLiteral("Tool ready"));
        refreshViewerEndpoints();
        refreshStatus();
    }

    ~SupportWindow() override
    {
        m_remote.stop();
    }

    // Offscreen/desktop regression oracle: the native top-level window must
    // shrink, and scrolling must expose the entire bottom operator panel.
    bool verifyCompactLayout()
    {
        resize(480, 360);
        QApplication::processEvents();

        auto *vertical = m_scrollArea->verticalScrollBar();
        auto *horizontal = m_scrollArea->horizontalScrollBar();
        const bool compact = width() <= 480 && height() <= 360;
        const bool overflow = vertical && vertical->maximum() > 0;
        if (overflow) {
            // The remote-support forms can exceed a 480px viewport's
            // width, so verify actual two-axis navigation rather than
            // assuming a user can reach a widget beyond the right edge.
            vertical->setValue(vertical->maximum());
            if (horizontal)
                horizontal->setValue(horizontal->maximum());
            QApplication::processEvents();
        }
        // An operator panel's border could be visible while its actual
        // controls are clipped. Demand that the final editable Notes control
        // itself is fully reachable, not just a corner of the group box.
        const QRect notesRect(m_operatorNotes->mapTo(m_scrollArea->viewport(), QPoint()),
                              m_operatorNotes->size());
        const bool reachable = m_scrollArea->viewport()->rect().contains(notesRect);
        std::cout << (compact && overflow && reachable ? "TOOL_COMPACT_LAYOUT_PASS"
                                                     : "TOOL_COMPACT_LAYOUT_FAIL")
                  << " window=" << width() << 'x' << height()
                  << " scroll_max=" << (vertical ? vertical->maximum() : -1)
                  << " horizontal_max=" << (horizontal ? horizontal->maximum() : -1)
                  << " notes_rect=" << notesRect.x() << ',' << notesRect.y()
                  << ',' << notesRect.width() << 'x' << notesRect.height()
                  << " viewport=" << m_scrollArea->viewport()->width()
                  << 'x' << m_scrollArea->viewport()->height() << std::endl;
        return compact && overflow && reachable;
    }

    bool saveDiagnosticReport(const QString &path) const
    {
        if (path.isEmpty())
            return false;

        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;

        const QByteArray payload = m_remote.diagnosticReport().toUtf8();
        if (file.write(payload) != payload.size())
            return false;

        return file.commit();
    }

    bool startRemoteAccess()
    {
        appendActivity(QStringLiteral("Start requested"));
        m_remote.clearError();
        if (!m_remote.start()) {
            refreshStatus();
            appendActivity(QStringLiteral("Start failed"));
            return false;
        }
        // Preserve the pre-start observed count so refreshStatus() can record a viewer that
        // connected while the listener was becoming ready. Emit the explicit startup count only
        // when refreshStatus() did not already publish a real client-count transition.
        const std::size_t observedBeforeStart = m_lastReportedClientCount;
        refreshStatus();
        std::cout << "REMOTE_STARTED " << m_port << std::endl;
        if (m_lastReportedClientCount == observedBeforeStart)
            std::cout << "SHOWCASE_CLIENTS " << m_lastReportedClientCount << std::endl;
        return true;
    }

    void stopRemoteAccess()
    {
        appendActivity(QStringLiteral("Stop requested"));
        m_remote.stop();
        refreshStatus();
        std::cout << "SHOWCASE_CLIENTS " << m_remote.connectedClientCount() << std::endl;
        std::cout << "REMOTE_STOPPED" << std::endl;
    }

private:
    void appendActivity(const QString &message)
    {
        if (!m_activity || message.isEmpty())
            return;
        m_activity->appendPlainText(message);
        std::cout << "TOOL_ACTIVITY " << message.toStdString() << std::endl;
    }

    void toggleRemoteAccess()
    {
        const auto state = m_remote.state();
        if (state == HyRemote::RemoteAccessState::Running ||
            state == HyRemote::RemoteAccessState::Starting ||
            state == HyRemote::RemoteAccessState::Faulted) {
            stopRemoteAccess();
            return;
        }
        startRemoteAccess();
    }

    void applyRemoteInputPolicy(bool enabled)
    {
        const bool wasRunning = m_remote.state() == HyRemote::RemoteAccessState::Running;
        if (wasRunning) {
            appendActivity(QStringLiteral("Applying remote input policy; restarting remote access"));
            m_remote.stop();
            // Observe the stopped/client-zero facts before restarting, otherwise a policy
            // restart can hide the forced viewer disconnect from Activity.
            refreshStatus();
        }

        if (!m_remote.setRemoteInputEnabled(enabled)) {
            m_input->blockSignals(true);
            m_input->setChecked(m_remote.remoteInputEnabled());
            m_input->blockSignals(false);
            refreshStatus();
            appendActivity(QStringLiteral("Remote input policy change rejected"));
            if (wasRunning)
                startRemoteAccess();
            return;
        }

        std::cout << "REMOTE_INPUT " << (enabled ? "enabled" : "disabled") << std::endl;
        if (wasRunning)
            startRemoteAccess();
        refreshStatus();
    }

    void refreshViewerEndpoints()
    {
        m_viewerEndpointList = viewerEndpoints(m_port);
        m_viewerEndpoints->setText(m_viewerEndpointList.isEmpty()
                                       ? QStringLiteral("No non-loopback IPv4 address detected")
                                       : m_viewerEndpointList.join(QStringLiteral("\n")));
        m_copyEndpoints->setEnabled(!m_viewerEndpointList.isEmpty());
    }

    void refreshStatus()
    {
        const auto state = m_remote.state();
        m_state->setText(stateName(state));
        m_policy->setText(m_remote.remoteInputEnabled()
                              ? QStringLiteral("Remote view + control")
                              : QStringLiteral("View-only (safe default)"));

        const bool windowActive = isActiveWindow();
        m_focus->setText(windowActive ? QStringLiteral("Active OS window")
                                      : QStringLiteral("Not active"));
        m_focusWarning->setVisible(m_remote.remoteInputEnabled() && !windowActive);

        const std::size_t clientCount = m_remote.connectedClientCount();
        m_clients->setText(QString::number(static_cast<qulonglong>(clientCount)));
        if (state == HyRemote::RemoteAccessState::Running) {
            m_connection->setText(clientCount > 0
                                      ? QStringLiteral("Viewer connected")
                                      : QStringLiteral("Waiting for viewer"));
        } else if (state == HyRemote::RemoteAccessState::Starting) {
            m_connection->setText(QStringLiteral("Starting listener"));
        } else {
            m_connection->setText(QStringLiteral("Remote access stopped"));
        }
        if (clientCount != m_lastReportedClientCount) {
            const std::size_t previousClientCount = m_lastReportedClientCount;
            m_lastReportedClientCount = clientCount;
            std::cout << "SHOWCASE_CLIENTS " << clientCount << std::endl;
            if (previousClientCount == 0 && clientCount > 0)
                appendActivity(QStringLiteral("Viewer connected"));
            else if (previousClientCount > 0 && clientCount == 0)
                appendActivity(QStringLiteral("Viewer disconnected"));
            else
                appendActivity(QStringLiteral("Connected clients: %1")
                                   .arg(static_cast<qulonglong>(clientCount)));
        }

        const bool active = state == HyRemote::RemoteAccessState::Running ||
                            state == HyRemote::RemoteAccessState::Starting;
        m_startStop->setText(active ? QStringLiteral("Stop remote access")
                                    : QStringLiteral("Start remote access"));

        QString errorText = QStringLiteral("None");
        int errorCode = -1;
        if (const auto error = m_remote.lastError()) {
            errorText = error->message.isEmpty() ? QStringLiteral("Runtime failure") : error->message;
            errorCode = static_cast<int>(error->code);
        }
        m_error->setText(errorText);

        const QString diagnosticTrigger =
            QStringLiteral("%1|%2|%3|%4|%5")
                .arg(static_cast<int>(state))
                .arg(static_cast<qulonglong>(clientCount))
                .arg(m_remote.remoteInputEnabled() ? 1 : 0)
                .arg(errorCode)
                .arg(errorText);
        if (diagnosticTrigger != m_lastDiagnosticTrigger) {
            m_lastDiagnosticTrigger = diagnosticTrigger;
            const QString report = m_remote.diagnosticReport();
            m_diagnostics->setPlainText(report);
            m_buildIdentity->setText(
                diagnosticValue(report, QStringLiteral("BUILD_IDENTITY")));
            m_deploymentIdentity->setText(
                diagnosticValue(report, QStringLiteral("DEPLOYMENT_IDENTITY")));
            m_platform->setText(
                QStringLiteral("Qt %1 / %2 / %3")
                    .arg(diagnosticValue(report, QStringLiteral("QT_VERSION")),
                         diagnosticValue(report, QStringLiteral("OS")),
                         diagnosticValue(report, QStringLiteral("ARCH"))));
            m_security->setText(
                QStringLiteral("%1 (%2)")
                    .arg(diagnosticValue(report, QStringLiteral("SECURITY_PROFILE")),
                         diagnosticValue(report, QStringLiteral("SECURITY_ENABLED"))));
            m_configuredListener->setText(
                diagnosticValue(report, QStringLiteral("LISTENER_CONFIGURED")));
            const QString effectiveListener =
                diagnosticValue(report, QStringLiteral("LISTENER_EFFECTIVE"));
            m_endpoint->setText(effectiveListener);

            const QString runtimeState = diagnosticValue(report, QStringLiteral("STATE"));
            if (runtimeState != m_lastActivityRuntimeState) {
                m_lastActivityRuntimeState = runtimeState;
                appendActivity(QStringLiteral("Runtime state: %1").arg(runtimeState));
            }
            if (effectiveListener != m_lastActivityEffectiveListener) {
                m_lastActivityEffectiveListener = effectiveListener;
                if (effectiveListener != QStringLiteral("none")
                    && effectiveListener != QStringLiteral("unknown")) {
                    appendActivity(QStringLiteral("Listener ready: %1").arg(effectiveListener));
                }
            }

            const QString remoteInput = diagnosticValue(report, QStringLiteral("REMOTE_INPUT"));
            if (remoteInput != m_lastActivityRemoteInput) {
                m_lastActivityRemoteInput = remoteInput;
                appendActivity(QStringLiteral("Remote input: %1")
                                   .arg(remoteInput == QStringLiteral("true")
                                            ? QStringLiteral("enabled")
                                            : QStringLiteral("disabled")));
            }

            const QString errorCode = diagnosticValue(report, QStringLiteral("LAST_ERROR_CODE"));
            const QString errorMessage = diagnosticValue(report, QStringLiteral("LAST_ERROR_MESSAGE"));
            const QString errorKey = errorCode + QLatin1Char('|') + errorMessage;
            if (errorKey != m_lastActivityError) {
                const bool hadError = !m_lastActivityError.isEmpty()
                                      && !m_lastActivityError.startsWith(QStringLiteral("none|"));
                m_lastActivityError = errorKey;
                if (errorCode != QStringLiteral("none") && errorCode != QStringLiteral("unknown"))
                    appendActivity(QStringLiteral("Error %1: %2").arg(errorCode, errorMessage));
                else if (hadError)
                    appendActivity(QStringLiteral("Last error cleared"));
            }

            refreshViewerEndpoints();
        }
    }

    HyRemote::RemoteAccess m_remote;
    quint16 m_port = 5921;
    std::size_t m_lastReportedClientCount = 0;
    QString m_lastDiagnosticTrigger;
    QString m_lastActivityRuntimeState;
    QString m_lastActivityEffectiveListener;
    QString m_lastActivityRemoteInput;
    QString m_lastActivityError;
    QLabel *m_state = nullptr;
    QLabel *m_buildIdentity = nullptr;
    QLabel *m_deploymentIdentity = nullptr;
    QLabel *m_platform = nullptr;
    QLabel *m_security = nullptr;
    QLabel *m_configuredListener = nullptr;
    QLabel *m_endpoint = nullptr;
    QLabel *m_viewerEndpoints = nullptr;
    QLabel *m_connection = nullptr;
    QLabel *m_clients = nullptr;
    QLabel *m_policy = nullptr;
    QLabel *m_focus = nullptr;
    QLabel *m_focusWarning = nullptr;
    QLabel *m_error = nullptr;
    QPlainTextEdit *m_diagnostics = nullptr;
    QPlainTextEdit *m_activity = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    QWidget *m_operatorControls = nullptr;
    QPlainTextEdit *m_operatorNotes = nullptr;
    QPushButton *m_startStop = nullptr;
    QPushButton *m_copyEndpoints = nullptr;
    QCheckBox *m_input = nullptr;
    QStringList m_viewerEndpointList;
};

}  // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("HyRemoteTool"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("HyRemote host-side product experience and validation tool"));
    parser.addHelpOption();
    QCommandLineOption portOption(QStringList{QStringLiteral("p"), QStringLiteral("port")},
                                  QStringLiteral("Listener port the viewer connects to."),
                                  QStringLiteral("port"),
                                  QStringLiteral("5921"));
    QCommandLineOption inputOption(QStringLiteral("remote-input"),
                                   QStringLiteral("Enable remote input when remote access starts."));
    QCommandLineOption autoStartOption(QStringLiteral("auto-start"),
                                       QStringLiteral("Explicit test/acceptance helper: start after the local window is shown."));
    QCommandLineOption secondsOption(QStringLiteral("test-seconds"),
                                     QStringLiteral("Exit after N seconds (CI/product-fit helper)."),
                                     QStringLiteral("seconds"),
                                     QStringLiteral("0"));
    QCommandLineOption compactLayoutOption(
        QStringLiteral("verify-compact-layout"),
        QStringLiteral("CI-only check: compact native window and bottom-panel scrolling."));
    parser.addOption(portOption);
    parser.addOption(inputOption);
    QCommandLineOption diagnosticReportFileOption(
        QStringLiteral("diagnostic-report-file"),
        QStringLiteral("Write one bounded public diagnostic report after startup handling."),
        QStringLiteral("path"));
    parser.addOption(autoStartOption);
    parser.addOption(secondsOption);
    parser.addOption(compactLayoutOption);
    parser.addOption(diagnosticReportFileOption);
    parser.process(app);

    const int parsedPort = readPositiveInt(parser, portOption, 5921);
    if (parsedPort > 65535) {
        std::cerr << "invalid port" << std::endl;
        return 64;
    }

    const int testSeconds = readPositiveInt(parser, secondsOption, 0);
    SupportWindow window(static_cast<quint16>(parsedPort), parser.isSet(inputOption));
    window.show();

    if (parser.isSet(compactLayoutOption)) {
        // Keep this isolated from --auto-start: a UI layout smoke must not
        // open a listener, change the input policy or require a VNC viewer.
        QTimer::singleShot(0, &window, [&window, &app] {
            app.exit(window.verifyCompactLayout() ? 0 : 1);
        });
        return app.exec();
    }

    const bool autoStart = parser.isSet(autoStartOption);
    const QString diagnosticReportFile = parser.value(diagnosticReportFileOption);
    if (autoStart || !diagnosticReportFile.isEmpty()) {
        QTimer::singleShot(0, &window, [&window, autoStart, diagnosticReportFile] {
            if (autoStart && !window.startRemoteAccess())
                std::cerr << "START_FAILED" << std::endl;

            if (!diagnosticReportFile.isEmpty()) {
                if (window.saveDiagnosticReport(diagnosticReportFile))
                    std::cout << "DIAGNOSTIC_SAVED " << diagnosticReportFile.toStdString() << std::endl;
                else
                    std::cerr << "DIAGNOSTIC_SAVE_FAILED " << diagnosticReportFile.toStdString() << std::endl;
            }
        });
    }

    if (testSeconds > 0)
        QTimer::singleShot(testSeconds * 1000, &app, &QCoreApplication::quit);

    return app.exec();
}
