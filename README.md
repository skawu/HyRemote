<p align="center">
  <img src="logo/huayan-software-horizontal.png" alt="HyRemote by Huayan Software" width="420">
</p>

<h1 align="center">HyRemote</h1>

<p align="center"><strong>Qt Remote Access Framework</strong><br>
Add remote viewing and optional remote control to existing Qt Widgets and Qt Quick applications without rebuilding the application around a remote-desktop stack.</p>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-Apache--2.0-blue.svg" alt="License: Apache-2.0"></a>
  <img src="https://img.shields.io/badge/product-V0.1%20Developer%20Preview-orange.svg" alt="Product: V0.1 Developer Preview">
  <img src="https://img.shields.io/badge/reference-Windows%20%7C%20Linux%20x86__64-lightgrey.svg" alt="Reference: Windows | Linux x86_64">
  <img src="https://img.shields.io/badge/Qt-6.8.3%20reference-41CD52.svg" alt="Qt 6.8.3 reference">
  <img src="https://img.shields.io/badge/C%2B%2B-17-blue.svg" alt="C++17">
</p>

HyRemote provides one shared remote-access Runtime and four peer ways to enter it. Applications choose the integration style that best fits their ownership model; Widgets and Qt Quick are Runtime target types, not separate products.

| Integration | Application change | Current product status | Typical use |
| --- | --- | --- | --- |
| **C++ API** | Link `HyRemote::RemoteAccess` | **V0.1 primary** | Applications that want explicit lifecycle and policy control |
| **Generic Plugin** | No HyRemote application linkage | **V0.1 primary** | Existing Qt applications that need a zero-code remote-access path while keeping their native Qt platform |
| **QML API** | `import HyRemote` | **Preview** | Qt Quick applications that prefer declarative configuration |
| **QPA** | Launch with `-platform hyremote` | **Preview** | Specialized zero-code integration that requires an exact qualified Qt private ABI |

All four frontends converge on the same Runtime/Core implementation. There is no second Session, capture, transport, or input stack hidden behind a different integration mode.

## Quick start — C++ API

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)
find_package(HyRemote CONFIG REQUIRED)

target_link_libraries(MyApp PRIVATE
    Qt6::Widgets
    HyRemote::RemoteAccess
)
```

```cpp
#include <HyRemote/RemoteAccess.h>

HyRemote::RemoteAccess remote(&window);
remote.start();
```

Construction is inert. The default listener is `127.0.0.1:5921`, and remote input is disabled by default.

Deploy the application and the HyRemote Runtime through the package helper:

```cmake
install(TARGETS MyApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyApp)
```

The same public facade accepts supported `QWidget` and `QQuickWindow` targets.

## Quick start — Generic Plugin

The Generic Plugin keeps the application Qt-only and preserves the application's normal platform plugin (`qwindows`, `qxcb`, and so on).

The application itself does not link a HyRemote target:

```cmake
target_link_libraries(MyExistingApp PRIVATE Qt6::Widgets)

find_package(HyRemote CONFIG REQUIRED)
install(TARGETS MyExistingApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyExistingApp GENERIC)
```

Activate the plugin with Qt's generic-plugin mechanism, for example:

```text
MyExistingApp -plugin hyremote
```

or through `QT_QPA_GENERIC_PLUGINS=hyremote` where environment-based activation is more convenient.

The Generic Plugin is not a QPA replacement. The native Qt platform remains authoritative for the local window, display, input, and GPU integration.

See [`docs/getting-started/generic.md`](docs/getting-started/generic.md) for configuration options.

## QML API — Preview

```qml
import HyRemote

RemoteAccess {
    target: mainWindow
    enabled: true
}
```

The QML type is a thin frontend over the same Runtime used by the C++ API.

```cmake
hyremote_deploy(TARGET MyQmlApp QML)
```

**TODO (productization):** complete the final installed-SDK examples, documentation polish, and cross-version qualification before promoting QML from Preview.

## QPA — Preview

```cmake
find_package(HyRemote CONFIG REQUIRED)
install(TARGETS ExistingQtApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET ExistingQtApp QPA)
```

```text
ExistingQtApp -platform hyremote
```

The QPA frontend uses a Factory Trampoline and delegates to the qualified native Qt platform implementation rather than replacing it with an offscreen/qvnc-style backend. QPA uses Qt private ABI and is therefore qualified per exact Qt patch; the current reference is Qt 6.8.3.

**TODO (productization):** broaden qualification only after exact-version compatibility and physical local+remote coexistence are verified for the target Qt/OS pair.

## Product architecture

```text
C++ API ---------\
QML API ----------\
Generic Plugin ----> Shared Runtime -> Core -> RFB transport -> Viewer
QPA --------------/        |            ^
                           |            |
                    Widgets / Quick   normalized input
                    target adapters
```

The canonical source ownership mirrors this product model:

```text
src/
├── core/
├── runtime/
└── integrations/
    ├── cpp/
    ├── qml/
    ├── generic/
    └── qpa/
