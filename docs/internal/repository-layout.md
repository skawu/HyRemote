# Repository Layout

HyRemote's repository structure follows current product architecture and ownership, not the historical order in which features were implemented.

## Canonical top-level layout

```text
HyRemote/
├─ AGENTS.md                     # repository execution entry for agents
├─ CMakeLists.txt
├─ build.cmd / build.yml         # canonical build/test/install authority
├─ README.md / CONTRIBUTING.md / SECURITY.md
├─ NOTICE.md / LICENSE
│
├─ src/                          # shipping/product implementation tree
│  ├─ core/                      # shared UI/protocol/platform-neutral Core
│  ├─ runtime/                   # one Shared Runtime
│  └─ integrations/              # four peer application integration frontends
│     ├─ cpp/                    # C++ public API frontend
│     ├─ qml/                    # QML frontend
│     ├─ generic/                # QGenericPlugin public-Qt zero-code frontend
│     └─ qpa/                    # QPA private-ABI/platform-entry frontend
│
├─ tests/                        # delivered-product verification and cross-module tests
│  └─ preflight/                 # bounded technical preflight fixtures where needed
├─ examples/                     # user-facing examples/reference applications
├─ docs/                         # contracts, guides, ADRs, evidence and maintainer docs
├─ logo/                         # product branding only; never a product-code dependency
├─ cmake/                        # build/package/deployment/toolchain modules
└─ .github/                      # CI and repository governance
```

The grouped `src/integrations/` directory is intentional. C++ / QML / Generic / QPA are four peer product integration technologies over one Shared Runtime; their implementation mechanisms differ, but no frontend is the implementation parent of another.

## Product architecture represented by the tree

```text
integrations/cpp --------\
integrations/qml ---------> runtime -> core
integrations/generic ----/
integrations/qpa --------/
```

The arrows mean "depends on". Runtime may depend on Core. Integration frontends may depend on Runtime. Runtime/Core may never depend on an integration frontend.

Widgets and Qt Quick are target/UI families implemented below the frontend boundary; they are not additional integration roots or Runtime personalities.

### `src/core`

`src/core` owns product-neutral semantics and abstractions such as:

- lifecycle/state/error semantics;
- `RemoteFrame` metadata and storage lifetime;
- normalized input contracts;
- bounded mailbox/scheduling/backpressure;
- target/transport-neutral interfaces and capabilities.

Core remains independent from QWidget/Qt Quick/QML, concrete RFB types, Qt private/QPA APIs, graphics-platform APIs and SoC/vendor APIs.

### `src/runtime`

`src/runtime` owns shared Qt/product implementation used by multiple/all frontends, including:

- Widgets and Quick target adapters;
- concrete transport/security implementation;
- component factories assembling Core interfaces with product implementations;
- application-surface discovery/model;
- composite capture/input routing;
- shared automatic-access behavior used by Generic and QPA;
- private Runtime configuration/types and shared lifecycle/diagnostic truth.

Runtime may use the appropriate Qt public APIs and Core. It must not depend on `src/integrations/*`.

RFB/VNC is an implementation backend inside the product architecture, not a repository-level product identity.

### `src/integrations/cpp`

Owns the public C++ application-facing facade (`HyRemote::RemoteAccess`) and C++ bootstrap semantics. Product behavior is delegated to Shared Runtime/Core.

### `src/integrations/qml`

Owns the declarative `import HyRemote` frontend over the same Shared Runtime/Core. It must not create a second lifecycle/capture/input/transport stack.

### `src/integrations/generic`

Owns the `QGenericPlugin` zero-code frontend (`-plugin hyremote` / `QT_QPA_GENERIC_PLUGINS`). It uses public Qt APIs, preserves the application's native platform identity and bootstraps the shared automatic Runtime path.

Generic must not include or link Qt private/QPA interfaces.

### `src/integrations/qpa`

Owns exact-compatible Qt private-ABI/platform-entry adaptation. It is the only frontend permitted to depend on Qt private/QPA APIs.

The product shape is a bounded native-delegate trampoline:

```text
-platform hyremote
    -> parse HyRemote/QPA configuration
    -> resolve a qualified native Qt delegate
    -> arm shared automatic Runtime
    -> QPlatformIntegrationFactory::create(native delegate)
    -> return native QPlatformIntegration*
```

QPA must not own a second capture/input/transport/session implementation. Per-platform delegate applicability is qualified truthfully; peer product status does not force every QPA platform cell to be Supported.

## Shipping-tree rules

| New thing | Placement |
| --- | --- |
| Core lifecycle/frame/input/backpressure abstraction | `src/core/` |
| shared Qt target adapter, transport/security, automatic surface/composition implementation | `src/runtime/` |
| C++ facade/API | `src/integrations/cpp/` |
| QML facade/payload | `src/integrations/qml/` |
| Generic Plugin payload | `src/integrations/generic/` |
| QPA/private compatibility/bootstrap | `src/integrations/qpa/` |
| cross-module or delivered-product verification | `tests/` |
| unit/private-detail test | beside the module it qualifies |
| user-facing example/reference application | `examples/` |
| architecture decision | `docs/adr/` |
| release/acceptance evidence | `docs/acceptance/` |

