<p align="center">
  <img src="logo/huayan-software-horizontal.png" alt="HyRemote by Huayan Software" width="420">
</p>

<h1 align="center">HyRemote</h1>

<p align="center"><strong>Qt Embedded Remote Access Framework</strong><br>
Remote display and remote input for existing Qt Widgets and Qt Quick applications.</p>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-Apache--2.0-blue.svg" alt="License: Apache-2.0"></a>
  <img src="https://img.shields.io/badge/status-pre--alpha-orange.svg" alt="Status: pre-alpha">
  <img src="https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20x86__64-lightgrey.svg" alt="Platform: Windows | Linux x86_64">
  <img src="https://img.shields.io/badge/Qt-6.8.x-41CD52.svg" alt="Qt 6.8.x reference line">
  <img src="https://img.shields.io/badge/C%2B%2B-17-blue.svg" alt="C++17">
</p>

HyRemote adds remote display and optional remote input to existing Qt applications. The V1 product is
intentionally small from the application developer's point of view: use one shared C++ library, or use
the Transparent QPA plugin without linking HyRemote into the application at all.

> **Status: pre-alpha / V1.0.0.0 convergence.** Reference acceptance is pinned to Qt 6.8.3 on
> Windows x86_64 and Linux x86_64. A configuration is not called supported until its required evidence
> has executed successfully.

## Two primary ways to use HyRemote

### 1. C++: link one shared library

```cmake
find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(HyRemote CONFIG REQUIRED)

target_link_libraries(MyApp PRIVATE
    Qt6::Widgets
    HyRemote::RemoteAccess
)
```

The normal application code is intentionally small:

```cpp
#include <HyRemote/RemoteAccess.h>

HyRemote::RemoteAccess remote(&window);
remote.start();
```

`RemoteAccess` works with supported `QWidget` and `QQuickWindow` targets. Construction is inert,
loopback is the default listen address, port 5900 is the default port, and remote input is disabled by
default. Applications only call setters when they deliberately need non-default policy.

For installation/deployment, one helper deploys Qt plus the HyRemote shared runtime:

```cmake
install(TARGETS MyApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyApp)
```

The application does **not** link or deploy Core, Session, CaptureSource, InputSink, transport, or RFB
backend targets directly.

### 2. Transparent QPA: no HyRemote application linkage

The existing application stays an ordinary Qt application:

```cmake
target_link_libraries(MyApp PRIVATE Qt6::Widgets)
```

Packaging adds the HyRemote QPA plugin through the same installed SDK helper:

```cmake
find_package(HyRemote CONFIG REQUIRED)
install(TARGETS MyApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyApp QPA)
```

Launch it through the proxy platform plugin:

```text
MyApp -platform hyremote
```

This is the zero-source-change path. `qhyremote` preserves the native `qwindows` / `qxcb` delegate and
adds remote access; it is not a replacement-only qvnc-style platform backend. The application itself
does not link `HyRemote::RemoteAccess` or any QPA-private type.

### Declarative QML

V1 also includes `import HyRemote` as a thin declarative wrapper over the same C++ `RemoteAccess`
runtime. It is not a separate Session, transport, capture, or input implementation.

```qml
import HyRemote

RemoteAccess {
    target: mainWindow
    enabled: true
}
```

Deploy with:

```cmake
hyremote_deploy(TARGET MyQmlApp QML)
```

A QML application may also deliberately use Transparent QPA with `QML QPA`; those are orthogonal
integration options over the same product runtime, not a fourth product architecture.

## V1 artifact model

The product artifact shape is fixed so normal application use does not depend on `BUILD_SHARED_LIBS`:

| Artifact | V1 role |
| --- | --- |
| `HyRemote::RemoteAccess` / `HyRemoteRemoteAccess` | **Shared library**; the normal C++ application API |
| `qhyremote` | **Qt platform MODULE**; zero-source-change Transparent QPA entry point |
| `hyremote-core` | **Static internal composition library** behind `RemoteAccess`; not a normal runtime deployment unit |
| `HyRemote` QML module | Thin declarative wrapper over the same shared `RemoteAccess` runtime |

Backend selection, target adapters, normalized input, RFB transport and capture implementation remain
internal. Product evolution must not turn those implementation layers into mandatory application setup.

## Product principles

- **Qt Widgets and Qt Quick are peers.** Neither is a compatibility afterthought.
- **Local/native behavior remains authoritative.** QPA remote access is additive to the native platform
  delegate rather than replacing normal local display/input semantics.
- **C++ first.** The shared `RemoteAccess` facade is the reference product API; QML is a thin wrapper.
- **One runtime.** C++, QML and QPA reuse the same Core/session/transport implementation.
- **Safe defaults.** No listener on C++ construction, loopback bind by default, remote input off by
  default.
- **Evidence over claims.** Windows evidence does not substitute for Linux evidence, and hosted/Xvfb
  correctness does not substitute for required physical local+remote coexistence proof.
- **Internal complexity stays internal.** A normal user should not need to understand Session,
  CaptureSource, InputSink, transport adapters, surface composition or QPA interception internals.

## Status

| Version | Product milestone | State |
| --- | --- | --- |
| `V0.0.1.0` | Windows + Linux x86_64 — Embedded C++ API | acceptance pending |
| `V0.0.2.0` | Windows + Linux x86_64 — Declarative QML API | acceptance pending |
| `V0.0.3.0` | Windows + Linux x86_64 — Transparent QPA Proxy | acceptance pending |
| `V1.0.0.0` | Windows + Linux x86_64 GA — all three integration modes | convergence / acceptance pending |

