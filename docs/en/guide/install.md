# Install, build and integration preparation

> Language / 语言: **English** | [中文](../../guide/install.md)

This guide covers obtaining HyRemote, building it, and integrating it into a Qt application (installed SDK or
source). It describes the final product shape; acceptance process, milestone tracking and internal engineering
conventions live elsewhere - see the "Internal documents" section of [`docs/README.md`](../README.md).

## 1. V1 reference support matrix

| Dimension | V1 commitment |
| --- | --- |
| Operating systems | Windows x86_64 and Linux x86_64 (desktop). Embedded Linux/EGLFS is **outside** the V1 support claim |
| Qt | **Exact Qt 6.8.3** is the reference version; the bounded RFB transport and all three integration modes are accepted against it |
| Compilers | Windows: MSVC x64 (C++17); Linux: GCC x86_64 (C++17) |
| Integration modes | (1) Embedded C++ (one shared library), (2) Declarative QML (`import HyRemote`), (3) Transparent QPA Proxy (`-platform hyremote`) |
| QPA constraint | Transparent QPA is exactly coupled to the Qt 6.8.3 private QPA ABI; it preserves the native `qwindows` / `qxcb` delegate |
| Viewer | Any standard VNC client, default `127.0.0.1:5900` |

> **Product status:** V1.0.0.0 acceptance is pending. The candidate implementation is not a **Supported** claim
> until the required hosted and physical evidence actually passes. Current status:
> [`docs/compatibility.md`](../../compatibility.md).

## 2. Two acquisition paths: exactly one per CMake configure

| Path | Use it when | Entry point |
| --- | --- | --- |
| **Installed SDK** | You consume a prebuilt/installed prefix shared by a team | `find_package(HyRemote CONFIG REQUIRED)` |
| **Source** | Vendored alongside the application, cross-building, or modifying HyRemote | `add_subdirectory(third_party/HyRemote)` |

Both paths expose the **same** application API, artifact shape and deployment helper; only the acquisition step
differs. A single CMake configure must choose exactly **one** acquisition: either one installed package prefix or
one source/add-subdirectory tree. Do not call `find_package(HyRemote)` and `add_subdirectory(HyRemote)` in the
same configure, and do not combine two different installed HyRemote prefixes - V1 fails those mixed cases closed
instead of pairing one runtime target with QML/QPA metadata from another HyRemote build. Repeating
`find_package(HyRemote)` for the same installed prefix remains valid.

## 3. Building HyRemote itself

### 3.1 Plain product build

A plain configure is intentionally product-only: it builds the standard C++
`HyRemote::RemoteAccess` path and does not build repository tests, examples or research code.

Windows (x64 MSVC developer environment):

```bat
cmake -S . -B build -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64
cmake --build build --parallel
```

Linux:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/opt/Qt/6.8.3/gcc_64
cmake --build build --parallel
```

### 3.2 Creating an installed SDK

```bash
cmake -S . -B build-hyremote -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/<toolchain> \
  -DCMAKE_INSTALL_PREFIX=/path/to/hyremote-sdk
