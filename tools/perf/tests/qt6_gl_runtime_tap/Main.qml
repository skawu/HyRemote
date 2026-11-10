// Zero-HyRemote-dependency Generic test app. Real Qt6 OpenGL scenegraph
// renders four independently colored quadrants. Each phase is a complete
// 2D image: the Raw RFB client must observe correct spatial orientation
// and reject mixed old/new quadrant content.
import QtQuick

Rectangle {
    id: root
    width: 640
    height: 360
    property int phase: 0
    color: "#000000"

    readonly property var palette: ["#e22634", "#27cf40", "#2c44e8", "#eec41e"]

    Rectangle {
        x: 0; y: 0; width: root.width / 2; height: root.height / 2
        color: root.palette[root.phase]
    }
    Rectangle {
        x: root.width / 2; y: 0; width: root.width / 2; height: root.height / 2
        color: root.palette[(root.phase + 1) % 4]
    }
    Rectangle {
        x: 0; y: root.height / 2; width: root.width / 2; height: root.height / 2
        color: root.palette[(root.phase + 2) % 4]
    }
    Rectangle {
        x: root.width / 2; y: root.height / 2; width: root.width / 2; height: root.height / 2
        color: root.palette[(root.phase + 3) % 4]
    }

    Timer {
        interval: 120
        repeat: true
        running: true
        onTriggered: {
            root.phase = (root.phase + 1) % 3
            console.log("SOURCE_STAGE=" + root.phase)
        }
    }
}
