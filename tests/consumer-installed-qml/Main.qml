// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Window
import HyRemote

Window {
    id: root
    width: 96
    height: 64
    visible: true

    // The clean consumer exercises the installed declarative type without starting a second remote
    // runtime. In the combined QML+QPA configuration the transparent platform plugin owns remote
    // access while this same QML module remains loadable from the deployed tree.
    RemoteAccess {
        id: remote
        target: root
        enabled: false
    }

    readonly property bool contractOk:
        !remote.enabled && remote.state === RemoteAccess.Stopped && remote.target === root
}
