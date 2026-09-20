# Deployment

> Language / 语言: **English** | [中文](../../guide/deployment.md)

HyRemote owns deployment of its product runtime and integration payloads instead of requiring application developers to discover internal backend files manually.

## One deployment entry point

The installed SDK and source/add_subdirectory acquisition expose the same application-facing helper:

```cmake
hyremote_deploy(TARGET MyCppApp)
hyremote_deploy(TARGET MyQmlApp QML)
hyremote_deploy(TARGET ExistingQtApp QPA)
hyremote_deploy(TARGET ExistingQmlApp QML QPA)
```

These forms extend one deployment contract; they do not create separate runtime architectures.

`QML` and `QPA` select optional payloads that must already exist in the chosen HyRemote build/package. They are not build-option switches: `hyremote_deploy()` does not turn a C++-only SDK into a QML/QPA SDK at application configure time. A requested optional payload that is unavailable fails closed during configuration instead of producing an incomplete deployment that fails later at runtime.

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

Build/install HyRemote with the QML API enabled, then package the application with:

```cmake
install(TARGETS MyQmlApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyQmlApp QML)
```

Both installed and source acquisition publish an absolute `HyRemote_QML_IMPORT_PATH` representing their current HyRemote QML import root. The helper appends that root to the application's existing QML import paths and uses Qt's supported QML-aware deployment machinery. The same supplemental HyRemote step carries the shared `RemoteAccess` runtime.

If the selected HyRemote build/package has no QML payload, `hyremote_deploy(... QML)` fails during configuration. It does not silently generate a deployment without `import HyRemote`.

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

- the exact SDK/source-built `qhyremote` platform module selected by acquisition;
- the shared `HyRemoteRemoteAccess` runtime used internally by that module;
- the Qt/native-platform dependencies resolved by Qt deployment tooling.

The application executable itself still does not link `HyRemote::RemoteAccess`.

The current QPA package is version-coupled to exact Qt 6.8.3. `hyremote_deploy(... QPA)` fails closed when the selected HyRemote build/package lacks QPA support or the consumer Qt version does not match the qualified private-ABI line.

On Linux, qhyremote carries a bounded origin-relative relocation anchor to the shared facade. When deployment moves the plugin into the application's normal `plugins/platforms` directory, HyRemote rewrites that package-owned RUNPATH segment to the application's Qt deploy library directory. Normal deployed applications should not require `LD_LIBRARY_PATH` or `QT_PLUGIN_PATH` merely to find HyRemote.

For source/add_subdirectory consumption, qhyremote remains an internal build target and is kept inside the HyRemote sub-build rather than writing into the host application's top-level plugin build directory. The deploy helper resolves the target file directly, so this build isolation does not change application usage.

See `docs/getting-started/qpa-proxy.md` and `docs/guide/install.md`.

## QML + QPA

A QML application may deliberately use both options when both optional payloads exist:

```cmake
hyremote_deploy(TARGET MyQmlApp QML QPA)
```

`QML` selects Qt's QML-aware deployment flow and `QPA` adds the proxy module. Both still use the same shared `RemoteAccess` runtime; this is not a fourth runtime architecture. Availability/version checks for both selected payloads remain fail-closed.

## Security and network configuration

Deployment does not weaken product defaults:

- Embedded C++ construction is inert;
- QML remains disabled until explicitly enabled by application policy;
- QPA listener creation follows the documented platform-plugin lifecycle;
- loopback is the default bind;
- remote input is disabled by default;
- the current RFB SecurityType None baseline provides neither viewer authentication nor transport encryption.

If an application intentionally changes the bind address, its operator/deployment documentation must describe the resulting trust boundary. See `docs/security.md`.

## Deployment verification boundary

Normal V1 deployment requires that clean installed and declared source-consumption applications can run from their deployed tree without depending on the original HyRemote SDK/build runtime path. The four deployment call shapes are exercised independently where applicable; QML-only is not inferred from QML+QPA because they select different supplemental deployment paths.

The reference environments are Windows x86_64 and Linux x86_64 with the exact Qt matrix defined by the milestone.
