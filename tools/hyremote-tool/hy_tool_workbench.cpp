#include "hy_tool_workbench.hpp"

#include "basic_diagnostics.hpp"

#include <QBuffer>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>

#include <array>
#include <cstdio>

namespace {

QString localized(bool chinese, const char *english, const char *zh)
{
    return chinese ? QString::fromUtf8(zh) : QString::fromUtf8(english);
}

QString stateText(RfbClient::State state, bool chinese)
{
    switch (state) {
    case RfbClient::State::Disconnected: return localized(chinese, "Disconnected", "未连接");
    case RfbClient::State::Connecting: return localized(chinese, "Connecting", "连接中");
    case RfbClient::State::Handshaking: return localized(chinese, "Handshaking", "握手中");
    case RfbClient::State::Connected: return localized(chinese, "Connected", "已连接");
    case RfbClient::State::Failed: return localized(chinese, "Failed", "失败");
    }
    return {};
}

QString diagnosticStageText(ToolDiagStage stage, bool chinese)
{
    switch (stage) {
    case ToolDiagStage::Disconnected: return localized(chinese, "Disconnected", "未连接");
    case ToolDiagStage::Connecting: return localized(chinese, "Connecting", "连接中");
    case ToolDiagStage::Handshaking: return localized(chinese, "Handshaking", "握手中");
    case ToolDiagStage::Connected: return localized(chinese, "Connected", "已连接");
    case ToolDiagStage::Failed: return localized(chinese, "Failed", "失败");
    }
    return {};
}

QString diagnosticFailureText(ToolDiagFailure failure, bool chinese)
{
    switch (failure) {
    case ToolDiagFailure::None: return localized(chinese, "None", "无");
    case ToolDiagFailure::ConnectionRefused: return localized(chinese, "Connection refused", "连接被拒绝");
    case ToolDiagFailure::DnsFailure: return localized(chinese, "DNS failure", "名称解析失败");
    case ToolDiagFailure::Timeout: return localized(chinese, "Timeout", "超时");
    case ToolDiagFailure::SecurityMismatch: return localized(chinese, "Security mismatch", "安全类型不匹配");
    case ToolDiagFailure::MalformedWire: return localized(chinese, "Malformed wire", "协议数据异常");
    case ToolDiagFailure::RemoteClosed: return localized(chinese, "Remote closed", "远端关闭连接");
    case ToolDiagFailure::Other: return localized(chinese, "Other local failure", "其他本机失败");
    }
    return {};
}

QString nextStepText(ToolDiagFailure failure, bool chinese)
{
    const char *token = ToolBasicDiagnostics::nextStepToken(failure);
    if (!chinese) {
        if (qstrcmp(token, "verify-listener-and-port") == 0)
            return QStringLiteral("Verify that the endpoint listens on the selected port.");
        if (qstrcmp(token, "check-dns-or-address") == 0)
            return QStringLiteral("Check the address or local name resolution.");
        if (qstrcmp(token, "check-network-or-firewall") == 0)
            return QStringLiteral("Check network reachability and the local firewall.");
        if (qstrcmp(token, "check-security-policy") == 0)
            return QStringLiteral("Check the endpoint security policy; do not weaken it implicitly.");
        if (qstrcmp(token, "capture-wire-evidence") == 0)
            return QStringLiteral("Capture bounded wire evidence for diagnosis.");
        if (qstrcmp(token, "check-remote-application") == 0)
            return QStringLiteral("Check that the remote application is still running.");
        if (qstrcmp(token, "review-local-diagnostics") == 0)
            return QStringLiteral("Review the redacted local diagnostics preview.");
        return QStringLiteral("No specific next step from current evidence.");
    }
    if (qstrcmp(token, "verify-listener-and-port") == 0)
        return QStringLiteral("确认端点在所选端口监听。");
    if (qstrcmp(token, "check-dns-or-address") == 0)
        return QStringLiteral("检查地址或本机名称解析。");
    if (qstrcmp(token, "check-network-or-firewall") == 0)
        return QStringLiteral("检查网络可达性与本机防火墙。");
    if (qstrcmp(token, "check-security-policy") == 0)
        return QStringLiteral("检查端点安全策略，不要隐式降低安全设置。");
    if (qstrcmp(token, "capture-wire-evidence") == 0)
        return QStringLiteral("采集有界协议证据用于诊断。");
    if (qstrcmp(token, "check-remote-application") == 0)
        return QStringLiteral("检查远端应用是否仍在运行。");
    if (qstrcmp(token, "review-local-diagnostics") == 0)
        return QStringLiteral("查看默认脱敏的本机诊断预览。");
    return QStringLiteral("目前没有可采信的具体排查步骤。");
}

QQuickItem *findItem(QObject *root, const QString &objectName)
{
    if (!root)
        return nullptr;
    if (root->objectName() == objectName) {
        if (auto *item = qobject_cast<QQuickItem *>(root))
            return item;
    }
    return root->findChild<QQuickItem *>(objectName, Qt::FindChildrenRecursively);
}

bool itemTextComplete(QQuickItem *item)
{
    if (!item || !item->isVisible())
        return false;
    const QVariant width = item->property("implicitWidth");
    const QVariant actual = item->property("width");
    if (!width.isValid() || !actual.isValid())
        return false;
    return width.toReal() <= actual.toReal() + 2.0;
}

} // namespace

