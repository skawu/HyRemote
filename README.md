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
  <img src="https://img.shields.io/badge/CMake-3.21%2B-064f8c.svg" alt="CMake 3.21+">
</p>

<p align="center">
  <a href="https://github.com/skawu/HyRemote/actions/workflows/remoteaccess-facade.yml"><img src="https://github.com/skawu/HyRemote/actions/workflows/remoteaccess-facade.yml/badge.svg" alt="RemoteAccess facade CI"></a>
  <a href="https://github.com/skawu/HyRemote/actions/workflows/sdk-consumption.yml"><img src="https://github.com/skawu/HyRemote/actions/workflows/sdk-consumption.yml/badge.svg" alt="SDK consumption CI"></a>
  <a href="https://github.com/skawu/HyRemote/actions/workflows/widgets-adapter.yml"><img src="https://github.com/skawu/HyRemote/actions/workflows/widgets-adapter.yml/badge.svg" alt="Widgets adapter CI"></a>
  <a href="https://github.com/skawu/HyRemote/actions/workflows/quick-adapter.yml"><img src="https://github.com/skawu/HyRemote/actions/workflows/quick-adapter.yml/badge.svg" alt="Quick adapter CI"></a>
</p>

HyRemote is an open-source remote access framework for Qt applications. It adds remote display and
remote input to existing Qt Widgets and Qt Quick applications without forcing an application to adopt a
single integration model, rendering stack, transport, or SoC-specific implementation.

