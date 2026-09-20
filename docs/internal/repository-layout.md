# Repository Layout

HyRemote's repository structure follows the product architecture rather than the historical order in which features were developed.

## Canonical top-level layout

The rule the whole tree follows: **`src/` is what ships, and everything else is what makes shipping possible.** A
directory at the top level answers exactly one question - is it built and delivered to a user, or is it evidence,
documentation, tooling or governance?

```text
HyRemote/
├─ CMakeLists.txt                # root build graph; maps every source directory to a stable binary directory
├─ compile.cmd / clean.cmd       # the single build entry point (one file that is both a POSIX sh and a Windows
│                                #   batch script); exactly one build/ tree, no per-mode subdirectories
├─ README.md                     # what HyRemote is, how to build it, how to integrate it
├─ CONTRIBUTING.md               # contributor rules: layout, documentation zones, branch lifecycle, language
├─ SECURITY.md                   # security policy and the current product security statement
├─ NOTICE.md / LICENSE           # third-party notices and license terms
│
├─ src/                          # SHIPPING TREE - named for what it delivers: a base plus the three access modes
│  ├─ core/                      #   BASE: internal static library; not installed, not linkable by a payload
│  │  ├─ include/hyremote/       #     internal headers: Session, frame, storage, normalized input, capabilities
│  │  ├─ src/                    #     implementation (session/frame/storage/input/types/capabilities + detail/)
│  │  └─ tests/                  #     colocated unit tests + the Core dependency guard
│  ├─ embedded/                  #   ACCESS MODE 1 - Embedded C++: the shared runtime and its public facade
│  │  ├─ include/                #     the one public API header (HyRemote::RemoteAccess)
│  │  ├─ src/                    #     facade implementation: detail/, transport/, widgets/, quick/
│  │  └─ tests/                  #     colocated unit/integration tests for the facade
│  ├─ declarative/               #   ACCESS MODE 2 - Declarative QML: provides `import HyRemote`
│  │  └─ tests/                  #     QML module tests + the deploy-helper fixture
│  └─ transparent/               #   ACCESS MODE 3 - Transparent QPA: `qhyremote`, Qt 6.8.3-qualified
│     └─ tests/                  #     QPA smoke / native-semantics / interception / relocation tests
│
├─ tests/                        # TESTS AND PRODUCT VERIFICATION: nothing here is a unit test, because a unit
│  │                             #   test stays with the module it qualifies (src/*/tests/)
│  ├─ consumer-installed-sdk/    #   an independent CMake project consuming the installed SDK
│  ├─ consumer-installed-qml/    #   the same, for the installed QML payload
│  ├─ consumer-installed-qpa/    #   the same, for the installed QPA payload
│  ├─ consumer-source/           #   an add_subdirectory consumer (must not inherit developer-only switches)
│  ├─ product-e2e/               #   product end-to-end scripts
│  ├─ public-api-contract/       #   the public API surface contract
│  ├─ third_party/               #   the real-world open-source application matrix (issue #134)
│  └─ release-readiness/         #   the release gates (CMake scripts + CI environment verifier)
│
├─ examples/                     # user-facing usage examples; opt-in, never in the default build
│  ├─ widgets-basic/ quick-basic/ qml-basic/      #   E1/E2/E3 - the three integration shapes
│  ├─ qpa-proxy-existing-app/                     #   E4 - an unmodified Qt application through the QPA proxy
│  └─ remote-support-showcase/                    #   E5/E6 - combined showcase, third-party app combinations
│
├─ docs/                         # documentation, physically zoned by reader, not by development history
│  ├─ guide/                     #   USER ZONE (Chinese primary): install, cross-compilation, deployment,
│  │                             #     viewer connection, troubleshooting
│  ├─ getting-started/           #   USER ZONE entry points by API shape: cpp.md, qml.md, qpa-proxy.md
│  ├─ en/guide/                  #   the English mirror of the user zone (+ en/README.md, the English index)
│  ├─ *.md                       #   PRODUCT FINAL-STATE CONTRACTS at the top level: architecture, security*,
│  │                             #     compatibility, known limitations, API stability, versioning, release
│  │                             #     package manifest, dependency policy, capture/input model, SDK/QML consumption
│  ├─ internal/                  #   MAINTAINER/RELEASE ZONE: layout and branch authority, release runbooks and
│  │                             #     checklists, roadmap, naming, historical proposals, evaluation records
│  ├─ adr/                       #   architecture decision records (what was decided and on what evidence)
│  ├─ releases/                  #   per-milestone release notes (the record of what each release did)
│  ├─ proposals/                 #   proposal-era design input; explicitly not present-day truth
│  ├─ acceptance/                #   recorded acceptance evidence, one directory per candidate or version
│  └─ assets/logo/               #   the product mark used by documentation and the UI; never in the build graph
│
├─ cmake/                        # build, package and deployment modules
│  ├─ HyRemoteProjectOptions.cmake   #   the frozen option defaults (the consumer-visible contract)
│  ├─ HyRemoteInstall.cmake / HyRemoteConfig.cmake.in / HyRemoteDeploy.cmake / HyRemoteReleaseProfile.cmake
│  └─ toolchains/                #   cross-compilation toolchain files
│
└─ .github/                      # CI and repository governance
   ├─ workflows/                 #   one workflow per integration/acceptance surface
   ├─ scripts/                   #   repository administration helpers + the shared Linux Qt dependency baseline
   ├─ ISSUE_TEMPLATE/            #   issue templates carrying the layer checklist
   └─ release/                   #   release metadata input
```

