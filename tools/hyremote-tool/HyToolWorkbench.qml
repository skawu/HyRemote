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
    title: "HyRemoteTool · Developer Workbench"

    property int currentPage: 0
    property var hyWorkbench: null
    function setPage(index) { currentPage = Math.max(0, Math.min(3, index)) }

    function tr(key) {
        const zh = hyWorkbench.language === "zh"
        const map = {
            product: zh ? "华艳远程开发工具" : "HyRemoteTool",
            version: zh ? "V0.3.6.0 · 研发工作台" : "V0.3.6.0 · Developer Workbench",
            environment: zh ? "开发环境" : "Environment",
            integration: zh ? "集成指导" : "Integration",
            connection: zh ? "连接验证" : "Connection",
            diagnostics: zh ? "诊断报告" : "Diagnostics",
            help: zh ? "帮助" : "Help",
            language: zh ? "语言" : "Language",
            legacy: zh ? "兼容联调入口" : "Legacy access",
            sdkRoot: zh ? "SDK 安装根（显式选择，不扫描）" : "SDK install root (explicit, no scan)",
            kitRoot: zh ? "Qt Kit 根（显式选择，不扫描）" : "Qt Kit root (explicit, no scan)",
            inspect: zh ? "静态核对" : "Inspect",
            checks: zh ? "静态核对结果" : "Static checks",
            source: zh ? "来源" : "Source",
            reason: zh ? "原因" : "Reason",
            next: zh ? "下一步" : "Next step",
            routes: zh ? "接入路线" : "Integration routes",
            prerequisites: zh ? "前置条件" : "Prerequisites",
            minimalCall: zh ? "最小调用" : "Minimal call",
            cmakeDeploy: zh ? "CMake 部署" : "CMake deploy",
            launchVerify: zh ? "启动与验证" : "Launch / verify",
            copy: zh ? "复制" : "Copy",
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

    background: Rectangle { color: "#f4f7fa" }

    header: Rectangle {
        height: 64
        color: "#ffffff"
        RowLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 12
            Image {
                source: "qrc:/hyremote/branding/huayan-logo-single.png"
                Layout.preferredWidth: 36
                Layout.preferredHeight: 36
                fillMode: Image.PreserveAspectFit
            }
            ColumnLayout {
                spacing: 2
                Text { text: root.tr("product"); font.pixelSize: 16; font.bold: true; color: "#16324a" }
                Text { text: root.tr("version"); font.pixelSize: 11; color: "#5b7184" }
            }
            Item { Layout.fillWidth: true }
            Text { text: root.tr("language"); font.pixelSize: 12; color: "#5b7184" }
            ComboBox {
                objectName: "hyLanguageCombo"
                model: ["Auto", "简体中文", "English"]
                currentIndex: hyWorkbench.languagePreference === "zh" ? 1 : (hyWorkbench.languagePreference === "en" ? 2 : 0)
                onActivated: hyWorkbench.setLanguagePreference(index === 1 ? "zh" : index === 2 ? "en" : "auto")
            }
            Button { text: root.tr("help"); onClicked: helpDialog.open() }
            Button { objectName: "hyLegacyButton"; text: root.tr("legacy"); onClicked: legacyDialog.open() }
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
            Layout.preferredWidth: 180
            Layout.fillHeight: true
            color: "#0f2c3f"
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 8
                Basic.Button {
                    id: navEnvironment
                    objectName: "hyNavEnvironment"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    text: root.tr("environment")
                    checkable: true
                    checked: root.currentPage === 0
                    background: Rectangle { color: navEnvironment.checked ? "#16788f" : "#163e55"; radius: 4 }
                    contentItem: Text {
                        text: navEnvironment.text
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: root.setPage(0)
                }
                Basic.Button {
                    id: navIntegration
                    objectName: "hyNavIntegration"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    text: root.tr("integration")
                    checkable: true
                    checked: root.currentPage === 1
                    background: Rectangle { color: navIntegration.checked ? "#16788f" : "#163e55"; radius: 4 }
                    contentItem: Text {
                        text: navIntegration.text
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: root.setPage(1)
                }
                Basic.Button {
                    id: navConnection
                    objectName: "hyNavConnection"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    text: root.tr("connection")
                    checkable: true
                    checked: root.currentPage === 2
                    background: Rectangle { color: navConnection.checked ? "#16788f" : "#163e55"; radius: 4 }
                    contentItem: Text {
                        text: navConnection.text
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: root.setPage(2)
                }
                Basic.Button {
                    id: navDiagnostics
                    objectName: "hyNavDiagnostics"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    text: root.tr("diagnostics")
                    checkable: true
                    checked: root.currentPage === 3
                    background: Rectangle { color: navDiagnostics.checked ? "#16788f" : "#163e55"; radius: 4 }
                    contentItem: Text {
                        text: navDiagnostics.text
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: root.setPage(3)
                }
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

            Flickable {
                id: environmentPage
                objectName: "hyEnvironmentPage"
                contentWidth: width
                contentHeight: envColumn.implicitHeight + 32
                clip: true
                ColumnLayout {
                    id: envColumn
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12
                    Text { text: root.tr("environment"); font.pixelSize: 20; font.bold: true; color: "#16324a" }
                    RowLayout { spacing: 8
                        TextField { id: sdkField; objectName: "hySdkRootField"; Layout.fillWidth: true; placeholderText: root.tr("sdkRoot"); text: hyWorkbench.sdkRoot; onEditingFinished: hyWorkbench.setSdkRoot(text) }
                        TextField { id: kitField; objectName: "hyKitRootField"; Layout.fillWidth: true; placeholderText: root.tr("kitRoot"); text: hyWorkbench.kitRoot; onEditingFinished: hyWorkbench.setKitRoot(text) }
                        Button { objectName: "hyInspectButton"; text: root.tr("inspect"); onClicked: { hyWorkbench.setSdkRoot(sdkField.text); hyWorkbench.setKitRoot(kitField.text); hyWorkbench.inspectSelectedPaths() } }
                    }
                    GroupBox {
                        Layout.fillWidth: true
                        title: root.tr("checks")
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8
                            Repeater {
                                model: hyWorkbench.checks
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: checkColumn.implicitHeight + 20
                                    radius: 6
                                    color: "#ffffff"
                                    border.color: "#d7e3ea"
                                    ColumnLayout {
                                        id: checkColumn
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        anchors.margins: 10
                                        spacing: 4
                                        RowLayout { spacing: 8
                                            Text { text: modelData.id; font.bold: true; color: "#16324a" }
                                            Text { text: modelData.result; color: "#16788f" }
                                            Text { text: root.tr("source") + ": " + modelData.source; color: "#5b7184" }
                                        }
                                        Text { Layout.fillWidth: true; text: root.tr("reason") + ": " + modelData.reason; wrapMode: Text.WordWrap; color: "#40586b" }
                                        Text { Layout.fillWidth: true; text: root.tr("next") + ": " + modelData.nextStep; wrapMode: Text.WordWrap; color: "#40586b" }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Flickable {
                id: integrationPage
                objectName: "hyIntegrationPage"
                contentWidth: width
                contentHeight: integrationColumn.implicitHeight + 32
                clip: true
                ColumnLayout {
                    id: integrationColumn
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12
                    Text { text: root.tr("routes"); font.pixelSize: 20; font.bold: true; color: "#16324a" }
                    RowLayout { spacing: 8
                        Repeater {
                            model: hyWorkbench.routes
                            Button {
                                objectName: "hyRoute_" + modelData.id
                                text: modelData.title
                                checkable: true
                                checked: routeTabs.currentIndex === index
                                onClicked: routeTabs.currentIndex = index
                            }
                        }
                    }
                    SwipeView {
                        id: routeTabs
                        Layout.fillWidth: true
                        Layout.preferredHeight: 430
                        Repeater {
                            model: hyWorkbench.routes
                            Rectangle {
                                radius: 8
                                color: "#ffffff"
                                border.color: "#d7e3ea"
                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 16
                                    spacing: 10
                                    RowLayout { spacing: 8
                                        Text { text: modelData.title; font.pixelSize: 16; font.bold: true; color: "#16324a" }
                                        Text { objectName: "hyRouteStatus_" + modelData.id; text: hyWorkbench.routeStatuses[modelData.id] || "UNKNOWN_SOURCE"; color: "#16788f" }
                                    }
                                    Label { Layout.fillWidth: true; text: root.tr("prerequisites") + ":\n" + modelData.prerequisites; wrapMode: Text.WordWrap }
                                    Label { Layout.fillWidth: true; text: root.tr("minimalCall") + ":\n" + modelData.call; wrapMode: Text.WordWrap; font.family: "Consolas" }
                                    Label { Layout.fillWidth: true; text: root.tr("cmakeDeploy") + ":\n" + modelData.deploy; wrapMode: Text.WordWrap; font.family: "Consolas" }
                                    Label { Layout.fillWidth: true; text: root.tr("launchVerify") + ":\n" + modelData.launch + "\n" + modelData.verify; wrapMode: Text.WordWrap }
                                    Button { objectName: "hyCopyGuide_" + modelData.id; text: root.tr("copy"); onClicked: hyWorkbench.copyGuidance(modelData.id) }
                                }
                            }
                        }
                    }
                }
            }

            Flickable {
                id: connectionPage
                objectName: "hyConnectionPage"
                contentWidth: width
                contentHeight: connectionColumn.implicitHeight + 32
                clip: true
                ColumnLayout {
                    id: connectionColumn
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12
                    Text { text: root.tr("connection"); font.pixelSize: 20; font.bold: true; color: "#16324a" }
                    RowLayout { spacing: 8
                        TextField { id: hostField; objectName: "hyHostField"; Layout.fillWidth: true; placeholderText: root.tr("host"); text: "127.0.0.1" }
                        SpinBox { id: portField; objectName: "hyPortField"; from: 1; to: 65535; value: 5921 }
                        Button { objectName: "hyConnectButton"; text: root.tr("connect"); onClicked: hyWorkbench.connectHost(hostField.text, portField.value) }
                        Button { objectName: "hyCancelButton"; text: root.tr("cancel"); onClicked: hyWorkbench.cancelConnection() }
                        Button { objectName: "hyEndButton"; text: root.tr("end"); onClicked: hyWorkbench.endSession() }
                    }
                    GridLayout { columns: 2; rowSpacing: 8; columnSpacing: 8
                        Label { text: root.tr("stage") + ": " + hyWorkbench.stageText; objectName: "hyStageText" }
                        Label { text: root.tr("failure") + ": " + hyWorkbench.failureText; objectName: "hyFailureText" }
                        Label { Layout.columnSpan: 2; text: root.tr("nextStep") + ": " + hyWorkbench.nextStep; objectName: "hyNextStepText"; wrapMode: Text.WordWrap }
                        Label { Layout.columnSpan: 2; text: hyWorkbench.connectionState + " | " + root.tr("stats") + ": " + hyWorkbench.frames + " / " + hyWorkbench.bytes + " / " + hyWorkbench.fps.toFixed(1) + " / " + hyWorkbench.mbps.toFixed(2); objectName: "hyStatsText" }
                    }
                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 180; color: "#0d1720"; radius: 6
                        Image { anchors.fill: parent; anchors.margins: 6; fillMode: Image.PreserveAspectFit; source: hyWorkbench.sampleImage }
                        Text { anchors.centerIn: parent; visible: hyWorkbench.sampleImage.length === 0; text: root.tr("preview"); color: "#8aa4b5" }
                    }
                    Button { objectName: "hySampleButton"; text: root.tr("sample"); onClicked: hyWorkbench.sampleFrame() }
                    TextArea { objectName: "hyEventsText"; Layout.fillWidth: true; Layout.preferredHeight: 100; readOnly: true; text: hyWorkbench.eventsText; wrapMode: Text.WordWrap }
                }
            }

            Flickable {
                id: diagnosticsPage
                objectName: "hyDiagnosticsPage"
                contentWidth: width
                contentHeight: diagnosticsColumn.implicitHeight + 32
                clip: true
                ColumnLayout {
                    id: diagnosticsColumn
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12
                    Text { text: root.tr("diagnostics"); font.pixelSize: 20; font.bold: true; color: "#16324a" }
                    Label { Layout.fillWidth: true; text: root.tr("masked"); color: "#5b7184"; wrapMode: Text.WordWrap }
                    TextArea { objectName: "hyDiagnosticsText"; Layout.fillWidth: true; Layout.preferredHeight: 220; readOnly: true; text: hyWorkbench.diagnosticsJson; wrapMode: Text.WordWrap; font.family: "Consolas" }
                    RowLayout { spacing: 8
                        Button { objectName: "hyCopyDiagnostics"; text: root.tr("copy"); onClicked: hyWorkbench.copyDiagnostics() }
                        Button { objectName: "hySaveDiagnostics"; text: root.tr("save"); onClicked: hyWorkbench.saveDiagnostics() }
                    }
                    TextArea { objectName: "hySdkSummaryText"; Layout.fillWidth: true; Layout.preferredHeight: 130; readOnly: true; text: hyWorkbench.sdkSummary; wrapMode: Text.WordWrap; font.family: "Consolas" }
                }
            }
        }
    }
}
