import QtQuick
import QtQuick.Controls
import HyRemote

ApplicationWindow {
    id: win
    width: 1920
    height: 1080
    flags: Qt.FramelessWindowHint | Qt.Window
    visible: true
    color: "#0f1722"
    title: enabledRemote ? "B609|quick|remote" : "B609|quick|local"

    property bool enabledRemote: Qt.application.arguments.indexOf("--no-remote") < 0

    RemoteAccess {
        id: remote
        enabled: win.enabledRemote
        target: win
        port: 5921
    }

    // Deterministic 1920x1080 scene: gradient band, tile grid and three text lines.
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#10243a" }
            GradientStop { position: 1.0; color: "#08131f" }
        }
    }

    Grid {
        anchors.fill: parent
        columns: 24
        Repeater {
            model: 24 * 13
            Rectangle {
                width: 80
                height: 83
                color: Qt.hsla(((index * 29) % 360) / 360.0, 0.35, 0.30, 1.0)
                border.width: 1
                border.color: "#16324a"
            }
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: 18
        Text {
            color: "#e9f1fa"
            font.pixelSize: 44
            font.bold: true
            text: "B609 static 1080p scene"
        }
        Text {
            color: "#8fc7ee"
            font.pixelSize: 28
            text: "widgets/quick baseline, no animation"
        }
    }

    Timer {
        interval: 240000
        running: true
        repeat: false
        onTriggered: Qt.exit(0)
    }
}