V1 implementation is converging through the single GA integration candidate. Release authorization still
requires the milestone authorities and exact-platform automated/physical evidence defined in
[`docs/v1-ga-acceptance.md`](docs/v1-ga-acceptance.md).

## Requirements

| Requirement | V1 rule |
| --- | --- |
| C++ | C++17 |
| CMake | 3.21 or newer |
| Qt | Qt 6.8.x reference line; QPA V1 qualification is exact Qt 6.8.3 |
| Reference OS | Windows x86_64 and Linux x86_64 |
| Near-term embedded direction | Embedded Linux; Rockchip and NXP i.MX class devices are high-priority candidates after x86 V1 |
| OpenHarmony | Longer-term direction; not a V1 GA gate |

## Build from source

```bash
cmake -S . -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/<toolchain>
cmake --build build
ctest --test-dir build --output-on-failure
```

Transparent QPA is opt-in because it is coupled to Qt private ABI:

```bash
cmake -S . -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/<toolchain> \
      -DHYREMOTE_WITH_QPA_PROXY=ON
cmake --build build
```

Core-only development remains available without turning Core into an application-facing product path:

```bash
cmake -S . -B build-core -G Ninja \
      -DHYREMOTE_BUILD_REMOTE_ACCESS=OFF
cmake --build build-core
```

Important project options:

| CMake option | Default | Purpose |
| --- | --- | --- |
| `HYREMOTE_BUILD_CORE` | `ON` | build internal Qt-free Core |
| `HYREMOTE_BUILD_REMOTE_ACCESS` | `ON` | build shared public `HyRemote::RemoteAccess` facade |
| `HYREMOTE_BUILD_WIDGETS_ADAPTER` | `ON` | supported QWidget target path |
| `HYREMOTE_BUILD_QUICK_ADAPTER` | `ON` | supported QQuickWindow target path |
| `HYREMOTE_BUILD_QML_API` | `OFF` | build the optional declarative wrapper |
| `HYREMOTE_WITH_VNC` | `ON` | current internal V1 RFB correctness transport |
| `HYREMOTE_WITH_QPA_PROXY` | `OFF` | build exact-Qt Transparent QPA plugin |
| `HYREMOTE_BUILD_SPIKES` | `OFF` | architecture experiments, not product runtime |
| `HYREMOTE_WITH_GBM` | `OFF` | post-V1 experimental backend work |
| `HYREMOTE_WITH_RKMPP` | `OFF` | post-V1 experimental Rockchip encoder work |

## Deployment

`hyremote_deploy()` is the only HyRemote-owned application deployment entry point:

```cmake
hyremote_deploy(TARGET MyCppApp)
hyremote_deploy(TARGET MyQmlApp QML)
hyremote_deploy(TARGET ExistingQtApp QPA)
hyremote_deploy(TARGET ExistingQmlApp QML QPA)
```

The helper owns HyRemote runtime/module placement and composes with Qt's supported deployment tooling.
Normal installed applications should not need to name `HyRemoteRemoteAccess`, `qhyremote`, QML plugin
files, backend libraries, or `QT_PLUGIN_PATH` manually.

See [`docs/deployment.md`](docs/deployment.md).

## Repository layout

| Path | Contents |
| --- | --- |
| `core/` | Qt-free internal Session/frame/input/dispatch implementation |
| `remoteaccess/` | shared public C++ facade, Widgets/Quick adapters and internal transport |
| `qml/` | thin declarative wrapper |
| `qpa/` | exact-Qt native-delegate-preserving QPA proxy plugin |
| `examples/` | C++, QML, QPA and remote-support product examples |
| `tests/` | product/installed/source/deployment acceptance fixtures |
| `docs/` | architecture, support, security, deployment and release evidence |
| `spikes/` | non-production architecture experiments |

## Documentation

Start here:

- [`docs/getting-started/cpp.md`](docs/getting-started/cpp.md) — C++ shared-library integration
- [`docs/getting-started/qpa-proxy.md`](docs/getting-started/qpa-proxy.md) — zero-source-change QPA path
- [`docs/getting-started/qml.md`](docs/getting-started/qml.md) — declarative wrapper
- [`docs/deployment.md`](docs/deployment.md) — the single deployment helper
- [`docs/security.md`](docs/security.md) — deployment/security boundary
- [`docs/compatibility.md`](docs/compatibility.md) — exact configuration evidence
- [`docs/v1-ga-acceptance.md`](docs/v1-ga-acceptance.md) — GA release gates

Architecture and evidence details remain in [`docs/architecture.md`](docs/architecture.md),
[`docs/core-architecture.md`](docs/core-architecture.md), [`docs/adr/`](docs/adr), and the capture /
transport evaluation documents. Those are framework-maintainer material, not prerequisites for normal
application integration.

## Compatibility and support

A configuration is marked **supported** only after reproducible build and functional validation are
recorded for that exact environment. Windows does not substitute for Linux, x86 does not substitute for
embedded hardware, and hosted/headless execution does not substitute for a physical local-display gate
when one is required. See [`docs/compatibility.md`](docs/compatibility.md).

## Security

HyRemote is remote-access infrastructure. The current RFB SecurityType None correctness baseline is
**unauthenticated and unencrypted**; it is not an Internet-safe security mode. Loopback is the default
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
