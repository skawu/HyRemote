# Qt Quick Capture and Input Baseline

> Language / 语言: **English** | [中文](../quick-capture.md)

This document describes the current HyRemote product behavior for qualified Qt Quick targets.

Applications attach a `QQuickWindow` through the public HyRemote frontend they selected. They do not construct capture sources or input sinks directly.

## Product boundary

For the programmable C++ API:

```cpp
HyRemote::RemoteAccess remote(quickWindow);
remote.start();
```

The QML API provides a declarative frontend over the same Runtime. Generic Plugin and QPA may discover supported application-owned Quick surfaces automatically.

The Quick target adapter remains an internal Runtime implementation detail.

## Capture baseline

The portable correctness path:

```text
Core scheduler
    -> Quick target adapter
    -> queued GUI-thread request
    -> QQuickWindow::contentItem()->grabToImage(...)
    -> asynchronous ready callback
    -> owned CPU-readable frame storage
    -> RemoteFrame
    -> bounded Core handoff
    -> transport
```

The production baseline avoids synchronous `QQuickWindow::grabWindow()` on the normal path.

Current properties:

- QQuickWindow and QQuickItem access occurs on the Qt GUI thread;
- capture completion is asynchronous;
- frame storage is independent of the temporary Qt grab result;
- pixel dimensions are derived from the current target size and DPR once;
- completion time is used as the safe baseline content timestamp;
- damage may be represented conservatively as full-frame;
- completed-frame handoff remains bounded;
- target destruction is surfaced as product-level target loss instead of leaving stale capture work alive.

If a window becomes temporarily unavailable for capture, the Runtime avoids a busy loop and does not synchronously block the render or UI path waiting for a frame.

## Remote input baseline

Normalized remote input follows:

```text
transport normalization
    -> Core InputEvent routing
    -> Quick InputSink
    -> queued GUI-thread delivery
    -> QQuickWindow
    -> normal Qt Quick focus and event routing
```

The baseline supports the product input categories in [`../input-model.md`](../input-model.md): pointer movement and buttons, wheel steps, logical key transitions, modifiers, and committed text where the transport provides it.

Pointer coordinates map from the remote frame viewport to the current logical QQuickWindow geometry. DPR is not applied twice.

Input delivery is asynchronous and bounded. The transport callback does not wait for the Qt GUI thread to process an event.

## Visibility and lifecycle

A hidden, minimized, destroyed, or rapidly changing Quick window is not treated as an ordinary static framebuffer.

The Runtime keeps capture work bounded and does not publish frames after the target is no longer valid. Normal viewer disconnect and reconnect do not require recreating the application or target window.

## Compatibility scope

The baseline covers qualified Qt Quick 2D application content, including ordinary controls, text focus, and same-scene overlays.

Basic Quick support does not automatically cover:

- Quick3D;
- custom framebuffer or render-node pipelines;
- unusual graphics backends;
- `QQuickWidget` mixed Widgets and Quick composition;
- native or windowed popup variants;
- foreign or native windows outside the normal application-owned Qt surface model.

These cases require explicit compatibility qualification.

## Performance direction

The current public Qt Quick readback path is a correctness baseline, not a zero-copy performance claim.

Future implementations may add:

- render-thread or RHI-aware capture;
- external or GPU-backed frame storage;
- more efficient readback and conversion;
- damage-aware updates;
- embedded low-copy paths;
- hardware-assisted encoding.

These optimizations remain behind the Shared Runtime and Core contracts. Normal applications should not need to replace their C++/QML/Generic/QPA integration model to adopt a faster backend.

## Current status

In the current reference environment, Qt Quick through the C++ API and Generic Plugin is a peer route.

The QML API and QPA are also peer routes. QPA additionally requires exact Qt private-ABI qualification.

See [`../compatibility.md`](../compatibility.md) and [`../known-limitations.md`](../known-limitations.md) for exact product status.
