// Test-only 1920x1080 full-scene A -> B -> A workload for Windows Generic Qt Quick.
// Only the application fixture changes. HyRemote product at each source SHA remains untouched.
// Both historical versions consume these EXACT SAME bytes. No animations, partial repaint,
// gradients or intentionally split-screen phases: mixed colors in the Viewer are defects.
import QtQuick

Rectangle {
    id: root
    width: 1920
    height: 1080
    property int phase: 0
    color: phase === 0 ? "#173cdb" : "#e8263b"

    function emitStage() {
        // Host application clock; Qt scene rendering/public capture happens later.
        // This is the source-change timestamp, NOT Viewer presentation time.
        console.log("HYREMOTE_DYNAMIC_STAGE_JSON=" + JSON.stringify({
            phase: phase,
            utc_ms: Date.now(),
            scene: "full-screen-solid",
            pixels: "1920x1080",
            source_timestamp_kind: "QML_TIMER_NOT_RENDER_OR_VIEWER"
        }))
    }

    Timer {
        interval: 1200
        repeat: true
        running: true
        onTriggered: {
            root.phase = 1 - root.phase
            root.emitStage()
        }
    }

    Component.onCompleted: emitStage()

    Rectangle {
        anchors.centerIn: parent
        width: 820
        height: 90
        radius: 8
        color: "#1b2030"
        opacity: 0.94

        Text {
            anchors.centerIn: parent
            text: "HyRemote Windows 1080p dynamic A-B-A: " + (root.phase === 0 ? "A BLUE" : "B RED")
            color: "white"
            font.pixelSize: 30
            font.bold: true
        }
    }
}
