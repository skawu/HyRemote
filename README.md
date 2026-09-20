<p align="center">
  <img src="assets/logo/huayan-software-horizontal.png" alt="HyRemote by Huayan Software" width="420">
</p>

<h1 align="center">HyRemote</h1>

<p align="center"><strong>Qt Remote Access Framework</strong><br>
Remote display and optional remote input for Qt Widgets and Qt Quick applications.<br>
V1 reference platforms: Windows x86_64 and Linux x86_64.</p>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-Apache--2.0-blue.svg" alt="License: Apache-2.0"></a>
  <img src="https://img.shields.io/badge/status-V1%20convergence-orange.svg" alt="Status: V1 convergence">
  <img src="https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20x86__64-lightgrey.svg" alt="Platform: Windows | Linux x86_64">
  <img src="https://img.shields.io/badge/Qt-6.8.3%20reference-41CD52.svg" alt="Qt 6.8.3 reference">
  <img src="https://img.shields.io/badge/C%2B%2B-17-blue.svg" alt="C++17">
</p>

HyRemote keeps the application-facing model deliberately small. V1 has three mandatory integration modes:

1. **Embedded C++ API:** link one shared library, `HyRemote::RemoteAccess`;
2. **Declarative QML API:** `import HyRemote` and use the thin `RemoteAccess` wrapper over the same C++ runtime;
3. **Transparent QPA Proxy:** keep the application Qt-only and launch it with `-platform hyremote`.

Qt Widgets and Qt Quick are first-class peers. The three integration modes share one remote-access runtime architecture rather than creating separate Session/transport stacks.

> **Release status:** V1.0.0.0 acceptance is pending. The current reference matrix is Windows x86_64 + Linux x86_64 with Qt 6.8.3. Candidate implementation is not a Supported claim until the required executable and physical evidence actually passes.

## C++ — one shared library

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

HyRemote::RemoteAccess remote(&window);
remote.start();
```

That is the normal integration. Construction is inert, the listener defaults to `127.0.0.1:5921`, and remote input is disabled by default.

The same facade supports qualified `QWidget` and `QQuickWindow` targets. Applications do not assemble Core, Session, capture, input, transport or RFB objects.

Deploy with one call:

```cmake
install(TARGETS MyApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyApp)
```

The helper carries the shared `HyRemoteRemoteAccess` runtime and its Qt runtime dependencies.

## Declarative QML

```qml
import HyRemote

RemoteAccess {
    target: mainWindow
    enabled: true
}
```

`enabled: true` is applied after QML component construction, so users do not need `Component.onCompleted` startup glue. The wrapper reuses the same shared C++ runtime.

Deploy with:

```cmake
hyremote_deploy(TARGET MyQmlApp QML)
```

The QML backing library is payload, not another consumer C++ target.

## Transparent QPA — zero HyRemote application linkage

An existing Qt application remains Qt-only:

```cmake
target_link_libraries(MyApp PRIVATE Qt6::Widgets)
```

Use the HyRemote package only for deployment:

```cmake
find_package(HyRemote CONFIG REQUIRED)
install(TARGETS MyApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyApp QPA)
```

Run it through the proxy platform plugin:

```text
MyApp -platform hyremote
```

No HyRemote source/API call is required in the application. `qhyremote` preserves the qualified native `qwindows` / `qxcb` delegate and adds remote access; it is not a replacement-only qvnc-style backend.

The installed SDK does **not** expose `HyRemote::QpaPlatform` as an application link target. Plugin/runtime placement belongs to `hyremote_deploy()`.

## Product artifacts

| Artifact | V1 role |
| --- | --- |
| `HyRemote::RemoteAccess` / `HyRemoteRemoteAccess` | **Shared library**; the one normal C++ product target |
| `HyRemote` QML module | Thin declarative payload over the same shared runtime |
| `qhyremote` | Qt platform **MODULE payload**; Transparent QPA entry point |
| `hyremote-core` | Internal static source component; not installed/exported as an application SDK target |

`BUILD_SHARED_LIBS` does not create alternate V1 product personalities.

## Build

A plain source build is intentionally product-only: tests and examples are not built unless explicitly requested.

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/<toolchain>
cmake --build build --parallel
```

To create an installed SDK:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/<toolchain> \
  -DCMAKE_INSTALL_PREFIX=/path/to/hyremote-sdk
cmake --build build --parallel
cmake --install build
```

Optional V1 integration packages:

```text
-DHYREMOTE_BUILD_QML_API=ON
-DHYREMOTE_WITH_QPA_PROXY=ON
```

Transparent QPA requires exact Qt 6.8.3 and the matching private Gui development package. Normal C++ use does not.

Maintainers/CI enable repository validation explicitly:

```text
-DHYREMOTE_BUILD_TESTS=ON
-DHYREMOTE_BUILD_EXAMPLES=ON
```

See the install guide ([中文](docs/guide/install.md) ｜ [English](docs/en/guide/install.md)) for reference test commands. Build-tree runtime path overrides are development details, not deployment requirements.

## Repository layout

The repository is organized by product responsibility rather than historical feature branches. Read the top level as
one question - does it ship?:

```text
src/                 the shipping tree: everything built and delivered, one directory per deliverable
  core/                internal static library (not installed, not linkable)
  remoteaccess/        the one shared library: HyRemote::RemoteAccess
  qml/HyRemote/        the `import HyRemote` payload
  qpa/                 the `qhyremote` Qt platform plugin payload
