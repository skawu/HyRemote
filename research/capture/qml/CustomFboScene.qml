// SPIKE-01 throwaway custom QQuickFramebufferObject scene (issue #3, case E).
//
// The CustomFboItem type comes from the same QML module, so it is available
// without an explicit import.

import QtQuick

Item {
    id: root

    property int frameCounter: 0

    Rectangle {
        anchors.fill: parent
        color: "#0d1117"
    }

    // Position and size are mirrored by CustomFboSample so that the captured
    // pixel inside this region can be decoded.
    CustomFboItem {
        id: fbo
        x: 40
        y: 40
        width: 280
        height: 280
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 8
        color: "transparent"
        border.color: "#3fb950"
        border.width: 4
    }

    Text {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 24
        anchors.leftMargin: 348
        text: "custom QQuickFramebufferObject\nclear color encodes the render counter"
        color: "#e6edf3"
        font.pixelSize: 20
    }

    Text {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 24
        text: "frame " + root.frameCounter
        color: "#e6edf3"
        font.pixelSize: 22
    }

    Timer {
        interval: 16
        repeat: true
        running: true
        onTriggered: root.frameCounter++
    }
}
