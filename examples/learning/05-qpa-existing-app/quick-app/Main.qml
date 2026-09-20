import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {
    id: root
    width: 760
    height: 460
    visible: true
    title: "Qt Quick Existing App - no HyRemote application API"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 32
        spacing: 18

        Label {
            text: "Ordinary Qt Quick application"
            font.pixelSize: 26
            font.bold: true
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: "This application does not import HyRemote and does not link HyRemote::RemoteAccess. Run it normally first, then launch the same binary with -platform hyremote to verify Transparent QPA integration."
        }

        RowLayout {
            Label { text: "Interactive value:" }
            Slider {
                id: slider
                Layout.fillWidth: true
                from: 0
                to: 100
                value: 35
            }
            Label { text: Math.round(slider.value).toString() }
        }

        TextField {
            Layout.fillWidth: true
            placeholderText: "Local/remote keyboard and committed-text probe"
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 12
            color: slider.pressed ? "#d7ebff" : "#f1f5f9"

            Text {
                anchors.centerIn: parent
                text: "Resize, focus, pointer, keyboard and dynamic Quick rendering"
                color: "#334155"
            }
        }
    }
}
