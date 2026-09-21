# Install, build, and integration setup

> Language / 语言: **English** | [中文](../../guide/install.md)

This guide explains how to build HyRemote, create an installed SDK, and consume HyRemote from an application project. It describes the current product shape rather than internal acceptance history.

## Current product environment

V0.1 Developer Preview currently references:

| Dimension | Current product status |
| --- | --- |
| Operating systems | Windows x86_64, Linux x86_64 |
| Qt | Qt 6.8.3 reference |
| C++ API | **V0.1 primary** |
| Generic Plugin | **V0.1 primary** |
| QML API | **Preview** |
| QPA | **Preview, Qt 6.8.3 exact private ABI** |
| Embedded Linux / ARM64 | **TODO V1.1** |
| Qt 5.15 LTS | **TODO V0.4 qualification** |

See [`../../compatibility.md`](../../compatibility.md) for the exact compatibility statement.

## Prerequisites

Recommended tools:

- CMake 3.21+;
- Ninja;
- Qt 6.8.3 development kit;
- Windows: MSVC x64;
- Linux: GCC/Clang x86_64.

QPA additionally requires the Qt private Gui development target from the **same exact Qt 6.8.3 SDK**.

## Build HyRemote from source

The repository's unified build entry point is `compile.cmd`. It works on Windows and POSIX shells and reads project defaults from `build.yml`.

Inspect the resolved configuration first:

```text
compile.cmd --show-config
```

Build the primary V0.1 paths:

```text
compile.cmd --integrations=cpp,generic --qt-prefix=/path/to/Qt/6.8.3/<kit>
```

Build all four frontends for development:

```text
compile.cmd --integrations=cpp,qml,generic,qpa --qt-prefix=/path/to/Qt/6.8.3/<kit>
```

Enable examples/tests explicitly when needed:

```text
compile.cmd --integrations=cpp,generic --examples --tests --run-tests
```

Command-line options override the corresponding `build.yml` fields. The four integrations are independent selections; enabling Generic, QML, or QPA does not make the C++ frontend their implementation parent.

## Direct CMake build

HyRemote can also be configured as a normal CMake project:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/<kit> \
  -DHYREMOTE_BUILD_CPP_API=ON \
  -DHYREMOTE_WITH_GENERIC_PLUGIN=ON
cmake --build build --parallel
```

Optional frontends:

```text
-DHYREMOTE_BUILD_QML_API=ON
-DHYREMOTE_WITH_GENERIC_PLUGIN=ON
-DHYREMOTE_WITH_QPA_PROXY=ON
```

QPA requires Qt 6.8.3 exact + `Qt6::GuiPrivate`.

## Create an installed SDK

Choose an install prefix and run the standard CMake install step:

```bash
cmake -S . -B build-sdk -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/<kit> \
  -DCMAKE_INSTALL_PREFIX=/path/to/hyremote-sdk \
  -DHYREMOTE_BUILD_CPP_API=ON \
  -DHYREMOTE_WITH_GENERIC_PLUGIN=ON

cmake --build build-sdk --parallel
cmake --install build-sdk
```

Enable QML/QPA while creating the SDK only when those payloads are required.

Applications consume the installed product through:

```cmake
find_package(HyRemote CONFIG REQUIRED)
```

They do not need to know HyRemote's internal source layout.

## Installed SDK: C++ API

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

Widgets and Quick use the same `HyRemote::RemoteAccess` target.

See [`../getting-started/cpp.md`](../getting-started/cpp.md).

## Installed SDK: Generic Plugin

A Generic application remains Qt-only and links no HyRemote target:

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)
find_package(HyRemote CONFIG REQUIRED)

target_link_libraries(MyExistingApp PRIVATE Qt6::Widgets)

install(TARGETS MyExistingApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyExistingApp GENERIC)
```

Activate it with Qt's generic-plugin mechanism:

```text
MyExistingApp -plugin hyremote
```

See [`../getting-started/generic.md`](../getting-started/generic.md).

## Installed SDK: QML API (Preview)

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick)
find_package(HyRemote CONFIG REQUIRED)
```

```qml
import HyRemote

RemoteAccess {
    target: mainWindow
    enabled: true
}
```

Deployment:

```cmake
hyremote_deploy(TARGET MyQmlApp QML)
```

> **TODO V0.3:** complete formal productization before promoting QML API from Preview.

## Installed SDK: QPA (Preview)

The application remains Qt-only:

```cmake
find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Widgets)
find_package(HyRemote CONFIG REQUIRED)

target_link_libraries(MyExistingApp PRIVATE Qt6::Widgets)

install(TARGETS MyExistingApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyExistingApp QPA)
```

Run with:

```text
MyExistingApp -platform hyremote
```

QPA support applies only to explicitly declared exact Qt/private-ABI combinations. The current reference is Qt 6.8.3.

## Source consumption

HyRemote can also be included as a source subproject:

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)

set(HYREMOTE_BUILD_CPP_API ON CACHE BOOL "" FORCE)
set(HYREMOTE_WITH_GENERIC_PLUGIN ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/HyRemote EXCLUDE_FROM_ALL)

add_executable(MyApp main.cpp)
target_link_libraries(MyApp PRIVATE Qt6::Widgets HyRemote::RemoteAccess)
```

Source consumption and installed-SDK consumption use the same product API and deployment model.

Do not mix these acquisition methods in one CMake configure:

```text
find_package(HyRemote)
+
add_subdirectory(HyRemote)
```

Choose one HyRemote acquisition source.

## Deployment

The single product deployment entry point is:

```cmake
hyremote_deploy(TARGET MyCppApp)
hyremote_deploy(TARGET MyQmlApp QML)
hyremote_deploy(TARGET ExistingQtApp GENERIC)
hyremote_deploy(TARGET ExistingQtApp QPA)
```

A normal deployment should run from the application's own deployed tree without pointing `QT_PLUGIN_PATH`, `LD_LIBRARY_PATH`, or similar variables back to the HyRemote/Qt SDK or build tree.

See [`deployment.md`](deployment.md).

## Security defaults

V0.1 defaults:

- `127.0.0.1:5921`;
- remote input disabled;
- `Insecure` is loopback-only;
- `Authenticated` is available only when HyRemote was built with the transport-security capability and a valid security descriptor is configured; it currently provides VNC authentication without stream encryption;
- the default V0.1 build/profile does not imply authenticated transport is compiled in;
- `AuthenticatedEncrypted` is not implemented and always fails closed before listener creation, without falling back to a weaker profile.

Do not expose the current product directly to the public Internet. See [`../security.md`](../security.md).

## Next steps

- C++ API: [`../getting-started/cpp.md`](../getting-started/cpp.md)
- Generic Plugin: [`../getting-started/generic.md`](../getting-started/generic.md)
- QML API (Preview): [`../getting-started/qml.md`](../getting-started/qml.md)
- QPA (Preview): [`../getting-started/qpa-proxy.md`](../getting-started/qpa-proxy.md)
- deployment: [`deployment.md`](deployment.md)
- compatibility: [`../../compatibility.md`](../../compatibility.md)
- known limitations: [`../../known-limitations.md`](../../known-limitations.md)