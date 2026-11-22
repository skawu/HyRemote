#include "framebuffer_view.h"
#include "rfb_client.h"
#include "language_preference.hpp"
#include "connection_profile_store.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFile>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFormLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QMetaObject>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QPixmap>
#include <QLocale>
#include <QSettings>
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

class ClientWindow final : public QWidget
{
public:
    explicit ClientWindow(Language language, ToolLanguagePreference preference,
                          bool cliOverride, QSettings &settings,
                          QString profilePath = {}, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_client(new RfbClient(this))
        , m_view(new FramebufferView(this))
        , m_language(language)
        , m_languageSettings(settings)
        , m_profileStore(profilePath.isEmpty()
            ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
                  .filePath(QStringLiteral("connections.json"))
            : std::move(profilePath))
    {
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(10, 10, 10, 10);
        root->setSpacing(8);

        auto *brandRow = new QHBoxLayout;
        m_brand = new QLabel(this);
        const QPixmap brandImage(QStringLiteral(":/hyremote/branding/huayan-logo-single.png"));
        if (brandImage.isNull())
            qFatal("BRANDING_ASSET_MISSING");
        m_brand->setPixmap(brandImage.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        brandRow->addWidget(m_brand);
        auto *brandName = new QLabel(QStringLiteral("HyRemoteTool"), this);
        brandRow->addWidget(brandName);
        brandRow->addStretch(1);
        root->addLayout(brandRow);

        auto *connectionRow = new QHBoxLayout;
        m_remoteLabel = new QLabel(this);
        m_host = new QLineEdit(this);
        m_host->setPlaceholderText(QStringLiteral("192.168.1.6"));
        m_host->setClearButtonEnabled(true);
        m_portLabel = new QLabel(this);
        m_port = new QSpinBox(this);
        m_port->setRange(1, 65535);
        m_port->setValue(5921);
        m_connect = new QPushButton(this);
        m_status = new QLabel(this);
        m_languageBox = new QComboBox(this);
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
        connectionRow->addWidget(m_remoteLabel);
        connectionRow->addWidget(m_host, 1);
        connectionRow->addWidget(m_portLabel);
        connectionRow->addWidget(m_port);
        connectionRow->addWidget(m_connect);
        connectionRow->addWidget(m_status);
        connectionRow->addWidget(m_languageBox);
        root->addLayout(connectionRow);

        // Local non-secret bookmarks. Selection only fills host/port; a
        // network attempt requires a separate explicit Connect action.
        auto *profilesRow = new QHBoxLayout;
        m_profilesLabel = new QLabel(this);
        m_profilesBox = new QComboBox(this);
        m_profilesBox->setObjectName(QStringLiteral("tool-saved-connections"));
        m_profilesBox->setMinimumContentsLength(18);
        m_profilesBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        m_profileSave = new QPushButton(this);
        m_profileEdit = new QPushButton(this);
        m_profileDelete = new QPushButton(this);
        m_profileConnect = new QPushButton(this);
        m_profileRecover = new QPushButton(this);
        m_profileRecover->hide();
        profilesRow->addWidget(m_profilesLabel);
        profilesRow->addWidget(m_profilesBox, 1);
        profilesRow->addWidget(m_profileSave);
        profilesRow->addWidget(m_profileEdit);
        profilesRow->addWidget(m_profileDelete);
        profilesRow->addWidget(m_profileConnect);
        profilesRow->addWidget(m_profileRecover);
        root->addLayout(profilesRow);

        m_profileWarning = new QLabel(this);
        m_profileWarning->setWordWrap(true);
        m_profileWarning->setStyleSheet(QStringLiteral("color: #b00020;"));
        m_profileWarning->hide();
        root->addWidget(m_profileWarning);

        // Persisted settings may fail (read-only profile, disk/permission
        // errors). Warn in the GUI instead of claiming the selection survived
        // a restart. The language still applies to this running process.
        m_languageWarning = new QLabel(this);
        m_languageWarning->setWordWrap(true);
        m_languageWarning->setStyleSheet(QStringLiteral("color: #b00020;"));
        m_languageWarning->hide();
        root->addWidget(m_languageWarning);

        root->addWidget(m_view, 1);

        auto *telemetryRow = new QHBoxLayout;
        m_control = new QCheckBox(this);
        m_fps = new QLabel(this);
        m_mbps = new QLabel(this);
        m_frames = new QLabel(this);
        m_bytes = new QLabel(this);
        m_resolution = new QLabel(this);
        telemetryRow->addWidget(m_control);
        telemetryRow->addSpacing(16);
        telemetryRow->addWidget(m_fps);
        telemetryRow->addWidget(m_mbps);
        telemetryRow->addWidget(m_frames);
        telemetryRow->addWidget(m_bytes);
        telemetryRow->addWidget(m_resolution);
        telemetryRow->addStretch(1);
        root->addLayout(telemetryRow);

        m_error = new QLabel(this);
        m_error->setWordWrap(true);
        m_error->setStyleSheet(QStringLiteral("color: #b00020;"));
        m_error->setVisible(false);
        root->addWidget(m_error);

        m_view->onPointer = [this](quint8 mask, quint16 x, quint16 y) {
            m_client->sendPointer(mask, x, y);
        };
        m_view->onKey = [this](quint32 keysym, bool pressed) {
            m_client->sendKey(keysym, pressed);
        };

        connect(m_connect, &QPushButton::clicked, this, [this] { toggleConnection(); });
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        connect(m_profilesBox, &QComboBox::activated, this, [this](int index) {
#else
        connect(m_profilesBox, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
#endif
            selectProfile(index);
        });
        connect(m_profileSave, &QPushButton::clicked, this, [this] { promptSaveProfile(); });
        connect(m_profileEdit, &QPushButton::clicked, this, [this] { promptEditProfile(); });
        connect(m_profileDelete, &QPushButton::clicked, this, [this] { promptDeleteProfile(); });
        connect(m_profileConnect, &QPushButton::clicked, this, [this] {
            connectSavedProfile();
        });
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
        connect(m_control, &QCheckBox::toggled, this, [this](bool enabled) {
            if (enabled) {
                m_client->setRemoteControlEnabled(true);
                m_view->setInputEnabled(true);
            } else {
                // Release held keys/buttons while the wire is still permitted;
                // disabling the client first would suppress the KeyUp/MouseUp.
                m_view->setInputEnabled(false);
                m_client->setRemoteControlEnabled(false);
            }
        });
        connect(m_client, &RfbClient::stateChanged, this, [this](RfbClient::State state) {
            applyState(state);
        });
        connect(m_client, &RfbClient::frameUpdated, this, [this](const QImage &frame) {
            m_view->setFrame(frame);
            m_frameSize = frame.size();
            retranslateUi();
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
        connect(m_client, &RfbClient::serverNameChanged, this, [this](const QString &name) {
            std::cout << "TOOL_CLIENT_SERVER name=" << name.toStdString() << std::endl;
            m_serverInputAvailable = !name.contains(QStringLiteral("view-only"), Qt::CaseInsensitive);
            if (!m_serverInputAvailable) {
                m_control->setChecked(false);
                m_view->setInputEnabled(false);
                m_client->setRemoteControlEnabled(false);
            }
            retranslateUi();
        });
        connect(m_client, &RfbClient::statsUpdated, this, [this](const RfbClient::Stats &stats) {
            m_lastStats = stats;
            retranslateUi();
            std::cout << "TOOL_CLIENT_STATS fps=" << stats.fps
                      << " mbit=" << stats.mbitPerSecond
                      << " frames=" << stats.frames
                      << " bytes=" << stats.bytes << std::endl;
        });
        connect(m_client, &RfbClient::errorOccurred, this, [this](const QString &message) {
            m_lastErrorRaw = message;
            retranslateUi();
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
        resize(1000, 700);
    }

    void connectTo(const QString &host, quint16 port)
    {
        m_host->setText(host);
        m_port->setValue(port);
        m_pendingProfileId.clear();
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

    bool verifyUi() const
    {
        const bool ok = m_brand && !QGuiApplication::windowIcon().isNull()
                        && m_host && m_port && m_connect && m_view && m_status
                        && m_control && m_fps && m_mbps && m_frames && m_bytes
                        && m_resolution && m_error && m_languageBox && m_languageWarning
                        && m_profilesLabel && m_profilesBox && m_profileSave
                        && m_profileEdit && m_profileDelete && m_profileConnect
                        && m_profileRecover && m_profileWarning;
        std::cout << (ok ? "TOOL_CLIENT_UI_PASS" : "TOOL_CLIENT_UI_FAIL")
                  << " host=" << (m_host != nullptr)
                  << " port=" << (m_port != nullptr)
                  << " view=" << (m_view != nullptr)
                  << " telemetry=" << (m_fps != nullptr && m_mbps != nullptr)
                  << " language=" << (m_languageBox != nullptr)
                  << " lang=" << (isChinese(m_language) ? "zh" : "en")
                  << " title=" << windowTitle().toStdString()
                  << std::endl;
        return ok;
    }

protected:
    void closeEvent(QCloseEvent *event) override
    {
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
    bool connectSavedProfile();
    void promptSaveProfile();
    void promptEditProfile();
    void promptDeleteProfile();
    void promptRecoverProfiles();

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
        m_remoteLabel->setText(text("远程", "Remote"));
        m_portLabel->setText(text("端口", "Port"));
        m_connect->setText((m_state == RfbClient::State::Connected
                            || m_state == RfbClient::State::Connecting
                            || m_state == RfbClient::State::Handshaking)
                               ? text("断开", "Disconnect")
                               : text("连接", "Connect"));
        QString status = stateText(m_state, m_language);
        if (m_state == RfbClient::State::Connected && !m_serverInputAvailable)
            status += text(" · 只读", " · view-only");
        m_status->setText(status);
        m_control->setText(text("允许远程控制", "Remote control"));
        m_control->setToolTip(m_serverInputAvailable
                                  ? text("未勾选时会话为只读，不发送鼠标或键盘事件。",
                                         "When unchecked the session is view-only and no pointer or keyboard events are sent.")
                                  : text("远程应用未开启远程输入。",
                                         "The remote application has remote input disabled."));
        m_control->setEnabled(m_state == RfbClient::State::Connected && m_serverInputAvailable);
        m_languageBox->setItemText(0, text("自动（本机系统）", "Auto (local system)"));
        m_profilesLabel->setText(text("已保存", "Saved"));
        m_profileSave->setText(text("保存", "Save"));
        m_profileEdit->setText(text("编辑", "Edit"));
        m_profileDelete->setText(text("删除", "Delete"));
        m_profileConnect->setText(text("连接档案", "Connect saved"));
        m_profileRecover->setText(text("恢复", "Recover"));
        m_profilesBox->setItemText(0, text("选择已保存的连接", "Select saved connection"));
        updateProfileButtons();
        m_languageBox->setToolTip(text("界面语言；选择时尝试保存到本机。命令行仅覆盖本次启动。",
                                       "UI language; selections are saved locally when writable. CLI applies to this launch only."));
        m_languageWarning->setText(
            text("界面语言保存失败；本次选择只对当前运行有效。",
                 "Could not save the UI language; this choice applies only to this session."));
        m_languageWarning->setVisible(m_languageSaveFailed);
        m_fps->setText(QStringLiteral("FPS %1").arg(m_lastStats.fps, 0, 'f', 1));
        m_mbps->setText(QStringLiteral("%1 Mbit/s").arg(m_lastStats.mbitPerSecond, 0, 'f', 2));
        m_frames->setText(text("帧数 %1", "Frames %1").arg(m_lastStats.frames));
        m_bytes->setText(text("字节 %1", "Bytes %1").arg(humanBytes(m_lastStats.bytes)));
        m_resolution->setText(m_frameSize.isEmpty()
                                  ? text("无画面", "No frame")
                                  : QStringLiteral("%1 × %2").arg(m_frameSize.width()).arg(m_frameSize.height()));
        m_error->setText(localizedError(m_lastErrorRaw));
        m_error->setVisible(!m_lastErrorRaw.isEmpty());
        if (m_view && m_view->hasFrame() == false)
            m_view->setPlaceholder(placeholderText());
    }

    void toggleConnection(bool fromProfile = false)
    {
        if (!fromProfile)
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
            retranslateUi();
            return;
        }
        m_lastErrorRaw.clear();
        retranslateUi();
        m_client->connectToHost(host, static_cast<quint16>(m_port->value()));
    }

    void applyState(RfbClient::State state)
    {
        m_state = state;
        std::cout << "TOOL_CLIENT_STATE " << stateText(state, Language::English).toStdString() << std::endl;

        const bool busy = state == RfbClient::State::Connecting
                          || state == RfbClient::State::Handshaking;
        const bool connected = state == RfbClient::State::Connected;
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
        if (!connected) {
            m_control->setChecked(false);
            m_view->setInputEnabled(false);
            m_client->setRemoteControlEnabled(false);
        }
        if (state == RfbClient::State::Disconnected || state == RfbClient::State::Failed) {
            m_frameSize = QSize();
            m_view->clearFrame(placeholderText());
            if (state == RfbClient::State::Disconnected)
                m_lastErrorRaw.clear();
        }
        retranslateUi();
    }

    RfbClient *m_client = nullptr;
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
    bool m_serverInputAvailable = true;

    QLabel *m_brand = nullptr;
    QLabel *m_remoteLabel = nullptr;
    QLineEdit *m_host = nullptr;
    QLabel *m_portLabel = nullptr;
    QSpinBox *m_port = nullptr;
    QPushButton *m_connect = nullptr;
    QLabel *m_status = nullptr;
    QComboBox *m_languageBox = nullptr;
    QLabel *m_languageWarning = nullptr;
    QLabel *m_profilesLabel = nullptr;
    QComboBox *m_profilesBox = nullptr;
    QPushButton *m_profileSave = nullptr;
    QPushButton *m_profileEdit = nullptr;
    QPushButton *m_profileDelete = nullptr;
    QPushButton *m_profileConnect = nullptr;
    QPushButton *m_profileRecover = nullptr;
    QLabel *m_profileWarning = nullptr;
    QLabel *m_error = nullptr;
    QLabel *m_fps = nullptr;
    QLabel *m_mbps = nullptr;
    QLabel *m_frames = nullptr;
    QLabel *m_bytes = nullptr;
    QLabel *m_resolution = nullptr;
    QCheckBox *m_control = nullptr;
};


QString ClientWindow::selectedProfileId() const
{
    return m_profilesBox ? m_profilesBox->currentData().toString() : QString();
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
    if (!m_profilesBox)
        return;
    const bool busy = m_state == RfbClient::State::Connecting
                      || m_state == RfbClient::State::Handshaking
                      || m_state == RfbClient::State::Connected;
    const bool hasProfile = selectedProfile() != nullptr;
    m_profilesBox->setEnabled(m_profilesLoaded && !busy && m_profilesBox->count() > 1);
    m_profileSave->setEnabled(m_profilesLoaded && !busy);
    m_profileEdit->setEnabled(m_profilesLoaded && !busy && hasProfile);
    m_profileDelete->setEnabled(m_profilesLoaded && !busy && hasProfile);
    m_profileConnect->setEnabled(m_profilesLoaded && !busy && hasProfile);
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
        QSignalBlocker blocker(m_profilesBox);
        m_profilesBox->clear();
        m_profilesBox->addItem(text("选择已保存的连接", "Select saved connection"), QString());
        for (const auto &p : m_profileStore.profiles()) {
            const QString label = QStringLiteral("%1 · %2:%3")
                                      .arg(p.name, p.host, QString::number(p.port));
            m_profilesBox->addItem(label, p.id);
            m_profilesBox->setItemData(m_profilesBox->count() - 1, label, Qt::ToolTipRole);
        }
        const int index = m_profilesBox->findData(keepId);
        m_profilesBox->setCurrentIndex(index > 0 ? index : 0);
    }
    updateProfileButtons();
}

void ClientWindow::selectProfile(int index)
{
    if (index < 0 || index >= m_profilesBox->count())
        return;
    const auto id = m_profilesBox->itemData(index).toString();
    const bool busy = m_state == RfbClient::State::Connecting
                      || m_state == RfbClient::State::Handshaking
                      || m_state == RfbClient::State::Connected;
    if (!busy && !id.isEmpty()) {
        for (const auto &p : m_profileStore.profiles()) {
            if (p.id == id) {
                m_host->setText(p.host);
                m_port->setValue(p.port);
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

bool ClientWindow::connectSavedProfile()
{
    if (!m_profilesLoaded || !m_profileConnect->isEnabled())
        return false;
    const auto *profile = selectedProfile();
    if (!profile)
        return false;
    const QString id = profile->id;
    m_host->setText(profile->host);
    m_port->setValue(profile->port);
    // A saved bookmark is never a ControlGrant; each new RFB session
    // explicitly begins view-only, regardless of any previous connection.
    m_control->setChecked(false);
    m_view->setInputEnabled(false);
    m_client->setRemoteControlEnabled(false);
    m_pendingProfileId = id;
    m_startingProfileConnection = true;
    toggleConnection(true);
    m_startingProfileConnection = false;
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
    bool accepted = false;
    const QString name = QInputDialog::getText(
        this, text("保存连接", "Save connection"),
        text("本机显示名称", "Local display name"),
        QLineEdit::Normal, host, &accepted);
    if (!accepted)
        return;
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
    const bool empty = m_profilesLoaded && !m_profilesCorrupt && m_profilesBox
                       && m_profilesBox->count() == 1
                       && selectedProfileId().isEmpty()
                       && m_client->state() == RfbClient::State::Disconnected
                       && !m_view->inputEnabled();
    QString id;
    const bool saved = saveProfileRecord(QStringLiteral("Factory A"),
                                         QStringLiteral("192.168.1.6"), 5921, &id);
    const bool listed = m_profilesBox->count() == 2 && selectedProfileId() == id;
    m_host->setText(QStringLiteral("unsaved.example"));
    m_port->setValue(6000);
    const int index = m_profilesBox->findData(id);
    const bool activated = index > 0 && QMetaObject::invokeMethod(
        m_profilesBox, "activated", Qt::DirectConnection, Q_ARG(int, index));
    const bool selected = m_host->text() == QStringLiteral("192.168.1.6")
                          && m_port->value() == 5921
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
                      && m_profilesBox->count() == 4;
    const int longIndex = m_profilesBox->findData(secondId);
    if (longIndex > 0)
        m_profilesBox->setCurrentIndex(longIndex);
    const bool longActivated = longIndex > 0 && QMetaObject::invokeMethod(
        m_profilesBox, "activated", Qt::DirectConnection, Q_ARG(int, longIndex));
    const bool longSelected = longActivated
                              && m_host->text() == longHost
                              && m_port->value() == 5930
                              && m_client->state() == RfbClient::State::Disconnected;
    // A literal percent-digit in a user-supplied display name/IPv6 zone
    // must never be re-expanded by a second QString::arg call.
    QString percentId;
    const bool percentSaved = saveProfileRecord(
        QStringLiteral("Lab %1"), QStringLiteral("fe80::1%2"), 5921, &percentId);
    const int percentIndex = m_profilesBox->findData(percentId);
    const bool percentLabel = percentSaved && percentIndex > 0
                              && m_profilesBox->itemText(percentIndex)
                                 == QStringLiteral("Lab %1 · fe80::1%2:5921")
                              && m_profilesBox->itemData(percentIndex, Qt::ToolTipRole).toString()
                                 == QStringLiteral("Lab %1 · fe80::1%2:5921");
    const bool deleted = removeProfileRecord(percentId)
                         && removeProfileRecord(secondId)
                         && removeProfileRecord(thirdId)
                         && removeProfileRecord(id)
                         && m_profilesBox->count() == 1
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
    if (!profileTestCase.isEmpty() && !testCase.isEmpty()) {
        std::cerr << "only one Tool UI regression case may run" << std::endl;
        return 64;
    }
    std::unique_ptr<QTemporaryDir> profileTestRoot;
    QString profilePath;
    if (!profileTestCase.isEmpty()) {
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
        QTimer::singleShot(0, &window, [&window, &app, testCase, profileTestCase] {
            const bool ok = !profileTestCase.isEmpty()
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
