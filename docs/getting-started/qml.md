# Declarative QML Getting Started

Status: **V0.0.2.0 candidate user guide; executable Windows/Linux acceptance pending #74.**

HyRemote's Declarative QML mode is a thin QML surface over the same `HyRemote::RemoteAccess` runtime used by Embedded C++. It does not create a second Session, capture path, transport or input implementation.

## Prerequisites

The V1 reference line is Qt 6.8.3 on Windows x86_64 and Linux x86_64. Until the exact reference jobs execute successfully, treat these rows as candidate configurations rather than released support claims.

For an installed SDK:

```cmake
find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Core Gui Qml Quick)
find_package(HyRemote CONFIG REQUIRED)
```

For source consumption, follow `../source-consumption.md`; the application still imports the same `HyRemote` QML module and uses the same runtime semantics.

## QML usage

A normal application can declare the remote-access object directly:

```qml
import QtQuick
import QtQuick.Controls
import HyRemote

ApplicationWindow {
    id: window
    width: 800
    height: 600
    visible: true

    RemoteAccess {
        id: remote
        target: window
        listenAddress: "127.0.0.1"
        port: 5900
        remoteInputEnabled: false
        enabled: false
    }

    Component.onCompleted: remote.enabled = true
}
```

Construction is inert: declaring `RemoteAccess` does not implicitly open a listener. The example starts it explicitly after the application window is ready.

Safe defaults remain the same as Embedded C++:

- loopback listener by default;
- remote input disabled by default;
- explicit start/enable required;
- the current RFB baseline uses SecurityType None and is not Internet-safe authentication/encryption.

## Lifecycle and configuration

The declarative wrapper preserves the C++ facade rules rather than hiding them:

- `enabled: true` requests start; `enabled: false` stops;
- failed start is transactional and does not leave QML claiming the runtime is enabled;
- listener address, port, target and remote-input policy are configuration and should be changed while Stopped;
- changing live configuration does not silently create a second runtime or implicit restart;
- state/error values remain product-level rather than protocol/backend objects.

## Connection status

`Running` means the remote runtime/listener is active. It does **not** mean a viewer is connected.

The QML type exposes the same read-only backend-neutral connection diagnostic as the C++ facade:

```qml
Label {
    text: remote.state === RemoteAccess.Running
          ? "Listening · clients " + remote.connectedClientCount
          : "Stopped"
}
```

`connectedClientCount` is not assignable from QML. E3 product-fit pins the real viewer lifecycle as `0 → 1 → 0 → 1 → 0` across connect, disconnect and reconnect without recreating the application. That repository evidence still requires successful reference-platform execution before it becomes a release support claim.

## View-only and remote control

Keep `remoteInputEnabled: false` for view-only operation. To opt into remote control, stop the runtime, change the policy, then start it again according to the shared configuration contract.

Remote pointer, key and committed-text events use the same normalized input path as the C++ mode. Application focus remains normal Qt focus; HyRemote does not expose RFB-specific key or socket types to QML.

## Connect and reconnect a viewer

With the default endpoint, connect a standard RFB/VNC viewer to `127.0.0.1:5900`.

Close the viewer and reconnect without restarting the Qt application. The listener remains the same runtime while the read-only connected-client count returns to zero between clients.

See `../viewer-connection.md` for viewer behavior and the security boundary.

## Deployment

Install the application normally, then use the single HyRemote deployment entry point in QML mode:

```cmake
install(TARGETS MyQmlApp
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

hyremote_deploy(TARGET MyQmlApp QML)
```

The installed package exports its QML import root and `hyremote_deploy(... QML)` bridges that root into Qt's supported QML deployment machinery. Application developers do not name or manually copy HyRemote's QML plugin/qmldir or transport implementation files.

`QML QPA` is also a valid deployment-option combination for an application that intentionally deploys both package payloads, but it does not merge the Declarative QML and Transparent QPA **integration semantics** into one mode. Choose the integration mode appropriate to the application rather than adding QPA merely to make QML work.

For the full packaging contract, see `../qml-consumption.md` and `../deployment.md`.

## Security

The current correctness baseline negotiates RFB SecurityType None. Do not expose it directly to an untrusted/public network. View-only is an input policy, not viewer authentication, and the stream is not encrypted by this baseline.

See `../security.md` for the implemented product boundary.

## Local + remote coexistence evidence

HyRemote's product requirement is that the local Qt application remains visible and interactive while remote access is active. Hosted offscreen/software E2E verifies the viewer-to-QML product path but is not proof of a physical local monitor/keyboard/mouse remaining usable at the same time.

Final physical local + remote coexistence remains part of #31/#33 release acceptance and must be recorded separately when the reference environment is available.

## Related documentation

- Installed QML package/deployment details: `../qml-consumption.md`
- Windows setup: `windows.md`
- Linux setup: `linux.md`
- Deployment: `../deployment.md`
- Viewer connection: `../viewer-connection.md`
- Security: `../security.md`
- Troubleshooting: `../troubleshooting.md`
- Compatibility: `../compatibility.md`
- Known limitations: `../known-limitations.md`

Governance mode: `transitional-explicit`.
