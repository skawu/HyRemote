# ADR-0001: HyRemote Core Boundaries

Status: **Proposed for ARCH-01 (#5)**

## Context

HyRemote must support Qt Widgets and Qt Quick through one session model while keeping protocol, Qt private/QPA, GPU/BSP and SoC-specific details outside the stable core. Host capture evidence from #3/#16 also shows that capture APIs differ materially by target family, so the Core must coordinate them without pretending that they share one rendering mechanism.

## Decision

`hyremote-core` owns **session semantics**, not UI-framework or transport implementation details.

The dependency direction is:

```text
                 application-facing modules
        ┌─────────────────┬─────────────────┐
        │ hyremote-widgets│ hyremote-quick  │
        └────────┬────────┴────────┬────────┘
                 │ capture/input adapters
                 ▼
             hyremote-core
       Session / RemoteFrame / queue
                 │
        ┌────────┼─────────┐
        ▼        ▼         ▼
 hyremote-vnc  encoder*  other transport*
     NeatVNC    adapters
```

Optional platform/QPA compatibility code sits beside the application-facing modules and may depend on Qt private APIs, but **nothing in Core may depend on it**.

## Core responsibilities

Core owns:

- `Session` lifecycle and state;
- capture scheduling policy and in-flight bounds;
- transport-neutral `RemoteFrame` metadata and storage lifetime;
- bounded completed-frame queue and drop policy;
- hand-off from capture completion to transport dispatch;
- normalized error/state reporting;
- routing remote input toward an input sink;
- capability negotiation at the level needed to select compatible adapters.

Core does **not** own:

- QWidget, QQuickWindow, QQuickItem or QML object semantics;
- OpenGL/RHI/EGLFS calls;
- NeatVNC/AML calls;
- RFB messages/codecs;
- GBM/DRM/DMA-BUF concrete types;
- RKMPP/V4L2/VA-API concrete types;
- QPA private APIs;
- viewer-specific policy.

## Type-boundary rule

The Core public contract is written in ordinary C++17 value/interface types. Qt GUI types, NeatVNC types and platform-native types must be adapted before crossing the Core boundary.

This does **not** prohibit a Core implementation from using QtCore internally if that later proves useful; it prohibits Qt GUI/private/protocol/platform types from becoming required public Core semantics.

## Modules

### `hyremote-core`

Protocol-neutral session, frame, queue, state/error and extension interfaces.

### `hyremote-widgets`

- QWidget/QPainter target adapter;
- Widgets capture source;
- QWidget-family input sink;
- QOpenGLWidget/QQuickWidget special handling where required.

### `hyremote-quick`

- QQuickWindow target adapter;
- native Quick public async capture candidate (`contentItem()->grabToImage()`);
- synchronous correctness fallback;
- Quick input sink;
- QML `RemoteAccess` wrapper, implemented on top of the same Core session.

### `hyremote-vnc`

- NeatVNC transport adapter;
- AML runtime isolation;
- RFB input translation;
- client-specific transport queues/codecs/authentication.

### `hyremote-qpa`

Optional zero-code integration. Qt-version-sensitive/private code is isolated here and never defines Core semantics.

### encoder/platform modules

Optional RKMPP/V4L2/VA-API/etc. integrations. An encoder may consume Core frames or a platform storage extension, but Core does not require an encoder to run.

## One Session, one shared target in v0.1

For v0.1 a `Session` owns one logical shared target/capture source and one transport instance. A transport may fan out to multiple remote clients internally. Multi-target/multi-display sessions are deliberately not frozen yet.

This keeps the MVP semantics small while remaining compatible with a later multi-display extension.

## Integration modes

All three project integration modes reuse the same Core contract:

1. **Embedded C++ API** — application code constructs/configures a Session through a Widgets/Quick facade.
2. **QML API** — declarative wrapper around the Quick facade/Core Session.
3. **QPA proxy** — creates/configures the same Core components from an isolated platform adapter.

No mode is allowed to create a second capture/transport architecture.

## Encoder placement

Encoding is **not** a mandatory stage in the Core pipeline. Standard RFB can consume CPU frames directly; an H.264-capable transport may insert an encoder downstream. Core therefore freezes the frame/storage lifetime contract required by asynchronous encoders, but does not freeze a codec-specific API in #5.

## Consequences

- Widgets and Quick can use different capture sources without special-casing the Session.
- A future WebRTC/other transport can reuse the same frame/session model.
- #17 may add a concrete external-buffer descriptor without changing Session/backpressure semantics.
- #18 may change which capture backend is preferred on EGLFS without changing the Core contract.
- Qt private/QPA breakage cannot invalidate `hyremote-core` ABI/source semantics.

## Deferred

- exact external-buffer descriptor/handle/fence shape: #17;
- final EGLFS backend preference/performance defaults: #18;
- full normalized keyboard schema: a later input-focused task may refine it without changing the dispatch boundary;
- binary ABI stability: pre-1.0 remains source-compatibility-first.
