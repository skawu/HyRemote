import QtQuick
import QtQuick.Controls
import HyRemote

ApplicationWindow {
    id: window
    width: 360
    height: 220
    visible: true
    title: "HyRemote QML Basic"

    color: "#202733"

    RemoteAccess {
        id: remote
        target: window
        listenAddress: "127.0.0.1"
        port: acceptancePort
        remoteInputEnabled: acceptanceRemoteInput
        enabled: false

        onStateChanged: {
            if (state === RemoteAccess.Running)
                console.log("READY " + port)
        }
        onErrorChanged: {
            if (errorString.length > 0)
                console.log("REMOTE_ERROR " + errorString)
        }
    }

    Column {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        Label {
            text: "HyRemote · Declarative QML API"
            color: "#f2f4f8"
            font.pixelSize: 19
        }

        Rectangle {
            width: 140
            height: 40
            radius: 3
            color: "#3977d5"

            Label {
                anchors.centerIn: parent
                text: "Remote click target"
                color: "white"
            }

            MouseArea {
                anchors.fill: parent
                onPressed: console.log("APP_POINTER x=" + mouseX + " y=" + mouseY)
                onClicked: {
                    console.log("APP_CLICKED")
                    editor.forceActiveFocus()
                }
            }
        }

        TextField {
            id: editor
            width: 320
            placeholderText: "Remote keyboard/text target"
            Keys.onPressed: event => console.log("APP_KEY key=" + event.key)
            onTextChanged: {
                if (text.length > 0)
                    console.log("APP_TEXT text=" + text)
            }
        }

        Label {
            text: acceptanceRemoteInput ? "Remote control: enabled explicitly"
                                        : "Remote control: view-only default"
            color: "#f2f4f8"
        }

        Label {
            text: "State: " + remote.state + " · 127.0.0.1:" + acceptancePort
            color: "#c6ceda"
        }
    }

    Component.onCompleted: {
        editor.forceActiveFocus()
        remote.enabled = true
        if (!remote.enabled && remote.errorString.length > 0) {
            console.log("START_FAILED " + remote.errorString)
            Qt.exit(2)
        }
    }

    Timer {
        interval: acceptanceTimeoutMs
        running: acceptanceTimeoutMs > 0
        repeat: false
        onTriggered: {
            remote.enabled = false
            console.log("STOPPED")
            Qt.quit()
        }
    }
}
