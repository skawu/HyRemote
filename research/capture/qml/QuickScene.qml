// SPDX-License-Identifier: Apache-2.0
// SPIKE-01 throwaway Qt Quick 2D scene (issue #3, case B).
//
// Deliberately mixes a continuously animated region with static chrome so that
// a capture can be checked against the expected content.

import QtQuick

Item {
    id: root

    property int frameCounter: 0
    property real angle: 0

    Rectangle {
        anchors.fill: parent
        color: "#0d1117"
    }

    Text {
        id: title
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 16
        text: "HyRemote SPIKE-01 :: Qt Quick 2D"
        color: "#e6edf3"
        font.pixelSize: 24
    }

    Repeater {
        model: 24

        delegate: Rectangle {
            required property int index

            width: 56
            height: 56
            radius: 10
            color: Qt.hsla(index / 24.0, 0.6, 0.55, 1.0)
            x: 40 + (index % 8) * 110 + 24 * Math.sin(root.angle + index)
            y: 90 + Math.floor(index / 8) * 130 + 24 * Math.cos(root.angle + index)

            Text {
                anchors.centerIn: parent
                text: parent.index
                color: "#0d1117"
                font.pixelSize: 18
                font.bold: true
            }
        }
    }

    Rectangle {
        id: ticker
        width: 260
        height: 64
        radius: 12
        color: "#f2c14e"
        x: 40
        y: root.height - 116

        Text {
            anchors.centerIn: parent
            text: "frame " + root.frameCounter
            color: "#0d1117"
            font.pixelSize: 22
        }
    }

    Rectangle {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 16
        width: 200
        height: 84
        radius: 8
        color: "#161b22"
        border.color: "#3fb950"
        border.width: 3

        Text {
            anchors.centerIn: parent
            text: "static region\n(no damage expected)"
            color: "#3fb950"
            font.pixelSize: 16
            horizontalAlignment: Text.AlignHCenter
        }
    }

    NumberAnimation on angle {
        from: 0
        to: Math.PI * 2
        duration: 1600
        loops: Animation.Infinite
        running: true
    }

    Timer {
        interval: 16
        repeat: true
        running: true
        onTriggered: root.frameCounter++
    }
}