### The same tree seen three ways

**Product architecture - what a user actually receives.** One shared library is the contract; everything else is
either behind it or beside it.

| Delivered artifact | Directory | What the user gets |
| --- | --- | --- |
| Shared runtime | `src/embedded` (+ `src/core` composed statically behind it) | `HyRemote::RemoteAccess`, one C++ library, no backend types in the API |
| Declarative payload | `src/declarative` | `import HyRemote` for QML applications |
| Transparent payload | `src/transparent` | `qhyremote`, a Qt platform plugin that proxies an unmodified application |
| Nothing | `src/core` | internal only: not installed, not linkable, no stability promise |

**Technical architecture - build graph, dependency direction, artifacts.** The root `CMakeLists.txt` maps each source
directory to a *stable binary directory* (`add_subdirectory(<source> <binary>)`), so a source reorganisation never
relocates an artifact a CI job, installer or deployment script depends on. Dependencies point one way only:
`core -> embedded -> {declarative, transparent}`, and a payload may never link `Core` directly or compile a second
facade. Tests
live where they qualify: private-detail unit tests are colocated under the module they qualify, and `tests/` holds only
what crosses a boundary or validates the delivered product. `cmake/` owns build/packaging/deployment modules and never
product code.

**Business architecture - who owns, reads and writes what.** The tree separates *product*, *process* and *evidence*,
because they have different audiences, different lifetimes and different review rules:

| Question | Where it is answered | Who reads it |
| --- | --- | --- |
| What is this product and how do I use it? | `README.md`, `docs/guide/**`, `docs/getting-started/**`, `examples/**` | application developers, evaluators |
| What is contractually promised, and until when? | `docs/*.md` (final-state contracts), `NOTICE.md`, `LICENSE`, `SECURITY.md` | integrators, legal/security review |
| How is it built, released and administered? | `cmake/**`, `.github/**`, `docs/internal/**`, `tests/release-readiness/**` | maintainers, release managers |
| Why is it like this? | `docs/adr/**`, `docs/internal/**` (evaluation records) | maintainers, future contributors |
| Was it actually accepted? | `docs/acceptance/**` (evidence), `docs/releases/**` (what shipped) | release owners, auditors |

### Where does new work go?

