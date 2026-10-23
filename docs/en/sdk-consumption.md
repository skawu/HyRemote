# HyRemote SDK Consumption Contract

> Language / 语言: **English** | [中文](../sdk-consumption.md)

HyRemote is an independent SDK and framework for Qt applications. It is consumed through normal CMake and Qt mechanisms and does not require modifying the user's Qt installation.

The product contract stays small: one Shared Runtime, four peer integration frontends, two acquisition methods, and one deployment helper.

## 1. Product mental model

An application developer chooses an integration frontend first:

| Frontend | Application code change | Product role |
| --- | --- | --- |
| **C++ API** | Link `HyRemote::RemoteAccess` | Explicit lifecycle and policy control |
| **Generic Plugin** | No HyRemote application linkage | Preferred zero-code path while preserving the native Qt platform |
| **QML API** | `import HyRemote` | Declarative frontend for Qt Quick |
| **QPA** | No HyRemote application linkage; start with `-platform hyremote` | Specialized exact-private-ABI zero-code path |

Qt Widgets and Qt Quick are target types handled by the Shared Runtime, not separate SDK products.

Applications do not need to instantiate or understand:

- Core;
- Session internals;
- `RemoteFrame`;
- capture backend classes;
- transport or RFB implementation objects;
- input backend objects;
- graphics or SoC acceleration backends.

HyRemote owns those.

## 2. Acquisition models

HyRemote supports two first-class acquisition paths.

### Installed SDK

The installed SDK exposes CMake package metadata through:

```cmake
find_package(HyRemote CONFIG REQUIRED)
```

A conceptual install prefix:

```text
hyremote-sdk/
├── include/HyRemote/
├── lib/
│   ├── cmake/HyRemote/
│   └── HyRemote product Runtime
├── qml/HyRemote/                 # when QML payload is included
├── lib/HyRemote/plugins/generic/ # when Generic payload is included
├── plugins/platforms/            # deployed QPA payload in the application tree
└── licenses/
```

The SDK is separate from the Qt SDK. Users do not copy HyRemote files into their Qt installation.

### Source consumption

HyRemote can also be vendored into an application build:

```cmake
add_subdirectory(third_party/HyRemote EXCLUDE_FROM_ALL)
```

Source consumption inherits the caller's compiler, toolchain, sysroot, and Qt SDK. It uses the same product API and deployment model as the installed SDK.

Do not mix installed-package and source acquisition for the same HyRemote instance in one CMake configure.

## 3. Shared Runtime contract

`HyRemote::RemoteAccess` and `HyRemoteRemoteAccess` are the normal Shared Runtime artifacts.

Core remains internal and is not a second deployed Runtime. QML, Generic, and QPA are frontend payloads over the same Runtime, not independent product stacks.

This rule remains true even when a physical build later slices optional Qt dependencies for embedded or Quick-only products.

## 4. C++ API consumption

Qt Widgets:

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)
find_package(HyRemote CONFIG REQUIRED)

target_link_libraries(MyApp PRIVATE
    Qt6::Widgets
    HyRemote::RemoteAccess
)
```

Qt Quick:

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Quick)
find_package(HyRemote CONFIG REQUIRED)

target_link_libraries(MyApp PRIVATE
    Qt6::Quick
    HyRemote::RemoteAccess
)
```

Both use the same API:

```cpp
#include <HyRemote/RemoteAccess.h>

HyRemote::RemoteAccess remote(&window);
remote.start();
```

Product invariants:

- construction alone does not open a listener;
- `start()` and `stop()` are explicit;
- bind address and port are product configuration, not backend-specific types;
- remote viewing and remote input are separate policies;
- errors and diagnostics use HyRemote product types;
- switching an internal transport or capture backend must not require ordinary application-source changes.

## 5. Generic Plugin consumption

Generic Plugin is the preferred zero-code route when the application can keep its normal Qt platform.

The application stays Qt-only:

```cmake
target_link_libraries(MyExistingApp PRIVATE Qt6::Widgets)

find_package(HyRemote CONFIG REQUIRED)
install(TARGETS MyExistingApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyExistingApp GENERIC)
```

Activate through Qt's generic-plugin mechanism:

```text
MyExistingApp -plugin hyremote
```

Or, where appropriate:

```text
QT_QPA_GENERIC_PLUGINS=hyremote
```

Generic compatibility is based on public Qt plugin APIs and must preserve the application's native Qt platform identity.

