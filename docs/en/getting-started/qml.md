# QML API Getting Started 

> Language / 语言: **English** | [中文](../../getting-started/qml.md)

The QML API is HyRemote's declarative integration path for Qt Quick applications. It is a thin frontend over the Shared Runtime and does not create a second Session, capture, transport, input, or security implementation.

Current product status: peer route.

## Who this is for

Choose the QML API when your application:

- is primarily Qt Quick / QML;
- prefers declarative properties for remote-access policy;
- does not want extra C++ glue for basic lifecycle control.

A Qt Quick application can also use the C++ API directly; see [`cpp.md`](cpp.md).

## Current reference environment

Current reference environment:

- Windows x86_64;
- Linux x86_64;
- Qt 6.8.3.

The QML API is currently provided as Preview. See [`../../compatibility.md`](../../compatibility.md) for the exact compatibility statement.

> **TODO V0.3:** complete installed-SDK, deployment, bilingual examples, and product qualification before promoting QML API to a formal product path.

## Minimal integration

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick)
find_package(HyRemote CONFIG REQUIRED)
```

```qml
import QtQuick
import QtQuick.Controls
import HyRemote

ApplicationWindow {
    id: window
    visible: true

    RemoteAccess {
        id: remote
        target: window
        enabled: true
    }
}
```

`enabled: true` is an explicit start request. HyRemote starts the Shared Runtime after the QML component is complete, so basic startup does not require `Component.onCompleted` glue.

Defaults match the C++ API:

- listener `0.0.0.0:5921`;
- remote input disabled;
- Runtime stays stopped until explicitly enabled;
- security behavior comes from the same Shared Runtime.

## Common configuration

```qml
RemoteAccess {
    id: remote
    target: window
    port: 5901
    remoteInputEnabled: true
    enabled: true
}
```

Runtime configuration follows an explicit lifecycle:

```text
disable -> change configuration -> enable
```

The QML frontend does not create a hidden second Runtime and does not expose RFB/socket-specific objects.

## State and connection count

`Running` means the Runtime/listener is active. It does not mean a viewer is connected or authenticated.

```qml
Label {
    text: remote.state === RemoteAccess.Running
          ? "Listening · clients " + remote.connectedClientCount
          : "Stopped"
}
```

`connectedClientCount` is operational state, not identity or authorization data.

## View-only and remote control

The default is view-only. Enable remote input explicitly when required:

```qml
RemoteAccess {
    target: window
    remoteInputEnabled: true
    enabled: true
}
```

Remote pointer, key, and text input follow the same normalized input semantics used by the other frontends. Qt focus and control state remain authoritative.

## Deployment

```cmake
install(TARGETS MyQmlApp
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

hyremote_deploy(TARGET MyQmlApp QML)
```

The QML payload reuses the same Shared Runtime as the C++ API. Applications should not copy `qmldir`, internal plugins, or Runtime libraries manually.

If the selected HyRemote SDK does not contain the QML payload, configuration should fail instead of producing an incomplete deployment.

See [`../guide/deployment.md`](../guide/deployment.md).

## Combining with QPA

When the application genuinely needs QPA platform-entry behavior, deploy both payloads:

```cmake
hyremote_deploy(TARGET MyQmlApp QML QPA)
```

This is still one Shared Runtime used by two frontend payloads, not a fourth Runtime architecture.

## Security boundary

The shipped Shared Runtime is LAN-first:

- default bind is `0.0.0.0` (every IPv4 interface), or one exact local IPv4, or a named network interface;
- remote input is disabled by default;
- `Insecure` is unauthenticated and unencrypted: it is for a **trusted LAN only** and is not Internet-safe;
- `Authenticated` may use RFB VNC authentication, while the stream remains unencrypted;
- `AuthenticatedEncrypted` fails closed while the encrypted backend is unavailable.

Do not expose the current product directly to the public Internet. See [`../../security.md`](../../security.md).

## Examples

The new Example curriculum provides a dedicated Quick + QML learning path and keeps it distinct from Quick + C++.

> **TODO V0.3:** complete the bilingual, branded `examples/learning/04-quick-qml` product example.

## Next steps

- Quick + C++: [`cpp.md`](cpp.md);
- Generic zero-code integration: [`generic.md`](generic.md);
- QPA : [`qpa-proxy.md`](qpa-proxy.md);
- deployment: [`../guide/deployment.md`](../guide/deployment.md);
- viewer workflow: [`../guide/viewer-connection.md`](../guide/viewer-connection.md);
- security: [`../../security.md`](../../security.md);
- compatibility: [`../../compatibility.md`](../../compatibility.md);
- known limitations: [`../../known-limitations.md`](../../known-limitations.md).