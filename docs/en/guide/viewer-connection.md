# Viewer Connection and Remote Control

> Language / 语言: **English** | [中文](../../guide/viewer-connection.md)

HyRemote uses one Shared Runtime and one RFB transport baseline behind all four integration frontends. The viewer does not need to know whether the application entered through C++, QML, Generic, or QPA.

## Start the target application

### C++ API

The application explicitly calls:

```cpp
remote.start();
```

### Generic Plugin

Keep the application Qt-only and activate HyRemote through Qt's generic-plugin mechanism:

```text
MyApp -plugin hyremote
```

### QML API (Preview)

```qml
RemoteAccess {
    target: mainWindow
    enabled: true
}
```

### QPA (Preview)

```text
MyApp -platform hyremote
```

All four routes converge on the same Shared Runtime, so the default network and input policy is shared.

## Default connection address

V0.1 listens on:

```text
127.0.0.1:5921
```

Point a standard VNC/RFB viewer at that address.

HyRemote currently uses numeric IP addresses. Binding outside loopback expands the network trust boundary; read [`../../security.md`](../../security.md) before changing it.

See [`../../known-limitations.md`](../../known-limitations.md) for address-family and platform-specific behavior.

## View-only and remote control

Remote viewing and remote input are separate capabilities. The default is **view-only**.

### C++ API

```cpp
remote.setRemoteInputEnabled(true);
remote.start();
```

### QML API

```qml
RemoteAccess {
    target: mainWindow
    remoteInputEnabled: true
    enabled: true
}
```

### Generic Plugin

Enable input through the Generic Plugin specification, for example:

```text
MyApp -plugin "hyremote:input=true"
```

Multiple fields belong after the colon in the plugin specification and are separated with semicolons; see [`../getting-started/generic.md`](../getting-started/generic.md).

### QPA

```text
MyApp -platform "hyremote:hyremote-input=true"
```

Keep the default view-only mode when remote control is not required.

## Disconnect and reconnect

An ordinary viewer disconnect does not require restarting the Qt application.

Typical lifecycle:

```text
application running
    ↓
viewer connects
    ↓
viewer disconnects
    ↓
application keeps running
    ↓
viewer reconnects
```

The Shared Runtime owns remote-client lifetime while the Qt application and local UI continue independently.

For C++/QML, an explicit `stop()`/disable closes the Runtime and listener. Generic/QPA follow their plugin lifecycle.

## “Running” is not “connected”

`RemoteAccessState::Running` means the Runtime/listener is active. It does not mean a viewer is connected or authenticated.

C++/QML can observe current connection count through:

```text
connectedClientCount()
```

This value is operational state, not user identity, role, or authorization.

Generic/QPA do not add a second business API to an otherwise zero-code application merely to expose this value.

## Multi-window applications

Supported application top-level surfaces may appear and disappear within one logical remote session rather than creating a listener per window.

This does not imply automatic support for arbitrary native/foreign OS windows. See [`../../compatibility.md`](../../compatibility.md) for the current complex-window/graphics scope.

## Viewer interoperability

HyRemote currently uses standard RFB/VNC as its transport baseline.

Viewer behavior can differ for shortcuts, clipboard, scaling, and input methods. Only viewer/version combinations explicitly listed in the compatibility matrix form a formal interoperability claim.

> **TODO V0.3/V0.4:** complete the formal viewer interoperability matrix and cover at least two maintained real VNC viewers.

## Local + remote coexistence

HyRemote's product goal is to add remote access without breaking native Qt display or local input.

Generic explicitly preserves the native Qt platform. QPA delegates to the native platform integration through the Factory Trampoline.

Headless/offscreen execution alone cannot qualify every physical display/keyboard/mouse combination.

> **TODO V0.4:** complete physical Windows/Linux local-display + local-input + remote-access qualification for the final compatibility matrix.

## Security boundary

V0.1:

- default bind is loopback;
- remote input is disabled by default;
- `Insecure` is unauthenticated and unencrypted: the listener is for a **trusted LAN only** and is **not Internet-safe**;
- `Authenticated` is available only when HyRemote was built with the transport-security capability and a valid security descriptor is configured; it currently provides VNC authentication without stream encryption;
- the default V0.1 build/profile does not imply authenticated transport is present;
- `AuthenticatedEncrypted` is not implemented and always fails closed before listener creation, without falling back to a weaker profile.

Do not expose the current product directly to the public Internet. See [`../../security.md`](../../security.md).

## Related documentation

- C++ API: [`../getting-started/cpp.md`](../getting-started/cpp.md)
- Generic Plugin: [`../getting-started/generic.md`](../getting-started/generic.md)
- QML API: [`../getting-started/qml.md`](../getting-started/qml.md)
- QPA: [`../getting-started/qpa-proxy.md`](../getting-started/qpa-proxy.md)
- deployment: [`deployment.md`](deployment.md)
- security: [`../../security.md`](../../security.md)
- compatibility: [`../../compatibility.md`](../../compatibility.md)
- known limitations: [`../../known-limitations.md`](../../known-limitations.md)