> **Status: pre-alpha.** The Core contract, public C++ facade, standalone SDK/source-consumption path,
> Widgets/Quick target adapters and bounded x86 RFB correctness transport are in place. Product examples,
> complete dual-OS acceptance, the declarative QML mode and the Transparent QPA Proxy are still being
> productized toward `V1.0.0.0`. See [Status](#status).

- [Highlights](#highlights)
- [Status](#status)
- [Requirements](#requirements)
- [Build from source](#build-from-source)
- [Use HyRemote in an application](#use-hyremote-in-an-application)
- [Repository layout](#repository-layout)
- [Documentation](#documentation)
- [Compatibility and support](#compatibility-and-support)
- [Contributing](#contributing)
- [Security](#security)
- [License](#license)
- [Brand and trademarks](#brand-and-trademarks)

## Highlights

- **Three product integration modes**: Embedded C++ API (reference), Declarative QML API and
  Transparent QPA Proxy. The QPA mode is a version-coupled proxy, not a replacement-only platform
  plugin, and stays outside the stable Core.
- **Qt Widgets and Qt Quick are peers** - neither is a compatibility afterthought. Quick3D,
  `QOpenGLWidget` and `QQuickWidget` capture paths have recorded per-configuration evidence.
- **Local + remote coexistence** is a product requirement for every supported integration mode.
- **Backend-neutral architecture**: target adapters, capture backends, input paths, transports and
  hardware encoders are replaceable implementation layers; the public product API does not expose them.
- **Qt-like consumption**: an installed SDK (`find_package(HyRemote CONFIG REQUIRED)`) or normal CMake
  source consumption, with a small `RemoteAccess` facade for `QWidget`/`QQuickWindow` targets.
- **Safer defaults**: constructing `RemoteAccess` opens no listener; remote input is opt-in and
  independently controlled; the lifecycle is explicit.
- **Evidence over claims**: a configuration is marked supported only after a reproducible build,
  functional validation and a documented limitation list are recorded.
- **Apache-2.0**, with third-party components kept behind reviewable boundaries.

## Status

| Version | Product milestone | State |
| --- | --- | --- |
| `V0.0.1.0` | x86_64 (Windows + Linux) - Embedded C++ API | in progress |
| `V0.0.2.0` | x86_64 (Windows + Linux) - Declarative QML API | in progress |
| `V0.0.3.0` | x86_64 (Windows + Linux) - Transparent QPA Proxy, local + remote | in progress |
| `V1.0.0.0` | x86_64 GA - all three integration modes productized | planned |

The Core session/frame/dispatch contract, the `RemoteAccess` facade, install/export package,
Widgets/Quick capture and input adapters, and the bounded cross-platform RFB 3.8 correctness baseline
are merged. Product examples and final acceptance evidence remain active; QML and QPA implementation
proceed in parallel without redefining the shared runtime semantics. Execution order:
[`docs/development-roadmap.md`](docs/development-roadmap.md); version semantics:
[`docs/versioning.md`](docs/versioning.md).

## Requirements

The support envelope follows the product roadmap rather than one development machine: the x86_64
reference platforms first, embedded Linux next, and the same framework serving both the installed-SDK and
the source-consumption workflows.

| Requirement | Notes |
| --- | --- |
| C++17 toolchain | A conforming C++17 toolchain is required. A compiler/version becomes a support claim only after its configuration is recorded in [`docs/compatibility.md`](docs/compatibility.md) |
| CMake | 3.21 or newer; Ninja is commonly used by the project but is not the product architecture boundary |
| Qt | **Qt 6.8.x is the current V1.0 reference/support line.** Other Qt lines remain unverified unless explicitly listed in the compatibility matrix; do not infer a standing `6.8+` support promise from the CMake minimum |
| Operating systems | **x86_64 reference: Windows and Linux** - both are gated for every pre-GA milestone, and one does not substitute for the other. Near-term embedded baseline: **Embedded Linux**, with Rockchip and NXP i.MX class platforms as current high-priority candidates. **OpenHarmony** is a long-term direction and deliberately not a V1.0 delivery gate |
| Optional toolchains and SDKs | Only for explicitly enabled experimental/spike components. Production x86 consumers do not require a Rust toolchain; GBM/DMA-BUF and RKMPP remain opt-in experimental backends (`HYREMOTE_WITH_GBM`, `HYREMOTE_WITH_RKMPP`, both `OFF` by default) |
| Support claims | A platform or integration mode counts as supported only with recorded evidence; per-configuration status is in [`docs/compatibility.md`](docs/compatibility.md) and version semantics in [`docs/versioning.md`](docs/versioning.md) |

The three product integration modes (Embedded C++, Declarative QML, Transparent QPA Proxy) are part of the
V1.0 product scope. The C++ and QML modes stay on Qt public APIs, while the QPA proxy is version-coupled -
Qt does not guarantee QPA compatibility - and is therefore validated per exact Qt/OS combination.

## Build from source

```bash
cmake -S . -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/<toolchain>
cmake --build build
ctest --test-dir build --output-on-failure
```

The Qt `bin` directory must be reachable when the tests run, otherwise the Qt-linked test binaries
abort with `STATUS_DLL_NOT_FOUND` on Windows: set it in the same shell before `ctest`
(`set PATH=<Qt>\bin;%PATH%` on Windows, `export LD_LIBRARY_PATH=<Qt>/lib:$LD_LIBRARY_PATH` on Linux).

Core-only builds (no Qt required):

```bash
cmake -S . -B build-core -G Ninja -DCMAKE_BUILD_TYPE=Release -DHYREMOTE_BUILD_REMOTE_ACCESS=OFF
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

The throwaway architecture spike harnesses are excluded by default:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DHYREMOTE_BUILD_SPIKES=ON
cmake --build build
ctest --test-dir build -R "spike|async-spike"
```

| CMake option | Default | Purpose |
| --- | --- | --- |
| `HYREMOTE_BUILD_CORE` | `ON` | Qt-free `hyremote-core` session/frame/dispatch library |
| `HYREMOTE_BUILD_REMOTE_ACCESS` | `ON` | public `HyRemote::RemoteAccess` facade (skipped with a status message on a Qt-less top-level build) |
| `HYREMOTE_BUILD_WIDGETS_ADAPTER` | `ON` | Qt Widgets target adapter (built when Qt Widgets is available) |
| `HYREMOTE_BUILD_QUICK_ADAPTER` | `ON` | Qt Quick target adapter (built when Qt Quick is available) |
| `HYREMOTE_BUILD_TESTS` | `ON` | Core/RemoteAccess test suites registered with CTest |
| `HYREMOTE_BUILD_EXAMPLES` | `ON` | product examples when present in the current milestone branch |
| `HYREMOTE_BUILD_SPIKES` | `OFF` | throwaway spike harnesses under `spikes/` (not part of the production graph) |
| `HYREMOTE_WITH_VNC` | `ON` | internal bounded RFB 3.8 correctness transport used by the x86 product path |
| `HYREMOTE_WITH_QPA_PROXY` | `OFF` | Transparent QPA Proxy mode (version-coupled, isolated from Core) |
| `HYREMOTE_WITH_GBM` | `OFF` | experimental GBM/DMA-BUF-oriented backends |
| `HYREMOTE_WITH_RKMPP` | `OFF` | experimental Rockchip MPP encoder backend |

## Use HyRemote in an application

After installing HyRemote (or adding it as a source subdirectory), consumption is the same as for a Qt
module:

```cmake
find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(HyRemote CONFIG REQUIRED)

target_link_libraries(MyApp PRIVATE
    Qt6::Widgets
    HyRemote::RemoteAccess
)
```

```cpp
#include <HyRemote/RemoteAccess.h>

// Construction has no side effects: no listener is opened until start().
HyRemote::RemoteAccess remote(window);
remote.setListenAddress(QHostAddress(QHostAddress::LocalHost));
remote.setPort(5900);
remote.setRemoteInputEnabled(true);  // remote input is opt-in

if (!remote.start())
    return handleError(remote.lastError());

// ... inspect with remote.state(), remote.target(), remote.listenAddress(), remote.port()
remote.stop();
```

`QWidget` and `QQuickWindow` targets use the same product-level facade; internal `Session`,
`RemoteFrame`, capture, input and transport objects are not part of the application contract. The
declarative QML form (`import HyRemote; RemoteAccess { target: mainWindow }`) is the V0.0.2.0 integration
mode and wraps the same runtime rather than creating a second stack.

- Consumption contract: [`docs/sdk-consumption.md`](docs/sdk-consumption.md)
- External consumer fixtures: [`tests/consumer-installed-sdk`](tests/consumer-installed-sdk),
  [`tests/consumer-source`](tests/consumer-source)

## Repository layout

| Path | Contents |
| --- | --- |
| `core/` | `hyremote-core`: session, `RemoteFrame`, mailbox/dispatch and capability contract. Qt-free, protocol-free and covered by the deterministic test suite |
| `remoteaccess/` | public product facade (`HyRemote::RemoteAccess`), target adapters, internal x86 transport and tests |
| `tests/` | external consumption fixtures: installed-SDK consumer and source consumer |
| `spikes/` | throwaway architecture evidence harnesses: `capture`, `async-capture`, `vnc-transport-rust-ffi` |
| `docs/` | architecture, ADRs, product/roadmap, SDK, security, compatibility and spike evidence |
| `cmake/` | project options, install/export and deployment helpers |
| `logo/` | project and brand assets |

## Documentation

| Document | Purpose |
| --- | --- |
| [`product-overview.md`](docs/product-overview.md) | Long-form product description: integration modes, design principles, roadmap detail, non-goals |
| [`architecture.md`](docs/architecture.md) | Top-level boundaries and layer model |
| [`core-architecture.md`](docs/core-architecture.md) | Implementation-level Core semantics |
| [`adr/`](docs/adr) | Accepted decisions: core boundaries, `RemoteFrame` lifetime/timestamps, threading/backpressure |
| [`sdk-consumption.md`](docs/sdk-consumption.md) | Frozen user-facing consumption contract |
| [`versioning.md`](docs/versioning.md), [`development-roadmap.md`](docs/development-roadmap.md) | Product versions and execution order toward V1.0.0.0 |
| [`input-model.md`](docs/input-model.md) | Normalized remote input contract |
| [`security-model.md`](docs/security-model.md) | Security requirements and trust boundaries |
| [`compatibility.md`](docs/compatibility.md) | Recorded status per configuration |
| [`dependency-policy.md`](docs/dependency-policy.md) | Dependency and license review rules |
| [`widgets-capture.md`](docs/widgets-capture.md), [`capture-spike.md`](docs/capture-spike.md), [`async-capture-spike.md`](docs/async-capture-spike.md) | Capture evidence for Widgets/Quick/Quick3D/OpenGLWidget/QQuickWidget |
| [`neatvnc-evaluation.md`](docs/neatvnc-evaluation.md), [`x86-vnc-transport-evaluation.md`](docs/x86-vnc-transport-evaluation.md) | Transport backend evaluation and bounded selection evidence |

## Compatibility and support

A configuration is marked **supported** only with a reproducible build, functional validation, a
compatibility entry and documented limitations; everything else is `experimental`, `unsupported` or
`unverified`, and no support claim should be inferred. Windows evidence does not substitute for Linux
evidence (or the reverse), and x86 desktop validation is not a substitute for an embedded-platform
claim. Current per-configuration status: [`docs/compatibility.md`](docs/compatibility.md).

## Contributing

Issues and pull requests are welcome. Non-trivial changes start with an Issue describing the problem,
the affected layers and application types, public/private API impact, the validation plan and known
limitations; a PR should be focused, linked to its Issue and include the tests and platforms actually
validated. The full rules - Core boundaries, integration modes, dependency review, commit style and
license expectations - are in [`CONTRIBUTING.md`](CONTRIBUTING.md).

## Security

HyRemote is remote-access infrastructure: a working viewer is not by itself a safe deployment. The
framework's security requirements (service enablement, listen address, view/input authorization,
transport authentication, client lifecycle, deployment guidance) are defined in
[`docs/security-model.md`](docs/security-model.md). Report vulnerabilities as described in
[`SECURITY.md`](SECURITY.md) instead of opening a public Issue.

## License

HyRemote is licensed under the **Apache License 2.0** - see [`LICENSE`](LICENSE). Third-party
components remain subject to their own licenses and attribution requirements.

Unless a contribution is explicitly marked otherwise and accepted under a compatible license,
contributions intentionally submitted for inclusion are provided under the Apache License 2.0,
consistent with Section 5 of that license.

## Brand and trademarks

<p align="center">
  <img src="logo/huayan-logo-single.png" alt="Huayan brand mark" width="96">
</p>

| Asset | Project use |
| --- | --- |
| [`logo/huayan-software-horizontal.png`](logo/huayan-software-horizontal.png) | Primary repository/project lockup, rendered at the top of this README |
| [`logo/huayan-logo-single.png`](logo/huayan-logo-single.png) | Compact brand mark for square/icon placements and project documentation |

The image files are distributed with this repository under Apache-2.0. Apache License 2.0 section 6 does
**not** grant trademark rights: copying or redistributing the files does not grant permission to present
a fork, derivative or unrelated product as the owner of these marks. Downstream products should use
their own branding unless separately authorized.
