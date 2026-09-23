// Plain Qt Quick UI for HyRemote learning example 02.
//
// There is deliberately no `import HyRemote` here: a Quick application reaches HyRemote through the same public C++
// facade a Widgets application uses, and this file only draws the window.
import QtQuick

Rectangle {
    id: root

    property bool remoteControlEnabled: false
    property int listenPort: 5921
    property int connectedClientCount: 0

    width: 420
    height: 180
    color: "#202733"

    Column {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 10

        Text {
            text: "HyRemote · Embedded C++ / Qt Quick"
            color: "#f2f4f8"
            font.pixelSize: 18
        }

        Text {
            text: "remote control: " + (root.remoteControlEnabled ? "enabled explicitly" : "view-only (default)")
            color: "#c8d2e0"
        }

        Text {
            text: "listener: 0.0.0.0:" + root.listenPort
            color: "#c8d2e0"
        }

        Text {
            text: "This UI has no HyRemote import - it is the C++ facade that provides remote access."
            color: "#8fa3bd"
            wrapMode: Text.WordWrap
            width: parent.width
        }
    }
}
