# Embedded C++ Getting Started

Status: V0.0.1.0 user guide. The release gate remains issue #30.

HyRemote's reference integration is a small C++ facade delivered as one shared library. Existing Qt Widgets and Qt Quick applications link only `HyRemote::RemoteAccess`; they do not assemble Core sessions, capture sources, transports, input sinks, or RFB objects.

## Prerequisites

The current V1 reference line is Qt 6.8.x; automated product work targets Qt 6.8.3 on Windows x86_64 and Linux x86_64. Other Qt versions are not implied to be supported unless recorded in `docs/reference/compatibility.md`.

Choose either:

- installed SDK: `find_package(HyRemote CONFIG REQUIRED)`;
- source/vendored: `add_subdirectory(path/to/HyRemote hyremote)`.

Both expose the same application target: `HyRemote::RemoteAccess`.

## Minimal Widgets use

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

That is the normal baseline. Construction is inert; `start()` opens the service. The default address is loopback, the default port is 5900, and remote input is disabled.

Enable remote control only when required:

```cpp
HyRemote::RemoteAccess remote(&window);
remote.setRemoteInputEnabled(true);
remote.start();
```

## Minimal Qt Quick use

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

Widgets and Quick share the same public facade. Capture/input implementation selection remains internal.

## Optional configuration

Configuration changes are made while stopped:

- `setTarget(QObject *)`;
- `setListenAddress(const QHostAddress &)`;
- `setPort(quint16)`;
- `setRemoteInputEnabled(bool)`;
- `start()` / `stop()`;
- `state()` / `connectedClientCount()` / `lastError()` / `clearError()`.

A non-default example:

```cpp
HyRemote::RemoteAccess remote(&window);
remote.setPort(5901);
remote.setRemoteInputEnabled(true);

if (!remote.start()) {
    const auto error = remote.lastError();
    // Report the product-level error.
}
```

Normal applications do not select transport/backend/capture classes.

## Deployment

The V1 C++ runtime artifact is the shared `HyRemoteRemoteAccess` library. Core is statically composed behind it, so users do not deploy a second HyRemote Core runtime.

Use the one package helper:

```cmake
install(TARGETS MyApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyApp)
```

The helper composes with Qt's supported deployment tooling and adds the HyRemote shared facade automatically. Applications should not copy HyRemote libraries by filename.

See `docs/deployment.md`.

## Security baseline

The current RFB SecurityType None correctness transport is unauthenticated and unencrypted. Do not expose it directly to untrusted networks. Loopback is the default bind and remote input is disabled by default. See `docs/reference/security.md` and `docs/reference/known-limitations.md`.

## Examples and evidence

- `examples/widgets-basic`
- `examples/quick-basic`
- `examples/remote-support-showcase`

Hosted/offscreen E2E verifies protocol-to-application correctness; it does not replace required physical local-display/local-input coexistence evidence.

## Next steps

- platform setup: `docs/guide/install.md` (中文) or `docs/en/guide/install.md` (English);
- viewer workflow: `docs/viewer-connection.md`;
- deployment: `docs/deployment.md`;
- troubleshooting: `docs/troubleshooting.md`;
- exact support status: `docs/reference/compatibility.md` and `docs/reference/known-limitations.md`.
