# Declarative QML Consumption and Deployment

HyRemote's QML API is a thin declarative frontend over the same Shared Runtime used by the C++ API. It does not introduce a second Session, capture, input, or transport stack, and its backing implementation is not a second public C++ SDK target.

Current product status: **Preview**.

> **TODO V0.3:** complete the final QML learning examples, clean installed-SDK qualification, and deployment matrix before promoting this path to the same product-support level as the V0.1 primary paths.

## 1. Installed SDK

Use the Qt version supported by the selected HyRemote package and locate the standalone HyRemote prefix normally:

```cmake
find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Core Gui Qml Quick)
find_package(HyRemote CONFIG REQUIRED)
```

When the installed package contains the QML payload, HyRemote publishes the QML import root needed by the deployment helper. Application developers do not copy HyRemote QML plugin files or internal libraries by filename.

The application-facing boundary is simply:

```qml
import HyRemote

RemoteAccess {
    target: mainWindow
    enabled: true
}
```

`enabled: true` is an explicit declarative start request. The Runtime starts only after QML component construction is complete so initial target and policy bindings can settle.

Safe defaults are shared with the C++ API:

- loopback listener;
- port 5921;
- remote input disabled;
- no listener merely from importing the module.

## 2. Define the application normally

A normal Qt Quick application keeps its own QML module and normal Qt dependencies:

```cmake
qt_add_executable(MyQmlApp
    main.cpp
)

qt_add_qml_module(MyQmlApp
    URI MyApplication
    VERSION 1.0
    QML_FILES Main.qml
)

target_link_libraries(MyQmlApp PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
)
```

The application does not link HyRemote Core, Session, capture, input, transport, or QML implementation targets.

## 3. Deploy through `hyremote_deploy()`

Install the application first, then use the HyRemote deployment helper:

```cmake
install(TARGETS MyQmlApp
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

hyremote_deploy(TARGET MyQmlApp QML)
```

`QML` selects an already-present HyRemote QML payload. It is not a build switch and cannot turn a C++-only SDK into a QML-enabled SDK at consumer configure time.

If the selected HyRemote package does not contain the QML payload, configuration fails instead of creating a partially deployable application.

The deployment helper integrates HyRemote's QML import root with Qt's normal QML deployment support and carries the same Shared Runtime used by the other frontends.

Applications should not:

- copy `qmldir` manually;
- copy HyRemote QML plugin binaries by filename;
- point `QML_IMPORT_PATH` back to the SDK as a permanent deployment workaround;
- discover transport/capture implementation files.

## 4. Source consumption

Source acquisition uses the same application-facing QML and deployment model.

Enable the QML frontend before adding HyRemote:

```cmake
set(HYREMOTE_BUILD_QML_API ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/HyRemote EXCLUDE_FROM_ALL)
```

Then deploy exactly as with an installed SDK:

```cmake
hyremote_deploy(TARGET MyQmlApp QML)
```

The source tree may create private build targets needed to materialize the QML payload, but those targets are not added to the application's public link contract.

## 5. QML + QPA

A QML application may deliberately combine the declarative HyRemote API with the QPA deployment frontend:

```cmake
hyremote_deploy(TARGET MyQmlApp QML QPA)
```

This packages both frontend payloads while still using one Shared Runtime. It does not create a new Runtime architecture.

QPA remains exact-Qt-private-ABI qualified; see [`getting-started/qpa-proxy.md`](getting-started/qpa-proxy.md) and [`compatibility.md`](compatibility.md).

## 6. Lifecycle semantics

The QML `RemoteAccess` surface follows the same product lifecycle as the C++ facade:

- object construction/import does not open a listener;
- `enabled: true` requests start after component completion;
- `enabled: false` stops the owned Runtime;
- configuration changes belong to the stopped state;
- a failed start returns `enabled` to false and exposes product-level error state;
- `Running` means the Runtime/listener is active, not that a viewer is connected;
- `connectedClientCount` is operational state, not authentication or authorization;
- destroying the QML object stops its owned Runtime.

## 7. Error handling

The QML API exposes HyRemote product-level state and diagnostics rather than RFB/socket/backend objects.

Applications should react to the documented public state/error properties and avoid depending on internal implementation messages.

A non-recoverable Runtime fault remains visible until the application explicitly stops/disables the Runtime. Clearing a diagnostic does not silently recover or replace a failed Runtime instance.

## 8. Security boundary

The QML frontend does not weaken the shared security defaults.

V0.1 behavior:

- loopback by default;
- remote input disabled by default;
- unauthenticated non-loopback exposure rejected;
- `Authenticated` may use RFB VNC authentication, on an unencrypted stream;
- `AuthenticatedEncrypted` encrypts the stream (VeNCrypt 0.2 + X509Vnc 261 + TLS >= 1.2) with VNC Authentication inside TLS, on the OpenSSL TLS backend only;
- `AuthenticatedEncrypted` fails closed while that backend or the certificate/private key is unusable, and never falls back to a weaker profile.

Do not expose the current product directly to the public Internet. See [`security.md`](security.md).

## 9. Compatibility

Current reference environment:

- Windows x86_64 + Qt 6.8.3;
- Linux x86_64 + Qt 6.8.3.

The QML API is currently **Preview**. Basic Qt Quick operation does not automatically qualify every Quick3D, custom FBO, mixed `QQuickWidget`, graphics-backend, or native-window configuration.

See [`compatibility.md`](compatibility.md) and [`known-limitations.md`](known-limitations.md) for the current product boundary.

## 10. Recommended next reading

- [`getting-started/qml.md`](getting-started/qml.md) — concise first integration
- [`guide/deployment.md`](guide/deployment.md) — deployment model
- [`guide/viewer-connection.md`](guide/viewer-connection.md) — viewer and remote-control workflow
- [`security.md`](security.md) — security boundary
- [`compatibility.md`](compatibility.md) — current support matrix