```

`HyRemote::RemoteAccess` / `HyRemoteRemoteAccess` is the normal shared Runtime artifact. Core remains internal; the QML, Generic, and QPA pieces are integration payloads over that Runtime.

## Build

The repository's normal developer build is driven by `build.cmd` and `build.yml`. `build.cmd` is the single build authority: `build.cmd build` configures if needed and compiles, `build.cmd install` materializes the product/SDK tree into `build/install/`, `build.cmd test` builds and runs the tests, `build.cmd clean` removes the tree and `build.cmd rebuild` is clean plus build. Everything under `build/` is developer intermediate output; everything under `build/install/` is what users and SDK consumers take. The checked-in profile enables all four integrations for development; integrations are independent selections and none implies another.

```text
PowerShell:  .\build.cmd build --show-config
POSIX:       sh ./build.cmd build --show-config
PowerShell:  .\build.cmd build --integrations=cpp,generic
POSIX:       sh ./build.cmd build --integrations=cpp,generic
PowerShell:  .\build.cmd test --integrations=cpp,qml,generic,qpa
POSIX:       sh ./build.cmd test --integrations=cpp,qml,generic,qpa
```

For SDK consumption, use `find_package(HyRemote CONFIG REQUIRED)` and `hyremote_deploy()` instead of manually copying internal libraries or plugins.

## Security model in V0.1

The shipped V0.2.0.0 candidate is a **user-first LAN trial with a truthful security boundary**:

- default bind: `0.0.0.0:5921` (reachable on the host's IPv4 interfaces), or one exact local IPv4, or a named interface;
- remote input: off by default;
- `Insecure` is unauthenticated and unencrypted: the listener is for a **trusted LAN only** and is **not Internet-safe**;
- `Authenticated` is conditional: it requires a transport-security-enabled HyRemote build plus a valid security descriptor, and currently provides VNC authentication without stream encryption;
- the default V0.1 build/profile does not imply authenticated transport is present;
- `AuthenticatedEncrypted` is not implemented and always fails closed before any listener is opened, without fallback to a weaker profile.

Do not expose the current product directly to the public Internet.

**TODO (V0.2):** encrypted transport, certificate policy, authenticated sessions, and production network policy.

See [`docs/security.md`](docs/security.md).

## Product roadmap

HyRemote is delivered progressively:

| Product line | Product promise |
| --- | --- |
| **V0.1 — Use It** | Developer Preview: C++ API + Generic Plugin as the primary paths |
| **V0.2 — Trust It** | Security, authenticated sessions, and production network behavior |
| **V0.3 — Productize It** | All four integrations fully packaged, deployed, documented, and taught |
| **V0.4 — Qualify It** | Compatibility, real-world applications, performance and release-candidate qualification |
| **V1.0 — Stabilize It** | First GA support contract |
| **V1.1+** | Embedded Linux/platform expansion, then measured hardware acceleration and advanced programmable control |

Version digits describe product evolution; they do not encode C++/QML/Generic/QPA, Qt version, UI family, or platform.

## Documentation

Repository paths are architecture boundaries rather than arbitrary folders: the canonical source layout and module
ownership are defined by [`docs/internal/repository-layout.md`](docs/internal/repository-layout.md).

User guides:

- [`docs/guide/install.md`](docs/guide/install.md) - install and reference setup;
- [`docs/guide/deployment.md`](docs/guide/deployment.md) - deployment forms, including `hyremote_deploy(TARGET MyApp GENERIC)`;
- [`docs/guide/viewer-connection.md`](docs/guide/viewer-connection.md) - connecting a viewer to a running application;
- [`docs/guide/troubleshooting.md`](docs/guide/troubleshooting.md) - diagnosing a deployment or connection problem;
- [`docs/getting-started/cpp.md`](docs/getting-started/cpp.md) - Embedded C++ (V0.1 primary surface);
- [`docs/getting-started/generic.md`](docs/getting-started/generic.md) - Generic Plugin (V0.1 primary surface);
- [`docs/getting-started/qml.md`](docs/getting-started/qml.md) - Declarative QML (V0.1 preview);
- [`docs/getting-started/qpa-proxy.md`](docs/getting-started/qpa-proxy.md) - Transparent QPA (V0.1 preview).

Start with [`docs/README.md`](docs/README.md).

Primary product guides:

- [`docs/getting-started/cpp.md`](docs/getting-started/cpp.md) — C++ API
- [`docs/getting-started/generic.md`](docs/getting-started/generic.md) — Generic Plugin
- [`docs/getting-started/qml.md`](docs/getting-started/qml.md) — QML API (Preview)
- [`docs/getting-started/qpa-proxy.md`](docs/getting-started/qpa-proxy.md) — QPA (Preview)
- [`docs/guide/deployment.md`](docs/guide/deployment.md) — packaging and deployment
- [`docs/security.md`](docs/security.md) — security behavior and limitations
- [`docs/compatibility.md`](docs/compatibility.md) — compatibility matrix
- [`docs/known-limitations.md`](docs/known-limitations.md) — known product limitations

## License

HyRemote is licensed under the **Apache License 2.0**. See [`LICENSE`](LICENSE) and [`NOTICE.md`](NOTICE.md).
