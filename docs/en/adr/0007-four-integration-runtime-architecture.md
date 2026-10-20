# ADR-0007: Four Integration Frontends over Core + Common Runtime

> Language / 语言: **English** | [中文](../../adr/0007-four-integration-runtime-architecture.md)

Status: **Accepted, V0.1 foundation / V1 architecture invariant (#219, reconciled by #209)**

## Context

HyRemote originally exposed application-facing technologies as sibling directories such as `src/cpp`, `src/qml`, and `src/qpa`. Designing the fourth zero-code integration, based on `QGenericPlugin`, exposed two structural problems:

1. `src/cpp` looked like a generic C++ implementation home even though it was only the Embedded C++ application-facing facade. Embedded C++ is not Widgets-specific: its `RemoteAccess` target may be a Widgets or Qt Quick object, and Runtime selects the adapter.
2. `src/qpa` owned application-level automatic surface/composition logic that is not inherently QPA-specific and must also be shared by Generic Plugin mode.

`src/core` remains the UI-neutral, protocol-neutral, platform-neutral core implementation. Its boundary excludes QWidget, Qt Quick/QML, concrete transport implementations, and Qt private/QPA types by contract. Those restrictions remain valuable and must not be weakened merely to share Qt-facing runtime code.

Qt compatibility analysis also establishes two facts:

- QPA/private APIs do not provide the public compatibility guarantees required for a product-wide common layer. QPA-specific code must stay isolated in one compatibility frontend.
- `QGenericPlugin` can provide a zero-code integration path without replacing native QPA, so it is a peer frontend, not an implementation detail of C++ or QPA.

## Decision

HyRemote uses **Core + Common Runtime + four peer integration frontends**, grouped under `src/integrations/`:

```text
integrations/cpp --------\
integrations/qml ---------> runtime -> core
integrations/generic ----/
integrations/qpa --------/
```

Canonical source roles:

```text
src/core                  shared UI/protocol/platform-neutral core implementation
src/runtime               shared Qt/product runtime implementation
src/integrations/cpp      Embedded C++ integration frontend
src/integrations/qml      QML integration frontend
src/integrations/generic  QGenericPlugin zero-code integration frontend
src/integrations/qpa      QPA zero-code integration frontend and compatibility shim
```

The four frontends are peers. No frontend is the architectural parent of another.

## `src/core` responsibilities

`src/core` owns stable product semantics independent of Qt UI technology, concrete transport, and platform implementation:

- Session lifecycle, state, and error semantics;
- `RemoteFrame` metadata and storage lifetime;
- bounded scheduling, mailbox, and backpressure policy;
- normalized remote-input model and dispatch boundary;
- transport-neutral and target-neutral interfaces;
- capability negotiation at the Core abstraction level.

Core must not require QWidget, QQuickWindow/QML, QPA/private APIs, concrete RFB implementation types, graphics-platform APIs, or SoC/vendor APIs.

## `src/runtime` responsibilities

`src/runtime` owns product implementation shared by multiple or all integration frontends that cannot live in the UI-neutral Core:

- Qt Widgets target adapters;
- Qt Quick target adapters;
- concrete RFB transport and transport-security implementation;
- component factories that assemble Core interfaces with Qt/product implementations;
- shared Qt application-surface discovery;
- application surface model;
- composite capture target and input routing;
- automatic application-level access controller shared by Generic Plugin and QPA;
- internal runtime configuration and ownership services shared across frontends.

`src/runtime` may depend on Qt public APIs and `HyRemote::Core`. It must not depend on any directory under `src/integrations/`.

## Integration frontend responsibilities

### `src/integrations/cpp`: Embedded C++

Owns the public `HyRemote::RemoteAccess` C++ facade and the integration-specific facade/ABI required by Embedded C++ consumers. Product work delegates to the common runtime.

This frontend is **not a Widgets frontend**. A C++ consumer may target Widgets or Qt Quick; target-family adaptation belongs to Runtime.

### `src/integrations/qml`: QML

Owns `import HyRemote` and the QML-facing wrapper/property/lifecycle semantics. Product work delegates to the common runtime and does not implement a second Session/capture/transport architecture.

### `src/integrations/generic`: QGenericPlugin

Owns a Qt Generic Plugin payload loaded with `-plugin hyremote` or `QT_QPA_GENERIC_PLUGINS`. It preserves the application's native QPA and platform identity and bootstraps the shared automatic runtime. It uses Qt public Generic Plugin APIs and must not depend on Qt QPA/private APIs.

### `src/integrations/qpa`: QPA

Owns the Qt private/QPA compatibility boundary. The long-term design is a **Factory Trampoline**: parse HyRemote platform parameters, resolve or create the qualified native Qt platform integration, bootstrap and arm the same automatic runtime, then return the native `QPlatformIntegration*` to Qt. QPA does not own product capture/input/transport/session semantics.

Only `src/integrations/qpa` may depend on Qt private/QPA ABI.

## Why `src/integrations/` is explicit

Keeping `cpp`, `qml`, `generic`, and `qpa` directly under `src/` makes those technology names look like shared implementation layers. That is especially misleading for `cpp`, because most of HyRemote is C++.

Grouping them under `src/integrations/` makes the ownership model readable from the tree:

```text
src/core           shared product semantics
src/runtime        shared product implementation
src/integrations/* ways an application enters the runtime
```

Widgets and Quick are Runtime adapter dimensions, not integration-directory dimensions.

## Automatic integration ownership

The following code is application-level automatic integration, not QPA implementation, so it moves from the QPA frontend into Common Runtime:

- application surface model;
- composite target and routing;
- interactive composite target;
- application-level automatic-access controller.

Generic Plugin and QPA must use the same automatic controller and surface/composition semantics.

## QPA composition boundary

`qpa_composition_seam.*` existed because QPA-owned composite code needed Core frame/input operations while QPA cannot link Core directly. Once composite and automatic code belonged entirely to Common Runtime, that indirection was no longer architecturally necessary. It was removed instead of being renamed into another frontend-specific seam; runtime composition sources now include `detail/composition_support.hpp` directly.

## QPA platform strategy

HyRemote does not create Rockchip/NXP/TI-specific QPA implementations. The platform chain is:

```text
HyRemote QPA Factory Trampoline
        -> native Qt QPA (windows/xcb/wayland/eglfs/...)
        -> vendor Qt/BSP
        -> OS/GPU/display/input hardware
```

Vendor BSP and Qt own native display/input/GPU adaptation. Platform-specific HyRemote code is permitted only in the QPA compatibility shim or in a separately justified acceleration backend after portable-path measurements prove it necessary.

## Compatibility model

- `core`: ordinary C++ compatibility boundary; no Qt private ABI;
- `runtime`: Qt public API and product implementation boundary;
- `integrations/cpp`: public C++ integration ABI/API;
- `integrations/qml`: public Qt/QML integration boundary;
- `integrations/generic`: public Qt Generic Plugin boundary;
- `integrations/qpa`: Qt private QPA boundary, rebuilt and qualified per supported Qt/QPA compatibility unit.

A shared source architecture does not imply one QPA binary across Qt minor lines, CPU architectures, or vendor-patched Qt distributions.

## Target product behavior

All four modes share the same product semantics:

1. Embedded C++ API;
2. QML API;
3. Generic Plugin zero-code integration;
4. QPA Factory-Trampoline zero-code integration.

Differences are limited to bootstrap, configuration, and target ownership. Session, frame, capture, input, transport, security, cleanup, and error semantics are common Runtime/Core behavior.

## Repository target layout

The canonical post-migration shipping tree:

```text
src/
├── core/
├── runtime/
└── integrations/
    ├── cpp/
    ├── qml/
    ├── generic/
    └── qpa/
```

`src/` remains the shipping tree. `runtime` is an internal shared implementation module, not a fifth application integration mode.

## Build and artifact compatibility during migration

Source ownership and artifact paths are separate contracts. The migration may preserve existing target names, output names, and build/deployment locations while source directories change.

In particular, `HyRemoteRemoteAccess` and `HyRemote::RemoteAccess` may keep the established build/output identity while target ownership moves to `src/runtime` and the C++ facade lives under `src/integrations/cpp`.

Any intentional artifact or API change requires a separate compatibility decision; it is not implied by repository relocation.

## Migration constraints

- Do not change a frozen release candidate or its evidence merely to absorb this migration.
- Perform the migration on the dedicated #219 architecture branch/PR.
- Preserve one Core/session/capture/input/transport architecture throughout.
- Do not temporarily solve ownership by making Generic or QPA depend on Embedded C++ implementation details.
- Keep QPA/private headers out of Core, Runtime, C++, QML, and Generic modules.
- Remove the legacy physical roots `src/cpp`, `src/qml`, `src/generic`, and `src/qpa`; do not leave forwarding directories or compatibility copies.
- Update repository-layout gates, package/deploy contracts, examples, and cross-mode tests before the migration is complete.

## Consequences

Positive:

- the source tree distinguishes shared implementation from application integration;
- `src/integrations/cpp` unambiguously means Embedded C++ API, not all C++ implementation;
- Widgets and Quick remain equal Runtime target families;
- Generic and QPA become true peer zero-code frontends;
- application-level automatic access is implemented once;
- QPA private compatibility risk is sharply isolated;
- vendor/SoC expansion does not create new product personalities;
- the QPA-specific composition bridge can disappear.

Cost:

- build, install, and release-readiness paths must be migrated deliberately;
- historical documentation and CI checks that encode old physical paths must be updated;
- moving automatic/composite implementation requires preserving existing QPA behavior through cross-mode tests.
