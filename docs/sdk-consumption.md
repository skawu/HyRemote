# HyRemote SDK Consumption Contract

Status: **frozen product contract; implementation tracked by #39**

Product milestone: **V0.0.1.0 — x86_64 (Windows + Linux) / Embedded C++ API**

HyRemote is an independent SDK/framework for Qt applications. It is not installed by modifying the Qt SDK tree and it does not pretend to be a built-in Qt module. The product requirement is that, after HyRemote is installed or added as source once, using it should feel comparable to using a normal Qt module.

This document is the user-facing consumption authority. Internal Core, capture, input, transport, protocol and hardware choices must converge on this contract rather than leak into application code.

## 1. Normal user mental model

A normal application developer should think in terms of only four things:

1. add/find HyRemote;
2. link the product target;
3. attach remote access to a Qt window/target;
4. start/stop remote access.

The developer should not need to understand or instantiate:

- `Session`;
- `RemoteFrame`;
- `CaptureSource`;
- `Transport`;
- `InputSink`;
- Core mailbox/backpressure internals;
- NeatVNC, rustvncserver or any future protocol backend;
- Tokio/Rust runtime details;
- platform acceleration backends.

Those are implementation details owned by HyRemote.

## 2. Distribution model

HyRemote supports two first-class consumption paths that expose the same public API.

### 2.1 Prebuilt SDK — primary x86 path

Windows x86_64 and Linux x86_64 releases provide a standalone install prefix conceptually shaped as:

```text
HyRemote/<version>/
├── include/
│   └── HyRemote/
│       └── RemoteAccess.h
├── lib/
│   ├── <HyRemote libraries>
│   └── cmake/
│       └── HyRemote/
│           ├── HyRemoteConfig.cmake
│           ├── HyRemoteConfigVersion.cmake
│           └── HyRemoteTargets.cmake
├── bin/                         # platform/runtime payload where applicable
├── qml/                         # introduced with V0.0.2.0
│   └── HyRemote/
└── licenses/
```

The SDK must not require copying files into the user's Qt installation tree.

### 2.2 Source consumption — open-source and cross-build path

The repository must support normal CMake source integration, including vendored `add_subdirectory()` and a documented acquisition path such as `FetchContent` where appropriate.

Source integration inherits the caller's:

- compiler/toolchain;
- sysroot;
- Qt target SDK;
- CMake configuration;
- platform-specific cross-build environment.

Embedded Linux and future OpenHarmony support extend this model instead of introducing a separate project-specific build system.

## 3. Public CMake contract

The normal installed-SDK experience should be approximately:

```cmake
find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(HyRemote CONFIG REQUIRED)

target_link_libraries(MyApp PRIVATE
    Qt6::Widgets
    HyRemote::RemoteAccess
)
```

For Qt Quick applications the application selects its normal Qt modules, while the HyRemote product target remains the same:

```cmake
find_package(Qt6 REQUIRED COMPONENTS Quick)
find_package(HyRemote CONFIG REQUIRED)

target_link_libraries(MyApp PRIVATE
    Qt6::Quick
    HyRemote::RemoteAccess
)
```

`HyRemote::RemoteAccess` is the normal product-level aggregate target. Internal implementation targets may exist, but getting-started documentation must not require application developers to manually compose Core, capture, input or VNC targets.

## 4. Public C++ API facade

The common path for Widgets and Quick must stay concise.

Conceptual Widgets usage:

```cpp
#include <HyRemote/RemoteAccess.h>

MainWindow window;
HyRemote::RemoteAccess remote(&window);
remote.start();
window.show();
```

Conceptual Qt Quick usage:

```cpp
#include <HyRemote/RemoteAccess.h>

QQuickWindow *window = /* application window */;
HyRemote::RemoteAccess remote(window);
remote.start();
```

The exact constructor/setter/error API is implementation-reviewed, but these product invariants are frozen:

