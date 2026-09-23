# C++ API Getting Started (Qt Widgets / Qt Quick)

> Language / 语言: **English** | [中文](../../getting-started/cpp.md)

The C++ API is one of the primary HyRemote V0.1 integration paths. An application links the public `HyRemote::RemoteAccess` target to add remote viewing and optional remote control to a Qt Widgets or Qt Quick top-level window.

Applications do not need to understand Core, Session, RFB, capture backends, or input backends.

## Who this is for

Choose the C++ API when your application:

- can add a small amount of C++ integration code;
- wants explicit start/stop, target, listener, or input-policy control;
- uses Qt Widgets, Qt Quick, or a mix of both;
- may later need richer programmable policy.

If you want a **zero-code application integration**, start with [`generic.md`](generic.md).


> The listener is intended for a **trusted LAN only** and is **not Internet-safe**. The security state actually
> shipped is what the runtime reports.

## Current reference environment

V0.1 currently references:

- Windows x86_64;
- Linux x86_64;
- Qt 6.8.3.

See [`../../compatibility.md`](../../compatibility.md) for the exact status of other Qt versions.

> **TODO V0.4:** qualify planned compatibility lines such as Qt 5.15 LTS before broadening the support statement.

## Minimal Qt Widgets integration

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)
find_package(HyRemote CONFIG REQUIRED)

target_link_libraries(MyApp PRIVATE
    Qt6::Widgets
    HyRemote::RemoteAccess
)
```

```cpp
#include <HyRemote/RemoteAccess.h>

MainWindow window;
window.show();

HyRemote::RemoteAccess remote(&window);
remote.start();
```

Default behavior:

- constructing `RemoteAccess` does not open a listener;
- `start()` explicitly starts the Runtime;
- the default listener is `0.0.0.0:5921`, reachable on the host's IPv4 interfaces;
- remote input is disabled by default.

## Minimal Qt Quick integration

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Quick)
find_package(HyRemote CONFIG REQUIRED)

target_link_libraries(MyApp PRIVATE
    Qt6::Quick
    HyRemote::RemoteAccess
)
```

```cpp
QQuickWindow *window = /* top-level Quick window */;
HyRemote::RemoteAccess remote(window);
remote.start();
```

Widgets and Qt Quick use the same public API and the same Shared Runtime. There is no separate product Runtime for each UI family.

## Enable remote control

The default mode is view-only. Enable remote input only when the application actually needs it:

```cpp
HyRemote::RemoteAccess remote(&window);
remote.setRemoteInputEnabled(true);
remote.start();
```

Remote input is routed to the attached Qt application target. It is not desktop-wide OS input injection.

## Common configuration

Change configuration while the Runtime is stopped:

- `setTarget(QObject *)`;
- `setListenAddress(const QHostAddress &)`;
- `setPort(quint16)`;
- `setRemoteInputEnabled(bool)`;
- `start()` / `stop()`;
- `state()`;
- `connectedClientCount()`;
- `lastError()` / `clearError()`.

Example:

```cpp
HyRemote::RemoteAccess remote(&window);
remote.setPort(5901);
remote.setRemoteInputEnabled(true);

if (!remote.start()) {
    const auto error = remote.lastError();
    // Present the product-level error through your application's normal diagnostics.
}
```

Listener addresses use numeric IP addresses. Binding outside loopback expands the network trust boundary; read [`../../security.md`](../../security.md) before doing so.

## Runtime state

Typical state flow:

```text
Stopped -> Starting -> Running
                    \-> Faulted
Running/Faulted -> stop -> Stopped
```

`Running` means the Runtime/listener is active. It does not mean a viewer is connected or authenticated.

`connectedClientCount()` reports operational connection count, not identity or authorization role.

## Deployment

Use the single deployment helper:

```cmake
install(TARGETS MyApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyApp)
```

`hyremote_deploy()` carries the Shared Runtime and required runtime dependencies. Applications should not copy internal HyRemote libraries by filename.

See [`../guide/deployment.md`](../guide/deployment.md).

## Security boundary

The shipped Shared Runtime is LAN-first:

- default bind is `0.0.0.0` (every IPv4 interface), or one exact local IPv4, or a named network interface;
- remote input is disabled by default;
- `Insecure` is unauthenticated and unencrypted: it is for a **trusted LAN only** and is not Internet-safe;
- `Authenticated` may use RFB VNC authentication, but the stream is currently unencrypted;
- `AuthenticatedEncrypted` fails closed without opening a listener while the encrypted backend is unavailable.

Do not expose V0.1 directly to the public Internet. See [`../../security.md`](../../security.md).

## Examples

The new Example curriculum follows a “from zero to real use” journey with dedicated C++ Widgets and C++ Quick onboarding paths.

The canonical walkthrough for integrating this route into your own project - acquisition, CMake discovery, build,
deploy, run and viewer connection - is [`../guide/integrate-your-project.md`](../guide/integrate-your-project.md).
Owned examples live in `examples/learning/`, and real open-source integration studies in `examples/real-world/`.

## Next steps

- zero-code integration: [`generic.md`](generic.md);
- QML API: [`qml.md`](qml.md);
- deployment: [`../guide/deployment.md`](../guide/deployment.md);
- viewer workflow: [`../guide/viewer-connection.md`](../guide/viewer-connection.md);
- troubleshooting: [`../guide/troubleshooting.md`](../guide/troubleshooting.md);
- compatibility: [`../../compatibility.md`](../../compatibility.md);
- known limitations: [`../../known-limitations.md`](../../known-limitations.md).