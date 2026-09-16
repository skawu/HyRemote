import QtQuick
import HyRemote

Item {
    width: 64
    height: 64

    RemoteAccess {
        id: remote
        enabled: false
    }

    Component.onCompleted: {
        if (remote.enabled || remote.state !== RemoteAccess.Stopped)
            Qt.exit(3)
        Qt.quit()
    }
}
