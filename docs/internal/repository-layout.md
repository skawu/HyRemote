# Repository Layout

HyRemote's repository structure follows product architecture and ownership, not the historical order in which features were implemented.

## Canonical top-level layout

The repository is divided into shipping product code, product verification, user examples, documentation, build tooling and CI/governance:

```text
HyRemote/
├─ CMakeLists.txt
├─ compile.cmd / clean.cmd
├─ README.md / CONTRIBUTING.md / SECURITY.md
├─ NOTICE.md / LICENSE
│
├─ src/                          # shipping/product implementation tree
│  ├─ core/                      # shared UI/protocol/platform-neutral core
│  ├─ runtime/                   # shared Qt/product runtime implementation
│  ├─ cpp/                       # Embedded C++ integration frontend
│  ├─ qml/                       # QML integration frontend
│  ├─ generic/                   # QGenericPlugin integration frontend (post-V1 migration target)
│  └─ qpa/                       # QPA integration frontend / private compatibility shim
│
├─ tests/                        # delivered-product verification and cross-module tests
├─ examples/                     # user-facing examples
├─ docs/                         # contracts, guides, ADRs, evidence and maintainer docs
├─ logo/                         # product branding only; never a build input
├─ cmake/                        # build/package/deployment/toolchain modules
└─ .github/                      # CI and repository governance
```

`src/generic/` becomes required only when the fourth integration frontend is implemented by #219. During the migration, the repository-layout gate requires the layers already present and the accepted ADR records the final six-directory `src/` state.

## Product architecture represented by the tree

HyRemote has two shared implementation layers and four peer application integration frontends:

```text
cpp --------\
qml ---------> runtime -> core
generic ----/
qpa --------/
```

The arrows above mean "depends on" from frontend toward shared implementation. No frontend is the parent implementation of another frontend.

### `src/core`

`src/core` is the shared Core implementation. It owns product semantics that remain independent of Qt UI technology, concrete transport and platform implementation:

- Session lifecycle/state/error semantics;
- `RemoteFrame` metadata and storage lifetime;
- normalized input contracts;
- bounded mailbox/scheduling/backpressure;
- target/transport-neutral interfaces and capabilities.

Core remains ordinary C++ and must not require QWidget, Qt Quick/QML, Qt private/QPA, concrete RFB types, graphics-platform APIs or SoC/vendor APIs. It is internal composition, not an installed application SDK target.

### `src/runtime`

`src/runtime` owns implementation shared by multiple/all integration frontends that cannot live in UI-neutral Core:

- Widgets and Quick target adapters;
- concrete RFB transport and transport-security implementation;
- component factories that assemble Core interfaces with product implementations;
- application-surface discovery/model;
- composite capture/input routing;
- automatic-access controller used by Generic Plugin and QPA zero-code modes.

Runtime may depend on Qt public APIs and Core. It must not depend on `src/cpp`, `src/qml`, `src/generic` or `src/qpa`.

During #219 the existing `HyRemoteRemoteAccess` target/output name is intentionally preserved while source ownership moves into `src/runtime`. Artifact/API changes are separate compatibility decisions and are not implied by source relocation.

### `src/cpp`

`src/cpp` is **Embedded C++ integration**, not the common implementation parent. It owns the public `HyRemote::RemoteAccess` facade/header and Embedded C++ integration semantics. Product work is delegated into Runtime/Core.

### `src/qml`

`src/qml` is the QML integration frontend providing `import HyRemote`. It is a thin application-facing payload over the same Runtime/Core and must not create a second Session/capture/input/transport architecture.

### `src/generic`

`src/generic` is the fourth integration frontend introduced by #219. It provides a `QGenericPlugin` zero-code payload (`-plugin hyremote` / `QT_QPA_GENERIC_PLUGINS`) using Qt public APIs. It preserves the application's native QPA/platform identity and bootstraps the shared automatic runtime.

It must not include/link Qt private/QPA interfaces.

### `src/qpa`

`src/qpa` is the QPA integration frontend and the only frontend permitted to depend on Qt private/QPA ABI. Its target design is a Factory Trampoline:

```text
-platform hyremote
    -> parse HyRemote/QPA configuration
    -> resolve qualified native Qt delegate
    -> arm shared automatic runtime
    -> QPlatformIntegrationFactory::create(native delegate)
    -> return native QPlatformIntegration*
```

QPA must not own a second capture/input/transport/session implementation. Rockchip/NXP/TI-specific QPA product personalities are forbidden; native display/input/GPU adaptation belongs to the selected Qt QPA and vendor Qt/BSP.

## Shipping-tree rules

`src/` is the product implementation/shipping tree. New work follows ownership rather than convenience:

