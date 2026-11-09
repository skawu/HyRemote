// Test-only 1920x1080 full-scene A-B-A workload for Windows Generic Qt Quick.
// The identical fixture overlays each pinned historical app; product Runtime is NOT edited.
// A and B each paint one SOLID color across the entire window. Half-red/half-blue
// on the physical Viewer screen is not an intentional intermediate page.
import QtQuick

Rectangle {
    id: root
    width: 1920
    height: 1080
    property int phase: 0
    color: phase === 0 ? "#173cdb" : "#e8263b"

    // The paired fixture C++ main.cpp sets "phase" every 1200ms and writes
    // an unbuffered HYREMOTE_DYNAMIC_STAGE_JSON line to stdout with UTC ms.
    // It timestamps the source change before Qt render/capture, not Viewer paint.
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
