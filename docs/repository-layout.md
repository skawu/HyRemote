# Repository Layout

HyRemote's repository structure follows the product architecture rather than the historical order in which features were developed.

## Canonical top-level layout

```text
HyRemote/
├─ src/
│  ├─ core/                  # internal static Core: Session, frame, storage, normalized input
│  └─ remoteaccess/          # one public/shared C++ RemoteAccess facade and Qt target adapters
├─ integrations/
│  ├─ qml/HyRemote/          # Declarative QML payload over the same RemoteAccess runtime
│  └─ qpa/                   # Transparent QPA platform MODULE payload
├─ tests/                    # cross-module, consumer, product-E2E and release-readiness evidence
├─ examples/                 # product examples E1-E5
├─ research/                 # non-product spikes/architecture evidence; opt-in only
├─ assets/
│  └─ branding/              # repository/product branding assets; never part of the build graph
├─ cmake/                    # package, deployment and build-system modules
├─ docs/                     # user, architecture, acceptance and release documentation
└─ .github/                  # CI and repository governance
```

## V1 layout freeze

The V1 repository layout is now frozen. Work toward V1.0.0.0 may fix implementation, packaging, tests, documentation and CI defects, but it must not introduce another structural repository migration unless a release-blocking architecture defect proves the canonical ownership model itself is wrong.

The release-readiness repository-layout gate enforces the physical layout and module boundaries. The CI-environment-baseline gate separately requires the Widgets, Quick, QML, QPA, SDK-consumption and integrated V1 GA Linux jobs to use the same repository-owned Qt desktop host dependency authority.

## Ownership rules

### `src/`

`src/` contains the normal product implementation only.

- `src/core` remains an internal STATIC composition target and is not an installed application SDK target.
- `src/remoteaccess` owns the single shared `HyRemote::RemoteAccess` / `HyRemoteRemoteAccess` runtime and its Widgets/Quick target adapters.
- A new transport, capture implementation, or target adapter that is part of the normal product belongs below the appropriate product module, not at repository root.

### `integrations/`

`integrations/` contains application-integration payloads that reuse the same shared runtime.

- `integrations/qml/HyRemote` provides `import HyRemote`; it is not a second C++ product runtime.
- `integrations/qpa` provides `qhyremote`; it is a platform MODULE and is not an application link target.

Integration payloads may depend on the product runtime. Product Core must not depend on an integration payload.

### `tests/`

Root `tests/` is for evidence that crosses a module boundary or validates the repository as a delivered product: clean consumers, installed/source SDK tests, product E2E and release-readiness gates.

Tests that need private implementation details remain colocated under the module they qualify, for example `src/remoteaccess/tests` and `integrations/qpa/tests`. They are not installed.

### `examples/`

`examples/` contains user-visible examples only. It must not become a second implementation location for product logic.

### `research/`

`research/` contains spikes and historical architecture evidence. Research sources are opt-in through the existing research/spike build option and are not a V1 release dependency. A successful experiment becomes product code only through an explicit architecture/product decision and migration into `src/` or `integrations/`.

### `assets/`

`assets/branding` contains non-code branding material. Assets do not participate in normal product compilation or package dependency discovery.

## Source paths versus build paths

The source layout was normalized without intentionally changing established build-tree artifact paths. Root CMake uses explicit binary directories, for example:

```cmake
add_subdirectory(src/core core)
add_subdirectory(src/remoteaccess remoteaccess)
add_subdirectory(integrations/qml/HyRemote qml/HyRemote)
add_subdirectory(integrations/qpa qpa)
```

This keeps existing CI/deployment artifact locations such as `build/remoteaccess`, `build/qml/HyRemote` and `build/plugins/platforms` stable while making repository ownership clear.

## Forbidden legacy root directories

The following historical root directories must not be reintroduced:

```text
core/
remoteaccess/
qml/
qpa/
spikes/
logo/
```

The release-readiness repository-layout gate enforces this boundary. Do not add compatibility copies, symlinks, or forwarding directories at the old locations; update source references to the canonical layout instead.
