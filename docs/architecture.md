# HyRemote Architecture Baseline

Status: **Core contract proposed in ARCH-01 (#5); embedded/performance backends remain evidence-driven**

This document defines the top-level boundaries that HyRemote implementation work must preserve. The implementation-ready Core semantics are refined by:

- [`core-architecture.md`](core-architecture.md);
- [`ADR-0001 Core Boundaries`](adr/0001-core-boundaries.md);
- [`ADR-0002 RemoteFrame Ownership, Damage and Timestamp Contract`](adr/0002-remoteframe-lifetime-timestamps.md);
- [`ADR-0003 Threading, Scheduling and Backpressure`](adr/0003-threading-backpressure.md).

## 1. Problem statement

HyRemote adds remote display and remote input to existing Qt Embedded applications without requiring the application to migrate its UI framework, graphics stack, or business architecture.

The project must support both Qt Widgets and Qt Quick and must not make one application type a compatibility layer for the other.

## 2. Primary integration modes

### 2.1 Embedded C++ API

Reference integration and stable API direction.

Conceptual API:

```cpp
HyRemote::Session session;
session.attach(target);
session.start();
```

Targets may include QWidget, QWindow, or QQuickWindow through dedicated adapters/facades. The public facade must reuse the Core Session rather than place Qt target types in `hyremote-core`.

### 2.2 Declarative QML API

Convenience wrapper for Qt Quick applications. It must reuse the same Core/session implementation instead of implementing a separate remote path.

Conceptual API:

```qml
RemoteAccess {
    target: window
    enabled: true
}
```

### 2.3 Zero-code platform proxy

Optional compatibility mode for applications that cannot be modified. It may use Qt private/QPA APIs but must remain isolated from stable public interfaces.

The proxy is an adapter, not the architecture core.

## 3. Layer model

```text
Application
   │
Target Adapter / facade
   │
CaptureSource
   │
RemoteFrame
   │
Bounded Core mailbox
   │
Transport
   │
Viewer

Viewer
   │
Transport Input
   │
Input Adapter
   │
Qt Application
```

### 3.1 Target Adapter

Describes what is being shared and the application semantics required to reach it.

Initial target families:

- QWidget/raster Widgets;
- QQuickWindow;
- QOpenGLWidget;
- QQuickWidget;
- Quick3D/mixed Quick content.

A target adapter does not own the network protocol.

### 3.2 Capture Backend / CaptureSource

Produces `RemoteFrame` completions from a target and obeys target thread affinity.

Validated/current candidate classes include:

- QWidget public raster capture baseline;
- native QQuickWindow public asynchronous baseline candidate using `contentItem()->grabToImage()`;
- synchronous GL/widget fallbacks where public async capture does not exist;
- future optimized GL/readback or external-buffer paths only when measured evidence justifies them.

Capture sources expose capability metadata so the Session can select/fail early without assuming that every QWidget is raster-only or every Qt Quick target is OpenGL-only.

### 3.3 RemoteFrame

Transport-neutral immutable frame metadata plus owned storage lifetime.

The semantic contract includes:

- geometry/size and pixel/storage description;
- owned CPU or external storage lifetime;
- content PTS whose source is explicit;
- optional request/completion diagnostics;
- damage as `Unknown`, `FullFrame`, or `Regions`;
- backend-neutral capability metadata.

A borrowed raw pointer is not a standalone frame contract. Request time is not content PTS. See ADR-0002.

### 3.4 Backpressure

Capture completion never calls the transport directly. Frames cross a bounded Core mailbox before transport dispatch.

The default near-live policy is `DropOldest / LatestFrameWins`; producer-throttle is a separate explicit policy. Transport implementations additionally own per-client queue/backpressure. See ADR-0003.

### 3.5 Transport

Consumes `RemoteFrame` objects and manages remote clients.

The first transport is VNC/RFB using NeatVNC as the preferred candidate. RFB/AML implementation details must not leak into target, capture or Core interfaces.

Future transports may include WebRTC or other low-latency transports without replacing the capture architecture.

### 3.6 Input Adapter

Converts transport-normalized input into application input and marshals it to the target's required thread.

The preferred Qt application path is direct Qt event delivery. Linux `uinput` may be offered later as an optional system-level backend but is not the default architecture.

### 3.7 Hardware Encoder

Optional acceleration used by transports/codecs that need encoded video.

Conceptual implementations may include:

- RKMPP;
- V4L2 M2M;
- VA-API;
- other platform backends.

Encoding is not a mandatory stage in the Core pipeline. No SoC-specific encoder belongs in `hyremote-core`.

## 4. Dependency rules

`hyremote-core` must not expose or require:

- NeatVNC/AML types;
- QWidget/QQuickWindow/QML types;
- EGLFS private implementation details;
- Rockchip MPP;
- GBM/DRM concrete types;
- Qt private/QPA headers.

Concrete adapters may depend on these technologies.

The Core contract uses ordinary C++17 value/interface types at its boundary. Internal implementation may use QtCore later if useful, provided Qt GUI/private/protocol/platform semantics do not leak into Core public types.

## 5. Threading principles

The implementation does not assume that capture, encoding, network I/O and Qt GUI work occur on the same thread.

Frozen principles:

- target adapters marshal Qt-affine operations to the appropriate Qt thread;
- capture completion performs only bounded hand-off work;
- a Core dispatch worker separates capture from transport runtime;
- transport owns its event loop/thread and client-specific queues;
- remote input is marshalled back to the Qt GUI/target thread;
- storage implementations own any thread-affine recycle/release behavior;
- slow clients do not block capture/render by default.

See ADR-0003 for the detailed sequence.

## 6. Compatibility model

HyRemote publishes a compatibility matrix instead of making blanket claims.

Initial cases:

| Application type | Baseline target |
|---|---|
| QWidget/raster | required |
| QWidget with custom QPainter | required |
| QQuickWindow 2D | required |
| QOpenGLWidget | synchronous public baseline; async gap recorded |
| QQuickWidget | parent QWidget whole-window baseline; embedded Quick async only |
| Quick3D | host public-async evidence accepted; EGLFS unverified |
| custom QQuickFramebufferObject | host public-async evidence accepted; EGLFS unverified |
| multiple native GL windows | not promised |

## 7. v0.1 architecture constraints

v0.1 should prove correctness before advanced optimization:

1. a Widgets example can be viewed and controlled remotely;
2. a Qt Quick/EGLFS example can be viewed and controlled remotely;
3. both use the same Session/Core semantics;
4. local rendering and local input continue to work;
5. the transport is replaceable;
6. no Qt private API is required by the stable Core;
7. frame lifetime, PTS and backpressure are explicit/testable;
8. performance data is collected before PBO, DMA-BUF or hardware encoding becomes a requirement.

## 8. Evidence/deferred decisions

Still intentionally open:

- exact production C++ names/ABI for the proposal under `docs/proposals/`;
- concrete external-buffer/native-handle/fence descriptor (#17);
- final EGLFS capture backend/default tuning (#18);
- exact QPA proxy implementation;
- RKMPP integration model;
- H.264-in-RFB versus another transport for high-motion content;
- normalized keyboard symbol schema;
- project license.

These open items may refine adapters/defaults without violating the Core invariants frozen by ARCH-01.
