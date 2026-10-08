#include <HyRemote/RemoteAccess.h>

#include <QAbstractSocket>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDateTime>
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
#include <QResizeEvent>
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
        resize(780, 580);

        m_remote.setPort(port);
        m_remote.setRemoteInputEnabled(initialInputEnabled);

        auto *root = new QVBoxLayout(this);

        auto *heading = new QLabel(QStringLiteral("HyRemoteTool"), this);
        QFont headingFont = heading->font();
        headingFont.setPointSize(16);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        root->addWidget(heading);

        auto *intro = new QLabel(
            QStringLiteral("The local application remains authoritative. Remote access is off until "
                           "the local operator explicitly starts it."),
            this);
        intro->setWordWrap(true);
        root->addWidget(intro);

        auto *remoteBox = new QGroupBox(QStringLiteral("Remote support"), this);
        auto *remoteLayout = new QVBoxLayout(remoteBox);

        auto *statusForm = new QFormLayout;
        m_state = new QLabel(remoteBox);
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

        auto *diagnosticsBox = new QGroupBox(QStringLiteral("Diagnostics"), this);
        auto *diagnosticsLayout = new QVBoxLayout(diagnosticsBox);
        m_diagnostics = new QPlainTextEdit(diagnosticsBox);
        m_diagnostics->setReadOnly(true);
        m_diagnostics->setLineWrapMode(QPlainTextEdit::NoWrap);
        m_diagnostics->setMaximumBlockCount(64);
        m_diagnostics->setMinimumHeight(150);
        diagnosticsLayout->addWidget(m_diagnostics);
        auto *copyDiagnostics = new QPushButton(QStringLiteral("Copy diagnostic report"), diagnosticsBox);
        diagnosticsLayout->addWidget(copyDiagnostics, 0, Qt::AlignLeft);
        root->addWidget(diagnosticsBox);

        auto *activityBox = new QGroupBox(QStringLiteral("Activity"), this);
        auto *activityLayout = new QVBoxLayout(activityBox);
        m_activity = new QPlainTextEdit(activityBox);
        m_activity->setReadOnly(true);
        m_activity->setMaximumBlockCount(100);
        m_activity->setMinimumHeight(120);
        activityLayout->addWidget(m_activity);
        root->addWidget(activityBox);

        auto *workBox = new QGroupBox(QStringLiteral("Remote-control test workspace"), this);
        auto *workLayout = new QFormLayout(workBox);
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
        auto *commandButton = new QPushButton(QStringLiteral("Activate command"), workBox);
        auto *commandResult = new QLabel(QStringLiteral("Not activated"), workBox);
        m_workspaceSize = new QLabel(workBox);
        auto *notes = new QPlainTextEdit(workBox);
        notes->setPlainText(QStringLiteral("Operator notes remain editable locally while remote support is active."));
        notes->setMaximumBlockCount(20);
        speed->setToolTip(QStringLiteral("Use the mouse wheel while this numeric field is focused."));
        m_workspaceSize->setText(
            QStringLiteral("%1 x %2").arg(width()).arg(height()));

        workLayout->addRow(QStringLiteral("Asset"), asset);
        workLayout->addRow(QStringLiteral("Command setpoint"), speed);
        workLayout->addRow(QStringLiteral("Process load"), load);
        workLayout->addRow(QStringLiteral("Load indicator"), loadValue);
        workLayout->addRow(QString(), maintenance);
        workLayout->addRow(QStringLiteral("Push button"), commandButton);
        workLayout->addRow(QStringLiteral("Button result"), commandResult);
        workLayout->addRow(QStringLiteral("Window size"), m_workspaceSize);
        workLayout->addRow(QStringLiteral("Notes"), notes);
        root->addWidget(workBox, 1);

        QObject::connect(load, &QSlider::valueChanged, loadValue, &QProgressBar::setValue);
        QObject::connect(commandButton, &QPushButton::clicked, this, [this, commandResult] {
            ++m_commandActivationCount;
            commandResult->setText(
                QStringLiteral("Activated %1 time(s)").arg(m_commandActivationCount));
            appendActivity(QStringLiteral("Workspace push button activated"));
        });
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

        auto *timer = new QTimer(this);
        timer->setInterval(100);
        timer->setTimerType(Qt::CoarseTimer);
        QObject::connect(timer, &QTimer::timeout, this, [this] { refreshStatus(); });
        timer->start();
        refreshViewerEndpoints();
        refreshStatus();
    }

    ~SupportWindow() override
    {
        m_remote.stop();
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        if (m_workspaceSize) {
            m_workspaceSize->setText(
                QStringLiteral("%1 x %2").arg(event->size().width()).arg(event->size().height()));
        }
    }