- one public facade model for Widgets and Quick;
- construction alone does not open a listener;
- `start()`/`stop()` are explicit;
- bind address and port are configurable without protocol-backend types;
- remote input is independently controllable from remote viewing;
- diagnostics/errors use HyRemote product types, not backend types;
- replacing the default VNC backend does not require normal application-source changes.

## 5. QML extension — V0.0.2.0

The QML API is a declarative wrapper over the same product/Core semantics, not a second implementation stack.

Target usage:

```qml
import HyRemote

RemoteAccess {
    target: mainWindow
    enabled: true
}
```

The module is delivered using normal Qt reusable-QML-module conventions. Users should not manually manage backend libraries or internal QML import paths in the documented deployment path.

## 6. Deployment contract

Installed-SDK usage must provide a deployment mechanism, conceptually:

```cmake
hyremote_deploy(TARGET MyApp)
```

The exact command may be refined during implementation, but HyRemote owns deployment of its required runtime payload, including:

- HyRemote runtime libraries;
- the selected default transport backend/runtime;
- QML module/plugin payload when used;
- required third-party notices/licenses where applicable.

The normal user must not manually discover or copy backend DLL/SO files or hand-edit `PATH`, runtime search paths or QML import paths merely to follow the supported getting-started path.

## 7. Backend/toolchain isolation

Transport and acceleration backends are selected for product fit, not only technical capability.

A backend-specific extra toolchain must not silently become a normal-user prerequisite.

For a Rust/Cargo implementation, for example:

- prebuilt Windows/Linux SDK users must not install/configure Rust;
- any source-build Rust requirement must be explicit, optional where practical, and justified by product benefit;
- embedded cross-compilation cost is part of the backend selection decision;
- Rust/Tokio types never cross the normal HyRemote public API.

The same principle applies to any future system library, hardware SDK or protocol implementation.

## 8. Product acceptance examples

Examples are product acceptance artifacts, not architecture demos.

### V0.0.1.0

Required:

```text
examples/widgets-basic/
examples/quick-basic/
```

Both examples must:

- consume only the public product facade;
- avoid direct use of Core/Transport/CaptureSource/InputSink internals;
- build on Windows x86_64 and Linux x86_64 in the claimed configurations;
- demonstrate remote view and remote input;
- demonstrate disconnect/reconnect;
- keep local rendering and local input functional.

### V0.0.2.0

Add a QML example that uses `import HyRemote` and the declarative `RemoteAccess` surface.

## 9. Required user documentation

Before V0.0.1.0 completion the repository/release must provide:

1. Windows getting started;
2. Linux getting started;
3. prebuilt SDK installation and `find_package(HyRemote)`;
4. source consumption (`add_subdirectory` and supported acquisition workflow);
5. C++ API reference/getting-started usage;
6. runtime deployment;
7. VNC viewer connect/view/control walkthrough;
8. safe listener and remote-input defaults;
9. compatibility matrix;
10. known limitations and release notes.

Before V0.0.2.0 completion add QML module/import usage documentation.

Documentation examples must match the released product API; internal engineering snippets are not a substitute for user documentation.

## 10. Comparison target with Qt modules

HyRemote cannot remove the fact that it is a separately acquired SDK, but after acquisition the expected developer flow is intentionally similar to a Qt module:

```text
Install/add HyRemote once
        ↓
find_package(HyRemote)
        ↓
link HyRemote::RemoteAccess
        ↓
use HyRemote::RemoteAccess / import HyRemote
        ↓
deploy through supported CMake packaging
```

The extra acquisition step is acceptable. Additional manual knowledge of HyRemote's protocol/capture/backend internals is not.

## 11. Definition of success

The SDK/user-consumption design succeeds when an external Qt developer can add remote access to a normal Widgets or Quick application using the documented product target and concise public API, build and deploy it reproducibly, and never need to understand which VNC/capture/input implementation HyRemote selected internally.
