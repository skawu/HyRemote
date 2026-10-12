#include <HyRemote/RemoteAccess.h>

#include <QAbstractSocket>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QClipboard>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFileDialog>
#include <QFrame>
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
#include <QTabWidget>
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

        // A host-side product console must answer: is it running, how do I
        // connect, and what can the remote side control? Low-level diagnostic
        // truth stays accessible in its own tab rather than swallowing the
        // first screen. Every page owns its compact scroll range.
        setObjectName(QStringLiteral("hyremoteToolWindow"));
        setStyleSheet(QStringLiteral(R"(
            QWidget#hyremoteToolWindow { background: #f5f7fb; }
            QWidget#toolHeader { background: #ffffff; border-bottom: 1px solid #dce3ed; }
            QFrame#toolCard { background: #ffffff; border: 1px solid #dce3ed; border-radius: 10px; }
            QFrame#actionDock { background: #ffffff; border-top: 1px solid #dce3ed; }
            QLabel#toolTitle { font-size: 20px; font-weight: 700; color: #17243b; }
            QLabel#sectionTitle { font-size: 16px; font-weight: 650; color: #17243b; }
            QLabel#eyebrow { font-size: 10px; font-weight: 700; color: #52657e; }
            QLabel#mutedLabel { color: #52657e; }
            QLabel#stateBadge { background: #eaf0fa; color: #234d78; padding: 7px 12px; border-radius: 12px; font-weight: 700; }
            QLabel#securityNote { background: #fff3dc; color: #734a00; border: 1px solid #f1d9a0; border-radius: 6px; padding: 9px; }
            QLabel#focusNote { background: #fff3dc; color: #734a00; border-radius: 6px; padding: 9px; }
            QPushButton#primaryAction { background: #2067d5; color: white; border: 1px solid #1557bb; border-radius: 6px; padding: 9px 15px; font-weight: 700; min-height: 24px; }
            QPushButton#primaryAction:hover { background: #1557bb; }
            QPushButton#primaryAction:focus { border: 2px solid #0c438f; }
            QTabWidget::pane { border: none; }
            QTabBar::tab { background: #edf1f7; color: #3c526d; padding: 10px 13px; border: 1px solid #dce3ed; }
            QTabBar::tab:selected { background: #ffffff; color: #1557bb; border-bottom: 2px solid #2067d5; }
            QComboBox#viewerAddress { padding: 7px; min-height: 26px; }
        )"));

        auto *windowLayout = new QVBoxLayout(this);
        windowLayout->setContentsMargins(0, 0, 0, 0);
        windowLayout->setSpacing(0);
        // Child native style size hints must not impose a top-level minimum:
        // the tab pages own horizontal/vertical scrolling at compact widths.
        windowLayout->setSizeConstraint(QLayout::SetNoConstraint);
        setMinimumSize(0, 0);

        auto *header = new QWidget(this);
        header->setObjectName(QStringLiteral("toolHeader"));
        auto *headerLayout = new QHBoxLayout(header);
        headerLayout->setContentsMargins(18, 11, 18, 11);
        auto *nameLayout = new QVBoxLayout;
        nameLayout->setSpacing(2);
        auto *heading = new QLabel(QStringLiteral("HyRemoteTool"), header);
        heading->setObjectName(QStringLiteral("toolTitle"));
        nameLayout->addWidget(heading);
        auto *subheading = new QLabel(QStringLiteral("Remote support  /  Host console"), header);
        subheading->setObjectName(QStringLiteral("mutedLabel"));
        nameLayout->addWidget(subheading);
        headerLayout->addLayout(nameLayout, 1);
        m_state = new QLabel(QStringLiteral("Stopped"), header);
        m_state->setObjectName(QStringLiteral("stateBadge"));
        headerLayout->addWidget(m_state, 0, Qt::AlignTop);
        windowLayout->addWidget(header);

        m_tabs = new QTabWidget(this);
        m_tabs->setObjectName(QStringLiteral("toolTabs"));
        m_tabs->setDocumentMode(true);
        m_tabs->setUsesScrollButtons(true);
        m_tabs->setElideMode(Qt::ElideRight);
        windowLayout->addWidget(m_tabs, 1);

        const auto newPage = [this](const QString &name, const QString &title) {
            auto *scroll = new QScrollArea(m_tabs);
            scroll->setObjectName(name);
            scroll->setWidgetResizable(true);
            scroll->setFrameShape(QFrame::NoFrame);
            scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            auto *body = new QWidget(scroll);
            auto *layout = new QVBoxLayout(body);
            layout->setContentsMargins(18, 16, 18, 16);
            layout->setSpacing(14);
            scroll->setWidget(body);
            m_tabs->addTab(scroll, title);
            return scroll;
        };
        const auto card = [](QWidget *parent) {
            auto *surface = new QFrame(parent);
            surface->setObjectName(QStringLiteral("toolCard"));
            auto *layout = new QVBoxLayout(surface);
            layout->setContentsMargins(16, 16, 16, 16);
            layout->setSpacing(10);
            return surface;
        };
        const auto sectionLabel = [](const QString &text, QWidget *parent) {
            auto *label = new QLabel(text, parent);
            label->setObjectName(QStringLiteral("sectionTitle"));
            return label;
        };

        // Connection: the primary job must be actionable without scrolling
        // past build IDs, every interface address and raw diagnostics.
        m_connectionScroll = newPage(QStringLiteral("connectionPage"), QStringLiteral("Connect"));
        auto *connectBody = m_connectionScroll->widget();
        auto *connectLayout = qobject_cast<QVBoxLayout *>(connectBody->layout());
        auto *connectCard = card(connectBody);
        auto *connectCardLayout = qobject_cast<QVBoxLayout *>(connectCard->layout());
        connectCardLayout->addWidget(sectionLabel(QStringLiteral("Viewer connection"), connectCard));
        auto *connectionSummary = new QHBoxLayout;
        m_connection = new QLabel(QStringLiteral("Remote access stopped"), connectCard);
        m_connection->setObjectName(QStringLiteral("mutedLabel"));
        connectionSummary->addWidget(m_connection, 1);
        m_clients = new QLabel(QStringLiteral("0 clients"), connectCard);
        m_clients->setObjectName(QStringLiteral("mutedLabel"));
        connectionSummary->addWidget(m_clients);
        connectCardLayout->addLayout(connectionSummary);

        auto *endpointHint = new QLabel(
            QStringLiteral("Select a reachable host address, then paste it into your VNC Viewer."),
            connectCard);
        endpointHint->setWordWrap(true);
        endpointHint->setObjectName(QStringLiteral("mutedLabel"));
        connectCardLayout->addWidget(endpointHint);
        auto *addressRow = new QHBoxLayout;
        m_endpointChoices = new QComboBox(connectCard);
        m_endpointChoices->setObjectName(QStringLiteral("viewerAddress"));
        m_endpointChoices->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_endpointChoices->setAccessibleName(QStringLiteral("Viewer connection address"));
        m_endpointChoices->setPlaceholderText(QStringLiteral("Select a LAN address"));
        addressRow->addWidget(m_endpointChoices, 1);
        m_copySelected = new QPushButton(QStringLiteral("Copy address"), connectCard);
        addressRow->addWidget(m_copySelected);
        connectCardLayout->addLayout(addressRow);


        m_policy = new QLabel(connectCard);
        m_policy->setObjectName(QStringLiteral("mutedLabel"));
        connectCardLayout->addWidget(m_policy);

        auto *connectionActions = new QHBoxLayout;
        auto *refreshEndpoints = new QPushButton(QStringLiteral("Refresh addresses"), connectCard);
        m_copyEndpoints = new QPushButton(QStringLiteral("Copy all addresses"), connectCard);
        connectionActions->addWidget(refreshEndpoints);
        connectionActions->addWidget(m_copyEndpoints);
        connectionActions->addStretch(1);
        connectCardLayout->addLayout(connectionActions);
        connectLayout->addWidget(connectCard);

        auto *securityCard = card(connectBody);
        auto *securityLayout = qobject_cast<QVBoxLayout *>(securityCard->layout());
        auto *security = new QLabel(
            QStringLiteral("Unencrypted connection - trusted LAN only. "
                           "SecurityType None has no authentication or transport encryption."),
            securityCard);
        security->setObjectName(QStringLiteral("securityNote"));
        security->setWordWrap(true);
        securityLayout->addWidget(security);
        m_focusWarning = new QLabel(
            QStringLiteral("Remote control is application-scoped. Activate this HyRemoteTool "
                           "window locally if remote pointer or text input stops responding."),
            securityCard);
        m_focusWarning->setObjectName(QStringLiteral("focusNote"));
        m_focusWarning->setWordWrap(true);
        m_focusWarning->setVisible(false);
        securityLayout->addWidget(m_focusWarning);
        auto *policyNote = new QLabel(
            QStringLiteral("Changing remote-control permission while running restarts "
                           "the listener and disconnects current viewers."),
            securityCard);
        policyNote->setWordWrap(true);
        policyNote->setObjectName(QStringLiteral("mutedLabel"));
        securityLayout->addWidget(policyNote);
        connectLayout->addWidget(securityCard);
        connectLayout->addStretch(1);

        // Native Windows styles may give checkboxes much wider size hints
        // than Linux. A side-by-side action row imposes a >480px top-level
        // minimum width, even with scroll areas. The primary start/stop and
        // consent control therefore live in a full-width fixed bottom dock:
        // always visible, never clipped by page content or a tab scroll.
        auto *actionDock = new QFrame(this);
        actionDock->setObjectName(QStringLiteral("actionDock"));
        auto *dockLayout = new QVBoxLayout(actionDock);
        dockLayout->setContentsMargins(18, 7, 18, 7);
        dockLayout->setSpacing(5);
        auto *startRow = new QHBoxLayout;
        m_startStop = new QPushButton(QStringLiteral("Start remote access"), actionDock);
        m_startStop->setObjectName(QStringLiteral("primaryAction"));
        startRow->addWidget(m_startStop, 0);
        startRow->addStretch(1);
        dockLayout->addLayout(startRow);
        m_input = new QCheckBox(QStringLiteral("Allow remote control"), actionDock);
        m_input->setChecked(initialInputEnabled);
        dockLayout->addWidget(m_input, 0, Qt::AlignLeft);
        m_dockSecurity = new QLabel(
            QStringLiteral("Unencrypted connection - trusted LAN only"), actionDock);
        m_dockSecurity->setObjectName(QStringLiteral("mutedLabel"));
        m_dockSecurity->setWordWrap(true);
        m_dockSecurity->setMinimumHeight(18);
        dockLayout->addWidget(m_dockSecurity);
        windowLayout->addWidget(actionDock, 0);


        // Controls stay meaningful and editable on the real host and Viewer.
        // Keep the original Widgets and RemoteAccess relationships intact.
        m_scrollArea = newPage(QStringLiteral("operatorPage"), QStringLiteral("Control"));
        auto *workBody = m_scrollArea->widget();
        auto *workRoot = qobject_cast<QVBoxLayout *>(workBody->layout());
        auto *workBox = card(workBody);
        m_operatorControls = workBox;
        auto *workOuter = qobject_cast<QVBoxLayout *>(workBox->layout());
        workOuter->addWidget(sectionLabel(QStringLiteral("Local operator controls"), workBox));
        auto *workDescription = new QLabel(
            QStringLiteral("Changes appear in the shared framebuffer. Local controls remain authoritative."),
            workBox);
        workDescription->setObjectName(QStringLiteral("mutedLabel"));
        workDescription->setWordWrap(true);
        workOuter->addWidget(workDescription);
        auto *workLayout = new QFormLayout;
        workLayout->setRowWrapPolicy(QFormLayout::WrapLongRows);
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
        auto *notes = new QPlainTextEdit(workBox);
        m_operatorNotes = notes;
        notes->setPlainText(QStringLiteral("Operator notes remain editable locally while remote support is active."));
        notes->setMaximumBlockCount(20);
        // Keep the full editor reachable on a 480x360 native window even
        // with a persistent safety/operation dock. Its own text scrolls.
        notes->setFixedHeight(96);
        workLayout->addRow(QStringLiteral("Asset"), asset);
        workLayout->addRow(QStringLiteral("Command setpoint"), speed);
        workLayout->addRow(QStringLiteral("Process load"), load);
        workLayout->addRow(QStringLiteral("Load indicator"), loadValue);
        workLayout->addRow(QString(), maintenance);
        workLayout->addRow(QStringLiteral("Notes"), notes);
        workOuter->addLayout(workLayout);
        workRoot->addWidget(workBox);
        workRoot->addStretch(1);

        // Diagnostics is secondary, but contains every fact formerly exposed
        // in the unprioritized status form and the same public report bytes.
        auto *diagnosticsScroll = newPage(QStringLiteral("diagnosticsPage"), QStringLiteral("Details"));
        auto *diagnosticsBody = diagnosticsScroll->widget();
        auto *diagnosticsRoot = qobject_cast<QVBoxLayout *>(diagnosticsBody->layout());
        auto *detailCard = card(diagnosticsBody);
        auto *detailLayout = qobject_cast<QVBoxLayout *>(detailCard->layout());
        detailLayout->addWidget(sectionLabel(QStringLiteral("System and runtime"), detailCard));
        auto *statusForm = new QFormLayout;
        statusForm->setRowWrapPolicy(QFormLayout::WrapLongRows);
        statusForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        m_buildIdentity = new QLabel(detailCard);
        m_deploymentIdentity = new QLabel(detailCard);
        for (QLabel *identity : {m_buildIdentity, m_deploymentIdentity}) {
            identity->setTextInteractionFlags(Qt::TextSelectableByMouse);
            identity->setWordWrap(true);
            auto policy = identity->sizePolicy();
            policy.setHorizontalPolicy(QSizePolicy::Ignored);
            policy.setHeightForWidth(true);
            identity->setSizePolicy(policy);
        }
        m_platform = new QLabel(detailCard);
        m_security = new QLabel(detailCard);
        m_configuredListener = new QLabel(detailCard);
        m_endpoint = new QLabel(detailCard);
        m_viewerEndpoints = new QLabel(detailCard);
        m_viewerEndpoints->setWordWrap(true);
        m_viewerEndpoints->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_focus = new QLabel(detailCard);
        m_error = new QLabel(QStringLiteral("None"), detailCard);
        m_error->setWordWrap(true);
        statusForm->addRow(QStringLiteral("Build identity"), m_buildIdentity);
        statusForm->addRow(QStringLiteral("Deployment identity"), m_deploymentIdentity);
        statusForm->addRow(QStringLiteral("Platform"), m_platform);
        statusForm->addRow(QStringLiteral("Security"), m_security);
        statusForm->addRow(QStringLiteral("Configured listener"), m_configuredListener);
        statusForm->addRow(QStringLiteral("Effective listener"), m_endpoint);
        statusForm->addRow(QStringLiteral("All viewer addresses"), m_viewerEndpoints);
        statusForm->addRow(QStringLiteral("Target window"), m_focus);
        statusForm->addRow(QStringLiteral("Last error"), m_error);
        detailLayout->addLayout(statusForm);

        auto *diagnosticsBox = card(diagnosticsBody);
        auto *diagnosticsLayout = qobject_cast<QVBoxLayout *>(diagnosticsBox->layout());
        diagnosticsLayout->addWidget(sectionLabel(QStringLiteral("Diagnostic report"), diagnosticsBox));
        m_diagnostics = new QPlainTextEdit(diagnosticsBox);
        m_diagnostics->setReadOnly(true);
        m_diagnostics->setLineWrapMode(QPlainTextEdit::NoWrap);
        m_diagnostics->setMaximumBlockCount(64);
        m_diagnostics->setMinimumHeight(128);
        diagnosticsLayout->addWidget(m_diagnostics);
        auto *diagnosticActions = new QHBoxLayout;
        auto *copyDiagnostics = new QPushButton(QStringLiteral("Copy report"), diagnosticsBox);
        auto *saveDiagnostics = new QPushButton(QStringLiteral("Save report"), diagnosticsBox);
        diagnosticActions->addWidget(copyDiagnostics);
        diagnosticActions->addWidget(saveDiagnostics);
        diagnosticActions->addStretch(1);
        diagnosticsLayout->addLayout(diagnosticActions);
        diagnosticsRoot->addWidget(diagnosticsBox);
        // Put the frequently used report in the first viewport; full build,
        // platform and endpoint metadata follows below as deeper detail.
        diagnosticsRoot->addWidget(detailCard);
        diagnosticsRoot->addStretch(1);

        auto *activityScroll = newPage(QStringLiteral("activityPage"), QStringLiteral("Log"));
        auto *activityBody = activityScroll->widget();
        auto *activityRoot = qobject_cast<QVBoxLayout *>(activityBody->layout());
        auto *activityBox = card(activityBody);
        auto *activityLayout = qobject_cast<QVBoxLayout *>(activityBox->layout());
        activityLayout->addWidget(sectionLabel(QStringLiteral("Recent activity"), activityBox));
        m_activity = new QPlainTextEdit(activityBox);
        m_activity->setReadOnly(true);
        m_activity->setLineWrapMode(QPlainTextEdit::NoWrap);
        m_activity->setMaximumBlockCount(100);
        m_activity->setMinimumHeight(175);
        activityLayout->addWidget(m_activity);
        auto *clearActivity = new QPushButton(QStringLiteral("Clear activity"), activityBox);
        activityLayout->addWidget(clearActivity, 0, Qt::AlignLeft);
        activityRoot->addWidget(activityBox);
        activityRoot->addStretch(1);

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
        QObject::connect(m_copySelected, &QPushButton::clicked, this, [this] {
            const QString address = m_endpointChoices->currentText();
            if (m_viewerEndpointList.contains(address))
                QApplication::clipboard()->setText(address);
        });
        QObject::connect(m_endpointChoices, &QComboBox::currentIndexChanged, this,
                         [this](int) {
            m_copySelected->setEnabled(
                m_viewerEndpointList.contains(m_endpointChoices->currentText()));
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

    // Offscreen/desktop regression oracle: primary connection actions and
    // all operator/diagnostic tabs must be navigable on compact screens.
    bool verifyCompactLayout()
    {
        resize(480, 360);
        QApplication::processEvents();
        const bool compact = width() <= 480 && height() <= 360;
        const bool tabsPresent = m_tabs->count() == 4;
        m_tabs->setCurrentIndex(0);
        QApplication::processEvents();
        const QRect actionRect(m_startStop->mapTo(this, QPoint()), m_startStop->size());
        const QRect inputRect(m_input->mapTo(this, QPoint()), m_input->size());
        const QRect safetyRect(m_dockSecurity->mapTo(this, QPoint()), m_dockSecurity->size());
        const bool firstScreen = rect().contains(actionRect)
                                 && rect().contains(inputRect)
                                 && rect().contains(safetyRect)
                                 && actionRect.width() >= 100 && actionRect.height() >= 25
                                 && inputRect.width() >= 110 && inputRect.height() >= 18
                                 && safetyRect.width() >= 150 && safetyRect.height() >= 14
                                 && m_startStop->isVisible() && m_input->isVisible()
                                 && m_dockSecurity->isVisible();

        m_tabs->setCurrentIndex(1);
        QApplication::processEvents();
        auto *vertical = m_scrollArea->verticalScrollBar();
        auto *horizontal = m_scrollArea->horizontalScrollBar();
        const bool overflow = vertical && vertical->maximum() > 0;
        if (vertical)
            vertical->setValue(vertical->maximum());
        if (horizontal)
            horizontal->setValue(horizontal->maximum());
        QApplication::processEvents();
        const QRect notesRect(m_operatorNotes->mapTo(m_scrollArea->viewport(), QPoint()),
                              m_operatorNotes->size());
        const bool usableEditor = notesRect.width() >= 160 && notesRect.height() >= 64;
        const bool reachable = usableEditor
                               && m_scrollArea->viewport()->rect().contains(notesRect);

        const auto visibleEditor = [](QScrollArea *page, QWidget *editor) {
            // QWidget::isVisible() is true even when the editor is clipped
            // below a page viewport. Prove that scrolling exposes usable
            // text/editing area, rather than accepting the mere existence of
            // a QWidget in the hierarchy.
            page->ensureWidgetVisible(editor, 0, 0);
            QApplication::processEvents();
            const QRect editorRect(editor->mapTo(page->viewport(), QPoint()),
                                   editor->size());
            const QRect exposed = editorRect.intersected(page->viewport()->rect());
            return editor->isVisible() && exposed.width() >= 160
                   && exposed.height() >= 60;
        };
        m_tabs->setCurrentIndex(2);
        QApplication::processEvents();
        auto *detailsPage = qobject_cast<QScrollArea *>(m_tabs->widget(2));
        const bool diagnosticsReachable = detailsPage && visibleEditor(detailsPage, m_diagnostics)
                                          && !m_buildIdentity->text().isEmpty();
        m_tabs->setCurrentIndex(3);
        QApplication::processEvents();
        auto *eventsPage = qobject_cast<QScrollArea *>(m_tabs->widget(3));
        const bool activityReachable = eventsPage && visibleEditor(eventsPage, m_activity);
        m_tabs->setCurrentIndex(0);
        const bool pass = compact && tabsPresent && firstScreen && overflow
                          && reachable && diagnosticsReachable && activityReachable
                          && m_remote.state() == HyRemote::RemoteAccessState::Stopped;
        std::cout << (pass ? "TOOL_COMPACT_LAYOUT_PASS" : "TOOL_COMPACT_LAYOUT_FAIL")
                  << " window=" << width() << 'x' << height()
                  << " tabs=" << m_tabs->count()
                  << " first_screen=" << firstScreen
                  << " action_rect=" << actionRect.x() << ',' << actionRect.y()
                  << ',' << actionRect.width() << 'x' << actionRect.height()
                  << " input_rect=" << inputRect.x() << ',' << inputRect.y()
                  << ',' << inputRect.width() << 'x' << inputRect.height()
                  << " safety_rect=" << safetyRect.x() << ',' << safetyRect.y()
                  << ',' << safetyRect.width() << 'x' << safetyRect.height()
                  << " tabs_min=" << m_tabs->minimumSizeHint().width()
                  << " window_min=" << minimumSizeHint().width()
                  << " connection_min=" << m_connectionScroll->minimumSizeHint().width()
                  << " dock_min=" << m_dockSecurity->parentWidget()->minimumSizeHint().width()
                  << " scroll_max=" << (vertical ? vertical->maximum() : -1)
                  << " horizontal_max=" << (horizontal ? horizontal->maximum() : -1)
                  << " notes_rect=" << notesRect.x() << ',' << notesRect.y()
                  << ',' << notesRect.width() << 'x' << notesRect.height()
                  << " viewport=" << m_scrollArea->viewport()->width()
                  << 'x' << m_scrollArea->viewport()->height()
                  << " diagnostics=" << diagnosticsReachable
                  << " activity=" << activityReachable << std::endl;
        return pass;
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
        const QString previous = m_endpointChoices->currentText();
        m_viewerEndpointList = viewerEndpoints(m_port);
        m_viewerEndpoints->setText(m_viewerEndpointList.isEmpty()
                                       ? QStringLiteral("No non-loopback IPv4 address detected")
                                       : m_viewerEndpointList.join(QStringLiteral("\n")));
        m_endpointChoices->blockSignals(true);
        m_endpointChoices->clear();
        m_endpointChoices->addItems(m_viewerEndpointList);
        if (m_viewerEndpointList.size() > 1 && !m_viewerEndpointList.contains(previous))
            m_endpointChoices->setCurrentIndex(-1);
        else if (m_viewerEndpointList.contains(previous))
            m_endpointChoices->setCurrentText(previous);
        m_endpointChoices->blockSignals(false);
        const bool selected = m_viewerEndpointList.contains(m_endpointChoices->currentText());
        m_copySelected->setEnabled(selected);
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
        m_clients->setText(QStringLiteral("%1 client%2")
                               .arg(static_cast<qulonglong>(clientCount))
                               .arg(clientCount == 1 ? QString() : QStringLiteral("s")));
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
        // The status poll runs every 100ms. Re-polishing a visible QLabel on
        // each tick costs layout/paint work and can itself trigger captured
        // framebuffer updates. Style only on actual state transitions.
        const int currentState = static_cast<int>(state);
        if (currentState != m_lastStyledState) {
            m_lastStyledState = currentState;
            m_state->setStyleSheet(state == HyRemote::RemoteAccessState::Running
                                       ? QStringLiteral("background:#e6f7ec;color:#11603a;padding:7px 12px;border-radius:12px;font-weight:700;")
                                       : state == HyRemote::RemoteAccessState::Faulted
                                             ? QStringLiteral("background:#ffebeb;color:#9b2424;padding:7px 12px;border-radius:12px;font-weight:700;")
                                             : QStringLiteral("background:#eaf0fa;color:#234d78;padding:7px 12px;border-radius:12px;font-weight:700;"));
        }

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
    int m_lastStyledState = -1;
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
    QLabel *m_dockSecurity = nullptr;
    QLabel *m_error = nullptr;
    QPlainTextEdit *m_diagnostics = nullptr;
    QPlainTextEdit *m_activity = nullptr;
    QTabWidget *m_tabs = nullptr;
    QScrollArea *m_connectionScroll = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    QComboBox *m_endpointChoices = nullptr;
    QPushButton *m_copySelected = nullptr;
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