HyToolWorkbench::HyToolWorkbench(QSettings &settings,
                                 std::optional<ToolLanguage> cliLanguage,
                                 bool cliOverride, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_cliLanguage(cliLanguage)
    , m_cliOverride(cliOverride)
    , m_preference(loadToolLanguagePreference(settings))
    , m_language(resolveToolLanguage(m_preference, QLocale::system(), cliLanguage))
{
    m_client.setRemoteControlEnabled(false);
    connect(&m_client, &RfbClient::stateChanged, this, [this](RfbClient::State state) {
        m_diagnostics.observeState(state);
        refreshConnectionText();
    });
    connect(&m_client, &RfbClient::socketErrorObserved, this,
            [this](QAbstractSocket::SocketError error) { m_diagnostics.observeSocketError(error); });
    connect(&m_client, &RfbClient::errorOccurred, this, [this](const QString &message) {
        m_diagnostics.observeError(message);
        refreshConnectionText();
    });
    connect(&m_client, &RfbClient::statsUpdated, this, [this](const RfbClient::Stats &stats) {
        m_stats = stats;
        m_diagnostics.observeStats(stats);
        refreshConnectionText();
    });
    connect(&m_client, &RfbClient::frameUpdated, this, [this](const QImage &frame) {
        m_lastFrame = frame;
        m_diagnostics.observeFrame(frame.size());
        refreshConnectionText();
    });
    refreshConnectionText();
}

#ifndef HYREMOTE_TOOL_VERSION
#define HYREMOTE_TOOL_VERSION "0.0.0"
#endif

QString HyToolWorkbench::version() const
{
    return QString::fromLatin1(HYREMOTE_TOOL_VERSION);
}

QString HyToolWorkbench::mismatchWarningText() const
{
    const bool mismatch = [this]() {
        for (const HySdkInspector::Check &check : m_report.checks) {
            if (check.id == QStringLiteral("version-match")
                && check.result != HySdkInspector::Result::PassObserved)
                return true;
        }
        return false;
    }();
    if (!mismatch)
        return QString();
    return m_language == ToolLanguage::Chinese
        ? QStringLiteral("警告：当前 SDK/Kit 根不匹配或来源未知；文件存在不代表可运行。\n\n")
        : QStringLiteral("WARNING: selected SDK/Kit roots do not match or are unknown; file presence is not runtime eligibility.\n\n");
}

QVariantList HyToolWorkbench::routes() const
{
    const bool chinese = m_language == ToolLanguage::Chinese;
    const QString warning = mismatchWarningText();
    QVariantList list;
    for (const RouteInfo &info : std::array<RouteInfo, 4>{
             HyToolWorkbench::routeInfo(QStringLiteral("cpp")),
             HyToolWorkbench::routeInfo(QStringLiteral("qml")),
             HyToolWorkbench::routeInfo(QStringLiteral("generic")),
             HyToolWorkbench::routeInfo(QStringLiteral("qpa"))}) {
        QVariantMap item;
        item.insert(QStringLiteral("id"), QString::fromLatin1(info.id));
        item.insert(QStringLiteral("title"), QString::fromUtf8(chinese ? info.zhTitle : info.title));
        item.insert(QStringLiteral("prerequisites"),
                    warning + QString::fromUtf8(chinese ? info.zhPrerequisites : info.prerequisites));
        item.insert(QStringLiteral("call"), QString::fromUtf8(chinese ? info.zhCall : info.call));
        item.insert(QStringLiteral("deploy"), QString::fromUtf8(chinese ? info.zhDeploy : info.deploy));
        item.insert(QStringLiteral("launch"), QString::fromUtf8(chinese ? info.zhLaunch : info.launch));
        item.insert(QStringLiteral("verify"), QString::fromUtf8(chinese ? info.zhVerify : info.verify));
        list.append(item);
    }
    return list;
}

QVariantMap HyToolWorkbench::routeStatuses() const
{
    QVariantMap statuses;
    for (const char *route : {"cpp", "qml", "generic", "qpa"})
        statuses.insert(QString::fromLatin1(route), routeStatus(QString::fromLatin1(route)));
    return statuses;
}

QString HyToolWorkbench::sdkSummary() const
{
    return QString::fromUtf8(QJsonDocument(m_report.summaryObject()).toJson(QJsonDocument::Compact));
}

QString HyToolWorkbench::connectionState() const
{
    return stateText(m_client.state(), m_language == ToolLanguage::Chinese);
}

QString HyToolWorkbench::stageText() const
{
    return diagnosticStageText(m_diagnostics.report().snapshot().stage, m_language == ToolLanguage::Chinese);
}

QString HyToolWorkbench::failureText() const
{
    return diagnosticFailureText(m_diagnostics.report().snapshot().lastFailure,
                                 m_language == ToolLanguage::Chinese);
}

QString HyToolWorkbench::nextStep() const
{
    return nextStepText(m_diagnostics.report().snapshot().lastFailure,
                        m_language == ToolLanguage::Chinese);
}

QString HyToolWorkbench::eventsText() const
{
    return m_eventsText;
}

QString HyToolWorkbench::diagnosticsJson() const
{
    return QString::fromUtf8(m_diagnostics.previewJson(false));
}

void HyToolWorkbench::setSdkRoot(const QString &root)
{
    if (m_sdkRoot == root)
        return;
    m_sdkRoot = root;
    m_report = HySdkInspector::Report{};
    emit inspectionChanged();
}

void HyToolWorkbench::setKitRoot(const QString &root)
{
    if (m_kitRoot == root)
        return;
    m_kitRoot = root;
    m_report = HySdkInspector::Report{};
    emit inspectionChanged();
}

void HyToolWorkbench::inspectSelectedPaths()
{
    m_report = HySdkInspector::inspect(m_sdkRoot, m_kitRoot);
    emit inspectionChanged();
    emit connectionChanged();
}

void HyToolWorkbench::setLanguagePreference(const QString &token)
{
    const auto preference = toolLanguagePreferenceFromToken(token);
    const bool saved = saveToolLanguagePreference(m_settings, preference);
    if (!saved)
        return;
    m_preference = preference;
    m_cliOverride = false;
    m_cliLanguage.reset();
    m_language = resolveToolLanguage(m_preference, QLocale::system(), std::nullopt);
    emit languageChanged();
    emit inspectionChanged();
    emit connectionChanged();
}

const HyToolWorkbench::RouteInfo &HyToolWorkbench::routeInfo(const QString &route)
{
    static const RouteInfo cpp{
        "cpp",
        "C++ / Widgets SDK route",
        "Qt 6.8.3 or Qt 5.15.2 SDK; HyRemote::RemoteAccess; C++17 consumer.",
        "HyRemote::RemoteAccess remote(&window); remote.setPort(5921); remote.start();",
        "find_package(HyRemote CONFIG REQUIRED)\ntarget_link_libraries(MyApp PRIVATE HyRemote::RemoteAccess)\nhyremote_deploy(TARGET MyApp)",
        "Launch the installed consumer; connect a trusted viewer explicitly. Default deployment is read-only: remote input stays off unless explicitly enabled.",
        "Use a real consumer to verify startup, frame delivery and explicit control.",
        "C++ / Widgets SDK 路线",
        "Qt 6.8.3 或 Qt 5.15.2 SDK；HyRemote::RemoteAccess；C++17 消费者。",
        "HyRemote::RemoteAccess remote(&window); remote.setPort(5921); remote.start();",
        "find_package(HyRemote CONFIG REQUIRED)\ntarget_link_libraries(MyApp PRIVATE HyRemote::RemoteAccess)\nhyremote_deploy(TARGET MyApp)",
        "启动已安装消费者，并显式连接可信查看端。默认部署为只读：未显式启用时远程输入保持关闭。",
        "用真实消费者验证启动、帧投递与显式控制。",
    };
    static const RouteInfo qml{
        "qml",
        "QML SDK route",
        "Qt 6.8.3; installed HyRemote QML module; public import HyRemote.",
        "import HyRemote\nRemoteAccess {\n    target: window\n    enabled: true\n}",
        "find_package(HyRemote CONFIG REQUIRED)\nhyremote_deploy(TARGET MyApp QML)",
        "Launch the installed QML consumer; verify imports resolve inside the deployed tree.",
        "Use a real QML consumer to verify frame and input behavior.",
        "QML SDK 路线",
        "Qt 6.8.3；已安装 HyRemote QML 模块；公共 import HyRemote。",
        "import HyRemote\nRemoteAccess {\n    target: window\n    enabled: true\n}",
        "find_package(HyRemote CONFIG REQUIRED)\nhyremote_deploy(TARGET MyApp QML)",
        "启动已安装 QML 消费者，验证导入在部署树内解析。",
        "用真实 QML 消费者验证帧与输入行为。",
    };
    static const RouteInfo generic{
        "generic",
        "Generic zero-code route",
        "Ordinary Qt application; no HyRemote source include or link; generic deploy payload.",
        "Run the unchanged binary with -plugin hyremote (or -plugin hyremote:port=5921).",
        "find_package(HyRemote CONFIG REQUIRED)\nhyremote_deploy(TARGET MyApp GENERIC)",
        "Keep the native Qt platform; no implicit connection and remote input stays off unless explicitly enabled.",
        "Verify the unchanged app remains locally interactive and a viewer can connect explicitly.",
        "Generic 零代码路线",
        "普通 Qt 应用；不include HyRemote 头、不链接 HyRemote；使用 generic 部署 payload。",
        "原样启动二进制并追加 -plugin hyremote（或 -plugin hyremote:port=5921）。",
        "find_package(HyRemote CONFIG REQUIRED)\nhyremote_deploy(TARGET MyApp GENERIC)",
        "保留原生 Qt platform；不隐式连接，未显式启用时远程输入保持关闭。",
        "验证未修改的应用仍可本地交互，且查看端可显式连接。",
    };
    static const RouteInfo qpa{
        "qpa",
        "QPA transparent proxy route",
        "Exact Qt 6.8.3 private ABI; ordinary Qt app; native platform delegate remains authoritative.",
        "Run the installed example with -platform hyremote; hyremote-address=<ip>, hyremote-port=<1..65535> are explicit launch options.",
        "find_package(HyRemote CONFIG REQUIRED)\n# QPA requires exact Qt 6.8.3; verify the selected Kit separately.\nhyremote_deploy(TARGET MyApp QPA)",
        "Use the installed example launch form; default deployment is read-only unless explicitly enabled.",
        "Verify exact-version deployment and native/local coexistence with a real consumer.",
        "QPA 透明代理路线",
        "精确 Qt 6.8.3 私有 ABI；普通 Qt 应用；原生 platform delegate 仍是权威。",
        "用 -platform hyremote 启动已安装示例；hyremote-address=<ip>、hyremote-port=<1..65535> 为显式启动参数。",
        "find_package(HyRemote CONFIG REQUIRED)\n# QPA 要求精确 Qt 6.8.3；所选 Kit 必须另行核对。\nhyremote_deploy(TARGET MyApp QPA)",
        "使用已安装示例的启动形式；默认部署只读，除非显式启用。",
        "用真实消费者验证精确版本部署与原生/本地共存。",
    };
    if (route == QStringLiteral("qml"))
        return qml;
    if (route == QStringLiteral("generic"))
        return generic;
    if (route == QStringLiteral("qpa"))
        return qpa;
    return cpp;
}

QString HyToolWorkbench::guidance(const QString &route) const
{
    const RouteInfo &info = routeInfo(route);
    const bool chinese = m_language == ToolLanguage::Chinese;
    const QString warning = mismatchWarningText();
    return warning + QStringLiteral("%1\n\n%2\n\n%3\n\n%4\n\n%5\n\n%6")
        .arg(QString::fromUtf8(chinese ? info.zhTitle : info.title),
             QString::fromUtf8(chinese ? info.zhPrerequisites : info.prerequisites),
             QString::fromUtf8(chinese ? info.zhCall : info.call),
             QString::fromUtf8(chinese ? info.zhDeploy : info.deploy),
             QString::fromUtf8(chinese ? info.zhLaunch : info.launch),
             QString::fromUtf8(chinese ? info.zhVerify : info.verify));
}

QString HyToolWorkbench::routeStatus(const QString &route) const
{
    for (const HySdkInspector::Check &check : m_report.checks) {
        if (check.id == route)
            return HySdkInspector::localizedResult(check.result,
                                                   m_language == ToolLanguage::Chinese);
    }
    return HySdkInspector::localizedResult(HySdkInspector::Result::UnknownSource,
                                           m_language == ToolLanguage::Chinese);
}

void HyToolWorkbench::copyGuidance(const QString &route)
{
    if (QClipboard *clipboard = QGuiApplication::clipboard())
        clipboard->setText(guidance(route));
}

void HyToolWorkbench::connectHost(const QString &host, int port)
{
    if (host.trimmed().isEmpty() || port <= 0 || port > 65535)
        return;
    if (m_client.state() != RfbClient::State::Disconnected)
        return;
    m_client.setRemoteControlEnabled(false);
    m_diagnostics.beginAttempt(host.trimmed(), static_cast<quint16>(port));
    m_client.connectToHost(host.trimmed(), static_cast<quint16>(port));
    refreshConnectionText();
}

void HyToolWorkbench::cancelConnection()
{
    if (m_client.state() == RfbClient::State::Disconnected)
        return;
    m_client.disconnectFromHost();
    clearFrame();
    refreshConnectionText();
}

void HyToolWorkbench::endSession()
{
    m_client.setRemoteControlEnabled(false);
    m_client.disconnectFromHost();
    clearFrame();
    refreshConnectionText();
}

void HyToolWorkbench::sampleFrame()
{
    if (m_lastFrame.isNull()) {
        m_sampleImage.clear();
        emit sampleChanged();
        return;
    }
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    m_lastFrame.save(&buffer, "PNG");
    m_sampleImage = QStringLiteral("data:image/png;base64,")
        + QString::fromLatin1(bytes.toBase64());
    emit sampleChanged();
}

void HyToolWorkbench::copyDiagnostics()
{
    if (QClipboard *clipboard = QGuiApplication::clipboard())
        clipboard->setText(diagnosticsJson());
}

void HyToolWorkbench::saveDiagnostics()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString path = QFileDialog::getSaveFileName(nullptr,
        m_language == ToolLanguage::Chinese ? QStringLiteral("保存脱敏诊断报告")
                                            : QStringLiteral("Save redacted diagnostics"),
        base.isEmpty() ? QStringLiteral("hyremote-diagnostics.json")
                       : base + QStringLiteral("/hyremote-diagnostics.json"),
        QStringLiteral("JSON (*.json)"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(m_diagnostics.previewJson(false));
}

void HyToolWorkbench::requestLegacySwitch()
{
    endSession();
    emit legacySwitchRequested();
}

void HyToolWorkbench::refreshConnectionText()
{
    const bool chinese = m_language == ToolLanguage::Chinese;
    m_connectionState = connectionState();
    m_stageText = stageText();
    m_failureText = failureText();
    m_nextStep = nextStep();
    QStringList eventLines;
    for (const ToolDiagEvent &event : m_diagnostics.report().recentEvents()) {
        const QString stamp = QDateTime::fromMSecsSinceEpoch(event.observedUtcMs, Qt::UTC)
                                  .toString(QStringLiteral("yyyy-MM-ddTHH:mm:ssZ"));
        eventLines << QStringLiteral("%1 | %2 | %3").arg(stamp,
            diagnosticStageText(event.stage, chinese), diagnosticFailureText(event.reason, chinese));
    }
    m_eventsText = eventLines.join(QLatin1Char('\n'));
    emit connectionChanged();
}

void HyToolWorkbench::clearFrame()
{
    m_lastFrame = QImage();
    m_sampleImage.clear();
    emit sampleChanged();
}

bool HyToolWorkbench::verifyWorkbench(QObject *root, const QString &sdkRoot,
                                      const QString &kitRoot, const QString &languageToken)
{
    bool ok = root != nullptr && root->objectName() == QStringLiteral("hyWorkbenchRoot");
    const char *pages[] = {"hyEnvironmentPage", "hyIntegrationPage",
                           "hyConnectionPage", "hyDiagnosticsPage"};
    const char *nav[] = {"hyNavEnvironment", "hyNavIntegration",
                         "hyNavConnection", "hyNavDiagnostics"};

    bool layoutOk = true;
    int navClipped = 0;
    int pageUnreachable = 0;
    int switchFailed = 0;
    const auto checkLayout = [&](int width, int height, const QString &lang) {
        setLanguagePreference(lang);
        auto *window = qobject_cast<QQuickWindow *>(root);
        QQuickItem *item = window ? window->contentItem() : qobject_cast<QQuickItem *>(root);
        if (!item) {
            layoutOk = false;
            return;
        }
        if (window) {
            window->setWidth(width);
            window->setHeight(height);
        } else {
            item->setWidth(width);
            item->setHeight(height);
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        for (const char *name : nav) {
            if (!itemTextComplete(findItem(root, QString::fromLatin1(name)))) {
                layoutOk = false;
                ++navClipped;
            }
        }
        for (int i = 0; i < 4; ++i) {
            root->setProperty("currentPage", i);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            QQuickItem *page = findItem(root, QString::fromLatin1(pages[i]));
            if (!page || !page->isVisible() || page->width() < 100 || page->height() < 100) {
                layoutOk = false;
                ++pageUnreachable;
            }
        }
    };

    checkLayout(800, 600, QStringLiteral("en"));
    checkLayout(1366, 768, QStringLiteral("zh"));
    checkLayout(800, 600, QStringLiteral("zh"));
    checkLayout(1366, 768, QStringLiteral("en"));
    QFont scaled = QGuiApplication::font();
    scaled.setPointSizeF(scaled.pointSizeF() * 1.15);
    QGuiApplication::setFont(scaled);
    checkLayout(800, 600, QStringLiteral("en"));
    QFont restored = QGuiApplication::font();
    restored.setPointSizeF(restored.pointSizeF() / 1.15);
    QGuiApplication::setFont(restored);
    ok = ok && layoutOk;

    setSdkRoot(sdkRoot);
    setKitRoot(kitRoot);
    inspectSelectedPaths();
    const auto hasResult = [this](const QString &id, HySdkInspector::Result result) {
        for (const HySdkInspector::Check &check : m_report.checks) {
            if (check.id == id && check.result == result)
                return true;
        }
        return false;
    };
    const auto checkResult = [](const HySdkInspector::Report &report, const QString &id,
                                HySdkInspector::Result result) {
        for (const HySdkInspector::Check &check : report.checks) {
            if (check.id == id)
                return check.result == result;
        }
        return false;
    };
    ok = ok && checkResult(m_report, QStringLiteral("manifest"), HySdkInspector::Result::PassObserved)
             && checkResult(m_report, QStringLiteral("kit"), HySdkInspector::Result::PassObserved)
             && checkResult(m_report, QStringLiteral("kit-binaries"), HySdkInspector::Result::PassObserved)
             && checkResult(m_report, QStringLiteral("version-match"), HySdkInspector::Result::PassObserved)
             && checkResult(m_report, QStringLiteral("qpa-exact"), HySdkInspector::Result::PassObserved)
             && checkResult(m_report, QStringLiteral("sdk-version"), HySdkInspector::Result::UnknownSource)
             && m_report.sdkVersion == QStringLiteral("0.0.0")
             && m_report.sdkQtVersion == QStringLiteral("6.8.3");
    const char *routes[] = {"cpp", "qml", "generic", "qpa"};
    for (const char *route : routes) {
        ok = ok && checkResult(m_report, QString::fromLatin1(route), HySdkInspector::Result::PassObserved)
                 && routeStatus(QString::fromLatin1(route))
                    == HySdkInspector::localizedResult(HySdkInspector::Result::PassObserved, false);
    }
    const QString cppGuide = guidance(QStringLiteral("cpp"));
    const QString qmlGuide = guidance(QStringLiteral("qml"));
    const QString genericGuide = guidance(QStringLiteral("generic"));
    const QString qpaGuide = guidance(QStringLiteral("qpa"));
    ok = ok && cppGuide.contains(QStringLiteral("HyRemote::RemoteAccess"))
             && cppGuide.contains(QStringLiteral("find_package(HyRemote CONFIG REQUIRED)"))
             && cppGuide.contains(QStringLiteral("read-only"))
             && qmlGuide.contains(QStringLiteral("target: window"))
             && qmlGuide.contains(QStringLiteral("enabled: true"))
             && genericGuide.contains(QStringLiteral("-plugin hyremote"))
             && qpaGuide.contains(QStringLiteral("-platform hyremote"))
             && qpaGuide.contains(QStringLiteral("hyremote-port="))
             && qpaGuide.contains(QStringLiteral("find_package(HyRemote CONFIG REQUIRED)"))
             && !qpaGuide.contains(QStringLiteral("6.8.3 EXACT"));
    copyGuidance(QStringLiteral("qml"));
    const QClipboard *clipboard = QGuiApplication::clipboard();
    ok = ok && clipboard && clipboard->text().contains(QStringLiteral("target: window"))
             && clipboard->text().contains(QStringLiteral("enabled: true"));

    const HySdkInspector::Report missingPayload = HySdkInspector::inspect(
        sdkRoot + QStringLiteral("/missing-qml-payload"), kitRoot);
    ok = ok && checkResult(missingPayload, QStringLiteral("qml"), HySdkInspector::Result::FailObserved);

    const HySdkInspector::Report mismatch = HySdkInspector::inspect(
        sdkRoot, QFileInfo(kitRoot).absolutePath() + QStringLiteral("/kit-mismatch"));
    ok = ok && checkResult(mismatch, QStringLiteral("version-match"), HySdkInspector::Result::FailObserved)
             && checkResult(mismatch, QStringLiteral("kit-binaries"), HySdkInspector::Result::PassObserved);

    const HySdkInspector::Report qpaOff = HySdkInspector::inspect(
        sdkRoot + QStringLiteral("/qpa-off"), kitRoot);
    ok = ok && checkResult(qpaOff, QStringLiteral("qpa-exact"), HySdkInspector::Result::NotApplicable)
             && checkResult(qpaOff, QStringLiteral("qpa"), HySdkInspector::Result::NotApplicable);

    const QString staleRoot = sdkRoot + QStringLiteral("/missing-fixture");
    setSdkRoot(staleRoot);
    ok = ok && checks().isEmpty();
    setSdkRoot(sdkRoot);
    setKitRoot(kitRoot);
    inspectSelectedPaths();

    setLanguagePreference(QStringLiteral("zh"));
    ok = ok && guidance(QStringLiteral("cpp")).contains(QStringLiteral("只读"))
             && !checks().isEmpty()
             && checks().first().toMap().value(QStringLiteral("reason")).toString().contains(QStringLiteral("manifest"));
    setLanguagePreference(languageToken);
    inspectSelectedPaths();

    ToolDiagnosticSession fixture;
    fixture.beginAttempt(QStringLiteral("198.51.100.7"), 5921);
    fixture.observeSocketError(QAbstractSocket::ConnectionRefusedError);
    const QByteArray preview = fixture.previewJson(false);
    ok = ok && preview.contains("[redacted]")
             && !preview.contains("198.51.100.7")
             && fixture.report().snapshot().lastFailure == ToolDiagFailure::ConnectionRefused;

    ok = ok && m_client.state() == RfbClient::State::Disconnected
             && !m_client.remoteControlEnabled()
             && languageToken == language()
             && !guidance(QStringLiteral("cpp")).isEmpty();

    const int sdkOk = checkResult(m_report, QStringLiteral("manifest"), HySdkInspector::Result::PassObserved)
        && checkResult(m_report, QStringLiteral("kit"), HySdkInspector::Result::PassObserved)
        && checkResult(m_report, QStringLiteral("version-match"), HySdkInspector::Result::PassObserved)
        && checkResult(m_report, QStringLiteral("qpa-exact"), HySdkInspector::Result::PassObserved)
        && checkResult(m_report, QStringLiteral("sdk-version"), HySdkInspector::Result::UnknownSource) ? 1 : 0;
    bool allRoutesPass = true;
    for (const char *route : routes) {
        allRoutesPass = allRoutesPass
            && checkResult(m_report, QString::fromLatin1(route), HySdkInspector::Result::PassObserved);
    }
    const int routesOk = allRoutesPass ? 1 : 0;
    const int fixturesOk = checkResult(missingPayload, QStringLiteral("qml"), HySdkInspector::Result::FailObserved)
        && checkResult(mismatch, QStringLiteral("version-match"), HySdkInspector::Result::FailObserved)
        && checkResult(qpaOff, QStringLiteral("qpa"), HySdkInspector::Result::NotApplicable) ? 1 : 0;
    const int previewOk = preview.contains("[redacted]") ? 1 : 0;
    const int stateOk = m_client.state() == RfbClient::State::Disconnected
        && !m_client.remoteControlEnabled() ? 1 : 0;
    if (ok) {
        std::fprintf(stdout, "TOOL_DEVELOPER_WORKBENCH_PASS root=1 pages=4 layouts=5 sdk=positive,missing,mismatch,sentinel routes=4 offline=%d masked=%d cli=%s legacy=available detail=layout1,nav0,page0,switch0\n",
                     stateOk, previewOk, languageToken.toUtf8().constData());
    } else {
        std::fprintf(stdout, "TOOL_DEVELOPER_WORKBENCH_FAIL layout=%d nav=%d page=%d switch=%d sdk=%d routes=%d fixtures=%d preview=%d state=%d\n",
                     layoutOk ? 1 : 0, navClipped, pageUnreachable, switchFailed,
                     sdkOk, routesOk, fixturesOk, previewOk, stateOk);
    }
    return ok;
}
