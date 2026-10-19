#include <HyRemote/RemoteAccess.h>

#include <QAbstractSocket>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDateTime>
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

QStringList ipv4Addresses()
{
    QStringList addresses;
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

            const QString text = address.toString();
            if (!addresses.contains(text))
                addresses.push_back(text);
        }
    }
    addresses.sort(Qt::CaseInsensitive);
    return addresses;
}

QStringList viewerEndpoints(quint16 port)
{
    QStringList endpoints;
    for (const QString &address : ipv4Addresses())
        endpoints.push_back(QStringLiteral("%1:%2").arg(address).arg(port));
    endpoints.sort(Qt::CaseInsensitive);
    return endpoints;
}

struct InterfaceChoice
{
    QString display;
    QString identity;
};

QList<InterfaceChoice> ipv4Interfaces()
{
    QList<InterfaceChoice> choices;
    for (const QNetworkInterface &interface : QNetworkInterface::allInterfaces()) {
        const auto flags = interface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp)
            || !flags.testFlag(QNetworkInterface::IsRunning))
            continue;

        bool hasIpv4 = false;
        for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
            const QHostAddress address = entry.ip();
            if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback()
                && !address.isLinkLocal() && !address.isNull()) {
                hasIpv4 = true;
                break;
            }
        }
        if (!hasIpv4)
            continue;

        const QString readable = interface.humanReadableName().trimmed();
        choices.push_back(InterfaceChoice{
            readable.isEmpty() ? interface.name()
                               : QStringLiteral("%1 (%2)").arg(readable, interface.name()),
            interface.name()});
    }
    std::sort(choices.begin(), choices.end(), [](const InterfaceChoice &left,
                                                 const InterfaceChoice &right) {
        return left.display.compare(right.display, Qt::CaseInsensitive) < 0;
    });
    return choices;
}

