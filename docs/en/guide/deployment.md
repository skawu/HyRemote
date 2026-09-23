# Deployment

> Language / 语言: **English** | [中文](../../guide/deployment.md)

HyRemote uses one `hyremote_deploy()` family to deploy its Shared Runtime and integration payloads. Applications should not copy HyRemote internals by filename or depend on the original source/build tree at runtime.

## One deployment entry point

Installed-SDK and source acquisition use the same application-facing helper:

```cmake
hyremote_deploy(TARGET MyCppApp)
hyremote_deploy(TARGET MyQmlApp QML)
hyremote_deploy(TARGET ExistingQtApp GENERIC)
hyremote_deploy(TARGET ExistingQtApp QPA)
hyremote_deploy(TARGET ExistingQmlApp QML QPA)
```

The four integration frontends share one Runtime. The options select deployment payloads; they do not create another Runtime architecture.

## Product payload model

| Payload | Role |
| --- | --- |
| `HyRemote::RemoteAccess` / `HyRemoteRemoteAccess` | One Shared Runtime |
| QML `HyRemote` module | Declarative QML frontend payload |
| Generic Plugin | Qt generic-plugin payload that preserves the native Qt platform |
| `qhyremote` | QPA platform-plugin payload |
| Core | Internal static composition, not a separately deployed application Runtime |

## C++ API deployment

```cmake
find_package(HyRemote CONFIG REQUIRED)

install(TARGETS MyApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyApp)
```

The application links `HyRemote::RemoteAccess`; the deployment helper carries the Shared Runtime and its Qt runtime closure.

Applications should not locate `HyRemoteRemoteAccess.dll` or `libHyRemoteRemoteAccess.so` manually.

## Generic Plugin deployment

Generic Plugin is the primary V0.1 zero-code route. The application remains Qt-only:

```cmake
target_link_libraries(MyExistingApp PRIVATE Qt6::Widgets)

find_package(HyRemote CONFIG REQUIRED)
install(TARGETS MyExistingApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyExistingApp GENERIC)
```

The deployed tree contains:

- the HyRemote Generic Plugin;
- the Shared Runtime;
- the application's normal native Qt platform plugin;
- required Qt runtime dependencies.

Generic Plugin does not replace the Qt platform. The application should still run with its normal `qwindows`, `qxcb`, or other Qt-provided platform identity.

Activate it with Qt's generic-plugin mechanism:

```text
MyExistingApp -plugin hyremote
```

or:

```text
QT_QPA_GENERIC_PLUGINS=hyremote
```

See [`../getting-started/generic.md`](../getting-started/generic.md).

## QML API deployment — Preview

```cmake
find_package(HyRemote CONFIG REQUIRED)

install(TARGETS MyQmlApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyQmlApp QML)
```

QML deployment adds the `HyRemote` import module and reuses the same Shared Runtime.

If the selected SDK does not contain the QML payload, configuration fails instead of producing an incomplete package that cannot resolve `import HyRemote`.

> **TODO:** complete the final installed-SDK example and product qualification before promoting QML API from Preview.

## QPA deployment — Preview

The application remains Qt-only at the source/link layer:

```cmake
target_link_libraries(MyApp PRIVATE Qt6::Widgets)

find_package(HyRemote CONFIG REQUIRED)
install(TARGETS MyApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyApp QPA)
```

Run with:

```text
MyApp -platform hyremote
```

The helper adds:

- `qhyremote`;
- the Shared Runtime;
- the native Qt platform plugin selected by the Factory Trampoline;
- required Qt runtime dependencies.

QPA uses Qt private ABI and is therefore qualified per exact Qt patch. The current reference is Qt 6.8.3.

A consumer whose Qt version does not match the selected QPA payload should fail closed instead of guessing compatibility.

> **TODO:** broaden QPA support only after exact-version compatibility and physical local+remote coexistence are qualified for the target Qt/OS pair.

## QML + QPA

A QML application may deliberately deploy both payloads:

```cmake
hyremote_deploy(TARGET MyQmlApp QML QPA)
```

Both still reuse one Shared Runtime.

## Generic and QPA are alternatives

Generic and QPA are two different zero-code platform strategies:

```text
Generic: native Qt platform + HyRemote generic plugin
QPA:     qhyremote Factory Trampoline -> native Qt platform
```

They are not two plugins that need to be enabled together. Choose the zero-code route that fits the application.

## Self-contained deployment

A product deployment should run without the original HyRemote SDK, Qt SDK, or build tree.

Do not hide missing payloads by:

- pointing `QT_PLUGIN_PATH` back to the Qt SDK;
- pointing `LD_LIBRARY_PATH` back to a build tree;
- copying internal HyRemote files manually from build output;
- mixing source acquisition with installed-package metadata in one consumer.

The deployed tree itself should contain the product payloads required by the application.

## Windows / Linux

Current V0.1 reference environments:

- Windows x86_64 + Qt 6.8.3;
- Linux x86_64 + Qt 6.8.3.

On Linux, HyRemote handles relocation of its own Runtime/plugin payload so ordinary deployment does not depend on returning to the SDK location.

See [`../../compatibility.md`](../../compatibility.md) for exact product status.

## Security and deployment

Deployment does not weaken the product defaults:

- default bind is `0.0.0.0` (every IPv4 interface), or one exact local IPv4, or a named network interface;
- remote input is disabled by default;
- unauthenticated non-loopback exposure is rejected;
- an authenticated profile may use RFB VNC authentication, but the current stream is not encrypted;
- `AuthenticatedEncrypted` fails closed without opening a listener while the TLS/VeNCrypt backend is unavailable.

> **TODO V0.2:** VeNCrypt/TLS, certificate policy, authenticated sessions, and production network policy.

Do not expose V0.1 directly to the public Internet. See [`../../security.md`](../../security.md).

## Deployment check

After packaging, verify that:

- the application starts without the original HyRemote/Qt SDK paths;
- a C++ API application loads the Shared Runtime;
- a Generic application contains the HyRemote generic plugin and the native Qt platform plugin;
- a QPA application contains `qhyremote` and the matching native delegate chain;
- a QML application resolves `import HyRemote`;
- runtime search paths do not point back to the source/build tree.