| New thing | Placement | Why |
| --- | --- | --- |
| A transport, capture implementation or platform backend of the product | under the module that owns it (`src/embedded/src/<area>/`) | it is implementation, not a new deliverable |
| A new payload installed into a host application | `src/<payload>/` + an explicit `add_subdirectory(<source> <binary>)` | siblings of the existing payloads; the binary directory is part of the contract |
| A new integration payload generation (for example another Qt line) | `src/<payload>-<qualifier>/`, never by widening the qualified payload | each payload stays qualified against exactly one line |
| A user-facing guide | `docs/guide/**` (Chinese primary) + the mirror in `docs/en/guide/**`, same relative path | the user zone is bilingual, one document per reader intent |
| A final-state contract (API, compatibility, security boundary) | top level of `docs/` | contracts are read on their own, not as part of a guide |
| A release runbook, checklist or governance rule | `docs/internal/**` | process content never enters the user zone |
| A decision that must outlive the chat that produced it | `docs/adr/**` | decisions are evidence, and they are cited by the gates |
| Acceptance evidence for a candidate | `docs/acceptance/<candidate>/` | evidence is not a test case |
| A cross-module integration test | `tests/` | unit tests stay colocated with the module they qualify |
| A clean consumer, product E2E, surface contract or release gate | `tests/<kind>/` | it consumes the delivered product the way a user would; it is not a test |
| A measurement or spike that produced a decision | the conclusion as an evaluation record under `docs/internal/**`, with the runnable evidence cited from git history | evidence is kept, unattached code is not |

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
- `src/embedded` owns the single shared `HyRemote::RemoteAccess` / `HyRemoteRemoteAccess` runtime and its Widgets/Quick target adapters.
- `src/declarative` is the Declarative QML payload: it provides `import HyRemote`, it is installed into the consuming application's QML import tree, and it is not a second C++ product runtime.
- `src/transparent` is the Transparent QPA payload: it provides `qhyremote`, it is a platform MODULE installed into the application's Qt plugin tree, and it is not an application link target.
- A new transport, capture implementation, or target adapter that is part of the normal product belongs below the product module that owns it (`src/embedded` today), not at repository root.
- A new payload that is delivered into a host application is a sibling deliverable under `src/`, registered with an explicit `add_subdirectory(<source> <stable-binary-dir>)`.

Payloads may depend on the product runtime. The shared runtime and Core must never depend on a payload - the direction the layout gate enforces.

### `src/`

`src/` contains application-integration payloads that reuse the same shared runtime.

- `src/declarative` provides `import HyRemote`; it is not a second C++ product runtime.
- `src/transparent` provides `qhyremote`; it is a platform MODULE and is not an application link target.

Integration payloads may depend on the product runtime. Product Core must not depend on an integration payload.

### `tests/`

`tests/` holds **tests only**: cross-module integration tests, which belong to no single module.

- Unit tests that need private implementation detail stay colocated under the module they qualify, for example
  `src/embedded/tests` and `src/transparent/tests`. They are never installed.
- Nothing that exercises the product *the way a user would* lives here. A clean consumer project, a product
  end-to-end script, a public-surface contract or a release gate is **verification of the delivered product**, not a
  test, and it belongs in `tests/`.
- Recorded review or acceptance **evidence** is not a test case either; it is documentation and lives under
  `docs/acceptance/`.

### `tests/`

`tests/` validates the **delivered product** by consuming it from outside the internal target graph:

- `consumer-installed-sdk`, `consumer-installed-qml`, `consumer-installed-qpa` and `consumer-source` are independent
  CMake projects - installed-prefix and `add_subdirectory` consumers - that use `HyRemote::RemoteAccess` only and know
  nothing about Core, transport or backend internals.
