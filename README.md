<p align="center">
  <img src="logo/huayan-software-horizontal.png" alt="HyRemote by Huayan Software" width="420">
</p>

<h1 align="center">HyRemote</h1>

<p align="center"><strong>Qt Embedded Remote Access Framework</strong><br>
Remote display and optional remote input for existing Qt Widgets and Qt Quick applications.</p>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-Apache--2.0-blue.svg" alt="License: Apache-2.0"></a>
  <img src="https://img.shields.io/badge/status-V1%20convergence-orange.svg" alt="Status: V1 convergence">
  <img src="https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20x86__64-lightgrey.svg" alt="Platform: Windows | Linux x86_64">
  <img src="https://img.shields.io/badge/Qt-6.8.x-41CD52.svg" alt="Qt 6.8.x reference line">
  <img src="https://img.shields.io/badge/C%2B%2B-17-blue.svg" alt="C++17">
</p>

HyRemote is designed to be simple to consume even though the implementation supports multiple Qt UI
families and remote-access internals. V1 has two primary user paths:

1. **C++:** link one shared library, `HyRemote::RemoteAccess`;
2. **Transparent QPA:** keep the application Qt-only and launch with `-platform hyremote`.

Declarative QML is a thin wrapper over the same C++ runtime, not a second implementation stack.

> **Release status:** V1.0.0.0 acceptance is still pending. The current reference matrix is Qt 6.8.3
> on Windows x86_64 and Linux x86_64. No configuration is called supported until its required
> executable evidence has actually run and passed.

## C++: one shared library

```cmake
find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(HyRemote CONFIG REQUIRED)

target_link_libraries(MyApp PRIVATE
    Qt6::Widgets
    HyRemote::RemoteAccess
)
```

Normal application code is intentionally small:

```cpp
#include <HyRemote/RemoteAccess.h>

HyRemote::RemoteAccess remote(&window);
remote.start();
```

The same facade supports qualified `QWidget` and `QQuickWindow` targets. Construction is inert,
loopback is the default listen address, port 5900 is the default port, and remote input is disabled by
default.

Package the application with one HyRemote deployment call:

```cmake
install(TARGETS MyApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyApp)
```

The installed product carries the shared `HyRemoteRemoteAccess` runtime automatically. Applications do
not link or deploy Core, Session, CaptureSource, InputSink, transport, or RFB backend targets.

## Transparent QPA: no HyRemote application linkage

An existing Qt application remains Qt-only:

```cmake
target_link_libraries(MyApp PRIVATE Qt6::Widgets)
```

Use the installed HyRemote package only for deployment:

```cmake
find_package(HyRemote CONFIG REQUIRED)
install(TARGETS MyApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyApp QPA)
```

Launch normally through the proxy platform plugin:

```text
MyApp -platform hyremote
```

`qhyremote` preserves the qualified native `qwindows` / `qxcb` delegate and adds remote access. It is
not a replacement-only qvnc-style platform backend, and the application itself does not link
`HyRemote::RemoteAccess` or QPA-private types.

## Declarative QML

V1 also provides:

```qml
import HyRemote

RemoteAccess {
    target: mainWindow
    enabled: true
}
```

The QML type wraps the same shared `RemoteAccess` runtime. Deploy it with:

```cmake
hyremote_deploy(TARGET MyQmlApp QML)
```

A QML application may also deliberately use Transparent QPA with `QML QPA`; this composes two entry
mechanisms over the same runtime rather than creating a fourth architecture.

## V1 product artifacts

| Artifact | Role |
| --- | --- |
| `HyRemote::RemoteAccess` / `HyRemoteRemoteAccess` | **Shared library**; normal C++ product API |
| `qhyremote` | **Qt platform MODULE**; zero-source-change Transparent QPA entry point |
| `HyRemote` QML module | Thin declarative wrapper over the same shared runtime |
| `hyremote-core` | Internal static source component; **not installed/exported as a V1 application SDK target** |

`BUILD_SHARED_LIBS` does not change the normal V1 product shape.

## Product rules

- **C++ first.** `HyRemote::RemoteAccess` is the reference application API.
- **Qt Widgets and Qt Quick are peers.** Neither is a compatibility afterthought.
- **One runtime.** C++, QML and QPA reuse the same runtime composition.
- **Native behavior stays authoritative.** QPA remote access is additive to the native platform path.
- **Safe defaults.** No listener on construction, loopback bind by default, remote input off by default.
- **Internal complexity stays internal.** Normal users do not assemble Session, capture, input,
  transport, surface-composition or QPA-interception objects.
