# ADR-0007: Four Integration Frontends over Core + Common Runtime

Status: **Accepted for post-V1 architecture migration (#219)**

## Context

HyRemote currently has three first-class integration technologies under `src/`: `cpp`, `qml` and `qpa`. The repository terminology now correctly names those directories after their integration technology. During the design of a fourth zero-code integration based on `QGenericPlugin`, a structural problem became explicit: `src/cpp` currently owns both the Embedded C++ facade and a large amount of implementation that is in fact shared product runtime, while `src/qpa` also owns application-level automatic surface/composition logic that is not inherently QPA-specific.

That ownership is no longer correct once the product has four peer integration frontends. `src/cpp` is the **Embedded C++ integration mode**, not the common implementation layer. `src/core` remains the common UI/protocol/platform-neutral core implementation, but its existing boundary intentionally excludes QWidget, Qt Quick, QML, concrete transport implementations and Qt private/QPA types. Those restrictions remain valuable and must not be weakened merely to share Qt-facing runtime code.

At the same time, Qt Compatibility analysis establishes two additional facts:

1. QPA/private APIs do not provide the public compatibility guarantees required for a product-wide common layer. QPA-specific code must therefore remain isolated in `src/qpa`.
2. `QGenericPlugin` can provide a zero-code integration path without replacing the native QPA, and should therefore be a peer frontend rather than an implementation detail of C++ or QPA integration.

## Decision

HyRemote will evolve to **Core + Common Runtime + four peer integration frontends**.

```text
cpp --------\
qml ---------> runtime -> core
generic ----/
qpa --------/
```

The canonical source roles are:

```text
src/core      shared UI/protocol/platform-neutral core implementation
src/runtime   shared Qt/product runtime implementation
src/cpp       Embedded C++ integration frontend
src/qml       QML integration frontend
src/generic   QGenericPlugin zero-code integration frontend
src/qpa       QPA zero-code integration frontend / compatibility shim
```

The four frontends are peers. No frontend is the architectural parent of another frontend.

## `src/core` responsibilities

`src/core` continues to own stable product semantics that are independent of Qt UI technology and concrete transport/platform implementation:

- Session lifecycle/state/error semantics;
- `RemoteFrame` metadata and storage lifetime;
- bounded scheduling, mailbox and backpressure policy;
- normalized remote input model and dispatch boundary;
- transport-neutral and target-neutral interfaces;
- capability negotiation at the Core abstraction level.

The existing Core boundary remains in force: Core must not require QWidget, QQuickWindow/QML, QPA/private API, concrete RFB implementation types, graphics-platform APIs, or SoC/vendor APIs.

## `src/runtime` responsibilities

`src/runtime` owns product implementation that is common to multiple/all integration frontends but cannot live in the UI-neutral Core. This includes, as applicable:

- Qt Widgets target adapters;
- Qt Quick target adapters;
- concrete RFB transport and transport-security implementation;
- component factories that assemble Core interfaces with Qt/product implementations;
- shared Qt application-surface discovery;
- application surface model;
- composite capture target and input routing;
- automatic application-level access controller used by Generic Plugin and QPA zero-code modes;
- internal runtime configuration/ownership services shared across frontends.

`src/runtime` may depend on Qt public APIs and `HyRemote::Core`. It must not depend on `src/cpp`, `src/qml`, `src/generic` or `src/qpa`.

## Integration frontend responsibilities

### `src/cpp` — Embedded C++ integration

Owns the public `HyRemote::RemoteAccess` C++ facade and only the integration-specific facade/ABI surface required for Embedded C++ consumers. It delegates product work to the common runtime.

### `src/qml` — QML integration

Owns `import HyRemote` and the QML-facing wrapper/property/lifecycle semantics. It delegates product work to the common runtime and does not implement a second Session/capture/transport architecture.

### `src/generic` — QGenericPlugin integration

Owns a Qt Generic Plugin payload loaded with `-plugin hyremote` or `QT_QPA_GENERIC_PLUGINS`. It preserves the application's native QPA and platform identity and bootstraps the shared automatic runtime. It must use Qt public Generic Plugin APIs and must not depend on Qt QPA/private APIs.

### `src/qpa` — QPA integration

Owns the Qt private/QPA compatibility boundary. The long-term design is a **Factory Trampoline**: parse HyRemote platform parameters, resolve/create the qualified native Qt platform integration, bootstrap/arm the same automatic runtime, and return the native `QPlatformIntegration*` to Qt. QPA does not own product capture/input/transport/session semantics.

Only `src/qpa` may depend on Qt private/QPA ABI for the four integration frontends.

## Automatic integration ownership

The following code is application-level automatic integration, not QPA implementation, and therefore moves out of `src/qpa` into the common runtime:

- `application_surface_model.*`;
- `composite_target.*`;
- `composite_target_routing.*`;
- `interactive_composite_target.*`;
- the application-level logic currently named `hyremote_qpa_remote_controller.*`, renamed to a QPA-neutral automatic-access controller.

Generic Plugin and QPA must use the same automatic controller and surface/composition semantics.

## QPA composition seam

The current source-private `qpa_composition_seam.*` exists because QPA-owned composite code needs Core-owned frame/input operations while QPA is forbidden from linking Core directly. After composite/automatic code moves into the common runtime, this indirection is no longer architecturally necessary and is removed rather than renamed into another frontend-specific seam.

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
- `cpp`: public C++ integration ABI/API.
- `qml`: public Qt/QML integration boundary.
- `generic`: public Qt Generic Plugin boundary.
- `qpa`: Qt private QPA boundary, rebuilt and qualified per supported Qt/QPA compatibility unit.

A shared source architecture does not imply one QPA binary across Qt minor lines, CPU architectures or vendor-patched Qt distributions.

## Target product behavior

All four modes share the same product semantics:

1. Embedded C++ API;
2. QML API;
3. Generic Plugin zero-code integration;
4. QPA Factory-Trampoline zero-code integration.

The differences are limited to bootstrap/configuration/target ownership. Session, frame, capture, input, transport, security, cleanup and error semantics are common runtime/Core behavior.

## Repository target layout

The post-migration shipping tree is:

```text
src/
├── core/
├── runtime/
├── cpp/
├── qml/
├── generic/
└── qpa/
```

`src/` remains the shipping tree. `runtime` is an internal/shared implementation module, not a fifth application integration mode.

## Migration constraints

- Do not change the frozen V1 candidate/release evidence merely to absorb this migration.
- Perform the migration on a dedicated post-V1 architecture branch.
- Preserve one Core/session/capture/input/transport architecture throughout the migration.
- Do not temporarily solve ownership by making Generic or QPA depend on Embedded C++ implementation details.
- Keep QPA/private headers out of Core, Runtime, C++, QML and Generic modules.
- Update repository-layout gates, package/deploy contracts, examples and cross-mode tests before the migration is considered complete.

## Consequences

Positive:

- `src/cpp` again means exactly Embedded C++ integration.
- Generic and QPA become true peer zero-code frontends.
- application-level automatic access is implemented once.
- QPA private compatibility risk is sharply isolated.
- vendor/SoC platform expansion does not create new product personalities.
- the current QPA-specific composition bridge can disappear.

Cost:

- the build/install graph must be migrated deliberately because current `HyRemoteRemoteAccess` binary ownership is rooted in `src/cpp`;
- repository-layout and deployment gates must be updated with the new runtime/generic modules;
- moving automatic/composite implementation requires preserving existing QPA behavior through cross-mode tests.