enum class ListenMode {
    AnyIpv4 = 0,
    Address = 1,
    Interface = 2,
};

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

        // Connection and control first: the operator must see whether remote
        // access is on, and be able to stop it, without scrolling.
        auto *remoteBox = new QGroupBox(QStringLiteral("Remote support"), content);
        auto *remoteLayout = new QVBoxLayout(remoteBox);

        auto *summaryForm = new QFormLayout;
        // On narrow/high-DPI desktops, break labels onto their own row
        // instead of forcing the content widget wider than the viewport.
        summaryForm->setRowWrapPolicy(QFormLayout::WrapLongRows);
        m_state = new QLabel(remoteBox);
        m_connection = new QLabel(remoteBox);
        m_clients = new QLabel(QStringLiteral("0"), remoteBox);
        m_endpoint = new QLabel(remoteBox);
        m_endpoint->setWordWrap(true);
        m_endpoint->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_configuredListener = new QLabel(remoteBox);
        m_configuredListener->setWordWrap(true);
        m_policy = new QLabel(remoteBox);
        summaryForm->addRow(QStringLiteral("State"), m_state);
        summaryForm->addRow(QStringLiteral("Connection"), m_connection);
        summaryForm->addRow(QStringLiteral("Connected clients"), m_clients);
        summaryForm->addRow(QStringLiteral("Effective listener"), m_endpoint);
        summaryForm->addRow(QStringLiteral("Configured listener"), m_configuredListener);
        summaryForm->addRow(QStringLiteral("Policy"), m_policy);
        remoteLayout->addLayout(summaryForm);

        auto *actions = new QHBoxLayout;
        m_startStop = new QPushButton(QStringLiteral("Start remote access"), remoteBox);
        m_startStop->setObjectName(QStringLiteral("startStop"));
        m_input = new QCheckBox(QStringLiteral("Allow remote control"), remoteBox);
        m_input->setObjectName(QStringLiteral("remoteInput"));
        m_input->setChecked(initialInputEnabled);
        actions->addWidget(m_startStop);
        actions->addWidget(m_input);
        actions->addStretch(1);
        remoteLayout->addLayout(actions);

        auto *listenerBox = new QGroupBox(QStringLiteral("Listener"), remoteBox);
        auto *listenerForm = new QFormLayout(listenerBox);
        listenerForm->setRowWrapPolicy(QFormLayout::WrapLongRows);
        m_portEditor = new QSpinBox(listenerBox);
        m_portEditor->setObjectName(QStringLiteral("portEditor"));
        m_portEditor->setRange(1, 65535);
        m_portEditor->setValue(m_port);
        m_listenMode = new QComboBox(listenerBox);
        m_listenMode->setObjectName(QStringLiteral("listenMode"));
        m_listenMode->addItem(QStringLiteral("All IPv4 interfaces"),
                              static_cast<int>(ListenMode::AnyIpv4));
        m_listenMode->addItem(QStringLiteral("Specific IPv4 address"),
                              static_cast<int>(ListenMode::Address));
        m_listenMode->addItem(QStringLiteral("Network interface"),
                              static_cast<int>(ListenMode::Interface));
        m_listenTarget = new QComboBox(listenerBox);
        m_listenTarget->setObjectName(QStringLiteral("listenTarget"));
        m_applyListener = new QPushButton(QStringLiteral("Apply listener settings"), listenerBox);
        m_applyListener->setObjectName(QStringLiteral("applyListener"));
        listenerForm->addRow(QStringLiteral("Port"), m_portEditor);
        listenerForm->addRow(QStringLiteral("Listen on"), m_listenMode);
        listenerForm->addRow(QStringLiteral("Address / interface"), m_listenTarget);
        listenerForm->addRow(QString(), m_applyListener);
        m_listenerHint = new QLabel(
            QStringLiteral("Stop remote access before changing the listener. The configured "
                           "value takes effect on the next start."),
            listenerBox);
        m_listenerHint->setWordWrap(true);
        listenerForm->addRow(QString(), m_listenerHint);
        remoteLayout->addWidget(listenerBox);

        auto *endpointForm = new QFormLayout;
        endpointForm->setRowWrapPolicy(QFormLayout::WrapLongRows);
        auto *endpointRow = new QWidget(remoteBox);
        auto *endpointLayout = new QHBoxLayout(endpointRow);
        endpointLayout->setContentsMargins(0, 0, 0, 0);
        m_endpointChoices = new QComboBox(endpointRow);
        m_endpointChoices->setObjectName(QStringLiteral("viewerAddress"));
        m_endpointChoices->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_copySelectedEndpoint = new QPushButton(QStringLiteral("Copy selected"), endpointRow);
        m_copyEndpoints = new QPushButton(QStringLiteral("Copy all"), endpointRow);
        auto *refreshEndpoints = new QPushButton(QStringLiteral("Refresh"), endpointRow);
        endpointLayout->addWidget(m_endpointChoices, 1);
        endpointLayout->addWidget(m_copySelectedEndpoint);
        endpointLayout->addWidget(m_copyEndpoints);
        endpointLayout->addWidget(refreshEndpoints);
        endpointForm->addRow(QStringLiteral("Viewer endpoint"), endpointRow);
        remoteLayout->addLayout(endpointForm);

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

        auto *identityBox = new QGroupBox(QStringLiteral("Build and environment"), content);
        auto *identityForm = new QFormLayout(identityBox);
        identityForm->setRowWrapPolicy(QFormLayout::WrapLongRows);
        m_buildIdentity = new QLabel(identityBox);
        m_buildIdentity->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_buildIdentity->setWordWrap(true);
        auto buildIdentityPolicy = m_buildIdentity->sizePolicy();
        buildIdentityPolicy.setHorizontalPolicy(QSizePolicy::Ignored);
        buildIdentityPolicy.setHeightForWidth(true);
        m_buildIdentity->setSizePolicy(buildIdentityPolicy);
        m_deploymentIdentity = new QLabel(identityBox);
        m_deploymentIdentity->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_deploymentIdentity->setWordWrap(true);
        auto deploymentIdentityPolicy = m_deploymentIdentity->sizePolicy();
        deploymentIdentityPolicy.setHorizontalPolicy(QSizePolicy::Ignored);
        deploymentIdentityPolicy.setHeightForWidth(true);
        m_deploymentIdentity->setSizePolicy(deploymentIdentityPolicy);
        m_platform = new QLabel(identityBox);
        m_security = new QLabel(identityBox);
        m_focus = new QLabel(identityBox);
        m_error = new QLabel(QStringLiteral("None"), identityBox);
        m_error->setWordWrap(true);
        identityForm->addRow(QStringLiteral("Build identity"), m_buildIdentity);
        identityForm->addRow(QStringLiteral("Deployment identity"), m_deploymentIdentity);
        identityForm->addRow(QStringLiteral("Platform"), m_platform);
        identityForm->addRow(QStringLiteral("Security profile"), m_security);
        identityForm->addRow(QStringLiteral("Target window"), m_focus);
        identityForm->addRow(QStringLiteral("Last error"), m_error);
        root->addWidget(identityBox);

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
        m_diagnosticStatus = new QLabel(QStringLiteral("No export yet."), diagnosticsBox);
        m_diagnosticStatus->setObjectName(QStringLiteral("diagnosticStatus"));
        m_diagnosticStatus->setWordWrap(true);
        diagnosticsLayout->addWidget(m_diagnosticStatus);
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
        // A style's ExpandingFieldsGrow default varies across platforms.
        // Keep editors expanding even when rows wrap on narrow desktops.
        workLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
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
        auto *testClick = new QPushButton(QStringLiteral("Send test click"), workBox);
        auto *testClickCount = new QLabel(QStringLiteral("0 clicks"), workBox);
        auto *notes = new QPlainTextEdit(workBox);
        m_operatorNotes = notes;
        // Retain QPlainTextEdit's normal Expanding width policy: an Ignored
        // policy can collapse the editor to zero width with some Qt styles.
        notes->setPlainText(QStringLiteral("Operator notes remain editable locally while remote support is active."));
        notes->setMaximumBlockCount(20);

        workLayout->addRow(QStringLiteral("Asset"), asset);
        workLayout->addRow(QStringLiteral("Command setpoint"), speed);
        workLayout->addRow(QStringLiteral("Process load"), load);
        workLayout->addRow(QStringLiteral("Load indicator"), loadValue);
        workLayout->addRow(QString(), maintenance);
        workLayout->addRow(QStringLiteral("Pointer test"), testClick);
        workLayout->addRow(QStringLiteral("Test clicks"), testClickCount);
        workLayout->addRow(QStringLiteral("Notes"), notes);
        root->addWidget(workBox, 1);

        auto *helpBox = new QGroupBox(QStringLiteral("Documentation"), content);
        auto *helpLayout = new QVBoxLayout(helpBox);
        auto *helpLinks = new QLabel(
            QStringLiteral("<a href=\"https://github.com/skawu/HyRemote/blob/develop/docs/guide/"
                           "integrate-your-project.md\">Integration guide</a> &middot; "
                           "<a href=\"https://github.com/skawu/HyRemote/blob/develop/docs/guide/"
                           "viewer-connection.md\">Viewer connection</a> &middot; "
                           "<a href=\"https://github.com/skawu/HyRemote/blob/develop/docs/guide/"
                           "troubleshooting.md\">Troubleshooting</a> &middot; "
                           "<a href=\"https://github.com/skawu/HyRemote/blob/develop/docs/"
                           "security-model.md\">Security boundary</a>"),
            helpBox);
        helpLinks->setOpenExternalLinks(true);
        helpLinks->setWordWrap(true);
        helpLayout->addWidget(helpLinks);
        root->addWidget(helpBox);

        QObject::connect(load, &QSlider::valueChanged, loadValue, &QProgressBar::setValue);
        QObject::connect(testClick, &QPushButton::clicked, this, [this, testClickCount] {
            ++m_testClicks;
            testClickCount->setText(QStringLiteral("%1 clicks").arg(m_testClicks));
        });
        QObject::connect(m_startStop, &QPushButton::clicked, this, [this] { toggleRemoteAccess(); });
        QObject::connect(m_input, &QCheckBox::toggled, this, [this](bool enabled) {
            applyRemoteInputPolicy(enabled);
        });
        QObject::connect(m_listenMode, &QComboBox::currentIndexChanged, this, [this] {
            refreshListenerTargets();
        });
        QObject::connect(m_applyListener, &QPushButton::clicked, this, [this] {
            applyListenerSettings();
        });
        QObject::connect(refreshEndpoints, &QPushButton::clicked, this, [this] {
            refreshViewerEndpoints();
        });
        QObject::connect(m_endpointChoices, &QComboBox::currentIndexChanged, this, [this] {
            m_copySelectedEndpoint->setEnabled(m_endpointChoices->currentIndex() >= 0
                                               && !m_endpointChoices->currentData().toString().isEmpty());
        });
        QObject::connect(m_copySelectedEndpoint, &QPushButton::clicked, this, [this] {
            const QString endpoint = m_endpointChoices->currentData().toString();
            if (!endpoint.isEmpty())
                QApplication::clipboard()->setText(endpoint);
        });
        QObject::connect(m_copyEndpoints, &QPushButton::clicked, this, [this] {
            QApplication::clipboard()->setText(m_viewerEndpointList.join(QLatin1Char('\n')));
        });
        QObject::connect(copyDiagnostics, &QPushButton::clicked, this, [this] {
            QApplication::clipboard()->setText(m_remote.diagnosticReport());
            m_diagnosticStatus->setText(
                QStringLiteral("Copied %1 bytes to the clipboard at %2.")
                    .arg(m_remote.diagnosticReport().toUtf8().size())
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))));
        });
        QObject::connect(saveDiagnostics, &QPushButton::clicked, this, [this] {
            const QString path = QFileDialog::getSaveFileName(
                this,
                QStringLiteral("Save HyRemote diagnostic report"),
                QStringLiteral("hyremote-diagnostic.txt"),
                QStringLiteral("Text files (*.txt);;All files (*)"));
            if (path.isEmpty())
                return;
            if (saveDiagnosticReport(path)) {
                m_diagnosticStatus->setText(
                    QStringLiteral("Saved %1 at %2.")
                        .arg(path, QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))));
            } else {
                m_diagnosticStatus->setText(QStringLiteral("Could not save %1.").arg(path));
                m_error->setText(QStringLiteral("Could not save diagnostic report"));
            }
        });
        QObject::connect(clearActivity, &QPushButton::clicked, m_activity, &QPlainTextEdit::clear);

        auto *timer = new QTimer(this);
        timer->setInterval(100);
        timer->setTimerType(Qt::CoarseTimer);
        QObject::connect(timer, &QTimer::timeout, this, [this] { refreshStatus(); });
        timer->start();
        appendActivity(QStringLiteral("Tool ready"));
        refreshListenerTargets();
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
        if (vertical)
            vertical->setValue(0);
        QApplication::processEvents();
        // The first screen must expose the primary action without scrolling.
        const QRect primaryRect(m_startStop->mapTo(m_scrollArea->viewport(), QPoint()),
                                m_startStop->size());
        const bool primaryActionVisible = m_scrollArea->viewport()->rect().contains(primaryRect);
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
        const bool usableEditor = notesRect.width() >= 160 && notesRect.height() >= 64;
        const bool reachable = usableEditor
                               && m_scrollArea->viewport()->rect().contains(notesRect);
        const bool pass = compact && overflow && reachable && primaryActionVisible;
        std::cout << (pass ? "TOOL_COMPACT_LAYOUT_PASS" : "TOOL_COMPACT_LAYOUT_FAIL")
                  << " window=" << width() << 'x' << height()
                  << " scroll_max=" << (vertical ? vertical->maximum() : -1)
                  << " horizontal_max=" << (horizontal ? horizontal->maximum() : -1)
                  << " primary_rect=" << primaryRect.x() << ',' << primaryRect.y()
                  << ',' << primaryRect.width() << 'x' << primaryRect.height()
                  << " notes_rect=" << notesRect.x() << ',' << notesRect.y()
                  << ',' << notesRect.width() << 'x' << notesRect.height()
                  << " viewport=" << m_scrollArea->viewport()->width()
                  << 'x' << m_scrollArea->viewport()->height() << std::endl;
        return pass;
    }

    // Deterministic offscreen/CI oracle for the listener editor. It drives the
    // same widgets an operator uses, then observes the public facade. No
    // listener is started and no network traffic is produced.
    bool verifyListenerControls()
    {
        auto *portEditor = findChild<QSpinBox *>(QStringLiteral("portEditor"));
        auto *apply = findChild<QPushButton *>(QStringLiteral("applyListener"));
        auto *target = findChild<QComboBox *>(QStringLiteral("listenTarget"));
        auto *modeBox = findChild<QComboBox *>(QStringLiteral("listenMode"));
        if (!portEditor || !apply || !target || !modeBox || !m_endpointChoices) {
            std::cout << "TOOL_LISTENER_CONTROLS_FAIL missing_widget" << std::endl;
            return false;
        }

        const auto selectMode = [modeBox](ListenMode mode) {
            const int index = modeBox->findData(static_cast<int>(mode));
            if (index >= 0)
                modeBox->setCurrentIndex(index);
            QApplication::processEvents();
        };
        bool ok = true;

        // Any IPv4 plus a valid port round-trip.
        selectMode(ListenMode::AnyIpv4);
        portEditor->setValue(5921);
        QApplication::processEvents();
        apply->click();
        QApplication::processEvents();
        ok = ok && m_remote.port() == 5921
             && m_remote.listenAddress() == QHostAddress(QHostAddress::AnyIPv4)
             && m_remote.listenInterface().isEmpty();

        // Explicit address: the editor is editable in this mode, so the check
        // does not depend on a live non-loopback interface.
        selectMode(ListenMode::Address);
        target->setEditText(QStringLiteral("127.0.0.1"));
        QApplication::processEvents();
        apply->click();
        QApplication::processEvents();
        ok = ok && m_remote.listenAddress() == QHostAddress(QHostAddress::LocalHost)
             && m_remote.listenInterface().isEmpty();

        // Interface mode: exercise the first real IPv4 interface when present.
        const QList<InterfaceChoice> interfaces = ipv4Interfaces();
        if (!interfaces.isEmpty()) {
            selectMode(ListenMode::Interface);
            target->setCurrentIndex(0);
            QApplication::processEvents();
            apply->click();
            QApplication::processEvents();
            ok = ok && m_remote.listenInterface() == interfaces.first().identity;
        } else {
            std::cout << "TOOL_LISTENER_CONTROLS_NOTE no_interface_choice" << std::endl;
        }

        // Restore the safe default and a normal port for later checks.
        selectMode(ListenMode::AnyIpv4);
        portEditor->setValue(5921);
        QApplication::processEvents();
        apply->click();
        QApplication::processEvents();

        std::cout << (ok ? "TOOL_LISTENER_CONTROLS_PASS" : "TOOL_LISTENER_CONTROLS_FAIL")
                  << " port=" << m_remote.port()
                  << " address=" << m_remote.listenAddress().toString().toStdString()
                  << " interface=" << m_remote.listenInterface().toStdString()
                  << " endpoints=" << m_endpointChoices->count() << std::endl;
        return ok;
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

    void refreshListenerTargets()
    {
        const auto mode = static_cast<ListenMode>(m_listenMode->currentData().toInt());
        m_listenTarget->blockSignals(true);
        m_listenTarget->clear();
        if (mode == ListenMode::Address) {
            for (const QString &address : ipv4Addresses())
                m_listenTarget->addItem(address, address);
        } else if (mode == ListenMode::Interface) {
            for (const InterfaceChoice &choice : ipv4Interfaces())
                m_listenTarget->addItem(choice.display, choice.identity);
        }
        m_listenTarget->blockSignals(false);
        // Address mode also accepts a manually typed address; interface mode
        // must stay a closed list because the identity is an exact interface name.
        m_listenTarget->setEditable(mode == ListenMode::Address);

        const bool configurable = m_remote.state() == HyRemote::RemoteAccessState::Stopped;
        m_listenTarget->setEnabled(configurable && mode != ListenMode::AnyIpv4);
        m_applyListener->setEnabled(configurable);
        if (mode != ListenMode::AnyIpv4 && m_listenTarget->count() == 0) {
            m_listenerHint->setText(
                QStringLiteral("No usable IPv4 address or interface was detected. "
                               "Use all IPv4 interfaces instead."));
        } else if (!configurable) {
            m_listenerHint->setText(
                QStringLiteral("Stop remote access before changing the listener. "
                               "The configured value takes effect on the next start."));
        } else {
            m_listenerHint->setText(
                QStringLiteral("The configured value takes effect on the next start."));
        }
    }

    void applyListenerSettings()
    {
        if (m_remote.state() != HyRemote::RemoteAccessState::Stopped) {
            appendActivity(QStringLiteral("Listener change rejected: stop remote access first"));
            return;
        }

        const auto mode = static_cast<ListenMode>(m_listenMode->currentData().toInt());
        bool ok = false;
        QString description;
        if (mode == ListenMode::AnyIpv4) {
            ok = m_remote.setListenAddress(QHostAddress(QHostAddress::AnyIPv4));
            description = QStringLiteral("all IPv4 interfaces");
        } else if (mode == ListenMode::Address) {
            // Editable on purpose: a selected entry and a manually typed
            // address must both work, and the visible text is the truth.
            QString address = m_listenTarget->currentText().trimmed();
            if (address.isEmpty())
                address = m_listenTarget->currentData().toString();
            ok = !address.isEmpty() && m_remote.setListenAddress(QHostAddress(address));
            description = address;
        } else {
            const QString identity = m_listenTarget->currentData().toString();
            ok = !identity.isEmpty() && m_remote.setListenInterface(identity);
            description = identity;
        }

        if (ok)
            ok = m_remote.setPort(static_cast<quint16>(m_portEditor->value()));

        if (!ok) {
            m_error->setText(QStringLiteral("Listener settings were rejected"));
            appendActivity(QStringLiteral("Listener settings rejected"));
            refreshStatus();
            return;
        }

        m_port = static_cast<quint16>(m_portEditor->value());
        appendActivity(QStringLiteral("Listener configured: port %1 on %2")
                           .arg(m_port)
                           .arg(description));
        refreshViewerEndpoints();
        refreshStatus();
    }

    void setListenerEditorsEnabled(bool enabled)
    {
        const auto mode = static_cast<ListenMode>(m_listenMode->currentData().toInt());
        m_portEditor->setEnabled(enabled);
        m_listenMode->setEnabled(enabled);
        m_applyListener->setEnabled(enabled);
        m_listenTarget->setEnabled(enabled && mode != ListenMode::AnyIpv4);
        m_listenerHint->setVisible(!enabled);
    }

    void refreshViewerEndpoints()
    {
        const QString previous = m_endpointChoices->currentData().toString();
        m_viewerEndpointList = viewerEndpoints(m_port);
        const QSignalBlocker blocker(m_endpointChoices);
        m_endpointChoices->clear();
        for (const QString &endpoint : m_viewerEndpointList)
            m_endpointChoices->addItem(endpoint, endpoint);
        const int restored = m_viewerEndpointList.indexOf(previous);
        if (restored >= 0)
            m_endpointChoices->setCurrentIndex(restored);
        else if (!m_viewerEndpointList.isEmpty())
            m_endpointChoices->setCurrentIndex(0);
        m_copyEndpoints->setEnabled(!m_viewerEndpointList.isEmpty());
        m_copySelectedEndpoint->setEnabled(m_endpointChoices->currentIndex() >= 0
                                           && !m_endpointChoices->currentData().toString().isEmpty());
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
        setListenerEditorsEnabled(state == HyRemote::RemoteAccessState::Stopped);

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
    QLabel *m_connection = nullptr;
    QLabel *m_clients = nullptr;
    QLabel *m_policy = nullptr;
    QLabel *m_focus = nullptr;
    QLabel *m_focusWarning = nullptr;
    QLabel *m_error = nullptr;
    QLabel *m_diagnosticStatus = nullptr;
    QLabel *m_listenerHint = nullptr;
    QPlainTextEdit *m_diagnostics = nullptr;
    QPlainTextEdit *m_activity = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    QWidget *m_operatorControls = nullptr;
    QPlainTextEdit *m_operatorNotes = nullptr;
    QSpinBox *m_portEditor = nullptr;
    QComboBox *m_listenMode = nullptr;
    QComboBox *m_listenTarget = nullptr;
    QPushButton *m_applyListener = nullptr;
    QComboBox *m_endpointChoices = nullptr;
    QPushButton *m_startStop = nullptr;
    QPushButton *m_copySelectedEndpoint = nullptr;
    QPushButton *m_copyEndpoints = nullptr;
    QCheckBox *m_input = nullptr;
    int m_testClicks = 0;
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
    QCommandLineOption listenerControlsOption(
        QStringLiteral("verify-listener-controls"),
        QStringLiteral("CI-only check: drive the listener editor and observe the public facade; "
                       "no listener is started."));
    parser.addOption(portOption);
    parser.addOption(inputOption);
    QCommandLineOption diagnosticReportFileOption(
        QStringLiteral("diagnostic-report-file"),
        QStringLiteral("Write one bounded public diagnostic report after startup handling."),
        QStringLiteral("path"));
    parser.addOption(autoStartOption);
    parser.addOption(secondsOption);
    parser.addOption(compactLayoutOption);
    parser.addOption(listenerControlsOption);
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

    if (parser.isSet(listenerControlsOption)) {
        // The listener editor is pure configuration: this check must not open
        // a socket or change the remote-input policy.
        QTimer::singleShot(0, &window, [&window, &app] {
            app.exit(window.verifyListenerControls() ? 0 : 1);
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