examples/            usage examples E1-E6; not shipped
docs/                documentation, zoned by reader (see docs/README.md)
tests/               tests only: cross-module integration (unit tests live with their module)
verification/        verification of the delivered product: consumers, E2E, contract, third-party matrix, release gates
cmake/               build, package and deployment modules
assets/logo/         the product logo, used in documentation and the UI
.github/             CI and repository governance
```

The full specification - every directory, what it is for, and where new work belongs - is
[`docs/internal/repository-layout.md`](docs/internal/repository-layout.md).

The source move does not intentionally change build-tree artifact paths; CMake maps canonical source directories onto the established `build/core`, `build/remoteaccess`, `build/qml/HyRemote` and QPA output locations. See [`docs/internal/repository-layout.md`](docs/internal/repository-layout.md).

## Deployment

`hyremote_deploy()` is the single HyRemote-owned deployment entry point:

```cmake
hyremote_deploy(TARGET MyCppApp)
hyremote_deploy(TARGET MyQmlApp QML)
hyremote_deploy(TARGET ExistingQtApp QPA)
hyremote_deploy(TARGET ExistingQmlApp QML QPA)
```

Normal deployed applications should not manually copy HyRemote DLL/SO/plugin files or set SDK-specific `QT_PLUGIN_PATH` / `LD_LIBRARY_PATH` overrides.

See [`docs/guide/deployment.md`](docs/guide/deployment.md).

## Examples

The V1 candidate contains:

- `examples/widgets-basic` — Embedded C++ / Widgets;
- `examples/quick-basic` — Embedded C++ / Quick;
- `examples/qml-basic` — Declarative QML;
- `examples/qpa-proxy-existing-app` — ordinary Qt application + Transparent QPA;
- `examples/remote-support-showcase` — operator-controlled remote-support workflow;
- clean installed/source consumer fixtures under `verification/` for SDK acceptance.

`-DHYREMOTE_BUILD_EXAMPLES=ON` builds the examples for product modes enabled in the current configuration. The standard C++ configuration therefore builds the C++ Widgets/Quick examples without requiring QML or QPA.

For the complete V1 three-mode example matrix, use:

```text
-DHYREMOTE_BUILD_EXAMPLES=ON
-DHYREMOTE_BUILD_QML_API=ON
-DHYREMOTE_WITH_QPA_PROXY=ON
```

Formal pre-GA release versions enforce their cumulative milestone profile and reject later integration modes until those modes reach their own release milestone.

## Documentation

Choose the application mode first:

- [`docs/getting-started/cpp.md`](docs/getting-started/cpp.md) — Embedded C++
- [`docs/getting-started/qml.md`](docs/getting-started/qml.md) — Declarative QML
- [`docs/getting-started/qpa-proxy.md`](docs/getting-started/qpa-proxy.md) — Transparent QPA

Reference/setup and delivery guides:

- [`docs/guide/install.md`](docs/guide/install.md) — build, platform setup, installed SDK and source consumption (中文; [English](docs/en/guide/install.md))
- [`docs/README.md`](docs/README.md) — documentation index (中文 ｜ [English](docs/en/README.md))
- [`docs/qml-consumption.md`](docs/qml-consumption.md) — installed QML module
- [`docs/guide/deployment.md`](docs/guide/deployment.md) — packaging/deployment
- [`docs/internal/repository-layout.md`](docs/internal/repository-layout.md) — canonical repository ownership/layout
- [`docs/guide/viewer-connection.md`](docs/guide/viewer-connection.md) — viewer/control/reconnect
- [`docs/security.md`](docs/security.md) — implemented security boundary
- [`docs/guide/troubleshooting.md`](docs/guide/troubleshooting.md) — product-level diagnosis
- [`docs/compatibility.md`](docs/compatibility.md) — exact evidence/status matrix
- [`docs/known-limitations.md`](docs/known-limitations.md) — explicit V1 limitations
- [`docs/internal/v1-ga-acceptance.md`](docs/internal/v1-ga-acceptance.md) — GA release gate

Maintainer material under `docs/internal/**` (implementation history, evaluation records, runbooks) is not needed to integrate HyRemote.

## Security

The current bounded RFB correctness transport uses **SecurityType None**: no transport authentication and no transport encryption. Loopback is the default bind and remote input is off by default.

Do **not** expose the current baseline directly to an untrusted network or the public Internet. See [`docs/security.md`](docs/security.md) and [`SECURITY.md`](SECURITY.md).

## V1 release discipline

Milestone tags are release facts, not progress markers, and the pre-GA profiles are cumulative:

| Milestone | Tag | Released product surface | Current state |
| --- | --- | --- | --- |
| Embedded C++ | `v0.0.1.0` | C++ | acceptance pending |
| Declarative QML | `v0.0.2.0` | C++ + QML | acceptance pending |
| Transparent QPA | `v0.0.3.0` | C++ + QML + QPA | acceptance pending |
| GA / all three modes | `v1.0.0.0` | C++ + QML + QPA | acceptance pending |

Development uses version sentinel `0.0.0`. Formal release branches enforce the milestone product surface, then merge to `main` before an annotated tag is created on the exact accepted main HEAD.

No tag is created from unexecuted hosted jobs. Physical local-display/local-input + remote coexistence evidence remains a separate acceptance gate where required.

## License

HyRemote is licensed under the **Apache License 2.0** — see [`LICENSE`](LICENSE). Third-party components remain subject to their own licenses and attribution requirements.
