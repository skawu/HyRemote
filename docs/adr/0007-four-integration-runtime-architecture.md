# ADR-0007: Four Integration Frontends over Core + Common Runtime

Status: **Accepted for post-V1 architecture migration (#219)**

## Context

HyRemote originally exposed its application-facing technologies as sibling directories such as `src/cpp`, `src/qml` and `src/qpa`. During the design of the fourth zero-code integration based on `QGenericPlugin`, two structural problems became explicit:

1. `src/cpp` looked like a generic home for C++ implementation even though it was only the Embedded C++ application-facing facade. Embedded C++ itself is not Widgets-specific: its `RemoteAccess` target may be a Widgets or Qt Quick object and Runtime selects the appropriate adapter.
2. `src/qpa` owned application-level automatic surface/composition logic that is not inherently QPA-specific and must also be shared by Generic Plugin mode.

`src/core` remains the common UI/protocol/platform-neutral core implementation. Its boundary intentionally excludes QWidget, Qt Quick/QML, concrete transport implementations and Qt private/QPA types. Those restrictions remain valuable and must not be weakened merely to share Qt-facing runtime code.

Qt Compatibility analysis also establishes two additional facts:

- QPA/private APIs do not provide the public compatibility guarantees required for a product-wide common layer. QPA-specific code must remain isolated in one compatibility frontend.
- `QGenericPlugin` can provide a zero-code integration path without replacing the native QPA and should therefore be a peer frontend rather than an implementation detail of C++ or QPA integration.

## Decision

HyRemote will use **Core + Common Runtime + four peer integration frontends**, with the four frontends grouped explicitly under `src/integrations/`:

```text
integrations/cpp --------\
integrations/qml ---------> runtime -> core
integrations/generic ----/
integrations/qpa --------/
```

The canonical source roles are:

```text
src/core                  shared UI/protocol/platform-neutral core implementation
src/runtime               shared Qt/product runtime implementation
src/integrations/cpp      Embedded C++ integration frontend
src/integrations/qml      QML integration frontend
src/integrations/generic  QGenericPlugin zero-code integration frontend
src/integrations/qpa      QPA zero-code integration frontend / compatibility shim
```

The four frontends are peers. No frontend is the architectural parent of another frontend.

## `src/core` responsibilities

`src/core` owns stable product semantics that are independent of Qt UI technology and concrete transport/platform implementation:

- Session lifecycle/state/error semantics;
- `RemoteFrame` metadata and storage lifetime;
- bounded scheduling, mailbox and backpressure policy;
- normalized remote input model and dispatch boundary;
- transport-neutral and target-neutral interfaces;
- capability negotiation at the Core abstraction level.

Core must not require QWidget, QQuickWindow/QML, QPA/private API, concrete RFB implementation types, graphics-platform APIs, or SoC/vendor APIs.

## `src/runtime` responsibilities

`src/runtime` owns product implementation common to multiple/all integration frontends that cannot live in the UI-neutral Core. This includes, as applicable:

- Qt Widgets target adapters;
- Qt Quick target adapters;
- concrete RFB transport and transport-security implementation;
- component factories that assemble Core interfaces with Qt/product implementations;
- shared Qt application-surface discovery;
- application surface model;
- composite capture target and input routing;
- automatic application-level access controller used by Generic Plugin and QPA zero-code modes;
- internal runtime configuration/ownership services shared across frontends.

`src/runtime` may depend on Qt public APIs and `HyRemote::Core`. It must not depend on any directory under `src/integrations/`.

## Integration frontend responsibilities

### `src/integrations/cpp` — Embedded C++ integration

Owns the public `HyRemote::RemoteAccess` C++ facade and only the integration-specific facade/ABI surface required for Embedded C++ consumers. It delegates product work to the common runtime.

This frontend is **not a Widgets frontend**. A C++ consumer may target Widgets or Qt Quick; target-family adaptation belongs to Runtime.

### `src/integrations/qml` — QML integration

Owns `import HyRemote` and the QML-facing wrapper/property/lifecycle semantics. It delegates product work to the common runtime and does not implement a second Session/capture/transport architecture.

### `src/integrations/generic` — QGenericPlugin integration

Owns a Qt Generic Plugin payload loaded with `-plugin hyremote` or `QT_QPA_GENERIC_PLUGINS`. It preserves the application's native QPA and platform identity and bootstraps the shared automatic runtime. It must use Qt public Generic Plugin APIs and must not depend on Qt QPA/private APIs.

### `src/integrations/qpa` — QPA integration

Owns the Qt private/QPA compatibility boundary. The long-term design is a **Factory Trampoline**: parse HyRemote platform parameters, resolve/create the qualified native Qt platform integration, bootstrap/arm the same automatic runtime, and return the native `QPlatformIntegration*` to Qt. QPA does not own product capture/input/transport/session semantics.

Only `src/integrations/qpa` may depend on Qt private/QPA ABI for the four integration frontends.

## Why `src/integrations/` is explicit

Keeping `cpp`, `qml`, `generic` and `qpa` directly under `src/` makes those technology names look like shared implementation layers. That is especially misleading for `cpp`, because almost all of HyRemote is C++.

Grouping them under `src/integrations/` makes the ownership model executable by inspection:

```text
src/core       = shared product semantics
src/runtime    = shared product implementation
src/integrations/* = ways an application enters the runtime
```

Widgets and Quick are Runtime adapter dimensions, not integration-directory dimensions.

## Automatic integration ownership

The following code is application-level automatic integration, not QPA implementation, and therefore moves out of the QPA frontend into Common Runtime:

- application surface model;
- composite target and routing;
- interactive composite target;
- application-level automatic-access controller.

Generic Plugin and QPA must use the same automatic controller and surface/composition semantics.

## QPA composition seam

The current source-private `qpa_composition_seam.*` exists because QPA-owned composite code needs Core-owned frame/input operations while QPA is forbidden from linking Core directly. After composite/automatic code is fully owned by Common Runtime, this indirection is no longer architecturally necessary and is removed rather than renamed into another frontend-specific seam.

## QPA platform strategy

HyRemote does not create Rockchip/NXP/TI-specific QPA implementations. The platform chain is:

```text
HyRemote QPA Factory Trampoline
        -> native Qt QPA (windows/xcb/wayland/eglfs/...)
        -> vendor Qt/BSP
        -> OS/GPU/display/input hardware
```

Vendor BSP + Qt owns native display/input/GPU adaptation. Platform-specific HyRemote code is permitted only in the QPA compatibility shim or in a separately justified acceleration backend after portable-path measurements prove it is necessary.

## Compatibility model

- `core`: ordinary C++ compatibility boundary; no Qt private ABI.
- `runtime`: Qt public API/product implementation boundary.
- `integrations/cpp`: public C++ integration ABI/API.
- `integrations/qml`: public Qt/QML integration boundary.
- `integrations/generic`: public Qt Generic Plugin boundary.
- `integrations/qpa`: Qt private QPA boundary, rebuilt and qualified per supported Qt/QPA compatibility unit.

A shared source architecture does not imply one QPA binary across Qt minor lines, CPU architectures or vendor-patched Qt distributions.

## Target product behavior

All four modes share the same product semantics:

1. Embedded C++ API;
2. QML API;
3. Generic Plugin zero-code integration;
4. QPA Factory-Trampoline zero-code integration.

The differences are limited to bootstrap/configuration/target ownership. Session, frame, capture, input, transport, security, cleanup and error semantics are common Runtime/Core behavior.

## Repository target layout

The canonical post-migration shipping tree is:

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

`src/` remains the shipping tree. `runtime` is an internal/shared implementation module, not a fifth application integration mode.

## Build/artifact compatibility during migration

Source ownership and artifact paths are separate contracts. The migration may preserve existing target names, output names and build/deployment locations while source directories change.

In particular, `HyRemoteRemoteAccess` / `HyRemote::RemoteAccess` may continue using the established build/output identity while target ownership moves to `src/runtime` and the C++ facade lives under `src/integrations/cpp`.

Any intentional artifact/API change requires a separate compatibility decision; it is not implied by repository relocation.

## Migration constraints

- Do not change the frozen V1 candidate/release evidence merely to absorb this migration.
- Perform the migration on the dedicated #219 architecture branch/PR.
- Preserve one Core/session/capture/input/transport architecture throughout the migration.
- Do not temporarily solve ownership by making Generic or QPA depend on Embedded C++ implementation details.
- Keep QPA/private headers out of Core, Runtime, C++, QML and Generic modules.
- Remove the legacy physical roots `src/cpp`, `src/qml`, `src/generic` and `src/qpa`; do not leave forwarding directories or compatibility copies.
- Update repository-layout gates, package/deploy contracts, examples and cross-mode tests before the migration is considered complete.

## Consequences

Positive:

- the source tree itself distinguishes shared implementation from application integration;
- `src/integrations/cpp` unambiguously means Embedded C++ API rather than all C++ implementation;
- Widgets and Quick remain equal Runtime target families;
- Generic and QPA become true peer zero-code frontends;
- application-level automatic access is implemented once;
- QPA private compatibility risk is sharply isolated;
- vendor/SoC platform expansion does not create new product personalities;
- the current QPA-specific composition bridge can disappear.

Cost:

- build/install/release-readiness paths must be migrated deliberately;
- historical documentation and CI checks that encode old physical paths must be updated;
- moving automatic/composite implementation requires preserving existing QPA behavior through cross-mode tests.