| New thing | Placement |
| --- | --- |
| Core lifecycle/frame/input/backpressure abstraction | `src/core/` |
| shared Qt target adapter, concrete transport/security, automatic surface/composition implementation | `src/runtime/` |
| Embedded C++ facade/API | `src/cpp/` |
| QML facade/payload | `src/qml/` |
| Generic Plugin payload | `src/generic/` |
| QPA/private compatibility/bootstrap | `src/qpa/` |
| cross-module or delivered-product verification | `tests/` |
| unit/private-detail test | beside the module it qualifies under `src/<module>/tests/` |
| user-facing example | `examples/` |
| architecture decision | `docs/adr/` |
| release/acceptance evidence | `docs/acceptance/` |

A frontend may depend on Runtime. Runtime may depend on Core. Runtime/Core may never depend on an integration frontend.

Only QPA may include Qt private/QPA headers. Generic, C++, QML, Runtime and Core must stay on public Qt/product interfaces appropriate to their layer.

## Stable artifact and binary paths

Source ownership and artifact paths are separate contracts. The architecture migration preserves the established `HyRemoteRemoteAccess` target/output and existing deployment paths until an explicit compatibility decision changes them.

The first #219 migration step intentionally enters the common runtime from the existing `src/cpp` build mapping so `build/remoteaccess` remains stable:

```cmake
add_subdirectory(src/core core)
add_subdirectory(src/cpp remoteaccess)
```

and `src/cpp/CMakeLists.txt` composes `../runtime` while contributing only the Embedded C++ facade. A later direct root mapping for `src/runtime` is allowed only as a deliberate build-layout change with CI/deployment evidence.

The existing root mappings for optional payloads remain explicit:

```cmake
add_subdirectory(src/qml qml/HyRemote)
add_subdirectory(src/qpa qpa)
```

When Generic Plugin becomes an implemented payload it must receive its own explicit stable binary mapping and package/deploy metadata.

## Tests and verification

Unit/private-detail tests stay with the module whose implementation they qualify. Root `tests/` validates the delivered product from outside the internal target graph, including installed/source consumers, E2E, public API contracts, third-party application lanes and release-readiness gates.

As implementation moves from `src/cpp` or `src/qpa` into `src/runtime`, the corresponding private tests must migrate with the implementation rather than remaining permanently under the old frontend merely for historical reasons.

Generic and QPA automatic-integration tests must share a cross-mode contract proving the same surface/composition/input/lifecycle semantics. Generic additionally verifies native `platformName()`/QPA preservation; QPA additionally verifies delegate/private-ABI behavior.

## V1 release boundary

The frozen V1 candidate/release evidence path is not rewritten merely to make it match this post-V1 architecture. #219 is developed on its dedicated architecture branch/PR. Only a proven release-blocking defect may justify a bounded change to the frozen V1 candidate.

This distinction allows the product's technical direction to move forward without invalidating accepted V1 evidence.

## Documentation zones

- `docs/guide/**` and `docs/getting-started/**`: user-facing documentation (Chinese primary, English mirror under `docs/en/**`);
- top-level `docs/*.md`: final-state product contracts;
- `docs/internal/**`: maintainer/release/process documentation;
- `docs/adr/**`: architecture decisions;
- `docs/releases/**`: release notes;
- `docs/acceptance/**`: recorded acceptance evidence;
- `docs/proposals/**`: proposal-era input, not present-day authority.

## Examples

`examples/` contains user-facing examples only. Examples are opt-in where appropriate, do not own product logic, and never vendor third-party source as a substitute for proper dependency/integration ownership.

The fourth Generic Plugin integration will add a corresponding existing-application example when the payload is implemented. Existing V1 examples remain evidence for the frozen V1 model until the new architecture is accepted for a later release.

## Research and branding

There is no product `research/` build tree. Experiment conclusions belong in documented evaluation/ADR records; accepted implementation moves into `src/` under the correct owner.

`logo/` contains product branding only. No `src/` or CMake product module may depend on branding assets.

## Forbidden legacy directories

Historical root/module names must not return:

```text
core/
runtime/
remoteaccess/
qml/
qpa/
integrations/
verification/
assets/
research/
spikes/

src/remoteaccess/
src/embedded/
src/declarative/
src/transparent/
docs/assets/
```

Do not add compatibility copies, symlinks or forwarding directories at historical locations. Update references to canonical ownership instead.

## Forward growth

The permanent growth rules are:

- Core grows by product-neutral abstractions and semantics;
- Runtime grows by shared Qt/product implementation;
- frontends grow only integration-specific bootstrap/facade code;
- QPA compatibility remains a narrow exact/private-ABI-qualified seam;
- platform acceleration is a separate evidence-driven backend concern and does not create SoC-specific QPA architectures;
- new installed payloads get their own sibling directory and explicit stable build/package mapping;
- a new top-level repository directory is a structural architecture decision, not a convenience refactor.

ADR-0007 (`docs/adr/0007-four-integration-runtime-architecture.md`) is the decision authority for the four-integration migration. `tests/release-readiness/check_repository_layout.cmake` is the executable guard for the physical/build ownership rules implemented at each migration phase.
