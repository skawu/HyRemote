# HyRemote SDK Consumption Contract

HyRemote is an independent SDK/framework for Qt applications. It is consumed through normal CMake/Qt mechanisms and does not require modifying the user's Qt installation.

The product contract is intentionally small: **one Shared Runtime**, four peer integration frontends, two acquisition methods, and one deployment helper.

## 1. Product mental model

An application developer should choose an integration frontend first:

| Frontend | Application code change | Product role |
| --- | --- | --- |
| **C++ API** | Link `HyRemote::RemoteAccess` | Explicit lifecycle/policy control |
| **Generic Plugin** | No HyRemote application linkage | Preferred zero-code path while preserving native Qt platform |
| **QML API** | `import HyRemote` | Declarative frontend for Qt Quick |
| **QPA** | No HyRemote application linkage; start with `-platform hyremote` | Specialized exact-private-ABI zero-code path |

Qt Widgets and Qt Quick are target types handled by the Shared Runtime; they are not separate SDK products.

Applications should not need to instantiate or understand:

- Core;
- Session internals;
- `RemoteFrame`;
- capture backend classes;
- transport/RFB implementation objects;
- input backend objects;
- graphics/SoC acceleration backends.

Those are owned by HyRemote.

## 2. Acquisition models

HyRemote supports two first-class acquisition paths.

### Installed SDK

The installed SDK exposes CMake package metadata through:

```cmake
find_package(HyRemote CONFIG REQUIRED)
```

A conceptual install prefix contains:

```text
hyremote-sdk/
├── include/HyRemote/
├── lib/
│   ├── cmake/HyRemote/
│   └── HyRemote product runtime
├── qml/HyRemote/                 # when QML payload is included
├── lib/HyRemote/plugins/generic/ # when Generic payload is included
├── plugins/platforms/            # deployed QPA payload in application tree
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

`HyRemote::RemoteAccess` / `HyRemoteRemoteAccess` is the normal shared Runtime artifact.

Core remains internal and is not a second deployed Runtime. QML, Generic, and QPA are frontend payloads over the same Runtime rather than independent product stacks.

This rule remains true even when a physical build later slices optional Qt dependencies for embedded/Quick-only products.

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
- `start()` / `stop()` are explicit;
- bind address and port are product configuration, not backend-specific types;
- remote viewing and remote input are separate policies;
- errors/diagnostics use HyRemote product types;
- switching an internal transport/capture backend must not require normal application-source changes.

## 5. Generic Plugin consumption

Generic Plugin is the preferred zero-code route when the application can keep its normal Qt platform.

The application stays Qt-only:

```cmake
target_link_libraries(MyExistingApp PRIVATE Qt6::Widgets)

find_package(HyRemote CONFIG REQUIRED)
install(TARGETS MyExistingApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyExistingApp GENERIC)
```

Activate with Qt's generic-plugin mechanism:

```text
MyExistingApp -plugin hyremote
```

or, where appropriate:

```text
QT_QPA_GENERIC_PLUGINS=hyremote
```

Generic compatibility is based on public Qt plugin APIs. It must preserve the application's native Qt platform identity.

## 6. QML API consumption — Preview

```qml
import HyRemote

RemoteAccess {
    target: mainWindow
    enabled: true
}
```

The QML API is a declarative frontend over the same Shared Runtime used by the C++ API.

Deployment uses:

```cmake
hyremote_deploy(TARGET MyQmlApp QML)
```

> **Open work (current product line):** complete full productization, bilingual examples, and deployment qualification on the QML route.

## 7. QPA consumption — Preview

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

QPA uses Qt private ABI and therefore has an exact-version compatibility boundary. The current reference is Qt 6.8.3 on Windows/x86_64 and Linux/x86_64.

Prefer Generic when it satisfies the application. Use QPA when platform-entry behavior is actually required.

> **Open work (current product line):** complete formal QPA productization and add new exact Qt/OS compatibility rows only after qualification.

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

A normal application should not manually discover/copy HyRemote internal DLL/SO/plugin files or keep SDK-specific runtime paths in the deployed environment.

Generic and QPA are alternative zero-code platform strategies and are not intended to be combined as one integration mode.

See [`guide/deployment.md`](guide/deployment.md).

## 9. Product package expectations

A clean deployed application should be able to run without the original HyRemote build/source tree.

The application deployment must not rely on:

- HyRemote source/build directories;
- an unrelated HyRemote SDK prefix;
- manual Core/transport/capture file copying;
- permanent `QT_PLUGIN_PATH` / `LD_LIBRARY_PATH` workarounds pointing back to a developer SDK.

The installed SDK and source-consumption paths should lead to the same application-facing product model.

## 10. Backend/toolchain isolation

Normal application code must remain isolated from transport/capture/acceleration implementation choices.

A backend-specific compiler, runtime, or SDK may be introduced internally only when its product benefit justifies the distribution cost. Backend-specific types must not leak into the ordinary C++/QML application contract.

The same rule applies to future DMA-BUF, RKMPP, VAAPI, D3D, or alternative transport implementations.

## 11. Current SDK compatibility

V0.1 reference:

- Windows x86_64;
- Linux x86_64;
- Qt 6.8.3;
- C++ API + Generic Plugin as primary paths;
- QML API + QPA as Preview paths.

> **TODO V0.4:** qualify the planned Qt 5.15 LTS compatibility line before adding it to the supported installed-SDK matrix.
>
> **TODO V1.1:** add embedded/ARM64 SDK and cross-compilation package/deployment contracts.

See [`compatibility.md`](compatibility.md).

## 12. Example/learning contract

Examples are the first developer-product entry point, not architecture demos.

The product curriculum is designed around the user journey:

```text
01 Widgets + C++
02 Quick + C++
03 Zero-code Generic
04 Quick + QML
05 Control/Lifecycle
06 Security/Session
07 Zero-code QPA
08 Deployment
09 Production Showcase
```

Every HyRemote-authored GUI example uses the canonical project branding and provides English + Simplified Chinese UI/documentation where applicable.

> The C++ and Generic onboarding sets are documented end to end in
> [\guide/integrate-your-project.md\](guide/integrate-your-project.md).
>
> **Open work (current product line):** complete the full 01–09 product curriculum.

## 13. Definition of success

The SDK contract succeeds when a Qt developer can:

1. choose an integration frontend from the application ownership model;
2. acquire HyRemote through installed SDK or source;
3. build the application without understanding internal Core/transport/capture architecture;
4. deploy with `hyremote_deploy()`;
5. run from the application's own deployment tree;
6. connect a standard viewer and obtain the documented view/control behavior;
7. understand the exact security, compatibility, and known-limitations boundary from product documentation.