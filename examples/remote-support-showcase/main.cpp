#include <HyRemote/RemoteAccess.h>

#include <QApplication>
#include <QCheckBox>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHostAddress>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
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

class SupportWindow final : public QWidget
{
public:
    explicit SupportWindow(quint16 port, bool initialInputEnabled, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_remote(this)
        , m_port(port)
    {
        setWindowTitle(QStringLiteral("HyRemote Remote Support Showcase"));
        resize(780, 580);

        m_remote.setPort(port);
        m_remote.setRemoteInputEnabled(initialInputEnabled);

        auto *root = new QVBoxLayout(this);

        auto *heading = new QLabel(QStringLiteral("HyRemote · Remote Support Showcase"), this);
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
        m_endpoint = new QLabel(QStringLiteral("0.0.0.0:%1 (this host's IPv4 interfaces; trusted LAN only)").arg(port), remoteBox);
        m_clients = new QLabel(QStringLiteral("0"), remoteBox);
        m_policy = new QLabel(remoteBox);
        m_error = new QLabel(QStringLiteral("None"), remoteBox);
        m_error->setWordWrap(true);
        statusForm->addRow(QStringLiteral("State"), m_state);
        statusForm->addRow(QStringLiteral("Listener"), m_endpoint);
        statusForm->addRow(QStringLiteral("Connected clients"), m_clients);
        statusForm->addRow(QStringLiteral("Policy"), m_policy);
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

        auto *policyNote = new QLabel(
            QStringLiteral("Changing remote-control policy while running performs an explicit "
                           "stop/apply/start cycle on the same RemoteAccess instance."),
            remoteBox);
        policyNote->setWordWrap(true);
        remoteLayout->addWidget(policyNote);

        auto *security = new QLabel(
            QStringLiteral("Security boundary: the current RFB correctness baseline uses "
                           "SecurityType None. Keep the listener on loopback or another trusted, "
                           "explicitly protected network path; this example does not simulate "
                           "authentication or encryption."),
            remoteBox);
        security->setWordWrap(true);
        remoteLayout->addWidget(security);
        root->addWidget(remoteBox);

        auto *workBox = new QGroupBox(QStringLiteral("Local operator controls"), this);
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
        auto *notes = new QPlainTextEdit(workBox);
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

        auto *timer = new QTimer(this);
        timer->setInterval(100);
        timer->setTimerType(Qt::CoarseTimer);
        QObject::connect(timer, &QTimer::timeout, this, [this] { refreshStatus(); });
        timer->start();
        refreshStatus();
    }

    ~SupportWindow() override
    {
        m_remote.stop();
    }

    bool startRemoteAccess()
    {
        m_remote.clearError();
        if (!m_remote.start()) {
            refreshStatus();
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
        if (wasRunning)
            m_remote.stop();

        if (!m_remote.setRemoteInputEnabled(enabled)) {
            m_input->blockSignals(true);
            m_input->setChecked(m_remote.remoteInputEnabled());
            m_input->blockSignals(false);
            refreshStatus();
            if (wasRunning)
                startRemoteAccess();
            return;
        }

        std::cout << "REMOTE_INPUT " << (enabled ? "enabled" : "disabled") << std::endl;
        if (wasRunning)
            startRemoteAccess();
        refreshStatus();
    }

    void refreshStatus()
    {
        const auto state = m_remote.state();
        m_state->setText(stateName(state));
        m_policy->setText(m_remote.remoteInputEnabled()
                              ? QStringLiteral("Remote view + control")
                              : QStringLiteral("View-only (safe default)"));

        const std::size_t clientCount = m_remote.connectedClientCount();
        m_clients->setText(QString::number(static_cast<qulonglong>(clientCount)));
        if (clientCount != m_lastReportedClientCount) {
            m_lastReportedClientCount = clientCount;
            std::cout << "SHOWCASE_CLIENTS " << clientCount << std::endl;
        }

        const bool active = state == HyRemote::RemoteAccessState::Running ||
                            state == HyRemote::RemoteAccessState::Starting;
        m_startStop->setText(active ? QStringLiteral("Stop remote access")
                                    : QStringLiteral("Start remote access"));

        if (const auto error = m_remote.lastError())
            m_error->setText(error->message.isEmpty() ? QStringLiteral("Runtime failure") : error->message);
        else
            m_error->setText(QStringLiteral("None"));
    }

    HyRemote::RemoteAccess m_remote;
    quint16 m_port = 5921;
    std::size_t m_lastReportedClientCount = 0;
    QLabel *m_state = nullptr;
    QLabel *m_endpoint = nullptr;
    QLabel *m_clients = nullptr;
    QLabel *m_policy = nullptr;
    QLabel *m_error = nullptr;
    QPushButton *m_startStop = nullptr;
    QCheckBox *m_input = nullptr;
};

}  // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("HyRemote Remote Support Showcase"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Production-like HyRemote local + remote support workflow"));
    parser.addHelpOption();
    QCommandLineOption portOption(QStringList{QStringLiteral("p"), QStringLiteral("port")},
                                  QStringLiteral("Loopback VNC port."),
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
