# SPIKE-01 Capture Architecture Spike

Status: **host evidence collected; embedded validation outstanding**

Issue: [#3 SPIKE-01 Validate Qt capture paths across Widgets, Quick and EGLFS](https://github.com/skawu/HyRemote/issues/3)

This document records the evidence produced by the throwaway harness in
[`spikes/capture/`](../spikes/capture/). It answers the architecture questions of
issue #3 and recommends the v0.1 capture path for each target family. It does **not**
freeze any `RemoteFrame` ABI and does not define a public HyRemote API.

> **Acceptance status.** The issue is complete only when an evidence-backed decision
> exists for both a QWidget application and a Qt Quick/EGLFS application. The host half
> is complete; the RK3588/EGLFS half could not be executed in this environment and is
> explicitly `unverified` below. See [Open risks](#9-open-risks) and
> [Unverified claims](#10-explicitly-unverified).

## 1. Scope

In scope:

- public-API capture paths for QWidget/raster, QOpenGLWidget, QQuickWidget,
  QQuickWindow 2D, Quick3D and custom `QQuickFramebufferObject` content;
- thread, graphics-context and buffer-lifetime properties of each path;
- damage information obtainable from public Qt APIs;
- comparative measurements with identical scenes and identical timing rules.

Out of scope (by the execution packet): VNC/RFB, H.264, RKMPP, the QPA proxy, any
public API/ABI freeze, and any zero-copy performance claim.

## 2. Environment inventory (S1)

### 2.1 Host environment used for all measurements

| Item | Value |
|---|---|
| OS | Windows 11, kernel 10.0.26200, `productType=windows` |
| CPU architecture | x86_64 |
| GPU / driver | Intel(R) UHD Graphics 770, driver 32.0.101.7088 |
| OpenGL | 4.6.0 (compatibility profile), GLSL 4.60 |
| Qt | 6.8.3 Open Source (runtime and compile version) |
| Compiler | MSVC 19.44.35227.0 (`cl` 14.44.35207), Visual Studio 18 Build Tools |
| Build system | CMake 3.30.5 + Ninja, Release (`/O2`), C++17 |
| QPA platform | `windows` |
| Qt Quick RHI backend | `<default>` (resolves to Direct3D 11 on this host); OpenGL RHI used explicitly where recorded |
| Screen | HP E24 G4, 1920x1080, devicePixelRatio 1, 96 logical DPI |
| Default surface format | 2.0, no explicit profile, swapInterval 1 |

Direct3D 11 was never selected deliberately: it is the platform default, and it is
recorded because it changes the behaviour of one Quick path significantly
(section 8.2). Every evidence file records the effective backend, and the per-window
`graphicsApi` is recorded per sample.

### 2.2 Embedded reference environment

| Item | Value |
|---|---|
| Board | not available in this environment |
| OS / BSP | not available |
| Qt | not available (reference target is 6.8.3 with EGLFS/OpenGL ES) |

No RK3588 or EGLFS result exists. Desktop results are **not** a substitute; the
acceptance criterion of issue #3 is therefore not met yet.

### 2.3 Test scenes

All scenes use a 960x600 logical surface, except the two widget-hierarchy cases which
use a 1000x640 window so that a GL/Quick child and raster siblings can coexist. Every
scene animates continuously and contains both a region that changes on every tick and
static chrome that must not change.

| Case | Scene | Target families covered |
|---|---|---|
| `widgets` | 960x600 window: child widgets, a custom `QPainter` widget, a pulsing region, a non-modal `QDialog`, a `QMenu` popup, `QLineEdit`, `QProgressBar` | QWidget/raster, QWidget with custom QPainter, popups/overlays |
| `quick` | 960x600 `QQuickView`, `QuickScene.qml`: 24 animated tiles, a text ticker, a static panel | QQuickWindow 2D |
| `quick3d` | 960x600 `QQuickView`, `Quick3DScene.qml`: `View3D`, sphere/cube/cylinder, `DirectionalLight`, MSAA | Quick3D |
| `openglwidget` | 1000x640 window: `QOpenGLWidget` (729x568) plus raster siblings | QOpenGLWidget |
| `quickwidget` | 1000x640 window: `QQuickWidget` (729x568, same 2D scene) plus raster siblings | QQuickWidget |
| `customfbo` | 960x600 `QQuickView`, `CustomFboScene.qml`: a `QQuickFramebufferObject` at (40,40) 280x280 whose renderer clears with a clear color encoding a monotonically increasing counter | custom Quick FBO/OpenGL content |

### 2.4 Measurement rules

- 15-20 warm-up captures are executed and discarded, except for the reported `first`
  (cold) cost of each path.
- Each measured iteration: advance the scene, run the application event loop for the
  requested interval (16 ms unless stated otherwise), then capture.
- `capture` times are wall-clock times of the capture call itself (average, p50, p95,
  max over the measured frames).
- Process CPU is the process-wide CPU time delta divided by wall time, as a percentage
  of one core. It includes the harness worker thread and any Qt render thread.
- GUI latency is the delay of a 16 ms precise timer owned by the harness relative to
  its expected firing time; a proxy for "does the local UI visibly degrade".
- `iteration` splits one iteration into application event processing, the capture calls,
  and the remainder, which is what makes the Direct3D 11 result in section 8.2
  visible.
- Damage is measured by an application-level event filter that unions
  `QPaintEvent::region()` and counts `QEvent::UpdateRequest` (section 6).
- Frames are handed to a worker thread that re-verifies the pixel checksum 40 ms later
  (section 7).

### 2.5 Repeatability and measurement variance

The same configuration was run repeatedly where the number drives a recommendation.
For `widgets`, `quickwidget` and `openglwidget` the harness was run with `--runs 3`,
which also re-creates and tears down the target three times and therefore covers the
"repeated start/stop works" requirement of the spike.

| Observation | Result |
|---|---|
| Repeated start/stop | 6 constructions of the widget targets and 3 of the QOpenGLWidget target, all runs captured every requested frame; no path failed to re-initialize |
| `widgets` totals over 3 x 60 frames | 33.6 / 33.3 / 33.3 fps; 12.78 / 13.20 / 13.11 ms of capture work per iteration |
| `widgets` per-path average over the 3 runs | `grab()` 3.38 / 3.47 / 3.48 ms; reused target 2.51 / 2.87 / 2.79 ms |
| `openglwidget` totals over 3 x 60 frames | 34.2 / 34.5 / 34.5 fps; 12.31 / 12.38 / 12.19 ms of capture work per iteration |
| `openglwidget` per-path average over the 3 runs | `grabFramebuffer()` 3.49 / 3.61 / 3.41 ms; parent `grab()` 4.83 / 4.84 / 4.63 ms |
| `quickwidget` `grabFramebuffer()` over the 3 runs | 3.05 / **7.78** / 3.21 ms - unstable |
| `quickwidget` parent `grab()` over the 3 runs | 5.19 / 5.52 / 6.00 ms - stable |
| Whole-run outlier | One 150-frame `openglwidget` run was 1.7x slower than every other run of the same configuration (19.9 fps, 22.37 ms of capture work per iteration). It is kept in the evidence set and marked as an outlier; desktop and GPU load is not controlled. |

Consequences for reading this document:

- Numbers are **comparative evidence with roughly 20-40 % run-to-run spread**, not
  production benchmark claims.
- `QQuickWidget::grabFramebuffer()` varied by more than 4x across identical runs
  (3.05 ms to 13.30 ms) while the parent `grab()` stayed between 4.3 and 6.0 ms. The
  recommendation for that target therefore depends on the stable path, not on the
  fastest single observation.
- The widget-target numbers come from one 150-frame run plus three 60-frame runs; the
  Quick numbers come from single 150-frame runs, because they are far more expensive.

## 3. Results: QWidget / raster (case A)

References: `host-windows-rhi-default-widgets-and-gl-widgets.json` (150 frames) and
`host-windows-rhi-default-repeatability.json` (3 runs x 60 frames), 960x600.

| Capture path | first | avg (150-frame run) | avg range over 3 runs | p95 (150-frame run) | returned frame |
|---|---|---|---|---|---|
| `QWidget::grab()` (primary) | 7.84 ms | 3.82 ms | 3.38-3.48 ms | 7.09 ms | 960x600 `RGB32` |
| `QWidget::render()` into a reused `QImage` | 2.29 ms | 2.93 ms | 2.51-2.87 ms | 5.68 ms | 960x600 `ARGB32_Premultiplied` |
| `QWidget::render()` into a fresh `QImage` | 2.61 ms | 2.75 ms | 2.70-2.91 ms | 5.70 ms | 960x600 `ARGB32_Premultiplied` |
| `QWidget::render()` into a borrowed raw pixel buffer | 2.28 ms | 2.87 ms | 2.78-3.07 ms | 6.35 ms | 960x600 `ARGB32_Premultiplied` |

Run-level figures for the 150-frame run: 150/150 captures succeeded, 32.5 fps at 16 ms
pacing, 13.62 ms of capture work per iteration, process CPU 54.4 % of one core,
RSS 64.1 -> 64.5 MiB (slope -772 KiB / 100 frames).

A 500-frame run at `--interval-ms 0`
(`host-windows-rhi-default-widgets-memory-run.json`) reached 79.9 fps with 9.71 ms of
capture work per iteration, 96.1 % of one core, and RSS 106.8 -> 105.1 MiB
(slope -426 KiB / 100 frames). No memory growth was observed over 500 captures.

Additional findings:

- `grab()` returns `RGB32` while `render()` into an explicitly created image returns
  `ARGB32_Premultiplied`. A caller that renders into its own image also chooses the
  format; downstream code must not assume one format.
- Rendering into a caller-owned image is the cheapest and most controllable option.
  The difference between reusing and freshly allocating the target is within noise,
  for the reason established in section 7.
- The popup `QMenu` and the non-modal `QDialog` are separate top-level windows; the
  captured frame contains neither. The harness observed 3 visible top-level widgets
  while only one was shared. A target adapter needs an explicit policy for extra
  top-level windows.
- Cold cost is not negligible: the first `grab()` in the 150-frame run took 7.84 ms
  against a 3.82 ms steady state.

## 4. Results: OpenGL and mixed cases (cases C and D)

### 4.1 QOpenGLWidget inside a QWidget hierarchy (case C)

References: `host-windows-rhi-default-openglwidget-repeatability.json` (3 runs x 60
frames, authoritative for the steady state) and the 150-frame run in
`host-windows-rhi-default-widgets-and-gl-widgets.json` (marked outlier).
Window 1000x640, GL widget 729x568.

| Capture path | first | avg over 3 runs | p95 over 3 runs | returned frame |
|---|---|---|---|---|
| `QOpenGLWidget::grabFramebuffer()` (primary) | 55.6-61.2 ms | 3.41 / 3.49 / 3.61 ms | 4.56-5.18 ms | 729x568 `ARGB32_Premultiplied` |
| `QWidget::grab()` on the GL widget | 3.80-6.99 ms | 3.90 / 3.97 / 4.12 ms | 5.05-5.49 ms | 729x568 `ARGB32_Premultiplied` |
| `QWidget::grab()` on the parent widget | 4.30-6.32 ms | 4.63 / 4.83 / 4.84 ms | 6.46-6.98 ms | 1000x640 `RGB32` |

Run-level figures: 34.2-34.5 fps, 12.19-12.38 ms of capture work per iteration, CPU
51.2-67.4 % of one core, RSS flat in runs 1 and 2 (the first run grows by 3.7 MiB
while the GL context and FBO are initialized).

Findings:

- The first `grabFramebuffer()` costs 55-61 ms in every run; steady state is 3.5 ms.
  A capture path must be warmed before its first frame is published, or the first
  remote frame pays the initialization.
- `QWidget::grab()` on the **parent** returns the composition of the GL child and the
  raster siblings. Qt does compose the `QOpenGLWidget` into the widget backing store,
  so a mixed raster+GL tree can be captured with a single raster call at 4.6-4.8 ms,
  which is cheaper than `grabFramebuffer()` plus manual composition.
- `grabFramebuffer()` returns only the GL widget, at the GL widget's size, so the
  caller would have to compose everything else itself.

### 4.2 QQuickWidget inside a QWidget hierarchy (case D)

References: `host-windows-rhi-default-repeatability.json` (3 runs x 60 frames) and the
150-frame run in `host-windows-rhi-default-widgets-and-gl-widgets.json`.
Window 1000x640, Quick target 729x568.

| Capture path | first | avg per run | p95 per run | returned frame |
|---|---|---|---|---|
| `QQuickWidget::grabFramebuffer()` (primary) | 2.64 ms | 3.05 / 7.78 / 3.21 ms | 4.25 / 18.58 / 4.63 ms | 729x568 `RGBA8888_Premultiplied` |
| `QWidget::grab()` on the parent widget | 4.55 ms | 5.19 / 5.52 / 6.00 ms | 9.28 / 9.07 / 11.92 ms | 1000x640 `RGB32` |

Run-level figures: 28.7-30.7 fps, 8.26-13.32 ms of capture work per iteration, CPU
34.3 % of one core, RSS stable.

Findings:

- `grabFramebuffer()` for this target is **not stable**: 3.05, 7.78 and 3.21 ms in
  three identical runs, plus 13.30 ms in an earlier run of the same configuration. It
  also returns a different size and pixel format than the parent capture.
- The parent capture is stable at 4.3-6.0 ms and already contains the Quick content,
  so a whole-window sharing target can use one raster call for a `QQuickWidget`
  application. The recommendation is based on stability and on returning the whole
  window in one image, **not** on it being cheaper.
- Input coordinates differ between the two paths: a viewer coordinate must be mapped
  from the parent geometry into the `QQuickWidget` coordinate space before delivery.

## 5. Results: Qt Quick (case B) and Quick3D / custom FBO (case E)

Quick runs were executed with the OpenGL RHI, because the default Direct3D 11 backend
shows the behaviour described in section 8.2.

`host-windows-rhi-opengl-quick-quick3d.json` (150 frames, 16 ms pacing):

| Case | Capture path | first | avg | p50 | p95 | max | returned frame |
|---|---|---|---|---|---|---|---|
| `quick` | `QQuickWindow::grabWindow()` (primary) | 19.11 ms | 18.79 ms | 17.83 ms | 23.35 ms | 24.83 ms | 960x600 `RGBA8888_Premultiplied` |
| `quick3d` | `QQuickWindow::grabWindow()` (primary) | 18.09 ms | 19.30 ms | 19.10 ms | 23.39 ms | 25.44 ms | 960x600 `RGBA8888_Premultiplied` |

| Case | fps | capture work / iteration | process CPU | RSS slope | render-thread renders | update requests |
|---|---|---|---|---|---|---|
| `quick` | 25.4 | 18.80 ms | 21.5 % of one core | +26 KiB / 100 frames (168.5 -> 169.0 MiB) | 591 | 354 |
| `quick3d` | 24.5 | 19.31 ms | 22.7 % of one core | -2066 KiB / 100 frames (208.5 -> 206.3 MiB) | 606 | 368 |

Quick3D is therefore not measurably more expensive than the 2D scene at this size: the
cost is dominated by a full scene render plus a GPU->CPU readback, not by the content.
The scene graph rendered 591-606 times while 150 captures were taken, on a thread other
than the GUI thread.

The asynchronous `QQuickItem::grabToImage()` path, measured separately
(`host-windows-rhi-default-quick-grabtoimage-only.json`,
`host-windows-rhi-opengl-quick-grabtoimage-only.json`):

| Backend | first | avg | p50 | p95 | max | loop fps |
|---|---|---|---|---|---|---|
| Direct3D 11 | 37.83 ms | 38.36 ms | 34.29 ms | 39.95 ms | 42.11 ms | 15.8 |
| OpenGL | 29.42 ms | 23.48 ms | 20.71 ms | 33.94 ms | 35.95 ms | 23.8 |

`grabToImage()` is asynchronous in the sense that the call itself returns immediately,
but the reported number is the end-to-end latency until the image becomes valid, which
is bounded by the event-loop cadence. It does not block the GUI thread, yet it cannot be
driven at a fixed cadence without extra buffering.

`host-windows-rhi-opengl-custom-fbo.json` (150 frames):

| Capture path | first | avg | p50 | p95 | max |
|---|---|---|---|---|---|
| `QQuickWindow::grabWindow()` (primary) | 18.35 ms | 17.31 ms | 17.06 ms | 21.58 ms | 22.75 ms |
| `QQuickItem::grabToImage()` | 29.12 ms | 18.19 ms | 17.08 ms | 29.25 ms | 30.11 ms |

18.7 fps, 35.52 ms of capture work per iteration, CPU 23.7 % of one core, RSS
+43 KiB / 100 frames.

Content fidelity check: the pixel inside the custom FBO region decoded an encoded value
of 24 at the start and 743 at the end of the run, while the renderer produced 743
frames. The baseline capture therefore contains freshly composed custom OpenGL content,
not a stale or blank buffer.

The same case on Direct3D 11 (`host-windows-rhi-d3d11-custom-fbo-unsupported.json`)
reports `graphicsApi = Direct3D11` and `rendererFrameCount = 0`: the
`QQuickFramebufferObject` renderer never ran, so that target is `unsupported` on a
non-OpenGL RHI backend and any pixel decoded from the region is meaningless.

## 6. Damage information from public APIs

The harness installs an application-level event filter, which receives the events Qt
delivers to every object in the application. That is enough to read
`QPaintEvent::region()` (public) without touching `QBackingStore` internals, and to
count `QEvent::UpdateRequest`.

| Target family | region-carrying paint events | update requests | app damage (share of frame area) | capture-induced damage |
|---|---|---|---|---|
| `widgets` (150 frames) | 6772 | 188 | 69.8 % | 79.7 % |
| `openglwidget` (150 frames) | 1200 | 150 | 69.3 % | 78.4 % |
| `quickwidget` (150 frames) | 1270 | 240 | 69.0 % | 78.4 % |
| `quick` (150 frames) | 0 | 354 | not measurable | not measurable |
| `quick3d` (150 frames) | 0 | 368 | not measurable | not measurable |
| `customfbo` (150 frames) | 0 | 329 | not measurable | not measurable |

Conclusions:

- Every Widgets-family target (raster, `QOpenGLWidget`, `QQuickWidget`) exposes
  region-level damage through public APIs.
- No Qt Quick target exposes a damage region: Qt Quick posts `QEvent::UpdateRequest`
  without geometry. A shared damage abstraction can therefore only be **optional**:
  `RemoteFrame` must accept "damage unknown" and fall back to a full frame.
- Capturing a widget also causes damage: capture-induced repaint area is larger than
  the application's own damage in every widget case (79.7 % vs 69.8 % for `widgets`).
  A naive "capture, then report the region that changed" loop would feed its own
  repaints back into the damage stream. The design must take the damage snapshot before
  capturing (which is what the harness does) or explicitly tag capture-induced
  repaints.

## 7. Frame hand-off, buffer ownership and lifetime (S5)

The probe hands captured frames to a worker thread that re-reads the pixels after 40 ms
and compares a checksum with the value recorded at hand-off time. Two hand-off forms are
compared: a `QImage` value (implicitly shared) and a borrowed view onto producer-owned
raw memory (a `QImage` constructed over the pointer without copying).

| Hand-off form | delivered | stable | mutated after hand-off |
|---|---|---|---|
| `QImage` value (`grab()`, `render()` paths) | 45 / 75 / 15 / 30 | 100 % | 0 |
| borrowed raw pixel buffer (pointer-only view) | 15 / 25 | 0 % | **100 %** |

The two runs that include the borrowed path
(`host-windows-rhi-default-widgets-and-gl-widgets.json` and
`host-windows-rhi-default-widgets-memory-run.json`) detected 15 of 15 and 25 of 25
borrowed hand-offs as rewritten, and never a single `QImage` hand-off in any run.

Two consequences that shape `RemoteFrame`:

1. **A borrowed pointer is not a frame.** A consumer that only receives a pointer to a
   producer-owned buffer sees corrupted data as soon as the producer captures the next
   frame. Any zero-copy path (GBM/DMA-BUF, GL PBO, external texture) must transfer
   ownership explicitly or carry a release callback; the frame model needs an ownership
   state, not just an address.
2. **`QImage` is safe but forces reallocation.** Copy-on-write protects `QImage`
   consumers, but the producer pays for it: publishing a shallow `QImage` copy of the
   reused target buffer caused **519 reallocations in 520 capture attempts** in the
   500-frame run (169 in 170 attempts in the 150-frame run). "Reuse one buffer" does not
   avoid a per-frame allocation, and it explains why the reused target (2.93 ms) is not
   cheaper than a freshly allocated one (2.75 ms) or the borrowed buffer (2.87 ms).

The v0.1 baseline should therefore publish frames with an explicit ownership transfer
(pool buffer handed to the consumer and returned after use) instead of a shared
`QImage`, and keep copy-on-write `QImage` as the correctness fallback.

## 8. Threads, contexts and cross-cutting findings (S5)

### 8.1 Thread and context matrix

| Target | Capture API | Caller thread | Graphics context | Render-thread involvement | Blocks the GUI thread |
|---|---|---|---|---|---|
| QWidget/raster | `QWidget::grab()`, `QWidget::render()` | GUI thread only | none (raster) | none | yes, for the whole repaint |
| QOpenGLWidget | `grabFramebuffer()` | GUI thread only | made current internally | none | yes, includes a synchronous readback |
| QOpenGLWidget in a hierarchy | `QWidget::grab()` on the parent | GUI thread only | handled by Qt | none | yes, includes the GL child composition |
| QQuickWidget | `grabFramebuffer()`, parent `grab()` | GUI thread only | own offscreen render target | own render loop inside the widget | yes |
| QQuickWindow / Quick3D / custom FBO | `QQuickWindow::grabWindow()` | GUI thread only | not required from the caller | yes: `beforeRendering`/`afterRendering` fired 591-606 times per 150 frames on a non-GUI thread | yes, it synchronizes with the render thread |
| QQuickItem (any Quick target) | `QQuickItem::grabToImage()` | GUI thread only | not required from the caller | yes | no, but it completes one event-loop iteration later |

Every capture path in this spike executed on the GUI thread (`caller Qt mainThread
(gui=yes)` in every evidence file). The harness deliberately did **not** call
`grabWindow()` from the render-thread hooks: Qt documents the call as GUI-thread only
and the experiment carries a deadlock risk. That path is `unverified` and belongs to the
GL/PBO follow-up.

What can safely be handed to another thread: a completed, immutable frame. Both a
`QImage` snapshot and an explicitly transferred buffer qualify; a borrowed pointer does
not (section 7). The capture calls themselves are not thread-safe and must stay on the
GUI thread for every public API tested here.

### 8.2 Direct3D 11 makes per-frame `grabWindow()` unusable on this host

This is the most significant host-specific result and it is not visible in the capture
cost alone.

| Configuration | Evidence file | loop fps | app event time / iteration | capture call |
|---|---|---|---|---|
| Quick, no capture at all (control) | `...-default-quick-no-capture-control.json` | 57.1 | 17.50 ms | - |
| Quick + `grabWindow()`, Direct3D 11 | `...-default-quick-grabwindow-only.json` | **1.0** | **1000.98 ms** | 15.36 ms |
| Quick + `grabWindow()`, OpenGL RHI | `...-opengl-quick-quick3d.json` | 25.4 | 20.22 ms | 18.79 ms |
| Quick + `grabToImage()` only, Direct3D 11 | `...-default-quick-grabtoimage-only.json` | 15.8 | 24.66 ms | 38.36 ms |

With the default Direct3D 11 backend, capturing once per iteration drops the
application's own event loop to about one iteration per second, while the measured
capture call still reports only 15.36 ms and the GUI latency probe reaches a p95 of
1002 ms. The stall is therefore not inside `grabWindow()`; it delays the application's
event processing. With the same window, the same scene and no capture at all, the loop
runs at its requested cadence, so the throttling is induced by the capture path rather
than by window occlusion.

This is recorded as a host/Windows/Direct3D 11 limitation. Whether an equivalent effect
exists on EGLFS/OpenGL ES is unknown, and it is one of the reasons the embedded half of
issue #3 must still be executed.

## 9. Open risks

1. **Embedded validation missing.** No RK3588/EGLFS/OpenGL ES evidence exists. The
   acceptance criterion of issue #3 is not met.
2. **`grabWindow()` contends with the render thread.** Section 8.2 shows the effect can
   be severe on one backend; on every backend the call must also serialize with the
   scene graph. A production baseline needs a render-thread or asynchronous readback
   path.
3. **No public non-blocking GL readback.** `grabFramebuffer()` and `grabWindow()` both
   pay a full render plus a synchronous readback on the GUI thread.
4. **No public damage region for Qt Quick** (section 6), so bandwidth reduction for
   Quick needs a private or application-assisted path.
5. **Buffer ownership is the central design problem**, not the pixel format
   (section 7).
6. **Extra top-level windows** (popups, menus, dialogs, tooltips, native child windows)
   are excluded from a top-level widget capture; a composition policy is required.
7. **Measurement spread of 20-40 %** on this uncontrolled desktop, and a path
   (`QQuickWidget::grabFramebuffer()`) that varied by more than 4x (section 2.5).
8. **Multiple native GL windows** are not addressed by any path tested here.
9. **`QQuickWidget`/`QOpenGLWidget` on EGLFS** may behave differently because there is
   no window system to composite the child GL surface; unverified.

## 10. Explicitly unverified

- RK3588, EGLFS, OpenGL ES: every case.
- Linux desktop (X11/Wayland) host behaviour.
- Calling any Quick capture API from the scene graph render thread.
- `QWidget` capture with a device pixel ratio other than 1.
- High-resolution / 4K capture cost, and any frame rate above the measured ones.
- Input delivery, backpressure and frame pacing (not part of this spike).
- Out-of-process zero-code capture (a QPA/platform plugin or an injected proxy). The
  harness only proves what an in-process observer can do with public APIs.

## 11. Conclusions and recommendations (S7)

### 11.1 Recommended v0.1 baseline for QWidget / Widgets

**`QWidget::render()` into a caller-owned buffer, driven from the GUI thread, with
`QWidget::grab()` as the convenience equivalent.**

Rationale: it is the cheapest and most controllable path measured (2.5-2.9 ms at
960x600), it lets the caller choose the pixel format and a pool buffer, it composes GL
children (`QOpenGLWidget`) through the parent capture, and it provides region-level
damage through public APIs.

Required companion decisions:

- publish frames with an explicit ownership transfer instead of a shared `QImage`
  (section 7);
- take the damage snapshot before capturing and exclude capture-induced repaints
  (section 6);
- define the policy for popups, menus, dialogs and other top-level windows;
- define the shared surface format explicitly: `grab()` returns `RGB32`, `render()`
  returns the format of the target the caller provides.

### 11.2 Recommended v0.1 baseline for Qt Quick / EGLFS

**`QQuickWindow::grabWindow()` from the GUI thread, full frames only, at a paced
cadence with backpressure, as an explicitly experimental baseline.**

Rationale: it is the only public API that returns the final composed content for a
native Quick window, it covers Qt Quick 2D, Quick3D and custom `QQuickFramebufferObject`
content (the fidelity check in section 5 passed), and it needs no Qt private API. Its
limits must be stated: ~19 ms per 960x600 frame (about 53 fps ceiling), a synchronous
GUI-thread block, no damage regions, and a full-frame update for every viewer refresh.

`QQuickItem::grabToImage()` is not the baseline: its latency is bound to the event loop
and it delivers one frame later, but it is the useful starting point for the GL/PBO
follow-up because it does not block the GUI thread.

`QQuickWidget` applications should be captured with the parent
`QWidget::render()`/`grab()` path: it is stable at 4.3-6.0 ms and returns the whole
window in one image, whereas `QQuickWidget::grabFramebuffer()` varied by more than 4x.

### 11.3 Recommended `RemoteFrame` requirements (no ABI freeze)

- geometry and stride;
- pixel format, or an external-buffer description;
- presentation timestamp;
- **explicit buffer ownership/lifetime state**: producer-owned-and-recycled (only with
  a release callback), ownership-transferred, or immutable snapshot. A borrowed pointer
  is not sufficient and the evidence shows it is corrupted within one frame;
- optional damage region with an explicit `unknown`/`invalid` state and a defined
  full-frame fallback (required for every Qt Quick target);
- capture capability metadata on the backend: needs-a-current-graphics-context,
  blocks-the-GUI-thread, allocates-per-frame, supports-region-damage;
- no Qt private type and no raw owning pointer in the stable core.

### 11.4 Public API versus private/QPA dependency

Public APIs used by the recommended baseline:

- `QWidget::grab()`, `QWidget::render()`, `QPaintEvent::region()` (read through an
  application-level event filter), `QEvent::UpdateRequest`;
- `QOpenGLWidget::grabFramebuffer()`;
- `QQuickWidget::grabFramebuffer()`;
- `QQuickWindow::grabWindow()`, `QQuickItem::grabToImage()`;
- `QQuickWindow::beforeRendering`/`afterRendering` (observation only),
  `QSGRendererInterface::graphicsApi()` (reporting only).

**Private API required for the recommended v0.1 baseline: no.**

Known public-API gaps that later work must fill with private or platform-specific code,
each behind an optional adapter:

- no public damage region for Qt Quick;
- no public non-blocking (asynchronous) GL readback;
- no public capture path that runs on the scene graph render thread;
- no public API to make `QQuickWindow::grabWindow()` cheap on backends like
  Direct3D 11.

### 11.5 What a zero-code integration mode can and cannot do

An in-process observer that may run code inside the application process but must not
change the application can, using public APIs only:

- find the application's top-level `QWidget`s and `QWindow`s
  (`QApplication::topLevelWidgets()`, `QGuiApplication::topLevelWindows()`);
- capture them (`QWidget::render()`, `QQuickWindow::grabWindow()`);
- observe raster damage application-wide through an application-level event filter
  (exactly what the harness's damage tracker does).

It cannot, using public APIs only:

- obtain damage regions for Qt Quick windows;
- capture a Quick window from a non-GUI thread;
- capture without paying a synchronous full render plus readback on the GUI thread;
- compose popups, menus and dialogs, which are separate top-level windows.

A truly out-of-process zero-code proxy (QPA plugin, `LD_PRELOAD`, platform hook) was not
tested and remains an open architectural question for a dedicated issue.

## 12. Follow-up work

1. **GL/PBO asynchronous readback** for `QOpenGLWidget`, `QQuickWidget` and
   `QQuickWindow`, including a render-thread capture path and its synchronization rules:
   [#16](https://github.com/skawu/HyRemote/issues/16).
2. **GBM/DMA-BUF zero-copy feasibility** from Qt/EGLFS, plus the explicit `RemoteFrame`
   ownership model that the evidence in section 7 requires:
   [#17](https://github.com/skawu/HyRemote/issues/17).
3. **Qt Quick damage**: private renderer integration versus application-declared dirty
   hints, with a full-frame fallback.
4. **RK3588/EGLFS validation** of the whole matrix in this document, which is the actual
   acceptance gate of issue #3. This is the blocker that keeps SPIKE-01 open.
5. **Buffer pool and frame lifetime API design** for `hyremote-core`, driven by the
   ownership result rather than by a pixel-format discussion.

## 13. Evidence index and reproduction

Evidence files (machine-readable, one per invocation) live in
[`spikes/capture/evidence/`](../spikes/capture/evidence/):

| File | Content |
|---|---|
| `host-windows-rhi-default-widgets-and-gl-widgets.json` | `widgets`, `openglwidget`, `quickwidget`, 150 frames each, default (Direct3D 11) backend |
| `host-windows-rhi-default-repeatability.json` | `widgets` and `quickwidget`, 3 x 60 frames, repeated start/stop |
| `host-windows-rhi-default-openglwidget-repeatability.json` | `openglwidget`, 3 x 60 frames, repeated start/stop |
| `host-windows-rhi-default-widgets-memory-run.json` | `widgets`, 500 frames, interval 0, memory trend |
| `host-windows-rhi-opengl-quick-quick3d.json` | `quick`, `quick3d`, 150 frames, OpenGL RHI |
| `host-windows-rhi-opengl-custom-fbo.json` | `customfbo`, 150 frames, OpenGL RHI |
| `host-windows-rhi-d3d11-custom-fbo-unsupported.json` | `customfbo` on Direct3D 11; renderer never ran |
| `host-windows-rhi-default-quick-grabwindow-only.json` | the ~1 s per-frame stall of section 8.2 |
| `host-windows-rhi-default-quick-grabtoimage-only.json` | `grabToImage()` on Direct3D 11 |
| `host-windows-rhi-opengl-quick-grabtoimage-only.json` | `grabToImage()` on OpenGL |
| `host-windows-rhi-default-quick-no-capture-control.json` | control run without any capture, Direct3D 11 |
| `host-windows-rhi-opengl-quick-no-capture-control.json` | control run without any capture, OpenGL |

Build and run:

```bash
cmake -S spikes/capture -B build/spike-capture -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=<qt-install>
cmake --build build/spike-capture
./build/spike-capture/hyremote-capture-spike --list
./build/spike-capture/hyremote-capture-spike \
      --sample widgets --frames 150 --warmup 20 --json /tmp/widgets.json
ctest --test-dir build/spike-capture --output-on-failure
```

See [`spikes/capture/README.md`](../spikes/capture/README.md) for the full option list
and platform notes.

## 14. Compatibility matrix

The status column uses the definitions of [`docs/compatibility.md`](compatibility.md).
The same host rows are recorded there; every embedded row stays `Unverified`.

| Qt | OS / target | QPA / graphics | Application type | Capture backend | Status | Notes |
|---|---|---|---|---|---|---|
| 6.8.3 | Windows 11 / x86_64 | `windows` / Direct3D 11 | QWidget/raster | `QWidget::render()` into a caller buffer (primary), `QWidget::grab()` | Experimental | 2.5-3.8 ms at 960x600; region damage available; popups excluded |
| 6.8.3 | Windows 11 / x86_64 | `windows` / Direct3D 11 | QWidget with custom `QPainter` | same | Experimental | Covered by the `widgets` scene |
| 6.8.3 | Windows 11 / x86_64 | `windows` / Direct3D 11 | QOpenGLWidget in a hierarchy | parent `QWidget::grab()` (primary), `grabFramebuffer()` | Experimental | Parent grab composes GL content (4.6-4.8 ms); `grabFramebuffer()` returns only the GL widget; first call 55-61 ms |
| 6.8.3 | Windows 11 / x86_64 | `windows` / Direct3D 11 | QQuickWidget in a hierarchy | parent `QWidget::grab()` (primary), `grabFramebuffer()` | Experimental | Parent grab stable at 4.3-6.0 ms; `grabFramebuffer()` varied 3.05-13.30 ms |
| 6.8.3 | Windows 11 / x86_64 | `windows` / OpenGL RHI | Qt Quick 2D | `QQuickWindow::grabWindow()` | Experimental | 18.8 ms at 960x600; blocks the GUI thread; no damage regions |
| 6.8.3 | Windows 11 / x86_64 | `windows` / OpenGL RHI | Quick3D | `QQuickWindow::grabWindow()` | Experimental | 19.3 ms; content fidelity verified |
| 6.8.3 | Windows 11 / x86_64 | `windows` / OpenGL RHI | custom `QQuickFramebufferObject` | `QQuickWindow::grabWindow()` | Experimental | Freshness verified with an encoded render counter (24 -> 743 over 743 rendered frames) |
| 6.8.3 | Windows 11 / x86_64 | `windows` / Direct3D 11 | custom `QQuickFramebufferObject` | none | Unsupported | Renderer never ran on a non-OpenGL RHI backend |
| 6.8.3 | Windows 11 / x86_64 | `windows` / Direct3D 11 | Qt Quick 2D | `QQuickWindow::grabWindow()` once per frame | Unsupported | Application loop fell to ~1 fps (section 8.2) |
| 6.8.x | Embedded Linux / RK3588 | EGLFS / OpenGL ES | all | recommended by SPIKE-01, unverified on target | Unverified | Acceptance gate of issue #3; not executed |
