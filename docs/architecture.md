# HyRemote Architecture Baseline

Status: **Draft baseline for architecture spike**

This document defines the boundaries that implementation work must preserve while the first technical spikes are performed.

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

Targets may include QWidget, QWindow, or QQuickWindow through dedicated adapters.

### 2.2 Declarative QML API

Convenience wrapper for Qt Quick applications. It must reuse the same core/session implementation instead of implementing a separate remote path.

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
Target Adapter
   │
Capture Backend
   │
RemoteFrame
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

### 3.2 Capture Backend

Produces a `RemoteFrame` from a target.

Planned backend classes:

- portable/raster baseline;
- OpenGL readback;
- asynchronous OpenGL/PBO readback;
- GBM/DMA-BUF low-copy path if proven viable.

Capture backends must expose capability metadata so a target can select an appropriate implementation without assuming that every QWidget is raster-only or every Qt Quick target is OpenGL-only.

### 3.3 RemoteFrame

Transport-neutral representation of a frame.

The model is expected to carry, as applicable:

- geometry/size;
- pixel format or external-buffer description;
- presentation timestamp;
- damage region;
- ownership/lifetime information;
- synchronization metadata where required.

The exact public representation is not frozen until SPIKE-01 is complete. The
requirements that SPIKE-01 did establish (ownership/lifetime states, optional damage,
capability metadata) are recorded in [`capture-spike.md`](capture-spike.md) and remain
requirements rather than a frozen ABI.

### 3.4 Transport

Consumes `RemoteFrame` objects and manages remote clients.

The first transport is VNC/RFB using NeatVNC as the preferred candidate. RFB implementation details must not leak into target or capture interfaces.

Future transports may include WebRTC or other low-latency transports without replacing the capture architecture.

### 3.5 Input Adapter

Converts transport input into application input.

The preferred Qt application path is direct Qt event delivery. Linux `uinput` may be offered later as an optional system-level backend but is not the default architecture.

### 3.6 Hardware Encoder

Optional acceleration layer used only by transports/codecs that need encoded video.

Conceptual implementations may include:

- RKMPP;
- V4L2 M2M;
- VA-API;
- other platform backends.

No SoC-specific encoder belongs in `hyremote-core`.

## 4. Dependency rules

`hyremote-core` must not depend directly on:

- NeatVNC;
- EGLFS private implementation details;
- Rockchip MPP;
- GBM/DRM unless an interface requires a generic external-buffer type;
- Qt private headers.

Concrete adapters may depend on these technologies.

## 5. Threading principles

The implementation must not assume that capture, encoding, network I/O, and Qt GUI work occur on the same thread.

Architecture spike work must explicitly determine:

- which callbacks execute on the GUI thread;
- which execute on the Qt scene graph/render thread;
- when GL contexts are current;
- who owns frame buffers and when they may be recycled;
- how backpressure prevents a slow client from blocking rendering.

Blocking GPU readback or network work on a render thread is considered a fallback baseline, not a final performance architecture.

## 6. Compatibility model

HyRemote will publish a compatibility matrix instead of making blanket claims.

Initial cases:

| Application type | Baseline target |
|---|---|
| QWidget/raster | required |
| QWidget with custom QPainter | required |
| QQuickWindow 2D | required |
| QOpenGLWidget | architecture spike |
| QQuickWidget | architecture spike |
| Quick3D | architecture spike |
| custom QQuickFramebufferObject | architecture spike |
| multiple native GL windows | not promised |

## 7. v0.1 architecture constraints

v0.1 should prove correctness before advanced optimization:

1. a Widgets example can be viewed and controlled remotely;
2. a Qt Quick/EGLFS example can be viewed and controlled remotely;
3. both use the same session/core abstractions;
4. local rendering and local input continue to work;
5. the transport is replaceable;
6. no Qt private API is required by the stable core;
7. performance data is collected before PBO, DMA-BUF, or hardware encoding becomes a requirement.

## 8. Explicitly deferred decisions

The following are intentionally not frozen yet:

- exact `RemoteFrame` C++ ABI;
- NeatVNC version and linkage strategy;
- Qt minimum version beyond the initial 6.8.x reference;
- exact QPA proxy implementation;
- DMA-BUF export path from Qt/EGLFS;
- RKMPP integration model;
- H.264-in-RFB versus another transport for high-motion content;
- project license.

These decisions require dedicated issues/ADRs and technical evidence.
