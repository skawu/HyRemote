# Contributing to HyRemote

HyRemote is in an early product-development phase. Contributions are welcome, but changes must preserve the project's portability, product integration modes, and separation of concerns.

## Before coding

For non-trivial changes, start with a GitHub Issue describing:

- the problem or capability;
- affected application types (Widgets, Quick, mixed UI);
- affected layer (target, capture, transport, input, encoder, integration);
- public API impact;
- Qt/private API impact;
- platform-specific impact;
- validation plan;
- affected product milestone/version when applicable.

Architecture-affecting changes should be discussed and recorded before implementation.

## Repository and branch ownership

Repository paths are architecture boundaries, not arbitrary folders. Follow [`docs/repository-layout.md`](docs/repository-layout.md):

- `src/` contains the normal product implementation;
- `integrations/` contains QML/QPA integration payloads over the same runtime;
- root `tests/` contains cross-module/consumer/release evidence;
- module-private tests stay with their module;
- `research/` contains opt-in non-product experiments/evidence;
- `assets/` contains non-code assets.

Do not recreate the historical root `core/`, `remoteaccess/`, `qml/`, `qpa/`, `spikes/` or `logo/` directories as compatibility copies.

Branches are temporary work cursors. Follow [`docs/branch-lifecycle.md`](docs/branch-lifecycle.md) and [`docs/git-flow-release.md`](docs/git-flow-release.md): normally only `main`, `develop` and current open-PR heads remain visible. Historical auditability belongs to Issues, PRs, commits and release tags rather than stale branch refs.

## Product milestones and technical WBS

HyRemote product milestones are defined by user-facing capability and platform support, not by internal implementation stages. See [`docs/versioning.md`](docs/versioning.md).

Core, capture, transport, RemoteFrame, DMA-BUF, hardware encoding, CI, security, and similar engineering work are WBS/tasks under the product milestone they enable.

## Core boundaries

Do not introduce dependencies that make the core permanently depend on:

- a specific transport protocol such as VNC/RFB;
- a specific QPA platform such as EGLFS;
- a specific SoC such as RK3588;
- a specific hardware encoder such as RKMPP;
- Qt private APIs.

Such dependencies belong behind adapters/backends.

## Integration modes

HyRemote intentionally supports three product integration styles. A contribution must not remove another mode merely to simplify one implementation:

- Embedded C++ API;
- Declarative QML API;
- Transparent QPA Proxy.

The Embedded C++ API is the stable reference integration. The QML API wraps the same core. The QPA Proxy is optional and may have stronger Qt-version constraints.

## Qt application types

Qt Widgets and Qt Quick are first-class peers. Changes should state whether they apply to:

- QWidget/raster UI;
- QOpenGLWidget;
- QQuickWindow;
- QQuickWidget;
- Quick3D;
- mixed UI.

Do not claim compatibility without repeatable validation.

## Public APIs and Qt private APIs

Prefer Qt public APIs for stable functionality. If Qt private/QPA APIs are unavoidable:

1. isolate them in a dedicated optional adapter;
2. document the exact Qt versions tested;
3. keep private types out of HyRemote public headers;
4. add compatibility tests before enabling support for a new Qt minor version.

## Dependencies

New third-party dependencies require an Issue that records:

- upstream repository and release/commit;
- license;
- maintenance/community status;
- security/update strategy;
- why existing dependencies are insufficient;
- whether the dependency is required or optional.

Avoid vendoring a fork when a maintained upstream library can be used or extended upstream.

## Pull requests

A pull request should be focused and independently reviewable. It should include:

- linked Issue;
- scope and non-scope;
- architecture/API impact;
- tests performed;
- platforms/Qt versions tested;
- known limitations.

Do not combine architecture changes, unrelated refactors, and feature work in one PR.

After a PR is merged, superseded or closed and no other open PR uses its head, delete the short-lived branch. Do not keep task branches as permanent documentation.

## Compatibility claims

A feature is considered **supported** only when it has:

- a reproducible build;
- a functional test or example;
- a compatibility entry;
- documented known limitations.

Everything else should be marked experimental, planned, or unverified.

## Documentation and comment language

Documentation is organised by **reader intent**, not by development stage, and it is bilingual. The
user-facing zone is Chinese-primary; the release/internal zone stays English because the release-readiness
documentation gate pins exact tokens in those files (translate them only together with the gate).

| Zone | Paths | Language | Content rule |
| --- | --- | --- | --- |
| User guide | `docs/guide/**` | Chinese primary + `docs/en/guide/**` mirror | Final shape only: install, integrate, deploy, troubleshoot |
| Reference | `docs/reference/**` | Chinese primary + `docs/en/reference/**` mirror | Product final-state contracts (architecture, capture/input, API stability, compatibility, security, versioning) |
| Internal / release | rest of `docs/`, `docs/adr/`, `docs/releases/`, `docs/proposals/` | English | Acceptance runbooks, repository administration, layout authority, milestone records |

Rules for the user-facing zones:

- **Bilingual pairs**: the Chinese primary document lives at `docs/<path>`; the English mirror lives at
  `docs/en/<path>`. A document that enters `guide/` or `reference/` must have its mirror at the same relative
  path, carries a one-line language switch at the top, and changes in both languages in the same change.
  Legacy documents that have not moved into those zones yet are not required to be mirrored while the
  migration is in progress.
- **One document per intent**: merge by what the reader wants to do. A single detailed document beats several
  that each cover a fragment; repository documents are not a place to record the path that produced them.
- **No process content**: issue numbers and tracking, acceptance scheduling/status, milestone chronicles,
  investigation logs and one-off checklists do not belong to the user-facing zones. Keep them in the internal
  zone, or as evidence under `research/` where they actually belong.
- **Moves update references**: when a document moves, update every reference in the repository in the same
  change. Do not leave forwarding copies - with one deliberate exception: a path that a release-readiness gate
  still lists may keep a short pointer file until the gate's path list is updated. Such pointers carry no
  documentation content, and removing them is a maintainer change rather than an authoring one.

Source comments are **not** under a mandatory bilingual policy for V1:

- keep existing source comments as they are - there is no bulk comment-only migration across public headers,
  internal code or tests;
- new or substantially edited comments prioritize clarity and consistency with the surrounding file;
- bilingual comments are allowed where they materially help maintainers, for example:

```cpp
// 采集回调运行在 GUI 线程上，不得在此阻塞 —— The capture callback runs on the GUI thread; never block here.
void onCapturedFrame(const RemoteFrame &frame);
```

- keep identifiers, log tokens, CMake target names and gate-checked strings in their existing form; a language
  preference never renames a symbol.

## Commit style

Use concise conventional-style subjects where practical, for example:

- `feat: add QWidget target adapter`
- `fix: avoid blocking GL readback on render thread`
- `docs: record capture backend decision`
- `test: add Quick3D compatibility case`

## License and contributions

HyRemote is licensed under the **Apache License 2.0**. Unless a contribution is explicitly marked otherwise and accepted under a compatible license, contributions intentionally submitted for inclusion in HyRemote are provided under the Apache License 2.0, consistent with Section 5 of that license.

Do not add source or assets copied from external projects unless their license compatibility, attribution, and redistribution requirements have been reviewed first.

The Apache License 2.0 does not grant trademark rights. Project names, logos, trademarks, and brand assets may have separate usage terms.
