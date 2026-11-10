// Same ordinary Qt-only Generic application, no HyRemote imports.
// The actual QQuickView source keeps rendering independently of Viewer demand.
import QtQuick

Rectangle {
    id: root
    width: 640
    height: 360
    property int phase: 0
    color: phase === 0 ? "#e22634" : (phase === 1 ? "#27cf40" : "#2c44e8")

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
