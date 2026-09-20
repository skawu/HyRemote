import QtQuick 2.15
import QtQuick.Window 2.15
import HyRemote

Window {
    id: window
    width: 1040
    height: 700
    minimumWidth: 900
    minimumHeight: 620
    visible: true
    color: "#0b1220"
    title: "HyRemote - Production Showcase"

    property bool hasRun: false
    property int localCommandCount: 0

    function stateText(state) {
        if (state === RemoteAccess.Stopped) return "Stopped"
        if (state === RemoteAccess.Starting) return "Starting"
        if (state === RemoteAccess.Running) return "Running"
        if (state === RemoteAccess.Stopping) return "Stopping"
        if (state === RemoteAccess.Faulted) return "Faulted"
        return "Unknown"
    }

    function securityText(profile) {
        if (profile === RemoteAccess.Authenticated) return "Authenticated"
        if (profile === RemoteAccess.AuthenticatedEncrypted) return "Authenticated + TLS"
        return "Insecure / loopback only"
    }

    RemoteAccess {
        id: remote
        target: window
        port: showcasePort
        remoteInputEnabled: showcaseRemoteInput
        securityProfile: RemoteAccess.Insecure
        enabled: false

        onStateChanged: {
            if (state === RemoteAccess.Running) {
                window.hasRun = true
                console.log("REMOTE_STARTED " + port)
            } else if (state === RemoteAccess.Stopped && window.hasRun) {
                console.log("REMOTE_STOPPED")
            }
        }
        onConnectedClientCountChanged:
            console.log("SHOWCASE_CLIENTS " + connectedClientCount)
        onRemoteInputEnabledChanged:
            console.log("REMOTE_INPUT " + (remoteInputEnabled ? "enabled" : "disabled"))
    }

    Component.onCompleted: {
        console.log("SHOWCASE_CLIENTS " + remote.connectedClientCount)
        if (showcaseAutoStart)
            remote.enabled = true
    }

    Timer {
        interval: showcaseTimeoutMs
        running: showcaseTimeoutMs > 0
        repeat: false
        onTriggered: Qt.quit()
    }

    Rectangle {
        anchors.fill: parent
        color: "#0b1220"

        Rectangle {
            id: topBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 82
            color: "#111c2e"

            Image {
                id: brandLogo
                anchors.left: parent.left
                anchors.leftMargin: 28
                anchors.verticalCenter: parent.verticalCenter
                width: 46
                height: 46
                source: "qrc:/hyremote/branding/logo.png"
                fillMode: Image.PreserveAspectFit
                smooth: true
            }

            Text {
                anchors.left: brandLogo.right
                anchors.leftMargin: 16
                anchors.top: parent.top
                anchors.topMargin: 18
                text: "HyRemote"
                color: "#f8fafc"
                font.pixelSize: 24
                font.bold: true
            }

            Text {
                anchors.left: brandLogo.right
                anchors.leftMargin: 16
                anchors.top: parent.top
                anchors.topMargin: 49
                text: "Low-intrusion remote access for Qt applications"
                color: "#8ea3bf"
                font.pixelSize: 13
            }

            Rectangle {
                anchors.right: parent.right
                anchors.rightMargin: 28
                anchors.verticalCenter: parent.verticalCenter
                width: 154
                height: 36
                radius: 18
                color: remote.state === RemoteAccess.Running ? "#123b31" :
                       remote.state === RemoteAccess.Faulted ? "#4a1d27" : "#1a2940"
                border.width: 1
                border.color: remote.state === RemoteAccess.Running ? "#28c48a" :
                              remote.state === RemoteAccess.Faulted ? "#ff6b7a" : "#405572"

                Rectangle {
                    width: 8
                    height: 8
                    radius: 4
                    anchors.left: parent.left
                    anchors.leftMargin: 15
                    anchors.verticalCenter: parent.verticalCenter
                    color: remote.state === RemoteAccess.Running ? "#34d399" :
                           remote.state === RemoteAccess.Faulted ? "#fb7185" : "#7890ad"
                }
                Text {
                    anchors.centerIn: parent
                    anchors.horizontalCenterOffset: 8
                    text: window.stateText(remote.state)
                    color: "#e6edf7"
                    font.pixelSize: 13
                    font.bold: true
                }
            }
        }

        Item {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: topBar.bottom
            anchors.bottom: parent.bottom
            anchors.margins: 24

            Rectangle {
                id: remoteCard
                anchors.left: parent.left
                anchors.top: parent.top
                width: parent.width * 0.47
                height: parent.height
                radius: 12
                color: "#111c2e"
                border.color: "#223550"
                border.width: 1

                Text {
                    id: remoteTitle
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.margins: 22
                    text: "Remote access"
                    color: "#f8fafc"
                    font.pixelSize: 20
                    font.bold: true
                }
                Text {
                    anchors.left: remoteTitle.left
                    anchors.top: remoteTitle.bottom
                    anchors.topMargin: 6
                    text: "One runtime · C++ / QML / QPA"
                    color: "#7f96b4"
                    font.pixelSize: 12
                }

                Rectangle {
                    id: endpointBox
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: remoteTitle.bottom
                    anchors.leftMargin: 22
                    anchors.rightMargin: 22
                    anchors.topMargin: 38
                    height: 92
                    radius: 8
                    color: "#0d1727"

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.top: parent.top
                        anchors.topMargin: 14
                        text: "LISTENER"
                        color: "#607895"
                        font.pixelSize: 10
                        font.bold: true
                    }
                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.top: parent.top
                        anchors.topMargin: 36
                        text: remote.listenAddress + ":" + remote.port
                        color: "#dce8f7"
                        font.pixelSize: 17
                        font.family: "monospace"
                    }
                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: 16
                        anchors.top: parent.top
                        anchors.topMargin: 18
                        text: remote.connectedClientCount + " viewer" +
                              (remote.connectedClientCount === 1 ? "" : "s")
                        color: remote.connectedClientCount > 0 ? "#34d399" : "#8ea3bf"
                        font.pixelSize: 13
                    }
                }

                Text {
                    id: securityLabel
                    anchors.left: parent.left
                    anchors.leftMargin: 22
                    anchors.top: endpointBox.bottom
                    anchors.topMargin: 24
                    text: "Security profile"
                    color: "#93a8c3"
                    font.pixelSize: 12
                }

                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: 22
                    anchors.top: securityLabel.bottom
                    anchors.topMargin: 10
                    spacing: 8

                    Repeater {
                        model: ["Loopback", "Auth", "TLS + Auth"]
                        delegate: Rectangle {
                            width: 114
                            height: 38
                            radius: 7
                            color: remote.securityProfile === index ? "#1f5b8f" : "#17263a"
                            border.width: 1
                            border.color: remote.securityProfile === index ? "#58a9e8" : "#2a405e"
                            opacity: remote.state === RemoteAccess.Stopped ? 1.0 : 0.55

                            Text {
                                anchors.centerIn: parent
                                text: modelData
                                color: "#edf6ff"
                                font.pixelSize: 12
                            }
                            MouseArea {
                                anchors.fill: parent
                                enabled: remote.state === RemoteAccess.Stopped
                                onClicked: remote.securityProfile = index
                            }
                        }
                    }
                }

                Text {
                    id: descriptorLabel
                    anchors.left: parent.left
                    anchors.leftMargin: 22
                    anchors.top: securityLabel.bottom
                    anchors.topMargin: 62
                    text: "Security descriptor"
                    color: "#93a8c3"
                    font.pixelSize: 12
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 22
                    anchors.rightMargin: 22
                    anchors.top: descriptorLabel.bottom
                    anchors.topMargin: 8
                    height: 42
                    radius: 7
                    color: "#0d1727"
                    border.width: 1
                    border.color: descriptor.activeFocus ? "#4c9bd4" : "#263a55"

                    TextInput {
                        id: descriptor
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        verticalAlignment: TextInput.AlignVCenter
                        color: "#e5edf7"
                        selectionColor: "#3276a8"
                        font.pixelSize: 12
                        enabled: remote.state === RemoteAccess.Stopped
                        text: remote.securityConfigFile
                        onEditingFinished: remote.securityConfigFile = text
                        Text {
                            visible: descriptor.text.length === 0
                            anchors.verticalCenter: parent.verticalCenter
                            text: "optional for loopback; required by secure profiles"
                            color: "#536b88"
                            font.pixelSize: 11
                        }
                    }
                }

                Row {
                    id: policyRow
                    anchors.left: parent.left
                    anchors.leftMargin: 22
                    anchors.top: descriptorLabel.bottom
                    anchors.topMargin: 72
                    spacing: 12

                    Rectangle {
                        width: 48
                        height: 26
                        radius: 13
                        color: remote.remoteInputEnabled ? "#157a62" : "#263a55"
                        opacity: remote.state === RemoteAccess.Stopped ? 1.0 : 0.65
                        Rectangle {
                            width: 20
                            height: 20
                            radius: 10
                            y: 3
                            x: remote.remoteInputEnabled ? 25 : 3
                            color: "#f8fafc"
                            Behavior on x { NumberAnimation { duration: 110 } }
                        }
                        MouseArea {
                            anchors.fill: parent
                            enabled: remote.state === RemoteAccess.Stopped
                            onClicked: remote.remoteInputEnabled = !remote.remoteInputEnabled
                        }
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: remote.remoteInputEnabled ? "Remote control enabled" : "View-only safe default"
                        color: "#d8e3f0"
                        font.pixelSize: 13
                    }
                }

                Rectangle {
                    id: startButton
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 22
                    anchors.rightMargin: 22
                    anchors.bottom: errorBox.visible ? errorBox.top : parent.bottom
                    anchors.bottomMargin: errorBox.visible ? 12 : 22
                    height: 48
                    radius: 8
                    color: remote.enabled ? "#7a2f43" : "#1773a8"

                    Text {
                        anchors.centerIn: parent
                        text: remote.enabled ? "Stop remote access" : "Start remote access"
                        color: "white"
                        font.pixelSize: 14
                        font.bold: true
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: remote.enabled = !remote.enabled
                    }
                }

                Rectangle {
                    id: errorBox
                    visible: remote.errorString.length > 0
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 22
                    anchors.rightMargin: 22
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 22
                    height: visible ? 56 : 0
                    radius: 7
                    color: "#3b1b25"
                    border.color: "#763348"
                    Text {
                        anchors.fill: parent
                        anchors.margins: 10
                        text: remote.errorString
                        color: "#ffc6d0"
                        wrapMode: Text.WordWrap
                        font.pixelSize: 11
                    }
                }
            }

            Rectangle {
                id: localCard
                anchors.right: parent.right
                anchors.top: parent.top
                width: parent.width * 0.50
                height: parent.height
                radius: 12
                color: "#111c2e"
                border.color: "#223550"
                border.width: 1

                Text {
                    id: localTitle
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.margins: 22
                    text: "Local application"
                    color: "#f8fafc"
                    font.pixelSize: 20
                    font.bold: true
                }
                Text {
                    anchors.left: localTitle.left
                    anchors.top: localTitle.bottom
                    anchors.topMargin: 6
                    text: "Local display and input stay authoritative while remote access is active"
                    color: "#7f96b4"
                    font.pixelSize: 12
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: localTitle.bottom
                    anchors.leftMargin: 22
                    anchors.rightMargin: 22
                    anchors.topMargin: 42
                    height: 172
                    radius: 9
                    color: "#0d1727"

                    Text {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.margins: 16
                        text: "Conveyor-01"
                        color: "#dce8f7"
                        font.pixelSize: 18
                        font.bold: true
                    }
                    Text {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 16
                        text: "RUNNING"
                        color: "#34d399"
                        font.pixelSize: 11
                        font.bold: true
                    }
                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.top: parent.top
                        anchors.topMargin: 58
                        text: "Process load"
                        color: "#738ba8"
                        font.pixelSize: 11
                    }
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        anchors.top: parent.top
                        anchors.topMargin: 86
                        height: 12
                        radius: 6
                        color: "#1e3149"
                        Rectangle {
                            width: parent.width * 0.68
                            height: parent.height
                            radius: parent.radius
                            color: "#2d8cc4"
                        }
                    }
                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 17
                        text: "68% nominal load · local operator online"
                        color: "#9eb2ca"
                        font.pixelSize: 12
                    }
                }

                Text {
                    id: notesLabel
                    anchors.left: parent.left
                    anchors.leftMargin: 22
                    anchors.top: parent.top
                    anchors.topMargin: 276
                    text: "Operator note"
                    color: "#93a8c3"
                    font.pixelSize: 12
                }
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 22
                    anchors.rightMargin: 22
                    anchors.top: notesLabel.bottom
                    anchors.topMargin: 8
                    height: 94
                    radius: 8
                    color: "#0d1727"
                    border.width: 1
                    border.color: operatorNote.activeFocus ? "#4c9bd4" : "#263a55"
                    TextInput {
                        id: operatorNote
                        anchors.fill: parent
                        anchors.margins: 12
                        color: "#e5edf7"
                        selectionColor: "#3276a8"
                        wrapMode: TextInput.Wrap
                        text: "Inspection complete. Local controls remain responsive."
                    }
                }

                Rectangle {
                    id: localCommand
                    anchors.left: parent.left
                    anchors.leftMargin: 22
                    anchors.top: parent.top
                    anchors.topMargin: 420
                    width: 190
                    height: 44
                    radius: 8
                    color: "#1b6b55"
                    Text {
                        anchors.centerIn: parent
                        text: "Run local command"
                        color: "white"
                        font.pixelSize: 13
                        font.bold: true
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: window.localCommandCount += 1
                    }
                }
                Text {
                    anchors.left: localCommand.right
                    anchors.leftMargin: 14
                    anchors.verticalCenter: localCommand.verticalCenter
                    text: window.localCommandCount + " local command" +
                          (window.localCommandCount === 1 ? "" : "s")
                    color: "#8ea3bf"
                    font.pixelSize: 12
                }

                Text {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 22
                    anchors.rightMargin: 22
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 22
                    text: "This showcase uses the same HyRemote QML API as 04-quick-qml. The richer UI is a product demonstration, not a second runtime."
                    color: "#647d9b"
                    wrapMode: Text.WordWrap
                    font.pixelSize: 11
                }
            }
        }
    }

    Item {
        id: acceptanceInputSink
        anchors.fill: parent
        z: 1000
        focus: showcaseAcceptanceMode
        enabled: showcaseAcceptanceMode

        Keys.onPressed: function(event) {
            console.log("SHOWCASE_KEY key=" + event.key)
            event.accepted = true
        }

        MouseArea {
            anchors.fill: parent
            enabled: showcaseAcceptanceMode
            acceptedButtons: Qt.AllButtons
            onPressed: function(mouse) {
                acceptanceInputSink.forceActiveFocus()
                console.log("SHOWCASE_POINTER button=" + mouse.button +
                            " x=" + mouse.x + " y=" + mouse.y)
            }
        }
    }
}
