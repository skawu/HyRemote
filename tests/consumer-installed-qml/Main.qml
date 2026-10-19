import QtQuick
import QtQuick.Window
import HyRemote

Window {
    id: root
    width: 96
    height: 64
    visible: true

    // The clean consumer exercises the installed declarative type without starting a second remote
    // runtime. In the combined QML+QPA configuration the transparent platform plugin owns remote
    // access while this same QML module remains loadable from the deployed tree.
    RemoteAccess {
        id: remote
        target: root
        enabled: false
    }

    function reportValue(report, key) {
        const prefix = key + "="
        const start = report.indexOf(prefix)
        if (start < 0)
            return ""
        const valueStart = start + prefix.length
        const end = report.indexOf("\n", valueStart)
        return end < 0 ? report.slice(valueStart) : report.slice(valueStart, end)
    }

    readonly property string diagnostics: remote.diagnosticReport()
    readonly property string buildIdentity: reportValue(diagnostics, "BUILD_IDENTITY")
    readonly property string deploymentIdentity: reportValue(diagnostics, "DEPLOYMENT_IDENTITY")
    readonly property bool contractOk:
        !remote.enabled && remote.state === RemoteAccess.Stopped && remote.target === root &&
        diagnostics.includes("INTEGRATION_ROUTE=qml\n") &&
        diagnostics.includes("UI_FAMILY=quick\n") &&
        diagnostics.includes("STATE=Stopped\n") &&
        buildIdentity !== "" && buildIdentity !== "unknown" &&
        deploymentIdentity === buildIdentity
}