Only QPA may include Qt private/QPA headers. Generic, C++, QML, Runtime and Core stay on public Qt/product interfaces appropriate to their layers.

Do not create a Qt5 tree, embedded Runtime, accelerated Runtime, SoC-specific product Runtime or frontend-to-frontend implementation dependency. Version/platform adaptation remains bounded behind the shared product semantics.

## Build and artifact mapping

Source ownership and artifact/output paths are separate contracts. Current root composition is direct and explicit:

```cmake
add_subdirectory(src/core core)
add_subdirectory(src/runtime runtime)
add_subdirectory(src/integrations/cpp integrations/cpp)
add_subdirectory(src/integrations/qml qml/HyRemote)
add_subdirectory(src/integrations/generic generic)
add_subdirectory(src/integrations/qpa qpa)
```

Each optional frontend is added only when its capability is enabled/applicable. Established installed target/output/deploy names remain compatibility contracts until a separate accepted change deliberately changes them.

The canonical repository execution path is `build.cmd + build.yml -> CMake -> build/test/install -> product install root`. No source-layout convenience path becomes a second build authority.

## Tests and verification

Private/unit tests stay with the module whose implementation they qualify. Root `tests/` validates the delivered product from outside the internal target graph, including installed/source consumers, cross-module behavior, E2E, public contracts, compatibility/qualification fixtures and release-readiness gates.

`tests/preflight/` may hold bounded technical fixtures that remove one named implementation risk before broad work. A preflight is decision evidence, not automatically delivered-product acceptance. Its execution mechanism follows the current owning Issue/CI authority; historical specialized workflows such as the TLS preflight are not normal development/release wait gates unless a future activated capability explicitly re-adopts them.

Generic and QPA automatic-integration evidence should reuse shared Runtime contracts where the product semantics are the same. Generic additionally proves native platform identity preservation; QPA additionally proves exact private-ABI/native-delegate behavior.

Hosted/headless evidence never substitutes for physical/native evidence when the support claim depends on real display/input/loader/graphics/hardware facts.

## Current roadmap boundary

The current product phase is V0.3.0 Product Preview / self-service adoption. Repository work must distinguish:

- V0.3.0 self-service productization: four-route SDK/deploy, task-oriented learning/docs and minimum diagnostics;
- mandatory V0.3-family pre-GA breadth: Qt 5.15/6.5/6.8, Windows/Linux, x86_64/ARM64, required qwindows/qxcb/Wayland/EGLFS cells, practical compressed/incremental delivery, maintained-viewer interop and performance baseline;
- V0.4 qualification only after that baseline exists;
- V1.0 support-contract freeze from a qualified V0.4 lineage.

Repository layout must not pre-reserve technical Feature numbers or classify a #343 hard requirement as optional merely because it lands after V0.3.0.

## Documentation zones

- `docs/guide/**` and other current user-guide paths: user-facing task documentation, with language mirrors where repository policy requires them;
- top-level `docs/*.md`: durable product/reference contracts;
- `docs/internal/**`: maintainer/execution/architecture-evaluation documentation;
- `docs/adr/**`: architecture decisions;
- `docs/releases/**`: release notes;
- `docs/acceptance/**`: recorded acceptance evidence;
- `docs/proposals/**`: proposal-era input, never present-day authority by itself.

Current GitHub/repository facts and accepted authority beat historical proposal text.

## Examples

`examples/` contains user-facing examples/reference applications. They consume public installed/product surfaces, do not own product logic and do not define route hierarchy or compatibility requirements.

C++ / QML / Generic / QPA learning paths remain peers. Examples may differ by technical applicability without describing one integration technology as globally primary or advanced.

## Research and branding

There is no product `research/` build tree. Experiment conclusions belong in bounded evaluation/ADR records; accepted implementation moves into the correct `src/` owner.

`logo/` contains product branding only. Product code/CMake modules must not depend on copied branding assets.

## Forbidden legacy directories

Historical root/module names must not return:

```text
core/
runtime/
remoteaccess/
qml/
qpa/
integrations/              # root-level legacy location; canonical location is src/integrations/
verification/
assets/
research/
spikes/

src/cpp/
src/qml/
src/generic/
src/qpa/
src/remoteaccess/
src/embedded/
src/declarative/
src/transparent/
docs/assets/
```

Do not add compatibility copies, symlinks or forwarding directories at historical locations. Update references to canonical ownership instead.

## Forward growth

- Core grows by product-neutral abstractions and semantics.
- Runtime grows by shared Qt/product implementation.
- `src/integrations/*` grows only integration-specific bootstrap/facade code.
- QPA compatibility remains a narrow exact/private-ABI-qualified seam.
- Platform/vendor acceleration is evidence-driven and does not create a new Runtime or SoC-specific QPA product architecture.
- A new integration technology becomes a new peer child of `src/integrations/` only through an explicit product/architecture decision.
- A new top-level repository directory is a structural architecture decision, not a convenience refactor.

ADR-0007 (`docs/adr/0007-four-integration-runtime-architecture.md`) records the four-integration/Shared-Runtime architecture. `tests/release-readiness/check_repository_layout.cmake` is the executable guard for physical/build ownership rules; repository docs must describe the tree that current `develop` actually has.