cmake --build build-hyremote --parallel
cmake --install build-hyremote
```

The resulting prefix is consumed with `find_package(HyRemote CONFIG REQUIRED)` as in section 4.

### 3.3 Optional integration packages

QML and Transparent QPA are optional payloads:

```text
-DHYREMOTE_BUILD_QML_API=ON      # provides import HyRemote
-DHYREMOTE_WITH_QPA_PROXY=ON     # provides the qhyremote platform plugin (requires exact Qt 6.8.3 and the matching private Gui development package)
```

Normal C++ use needs neither, and needs no Qt private development package. Enabling them does not enlarge the
normal C++ link surface.

### 3.4 Maintainer / acceptance build

Repository validation is explicit rather than hidden in the normal product build:

```text
-DHYREMOTE_BUILD_TESTS=ON
-DHYREMOTE_BUILD_EXAMPLES=ON
```

When running **build-tree** tests, make the Qt and HyRemote build-tree runtime directories discoverable (this is a
build-tree test concern, not the deployment contract):

```bat
:: Windows
set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%CD%\build-test\remoteaccess;%PATH%
ctest --test-dir build-test --output-on-failure
```

```bash
# Linux (only when the local kit does not already provide suitable runtime lookup)
export LD_LIBRARY_PATH=/opt/Qt/6.8.3/gcc_64/lib:${LD_LIBRARY_PATH}
ctest --test-dir build-test --output-on-failure
```

## 4. Consumption A: installed SDK

### 4.1 Contract

The V1 installed SDK exposes **one normal C++ product target**: `HyRemote::RemoteAccess` (a shared library).

- Core is statically composed behind the facade and is **not installed/exported** as a V1 SDK target;
- the QML module is consumed through `import HyRemote`; its backing library is not a second C++ SDK target;
- Transparent QPA is consumed through `hyremote_deploy(... QPA)` and is **not** an application link target (the
  installed SDK does not export `HyRemote::QpaPlatform`).

Application code does not discover or link internal Core/transport/capture/input/QPA targets individually.

### 4.2 Minimal application

```bash
cmake -S . -B build \
  -DCMAKE_PREFIX_PATH="/path/to/Qt/6.8.3/<toolchain>;/path/to/hyremote-sdk"
```

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)
find_package(HyRemote CONFIG REQUIRED)

target_link_libraries(MyApp PRIVATE Qt6::Widgets HyRemote::RemoteAccess)
```

```cpp
#include <HyRemote/RemoteAccess.h>

HyRemote::RemoteAccess remote(&window);
remote.start();
```

Defaults are the safe defaults: loopback listener, port 5900, remote input disabled. Setters are optional policy
controls, not mandatory setup. For Qt Quick, request the application's normal Qt Quick components and keep the
same HyRemote product target; `find_package(HyRemote)` does not force an application to resolve Widgets, Quick or
QML modules it does not use.

### 4.3 What `find_package(HyRemote)` means

`HyRemoteConfig.cmake` resolves the public dependencies of `HyRemote::RemoteAccess`, loads that exported product
target, publishes optional QML/QPA payload metadata, and exposes `hyremote_deploy()`.

It does **not** turn Core, the QML backing library or `qhyremote` into application link choices. It is generated by
HyRemote install/export rules and is not expected to exist merely because a source directory was added to another
project.

## 5. Consumption B: source

Source consumption suits vendored, cross-built or modified trees; it does **not** require or imply
`find_package(HyRemote)` for the HyRemote source tree itself.

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)

add_subdirectory(third_party/HyRemote)