public:
    bool startRemoteAccess(bool recordRequest = true)
    {
        if (recordRequest)
            appendActivity(QStringLiteral("Start remote access requested"));
        m_remote.clearError();
        if (!m_remote.start()) {
            refreshStatus();
            appendActivity(QStringLiteral("Remote access start failed"));
            return false;
        }
        m_lastReportedClientCount = m_remote.connectedClientCount();
        refreshStatus();
        std::cout << "REMOTE_STARTED " << m_port << std::endl;
        std::cout << "SHOWCASE_CLIENTS " << m_lastReportedClientCount << std::endl;
        return true;
    }

    void stopRemoteAccess()
    {
        appendActivity(QStringLiteral("Stop remote access requested"));
        m_remote.stop();
        refreshStatus();
        std::cout << "SHOWCASE_CLIENTS " << m_remote.connectedClientCount() << std::endl;
        std::cout << "REMOTE_STOPPED" << std::endl;
    }

private:
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
            appendActivity(QStringLiteral("Applying remote-input policy requires restart"));
            m_remote.stop();
            refreshStatus();
        }

        if (!m_remote.setRemoteInputEnabled(enabled)) {
            m_input->blockSignals(true);
            m_input->setChecked(m_remote.remoteInputEnabled());
            m_input->blockSignals(false);
            refreshStatus();
            if (wasRunning) {
                appendActivity(
                    QStringLiteral("Remote-input policy change rejected; restoring remote access"));
                startRemoteAccess(false);
            }
            return;
        }

        appendActivity(enabled ? QStringLiteral("Remote input enabled")
                               : QStringLiteral("Remote input disabled"));
        std::cout << "REMOTE_INPUT " << (enabled ? "enabled" : "disabled") << std::endl;
        if (wasRunning) {
            appendActivity(QStringLiteral("Restarting remote access after policy change"));
            startRemoteAccess(false);
        }
        refreshStatus();
    }

    void appendActivity(const QString &message)
    {
        if (!m_activity)
            return;
        const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
        m_activity->appendPlainText(QStringLiteral("[%1] %2").arg(timestamp, message));
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
        if (!m_hasActivityState || state != m_lastActivityState) {
            m_hasActivityState = true;
            m_lastActivityState = state;
            appendActivity(QStringLiteral("Runtime state: %1").arg(stateName(state)));
        }
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
            if (clientCount > previousClientCount)
                appendActivity(QStringLiteral("Viewer connected (%1 client(s))")
                                   .arg(static_cast<qulonglong>(clientCount)));
            else if (clientCount == 0)
                appendActivity(QStringLiteral("Viewer disconnected"));
            else
                appendActivity(QStringLiteral("Viewer count changed to %1")
                                   .arg(static_cast<qulonglong>(clientCount)));
            std::cout << "SHOWCASE_CLIENTS " << clientCount << std::endl;
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
        if (errorText != m_lastActivityError) {
            m_lastActivityError = errorText;
            if (errorText != QStringLiteral("None"))
                appendActivity(QStringLiteral("Error: %1").arg(errorText));
        }

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
            m_configuredListener->setText(
                diagnosticValue(report, QStringLiteral("LISTENER_CONFIGURED")));
            m_endpoint->setText(
                diagnosticValue(report, QStringLiteral("LISTENER_EFFECTIVE")));
            refreshViewerEndpoints();
        }
    }

    HyRemote::RemoteAccess m_remote;
    quint16 m_port = 5921;
    std::size_t m_lastReportedClientCount = 0;
    int m_commandActivationCount = 0;
    QString m_lastDiagnosticTrigger;
    QString m_lastActivityError = QStringLiteral("None");
    HyRemote::RemoteAccessState m_lastActivityState = HyRemote::RemoteAccessState::Stopped;
    bool m_hasActivityState = false;
    QLabel *m_state = nullptr;
    QLabel *m_configuredListener = nullptr;
    QLabel *m_endpoint = nullptr;
    QLabel *m_viewerEndpoints = nullptr;
    QLabel *m_connection = nullptr;
    QLabel *m_clients = nullptr;
    QLabel *m_policy = nullptr;
    QLabel *m_focus = nullptr;
    QLabel *m_focusWarning = nullptr;
    QLabel *m_error = nullptr;
    QLabel *m_workspaceSize = nullptr;
    QPlainTextEdit *m_diagnostics = nullptr;
    QPlainTextEdit *m_activity = nullptr;
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
    parser.addOption(portOption);
    parser.addOption(inputOption);
    parser.addOption(autoStartOption);
    parser.addOption(secondsOption);
    parser.process(app);

    const int parsedPort = readPositiveInt(parser, portOption, 5921);
    if (parsedPort > 65535) {
        std::cerr << "invalid port" << std::endl;
        return 64;
    }

    const int testSeconds = readPositiveInt(parser, secondsOption, 0);
    SupportWindow window(static_cast<quint16>(parsedPort), parser.isSet(inputOption));
    window.show();

    if (parser.isSet(autoStartOption)) {
        QTimer::singleShot(0, &window, [&window] {
            if (!window.startRemoteAccess())
                std::cerr << "START_FAILED" << std::endl;
        });
    }

    if (testSeconds > 0)
        QTimer::singleShot(testSeconds * 1000, &app, &QCoreApplication::quit);

    return app.exec();
}