- **Evidence over claims.** Windows does not substitute for Linux; hosted/headless correctness does
  not substitute for physical local-display/input evidence when that evidence is required.

## V1 scope

| Version | Milestone | State |
| --- | --- | --- |
| `V0.0.1.0` | Windows + Linux x86_64 — Embedded C++ API | acceptance pending |
| `V0.0.2.0` | Windows + Linux x86_64 — Declarative QML API | acceptance pending |
| `V0.0.3.0` | Windows + Linux x86_64 — Transparent QPA Proxy | acceptance pending |
| `V1.0.0.0` | Windows + Linux x86_64 GA — all three modes | convergence / acceptance pending |

Embedded Linux, Rockchip/NXP targets and hardware-accelerated paths are post-V1 expansion work unless
an actual V1 blocker proves otherwise. OpenHarmony is longer-term work, not a V1 GA gate.

## Requirements

- C++17
- CMake 3.21+
- Qt 6.8.x for public-API V1 work
- exact Qt 6.8.3 for the V1 Transparent QPA package
- Windows x86_64 or Linux x86_64 for the V1 reference matrix

## Build HyRemote

```bash
cmake -S . -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/<toolchain>
cmake --build build
ctest --test-dir build --output-on-failure
```

To create an installed SDK:

```bash
cmake -S . -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/<toolchain> \
      -DCMAKE_INSTALL_PREFIX=/path/to/hyremote-sdk
cmake --build build
cmake --install build
```

Optional V1 integration packages are enabled only when needed:

```text
-DHYREMOTE_BUILD_QML_API=ON
-DHYREMOTE_WITH_QPA_PROXY=ON
```

Maintainer-only backend/spike options are documented in the architecture and development documents;
they are not part of normal application setup.

## Deployment

`hyremote_deploy()` is the single HyRemote-owned application deployment entry point:

```cmake
hyremote_deploy(TARGET MyCppApp)
hyremote_deploy(TARGET MyQmlApp QML)
hyremote_deploy(TARGET ExistingQtApp QPA)
hyremote_deploy(TARGET ExistingQmlApp QML QPA)
```

Normal deployed applications should not need to copy HyRemote runtime/plugin files manually or set
`QT_PLUGIN_PATH` just to locate `qhyremote`.

See [`docs/deployment.md`](docs/deployment.md).

## Documentation

Start with the integration path you need:

- [`docs/getting-started/cpp.md`](docs/getting-started/cpp.md) — C++ shared-library integration
- [`docs/getting-started/qpa-proxy.md`](docs/getting-started/qpa-proxy.md) — Transparent QPA
- [`docs/getting-started/qml.md`](docs/getting-started/qml.md) — declarative QML
- [`docs/deployment.md`](docs/deployment.md) — packaging/deployment
- [`docs/security.md`](docs/security.md) — security boundary
- [`docs/compatibility.md`](docs/compatibility.md) — exact evidence/status matrix
- [`docs/v1-ga-acceptance.md`](docs/v1-ga-acceptance.md) — V1 release gate

Architecture, Core contracts, capture/transport evidence and experimental backends remain maintainer
material under [`docs/`](docs/) and [`spikes/`](spikes/); they are not prerequisites for normal use.

## Security

HyRemote is remote-access infrastructure. The current RFB SecurityType None correctness baseline is
**unauthenticated and unencrypted** and is not an Internet-safe security mode. Loopback is the default
bind and remote input is disabled by default. See [`docs/security.md`](docs/security.md) and
[`SECURITY.md`](SECURITY.md).

## Contributing

Keep product-facing changes small and preserve the public boundary. New implementation layers must not
force ordinary applications to assemble Core, Session, capture, input, transport, or backend objects.
See [`CONTRIBUTING.md`](CONTRIBUTING.md).

## License

HyRemote is licensed under the **Apache License 2.0** — see [`LICENSE`](LICENSE). Third-party
components remain subject to their own licenses and attribution requirements.

## Brand and trademarks

<p align="center">
  <img src="logo/huayan-logo-single.png" alt="Huayan brand mark" width="96">
</p>

The image files are distributed with this repository under Apache-2.0. Apache License 2.0 section 6 does
not grant trademark rights: copying or redistributing them does not grant permission to present a fork,
derivative or unrelated product as the owner of these marks.
