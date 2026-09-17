// Async capture spike scene (issue #16).
//
// The harness drives `tick` from C++, so every captured image carries the scene
// state it was rendered from in an 8-bit-exact color patch named "tickPatch".
// `frozen` stops the animation for the pixel-exact fidelity comparison.

import QtQuick

Item {
    id: root

    property int tick: 0
    property bool frozen: false
    property real angle: 0

    Rectangle {
        anchors.fill: parent
        color: "#101418"
    }

    // Encoded capture counter: red = low byte, green = high byte, blue = 1.
    Rectangle {
        objectName: "tickPatch"
        x: 8
        y: 8
        width: 48
        height: 48
        color: Qt.rgba((root.tick & 0xff) / 255.0, ((root.tick >> 8) & 0xff) / 255.0, 1.0, 1.0)
    }

    // Static chrome used by the fidelity comparison.
    Rectangle {
        objectName: "staticBar"
        x: 80
        y: 8
        width: 240
        height: 48
        color: "#3fb950"
    }

    Rectangle {
        objectName: "staticBlock"
        x: 80
        y: 90
        width: 160
        height: 160
        color: "#4a90d9"

        Text {
            anchors.centerIn: parent
            text: "static"
            color: "#101418"
            font.pixelSize: 20
        }
    }

    Rectangle {
        objectName: "mover"
        width: 80
        height: 80
        radius: 16
        color: "#e0592a"
        x: 320 + 260 * Math.sin(root.angle)
        y: 320
    }

    Text {
        objectName: "tickLabel"
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 16
        text: "tick " + root.tick
        color: "#e6edf3"
        font.pixelSize: 20
    }

    NumberAnimation on angle {
        from: 0
        to: Math.PI * 2
        duration: 2000
        loops: Animation.Infinite
        running: !root.frozen
    }
}
