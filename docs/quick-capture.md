# Qt Quick capture and input baseline

Status: production integration slice for #28 / V0.0.1.0 on the path to V1.0.0.0.

## Product boundary

Normal applications attach a `QQuickWindow` to `HyRemote::RemoteAccess`. They do not construct a
capture source or input sink. The same public Embedded C++ API is used for Widgets and Quick.

The installed HyRemote package declares Qt Quick as a dependency only when the built package
contains this adapter.

## Capture baseline

The host evidence accepted under #16 is used as the public-API correctness path:

```text
Core scheduler thread
    -> QuickCaptureSource::requestFrame()
    -> queued GUI-thread attempt
    -> QQuickWindow::contentItem()->grabToImage(pixelSize)
    -> ready()
    -> owned CpuFrameStorage RGBA8888 premultiplied copy
    -> RemoteFrame { Completion PTS, FullFrame damage }
    -> Core bounded mailbox
```

The adapter never calls synchronous `QQuickWindow::grabWindow()` in the production path. Capture
requests accepted by Core remain bounded by `maxInFlight`. When a window is hidden/minimized, an
accepted request is retained and retried asynchronously; it does not busy-loop the Core scheduler
or synchronously wait for the scene graph. A ready callback that fails to produce an image is also
retried. A bounded timeout breaks a public-grab completion that becomes stranded across a
visibility transition and retries the same Core request.

Properties of the baseline:

- QQuickWindow/QQuickItem access occurs on the Qt GUI thread;
- frame storage is owned independently of `QQuickItemGrabResult`;
- frame pixel dimensions are derived from current logical window size and DPR once;
- completion time is the safe fallback content PTS;
- damage is conservatively `FullFrame`;
- Core remains responsible for bounded completed-frame backpressure;
- target destruction is surfaced as non-recoverable `TargetLost`.

## Remote input baseline

```text
transport normalization
    -> hyremote::InputEvent
    -> Core InputSink::post()
    -> queued GUI-thread delivery
    -> QQuickWindow
    -> normal Qt Quick event/focus routing
```

The baseline maps remote frame coordinates to the current logical QQuickWindow coordinate space and
injects pointer move/button/wheel, logical key press/release, modifiers and committed UTF-8 text.
Delivery is queued; the transport callback never waits for GUI handling. A destroyed sink drops
queued events.

## Compatibility scope

V0.0.1.0 acceptance targets normal QQuickWindow / Qt Quick 2D content, controls, text focus,
same-scene overlays and local + remote input coexistence on Windows x86_64 and Linux x86_64.

Quick3D, custom `QQuickFramebufferObject`, QQuickWidget composition and native/windowed popup
variants remain separately evidenced compatibility cases. They do not silently widen the minimum
acceptance set.

This public readback path is the correctness baseline, not the final performance ceiling. A future
render-thread/RHI or external-buffer backend may replace capture behind the same Core and
`HyRemote::RemoteAccess` contracts.
