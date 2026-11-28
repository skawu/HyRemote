import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts

ApplicationWindow {
    id: root
    objectName: "hyWorkbenchRoot"
    width: 1366
    height: 768
    minimumWidth: 800
    minimumHeight: 600
    visible: true
    title: root.tr("windowTitle")

    property int currentPage: 0
    property int currentRoute: 0
    property var hyWorkbench: null
    readonly property int titlePx: Math.max(18, Qt.application.font.pixelSize + 4)
    readonly property int bodyPx: Math.max(14, Qt.application.font.pixelSize + 2)
    readonly property int codePx: Math.max(13, Qt.application.font.pixelSize + 1)
    readonly property int auxPx: Math.max(12, Qt.application.font.pixelSize)
    function setPage(index) { currentPage = Math.max(0, Math.min(3, index)) }
    function shortRoute(id) {
        if (id === "qml") return "QML"
        if (id === "generic") return "Generic"
        if (id === "qpa") return "QPA"
        return "C++"
    }

    function tr(key) {
        const zh = hyWorkbench.language === "zh"
        const map = {
            windowTitle: zh ? "HyRemoteTool · 研发工作台" : "HyRemoteTool · Developer Workbench",
            product: zh ? "华艳远程开发工具" : "HyRemoteTool",
            version: zh
                ? (hyWorkbench && hyWorkbench.version === "0.0.0"
                   ? "V0.0.0 · 开发哨兵 · 研发工作台"
                   : "V" + (hyWorkbench ? hyWorkbench.version : "0.0.0") + " · 研发工作台")
                : (hyWorkbench && hyWorkbench.version === "0.0.0"
                   ? "V0.0.0 · DEVELOP SENTINEL · Developer Workbench"
                   : "V" + (hyWorkbench ? hyWorkbench.version : "0.0.0") + " · Developer Workbench"),
            environment: zh ? "开发环境" : "Environment",
            integration: zh ? "集成指导" : "Integration",
            connection: zh ? "连接验证" : "Connection",
            diagnostics: zh ? "诊断报告" : "Diagnostics",
            help: zh ? "帮助" : "Help",
            language: zh ? "语言" : "Language",
            autoLanguage: zh ? "自动跟随系统" : "Auto (system)",
            languageAuto: zh ? "自动跟随系统" : "Follows OS locale",
            languageExplicit: zh ? "已显式保存" : "Explicit preference",
            languageCli: zh ? "CLI 覆盖 · 仅本次启动" : "CLI override · this launch only",
            languageSaveFailed: zh ? "语言偏好保存失败；已恢复当前生效语言。" : "Could not save language preference; restored the active language.",
            legacy: zh ? "兼容联调入口" : "Legacy access",
            sdkRoot: zh ? "SDK 安装根（显式选择，不扫描）" : "SDK install root (explicit, no scan)",
            kitRoot: zh ? "Qt Kit 根（显式选择，不扫描）" : "Qt Kit root (explicit, no scan)",
            inspect: zh ? "静态核对" : "Inspect",
            checks: zh ? "静态核对结果" : "Static checks",
            source: zh ? "来源" : "Source",
            reason: zh ? "原因" : "Reason",
            next: zh ? "下一步" : "Next step",
            routes: zh ? "接入路线" : "Integration routes",
            prerequisites: zh ? "1 前置条件" : "1 Prerequisites",
            minimalCall: zh ? "2 最小接入" : "2 Minimal integration",
            cmakeDeploy: zh ? "3 构建与部署" : "3 Build and deploy",
            launchVerify: zh ? "4 启动与验证" : "4 Launch and verify",
            copyCode: zh ? "复制代码" : "Copy code",
            copy: zh ? "复制完整指导" : "Copy full guidance",
            host: zh ? "远程主机" : "Remote host",
            port: zh ? "端口" : "Port",
            connect: zh ? "连接（只读）" : "Connect (read-only)",
            cancel: zh ? "取消" : "Cancel",
            end: zh ? "结束" : "End",
            sample: zh ? "取样当前帧" : "Sample current frame",
            stage: zh ? "阶段" : "Stage",
            failure: zh ? "失败分类" : "Failure",
            nextStep: zh ? "下一步建议" : "Next step",
            stats: zh ? "本机观察：帧 / 字节 / FPS / 吞吐" : "Local observations: frames / bytes / FPS / throughput",
            events: zh ? "有界事件" : "Bounded events",
            preview: zh ? "单帧预览（显式取样）" : "Frame preview (explicit sample)",
            diagnosticsPreview: zh ? "连接诊断预览（默认遮蔽端点）" : "Connection diagnostics preview (endpoint redacted)",
            sdkSummary: zh ? "SDK 核对摘要（白名单字段）" : "SDK inspection summary (allow-listed fields)",
            save: zh ? "保存报告" : "Save report",
            masked: zh ? "端点默认遮蔽；报告只含本机客户端观察。" : "Endpoints are redacted by default; reports contain local client observations only.",
            legacyAsk: zh ? "切换到兼容联调入口将结束当前只读验证，并打开既有 Widgets 界面。继续？" : "Switching to legacy access ends the current read-only verification and opens the existing Widgets window. Continue?",
            confirm: zh ? "确认切换" : "Confirm switch",
            helpText: zh ? "此工作台仅做 SDK/Qt Kit 静态核对、四路线指导、只读连接验证与脱敏诊断。不执行命令、不扫描磁盘、不实施远程控制、不宣称 ABI 或运行资格。" : "This workbench only performs static SDK/Kit inspection, four-route guidance, read-only connection verification and redacted diagnostics. It executes no commands, scans no disks, implements no remote control, and claims no ABI or runtime qualification."
        }
        return map[key] || key
    }

    component ActionButton: Basic.Button {
        id: button
        property bool primary: false
        implicitHeight: 36
        padding: 12
        font.pixelSize: root.bodyPx
        background: Rectangle {
            radius: 4
            color: button.primary ? (button.down ? "#0e6276" : "#16788f")
                                  : (button.down ? "#d5e6ef" : "#e8f1f7")
            border.color: button.activeFocus ? "#0b4a5c" : "#b8cfdc"
            border.width: button.activeFocus ? 2 : 1
        }
        contentItem: Text {
            text: button.text
            color: button.primary ? "white" : "#16324a"
            font.pixelSize: root.bodyPx
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    component NavButton: Basic.Button {
        id: nav
        Layout.fillWidth: true
        Layout.preferredHeight: 40
        checkable: true
        padding: 10
        font.pixelSize: root.bodyPx
        palette.button: nav.checked ? "#16788f" : "#163e55"
        palette.buttonText: "white"
        background: Rectangle {
            radius: 4
            color: nav.palette.button
            border.color: nav.activeFocus ? "#8ed7e8" : "transparent"
            border.width: nav.activeFocus ? 2 : 0
        }
    }

    component SectionTitle: Text {
        font.pixelSize: root.bodyPx + 1
        font.bold: true
        color: "#16324a"
        wrapMode: Text.WordWrap
    }

    component BodyText: Text {
        font.pixelSize: root.bodyPx
        color: "#40586b"
        wrapMode: Text.WordWrap
    }

    component ScrollPage: Flickable {
        id: flick
        property int pageMargin: 16
        contentWidth: width
        contentHeight: inner.implicitHeight + 2 * pageMargin
        clip: true
        default property alias pageContent: inner.data
        ColumnLayout {
            id: inner
            x: flick.pageMargin
            y: flick.pageMargin
            width: Math.max(0, flick.width - 2 * flick.pageMargin)
            spacing: 12
        }
    }

    component RoutePanel: ScrollPage {
        property int routeIndex: 0
        property var route: hyWorkbench.routes[routeIndex]
        SectionTitle { Layout.fillWidth: true; text: route.title }
        SectionTitle { Layout.fillWidth: true; text: root.tr("prerequisites") }
        BodyText { Layout.fillWidth: true; text: route.prerequisites }
        SectionTitle { Layout.fillWidth: true; text: root.tr("minimalCall") }
        CodeBlock { blockName: "hyRouteCode_" + route.id; codeText: route.call }
        ActionButton { objectName: "hyCopyCode_" + route.id; text: root.tr("copyCode"); onClicked: hyWorkbench.copyRouteCode(route.id) }
        SectionTitle { Layout.fillWidth: true; text: root.tr("cmakeDeploy") }
        CodeBlock { blockName: "hyRouteDeploy_" + route.id; codeText: route.deploy }
        SectionTitle { Layout.fillWidth: true; text: root.tr("launchVerify") }
        BodyText { Layout.fillWidth: true; text: route.launch + "\n" + route.verify }
        ActionButton { objectName: "hyCopyGuide_" + route.id; text: root.tr("copy"); onClicked: hyWorkbench.copyGuidance(route.id) }
    }

    component CodeBlock: Rectangle {
        id: block
        property string codeText: ""
        property string blockName: ""
        radius: 4
        color: "#f2f6f9"
        border.color: "#d7e3ea"
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(150, Math.max(72, codeText.split("\n").length * (root.codePx + 4) + 16))
        Basic.TextArea {
            objectName: block.blockName
            anchors.fill: parent
            anchors.margins: 8
            readOnly: true
            selectByMouse: true
            text: block.codeText
            font.family: "Consolas"
            font.pixelSize: root.codePx
            color: "#1d3342"
            wrapMode: TextEdit.Wrap
            background: Rectangle { color: "transparent" }
            padding: 0
        }
    }

    background: Rectangle { color: "#f4f7fa" }

    header: Rectangle {
        height: 88
        color: "#ffffff"
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 2
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Image {
                    source: "qrc:/hyremote/branding/huayan-logo-single.png"
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    fillMode: Image.PreserveAspectFit
                }
                ColumnLayout {
                    spacing: 0
                    Text { text: root.tr("product"); font.pixelSize: root.bodyPx + 2; font.bold: true; color: "#16324a" }
                    Text { text: root.tr("version"); font.pixelSize: root.auxPx; color: "#5b7184" }
                }
                Item { Layout.fillWidth: true }
                Text { text: root.tr("language"); font.pixelSize: root.auxPx; color: "#5b7184" }
                Basic.ComboBox {
                    id: languageCombo
                    objectName: "hyLanguageCombo"
                    Layout.preferredWidth: 126
                    model: [root.tr("autoLanguage"), "简体中文", "English"]
                    currentIndex: hyWorkbench.languageSource === "cli"
                        ? (hyWorkbench.language === "zh" ? 1 : 2)
                        : (hyWorkbench.languagePreference === "zh" ? 1 : (hyWorkbench.languagePreference === "en" ? 2 : 0))
                    font.pixelSize: root.bodyPx
                    onActivated: {
                        const before = currentIndex
                        hyWorkbench.setLanguagePreference(index === 1 ? "zh" : index === 2 ? "en" : "auto")
                        if (hyWorkbench.languageNotice.length > 0)
                            currentIndex = before
                    }
                }
                ActionButton { text: root.tr("help"); onClicked: helpDialog.open() }
                ActionButton { objectName: "hyLegacyButton"; text: root.tr("legacy"); onClicked: legacyDialog.open() }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Item { Layout.fillWidth: true }
                Text {
                    objectName: "hyLanguageSource"
                    text: hyWorkbench.languageSource === "cli" ? root.tr("languageCli")
                        : (hyWorkbench.languageSource === "explicit" ? root.tr("languageExplicit") : root.tr("languageAuto"))
                    font.pixelSize: root.auxPx
                    color: "#5b7184"
                }
                Text {
                    objectName: "hyLanguageNotice"
                    visible: hyWorkbench.languageNotice.length > 0
                    Layout.maximumWidth: root.width - 180
                    text: hyWorkbench.languageNotice
                    font.pixelSize: root.auxPx
                    color: "#9b4a25"
                    elide: Text.ElideRight
                }
            }
        }
    }

    Dialog {
        id: helpDialog
        title: root.tr("help")
        anchors.centerIn: parent
        width: Math.min(560, root.width - 40)
        height: 220
        standardButtons: Dialog.Close
        Label { anchors.fill: parent; anchors.margins: 12; text: root.tr("helpText"); wrapMode: Text.WordWrap }
    }

    Dialog {
        id: legacyDialog
        title: root.tr("legacy")
        anchors.centerIn: parent
        width: Math.min(520, root.width - 40)
        height: 180
        standardButtons: Dialog.Yes | Dialog.No
        Label { anchors.fill: parent; anchors.margins: 12; text: root.tr("legacyAsk"); wrapMode: Text.WordWrap }
        onAccepted: hyWorkbench.requestLegacySwitch()
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 220
            Layout.fillHeight: true
            color: "#0f2c3f"
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 8
                NavButton { id: navEnvironment; objectName: "hyNavEnvironment"; text: root.tr("environment"); checked: root.currentPage === 0; onClicked: root.setPage(0) }
                NavButton { id: navIntegration; objectName: "hyNavIntegration"; text: root.tr("integration"); checked: root.currentPage === 1; onClicked: root.setPage(1) }
                NavButton { id: navConnection; objectName: "hyNavConnection"; text: root.tr("connection"); checked: root.currentPage === 2; onClicked: root.setPage(2) }
                NavButton { id: navDiagnostics; objectName: "hyNavDiagnostics"; text: root.tr("diagnostics"); checked: root.currentPage === 3; onClicked: root.setPage(3) }
                Item { Layout.fillHeight: true }
                Text {
                    text: "© Huayan"
                    color: "#7fa3b9"
                    font.pixelSize: 11
                }
            }
        }

        StackLayout {
            id: pages
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.currentPage

            ScrollPage {
                id: environmentPage
                objectName: "hyEnvironmentPage"
                SectionTitle { Layout.fillWidth: true; text: root.tr("environment"); font.pixelSize: root.titlePx }
                RowLayout { spacing: 8
                    Basic.TextField { id: sdkField; objectName: "hySdkRootField"; Layout.fillWidth: true; implicitHeight: 36; font.pixelSize: root.bodyPx; placeholderText: root.tr("sdkRoot"); text: hyWorkbench.sdkRoot; onEditingFinished: hyWorkbench.setSdkRoot(text) }
                    Basic.TextField { id: kitField; objectName: "hyKitRootField"; Layout.fillWidth: true; implicitHeight: 36; font.pixelSize: root.bodyPx; placeholderText: root.tr("kitRoot"); text: hyWorkbench.kitRoot; onEditingFinished: hyWorkbench.setKitRoot(text) }
                    ActionButton { objectName: "hyInspectButton"; primary: true; text: root.tr("inspect"); onClicked: { hyWorkbench.setSdkRoot(sdkField.text); hyWorkbench.setKitRoot(kitField.text); hyWorkbench.inspectSelectedPaths() } }
                }
                SectionTitle { Layout.fillWidth: true; text: root.tr("checks") }
                Repeater {
                    model: hyWorkbench.checks
                    Rectangle {
                        Layout.fillWidth: true
                        radius: 4
                        color: "#ffffff"
                        border.color: "#d7e3ea"
                        implicitHeight: checkColumn.implicitHeight + 16
                        ColumnLayout {
                            id: checkColumn
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 8
                            spacing: 4
                            RowLayout { spacing: 8
                                Text { text: modelData.id; font.bold: true; font.pixelSize: root.bodyPx; color: "#16324a" }
                                Text { text: modelData.result; font.pixelSize: root.bodyPx; color: "#16788f" }
                                Text { text: root.tr("source") + ": " + modelData.source; font.pixelSize: root.auxPx; color: "#5b7184" }
                            }
                            BodyText { Layout.fillWidth: true; text: root.tr("reason") + ": " + modelData.reason }
                            BodyText { Layout.fillWidth: true; text: root.tr("next") + ": " + modelData.nextStep }
                        }
                    }
                }
            }

            ScrollPage {
                id: integrationPage
                objectName: "hyIntegrationPage"
                SectionTitle { Layout.fillWidth: true; text: root.tr("routes"); font.pixelSize: root.titlePx }
                Basic.TabBar {
                    id: routeTabs
                    objectName: "hyRouteTabs"
                    Layout.fillWidth: true
                    currentIndex: root.currentRoute
                    onCurrentIndexChanged: root.currentRoute = currentIndex
                    Basic.TabButton { objectName: "hyRouteTab_cpp"; text: root.shortRoute("cpp"); checked: root.currentRoute === 0; onClicked: root.currentRoute = 0 }
                    Basic.TabButton { objectName: "hyRouteTab_qml"; text: root.shortRoute("qml"); checked: root.currentRoute === 1; onClicked: root.currentRoute = 1 }
                    Basic.TabButton { objectName: "hyRouteTab_generic"; text: root.shortRoute("generic"); checked: root.currentRoute === 2; onClicked: root.currentRoute = 2 }
                    Basic.TabButton { objectName: "hyRouteTab_qpa"; text: root.shortRoute("qpa"); checked: root.currentRoute === 3; onClicked: root.currentRoute = 3 }
                }
                Rectangle {
                    objectName: "hyIntegrationStatus"
                    Layout.fillWidth: true
                    radius: 4
                    color: {
                        const status = hyWorkbench.inspectionSummary.status || ""
                        if (status === "MISMATCH" || status === "STATIC_MISSING") return "#fff2e8"
                        if (status === "UNKNOWN_SOURCE") return "#f4f0e8"
                        if (status === "STATIC_PASS") return "#e8f5ee"
                        return "#eef3f7"
                    }
                    border.color: "#d7e3ea"
                    implicitHeight: statusColumn.implicitHeight + 16
                    ColumnLayout {
                        id: statusColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 8
                        spacing: 4
                        Text { objectName: "hyIntegrationStatusText"; Layout.fillWidth: true; text: hyWorkbench.inspectionSummary.label + " · " + hyWorkbench.inspectionSummary.reason; font.pixelSize: root.bodyPx; font.bold: true; color: "#16324a"; wrapMode: Text.WordWrap }
                        Text { objectName: "hyIntegrationStatusNext"; Layout.fillWidth: true; text: root.tr("next") + ": " + hyWorkbench.inspectionSummary.nextStep; font.pixelSize: root.auxPx; color: "#40586b"; wrapMode: Text.WordWrap }
                    }
                }
                StackLayout {
                    id: routeStack
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: Math.max(320, Math.min(470, root.height - 250))
                    currentIndex: root.currentRoute
                    RoutePanel { objectName: "hyRoutePanel_cpp"; routeIndex: 0; visible: root.currentRoute === 0 }
                    RoutePanel { objectName: "hyRoutePanel_qml"; routeIndex: 1; visible: root.currentRoute === 1 }
                    RoutePanel { objectName: "hyRoutePanel_generic"; routeIndex: 2; visible: root.currentRoute === 2 }
                    RoutePanel { objectName: "hyRoutePanel_qpa"; routeIndex: 3; visible: root.currentRoute === 3 }
                }
            }

            ScrollPage {
                id: connectionPage
                objectName: "hyConnectionPage"
                SectionTitle { Layout.fillWidth: true; text: root.tr("connection"); font.pixelSize: root.titlePx }
                RowLayout { spacing: 8
                    Basic.TextField { id: hostField; objectName: "hyHostField"; Layout.fillWidth: true; implicitHeight: 36; font.pixelSize: root.bodyPx; placeholderText: root.tr("host"); text: "127.0.0.1" }
                    Basic.SpinBox { id: portField; objectName: "hyPortField"; from: 1; to: 65535; value: 5921; font.pixelSize: root.bodyPx }
                    ActionButton { objectName: "hyConnectButton"; primary: true; text: root.tr("connect"); onClicked: hyWorkbench.connectHost(hostField.text, portField.value) }
                    ActionButton { objectName: "hyCancelButton"; text: root.tr("cancel"); onClicked: hyWorkbench.cancelConnection() }
                    ActionButton { objectName: "hyEndButton"; text: root.tr("end"); onClicked: hyWorkbench.endSession() }
                }
                GridLayout { columns: 2; rowSpacing: 8; columnSpacing: 8
                    BodyText { text: root.tr("stage") + ": " + hyWorkbench.stageText; objectName: "hyStageText" }
                    BodyText { text: root.tr("failure") + ": " + hyWorkbench.failureText; objectName: "hyFailureText" }
                    BodyText { Layout.columnSpan: 2; text: root.tr("nextStep") + ": " + hyWorkbench.nextStep; objectName: "hyNextStepText" }
                    BodyText { Layout.columnSpan: 2; text: hyWorkbench.connectionState + " | " + root.tr("stats") + ": " + hyWorkbench.frames + " / " + hyWorkbench.bytes + " / " + hyWorkbench.fps.toFixed(1) + " / " + hyWorkbench.mbps.toFixed(2); objectName: "hyStatsText" }
                }
                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 180; color: "#0d1720"; radius: 6
                    Image { anchors.fill: parent; anchors.margins: 6; fillMode: Image.PreserveAspectFit; source: hyWorkbench.sampleImage }
                    Text { anchors.centerIn: parent; visible: hyWorkbench.sampleImage.length === 0; text: root.tr("preview"); color: "#8aa4b5"; font.pixelSize: root.bodyPx }
                }
                ActionButton { objectName: "hySampleButton"; text: root.tr("sample"); onClicked: hyWorkbench.sampleFrame() }
                Basic.TextArea { objectName: "hyEventsText"; Layout.fillWidth: true; Layout.preferredHeight: 100; readOnly: true; font.pixelSize: root.bodyPx; text: hyWorkbench.eventsText; wrapMode: Text.WordWrap; background: Rectangle { color: "#ffffff"; border.color: "#d7e3ea" } }
            }

            ScrollPage {
                id: diagnosticsPage
                objectName: "hyDiagnosticsPage"
                SectionTitle { Layout.fillWidth: true; text: root.tr("diagnostics"); font.pixelSize: root.titlePx }
                BodyText { Layout.fillWidth: true; text: root.tr("masked") }
                Basic.TextArea { objectName: "hyDiagnosticsText"; Layout.fillWidth: true; Layout.preferredHeight: 220; readOnly: true; selectByMouse: true; font.family: "Consolas"; font.pixelSize: root.codePx; text: hyWorkbench.diagnosticsJson; wrapMode: Text.WordWrap; background: Rectangle { color: "#ffffff"; border.color: "#d7e3ea" } }
                RowLayout { spacing: 8
                    ActionButton { objectName: "hyCopyDiagnostics"; text: root.tr("copy"); onClicked: hyWorkbench.copyDiagnostics() }
                    ActionButton { objectName: "hySaveDiagnostics"; text: root.tr("save"); onClicked: hyWorkbench.saveDiagnostics() }
                }
                Basic.TextArea { objectName: "hySdkSummaryText"; Layout.fillWidth: true; Layout.preferredHeight: 130; readOnly: true; selectByMouse: true; font.family: "Consolas"; font.pixelSize: root.codePx; text: hyWorkbench.sdkSummary; wrapMode: Text.WordWrap; background: Rectangle { color: "#ffffff"; border.color: "#d7e3ea" } }
            }
        }
    }
}
