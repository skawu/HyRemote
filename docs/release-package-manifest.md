# HyRemote Package Manifest

This document defines the application-facing SDK and deployment payload model. It describes product artifacts, not repository acceptance procedure.

HyRemote deliberately keeps the installed product small: **one Shared Runtime**, optional frontend payloads, CMake package metadata, and product documentation/licenses.

## Package metadata

An installed HyRemote SDK provides:

- `HyRemoteConfig.cmake` and version/package metadata;
- `HyRemoteTargets.cmake` for exported application targets;
- `HyRemoteDeploy.cmake` / `hyremote_deploy()`;
- public headers for exported APIs;
- the selected frontend payloads;
- Apache-2.0 `LICENSE`;
- `NOTICE.md`;
- applicable product documentation.

The installed SDK is separate from the user's Qt installation. HyRemote does not require copying files into the Qt SDK tree.

## Shared Runtime

The normal Runtime artifact is:

```text
HyRemote::RemoteAccess
HyRemoteRemoteAccess
```

It is the single shared C++ Runtime used by all frontends.

Core is internal composition and is not a second application Runtime or ordinary exported SDK target.

`BUILD_SHARED_LIBS` does not create separate public HyRemote product personalities.

## C++ API payload

The installed SDK exports:

- `HyRemote::RemoteAccess`;
- `HyRemote/RemoteAccess.h` and other explicitly public application types;
- `HyRemoteRemoteAccess` runtime payload.

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

## QML payload — Preview

When QML API is included, the SDK contains the `HyRemote` QML module over the same Shared Runtime.

Deployment:

```cmake
hyremote_deploy(TARGET MyQmlApp QML)
```

The QML backing payload is not a second public C++ Runtime target.

> **TODO V0.3:** complete full QML package/example qualification before promoting this payload from Preview.

## QPA payload — Preview

When QPA is included, the SDK contains the `qhyremote` platform plugin and package metadata required by:

```cmake
hyremote_deploy(TARGET ExistingQtApp QPA)
```

or, for an application that also uses the HyRemote QML API:

```cmake
hyremote_deploy(TARGET ExistingQmlApp QML QPA)
```

The application remains Qt-only at link level. The installed SDK does not expose `HyRemote::QpaPlatform` as an application target.

QPA uses Qt private ABI and therefore has an exact-version compatibility boundary. The current reference is Qt 6.8.3 on Windows x86_64 and Linux x86_64.

> **TODO V0.3/V0.4:** complete formal QPA productization and expand exact Qt/OS rows only after qualification.

## Deployment forms

The product deployment helper supports these application-facing forms:

```cmake
hyremote_deploy(TARGET MyCppApp)
hyremote_deploy(TARGET MyQmlApp QML)
hyremote_deploy(TARGET ExistingQtApp GENERIC)
hyremote_deploy(TARGET ExistingQtApp QPA)
hyremote_deploy(TARGET ExistingQmlApp QML QPA)
```

Generic and QPA are alternative zero-code platform strategies and are not intended to be enabled together as one product mode.

The helper owns placement of HyRemote Runtime/frontend payloads and cooperates with Qt deployment support for Qt runtime dependencies.

## Deployed tree expectations

The exact filenames vary by platform, but the conceptual deployed application tree is:

```text
MyApplication/
├── bin/ or application executable
├── HyRemote Shared Runtime
├── Qt runtime libraries
├── plugins/
│   ├── platforms/       # native Qt platform, and qhyremote when QPA is selected
│   └── generic/         # HyRemote Generic Plugin when GENERIC is selected
└── qml/
    └── HyRemote/        # when QML is selected
```

A clean deployment must not depend on the original HyRemote source/build tree.

Do not use permanent SDK-path workarounds such as:

- `QT_PLUGIN_PATH` pointing back to the developer SDK;
- `QT_QPA_PLATFORM_PLUGIN_PATH` pointing back to a build tree;
- `LD_LIBRARY_PATH` pointing back to HyRemote build output;
- manual copying of internal Core/transport/capture files.

## What is not part of the public SDK

Repository implementation/testing material does not become application API merely because it exists in source control.

The following are internal or development assets unless a future product explicitly promotes them:

- Core implementation classes;
- Session/transport/capture/input internals;
- private Widgets/Quick target adapters;
- QPA private controller/composition classes;
- test clients/harnesses;
- CI/workflow scripts;
- research/benchmark utilities;
- hardware-acceleration implementation details.

Their internal shape may evolve without creating additional application integration modes.

## Licenses and notices

HyRemote package artifacts carry the project's Apache-2.0 `LICENSE` and `NOTICE.md`.

Qt and other third-party runtime components remain governed by their own licenses. A distributor is responsible for satisfying the obligations of the exact third-party binaries shipped with an application.

Any newly bundled third-party component must update the package license/notice set deliberately.

## Current product package status

V0.1 package focus:

| Payload | Status |
| --- | --- |
| Shared Runtime / C++ API | **Primary** |
| Generic Plugin | **Primary** |
| QML module | **Preview** |
| QPA plugin | **Preview** |
| Qt 6.8.3 Windows/Linux package line | Current reference |
| Qt 5.15 package line | **TODO V0.4** |
| ARM64 / Embedded Linux packages | **TODO V1.1** |

The package contract stays centered on one Runtime even as later releases add platform/dependency slicing or hardware acceleration.