- `product-e2e` drives the built product end to end, and `public-api-contract` pins the public surface at compile time.
- `third_party` is the real-world open-source application matrix (issue #134): pristine upstream applications, with all
  integration living under `tests/third_party/**` and nothing ever committed inside an upstream submodule.
- `release-readiness` holds the release gates.

Verification builds against the product rather than inside it, so no verification target is ever a product dependency,
and no product module may include or link one.

### `examples/`

`examples/` contains user-facing usage examples only.

- The acceptance suite is E1-E6: `widgets-basic`, `quick-basic`, `qml-basic`, `qpa-proxy-existing-app`, `remote-support-showcase`, and the installed/source SDK consumers.
- Usage examples that combine HyRemote with a **third-party open-source application** belong here too: they are the documented way a user reproduces an integration on their own machine, so their README carries the exact application version, the exact HyRemote candidate, the Qt version and the launch/deployment commands, together with the caveat that they are verification examples and not a support claim.
- Such an example must be opt-in (its own CMake option, OFF by default), must never vendor third-party sources into this repository (fetch them at a recorded commit into an ignored build directory instead), must not enter the default build or the acceptance graph, and must never become an implementation location for product logic. This is the rule for **examples**: a third-party **dependency** the product links is a different case, governed by `dependency-policy.md`, where the project may carry it as a submodule at a pinned release when the user's own environment cannot provide it.

### Research: conclusions are kept, orphan experiment trees are not

There is deliberately **no `research/` directory**. An experiment's value is the decision it produced, and decisions are
documented, not left as unattached code: the measurement and its conclusion are recorded under `docs/internal/**`
(evaluation records) and the runnable evidence is retrievable from git history at the commit that removed it, cited by
the document that needed it. A successful experiment becomes product code only through an explicit architecture/product
decision and a migration into `src/`; until then it leaves a conclusion, not a directory. `tests/release-readiness/check_repository_layout.cmake`
fails the build if a `research/` root or an `add_subdirectory(research/...)` mapping ever returns.

### Branding assets live under `docs/`

There is deliberately **no root `assets/` directory**. The product mark itself - the logo files used in documentation
and in the UI - lives at `docs/assets/logo/`, because documentation is its only consumer: nothing in `src/`, `cmake/`
or the build graph may reference it, and the layout gate fails if a module does. Anything that is not the product's own
non-code identity does not belong there at all.

## Source paths versus build paths

The source layout was normalized without intentionally changing established build-tree artifact paths. Root CMake uses explicit binary directories, for example:

```cmake
add_subdirectory(src/core core)
add_subdirectory(src/embedded remoteaccess)
add_subdirectory(src/declarative qml/HyRemote)
add_subdirectory(src/transparent qpa)
```

This keeps existing CI/deployment artifact locations such as `build/remoteaccess`, `build/qml/HyRemote` and `build/plugins/platforms` stable while making repository ownership clear.

## Extension points and forward compatibility

The canonical axes above are the repository's permanent growth surface. New work lands inside them; a new
top-level directory is a structural decision carrying the same authority as a migration, not a review detail.
Naming rules for anything added here are in [`naming-conventions.md`](naming-conventions.md).

- **`src/` grows by product module.** A new transport, capture implementation, target adapter or platform
  backend that is part of the normal product belongs below the product module that owns it (`src/embedded`
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
  line (`src/transparent-<qt-line>/`), not by widening the existing one: `src/transparent` keeps its path,
  target and artifact names, and each payload stays qualified against exactly one Qt line.
- **`tests/` grows by scope, never by module.** A unit test stays colocated with the module it qualifies; a
  cross-module integration test is added directly under `tests/`; a new clean consumer, product E2E script, surface
  contract, third-party lane or release gate is added under `tests/<kind>/`.
- **Recorded evidence grows under `docs/`.** Physical-acceptance and review evidence for a candidate is
  documentation: a directory per candidate or version under `docs/acceptance/`, next to the runbook that
  produced it (`docs/internal/v1-physical-acceptance.md`), so evidence never mixes with test code.
- **`examples/` grows by integration mode and usage scenario.** The E-numbering continues, one directory per
  example, and examples that combine HyRemote with third-party open-source applications are opt-in, unfetched
  by default and never an implementation location for product logic (see the ownership section above).
- **Branding lives under `docs/`, not at the root.** `docs/assets/logo/` is the mark used by documentation and the UI;
  packaging icons or desktop-entry material for a future platform become sibling directories under `docs/assets/`, each
  named for what it is rather than for a generic "asset" role, and none of them may enter the build graph.
- **`cmake/` owns build, package and deployment modules.** Platform-specific deployment is an additional module
  here, never code inside product sources.
- **Research leaves a conclusion, not a directory.** A successful experiment becomes product code only through an
  explicit architecture decision and a migration into `src/`; otherwise its record goes under `docs/internal/**`.

## Forbidden legacy root directories

The following historical root directories must not be reintroduced:

```text
core/
remoteaccess/
qml/
qpa/
integrations/
verification/
assets/
research/
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
