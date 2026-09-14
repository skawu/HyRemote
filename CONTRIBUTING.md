# Contributing to HyRemote

HyRemote is in an early architecture phase. Contributions are welcome, but changes must preserve the project's portability and separation of concerns.

## Before coding

For non-trivial changes, start with a GitHub Issue describing:

- the problem or capability;
- affected application types (Widgets, Quick, mixed UI);
- affected layer (target, capture, transport, input, encoder, integration);
- public API impact;
- Qt/private API impact;
- platform-specific impact;
- validation plan.

Architecture-affecting changes should be discussed and recorded before implementation.

## Core boundaries

Do not introduce dependencies that make the core permanently depend on:

- a specific transport protocol such as VNC/RFB;
- a specific QPA platform such as EGLFS;
- a specific SoC such as RK3588;
- a specific hardware encoder such as RKMPP;
- Qt private APIs.

Such dependencies belong behind adapters/backends.

## Integration modes

HyRemote intentionally supports multiple integration styles. A contribution must not remove another mode merely to simplify one implementation:

- Embedded C++ API;
- Declarative QML API;
- optional zero-code platform/QPA proxy.

The Embedded C++ API is the stable reference integration. The QML API wraps the same core. The zero-code path is optional and may have stronger Qt-version constraints.

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

## Compatibility claims

A feature is considered **supported** only when it has:

- a reproducible build;
- a functional test or example;
- a compatibility entry;
- documented known limitations.

Everything else should be marked experimental, planned, or unverified.

## Commit style

Use concise conventional-style subjects where practical, for example:

- `feat: add QWidget target adapter`
- `fix: avoid blocking GL readback on render thread`
- `docs: record capture backend decision`
- `test: add Quick3D compatibility case`

## License

The project license is intentionally not frozen during bootstrap. Do not add source files copied from external projects unless their license compatibility and attribution requirements have been reviewed first.
