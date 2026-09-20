# Repository Layout

HyRemote's repository structure follows the product architecture rather than the historical order in which features were developed.

## Canonical top-level layout

```text
HyRemote/
├─ src/                      # everything the repository builds and ships: one directory per deliverable
│  ├─ core/                  # internal static Core: Session, frame, storage, normalized input (not installed)
│  ├─ remoteaccess/          # one public/shared C++ RemoteAccess facade and Qt target adapters
│  ├─ qml/HyRemote/          # Declarative QML payload installed into the consuming application
│  └─ qpa/                   # Transparent QPA platform MODULE payload installed into the Qt plugin tree
├─ tests/                    # unit and integration tests: module-colocated units, cross-module/consumer/
│                            # product-E2E suites and the release-readiness gates
├─ examples/                 # user-facing usage examples E1-E6, including examples that combine HyRemote
│                            # with third-party open-source applications
├─ research/                 # non-product architecture evidence; never part of a build
├─ assets/
│  └─ branding/              # repository/product branding assets; never part of the build graph
├─ cmake/                    # package, deployment and build-system modules
├─ docs/                     # documentation, physically zoned by reader
│  ├─ guide/                 # user guide, Chinese primary with docs/en/guide/ mirror
│  ├─ getting-started/       # user entry points by API shape (C++ / QML / QPA)
│  ├─ internal/              # maintainer/release zone: runbooks, layout authority, roadmap,
│  │                         # research and evaluation records
│  ├─ acceptance/            # recorded acceptance evidence, one directory per candidate
│  ├─ adr/ releases/ proposals/
│  ├─ en/                    # English mirror of the user zone plus the English index
│  └─ *.md                   # product final-state contracts (architecture, security,
│                            # compatibility, API stability, capture/input, versioning)
└─ .github/                  # CI and repository governance
```

## V1 layout freeze

The V1 repository layout is now frozen. Work toward V1.0.0.0 may fix implementation, packaging, tests, documentation and CI defects, but it must not introduce another structural repository migration unless a release-blocking architecture defect proves the canonical ownership model itself is wrong.

**Documentation zoning.** Inside `docs/`, the reader-intent zones are physical directories, not prose: `docs/guide/**`
and `docs/getting-started/**` are the user-facing zone (Chinese primary, `docs/en/**` mirror), the product final-state
contracts stay at the top level of `docs/`, and everything written for maintainers or the release process lives in
`docs/internal/**` (plus the long-standing `docs/adr/`, `docs/releases/` and `docs/proposals/`). The zone rules and the
writer-facing conventions are in [`../../CONTRIBUTING.md`](../../CONTRIBUTING.md) and the indexes
[`../README.md`](../README.md) / [`../en/README.md`](../en/README.md). This zoning is a documentation reorganisation
inside an existing top-level section: it does not change product-module ownership, the build graph, or any canonical
source path, so the V1 layout freeze above still holds for everything it describes.

**The shipping tree is flat.** `src/` answers one question - does it ship? - and each directory below it is one deliverable: the internal static Core, the shared `RemoteAccess` runtime and its Qt target adapters, and the two payloads installed into a host application (the QML module and the QPA platform plugin). The former separate `integrations/` top-level directory was folded into `src/` for exactly that reason: the boundary it encoded (a payload may depend on the shared runtime, never the reverse) is a **rule enforced by the layout gate**, not an axis worth a second top-level directory. The gate still pins both payloads' CMake (must link `HyRemote::RemoteAccess`, must not link `HyRemote::Core`, must not compile a second `RemoteAccess` facade) and the root build's explicit `add_subdirectory(<source> <stable-binary-dir>)` mapping, so no artifact path moved.

The release-readiness repository-layout gate enforces the physical layout and module boundaries. The CI-environment-baseline gate separately requires the Widgets, Quick, QML, QPA, SDK-consumption and integrated V1 GA Linux jobs to use the same repository-owned Qt desktop host dependency authority.

## Ownership rules

### `src/`

`src/` is the **shipping tree**: everything below it is built and delivered, one directory per deliverable, and nothing that is not shipped belongs here.

