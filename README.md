<p align="center">
  <img src="logo/huayan-software-horizontal.png" alt="HyRemote by Huayan Software" width="420">
</p>

<h1 align="center">HyRemote</h1>

<p align="center"><strong>Qt Remote Access Framework</strong><br>
Remote display and optional remote input for Qt Widgets and Qt Quick applications.<br>
V1 reference platforms: Windows x86_64 and Linux x86_64.</p>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-Apache--2.0-blue.svg" alt="License: Apache-2.0"></a>
  <img src="https://img.shields.io/badge/status-V0.1%20Developer%20Preview-orange.svg" alt="Status: V0.1 Developer Preview">
  <img src="https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20x86__64-lightgrey.svg" alt="Platform: Windows | Linux x86_64">
  <img src="https://img.shields.io/badge/Qt-6.8.3%20reference-41CD52.svg" alt="Qt 6.8.3 reference">
  <img src="https://img.shields.io/badge/C%2B%2B-17-blue.svg" alt="C++17">
</p>

HyRemote keeps the application-facing model deliberately small. The architecture has four peer integration frontends:

1. **Embedded C++ API:** link one shared library, `HyRemote::RemoteAccess`;
2. **Declarative QML API:** `import HyRemote` and use the thin `RemoteAccess` wrapper over the same C++ runtime;
3. **Generic Plugin:** a public Qt generic plugin, activated with `-plugin hyremote`, that leaves the application's native Qt platform integration untouched;
4. **Transparent QPA Proxy:** keep the application Qt-only and launch it with `-platform hyremote`.

Qt Widgets and Qt Quick are first-class peers. The four frontends share one remote-access runtime architecture rather than creating separate Session/transport stacks.

> **Release status:** V0.1.0.0 Developer Preview, acceptance pending. V0.1 promises two primary surfaces - **Embedded C++** and the **Generic Plugin** - while the Declarative QML and Transparent QPA frontends are preview surfaces with a narrower support contract. The current reference matrix is Windows x86_64 + Linux x86_64 with Qt 6.8.3. This is not a GA or production-ready claim until the required executable and physical evidence actually passes.

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
  core/                BASE       internal static library; not installed, not linkable by a payload
  runtime/             RUNTIME    the one shared remote-access runtime every frontend is built on
  integrations/        FRONTENDS  four peer integration frontends, one directory each
    cpp/               C++        the public `HyRemote::RemoteAccess` facade
    qml/               QML        provides `import HyRemote`
    generic/           GENERIC    the public Qt generic plugin, `-plugin hyremote`
    qpa/               QPA        provides `qhyremote`, Qt 6.8.3-qualified
tests/               tests and product verification: clean consumers, product E2E, the public-surface contract,
                     the third-party matrix and the release gates. Unit tests live with the module they
                     qualify, in src/*/tests/, so they are never here.
examples/            usage examples E1-E6; not shipped
docs/                documentation, zoned by reader (see docs/README.md)
logo/                the product mark, referenced by the documentation and the examples; never built
cmake/               build, package and deployment modules
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
- clean installed/source consumer fixtures under `tests/` for SDK acceptance.

`-DHYREMOTE_BUILD_EXAMPLES=ON` builds the examples for product modes enabled in the current configuration. The standard C++ configuration therefore builds the C++ Widgets/Quick examples without requiring QML or QPA.

For the complete example matrix across all four frontends, use:

```text
-DHYREMOTE_BUILD_EXAMPLES=ON
-DHYREMOTE_BUILD_QML_API=ON
-DHYREMOTE_WITH_GENERIC_PLUGIN=ON
-DHYREMOTE_WITH_QPA_PROXY=ON
```

A release version does not select which frontends may exist: frontend enablement, support level and preview/qualified/supported status are release-train scope and compatibility decisions. The release-profile validator refuses only the retired `V0.0.x` planning labels.

## Documentation

Choose the frontend you consume first:

- [`docs/getting-started/cpp.md`](docs/getting-started/cpp.md) — Embedded C++
- [`docs/getting-started/generic.md`](docs/getting-started/generic.md) — Generic Plugin, including `hyremote_deploy(TARGET MyApp GENERIC)`
- [`docs/getting-started/qml.md`](docs/getting-started/qml.md) — Declarative QML (V0.1 preview)
- [`docs/getting-started/qpa-proxy.md`](docs/getting-started/qpa-proxy.md) — Transparent QPA (V0.1 preview)

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

The bounded RFB correctness transport uses **SecurityType None** unless an authenticated profile is configured: `SecurityType None` carries no transport authentication and is refused beyond loopback, while a configured authenticated profile authenticates the viewer with **RFB VNC authentication** (security type 2). Transport encryption is not provided - TLS is a separate, later step - so a listener beyond loopback still belongs behind an appropriate trusted access boundary. Loopback is the default bind and remote input is off by default.

Do **not** expose the current baseline directly to an untrusted network or the public Internet. See [`docs/security.md`](docs/security.md) and [`SECURITY.md`](SECURITY.md).

## Release discipline

Milestone tags are release facts, not progress markers. Delivery before first GA is progressive, and every train is a real public release with a deliberately narrower support contract than GA:

| Train | User promise | State |
| --- | --- | --- |
| `v0.1.0.0` | Use It / Developer Preview - loopback-only, with Embedded C++ and the Generic Plugin as the primary surfaces | acceptance pending |
| `v0.2.0.0` | Trust It / Operational Preview - security, session and network | not started |
| `v0.3.0.0` | Productize It / Product Preview - four product-deliverable integrations, deployment, examples | not started |
| `v0.4.0.0` | Qualify It / Release Candidate line, with `V0.4.0.x` maintenance releases | not started |
| `v1.0.0.0` | Stabilize It / first GA, promoted from one mature V0.4 lineage | not started |

The retired `v0.0.1.0` / `v0.0.2.0` / `v0.0.3.0` planning labels are not release trains and are refused by the release-profile validator. The machine-readable train map, including each train's authority and closeable mandatory children, is `.github/release/release-trains.json`.

Development uses version sentinel `0.0.0`. A release is prepared on `release/vX.Y.Z.W`, merged to `main`, and only then tagged with an annotated tag on that exact accepted main HEAD.

No tag is created from unexecuted hosted jobs. Physical local-display/local-input + remote coexistence evidence remains a separate acceptance gate where required.

## License

HyRemote is licensed under the **Apache License 2.0** — see [`LICENSE`](LICENSE). Third-party components remain subject to their own licenses and attribution requirements.
