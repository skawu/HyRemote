import QtQuick
import QtQuick.Controls
import HyRemote

ApplicationWindow {
    id: window
    width: 360
    height: 240
    visible: true
    title: "HyRemote QML Basic"

    color: "#202733"
    property bool runtimeRemoteInput: acceptanceRemoteInput

    // Normal declarative use is target + enabled. The port/input bindings and transition timer
    // exist only so repository product-fit can exercise non-default policy and the documented
    // stop -> configure -> start lifecycle without introducing another runtime or private API.
    RemoteAccess {
        id: remote
        enabled: true
        target: window
        port: acceptancePort
        remoteInputEnabled: window.runtimeRemoteInput

        onStateChanged: {
            if (state === RemoteAccess.Running)
                console.log("READY " + port)
        }
        onConnectedClientCountChanged:
            console.log("CLIENT_COUNT " + connectedClientCount)
        onErrorChanged: {
            if (errorString.length > 0)
                console.log("REMOTE_ERROR " + errorString)
        }
    }

    Column {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12

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
            text: window.runtimeRemoteInput ? "Remote control: enabled explicitly"
                                            : "Remote control: view-only default"
            color: "#f2f4f8"
        }

        Label {
            text: "State: " + remote.state + " · clients: " + remote.connectedClientCount
                  + " · 127.0.0.1:" + acceptancePort
            color: "#c6ceda"
        }
    }

    Component.onCompleted: {
        editor.forceActiveFocus()
        if (!remote.enabled && remote.errorString.length > 0) {
            console.log("START_FAILED " + remote.errorString)
            Qt.exit(2)
            return
        }
        console.log("CLIENT_COUNT " + remote.connectedClientCount)
    }

    Timer {
        interval: acceptancePolicyTransitionMs
        running: acceptancePolicyTransitionMs > 0
        repeat: false
        onTriggered: {
            remote.enabled = false
            console.log("POLICY_STOPPED")
            window.runtimeRemoteInput = true
            console.log("POLICY_INPUT " + remote.remoteInputEnabled)
            remote.enabled = true
            console.log("POLICY_RESTART_REQUESTED")
        }
    }

    Timer {
        interval: acceptanceTimeoutMs
        running: acceptanceTimeoutMs > 0
        repeat: false
        onTriggered: {
            remote.enabled = false
            console.log("CLIENT_COUNT " + remote.connectedClientCount)
            console.log("STOPPED")
            Qt.quit()
        }
    }
}