## 6. QML API consumption

```qml
import HyRemote

RemoteAccess {
    target: mainWindow
    enabled: true
}
```

The QML API is a declarative frontend over the same Shared Runtime used by the C++ API.

Deployment:

```cmake
hyremote_deploy(TARGET MyQmlApp QML)
```

Current status: peer route. Installed-SDK QML examples and deployment qualification continue to improve.

## 7. QPA consumption

QPA keeps the application Qt-only:

```cmake
target_link_libraries(MyExistingApp PRIVATE Qt6::Widgets)

find_package(HyRemote CONFIG REQUIRED)
install(TARGETS MyExistingApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyExistingApp QPA)
```

Run with:

```text
MyExistingApp -platform hyremote
```

QPA uses the Qt private ABI and therefore has an exact-version compatibility boundary. The current reference is Qt 6.8.3 on Windows x86_64 and Linux x86_64.

Prefer Generic when it satisfies the application. Use QPA when platform-entry behavior is actually required.

## 8. Deployment contract

HyRemote owns one deployment helper family:

```cmake
hyremote_deploy(TARGET MyCppApp)
hyremote_deploy(TARGET MyQmlApp QML)
hyremote_deploy(TARGET ExistingQtApp GENERIC)
hyremote_deploy(TARGET ExistingQtApp QPA)
hyremote_deploy(TARGET ExistingQmlApp QML QPA)
```

The helper places the Shared Runtime and selected frontend payloads and cooperates with Qt deployment support for runtime dependencies.

A normal application should not manually discover or copy HyRemote internal DLL, SO, or plugin files, or keep SDK-specific runtime paths in the deployed environment.

Generic and QPA are alternative zero-code platform strategies, not one combined integration mode.

See [`../guide/deployment.md`](../guide/deployment.md).

## 9. Product package expectations

A clean deployed application runs without the original HyRemote build or source tree.

The deployment must not rely on:

- HyRemote source or build directories;
- an unrelated HyRemote SDK prefix;
- manual Core, transport, or capture file copying;
- permanent `QT_PLUGIN_PATH` or `LD_LIBRARY_PATH` workarounds pointing back to a developer SDK.

Installed SDK and source-consumption paths lead to the same application-facing product model.

## 10. Backend and toolchain isolation

Normal application code remains isolated from transport, capture, and acceleration implementation choices.

A backend-specific compiler, runtime, or SDK may be introduced internally only when its product benefit justifies the distribution cost. Backend-specific types must not leak into the ordinary C++ or QML application contract.

The same rule applies to future DMA-BUF, RKMPP, VAAPI, D3D, or alternative transport implementations.

## 11. Current SDK compatibility

V0.3.0.0 reference:

- Windows x86_64;
- Linux x86_64;
- Qt 6.8.3;
- C++ API, QML API, Generic Plugin, and QPA are peer routes;
- QPA still requires an exact Qt private ABI.

> **TODO V0.4:** qualify the planned Qt 5.15 LTS compatibility line before adding it to the supported installed-SDK matrix.
>
> **TODO:** add embedded and ARM64 SDK and cross-compilation package/deployment contracts. Embedded Linux, ARM64, and EGLFS reference cells are mandatory pre-GA V0.3-family baseline work under #343. Additional BSP or SoC expansion is admitted separately, with no version number preassigned (#333).

See [`../compatibility.md`](../compatibility.md).

## 12. Example and learning contract

Examples are the developer product entry point, not architecture demos.

Current runnable routes:

```text
01-widgets-cpp        C++ Widgets
02-quick-cpp          C++ Quick
03-generic            Generic zero-code
04-qml                QML API
05-qpa                QPA proxy
```

Every HyRemote-authored GUI example uses the canonical project branding and provides English and Simplified Chinese UI/documentation where applicable.

The C++ and Generic end-to-end integration path is in [`../guide/integrate-your-project.md`](../guide/integrate-your-project.md).

## 13. Definition of success

A Qt developer can:

1. choose an integration frontend from the application ownership model;
2. acquire HyRemote through the installed SDK or source;
3. build the application without understanding internal Core, transport, or capture architecture;
4. deploy with `hyremote_deploy()`;
5. run from the application's own deployment tree;
6. connect a standard viewer and obtain the documented view and control behavior;
7. understand the exact security, compatibility, and known-limitations boundary from product documentation.
