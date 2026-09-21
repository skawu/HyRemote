// Plain Qt Quick UI for the ordinary Qt-only application in HyRemote learning example 03.
// No HyRemote import, no HyRemote types: this application is not aware of HyRemote at all.
import QtQuick

Rectangle {
    width: 460
    height: 190
    color: "#202733"

    Column {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 10

        Text {
            text: "An ordinary Qt Quick application"
            color: "#f2f4f8"
            font.pixelSize: 18
        }

        Text {
            text: "This file has no HyRemote import and the application links no HyRemote library."
            color: "#c8d2e0"
            wrapMode: Text.WordWrap
            width: parent.width
        }

        Text {
            text: "Launch it with  -plugin hyremote  to make it remotely viewable with no source change."
            color: "#8fa3bd"
            wrapMode: Text.WordWrap
            width: parent.width
        }
    }
}
