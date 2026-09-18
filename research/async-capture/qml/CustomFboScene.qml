// SPDX-License-Identifier: Apache-2.0
// Async capture spike custom QQuickFramebufferObject scene (issue #16).
//
// The custom FBO renderer encodes its own render counter in its clear color, so a
// captured image can be attributed to the render that produced it. CustomFboItem
// comes from the same QML module, so it needs no explicit import.

import QtQuick

Item {
    id: root

    property int tick: 0
    property bool frozen: false

    Rectangle {
        anchors.fill: parent
        color: "#101418"
    }

    Rectangle {
        objectName: "tickPatch"
        x: 8
        y: 8
        width: 48
        height: 48
        color: Qt.rgba((root.tick & 0xff) / 255.0, ((root.tick >> 8) & 0xff) / 255.0, 1.0, 1.0)
    }

    Rectangle {
        objectName: "staticBlock"
        x: 80
        y: 90
        width: 160
        height: 160
        color: "#3fb950"
    }

    // Position and size are mirrored by the host so that the captured pixel
    // inside this region can be decoded.
    CustomFboItem {
        id: fbo
        objectName: "fboItem"
        x: 360
        y: 160
        width: 280
        height: 280
        frozen: root.frozen
    }

    Text {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 16
        text: "fbo counter " + fboCounter
        color: "#e6edf3"
        font.pixelSize: 20

        property int fboCounter: 0
    }
}
