# HyRemote Package Manifest

> Language / 语言: **English** | [中文](../release-package-manifest.md)

This document defines the application-facing SDK and deployment payload model. It describes product artifacts, not repository acceptance procedure.

HyRemote keeps the installed product small: **one Shared Runtime**, optional frontend payloads, CMake package metadata, and product documentation and licences.

## Package metadata

An installed HyRemote SDK provides:

- `HyRemoteConfig.cmake` and version/package metadata;
- `HyRemoteTargets.cmake` for exported application targets;
- `HyRemoteDeploy.cmake` and `hyremote_deploy()`;
- public headers for exported APIs;
- the selected frontend payloads;
- Apache-2.0 `LICENSE`;
- `NOTICE.md`;
- applicable product documentation.

The installed SDK is separate from the user's Qt installation. HyRemote does not require copying files into the Qt SDK tree.

## Shared Runtime

The normal Runtime artifact:

```text
HyRemote::RemoteAccess
HyRemoteRemoteAccess
```

It is the single shared C++ Runtime used by all frontends.

Core is internal composition and is not a second application Runtime or ordinary exported SDK target.

`BUILD_SHARED_LIBS` does not create separate public HyRemote product variants.

## C++ API payload

The installed SDK exports:

- `HyRemote::RemoteAccess`;
- `HyRemote/RemoteAccess.h` and other explicitly public application types;
- the `HyRemoteRemoteAccess` runtime payload.

Typical application use:

```cmake
find_package(HyRemote CONFIG REQUIRED)
target_link_libraries(MyApp PRIVATE HyRemote::RemoteAccess)
```

Deployment:

```cmake
hyremote_deploy(TARGET MyApp)
```

Applications do not need Core, Session, capture, input, transport, or target-adapter filenames.

## Generic Plugin payload

When Generic Plugin is included, the SDK contains the HyRemote Qt Generic Plugin plus package metadata required by:

```cmake
hyremote_deploy(TARGET ExistingQtApp GENERIC)
```

The target application remains Qt-only and does not link a HyRemote application target.

The deployed application keeps its native Qt platform plugin (`qwindows`, `qxcb`, or another qualified native platform) and loads HyRemote through Qt's generic-plugin mechanism.

Generic Plugin is based on public Qt APIs and does not inherit QPA private-ABI compatibility constraints.

## QML payload: peer integration route

When QML API is included, the SDK contains the `HyRemote` QML module over the same Shared Runtime.

Deployment:

```cmake
hyremote_deploy(TARGET MyQmlApp QML)
```

The QML backing payload is not a second public C++ Runtime target.

> The installed QML-only consumer and deployed diagnostic path have L1 regression evidence. Formal V0.3.0.0 release qualification still requires the exact frozen-candidate L2 Windows and Linux four-route gate. Peer status is not a claim for unqualified Qt or platform combinations.

## QPA payload: peer integration route with exact private ABI

When QPA is included, the SDK contains the `qhyremote` platform plugin and package metadata required by:

```cmake
hyremote_deploy(TARGET ExistingQtApp QPA)
```

Or, for an application that also uses the HyRemote QML API:

```cmake
hyremote_deploy(TARGET ExistingQmlApp QML QPA)
```

The application remains Qt-only at link level. The installed SDK does not expose `HyRemote::QpaPlatform` as an application target.

QPA uses the Qt private ABI and therefore has an exact-version compatibility boundary. The current reference is Qt 6.8.3 on Windows x86_64 and Linux x86_64.

> QPA installed-consumer and product-fit tests exist on the Qt 6.8.3 Windows and Linux reference cells. V0.3.0.0 acceptance still requires an exact frozen-candidate L2 gate. Do not infer compatibility with another Qt patch, QPA delegate, or graphics stack.

## Deployment forms

The product deployment helper supports these application-facing forms:

```cmake
hyremote_deploy(TARGET MyCppApp)
hyremote_deploy(TARGET MyQmlApp QML)
hyremote_deploy(TARGET ExistingQtApp GENERIC)
hyremote_deploy(TARGET ExistingQtApp QPA)
hyremote_deploy(TARGET ExistingQmlApp QML QPA)
```

Generic and QPA are alternative zero-code platform strategies and are not enabled together as one product mode.

The helper owns HyRemote Runtime and frontend payload placement and cooperates with Qt deployment support for Qt runtime dependencies.

## Deployed tree expectations

Exact filenames vary by platform. The conceptual deployed tree is:

```text
MyApplication/
├── bin/ or application executable
├── HyRemote Shared Runtime
├── Qt runtime libraries
├── plugins/
│   ├── platforms/       # native Qt platform, plus qhyremote when QPA is selected
│   └── generic/         # HyRemote Generic Plugin when GENERIC is selected
└── qml/
    └── HyRemote/        # when QML is selected
```

A clean deployment must not depend on the original HyRemote source or build tree.

Do not use permanent SDK-path workarounds:

- `QT_PLUGIN_PATH` pointing back to the developer SDK;
- `QT_QPA_PLATFORM_PLUGIN_PATH` pointing back to a build tree;
- `LD_LIBRARY_PATH` pointing back to HyRemote build output;
- manual copying of internal Core, transport, or capture files.

## What is not part of the public SDK

Repository implementation and testing material does not become application API merely because it exists in source control.

The following are internal or development assets unless a future product explicitly promotes them:

- Core implementation classes;
- Session, transport, capture, and input internals;
- private Widgets and Quick target adapters;
- QPA private controller and composition classes;
- test clients and harnesses;
- CI and workflow scripts;
- research and benchmark utilities;
- hardware-acceleration implementation details.

Their internal shape may evolve without creating additional application integration modes.

## Licences and notices

HyRemote package artifacts carry the project's Apache-2.0 `LICENSE` and `NOTICE.md`.

Qt and other third-party runtime components remain governed by their own licences. A distributor is responsible for satisfying the obligations of the exact third-party binaries shipped with an application.

Any newly bundled third-party component must update the package licence and notice set deliberately.

## Current product package status

Current package focus:

| Payload | Status |
| --- | --- |
| Shared Runtime / C++ API | peer route |
| Generic Plugin | peer route |
| QML module | peer route |
| QPA plugin | peer route with exact Qt private ABI |
| Qt 6.8.3 Windows/Linux package line | current reference |
| Qt 5.15 package line | mandatory V0.3-family pre-GA work under #343; not part of the V0.3.0.0 reference release |
| ARM64 / embedded Linux packages | TODO, mandatory pre-GA V0.3-family compatibility baseline under #343; no exact Feature version reserved |

The package contract stays centered on one Runtime even as later releases add platform or dependency slicing or hardware acceleration.
