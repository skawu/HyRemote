# Declarative QML Getting Started

> Language / 语言: **English** | [中文](../../getting-started/qml.md)

HyRemote's Declarative QML mode is a thin declarative surface over the same shared `HyRemote::RemoteAccess` runtime used by C++. It does not create a second Session, capture, transport or input stack.

## Prerequisites

The V1 reference line is Qt 6.8.3 on Windows x86_64 and Linux x86_64.

A QML application resolves its normal Qt modules and then the HyRemote package:

```cmake
find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Core Gui Qml Quick)
find_package(HyRemote CONFIG REQUIRED)
```

`find_package(HyRemote)` itself does not force unrelated Widgets/Quick/QML development components onto a plain C++ consumer; the application chooses the Qt UI stack it already uses.

## Minimal QML use

Normal declarative startup is intentionally two properties:

```qml
import QtQuick
import QtQuick.Controls
import HyRemote

ApplicationWindow {
    id: window
    visible: true

    RemoteAccess {
        target: window
        enabled: true
    }
}
```

`enabled: true` is a request, not constructor side effect. HyRemote waits until the QML component is complete before starting the shared runtime, so initial property declaration order does not require `Component.onCompleted` glue. If start fails, `enabled` rolls back to `false` and the product-level error properties describe the failure.

Safe defaults match C++:

- loopback listener;
- port 5921;
- remote input disabled;
- explicit `enabled: true` required;
- current RFB correctness baseline uses SecurityType None and is unauthenticated/unencrypted.

## Optional configuration

Only set values you actually need to change:

```qml
RemoteAccess {
    target: window
    port: 5901
    remoteInputEnabled: true
    enabled: true
}
```

Configuration belongs to the stopped state. To change listener/input policy while running, disable first, update the properties, then enable again. HyRemote does not silently create a second runtime or hidden restart path.

## Connection status

`Running` means the listener/runtime is active; it does not mean a viewer is connected.

```qml
Label {
    text: remote.state === RemoteAccess.Running
          ? "Listening · clients " + remote.connectedClientCount
          : "Stopped"
}
```

`connectedClientCount` is read-only and backend-neutral. E3 product-fit requires the real viewer lifecycle `0 → 1 → 0 → 1 → 0` across connect, disconnect and reconnect without recreating the application.

## View-only and remote control

The default is view-only. Opt into remote control only when intended:

```qml
RemoteAccess {
    target: window
    remoteInputEnabled: true
    enabled: true
}
```

Remote pointer, key and committed-text events use the same normalized input path as C++. Normal Qt focus remains authoritative; no RFB-specific key/socket object enters QML.

## Deployment

Install the application normally and use the one HyRemote helper:

```cmake
install(TARGETS MyQmlApp
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

hyremote_deploy(TARGET MyQmlApp QML)
```

The QML module is an import payload, not a second C++ SDK target. Application developers do not link a `HyRemote::Qml` target or manually copy the backing library, plugin, `qmldir`, shared facade, or transport files.

`QML QPA` is available when an application deliberately combines declarative API use with Transparent QPA packaging:

```cmake
hyremote_deploy(TARGET MyQmlApp QML QPA)
```

This still reuses one shared runtime; it does not create a fourth integration architecture.

## Viewer workflow

With defaults, connect a standard RFB/VNC viewer to `127.0.0.1:5921`. Close the viewer and reconnect without restarting the Qt application. The listener remains active and `connectedClientCount` returns to zero between clients.

See [`../../guide/viewer-connection.md`](../../guide/viewer-connection.md) for viewer behavior.

## Security boundary

The current correctness baseline negotiates RFB SecurityType None. Do not expose it directly to an untrusted/public network. View-only is an input policy, not authentication, and the stream is not encrypted.

See `../security.md`.

## Local + remote coexistence

Hosted offscreen/software E2E proves the viewer-to-QML product path but does not prove a physical monitor and local keyboard/mouse remain usable at the same time. Cross-mode physical coexistence is a separate question from standard-viewer interoperability.

## Related documentation

- deployment: [`../../guide/deployment.md`](../../guide/deployment.md)
- Windows / Linux platform setup: [`../../guide/install.md`](../../guide/install.md) and [`../guide/install.md`](../guide/install.md)
- viewer connection: [`../../guide/viewer-connection.md`](../../guide/viewer-connection.md)
- security: [`../../security.md`](../../security.md)
- troubleshooting: [`../../guide/troubleshooting.md`](../../guide/troubleshooting.md)
- compatibility: [`../../compatibility.md`](../../compatibility.md)
- known limitations: [`../../known-limitations.md`](../../known-limitations.md)