- `src/core` remains an internal STATIC composition target and is not an installed application SDK target.
- `src/remoteaccess` owns the single shared `HyRemote::RemoteAccess` / `HyRemoteRemoteAccess` runtime and its Widgets/Quick target adapters.
- `src/qml/HyRemote` is the Declarative QML payload: it provides `import HyRemote`, it is installed into the consuming application's QML import tree, and it is not a second C++ product runtime.
- `src/qpa` is the Transparent QPA payload: it provides `qhyremote`, it is a platform MODULE installed into the application's Qt plugin tree, and it is not an application link target.
- A new transport, capture implementation, or target adapter that is part of the normal product belongs below the product module that owns it (`src/remoteaccess` today), not at repository root.
- A new payload that is delivered into a host application is a sibling deliverable under `src/`, registered with an explicit `add_subdirectory(<source> <stable-binary-dir>)`.

Payloads may depend on the product runtime. The shared runtime and Core must never depend on a payload - the direction the layout gate enforces.

### `src/`

`src/` contains application-integration payloads that reuse the same shared runtime.

- `src/qml/HyRemote` provides `import HyRemote`; it is not a second C++ product runtime.
- `src/qpa` provides `qhyremote`; it is a platform MODULE and is not an application link target.

Integration payloads may depend on the product runtime. Product Core must not depend on an integration payload.

### `tests/`

`tests/` holds unit tests and integration test cases.

- Root `tests/` carries the suites that cross a module boundary or validate the repository as a delivered product: clean consumers, installed/source SDK tests, product E2E and the release-readiness gates.
- Unit tests that need private implementation details remain colocated under the module they qualify, for example `src/remoteaccess/tests` and `src/qpa/tests`. They are not installed.
- Recorded review or acceptance **evidence** is not a test case and does not belong here; it is documentation and lives under `docs/acceptance/`.

### `examples/`

`examples/` contains user-facing usage examples only.

- The acceptance suite is E1-E6: `widgets-basic`, `quick-basic`, `qml-basic`, `qpa-proxy-existing-app`, `remote-support-showcase`, and the installed/source SDK consumers.
- Usage examples that combine HyRemote with a **third-party open-source application** belong here too: they are the documented way a user reproduces an integration on their own machine, so their README carries the exact application version, the exact HyRemote candidate, the Qt version and the launch/deployment commands, together with the caveat that they are verification examples and not a support claim.
- Such an example must be opt-in (its own CMake option, OFF by default), must never vendor third-party sources into this repository (fetch them at a recorded commit into an ignored build directory instead), must not enter the default build or the acceptance graph, and must never become an implementation location for product logic. This is the rule for **examples**: a third-party **dependency** the product links is a different case, governed by `dependency-policy.md`, where the project may carry it as a submodule at a pinned release when the user's own environment cannot provide it.

### `research/`

`research/` holds non-product architecture evidence: the measurements and decisions that are worth keeping as an audit trail. It is not a V1 release dependency, no CI workflow builds it and no product module may reference it. A successful experiment becomes product code only through an explicit architecture/product decision and a migration into `src/`.

### `assets/`

`assets/branding` contains non-code branding material. Assets do not participate in normal product compilation or package dependency discovery.

## Source paths versus build paths

The source layout was normalized without intentionally changing established build-tree artifact paths. Root CMake uses explicit binary directories, for example:

```cmake
add_subdirectory(src/core core)
add_subdirectory(src/remoteaccess remoteaccess)
add_subdirectory(src/qml/HyRemote qml/HyRemote)
add_subdirectory(src/qpa qpa)
```

This keeps existing CI/deployment artifact locations such as `build/remoteaccess`, `build/qml/HyRemote` and `build/plugins/platforms` stable while making repository ownership clear.

## Extension points and forward compatibility

The canonical axes above are the repository's permanent growth surface. New work lands inside them; a new
top-level directory is a structural decision carrying the same authority as a migration, not a review detail.
Naming rules for anything added here are in [`naming-conventions.md`](naming-conventions.md).