add_executable(MyApp main.cpp)
target_link_libraries(MyApp PRIVATE Qt6::Widgets HyRemote::RemoteAccess)
```

When HyRemote is included as a subproject, its own tests, examples and research spikes default to **OFF**
automatically; applications do not need to know or override those developer-only switches.
`add_subdirectory(... EXCLUDE_FROM_ALL)` is supported: HyRemote's source-tree payloads remain internal build
targets, the deployment helper adds only the local build dependencies needed to materialize the selected payload,
and it does **not** turn those targets into application link dependencies - a Transparent QPA application remains
Qt-only even when `qhyremote` and `HyRemoteRemoteAccess` are built from source.

Opt into extra integration packages only when needed:

```cmake
set(HYREMOTE_BUILD_QML_API ON CACHE BOOL "" FORCE)       # import HyRemote
set(HYREMOTE_WITH_QPA_PROXY ON CACHE BOOL "" FORCE)      # exact Qt 6.8.3 qhyremote plugin
add_subdirectory(third_party/HyRemote EXCLUDE_FROM_ALL)
```

These options do not create alternate Core/transport stacks: QML remains a thin wrapper and QPA remains a plugin
entry point over the same shared runtime. Source consumption inherits the caller's compiler, sysroot, CMake
toolchain file and target Qt SDK, which is why the future Embedded Linux families extend this model - and why
they must not introduce a second project-specific build system or a more complex application API.

## 6. Deployment: `hyremote_deploy()`

One HyRemote-owned entry point owns deployment:

```cmake
hyremote_deploy(TARGET MyCppApp)          # Embedded C++
hyremote_deploy(TARGET MyQmlApp QML)      # Declarative QML
hyremote_deploy(TARGET ExistingQtApp QPA) # Transparent QPA
hyremote_deploy(TARGET ExistingQmlApp QML QPA)  # QML + QPA composition
```

```cmake
install(TARGETS MyApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyApp)
```

The helper owns placement of the selected payload (shared runtime, Qt dependencies, QML plugin files,
`qhyremote`). Applications must **not** manually copy DLL/SO, `qmldir` or plugin files, and must not set
SDK-specific `QT_PLUGIN_PATH` / `QT_QPA_PLATFORM_PLUGIN_PATH` / `LD_LIBRARY_PATH` overrides. Full contract:
[`docs/deployment.md`](../../deployment.md).

## 7. Clean deployment requirement

A successful build is **not** sufficient evidence. The deployed application must run from its deployment prefix
without depending on the HyRemote build tree, the original SDK prefix, or manual `PATH`, `LD_LIBRARY_PATH`,
`QT_PLUGIN_PATH`, `QT_QPA_PLATFORM_PLUGIN_PATH`, `QML2_IMPORT_PATH` or `QML_IMPORT_PATH` workarounds.

On Linux the V1 fixtures additionally verify where the HyRemote/Qt shared objects are actually resolved from, so a
surviving SDK/build-tree RUNPATH cannot masquerade as a clean deployment. On Windows, clean runtime checks narrow
`PATH` to the deployed application plus required system directories.

## 8. Platform notes

| Topic | Windows x86_64 | Linux x86_64 |
| --- | --- | --- |
| Reference Qt | `C:\Qt\6.8.3\msvc2022_64` | `/opt/Qt/6.8.3/gcc_64` |
| Build-tree test support | Put Qt `bin` and the build tree's `remoteaccess` directory on `PATH` | Set `LD_LIBRARY_PATH` when needed |
| QPA native delegate | `qwindows` | `qxcb` |
| Offscreen/software rendering | Validates the protocol-to-application path; it is **not** evidence of local-visible display/input coexistence | Same (`offscreen` and similar) |

Desktop Linux results do not imply Embedded Linux/EGLFS support. Final platform acceptance must record the exact
Qt/compiler/QPA/viewer configuration.

## 9. Examples and viewer

With `-DHYREMOTE_BUILD_EXAMPLES=ON` the V1 candidate contains `widgets-basic`, `quick-basic`, `qml-basic`,
`qpa-proxy-existing-app` and `remote-support-showcase`, according to the enabled integration packages.

E1/E2 use the same public `HyRemote::RemoteAccess` facade and are view-only by default; the explicit remote-input
option is for controlled test environments only. Viewer connection:
[`docs/viewer-connection.md`](../../viewer-connection.md).

## 10. Security and support boundary

The current bounded RFB correctness transport uses **SecurityType None**: no transport authentication and no
transport encryption. The listener defaults to loopback and remote input is off by default - do not expose that
baseline to an untrusted network or the public Internet. See [`docs/security.md`](../../security.md) and
[`SECURITY.md`](../../../SECURITY.md).

The support boundary follows evidence: a hosted/offscreen build is not by itself proof of local-visible
display/input coexistence with remote access. A capability that has not passed acceptance is not described as
Supported; explicit limitations are in [`docs/known-limitations.md`](../../known-limitations.md).

## 11. Related documents

- Integration modes (choose one): [Embedded C++](../../getting-started/cpp.md) | [Declarative QML](../../getting-started/qml.md) | [Transparent QPA](../../getting-started/qpa-proxy.md) (these move into `guide/` with the rest of the migration)
- Deployment and packaging: [`deployment.md`](../../deployment.md)
- Compatibility and limits: [`compatibility.md`](../../compatibility.md) | [`known-limitations.md`](../../known-limitations.md)
- Troubleshooting: [`troubleshooting.md`](../../troubleshooting.md)
