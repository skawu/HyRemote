# Embedded C++ Getting Started

Status: V0.0.1.0 user guide. The release gate remains issue #30.

HyRemote's Embedded C++ mode adds remote view/control to an existing Qt Widgets or Qt Quick application through the public `HyRemote::RemoteAccess` facade. Application code does not construct Core sessions, capture sources, transports, input sinks, or RFB protocol objects.

## Prerequisites

The current V1 reference line is Qt 6.8.x; automated product work targets Qt 6.8.3 on Windows x86_64 and Linux x86_64. Other Qt versions are not implied to be supported unless they appear as validated entries in `docs/compatibility.md`.

Choose one consumption path:

- installed SDK: follow `docs/sdk-installation.md` and use `find_package(HyRemote CONFIG REQUIRED)`;
- source/vendored: follow `docs/source-consumption.md` and add the HyRemote source tree with `add_subdirectory()`.

These paths expose the same product target: `HyRemote::RemoteAccess`.

## Widgets application

For an installed SDK:

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)
find_package(HyRemote CONFIG REQUIRED)

target_link_libraries(MyApp PRIVATE
    Qt6::Widgets
    HyRemote::RemoteAccess
)
```

Application code stays small:

```cpp
#include <HyRemote/RemoteAccess.h>

MainWindow window;
window.show();

HyRemote::RemoteAccess remote(&window);
// Construction is inert: no listener has been opened yet.
if (!remote.start()) {
    const auto error = remote.lastError();
    // Handle/report the product-level error without depending on an RFB backend type.
}
```

The default listen address is loopback and remote input is disabled. Call `setRemoteInputEnabled(true)` before `start()` only when remote control is intended.

## Qt Quick application

Link the normal Qt Quick modules plus the same HyRemote target:

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Quick)
find_package(HyRemote CONFIG REQUIRED)

target_link_libraries(MyApp PRIVATE
    Qt6::Quick
    HyRemote::RemoteAccess
)
```

Attach the facade to a live `QQuickWindow`:

```cpp
QQuickWindow *window = /* your top-level Quick window */;
HyRemote::RemoteAccess remote(window);
remote.start();
```

The current Quick correctness path uses the public asynchronous Qt capture route behind the adapter; application code does not select that capture mechanism.

## Configuration and lifecycle

Configuration is intended to be set while stopped. The public surface currently provides:

- `setTarget(QObject *)`;
- `setListenAddress(const QHostAddress &)`;
- `setPort(quint16)`;
- `setRemoteInputEnabled(bool)`;
- `start()` / `stop()`;
- `state()` / `lastError()` / `clearError()`.

Safe baseline:

```cpp
HyRemote::RemoteAccess remote(window);
remote.setListenAddress(QHostAddress(QHostAddress::LocalHost));
remote.setPort(5900);
remote.setRemoteInputEnabled(false); // view-only

if (!remote.start()) {
    // inspect remote.lastError()
}

// ... application event loop ...

remote.stop();
```

Do not expose the current unauthenticated correctness transport directly to untrusted networks. See `docs/security-model.md` and `docs/known-limitations.md`.

## Examples

The V0.0.1 product examples are tracked by #60 / PR #61:

- `examples/widgets-basic`
- `examples/quick-basic`

They use only the public facade. Their hosted VNC E2E runs with an offscreen Qt platform so that CI can verify the protocol-to-application path; that CI does not by itself prove locally visible display/input coexistence. Final coexistence evidence belongs to the product acceptance gate #30.

## Next steps

- platform-specific setup: `docs/getting-started/windows.md` or `docs/getting-started/linux.md`;
- viewer connection/control: `docs/viewer-connection.md`;
- deployment: `docs/deployment.md`;
- troubleshooting: `docs/troubleshooting.md`;
- exact support status: `docs/compatibility.md` and `docs/known-limitations.md`.
