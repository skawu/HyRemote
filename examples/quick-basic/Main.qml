import QtQuick

Rectangle {
    id: root
    width: 360
    height: 220
    color: "#202733"
    property bool remoteControlEnabled: false
    property int connectedClientCount: 0

    Text {
        x: 20
        y: 18
        width: 320
        height: 32
        text: "HyRemote · C++ API / Qt Quick"
        color: "#f2f4f8"
        font.pixelSize: 18
    }

    Rectangle {
        id: clickTarget
        x: 20
        y: 70
        width: 140
        height: 40
        radius: 4
        color: mouse.pressed ? "#2a5ea9" : "#3977d5"

        Text {
            anchors.centerIn: parent
            text: "Remote click target"
            color: "white"
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            onClicked: {
                editor.forceActiveFocus()
            }
        }
    }

    Text {
        id: status
        x: 180
        y: 66
        width: 160
        height: 48
        text: "Listening\nclients: " + root.connectedClientCount
        color: "#f2f4f8"
        wrapMode: Text.WordWrap
    }

    Rectangle {
        x: 20
        y: 130
        width: 320
        height: 36
        color: "white"
        radius: 3

        TextInput {
            id: editor
            anchors.fill: parent
            anchors.margins: 6
            color: "#202733"
            text: ""
            focus: true
            clip: true
        }
    }

    Text {
        x: 20
        y: 178
        width: 320
        height: 24
        text: root.remoteControlEnabled
              ? "Remote control: enabled explicitly"
              : "Remote control: view-only default"
        color: "#d7deea"
    }

    Component.onCompleted: editor.forceActiveFocus()
}