- **`src/` grows by product module.** A new transport, capture implementation, target adapter or platform
  backend that is part of the normal product belongs below the product module that owns it (`src/remoteaccess`
  today), never at the repository root. A future module that is not an adapter of the shared runtime is added as
  `src/<module>/` with its own `add_subdirectory(<source> <stable-binary-dir>)` mapping, and Core stays Qt-free
  and platform-free.
- **`src/` grows by deliverable, one directory per deliverable.** Every payload reuses the same shared runtime,
  may not link Core directly and may not compile a second `RemoteAccess` facade - the rules the gate already
  enforces for QML and QPA, and which apply to any `src/*` payload. Expected later payloads (embedded
  EGLFS or DRM/GBM platform support) are added as `src/<payload>/` and registered with an explicit
  `add_subdirectory(<source> <stable-binary-dir>)`; the stable binary directory is part of the contract, so a
  payload never relocates an existing artifact.
- **Version-qualified payloads.** The Transparent QPA payload is coupled to the exact Qt private ABI it was
  qualified against (Qt 6.8.3 for V1). A future Qt LTS line is served by a new payload directory named for that
  line (`src/qpa-<qt-line>/`), not by widening the existing one: `src/qpa` keeps its path,
  target and artifact names, and each payload stays qualified against exactly one Qt line.
- **`tests/` grows by test scope, not by module.** Unit and integration suites follow the rules in the ownership
  section above; a new platform adds tests in the same shape rather than a new root.
- **Recorded evidence grows under `docs/`.** Physical-acceptance and review evidence for a candidate is
  documentation: a directory per candidate or version under `docs/acceptance/`, next to the runbook that
  produced it (`docs/internal/v1-physical-acceptance.md`), so evidence never mixes with test code.
- **`examples/` grows by integration mode and usage scenario.** The E-numbering continues, one directory per
  example, and examples that combine HyRemote with third-party open-source applications are opt-in, unfetched
  by default and never an implementation location for product logic (see the ownership section above).
- **`assets/` holds non-code material only** (`assets/branding` today; packaging icons or desktop-entry
  material for a future platform are sibling directories under `assets/`).
- **`cmake/` owns build, package and deployment modules.** Platform-specific deployment is an additional module
  here, never code inside product sources.
- **`research/` stays non-product.** A successful experiment becomes product code only through an explicit
  architecture decision and a migration into `src/`.

## Forbidden legacy root directories

The following historical root directories must not be reintroduced:

```text
core/
remoteaccess/
qml/
qpa/
integrations/
spikes/
logo/
```

The release-readiness repository-layout gate enforces this boundary. Do not add compatibility copies, symlinks, or forwarding directories at the old locations; update source references to the canonical layout instead.

## Build entry points and toolchains

Two additive entry points sit at the root, alongside the CMake project. They are **build conveniences, not product
artifacts**, and they do not change any CMake default - the frozen V1 option defaults in
`cmake/HyRemoteProjectOptions.cmake` remain authoritative:

```text
compile.cmd         build entry point - valid POSIX shell script *and* Windows batch file
clean.cmd           removes a mode's build tree, or the whole build tree with --all
```

Their layout mirrors the 4diac-fbe build environment: a single script per action that runs on both operating systems,
a named build configuration selected with `--mode` (default `qpa`, overridable in the script or on the command line),
and one log file per phase.

**Exactly one build directory.** All output, and the `build/configure.log` / `build/build.log` logs, go to the
git-ignored `build/` - there is no per-mode subdirectory and no second tree anywhere. Switching mode or rebuilding
means cleaning first (`clean.cmd`); if `build/` holds a different mode's configuration the script refuses to mix and
prints the exact `--clean --mode <new-mode>` command. This keeps the repository root free of build files and stops a
stale generator, compiler or cache from being reused silently.

Cross-compilation toolchain files live under the existing `cmake/` root:

```text
cmake/toolchains/*.cmake       CMake toolchain files for embedded targets
cmake/toolchains/README.md     how to use and add one
```

`cmake/toolchains/` grows by target; adding a toolchain file is not a structural decision, while adding a new
top-level directory still is. User-facing instructions live in `docs/guide/cross-compilation.md` with its English
