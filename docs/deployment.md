# Deployment

HyRemote owns deployment of its product runtime and integration payloads instead of requiring application developers to discover internal backend files manually.

## One installed-package entry point

The installed SDK exposes one application-facing helper:

```cmake
hyremote_deploy(TARGET MyCppApp)
hyremote_deploy(TARGET MyQmlApp QML)
hyremote_deploy(TARGET ExistingQtApp QPA)
hyremote_deploy(TARGET ExistingQmlApp QML QPA)
```

These forms extend one deployment contract; they do not create separate runtime architectures.

## Fixed V1 artifact model

Normal V1 deployment is intentionally simple:

- `HyRemote::RemoteAccess` is one shared C++ product library;
- Core is statically composed behind that facade and is not a separate runtime payload;
- `qhyremote` is one Qt platform MODULE for Transparent QPA;
- the QML module is a thin wrapper over the same shared `RemoteAccess` runtime.

`BUILD_SHARED_LIBS` does not switch the normal product between static and shared personalities.

Applications must not copy or select Core, Session, RFB, capture, input, adapter, or backend implementation files by name.

## Ordinary C++ deployment

For a C++ Widgets/Quick application:

```cmake
install(TARGETS MyApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyApp)
```

Qt's normal deployment script owns Qt runtime placement. The HyRemote supplemental script installs the single `HyRemoteRemoteAccess` shared library and asks Qt's deployment support to resolve that library's Qt dependencies.

The application developer should not need to locate `HyRemoteRemoteAccess.dll` / `libHyRemoteRemoteAccess.so` manually.

## QML deployment

```cmake
install(TARGETS MyQmlApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyQmlApp QML)
```

The helper preserves application-owned QML import paths, appends the installed HyRemote import root and uses Qt's QML-aware deployment machinery. The same supplemental HyRemote step carries the shared `RemoteAccess` runtime.

See `docs/qml-consumption.md` and `docs/getting-started/qml.md`.

## Transparent QPA deployment

The application remains Qt-only at the source/link layer:

```cmake
target_link_libraries(MyApp PRIVATE Qt6::Widgets)
```

Package through the installed HyRemote SDK:

```cmake
find_package(HyRemote CONFIG REQUIRED)
install(TARGETS MyApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyApp QPA)
```

The helper adds:

- the exact SDK-built `qhyremote` platform module;
- the shared `HyRemoteRemoteAccess` runtime used internally by that module;
- the Qt/native-platform dependencies resolved by Qt deployment tooling.

The application executable itself still does not link `HyRemote::RemoteAccess`.

The current QPA package is version-coupled to exact Qt 6.8.3. `hyremote_deploy(... QPA)` fails closed when the SDK lacks QPA support or the consumer Qt version does not match the qualified private-ABI line.

On Linux, the SDK plugin uses an origin-relative RUNPATH to the shared facade. When deployment moves the plugin from the SDK tree into the application's normal `plugins/platforms` directory, HyRemote rewrites that controlled RUNPATH to the application's Qt deploy library directory. Normal installed applications should not require `LD_LIBRARY_PATH` or `QT_PLUGIN_PATH` merely to find HyRemote.

See `docs/getting-started/qpa-proxy.md`.

## QML + QPA

A QML application may deliberately use both options:

```cmake
hyremote_deploy(TARGET MyQmlApp QML QPA)
```

`QML` selects Qt's QML-aware deployment flow and `QPA` adds the proxy module. Both still use the same shared `RemoteAccess` runtime; this is not a fourth runtime architecture.

## Security and network configuration

Deployment does not weaken product defaults:

- Embedded C++ construction is inert;
- QML remains disabled until explicitly enabled by application policy;
- QPA listener creation follows the documented platform-plugin lifecycle;
- loopback is the default bind;
- remote input is disabled by default;
- the current RFB SecurityType None baseline provides neither viewer authentication nor transport encryption.

If an application intentionally changes the bind address, its operator/deployment documentation must describe the resulting trust boundary. See `docs/security.md`.

## Acceptance boundary

V1 release evidence must prove clean installed applications can run from their deployed tree without depending on the HyRemote SDK's original runtime path. Required reference environments are Windows x86_64 and Linux x86_64 with the exact Qt matrix defined by the milestone.

Hosted runner failure under #74 is infrastructure evidence only; it is not a product pass and cannot authorize a milestone tag.
