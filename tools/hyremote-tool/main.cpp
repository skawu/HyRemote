#include "framebuffer_view.h"
#include "rfb_client.h"
#include "language_preference.hpp"
#include "connection_profile_store.hpp"
#include "tool_diagnostic_session.hpp"
#include "tool_diagnostics_dialog.hpp"

#include <QApplication>
#include <QClipboard>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFile>
#include <QDateTime>
#include <QEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFontMetrics>
#include <QFormLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QMetaObject>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QStackedWidget>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QPixmap>
#include <QLocale>
#include <QSettings>
#include <QStringList>
#include <QSize>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <iostream>
#include <memory>
#include <utility>

namespace {

using Language = ToolLanguage;

QString humanBytes(quint64 bytes)
{
    constexpr double kKiB = 1024.0;
    constexpr double kMiB = kKiB * 1024.0;
    constexpr double kGiB = kMiB * 1024.0;
    if (bytes >= static_cast<quint64>(kGiB))
        return QStringLiteral("%1 GiB").arg(bytes / kGiB, 0, 'f', 2);
    if (bytes >= static_cast<quint64>(kMiB))
        return QStringLiteral("%1 MiB").arg(bytes / kMiB, 0, 'f', 1);
    if (bytes >= static_cast<quint64>(kKiB))
        return QStringLiteral("%1 KiB").arg(bytes / kKiB, 0, 'f', 1);
    return QStringLiteral("%1 B").arg(bytes);
}

bool isChinese(Language language)
{
    return language == Language::Chinese;
}

QString stateText(RfbClient::State state, Language language)
{
    const bool zh = isChinese(language);
    switch (state) {
    case RfbClient::State::Disconnected:
        return zh ? QStringLiteral("未连接") : QStringLiteral("Disconnected");
    case RfbClient::State::Connecting:
        return zh ? QStringLiteral("连接中") : QStringLiteral("Connecting");
    case RfbClient::State::Handshaking:
        return zh ? QStringLiteral("握手中") : QStringLiteral("Handshaking");
    case RfbClient::State::Connected:
        return zh ? QStringLiteral("已连接") : QStringLiteral("Connected");
    case RfbClient::State::Failed:
        return zh ? QStringLiteral("连接失败") : QStringLiteral("Failed");
    }
    return zh ? QStringLiteral("未知") : QStringLiteral("Unknown");
}

QString diagStageText(ToolDiagStage stage, Language language)
{
    const bool zh = isChinese(language);
    switch (stage) {
    case ToolDiagStage::Disconnected:
        return zh ? QStringLiteral("未连接") : QStringLiteral("Disconnected");
    case ToolDiagStage::Connecting:
        return zh ? QStringLiteral("连接中") : QStringLiteral("Connecting");
    case ToolDiagStage::Handshaking:
        return zh ? QStringLiteral("握手中") : QStringLiteral("Handshaking");
    case ToolDiagStage::Connected:
        return zh ? QStringLiteral("已连接") : QStringLiteral("Connected");
    case ToolDiagStage::Failed:
        return zh ? QStringLiteral("失败") : QStringLiteral("Failed");
    }
    return zh ? QStringLiteral("未知") : QStringLiteral("Unknown");
}

QString diagFailureText(ToolDiagFailure failure, Language language)
{
    const bool zh = isChinese(language);
    switch (failure) {
    case ToolDiagFailure::None:
        return zh ? QStringLiteral("无") : QStringLiteral("None");
    case ToolDiagFailure::ConnectionRefused:
        return zh ? QStringLiteral("连接被拒绝") : QStringLiteral("Connection refused");
    case ToolDiagFailure::DnsFailure:
        return zh ? QStringLiteral("主机未找到") : QStringLiteral("Host not found");
    case ToolDiagFailure::Timeout:
        return zh ? QStringLiteral("超时") : QStringLiteral("Timeout");
    case ToolDiagFailure::SecurityMismatch:
        return zh ? QStringLiteral("安全类型不匹配") : QStringLiteral("Security mismatch");
    case ToolDiagFailure::MalformedWire:
        return zh ? QStringLiteral("端点协议不符") : QStringLiteral("Malformed endpoint");
    case ToolDiagFailure::RemoteClosed:
        return zh ? QStringLiteral("远端关闭连接") : QStringLiteral("Remote closed");
    case ToolDiagFailure::Other:
        return zh ? QStringLiteral("其他本机观察错误") : QStringLiteral("Other local error");
    }
    return zh ? QStringLiteral("未知") : QStringLiteral("Unknown");
}

QString diagAdviceText(ToolDiagFailure failure, Language language)
{
    const bool zh = isChinese(language);
    switch (failure) {
    case ToolDiagFailure::ConnectionRefused:
        return zh ? QStringLiteral("确认远程应用正在监听该端点，然后重试。")
                  : QStringLiteral("Verify the application is listening on this endpoint, then retry.");
    case ToolDiagFailure::DnsFailure:
        return zh ? QStringLiteral("检查 IP 或主机名拼写。")
                  : QStringLiteral("Check the IP address or host name.");
    case ToolDiagFailure::Timeout:
        return zh ? QStringLiteral("检查本机网络与防火墙后重试。")
                  : QStringLiteral("Check local networking/firewall, then retry.");
    case ToolDiagFailure::SecurityMismatch:
        return zh ? QStringLiteral("此客户端仅支持 RFB None；确认端点允许未加密访问。")
                  : QStringLiteral("This client supports RFB None only; confirm the endpoint allows unencrypted access.");
    case ToolDiagFailure::MalformedWire:
        return zh ? QStringLiteral("确认端点是否为兼容的 RFB 服务。")
                  : QStringLiteral("Verify that the endpoint provides compatible RFB.");
    case ToolDiagFailure::RemoteClosed:
        return zh ? QStringLiteral("检查会话和网络连接后重试。")
                  : QStringLiteral("Check the session and network, then retry.");
    case ToolDiagFailure::Other:
        return zh ? QStringLiteral("检查本机连接设置及受信任的网络。")
                  : QStringLiteral("Review local connection settings and trusted network.");
    case ToolDiagFailure::None:
        return zh ? QStringLiteral("目前没有可采取的具体排查步骤。")
                  : QStringLiteral("No specific next step from current evidence.");
    }
    return zh ? QStringLiteral("没有足够证据判断。") : QStringLiteral("Insufficient evidence.");
}

bool textFits(const QWidget *widget, const QString &value)
{
    if (!widget || value.isEmpty())
        return false;
    const QFontMetrics metrics(widget->font());
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    const int width = metrics.horizontalAdvance(value);
#else
    const int width = metrics.width(value);
#endif
    return widget->width() >= width && widget->height() >= metrics.height();
}

class ClientWindow final : public QMainWindow

{
public:
    explicit ClientWindow(Language language, ToolLanguagePreference preference,
                          bool cliOverride, QSettings &settings,
                          QString profilePath = {}, QWidget *parent = nullptr)
        : QMainWindow(parent)
        , m_client(new RfbClient(this))
        , m_view(new FramebufferView(this))
        , m_language(language)
        , m_languageSettings(settings)
        , m_profileStore(profilePath.isEmpty()
            ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
                  .filePath(QStringLiteral("connections.json"))
            : std::move(profilePath))
    {
        auto *central = new QWidget(this);
        setCentralWidget(central);
        auto *root = new QVBoxLayout(central);
        root->setContentsMargins(12, 8, 12, 8);
        root->setSpacing(8);

        const auto actionButton = [](QPushButton *button) {
            button->setMinimumHeight(34);
            button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        };

        m_topBar = new QWidget(central);
        auto *top = new QVBoxLayout(m_topBar);
        top->setContentsMargins(0, 0, 0, 0);
        top->setSpacing(4);
        auto *brandRow = new QHBoxLayout;
        brandRow->setSpacing(8);
        m_brand = new QLabel(m_topBar);
        const QPixmap brandImage(QStringLiteral(":/hyremote/branding/huayan-logo-single.png"));
        if (brandImage.isNull())
            qFatal("BRANDING_ASSET_MISSING");
        m_brand->setPixmap(brandImage.scaled(28, 28, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        m_brandText = new QLabel(QStringLiteral("HyRemoteTool"), m_topBar);
        brandRow->addWidget(m_brand);
        brandRow->addWidget(m_brandText);
        brandRow->addStretch(1);
        top->addLayout(brandRow);

        auto *navRow = new QHBoxLayout;
        navRow->setSpacing(8);
        m_connectPageButton = new QPushButton(m_topBar);
        m_sessionPageButton = new QPushButton(m_topBar);
        m_diagnosticsPageButton = new QPushButton(m_topBar);
        for (QPushButton *button : {m_connectPageButton, m_sessionPageButton,
                                    m_diagnosticsPageButton}) {
            button->setCheckable(true);
            button->setAutoExclusive(true);
            actionButton(button);
            navRow->addWidget(button);
        }
        navRow->addStretch(1);
        top->addLayout(navRow);
        m_languageBox = new QComboBox(m_topBar);
        m_languageBox->addItem(QString()); // Auto follows the local OS
        m_languageBox->addItem(QStringLiteral("简体中文"));
        m_languageBox->addItem(QStringLiteral("English"));
        // A launch-only CLI override is visible in the selector but MUST
        // NOT silently rewrite the previously saved Auto/manual choice.
        m_languageBox->setCurrentIndex(
            cliOverride ? (isChinese(m_language) ? 1 : 2)
                        : (preference == ToolLanguagePreference::Auto
                               ? 0
                               : preference == ToolLanguagePreference::Chinese ? 1 : 2));
        m_languageBox->setMinimumHeight(34);
        m_aboutButton = new QPushButton(m_topBar);
        actionButton(m_aboutButton);
        brandRow->addWidget(m_languageBox);
        brandRow->addWidget(m_aboutButton);
        root->addWidget(m_topBar);

        m_pages = new QStackedWidget(central);
        root->addWidget(m_pages, 1);

        auto *connectPage = new QWidget(m_pages);
        auto *connectLayout = new QVBoxLayout(connectPage);
        connectLayout->setContentsMargins(0, 0, 0, 0);
        m_connectTitle = new QLabel(connectPage);
        m_connectIntro = new QLabel(connectPage);
        m_connectIntro->setWordWrap(true);
        connectLayout->addWidget(m_connectTitle);
        connectLayout->addWidget(m_connectIntro);
        auto *connectContent = new QHBoxLayout;
        connectContent->setSpacing(12);

        auto *profilesPanel =new QVBoxLayout;
        m_profilesLabel = new QLabel(connectPage);
        m_profilesList = new QListWidget(connectPage);
        m_profilesList->setObjectName(QStringLiteral("tool-saved-connections"));
        m_profilesList->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_profilesList->setSelectionMode(QAbstractItemView::SingleSelection);
        m_profilesList->setAlternatingRowColors(true);
        m_profilesEmpty = new QLabel(connectPage);
        m_profilesEmpty->setWordWrap(true);
        m_profileSave = new QPushButton(connectPage);
        m_profileEdit = new QPushButton(connectPage);
        m_profileDelete = new QPushButton(connectPage);
        m_profileRecover = new QPushButton(connectPage);
        for (QPushButton *button : {m_profileSave, m_profileEdit, m_profileDelete,
                                    m_profileRecover}) {
            actionButton(button);
        }
        m_profileRecover->hide();
        auto *profileActions = new QHBoxLayout;
        profileActions->setSpacing(6);
        for (QPushButton *button : {m_profileSave, m_profileEdit, m_profileDelete,
                                    m_profileRecover}) {
            profileActions->addWidget(button);
        }
        profilesPanel->addWidget(m_profilesLabel);
        profilesPanel->addWidget(m_profilesList, 1);
        profilesPanel->addWidget(m_profilesEmpty);
        profilesPanel->addLayout(profileActions);
        connectContent->addLayout(profilesPanel, 1);

        auto *formPanel = new QVBoxLayout;
        auto *form = new QFormLayout;
        form->setLabelAlignment(Qt::AlignRight);
        form->setSpacing(8);
        m_remoteLabel = new QLabel(connectPage);
        m_host = new QLineEdit(connectPage);
        m_host->setPlaceholderText(QStringLiteral("192.168.1.6"));
        m_host->setClearButtonEnabled(true);
        m_portLabel = new QLabel(connectPage);
        m_port = new QSpinBox(connectPage);
        m_port->setRange(1, 65535);
        m_port->setValue(5921);
        m_profileNameLabel = new QLabel(connectPage);
        m_profileName = new QLineEdit(connectPage);
        m_profileName->setClearButtonEnabled(true);
        form->addRow(m_remoteLabel, m_host);
        form->addRow(m_portLabel, m_port);
        form->addRow(m_profileNameLabel, m_profileName);
        formPanel->addLayout(form);
        m_connect = new QPushButton(connectPage);
        actionButton(m_connect);
        formPanel->addWidget(m_connect);
        m_status = new QLabel(connectPage);
        m_status->setWordWrap(true);
        m_nextStep = new QLabel(connectPage);
        m_nextStep->setWordWrap(true);
        formPanel->addWidget(m_status);
        formPanel->addWidget(m_nextStep);
        m_connectionError = new QLabel(connectPage);
        m_connectionError->setWordWrap(true);
        m_connectionError->setStyleSheet(QStringLiteral("color: #b00020;"));
        m_connectionError->setVisible(false);
        formPanel->addWidget(m_connectionError);
        m_connectionDiagnosticsButton = new QPushButton(connectPage);
        actionButton(m_connectionDiagnosticsButton);
        formPanel->addWidget(m_connectionDiagnosticsButton);
        formPanel->addStretch(1);
        connectContent->addLayout(formPanel, 2);
        connectLayout->addLayout(connectContent, 1);

        m_profileWarning = new QLabel(connectPage);
        m_profileWarning->setWordWrap(true);
        m_profileWarning->setStyleSheet(QStringLiteral("color: #b00020;"));
        m_profileWarning->hide();
        connectLayout->addWidget(m_profileWarning);
        m_languageWarning = new QLabel(connectPage);
        m_languageWarning->setWordWrap(true);
        m_languageWarning->setStyleSheet(QStringLiteral("color: #b00020;"));
        m_languageWarning->hide();
        connectLayout->addWidget(m_languageWarning);
        m_pages->addWidget(connectPage);

        auto *sessionPage = new QWidget(m_pages);
        auto *sessionLayout = new QVBoxLayout(sessionPage);
        sessionLayout->setContentsMargins(0, 0, 0, 0);
        m_sessionHeader = new QLabel(sessionPage);
        m_sessionHeader->setWordWrap(true);
        sessionLayout->addWidget(m_sessionHeader);
        m_operationBar = new QWidget(sessionPage);
        auto *operations = new QVBoxLayout(m_operationBar);
        operations->setContentsMargins(0, 0, 0, 0);
        operations->setSpacing(4);
        auto *controlRow = new QHBoxLayout;
        controlRow->setSpacing(6);
        auto *viewRow = new QHBoxLayout;
        viewRow->setSpacing(6);
        m_control = new QCheckBox(m_operationBar);
        m_fitButton = new QPushButton(m_operationBar);
        m_nativeButton = new QPushButton(m_operationBar);
        m_fullscreenButton = new QPushButton(m_operationBar);
        m_reconnectButton = new QPushButton(m_operationBar);
        m_disconnectButton = new QPushButton(m_operationBar);
        m_sessionDiagnosticsButton = new QPushButton(m_operationBar);
        for (QPushButton *button : {m_fitButton, m_nativeButton, m_fullscreenButton,
                                    m_reconnectButton, m_disconnectButton,
                                    m_sessionDiagnosticsButton}) {
            actionButton(button);
        }
        m_fitButton->setCheckable(true);
        m_nativeButton->setCheckable(true);
        m_fitButton->setChecked(true);
        controlRow->addWidget(m_control);
        controlRow->addWidget(m_fullscreenButton);
        controlRow->addWidget(m_disconnectButton);
        controlRow->addStretch(1);
        viewRow->addWidget(m_fitButton);
        viewRow->addWidget(m_nativeButton);
        viewRow->addWidget(m_reconnectButton);
        viewRow->addWidget(m_sessionDiagnosticsButton);
        viewRow->addStretch(1);
        operations->addLayout(controlRow);
        operations->addLayout(viewRow);
        sessionLayout->addWidget(m_operationBar);

        m_viewScroll = new QScrollArea(sessionPage);
        m_viewScroll->setWidgetResizable(true);
        m_viewScroll->setWidget(m_view);
        sessionLayout->addWidget(m_viewScroll, 1);
        m_error = new QLabel(sessionPage);
        m_error->setWordWrap(true);
        m_error->setStyleSheet(QStringLiteral("color: #b00020;"));
        m_error->setVisible(false);
        sessionLayout->addWidget(m_error);
        m_statusRow = new QWidget(sessionPage);
        auto *statusRow = new QHBoxLayout(m_statusRow);
        statusRow->setContentsMargins(0, 0, 0, 0);
        statusRow->setSpacing(12);
        m_securityLabel = new QLabel(m_statusRow);
        m_resolution = new QLabel(m_statusRow);
        m_fps = new QLabel(m_statusRow);
        m_mbps = new QLabel(m_statusRow);
        for (QLabel *label : {m_securityLabel, m_resolution, m_fps, m_mbps})
            statusRow->addWidget(label);
        statusRow->addStretch(1);
        sessionLayout->addWidget(m_statusRow);
        m_pages->addWidget(sessionPage);

        auto *diagnosticsPage = new QWidget(m_pages);
        auto *diagnosticsLayout = new QVBoxLayout(diagnosticsPage);
        diagnosticsLayout->setContentsMargins(0, 0, 0, 0);
        m_diagTitle = new QLabel(diagnosticsPage);
        m_diagSummary = new QLabel(diagnosticsPage);
        m_diagMetrics = new QLabel(diagnosticsPage);
        m_diagFrames = new QLabel(diagnosticsPage);
        m_diagBytes = new QLabel(diagnosticsPage);
        m_diagEvents = new QLabel(diagnosticsPage);
        for (QLabel *label : {m_diagSummary, m_diagMetrics, m_diagFrames,
                              m_diagBytes, m_diagEvents}) {
            label->setWordWrap(true);
            label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        }
        m_diagnosticsAction = new QPushButton(diagnosticsPage);
        m_diagnosticsAction->setObjectName(QStringLiteral("tool-open-diagnostics"));
        actionButton(m_diagnosticsAction);
        diagnosticsLayout->addWidget(m_diagTitle);
        diagnosticsLayout->addWidget(m_diagSummary);
        diagnosticsLayout->addWidget(m_diagMetrics);
        diagnosticsLayout->addWidget(m_diagFrames);
        diagnosticsLayout->addWidget(m_diagBytes);
        diagnosticsLayout->addWidget(m_diagEvents, 1);
        diagnosticsLayout->addWidget(m_diagnosticsAction);
        m_pages->addWidget(diagnosticsPage);

        m_connectPageButton->setChecked(true);
        connect(m_connectPageButton, &QPushButton::clicked, this,
                [this] { m_pages->setCurrentIndex(0); });
        connect(m_sessionPageButton, &QPushButton::clicked, this,
                [this] { m_pages->setCurrentIndex(1); });
        connect(m_diagnosticsPageButton, &QPushButton::clicked, this,
                [this] { m_pages->setCurrentIndex(2); });
        connect(m_pages, &QStackedWidget::currentChanged, this, [this](int index) {
            m_connectPageButton->setChecked(index == 0);
            m_sessionPageButton->setChecked(index == 1);
            m_diagnosticsPageButton->setChecked(index == 2);
            if (index != 1)
                setLocalControlRequested(false);
            if (index == 2)
                refreshDiagnosticsSummary();
        });

        m_view->onPointer = [this](quint8 mask, quint16 x, quint16 y) {
            m_client->sendPointer(mask, x, y);
        };
        m_view->onKey = [this](quint32 keysym, bool pressed) {
            m_client->sendKey(keysym, pressed);
        };
        if (QApplication::instance())
            QApplication::instance()->installEventFilter(this);
        connect(m_fitButton, &QPushButton::clicked, this, [this] {
            setViewMode(FramebufferView::DisplayMode::Fit);
        });
        connect(m_nativeButton, &QPushButton::clicked, this, [this] {
            setViewMode(FramebufferView::DisplayMode::Native);
        });
        connect(m_fullscreenButton, &QPushButton::clicked, this, [this] {
            toggleFullscreen();
        });
        connect(m_reconnectButton, &QPushButton::clicked, this, [this] {
            reconnectSession();
        });
        connect(m_disconnectButton, &QPushButton::clicked, this, [this] {
            toggleConnection();
        });
        connect(m_sessionDiagnosticsButton, &QPushButton::clicked, this, [this] {
            m_pages->setCurrentIndex(2);
        });
        connect(m_connectionDiagnosticsButton, &QPushButton::clicked, this, [this] {
            m_pages->setCurrentIndex(2);
        });

        connect(m_connect, &QPushButton::clicked, this, [this] { toggleConnection(); });
        connect(m_diagnosticsAction, &QPushButton::clicked, this, [this] {
            openDiagnostics();
        });
        connect(m_aboutButton, &QPushButton::clicked, this, [this] { openAbout(); });
        connect(m_profilesList, &QListWidget::currentRowChanged, this, [this](int row) {
            selectProfile(row);
        });
        connect(m_profileSave, &QPushButton::clicked, this, [this] { promptSaveProfile(); });
        connect(m_profileEdit, &QPushButton::clicked, this, [this] { promptEditProfile(); });
        connect(m_profileDelete, &QPushButton::clicked, this, [this] { promptDeleteProfile(); });
        connect(m_profileRecover, &QPushButton::clicked, this, [this] {
            promptRecoverProfiles();
        });
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        connect(m_languageBox, &QComboBox::activated, this, [this](int index) {
#else
        connect(m_languageBox, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
#endif
            const auto preference = index == 1 ? ToolLanguagePreference::Chinese
                                    : index == 2 ? ToolLanguagePreference::English
                                                 : ToolLanguagePreference::Auto;
            m_languageSaveFailed = !saveToolLanguagePreference(m_languageSettings, preference);
            if (m_languageSaveFailed)
                std::cerr << "TOOL_LANGUAGE_SAVE_FAIL" << std::endl;
            m_language = resolveToolLanguage(preference, QLocale::system());
            retranslateUi();
        });
        connect(m_control, &QCheckBox::toggled, this,
                [this](bool enabled) { setLocalControlRequested(enabled); });
        connect(m_client, &RfbClient::stateChanged, this, [this](RfbClient::State state) {
            m_diagnostics.observeState(state);
            applyState(state);
            if (m_diagDialog && m_diagDialog->isVisible())
                m_diagDialog->refresh();
        });
        connect(m_client, &RfbClient::socketErrorObserved, this,
                [this](QAbstractSocket::SocketError error) {
                    m_diagnostics.observeSocketError(error);
                });
        connect(m_client, &RfbClient::frameUpdated, this, [this](const QImage &frame) {
            m_view->setFrame(frame);
            m_frameSize = frame.size();
            m_diagnostics.observeFrame(frame.size());
            refreshDynamicText();
            std::cout << "TOOL_CLIENT_FRAME " << frame.width() << 'x' << frame.height() << std::endl;
        });
        connect(m_client, &RfbClient::cursorUpdated, this, [this](const QImage &cursor, const QPoint &hotspot) {
            m_view->setCursorImage(cursor, hotspot);
            if (cursor.isNull()) {
                std::cout << "TOOL_CLIENT_CURSOR clear=1" << std::endl;
            } else {
                std::cout << "TOOL_CLIENT_CURSOR width=" << cursor.width()
                          << " height=" << cursor.height()
                          << " hotspot=" << hotspot.x() << ',' << hotspot.y() << std::endl;
            }
        });
        connect(m_client, &RfbClient::serverNameChanged, this, [](const QString &name) {
            Q_UNUSED(name)
            // ServerInit name is untrusted peer metadata, never proof of
            // remote input capability/authorization. It may carry secrets,
            // forged log lines or terminal escape sequences.
            std::cout << "TOOL_CLIENT_SERVER name=[redacted]" << std::endl;
        });
        connect(m_client, &RfbClient::statsUpdated, this, [this](const RfbClient::Stats &stats) {
            m_lastStats = stats;
            m_diagnostics.observeStats(stats);
            refreshDynamicText();
            std::cout << "TOOL_CLIENT_STATS fps=" << stats.fps
                      << " mbit=" << stats.mbitPerSecond
                      << " frames=" << stats.frames
                      << " bytes=" << stats.bytes << std::endl;
        });
        connect(m_client, &RfbClient::errorOccurred, this, [this](const QString &message) {
            m_lastErrorRaw = message;
            m_diagnostics.observeError(message);
            if (m_diagDialog && m_diagDialog->isVisible())
                m_diagDialog->refresh();
            refreshDynamicText();
            std::cerr << "TOOL_CLIENT_ERROR " << message.toStdString() << std::endl;
        });

        m_view->setInputEnabled(false);
        m_client->setRemoteControlEnabled(false);
        QString profileError;
        const auto profileState = m_profileStore.load(&profileError);
        m_profilesCorrupt = profileState == ToolConnectionProfileStore::LoadState::Corrupt;
        m_profilesLoaded = profileState == ToolConnectionProfileStore::LoadState::Missing
                           || profileState == ToolConnectionProfileStore::LoadState::Loaded
                           || profileState == ToolConnectionProfileStore::LoadState::MigratedV0;
        if (m_profilesCorrupt || profileState == ToolConnectionProfileStore::LoadState::IoError)
            m_profileError = profileError;
        rebuildProfiles();
        applyState(RfbClient::State::Disconnected);
        resize(1366, 768);
    }

    void connectTo(const QString &host, quint16 port)
    {
        m_host->setText(host);
        m_port->setValue(port);
        m_pendingProfileId.clear();
        m_diagnostics.beginAttempt(host, port);
        if (m_diagDialog)
            m_diagDialog->resetEndpointSelection();
        m_client->connectToHost(host, port);
    }

    // Deterministic GUI test of the *real* QComboBox::activated(int)
    // callback, including explicit re-selection of an already selected CLI
    // language. A synthetic signal is sufficient to reach the production
    // connection without opening a dropdown on offscreen QPA.
    bool verifyLanguageSelection(bool expectSaveFailure)
    {
        const bool initial = m_languageBox && m_languageBox->currentIndex() == 2
                             && m_language == Language::English;
        const bool signal = initial && QMetaObject::invokeMethod(
            m_languageBox, "activated", Qt::DirectConnection, Q_ARG(int, 2));
        const bool saved = !expectSaveFailure
                               && loadToolLanguagePreference(m_languageSettings)
                                      == ToolLanguagePreference::English;
        const bool failed = expectSaveFailure && m_languageSaveFailed
                             && m_languageWarning->isVisible()
                             && m_languageSettings.status() != QSettings::NoError;
        const bool ok = initial && signal && m_language == Language::English
                        && (expectSaveFailure ? failed : saved && !m_languageSaveFailed);
        std::cout << (ok ? "TOOL_LANGUAGE_UI_PASS" : "TOOL_LANGUAGE_UI_FAIL")
                  << " mode=" << (expectSaveFailure ? "save-failure" : "repeat-select")
                  << " lang=en"
                  << std::endl;
        return ok;
    }

    bool verifyProfileUi();
    bool verifyDiagnosticsUi(const QString &testCase);
    bool verifyServerNameMetadata();
    void setLocalControlRequested(bool requested);
    void setViewMode(FramebufferView::DisplayMode mode);
    void toggleFullscreen();
    void reconnectSession();
    void openAbout();

    bool verifyLayoutAt(const QSize &viewport, Language language, bool scaledFont)
    {
        const Language originalLanguage = m_language;
        const QFont originalFont = QApplication::font();
        QFont checkFont = originalFont;
        if (scaledFont)
            checkFont.setPointSizeF(checkFont.pointSizeF() * 1.15);
        QApplication::setFont(checkFont);
        m_language = language;
        retranslateUi();
        showNormal();
        resize(viewport);
        show();
        QApplication::processEvents();
        QApplication::processEvents();

        const QRect windowRect = rect();
        const auto inWindow = [this](const QWidget *widget) {
            return QRect(widget->mapTo(this, QPoint(0, 0)), widget->size());
        };
        bool sizeOk = size() == viewport && m_pages->count() == 3;
        bool navOk = true;
        const QVector<QPushButton *> nav = {m_connectPageButton, m_sessionPageButton,
                                            m_diagnosticsPageButton};
        for (int i = 0; i < nav.size(); ++i) {
            navOk = navOk && nav.at(i) && !nav.at(i)->text().isEmpty()
                            && textFits(nav.at(i), nav.at(i)->text())
                            && windowRect.contains(inWindow(nav.at(i)));
            for (int j = i + 1; j < nav.size(); ++j)
                navOk = navOk && !inWindow(nav.at(i)).intersects(inWindow(nav.at(j)));
        }

        struct PageCheck {
            int index;
            QPushButton *button;
            QVector<QWidget *> controls;
        };
        const QVector<PageCheck> pages = {
            {0, m_connectPageButton, {m_profilesList, m_profileSave, m_profileEdit,
                                      m_profileDelete, m_host, m_port, m_profileName,
                                      m_connect, m_status, m_nextStep,
                                      m_connectionDiagnosticsButton}},
            {1, m_sessionPageButton, {m_control, m_fitButton, m_nativeButton,
                                      m_fullscreenButton, m_reconnectButton,
                                      m_disconnectButton, m_sessionDiagnosticsButton,
                                      m_viewScroll, m_securityLabel, m_resolution,
                                      m_fps, m_mbps}},
            {2, m_diagnosticsPageButton, {m_diagTitle, m_diagSummary, m_diagMetrics,
                                          m_diagFrames, m_diagBytes, m_diagEvents,
                                          m_diagnosticsAction}}
        };
        bool pagesOk = true;
        bool controlsOk = true;
        bool viewAreaOk = true;
        bool overlapOk = true;
        for (const PageCheck &page : pages) {
            page.button->click();
            QApplication::processEvents();
            pagesOk = pagesOk && m_pages->currentIndex() == page.index
                                && m_pages->currentWidget()
                                && m_pages->currentWidget()->isVisibleTo(this);
            int checked = 0;
            for (QPushButton *button : nav)
                checked += button->isChecked() ? 1 : 0;
            pagesOk = pagesOk && checked == 1 && page.button->isChecked();
            for (QWidget *control : page.controls) {
                controlsOk = controlsOk && control && control->isVisibleTo(this)
                             && windowRect.contains(inWindow(control))
                             && control->width() > 0 && control->height() > 0;
                if (auto *button = qobject_cast<QPushButton *>(control))
                    controlsOk = controlsOk && textFits(button, button->text());
            }
            if (page.index == 1) {
                const double viewArea = static_cast<double>(m_viewScroll->width())
                                        * m_viewScroll->height();
                const double clientArea = static_cast<double>(width()) * height();
                viewAreaOk = viewArea >= clientArea * 0.60;
                const QVector<QPushButton *> actions = {m_fitButton, m_nativeButton,
                                                        m_fullscreenButton, m_reconnectButton,
                                                        m_disconnectButton,
                                                        m_sessionDiagnosticsButton};
                for (int i = 0; i < actions.size(); ++i)
                    for (int j = i + 1; j < actions.size(); ++j)
                        overlapOk = overlapOk
                            && !inWindow(actions.at(i)).intersects(inWindow(actions.at(j)));
            }
        }

        m_language = originalLanguage;
        QApplication::setFont(originalFont);
        retranslateUi();
        QApplication::processEvents();
        return sizeOk && navOk && pagesOk && controlsOk && viewAreaOk && overlapOk;
    }

    bool verifyUi()
    {
        const bool base = m_brand && m_brandText && m_topBar && m_pages
                          && m_pages->count() == 3 && !QGuiApplication::windowIcon().isNull()
                          && m_host && m_port && m_connect && m_view && m_viewScroll
                          && m_status && m_nextStep && m_securityLabel
                          && m_control && m_fps && m_mbps && m_diagFrames && m_diagBytes
                          && m_resolution && m_error && m_connectionError
                          && m_languageBox && m_languageWarning && m_aboutButton
                          && m_profilesLabel && m_profilesList && m_profilesEmpty
                          && m_profileName && m_profileSave && m_profileEdit
                          && m_profileDelete && m_profileRecover && m_profileWarning
                          && m_diagTitle && m_diagSummary && m_diagMetrics
                          && m_diagEvents && m_diagnosticsAction
                          && m_reconnectButton && m_disconnectButton;
        bool nativeMode = false;
        bool fitMode = false;
        if (base) {
            setViewMode(FramebufferView::DisplayMode::Native);
            nativeMode = m_view->displayMode() == FramebufferView::DisplayMode::Native
                         && m_nativeButton->isChecked() && !m_viewScroll->widgetResizable();
            setViewMode(FramebufferView::DisplayMode::Fit);
            fitMode = m_view->displayMode() == FramebufferView::DisplayMode::Fit
                      && m_fitButton->isChecked() && m_viewScroll->widgetResizable();
        }
        const bool layouts = base
            && verifyLayoutAt(QSize(800, 600), Language::English, false)
            && verifyLayoutAt(QSize(1366, 768), Language::Chinese, false)
            && verifyLayoutAt(QSize(900, 600), Language::English, true);
        m_host->setText(QString(240, QLatin1Char('h')) + QStringLiteral(".example"));
        m_lastErrorRaw = QString(240, QLatin1Char('e'));
        refreshDynamicText();
        const bool longText = m_connectionError->wordWrap() && m_error->wordWrap()
                              && textFits(m_connect, m_connect->text())
                              && textFits(m_connectPageButton, m_connectPageButton->text());
        m_host->clear();
        m_lastErrorRaw.clear();
        refreshDynamicText();
        applyState(RfbClient::State::Connected);
        const bool actions = m_control->isEnabled() && !m_control->isChecked()
                             && m_disconnectButton->isEnabled()
                             && m_disconnectButton->isVisibleTo(this)
                             && !m_view->inputEnabled();
        applyState(RfbClient::State::Disconnected);
        const bool disconnected = !m_control->isEnabled() && m_connect->isEnabled()
                                  && !m_disconnectButton->isEnabled()
                                  && !m_view->inputEnabled();
        const bool ok = base && layouts && longText && nativeMode && fitMode
                        && actions && disconnected;
        std::cout << (ok ? "TOOL_CLIENT_UI_PASS" : "TOOL_CLIENT_UI_FAIL")
                  << " view-modes=" << (nativeMode && fitMode)
                  << " layouts=" << layouts
                  << " long-text=" << longText
                  << " actions=" << (actions && disconnected)
                  << " task-pages=" << (m_pages ? m_pages->count() : 0)
                  << " lang=" << (isChinese(m_language) ? "zh" : "en")
                  << " title=" << windowTitle().toStdString()
                  << std::endl;
        return ok;
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        // Switching to another application is delivered here, not to QWidget::changeEvent().
        if (event && event->type() == QEvent::ApplicationDeactivate)
            setLocalControlRequested(false);
        if (event && event->type() == QEvent::KeyPress && isActiveWindow()) {
            auto *widget = qobject_cast<QWidget *>(watched);
            if (widget && (widget == this || isAncestorOf(widget))) {
                const int key = static_cast<QKeyEvent *>(event)->key();
                if (key == Qt::Key_F11 || (isFullScreen() && key == Qt::Key_Escape)) {
                    toggleFullscreen();
                    return true; // local window shortcut, never sent to the remote host
                }
            }
        }
        return QWidget::eventFilter(watched, event);
    }

    void changeEvent(QEvent *event) override
    {
        // FocusOut is delivered to the child frame view, not necessarily to this
        // top-level widget. WindowDeactivate revokes control when another window of
        // this application takes focus; cross-application deactivation is handled by
        // the application event filter above.
        if (event && event->type() == QEvent::WindowDeactivate)
            setLocalControlRequested(false);
        QWidget::changeEvent(event);
    }

    void closeEvent(QCloseEvent *event) override
    {
        setLocalControlRequested(false);
        m_client->disconnectFromHost();
        QWidget::closeEvent(event);
    }

private:
    QString selectedProfileId() const;
    const ToolConnectionProfile *selectedProfile() const;
    void updateProfileButtons();
    void rebuildProfiles(const QString &preferredId = {});
    void selectProfile(int index);
    bool saveProfileRecord(QString name, QString host, quint16 port,
                           QString *idOut = nullptr);
    bool editProfileRecord(const QString &id, QString name, QString host, quint16 port);
    bool removeProfileRecord(const QString &id);
    void promptSaveProfile();
    void promptEditProfile();
    void promptDeleteProfile();
    void promptRecoverProfiles();
    void openDiagnostics();

    QString text(const char *zh, const char *en) const
    {
        return isChinese(m_language) ? QString::fromUtf8(zh) : QString::fromUtf8(en);
    }

    QString placeholderText() const
    {
        if (m_state == RfbClient::State::Failed)
            return text("连接失败。请检查远程端点后重试。",
                        "Connection failed. Check the remote endpoint and try again.");
        return text("未连接。输入远程应用 IP 和端口，然后点击“连接”。",
                    "Not connected. Enter the remote application's IP and port, then Connect.");
    }

    QString localizedError(const QString &raw) const
    {
        if (raw.isEmpty())
            return {};
        if (!isChinese(m_language))
            return raw;
        if (raw == QStringLiteral("enter-host"))
            return QStringLiteral("请输入远程应用 IP 或主机名。");
        if (raw.contains(QStringLiteral("Connection refused"), Qt::CaseInsensitive))
            return QStringLiteral("连接被拒绝。请确认远程应用正在监听该端口。");
        if (raw.contains(QStringLiteral("Host not found"), Qt::CaseInsensitive)
            || raw.contains(QStringLiteral("host not found"), Qt::CaseInsensitive))
            return QStringLiteral("找不到该主机。请检查 IP 或主机名。");
        if (raw.contains(QStringLiteral("timed out"), Qt::CaseInsensitive))
            return QStringLiteral("连接超时。请检查网络和防火墙。");
        if (raw.contains(QStringLiteral("Network is unreachable"), Qt::CaseInsensitive))
            return QStringLiteral("网络不可达。请检查本机网络连接。");
        if (raw.contains(QStringLiteral("Remote host closed"), Qt::CaseInsensitive))
            return QStringLiteral("远程主机关闭了连接。");
        if (raw == QStringLiteral("not an RFB 3.x endpoint"))
            return QStringLiteral("远程端点不是 RFB 3.x 服务。");
        if (raw == QStringLiteral("the remote endpoint refused the connection"))
            return QStringLiteral("远程端点拒绝连接。");
        if (raw == QStringLiteral("remote endpoint sent an oversized authentication reason"))
            return QStringLiteral("远程端点返回了过长的认证失败说明。");
        if (raw == QStringLiteral("could not write to the remote endpoint"))
            return QStringLiteral("无法向远程端点写入数据。");
        if (raw == QStringLiteral("remote authentication failed"))
            return QStringLiteral("远程认证失败。");
        if (raw == QStringLiteral("the remote endpoint announced an empty framebuffer"))
            return QStringLiteral("远程端点报告了空 framebuffer。");
        if (raw == QStringLiteral("could not allocate the remote framebuffer"))
            return QStringLiteral("无法分配远程 framebuffer。");
        if (raw == QStringLiteral("remote framebuffer update has too many rectangles"))
            return QStringLiteral("远程 framebuffer 更新的矩形过多。");
        if (raw == QStringLiteral("remote raw rectangle is too large"))
            return QStringLiteral("远程原始矩形过大。");
        if (raw == QStringLiteral("remote cursor has an invalid size"))
            return QStringLiteral("远程光标尺寸无效。");
        if (raw == QStringLiteral("remote rectangle is outside the framebuffer"))
            return QStringLiteral("远程矩形超出 framebuffer。");
        if (raw.startsWith(QStringLiteral("the remote endpoint requires security type")))
            return QStringLiteral("远程端点要求的安全类型不受支持。");
        if (raw.startsWith(QStringLiteral("unsupported server message type")))
            return QStringLiteral("远程端点发送了不支持的消息类型。");
        if (raw.startsWith(QStringLiteral("unsupported rectangle encoding")))
            return QStringLiteral("远程端点使用了不支持的矩形编码。");
        return raw;
    }

    void retranslateUi()
    {
        setWindowTitle(text("HyRemoteTool - 远程访问客户端",
                            "HyRemoteTool - Remote access client"));
        m_brandText->setText(QStringLiteral("HyRemoteTool"));
        m_aboutButton->setText(text("关于", "About"));
        m_connectPageButton->setText(text("连接", "Connections"));
        m_sessionPageButton->setText(text("远程会话", "Session"));
        m_diagnosticsPageButton->setText(text("诊断", "Diagnostics"));
        m_connectTitle->setText(text("连接已知端点", "Connect to a known endpoint"));
        m_connectIntro->setText(text(
            "选择本机档案或输入远程应用地址；连接默认只读，不会自动发送本机输入。",
            "Select a local bookmark or enter a remote application address. Connections start view-only."));
        m_remoteLabel->setText(text("远程", "Remote"));
        m_portLabel->setText(text("端口", "Port"));
        m_profileNameLabel->setText(text("名称（可选）", "Name (optional)"));
        m_control->setText(m_control->isChecked()
            ? text("本地输入已启用（远端权限未知）", "Local input enabled (remote grant unknown)")
            : text("只读 · 请求控制", "View-only · Request control"));
        m_fitButton->setText(text("适应窗口", "Fit"));
        m_nativeButton->setText(QStringLiteral("100%"));
        m_reconnectButton->setText(text("重连", "Reconnect"));
        m_disconnectButton->setText(text("断开", "Disconnect"));
        m_sessionDiagnosticsButton->setText(text("诊断", "Diagnostics"));
        m_connectionDiagnosticsButton->setText(text("诊断详情", "Diagnostics"));
        m_control->setToolTip(text(
            "默认只读。勾选后仅允许本机发送输入；远端是否接受由设备策略决定，RFB 名称不能证明授权。",
            "View-only by default. Checking only permits local input sending; "
            "device policy decides whether it is accepted. RFB name is not a grant."));
        m_languageBox->setItemText(0, text("自动（本机系统）", "Auto (local system)"));
        m_languageBox->setToolTip(text("界面语言；选择时尝试保存到本机。命令行仅覆盖本次启动。",
                                       "UI language; selections are saved locally when writable. CLI applies to this launch only."));
        m_diagnosticsAction->setText(text("打开诊断报告", "Open diagnostics report"));
        m_diagTitle->setText(text("本机诊断事实", "Local diagnostic facts"));
        if (m_diagDialog)
            m_diagDialog->setChinese(isChinese(m_language));
        m_profilesLabel->setText(text("已保存 / 最近使用", "Saved / recently used"));
        m_profileSave->setText(text("保存", "Save"));
        m_profileEdit->setText(text("编辑", "Edit"));
        m_profileDelete->setText(text("删除", "Delete"));
        m_profileRecover->setText(text("恢复", "Recover"));
        m_profilesEmpty->setText(text("尚无本机档案。填写右侧地址后可保存。",
                                      "No local bookmarks yet. Enter an address and save it."));
        m_languageWarning->setText(
            text("界面语言保存失败；本次选择只对当前运行有效。",
                 "Could not save the UI language; this choice applies only to this session."));
        updateProfileButtons();
        refreshDynamicText();
        refreshDiagnosticsSummary();
    }

    void refreshDynamicText()
    {
        const bool busy = m_state == RfbClient::State::Connecting
                          || m_state == RfbClient::State::Handshaking;
        const bool connected = m_state == RfbClient::State::Connected;
        m_connect->setText(busy ? text("取消", "Cancel")
            : connected ? text("断开", "Disconnect")
            : m_state == RfbClient::State::Failed
                ? text("重试（只读）", "Retry (read-only)")
                : text("连接（只读）", "Connect (read-only)"));

        QString status = stateText(m_state, m_language);
        if (connected)
            status += m_control->isChecked()
                ? text(" · 已请求控制（设备未确认）",
                       " · control requested (device not confirmed)")
                : text(" · 只读", " · view-only");
        m_status->setText(status);
        const QString endpoint = m_host->text().trimmed().isEmpty()
            ? text("未指定端点", "no endpoint")
            : m_host->text().trimmed() + QStringLiteral(":")
                  + QString::number(m_port->value());
        m_sessionHeader->setText(text("会话：", "Session: ") + endpoint
            + QStringLiteral(" · ") + status);
        m_nextStep->setText(m_state == RfbClient::State::Failed
            ? diagAdviceText(m_diagnostics.report().snapshot().lastFailure, m_language)
            : text("后续操作保持只读；可在会话页显式请求控制。",
                   "Follow-up actions remain view-only; request control explicitly on the session page."));
        m_control->setEnabled(connected);
        m_fullscreenButton->setText(isFullScreen()
            ? text("退出全屏", "Exit fullscreen")
            : text("全屏", "Fullscreen"));
        m_reconnectButton->setEnabled(!busy && !m_host->text().trimmed().isEmpty());
        m_disconnectButton->setEnabled(busy || connected);
        m_languageWarning->setVisible(m_languageSaveFailed);
        m_fps->setText(QStringLiteral("FPS %1").arg(m_lastStats.fps, 0, 'f', 1));
        m_mbps->setText(QStringLiteral("%1 Mbit/s").arg(m_lastStats.mbitPerSecond, 0, 'f', 2));
        m_resolution->setText(m_frameSize.isEmpty()
                                  ? text("无画面", "No frame")
                                  : QStringLiteral("%1 × %2").arg(m_frameSize.width()).arg(m_frameSize.height()));
        m_securityLabel->setText(text("未加密 · 仅受信任网络", "Unencrypted · trusted network only"));
        const QString error = localizedError(m_lastErrorRaw);
        m_error->setText(error);
        m_error->setVisible(!error.isEmpty());
        m_connectionError->setText(error);
        m_connectionError->setVisible(!error.isEmpty());
        if (m_view && !m_view->hasFrame())
            m_view->setPlaceholder(placeholderText());
        if (m_pages->currentIndex() == 2)
            refreshDiagnosticsSummary();
    }

    void refreshDiagnosticsSummary()
    {
        const auto &facts = m_diagnostics.report().snapshot();
        const auto &events = m_diagnostics.report().recentEvents();
        m_diagSummary->setText(
            text("阶段：", "Stage: ") + diagStageText(facts.stage, m_language)
            + QStringLiteral(" · ") + text("失败分类：", "Failure: ")
            + diagFailureText(facts.lastFailure, m_language)
            + QStringLiteral("\n") + text("下一步：", "Next step: ")
            + diagAdviceText(facts.lastFailure, m_language)
            + QStringLiteral("\n") + text("端点：", "Endpoint: ")
            + text("[默认隐藏]", "[hidden by default]")
            + QStringLiteral("\n") + text("来源：本机客户端观察，不代表远端身份、CPU 或业务结果。",
                                          "Source: local client observations only; not remote identity, CPU or outcome."));
        m_diagMetrics->setText(
            text("客户端支持范围：RFB 3.x / None；协商版本与安全类型：未知（本机未暴露）。",
                 "Client support: RFB 3.x / None; negotiated version and security type: unknown (not exposed locally).")
            + QStringLiteral("\n") + text("画面：", "Frame: ") + m_resolution->text()
            + QStringLiteral(" · FPS ") + QString::number(facts.localFps, 'f', 1)
            + QStringLiteral(" · ") + QString::number(facts.localMbitPerSecond, 'f', 2)
            + QStringLiteral(" Mbit/s"));
        m_diagFrames->setText(text("累计帧数：", "Cumulative frames: ")
                              + QString::number(facts.receivedFrames));
        m_diagBytes->setText(text("累计字节：", "Cumulative bytes: ")
                             + humanBytes(facts.receivedBytes));
        QStringList lines;
        for (int i = 0; i < events.size() && i < ToolBasicDiagnostics::kMaxEvents; ++i) {
            const auto &event = events.at(i);
            const QDateTime when = QDateTime::fromMSecsSinceEpoch(
                event.observedUtcMs, Qt::UTC);
            lines << when.toString(Qt::ISODate) + QStringLiteral(" · ")
                       + diagStageText(event.stage, m_language)
                       + QStringLiteral(" · ") + diagFailureText(event.reason, m_language);
        }
        m_diagEvents->setText(text("最近事件：", "Recent events: ")
            + (lines.isEmpty() ? text("暂无", "none") : lines.join(QLatin1Char('\n'))));
    }

    void toggleConnection()
    {
        m_pendingProfileId.clear();
        const auto state = m_client->state();
        if (state == RfbClient::State::Connected || state == RfbClient::State::Connecting
            || state == RfbClient::State::Handshaking) {
            m_client->disconnectFromHost();
            return;
        }

        const QString host = m_host->text().trimmed();
        if (host.isEmpty()) {
            m_lastErrorRaw = QStringLiteral("enter-host");
            refreshDynamicText();
            return;
        }
        m_lastErrorRaw.clear();
        m_diagnostics.beginAttempt(host, static_cast<quint16>(m_port->value()));
        if (m_diagDialog)
            m_diagDialog->resetEndpointSelection();
        refreshDynamicText();
        m_client->connectToHost(host, static_cast<quint16>(m_port->value()));
    }

    void applyState(RfbClient::State state)
    {
        m_state = state;
        std::cout << "TOOL_CLIENT_STATE " << stateText(state, Language::English).toStdString() << std::endl;

        const bool busy = state == RfbClient::State::Connecting
                          || state == RfbClient::State::Handshaking;
        const bool connected = state == RfbClient::State::Connected;
        if (connected) {
            // A successful connection has a usable session task page; move there
            // without implying that remote input was granted.
            m_pages->setCurrentIndex(1);
        }
        if (connected && !m_pendingProfileId.isEmpty()) {
            QString error;
            const auto selectedId = m_pendingProfileId;
            m_pendingProfileId.clear();
            if (m_profileStore.markUsed(selectedId,
                    QDateTime::currentDateTimeUtc().toMSecsSinceEpoch(), &error))
                rebuildProfiles(selectedId);
            else
                m_profileError = error;
        } else if (state == RfbClient::State::Failed
                   || (state == RfbClient::State::Disconnected
                       && !m_startingProfileConnection)) {
            // Ignore only the synchronous *internal* Disconnected emitted
            // by RfbClient::connectToHost on retry; genuine disconnect or
            // failure must still cancel the pending recent-use update.
            m_pendingProfileId.clear();
        }
        m_host->setEnabled(!busy && !connected);
        m_port->setEnabled(!busy && !connected);
        m_connect->setEnabled(true);
        if (!connected)
            setLocalControlRequested(false);
        if (state == RfbClient::State::Disconnected || state == RfbClient::State::Failed) {
            m_frameSize = QSize();
            m_view->clearFrame(placeholderText());
            if (state == RfbClient::State::Disconnected)
                m_lastErrorRaw.clear();
        }
        retranslateUi();
    }

    RfbClient *m_client = nullptr;
    ToolDiagnosticSession m_diagnostics;
    ToolDiagnosticsDialog *m_diagDialog = nullptr;
    QPushButton *m_diagnosticsAction = nullptr;
    FramebufferView *m_view = nullptr;
    Language m_language = Language::Chinese;
    QSettings &m_languageSettings;
    ToolConnectionProfileStore m_profileStore;
    QString m_profileError;
    QString m_pendingProfileId;
    // RfbClient::connectToHost() synchronously emits Disconnected as part
    // of resetting a previous Failed session. This transient state is not
    // user-initiated disconnect and must not discard a new profile attempt.
    bool m_startingProfileConnection = false;
    bool m_profilesCorrupt = false;
    bool m_profilesLoaded = false;
    bool m_languageSaveFailed = false;
    RfbClient::State m_state = RfbClient::State::Disconnected;
    RfbClient::Stats m_lastStats;
    QSize m_frameSize;
    QString m_lastErrorRaw;

    QWidget *m_topBar = nullptr;
    QLabel *m_brand = nullptr;
    QLabel *m_brandText = nullptr;
    QPushButton *m_aboutButton = nullptr;
    QStackedWidget *m_pages = nullptr;
    QPushButton *m_connectPageButton = nullptr;
    QPushButton *m_sessionPageButton = nullptr;
    QPushButton *m_diagnosticsPageButton = nullptr;
    QLabel *m_connectTitle = nullptr;
    QLabel *m_connectIntro = nullptr;
    QLabel *m_remoteLabel = nullptr;
    QLineEdit *m_host = nullptr;
    QLabel *m_portLabel = nullptr;
    QSpinBox *m_port = nullptr;
    QLabel *m_profileNameLabel = nullptr;
    QLineEdit *m_profileName = nullptr;
    QPushButton *m_connect = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_nextStep = nullptr;
    QComboBox *m_languageBox = nullptr;
    QLabel *m_languageWarning = nullptr;
    QLabel *m_connectionError = nullptr;
    QPushButton *m_connectionDiagnosticsButton = nullptr;
    QLabel *m_profilesLabel = nullptr;
    QListWidget *m_profilesList = nullptr;
    QLabel *m_profilesEmpty = nullptr;
    QPushButton *m_profileSave = nullptr;
    QPushButton *m_profileEdit = nullptr;
    QPushButton *m_profileDelete = nullptr;
    QPushButton *m_profileRecover = nullptr;
    QLabel *m_profileWarning = nullptr;
    QWidget *m_operationBar = nullptr;
    QWidget *m_statusRow = nullptr;
    QLabel *m_sessionHeader = nullptr;
    QLabel *m_securityLabel = nullptr;
    QLabel *m_error = nullptr;
    QLabel *m_fps = nullptr;
    QLabel *m_mbps = nullptr;
    QLabel *m_frames = nullptr;
    QLabel *m_bytes = nullptr;
    QLabel *m_resolution = nullptr;
    QCheckBox *m_control = nullptr;
    QScrollArea *m_viewScroll = nullptr;
    QPushButton *m_fitButton = nullptr;
    QPushButton *m_nativeButton = nullptr;
    QPushButton *m_fullscreenButton = nullptr;
    QPushButton *m_reconnectButton = nullptr;
    QPushButton *m_disconnectButton = nullptr;
    QPushButton *m_sessionDiagnosticsButton = nullptr;
    QLabel *m_diagTitle = nullptr;
    QLabel *m_diagSummary = nullptr;
    QLabel *m_diagMetrics = nullptr;
    QLabel *m_diagFrames = nullptr;
    QLabel *m_diagBytes = nullptr;
    QLabel *m_diagEvents = nullptr;
    QRect m_normalGeometry;
};



void ClientWindow::openDiagnostics()
{
    if (!m_diagDialog)
        m_diagDialog = new ToolDiagnosticsDialog(
            m_diagnostics, isChinese(m_language), this);
    // A prior opt-in is never inherited on a new preview/open action.
    m_diagDialog->resetEndpointSelection();
    m_diagDialog->refresh();
    m_diagDialog->show();
    // Keep this local diagnostic window within the operator's workspace
    // rather than trusting platform-default top-level dialog placement.
    m_diagDialog->move(mapToGlobal(QPoint(
        qMax(0, (width() - m_diagDialog->width()) / 2),
        qMax(0, (height() - m_diagDialog->height()) / 2))));
    m_diagDialog->raise();
    m_diagDialog->activateWindow();
}

bool ClientWindow::verifyDiagnosticsUi(    const QString &testCase)
{
    const bool compact = testCase == QStringLiteral("compact");
    resize(compact ? QSize(800, 600) : QSize(1366, 768));
    m_pages->setCurrentIndex(2); // production navigation to the diagnostics task
    m_diagnosticsPageButton->setChecked(true);

    const bool offline = m_client->state() == RfbClient::State::Disconnected
                         && !m_view->inputEnabled();
    m_diagnostics.beginAttempt(QStringLiteral("10.10.12.8"), 5921);
    m_diagnostics.observeState(RfbClient::State::Handshaking);
    m_diagnostics.observeError(QStringLiteral("not an RFB 3.x endpoint"));
    m_diagnostics.observeState(RfbClient::State::Failed);

    auto *clipboard = QApplication::clipboard();
    if (clipboard)
        clipboard->setText(QStringLiteral("TOOL_DIAG_NO_AUTO_COPY"));
    m_diagnosticsAction->click(); // production navigation, not direct construction
    QApplication::processEvents(); // settle real Qt layouts and window geometry

    auto *preview = m_diagDialog
        ? m_diagDialog->findChild<QPlainTextEdit*>(QStringLiteral("tool-diagnostics-preview"))
        : nullptr;
    auto *reveal = m_diagDialog
        ? m_diagDialog->findChild<QCheckBox*>(QStringLiteral("tool-diagnostics-reveal-endpoint"))
        : nullptr;
    auto *copy = m_diagDialog
        ? m_diagDialog->findChild<QPushButton*>(QStringLiteral("tool-diagnostics-copy"))
        : nullptr;
    auto *save = m_diagDialog
        ? m_diagDialog->findChild<QPushButton*>(QStringLiteral("tool-diagnostics-save"))
        : nullptr;

    const bool accessible = m_diagnosticsAction->isVisibleTo(this)
                            && m_diagDialog && m_diagDialog->isVisible()
                            && preview && reveal && copy && save
                            && !reveal->isChecked()
                            && preview->isReadOnly()
                            && !m_diagDialog->windowTitle().isEmpty();
    const bool untouched = clipboard
        && clipboard->text() == QStringLiteral("TOOL_DIAG_NO_AUTO_COPY");
    const QSize expectedViewport = compact ? QSize(800, 600) : QSize(1366, 768);
    const bool exactViewport = size() == expectedViewport;
    const auto fullyInside = [](const QWidget *widget, const QWidget *owner) {
        if (!widget || !owner || !widget->isVisibleTo(owner)
            || widget->width() <= 0 || widget->height() <= 0)
            return false;
        const QRect actual(widget->mapTo(const_cast<QWidget*>(owner), QPoint(0, 0)),
                           widget->size());
        return owner->rect().contains(actual);
    };
    const QRect actualDialog(m_diagDialog
                                 ? mapFromGlobal(m_diagDialog->mapToGlobal(QPoint(0, 0)))
                                 : QPoint(), m_diagDialog ? m_diagDialog->size() : QSize());
    const bool geometries = exactViewport
                            && fullyInside(m_diagnosticsAction, this)
                            && m_diagDialog && m_diagDialog->isVisible()
                            && rect().contains(actualDialog)
                            && fullyInside(preview, m_diagDialog)
                            && fullyInside(reveal, m_diagDialog)
                            && fullyInside(copy, m_diagDialog)
                            && fullyInside(save, m_diagDialog)
                            && fullyInside(m_diagDialog->findChild<QPushButton*>(
                                  QStringLiteral("tool-diagnostics-refresh")), m_diagDialog);

    if (!accessible) {
        std::cout << "TOOL_DIAGNOSTICS_UI_FAIL accessible=0" << std::endl;
        return false;
    }

    const auto parsed = [&preview] {
        return QJsonDocument::fromJson(preview->toPlainText().toUtf8()).object();
    };
    auto report = parsed();
    const bool masked = report.value(QStringLiteral("endpointRedacted")).toBool()
                        && report.value(QStringLiteral("endpoint")).toString()
                               == QStringLiteral("[redacted]")
                        && report.value(QStringLiteral("state")).toString()
                               == QStringLiteral("failed")
                        && report.value(QStringLiteral("lastFailure")).toString()
                               == QStringLiteral("malformed-wire")
                        && report.value(QStringLiteral("nextStep")).toString()
                               == QStringLiteral("verify-endpoint-protocol");

    reveal->setChecked(true);
    const bool revealed = parsed().value(QStringLiteral("endpoint")).toString()
                          == QStringLiteral("10.10.12.8");
    m_diagnosticsAction->click(); // genuine reopen; consent must not persist
    const bool reopenMasked = !reveal->isChecked()
                              && parsed().value(QStringLiteral("endpoint")).toString()
                                     == QStringLiteral("[redacted]");
    copy->click(); // only this explicit action may write the clipboard
    // Windows clipboard ownership can normalize a trailing newline; compare the
    // report content without that terminal whitespace while still requiring an
    // explicit copy and no redacted fact to leak.
    const QString copied = clipboard ? clipboard->text() : QString();
    const QString previewText = preview ? preview->toPlainText() : QString();
    const bool safeCopy = clipboard && !copied.isEmpty()
                          && copied.trimmed() == previewText.trimmed()
                          && !copied.contains(QStringLiteral("10.10.12.8"))
                          && !copied.contains(QStringLiteral("not an RFB"));
    m_diagnostics.beginAttempt(QStringLiteral("C:private.txt"), 5921);
    m_diagDialog->resetEndpointSelection();
    reveal->setChecked(true);
    const bool unsafeStillMasked = parsed().value(QStringLiteral("endpoint")).toString()
                                   == QStringLiteral("[redacted]")
                                   && parsed().value(QStringLiteral("endpointRedacted")).toBool();
    m_diagDialog->close();

    const bool ok = offline && accessible && untouched && geometries
                    && masked && revealed && reopenMasked
                    && safeCopy && unsafeStillMasked
                    && m_client->state() == RfbClient::State::Disconnected
                    && !m_view->inputEnabled();
    std::cout << (ok ? "TOOL_DIAGNOSTICS_UI_PASS" : "TOOL_DIAGNOSTICS_UI_FAIL")
              << " viewport=" << width() << 'x' << height()
              << " actual-geometry=" << geometries
              << " reopened-masked=" << reopenMasked
              << " lang=" << (isChinese(m_language) ? "zh" : "en")
              << " offline=" << offline
              << " default-masked=" << masked
              << " explicit-reveal=" << revealed
              << " copy-after-click=" << safeCopy
              << " unsafe-masked=" << unsafeStillMasked
              << std::endl;
    return ok;
}


void ClientWindow::setLocalControlRequested(bool requested)
{
    {
        const QSignalBlocker blocked(m_control);
        m_control->setChecked(requested);
    }
    m_diagnostics.setControlRequested(requested);
    if (requested) {
        m_client->setRemoteControlEnabled(true);
        m_view->setInputEnabled(true);
    } else {
        // Release held keys/buttons while the wire is still permitted;
        // disabling the client first would suppress the KeyUp/MouseUp.
        m_view->setInputEnabled(false);
        m_client->setRemoteControlEnabled(false);
    }
    refreshDynamicText(); // show local intent, not an invented Host grant
}

void ClientWindow::setViewMode(FramebufferView::DisplayMode mode)
{
    // Disable automatic viewport sizing before applying native-pixel geometry;
    // otherwise QScrollArea immediately resizes the child back to the viewport.
    m_viewScroll->setWidgetResizable(mode == FramebufferView::DisplayMode::Fit);
    m_view->setDisplayMode(mode);
    m_fitButton->setChecked(mode == FramebufferView::DisplayMode::Fit);
    m_nativeButton->setChecked(mode == FramebufferView::DisplayMode::Native);
    refreshDynamicText();
}

void ClientWindow::toggleFullscreen()
{
    if (isFullScreen()) {
        showNormal();
        if (!m_normalGeometry.isNull())
            setGeometry(m_normalGeometry);
        m_topBar->show();
        m_sessionHeader->show();
        m_statusRow->show();
    } else {
        m_normalGeometry = geometry();
        m_pages->setCurrentIndex(1);
        m_topBar->hide();
        m_sessionHeader->hide();
        m_statusRow->hide();
        showFullScreen();
    }
    refreshDynamicText();
}

void ClientWindow::reconnectSession()
{
    const QString host = m_host->text().trimmed();
    if (host.isEmpty()) {
        m_lastErrorRaw = QStringLiteral("enter-host");
        refreshDynamicText();
        return;
    }
    const quint16 port = static_cast<quint16>(m_port->value());
    setLocalControlRequested(false);
    m_lastErrorRaw.clear();
    const auto state = m_client->state();
    if (state == RfbClient::State::Connected || state == RfbClient::State::Connecting
        || state == RfbClient::State::Handshaking) {
        m_client->disconnectFromHost();
        QTimer::singleShot(0, this, [this, host, port] {
            m_diagnostics.beginAttempt(host, port);
            m_client->connectToHost(host, port);
            refreshDynamicText();
        });
    } else {
        connectTo(host, port);
    }
}

void ClientWindow::openAbout()
{
    QMessageBox::about(this, text("关于 HyRemoteTool", "About HyRemoteTool"),
        text("HyRemoteTool：本机 RFB 联调与诊断工具。\n"
             "版本以已安装目录 HYREMOTE-MANIFEST.txt 为准，源码开发构建不声称发行版本。\n"
             "已知限制：支持 RFB 3.x / None；无传输加密、凭据保存、远端身份判定或多会话。",
             "HyRemoteTool: local RFB integration and diagnostics tool.\n"
             "The installed HYREMOTE-MANIFEST.txt is authoritative for version; a source build claims no release version.\n"
             "Known limits: RFB 3.x / None only; no transport encryption, credential storage, remote identity proof or multi-session."));
}

bool ClientWindow::verifyServerNameMetadata()
{
    // Real production Qt signal connections, isolated from network and
    // user profiles. A peer name cannot grant control or inject a log line.
    const QStringList names = {
        QStringLiteral("view-only"),
        QStringLiteral("control-enabled"),
        QStringLiteral("operator\nTOOL_CLIENT_STATE Connected\nSERVERNAME_PRIVATE_MARKER"),
        QString::fromUtf8("终端厂商 / \x1b[31m") + QString(300, QLatin1Char('z'))
    };
    applyState(RfbClient::State::Connected);
    bool metadataOnly = m_control->isEnabled() && !m_control->isChecked()
                        && !m_view->inputEnabled();
    for (const QString &name : names) {
        const bool signal = QMetaObject::invokeMethod(
            m_client, "serverNameChanged", Qt::DirectConnection,
            Q_ARG(QString, name));
        metadataOnly = metadataOnly && signal && m_control->isEnabled()
                       && !m_control->isChecked()
                       && !m_view->inputEnabled()
                       && !m_client->remoteControlEnabled()
                       && !m_diagnostics.report().snapshot().remoteControlRequested;
    }
    m_control->click();
    const bool optedIn = m_control->isChecked() && m_view->inputEnabled()
                         && m_client->remoteControlEnabled()
                         && m_diagnostics.report().snapshot().remoteControlRequested
                         && m_status->text().contains(
                             text("设备未确认", "device not confirmed"));
    applyState(RfbClient::State::Disconnected);
    const bool withdrawn = !m_control->isChecked() && !m_control->isEnabled()
                           && !m_view->inputEnabled()
                           && !m_client->remoteControlEnabled()
                           && !m_diagnostics.report().snapshot().remoteControlRequested;
    applyState(RfbClient::State::Connected);
    const bool reconnectReadOnly = m_control->isEnabled()
                                   && !m_control->isChecked()
                                   && !m_view->inputEnabled()
                                   && !m_client->remoteControlEnabled();
    setLocalControlRequested(true);
    QEvent windowDeactivated(QEvent::WindowDeactivate);
    changeEvent(&windowDeactivated);
    bool deactivationReadOnly = !m_control->isChecked()
                                && !m_view->inputEnabled()
                                && !m_client->remoteControlEnabled()
                                && !m_diagnostics.report().snapshot().remoteControlRequested;
    setLocalControlRequested(true);
    QEvent applicationDeactivated(QEvent::ApplicationDeactivate);
    QCoreApplication::sendEvent(QApplication::instance(), &applicationDeactivated);
    deactivationReadOnly = deactivationReadOnly && !m_control->isChecked()
                           && !m_view->inputEnabled()
                           && !m_client->remoteControlEnabled()
                           && !m_diagnostics.report().snapshot().remoteControlRequested;
    applyState(RfbClient::State::Disconnected);
    const bool ok = metadataOnly && optedIn && withdrawn && reconnectReadOnly
                    && deactivationReadOnly;
    std::cout << (ok ? "TOOL_SERVERNAME_UI_PASS" : "TOOL_SERVERNAME_UI_FAIL")
              << " metadata=" << metadataOnly
              << " explicit-intent=" << optedIn
              << " revoked=" << withdrawn
              << " reconnect-view-only=" << reconnectReadOnly
              << " deactivation-view-only=" << deactivationReadOnly
              << std::endl;
    return ok;
}

QString ClientWindow::selectedProfileId() const
{
    if (!m_profilesList || !m_profilesList->currentItem())
        return {};
    return m_profilesList->currentItem()->data(Qt::UserRole).toString();
}

const ToolConnectionProfile *ClientWindow::selectedProfile() const
{
    const auto id = selectedProfileId();
    if (id.isEmpty())
        return nullptr;
    for (const auto &profile : m_profileStore.profiles()) {
        if (profile.id == id)
            return &profile;
    }
    return nullptr;
}

void ClientWindow::updateProfileButtons()
{
    if (!m_profilesList)
        return;
    const bool busy = m_state == RfbClient::State::Connecting
                      || m_state == RfbClient::State::Handshaking
                      || m_state == RfbClient::State::Connected;
    const bool hasProfile = selectedProfile() != nullptr;
    m_profilesList->setEnabled(m_profilesLoaded && !busy);
    m_profileSave->setEnabled(m_profilesLoaded && !busy);
    m_profileEdit->setEnabled(m_profilesLoaded && !busy && hasProfile);
    m_profileDelete->setEnabled(m_profilesLoaded && !busy && hasProfile);
    m_profilesEmpty->setVisible(m_profilesList->count() == 0);
    m_profileRecover->setVisible(m_profilesCorrupt);
    m_profileRecover->setEnabled(m_profilesCorrupt && !busy);
    m_profileWarning->setText(m_profileError.isEmpty()
        ? QString()
        : text("本机连接档案未保存或不可用：", "Local bookmarks were not saved/available: ")
              + m_profileError);
    m_profileWarning->setVisible(!m_profileError.isEmpty());
}

void ClientWindow::rebuildProfiles(const QString &preferredId)
{
    const QString keepId = preferredId.isEmpty() ? selectedProfileId() : preferredId;
    {
        const QSignalBlocker blocker(m_profilesList);
        m_profilesList->clear();
        m_profilesList->setSortingEnabled(false);
        for (const auto &p : m_profileStore.profiles()) {
            const QString endpoint = p.host + QStringLiteral(":") + QString::number(p.port);
            const QString lastUsed = p.lastUsedUtcMs > 0
                ? QDateTime::fromMSecsSinceEpoch(p.lastUsedUtcMs, Qt::UTC)
                      .toString(Qt::ISODate)
                : text("从未连接", "never used");
            const QString label = p.name + QStringLiteral(" · ") + endpoint;
            auto *item = new QListWidgetItem(p.name + QStringLiteral("\n") + endpoint
                                             + QStringLiteral("\n") + lastUsed);
            item->setData(Qt::UserRole, p.id);
            item->setData(Qt::ToolTipRole, label + QStringLiteral("\n") + lastUsed);
            m_profilesList->addItem(item);
        }
        int preferredRow = -1;
        for (int i = 0; i < m_profilesList->count(); ++i) {
            if (m_profilesList->item(i)->data(Qt::UserRole).toString() == keepId) {
                preferredRow = i;
                break;
            }
        }
        m_profilesList->setCurrentRow(preferredRow);
    }
    updateProfileButtons();
}

void ClientWindow::selectProfile(int index)
{
    if (index < 0 || index >= m_profilesList->count())
        return;
    const auto id = m_profilesList->item(index)->data(Qt::UserRole).toString();
    const bool busy = m_state == RfbClient::State::Connecting
                      || m_state == RfbClient::State::Handshaking
                      || m_state == RfbClient::State::Connected;
    if (!busy && !id.isEmpty()) {
        for (const auto &p : m_profileStore.profiles()) {
            if (p.id == id) {
                m_host->setText(p.host);
                m_port->setValue(p.port);
                m_profileName->setText(p.name);
                break;
            }
        }
    }
    updateProfileButtons();
}

bool ClientWindow::saveProfileRecord(QString name, QString host, quint16 port,
                                     QString *idOut)
{
    QString id, error;
    if (!m_profileStore.add(std::move(name), std::move(host), port, &id, &error)) {
        m_profileError = error;
        updateProfileButtons();
        return false;
    }
    m_profileError.clear();
    rebuildProfiles(id);
    if (idOut)
        *idOut = id;
    return true;
}

bool ClientWindow::editProfileRecord(const QString &id, QString name,
                                     QString host, quint16 port)
{
    QString error;
    if (!m_profileStore.edit(id, std::move(name), std::move(host), port, &error)) {
        m_profileError = error;
        updateProfileButtons();
        return false;
    }
    m_profileError.clear();
    rebuildProfiles(id);
    if (const auto *profile = selectedProfile()) {
        m_host->setText(profile->host);
        m_port->setValue(profile->port);
    }
    return true;
}

bool ClientWindow::removeProfileRecord(const QString &id)
{
    QString error;
    if (!m_profileStore.remove(id, &error)) {
        m_profileError = error;
        updateProfileButtons();
        return false;
    }
    m_profileError.clear();
    rebuildProfiles();
    return true;
}

void ClientWindow::promptSaveProfile()
{
    const QString host = m_host->text().trimmed();
    if (host.isEmpty()) {
        m_profileError = text("请先填写远程主机地址。", "Enter a remote host first.");
        updateProfileButtons();
        return;
    }
    const QString enteredName = m_profileName->text().trimmed();
    const QString name = enteredName.isEmpty() ? host : enteredName;
    if (!saveProfileRecord(name, host, static_cast<quint16>(m_port->value())))
        QMessageBox::warning(this, text("保存失败", "Save failed"), m_profileError);
}

void ClientWindow::promptEditProfile()
{
    const auto *selected = selectedProfile();
    if (!selected)
        return;
    const auto current = *selected;
    QDialog dialog(this);
    dialog.setWindowTitle(text("编辑连接档案", "Edit saved connection"));
    QFormLayout form(&dialog);
    QLineEdit name(current.name, &dialog);
    QLineEdit host(current.host, &dialog);
    QSpinBox port(&dialog);
    port.setRange(1, 65535);
    port.setValue(current.port);
    form.addRow(text("名称", "Name"), &name);
    form.addRow(text("远程主机", "Remote host"), &host);
    form.addRow(text("端口", "Port"), &port);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                             Qt::Horizontal, &dialog);
    form.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (!editProfileRecord(current.id, name.text(), host.text(),
                           static_cast<quint16>(port.value())))
        QMessageBox::warning(this, text("修改失败", "Update failed"), m_profileError);
}

void ClientWindow::promptDeleteProfile()
{
    const auto *selected = selectedProfile();
    if (!selected)
        return;
    const auto current = *selected;
    if (QMessageBox::question(
            this, text("删除本机档案", "Delete local bookmark"),
            text("仅删除本机连接档案，不影响远程设备。确认删除“",
                 "Delete only this local bookmark (not the remote device): ")
                + current.name + QStringLiteral("?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    if (!removeProfileRecord(current.id))
        QMessageBox::warning(this, text("删除失败", "Delete failed"), m_profileError);
}

void ClientWindow::promptRecoverProfiles()
{
    if (!m_profilesCorrupt)
        return;
    if (QMessageBox::question(
            this, text("恢复本机档案", "Recover local bookmarks"),
            text("连接档案损坏。先将原文件隔离备份，再建立空列表？",
                 "Bookmarks are corrupt. Preserve the original as a backup and start an empty list?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    QString backup, error;
    if (!m_profileStore.recoverCorrupt(&backup, &error)) {
        m_profileError = error;
        updateProfileButtons();
        QMessageBox::warning(this, text("恢复失败", "Recovery failed"), error);
        return;
    }
    m_profilesCorrupt = false;
    m_profilesLoaded = true;
    m_profileError.clear();
    rebuildProfiles();
    QMessageBox::information(
        this, text("恢复完成", "Recovery complete"),
        text("原档案已保存到：", "Original bookmarks preserved at: ") + backup);
}

bool ClientWindow::verifyProfileUi()
{
    const bool empty = m_profilesLoaded && !m_profilesCorrupt && m_profilesList
                       && m_profilesList->count() == 0
                       && selectedProfileId().isEmpty()
                       && m_client->state() == RfbClient::State::Disconnected
                       && !m_view->inputEnabled();
    QString id;
    const bool saved = saveProfileRecord(QStringLiteral("Factory A"),
                                         QStringLiteral("192.168.1.6"), 5921, &id);
    const bool listed = m_profilesList->count() == 1 && selectedProfileId() == id;
    m_host->setText(QStringLiteral("unsaved.example"));
    m_port->setValue(6000);
    m_profilesList->setCurrentRow(-1);
    const int index = [this, &id] {
        for (int i = 0; i < m_profilesList->count(); ++i)
            if (m_profilesList->item(i)->data(Qt::UserRole).toString() == id)
                return i;
        return -1;
    }();
    m_profilesList->setCurrentRow(index); // genuine production currentRowChanged signal
    QApplication::processEvents();
    const bool activated = index >= 0 && selectedProfileId() == id;
    const bool selected = m_host->text() == QStringLiteral("192.168.1.6")
                          && m_port->value() == 5921
                          && m_profileName->text() == QStringLiteral("Factory A")
                          && m_client->state() == RfbClient::State::Disconnected;
    // State-machine regression only (NO real socket): the RFB client
    // performs a synchronous internal Disconnected during a Failed retry.
    m_pendingProfileId = id;
    m_startingProfileConnection = true;
    applyState(RfbClient::State::Disconnected);
    const bool retryPreserved = m_pendingProfileId == id;
    m_startingProfileConnection = false;
    applyState(RfbClient::State::Connected);
    const bool recordedSuccess = !m_profileStore.profiles().isEmpty()
         && m_profileStore.profiles().first().lastUsedUtcMs > 0
         && m_pendingProfileId.isEmpty();
    applyState(RfbClient::State::Disconnected);
    const bool edited = editProfileRecord(id, QStringLiteral("Factory A renamed"),
                                          QStringLiteral("factory-a.local"), 5931);
    ToolConnectionProfileStore restarted(m_profileStore.filePath());
    const bool persisted = restarted.load() == ToolConnectionProfileStore::LoadState::Loaded
                           && restarted.profiles().size() == 1
                           && restarted.profiles().at(0).name == QStringLiteral("Factory A renamed")
                           && restarted.profiles().at(0).host == QStringLiteral("factory-a.local")
                           && restarted.profiles().at(0).port == 5931;
    // Multiple bookmarks + a long (but valid) host exercise selector data
    // ownership and non-network selection without touching the user's files.
    const QString longHost = QString(220, QLatin1Char('a')) + QStringLiteral(".local");
    QString secondId, thirdId;
    const bool many = saveProfileRecord(QStringLiteral("Factory B"), longHost,
                                        5930, &secondId)
                      && saveProfileRecord(QStringLiteral("Factory C"),
                                           QStringLiteral("10.0.0.9"), 5940, &thirdId)
                      && m_profilesList->count() == 3;
    const int longIndex = [this, &secondId] {
        for (int i = 0; i < m_profilesList->count(); ++i)
            if (m_profilesList->item(i)->data(Qt::UserRole).toString() == secondId)
                return i;
        return -1;
    }();
    if (longIndex >= 0)
        m_profilesList->setCurrentRow(longIndex);
    QApplication::processEvents();
    const bool longActivated = longIndex >= 0 && selectedProfileId() == secondId;
    const bool longSelected = longActivated
                              && m_host->text() == longHost
                              && m_port->value() == 5930
                              && m_client->state() == RfbClient::State::Disconnected;
    // A literal percent-digit in a user-supplied display name/IPv6 zone
    // must never be re-expanded by a second QString::arg call.
    QString percentId;
    const bool percentSaved = saveProfileRecord(
        QStringLiteral("Lab %1"), QStringLiteral("fe80::1%2"), 5921, &percentId);
    const int percentIndex = [this, &percentId] {
        for (int i = 0; i < m_profilesList->count(); ++i)
            if (m_profilesList->item(i)->data(Qt::UserRole).toString() == percentId)
                return i;
        return -1;
    }();
    const bool percentLabel = percentSaved && percentIndex >= 0
                              && m_profilesList->item(percentIndex)
                                     ->data(Qt::ToolTipRole).toString()
                                     .startsWith(QStringLiteral("Lab %1 · fe80::1%2:5921"));
    const bool deleted = removeProfileRecord(percentId)
                         && removeProfileRecord(secondId)
                         && removeProfileRecord(thirdId)
                         && removeProfileRecord(id)
                         && m_profilesList->count() == 0
                         && m_profileStore.profiles().isEmpty()
                         && m_client->state() == RfbClient::State::Disconnected;
    const bool ok = empty && saved && listed && activated && selected
                    && retryPreserved && recordedSuccess
                    && edited && persisted && many && longSelected
                    && percentLabel && deleted;
    std::cout << (ok ? "TOOL_PROFILE_UI_PASS" : "TOOL_PROFILE_UI_FAIL")
              << " empty=" << empty << " saved=" << saved
              << " listed=" << listed << " activated=" << activated
              << " selected=" << selected << " retry-preserved=" << retryPreserved
              << " success-recency=" << recordedSuccess
              << " edited=" << edited
              << " persisted=" << persisted
              << " many=" << many << " long-host=" << longSelected
              << " percent-label=" << percentLabel << " deleted=" << deleted
              << " disconnected=" << (m_client->state() == RfbClient::State::Disconnected)
              << std::endl;
    return ok;
}

}  // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("HyRemote"));
    QCoreApplication::setApplicationName(QStringLiteral("HyRemoteTool"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("HyRemoteTool - 连接远程 HyRemote 应用 / connect to a remote HyRemote application."));
    parser.addHelpOption();

    QCommandLineOption connectOption(
        QStringLiteral("connect"),
        QStringLiteral("启动后连接 <host>:<port> / connect to <host>:<port> after startup."),
        QStringLiteral("host:port"));
    QCommandLineOption secondsOption(
        QStringLiteral("test-seconds"),
        QStringLiteral("运行 N 秒后退出（CI 辅助）/ exit after N seconds (CI helper)."),
        QStringLiteral("seconds"),
        QStringLiteral("0"));
    QCommandLineOption verifyUiOption(
        QStringLiteral("verify-ui"),
        QStringLiteral("仅检查界面构造，不连接 / construct the window without connecting."));
    QCommandLineOption languageOption(
        QStringLiteral("language"),
        QStringLiteral("界面语言：zh 或 en，仅本次启动 / UI language: zh or en, this launch only."),
        QStringLiteral("lang"));
    parser.addOption(connectOption);
    parser.addOption(secondsOption);
    parser.addOption(verifyUiOption);
    parser.addOption(languageOption);
    parser.process(app);

    std::optional<Language> cliLanguage;
    if (parser.isSet(languageOption)) {
        cliLanguage = parseToolCliLanguage(parser.value(languageOption));
        if (!cliLanguage) {
            std::cerr << "invalid --language; expected zh or en" << std::endl;
            return 64;
        }
    }
    // The existing --verify-ui command may run a deterministic interaction
    // regression using a throwaway INI settings file. This is never active
    // for normal GUI sessions and cannot modify a real user's preference.
    const QString testCase = parser.isSet(verifyUiOption)
        ? qEnvironmentVariable("HYREMOTE_TOOL_TEST_LANGUAGE_CASE") : QString();
    if (!testCase.isEmpty() && testCase != QStringLiteral("repeat")
                            && testCase != QStringLiteral("save-failure")) {
        std::cerr << "invalid HYREMOTE_TOOL_TEST_LANGUAGE_CASE" << std::endl;
        return 64;
    }
    std::unique_ptr<QTemporaryDir> testRoot;
    std::unique_ptr<QSettings> languageSettings;
    if (!testCase.isEmpty()) {
        testRoot = std::make_unique<QTemporaryDir>();
        if (!testRoot->isValid()) {
            std::cerr << "TOOL_LANGUAGE_UI_TEMP_FAIL" << std::endl;
            return 1;
        }
        QString settingsPath = testRoot->filePath(QStringLiteral("settings.ini"));
        if (testCase == QStringLiteral("save-failure")) {
            // A regular file as the parent of settings.ini is unwriteable
            // independently of user identity / chmod / runner privileges.
            QFile blockingParent(testRoot->filePath(QStringLiteral("blocked")));
            if (!blockingParent.open(QIODevice::WriteOnly)) {
                std::cerr << "TOOL_LANGUAGE_UI_TEMP_FAIL" << std::endl;
                return 1;
            }
            blockingParent.close();
            settingsPath = testRoot->filePath(QStringLiteral("blocked/settings.ini"));
        }
        languageSettings = std::make_unique<QSettings>(
            settingsPath, QSettings::IniFormat);
        if (testCase == QStringLiteral("repeat")
            && !saveToolLanguagePreference(*languageSettings,
                                           ToolLanguagePreference::Chinese)) {
            std::cerr << "TOOL_LANGUAGE_UI_TEMP_FAIL" << std::endl;
            return 1;
        }
        // Both regressions start with an English CLI override. The repeat
        // case must preserve the saved Chinese preference until the user
        // explicitly selects the already displayed English item.
        if (!cliLanguage || *cliLanguage != Language::English) {
            std::cerr << "TOOL_LANGUAGE_UI_REQUIRES_CLI_EN" << std::endl;
            return 64;
        }
    } else {
        languageSettings = std::make_unique<QSettings>();
    }
    // Offscreen profile GUI regression uses a fresh throwaway file, never
    // the user's actual bookmark store and never an implicit socket connect.
    const QString profileTestCase = parser.isSet(verifyUiOption)
        ? qEnvironmentVariable("HYREMOTE_TOOL_TEST_PROFILE_CASE") : QString();
    if (!profileTestCase.isEmpty()
        && profileTestCase != QStringLiteral("crud")) {
        std::cerr << "invalid HYREMOTE_TOOL_TEST_PROFILE_CASE" << std::endl;
        return 64;
    }
    const QString diagnosticTestCase = parser.isSet(verifyUiOption)
        ? qEnvironmentVariable("HYREMOTE_TOOL_TEST_DIAGNOSTICS_CASE") : QString();
    if (!diagnosticTestCase.isEmpty()
        && diagnosticTestCase != QStringLiteral("compact")
        && diagnosticTestCase != QStringLiteral("wide")) {
        std::cerr << "invalid HYREMOTE_TOOL_TEST_DIAGNOSTICS_CASE" << std::endl;
        return 64;
    }
    const QString nameTestCase = parser.isSet(verifyUiOption)
        ? qEnvironmentVariable("HYREMOTE_TOOL_TEST_SERVER_NAME_CASE") : QString();
    if (!nameTestCase.isEmpty() && nameTestCase != QStringLiteral("metadata")) {
        std::cerr << "invalid HYREMOTE_TOOL_TEST_SERVER_NAME_CASE" << std::endl;
        return 64;
    }
    if (int(!testCase.isEmpty()) + int(!profileTestCase.isEmpty())
        + int(!diagnosticTestCase.isEmpty()) + int(!nameTestCase.isEmpty()) > 1) {
        std::cerr << "only one Tool UI regression case may run" << std::endl;
        return 64;
    }
    std::unique_ptr<QTemporaryDir> profileTestRoot;
    QString profilePath;
    if (!profileTestCase.isEmpty() || !diagnosticTestCase.isEmpty()
        || !nameTestCase.isEmpty()) {
        profileTestRoot = std::make_unique<QTemporaryDir>();
        if (!profileTestRoot->isValid()) {
            std::cerr << "TOOL_PROFILE_UI_TEMP_FAIL" << std::endl;
            return 1;
        }
        profilePath = profileTestRoot->filePath(QStringLiteral("connections.json"));
    }
    const auto savedLanguage = loadToolLanguagePreference(*languageSettings);
    const auto startLanguage = resolveToolLanguage(savedLanguage, QLocale::system(),
                                                   cliLanguage);
    ClientWindow window(startLanguage, savedLanguage, cliLanguage.has_value(),
                        *languageSettings, profilePath);
    window.show();

    if (parser.isSet(verifyUiOption)) {
        QTimer::singleShot(0, &window, [&window, &app, testCase, profileTestCase,
                                         diagnosticTestCase, nameTestCase] {
            const bool ok = !nameTestCase.isEmpty()
                                ? window.verifyServerNameMetadata()
                                : !diagnosticTestCase.isEmpty()
                                      ? window.verifyDiagnosticsUi(diagnosticTestCase)
                                : !profileTestCase.isEmpty()
                                      ? window.verifyProfileUi()
                                      : testCase.isEmpty()
                                            ? window.verifyUi()
                                            : window.verifyLanguageSelection(
                                                  testCase == QStringLiteral("save-failure"));
            app.exit(ok ? 0 : 1);
        });
        return app.exec();
    }

    const QString connectTarget = parser.value(connectOption);
    if (!connectTarget.isEmpty()) {
        const int separator = connectTarget.lastIndexOf(QLatin1Char(':'));
        if (separator <= 0) {
            std::cerr << "invalid --connect; expected host:port" << std::endl;
            return 64;
        }
        bool ok = false;
        const int port = connectTarget.mid(separator + 1).toInt(&ok);
        if (!ok || port <= 0 || port > 65535) {
            std::cerr << "invalid --connect port" << std::endl;
            return 64;
        }
        const QString host = connectTarget.left(separator);
        QTimer::singleShot(0, &window, [&window, host, port] {
            window.connectTo(host, static_cast<quint16>(port));
        });
    }

    const int testSeconds = parser.value(secondsOption).toInt();
    if (testSeconds > 0)
        QTimer::singleShot(testSeconds * 1000, &app, &QCoreApplication::quit);

    return app.exec();
}
