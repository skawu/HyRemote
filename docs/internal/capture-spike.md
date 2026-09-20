# SPIKE-01 Capture Architecture Spike

Status: **host evidence collected (Round-2 corrected); embedded validation outstanding**

> **Retired.** The harness this document was produced with was removed from the mainline once the capture architecture
> decision it recommends had been implemented in the product: Widgets capture (`docs/widgets-capture.md`) and Quick
> capture (`docs/quick-capture.md`), with the ownership, timestamp and backpressure rules frozen in ADR-0001, ADR-0002
> and ADR-0003. This document stays as the record of what was measured and decided. The harness and its recorded
> evidence files are retrievable from history:
> `git show 3e6e191:research/capture/README.md` (the removal commit's parent).

Issue: [#3 SPIKE-01 Validate Qt capture paths across Widgets, Quick and EGLFS](https://github.com/skawu/HyRemote/issues/3)

This document records the evidence produced by the throwaway harness that used to live in
`research/capture/` (retired; see the notice above). It answers the architecture questions of
issue #3 and recommends the v0.1 capture path for each target family. It does **not**
freeze any `RemoteFrame` ABI and does not define a public HyRemote API.

> **Round-2 correction notice.** Two evidence defects found in the Round-1 review are
> corrected here:
>
> 1. the earlier `QImage::cacheKey()`-based "producer reallocation rate" is **withdrawn**;
>    section 7 now uses a real backing-store identity probe with control runs, and states
>    the copy-on-write semantic separately from any allocation-rate claim;
> 2. damage regions are now mapped into a bound shared target coordinate system and
>    clipped before they are unioned (section 6), with deterministic mapping controls;
>    the window-level damage ratios and the damage-stream claim were recomputed and the
>    claim was weakened accordingly.

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
- damage information obtainable from public Qt APIs, expressed in the shared target's
  coordinate system;
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

The `widgets` scene runs with two deterministic constraints so that its damage controls
are repeatable: the `QLineEdit` has `Qt::NoFocus` (a focused line edit blinks its cursor
asynchronously) and the `QProgressBar` is determinate (an indeterminate bar animates
forever). Two `DamageControlResult` steps then check the mapping rules exactly; see
section 6.4.

### 2.4 Measurement rules

- 15-20 warm-up captures are executed and discarded, except for the reported `first`
  (cold) cost of each path.
- Each measured iteration: advance the scene, run the application event loop for the
  requested interval (16 ms unless stated otherwise), then capture.
- `capture` times are wall-clock times of the capture call itself (average, p50, p95,
  max over the measured frames). For the "reused target" and "fresh target" paths the
  measured region covers the **complete** operation - target buffer allocation or clear
  plus the render - so the two are directly comparable and a detach-induced
  reallocation is included rather than hidden before the timer starts.
- Process CPU is the process-wide CPU time delta divided by wall time, as a percentage
  of one core. It includes the harness worker thread and any Qt render thread.
- GUI latency is the delay of a 16 ms precise timer owned by the harness relative to
  its expected firing time; a proxy for "does the local UI visibly degrade".
- `iteration` splits one iteration into application event processing, the capture calls,
  and the remainder, which is what makes the Direct3D 11 result in section 8.2
  visible.
- Damage is measured by an application-level event filter bound to one shared target
  widget; child regions are translated into the target's coordinate system and clipped
  before they are unioned (section 6).
- Frames are handed to a worker thread that re-verifies the pixel checksum 40 ms later
  (section 7.1), and producer pixel storage is tracked by an address probe (section 7.2).

### 2.5 Repeatability and measurement variance

The same configuration was run repeatedly where the number drives a recommendation.
For `widgets`, `quickwidget` and `openglwidget` the harness was run with `--runs 3`,
which also re-creates and tears down the target three times and therefore covers the
"repeated start/stop works" requirement of the spike.

| Observation | Result |
|---|---|
| Repeated start/stop | 3 constructions each of the Widgets, QQuickWidget and QOpenGLWidget targets; every run re-created the target and captured every requested frame |
| `widgets` totals over 3 x 60 frames | 33.9 / 33.3 / 32.7 fps; 12.54 / 13.05 / 13.47 ms of capture work per iteration |
| `widgets` per-path average over the 3 runs | `grab()` 3.29 / 3.33 / 3.64 ms; reused target 2.92 / 2.73 / 3.02 ms; fresh target 3.10 / 3.48 / 3.28 ms |
| `widgets` damage over the 3 runs | identical ratios in every run (app 83.51 %, capture-induced 100.00 % of the 576000 px^2 target) because the scene's repaint geometry is deterministic |
| `openglwidget` totals over 3 x 60 frames | 27.0 / 20.3 / 32.5 fps; 18.28 / 21.39 / 13.67 ms of capture work per iteration |
| `openglwidget` per-path average over the 3 runs | `grabFramebuffer()` 4.91 / 4.69 / 3.80 ms; parent `grab()` 7.30 / 9.94 / 5.23 ms |
| `quickwidget` per-path average over the 3 runs | `grabFramebuffer()` 12.72 / 12.28 / 11.71 ms; parent `grab()` 4.86 / 5.84 / 5.83 ms |
| Cross-session instability of `QQuickWidget::grabFramebuffer()` | 4.11 ms in the 150-frame session versus 11.71-12.72 ms in the repeatability session, and 13.30 / 3.05 / 7.78 / 3.21 ms in earlier sessions: the path has no stable cost across sessions, while the parent `grab()` stayed between 4.3 and 8.2 ms |

Consequences for reading this document:

- Numbers are **comparative evidence with roughly 20-40 % run-to-run spread**, not
  production benchmark claims.
- The widget-target numbers come from one 150-frame run plus three 60-frame runs; the
  Quick numbers come from single 150-frame runs, because they are far more expensive.
- `QQuickWidget::grabFramebuffer()` cannot be recommended for performance reasons: its
  cost moves by a factor of three between sessions. The recommendation for that target
  rests on the whole-window contract and on the damage mapping instead (section 11.2).
- One 150-frame `openglwidget` run was 1.7x slower than the three repeatability runs of
  the same configuration (17.9 fps with 25.90 ms of capture work per iteration versus
  27.0-32.5 fps with 13.67-21.39 ms). It is kept in the evidence set and marked as an
  outlier; desktop and GPU load is not controlled.

## 3. Results: QWidget / raster (case A)

References: `host-windows-rhi-default-widgets-and-gl-widgets.json` (150 frames),
`host-windows-rhi-default-repeatability.json` (3 runs x 60 frames) and
`host-windows-rhi-default-widgets-storage-control-no-consumer.json`
(150 frames, no consumer). Target 960x600.

| Capture path | first | avg (150-frame run) | avg range over 3 runs | p95 (150-frame run) | returned frame |
|---|---|---|---|---|---|
| `QWidget::grab()` (primary) | 2.51 ms | 3.74 ms | 3.29-3.64 ms | 6.37 ms | 960x600 `RGB32` |
| `QWidget::render()` into a reused `QImage` | 2.31 ms | 3.14 ms | 2.73-3.02 ms | 5.29 ms | 960x600 `ARGB32_Premultiplied` |
| `QWidget::render()` into a fresh `QImage` | 4.19 ms | 3.41 ms | 3.10-3.48 ms | 6.06 ms | 960x600 `ARGB32_Premultiplied` |
| `QWidget::render()` into a borrowed raw pixel buffer | 2.39 ms | 2.77 ms | 2.70-3.00 ms | 5.01 ms | 960x600 `ARGB32_Premultiplied` |

Run-level figures for the 150-frame run: 150/150 captures succeeded, 32.6 fps at 16 ms
pacing, 13.58 ms of capture work per iteration, process CPU 59.7 % of one core,
RSS 63.8 -> 64.4 MiB (slope -591 KiB / 100 frames).

A 500-frame run at `--interval-ms 0`
(`host-windows-rhi-default-widgets-memory-run.json`) reached 53.3 fps with 14.80 ms of
capture work per iteration, 89.4 % of one core, and RSS 63.8 -> 61.8 MiB
(slope -970 KiB / 100 frames). No memory growth was observed over 500 captures.

Additional findings:

- `grab()` returns `RGB32` while `render()` into an explicitly created image returns
  `ARGB32_Premultiplied`. A caller that renders into its own image also chooses the
  format; downstream code must not assume one format.
- Rendering into a caller-owned image is the cheapest path, and the difference between
  reusing the target buffer and allocating a new one each capture is small
  (3.14 ms vs 3.41 ms) once the allocation is included in both measurements. Section 7
  explains why: the reused buffer only avoids a reallocation on the captures where no
  consumer held the previous frame.
- The popup `QMenu` and the non-modal `QDialog` are separate top-level windows; the
  captured frame contains neither. The harness observed 3 visible top-level widgets
  while only one was shared. A target adapter needs an explicit policy for extra
  top-level windows.
- Cold cost is not negligible: the first `grab()` in the 150-frame run took 2.51 ms and
  the first fresh-target capture 4.19 ms against a 3.4 ms steady state.

## 4. Results: OpenGL and mixed cases (cases C and D)

### 4.1 QOpenGLWidget inside a QWidget hierarchy (case C)

References: `host-windows-rhi-default-openglwidget-repeatability.json` (3 runs x 60
frames, authoritative for the steady state) and the 150-frame run in
`host-windows-rhi-default-widgets-and-gl-widgets.json` (marked outlier).
Window 1000x640, GL widget 729x568.

| Capture path | first | avg over 3 runs | p95 over 3 runs | returned frame |
|---|---|---|---|---|
| `QOpenGLWidget::grabFramebuffer()` (primary) | 69.9-141.1 ms | 3.80 / 4.69 / 4.91 ms | 5.22-7.06 ms | 729x568 `ARGB32_Premultiplied` |
| `QWidget::grab()` on the GL widget | 3.99-7.06 ms | 4.61 / 6.04 / 6.72 ms | 6.63-11.24 ms | 729x568 `ARGB32_Premultiplied` |
| `QWidget::grab()` on the parent widget | 4.46-8.08 ms | 5.23 / 7.30 / 9.94 ms | 7.37-14.69 ms | 1000x640 `RGB32` |

Run-level figures: 20.3-32.5 fps, 13.67-21.39 ms of capture work per iteration, CPU
47.7-78.8 % of one core, RSS flat or decreasing in the repeatability runs (the
150-frame run grows while the GL context and FBO are initialized).

Findings:

- The first `grabFramebuffer()` costs 70-141 ms in every run; steady state is 3.8-4.9 ms.
  A capture path must be warmed before its first frame is published, or the first
  remote frame pays the initialization.
- `QWidget::grab()` on the **parent** composes the GL child and the raster siblings into
  one image, so a mixed raster+GL tree can be captured with a single raster call without
  manual composition. In these runs it cost 5.2-9.9 ms against 3.8-4.9 ms for
  `grabFramebuffer()` alone, which returns only the GL child and leaves the rest to the
  caller - a fair comparison has to include the composition the caller would have to do.
- The parent capture also yields a damage region expressed in the target's coordinate
  system, whereas `grabFramebuffer()` returns a sub-region at the child's size.

### 4.2 QQuickWidget inside a QWidget hierarchy (case D)

References: `host-windows-rhi-default-repeatability.json` (3 runs x 60 frames) and the
150-frame run in `host-windows-rhi-default-widgets-and-gl-widgets.json`.
Window 1000x640, Quick target 729x568.

| Capture path | first | avg per run | p95 per run | returned frame |
|---|---|---|---|---|
| `QQuickWidget::grabFramebuffer()` (primary) | 2.33 ms | 12.72 / 12.28 / 11.71 ms (150-frame session: 4.11 ms) | 18.75 / 19.46 / 19.36 ms | 729x568 `RGBA8888_Premultiplied` |
| `QWidget::grab()` on the parent widget | 10.30 ms | 4.86 / 5.84 / 5.83 ms (150-frame session: 8.22 ms) | 6.99 / 7.40 / 7.13 ms | 1000x640 `RGB32` |

Run-level figures: 26.8-27.7 fps, 17.57-18.15 ms of capture work per iteration, CPU
41.5-45.5 % of one core, RSS stable.

Findings:

- `grabFramebuffer()` for this target is **not stable across sessions**: 11.7-12.7 ms in
  the repeatability session, 4.11 ms in the 150-frame session and 3.05-13.30 ms in
  earlier sessions. It also returns a sub-region at a different size and pixel format.
- The parent capture is stable (4.86-8.22 ms across sessions, p95 below 16 ms) and
  already contains the Quick content, so a whole-window sharing target can use one
  raster call for a `QQuickWidget` application. The recommendation is based on the
  whole-window contract and on the damage region being expressed in the same coordinate
  system, **not** on the parent path being cheaper in every session.
- Input coordinates differ between the two paths: a viewer coordinate must be mapped
  from the parent geometry into the `QQuickWidget` coordinate space before delivery.

## 5. Results: Qt Quick (case B) and Quick3D / custom FBO (case E)

Quick runs were executed with the OpenGL RHI, because the default Direct3D 11 backend
shows the behaviour described in section 8.2.

`host-windows-rhi-opengl-quick-quick3d.json` (150 frames, 16 ms pacing):

| Case | Capture path | first | avg | p95 | max | returned frame |
|---|---|---|---|---|---|---|
| `quick` | `QQuickWindow::grabWindow()` (primary) | 17.55 ms | 17.05 ms | 20.44 ms | 21.29 ms | 960x600 `RGBA8888_Premultiplied` |
| `quick3d` | `QQuickWindow::grabWindow()` (primary) | 16.63 ms | 16.40 ms | 22.13 ms | 51.86 ms | 960x600 `RGBA8888_Premultiplied` |

| Case | fps | capture work / iteration | process CPU | RSS slope | update requests |
|---|---|---|---|---|---|
| `quick` | 28.7 | 17.06 ms | 23.6 % of one core | +467 KiB / 100 frames (123.0 -> 128.0 MiB) | 314 |
| `quick3d` | 24.4 | 16.42 ms | 28.2 % of one core | -1339 KiB / 100 frames (166.9 -> 164.9 MiB) | 366 |

Quick3D is therefore not measurably more expensive than the 2D scene at this size: the
cost is dominated by a full scene render plus a GPU->CPU readback, not by the content.
The scene graph rendered 591-606 times while 150 captures were taken in the Round-1
runs, on a thread other than the GUI thread (section 8.1).

The asynchronous `QQuickItem::grabToImage()` path, measured separately
(`host-windows-rhi-default-quick-grabtoimage-only.json`,
`host-windows-rhi-opengl-quick-grabtoimage-only.json`):

| Backend | first | avg | p95 | max | loop fps |
|---|---|---|---|---|---|
| Direct3D 11 | 35.19 ms | 35.48 ms | 38.67 ms | 39.82 ms | 17.9 |
| OpenGL | 37.29 ms | 24.94 ms | 33.98 ms | 34.70 ms | 22.9 |

`grabToImage()` is asynchronous in the sense that the call itself returns immediately,
but the reported number is the end-to-end latency until the image becomes valid, which
is bounded by the event-loop cadence. It does not block the GUI thread, yet it cannot be
driven at a fixed cadence without extra buffering.

`host-windows-rhi-opengl-custom-fbo.json` (150 frames):

| Capture path | first | avg | p95 | max |
|---|---|---|---|---|
| `QQuickWindow::grabWindow()` (primary) | 17.00 ms | 16.58 ms | 20.24 ms | 20.77 ms |
| `QQuickItem::grabToImage()` | 29.52 ms | 17.76 ms | 19.00 ms | 30.45 ms |

19.2 fps, 34.37 ms of capture work per iteration, CPU 27.6 % of one core, RSS
+66 KiB / 100 frames.

Content fidelity check: the pixel inside the custom FBO region decoded an encoded value
of 24 at the start and 723 at the end of the run, while the renderer produced 723
frames. The baseline capture therefore contains freshly composed custom OpenGL content,
not a stale or blank buffer.

The same case on Direct3D 11 (`host-windows-rhi-d3d11-custom-fbo-unsupported.json`)
reports `graphicsApi = Direct3D11` and `rendererFrameCount = 0`: the
`QQuickFramebufferObject` renderer never ran, so that target is `unsupported` on a
non-OpenGL RHI backend and any pixel decoded from the region is meaningless.

## 6. Damage information from public APIs

### 6.1 Mapping rules

An application-level event filter receives the events Qt delivers to every object in the
application, which is enough to read `QPaintEvent::region()` (public) without touching
`QBackingStore` internals. A paint region is expressed in the coordinate system of the
widget that painted it, so the tracker:

1. binds to one shared target widget (the root of the shared surface);
2. accepts paint events only from that target and from widgets whose parent chain reaches
   the target **without crossing a window boundary** - a widget inside a popup, menu,
   dialog or tooltip is rejected even though the dialog's parent widget is the target;
3. translates each accepted region into the target's coordinate system with
   `QWidget::mapTo()` and clips it to the visible intersection of the widget's ancestor
   chain with the target rect;
4. only then unions it into the pending damage region.

`QEvent::UpdateRequest` is counted separately and application-wide, because Qt Quick
posts it without any geometry.

### 6.2 Deterministic mapping controls

A damage ratio alone cannot distinguish a correct mapping from a tracker that unions
child-local rectangles as if they already were target coordinates. The `widgets` sample
therefore runs two controls after the measurement loop, with captures stopped:

| Control | Expectation | Result |
|---|---|---|
| Repaint only the pulsing child widget | the damage region is exactly that child's rectangle in target coordinates: `[716,38 233x501]`, area 116733 px^2 | observed `[716,38 233x501]`, area 116733 - **passed** in every run |
| Repaint a widget inside the separate top-level dialog | no damage in the shared target, and the paint events are counted as excluded | observed empty region, area 0, 2 excluded paint events - **passed** in every run |

The first control is the decisive one: an unmapped tracker would have reported the
pulsing rectangle at its own widget-local origin (roughly `[0,0 233x501]`) instead of
`[716,38 233x501]`. The second control exercises the window-boundary rule, which the
measured loops do not hit because the popup and the dialog do not repaint while the
measured scene animates.

### 6.3 Measured damage

| Target family | shared target area | region-carrying paint events (mapped) | excluded paint events | update requests | app damage per frame | capture-induced damage per frame |
|---|---|---|---|---|---|---|
| `widgets` (150 frames) | 576000 px^2 | 6600 | 0 | 150 | 83.51 % | 100.00 % |
| `openglwidget` (3 x 60 frames) | 640000 px^2 | 480 per run | 0 | 60 per run | 86.27 % | 100.00 % |
| `quickwidget` (3 x 60 frames) | 640000 px^2 | 509-514 per run | 0 | 130-134 per run | 86.27 % | 100.00 % |
| `quick` (150 frames) | no QWidget root | 0 | 0 | 314 | not measurable | not measurable |
| `quick3d` (150 frames) | no QWidget root | 0 | 0 | 366 | not measurable | not measurable |
| `customfbo` (150 frames) | no QWidget root | 0 | 0 | 319 | not measurable | not measurable |

The ratios are stable across runs (the same 83.51 % / 86.27 % in every run of a scene)
because the animated children's repaint geometry is deterministic. They are also
explained by the scene rather than by over-counting: in `widgets` the two large animated
children are the custom `QPainter` widget (about 63 % of the 960x600 window) and the
pulsing region (20.3 %, exactly the 233x501 rectangle the control verifies), plus a
small counter label. The scenes used for this spike are deliberately damage-heavy, so
these ratios characterise "a scene whose animated children cover most of the window",
not a typical application.

### 6.4 Conclusions

- Every Widgets-family target (raster, `QOpenGLWidget`, `QQuickWidget`) exposes
  **observable child paint regions that can be mapped into the shared target**, and the
  mapping rules are verified by deterministic controls. This is still a raw
  child-region stream: it is not yet a transport-ready damage protocol. In particular
  it has no region coalescing policy, it counts capture-induced repaints unless the
  snapshot is taken before capturing, and it does not cover popups or separate windows.
- No Qt Quick target exposes a damage region: a native `QQuickWindow` has no QWidget root
  at all, and Qt Quick posts `QEvent::UpdateRequest` without geometry. A shared damage
  abstraction can therefore only be **optional**: `RemoteFrame` must accept "damage
  unknown" and fall back to a full frame.
- Capturing a widget also causes damage: the capture-induced area is a full-window
  repaint (100.00 % of the target in every widget case) because `grab()` and `render()`
  repaint the whole widget. The design must take the damage snapshot before capturing
  (which is what the harness does) or explicitly tag capture-induced repaints.
- For `QQuickWidget` and `QOpenGLWidget` the mapped region is expressed in the parent
  window's coordinate system, which is the coordinate system of the parent `grab()` path
  and not of `grabFramebuffer()`. A viewer that uses the GL/Quick-specific sub-region
  capture would have to translate or crop that damage region.

## 7. Frame hand-off, buffer ownership and lifetime (S5)

### 7.1 Content hand-off probe

The probe hands captured frames to a worker thread that re-reads the pixels after 40 ms
and compares a checksum with the value recorded at hand-off time. Two hand-off forms are
compared: a `QImage` value (implicitly shared) and a borrowed view onto producer-owned
raw memory (a `QImage` constructed over the pointer without copying).

| Hand-off form | delivered | stable | mutated after hand-off |
|---|---|---|---|
| `QImage` value (`grab()`, `render()` paths) | 45 per 150-frame run | 100 % | 0 |
| borrowed raw pixel buffer (pointer-only view) | 15 per 150-frame run, 25 per 500-frame run | 0 % | **100 %** |

Result: a borrowed pointer into producer-owned memory is rewritten before a delayed
consumer reads it, in 15 of 15 and 25 of 25 probes, while no `QImage` hand-off ever
changed. **A borrowed pointer is not a frame**: any zero-copy path (GBM/DMA-BUF, GL PBO,
external texture) must transfer ownership explicitly or carry a release callback, and the
frame model needs an ownership state rather than an address.

### 7.2 Storage-identity probe (corrected)

**Withdrawn claim.** The Round-1 version of this spike counted producer-side
`QImage::cacheKey()` changes and reported them as "519 reallocations in 520 captures".
That is invalid: `QImage::cacheKey()` is documented and implemented as
`(serial_number << 32) | detach_counter`, so it changes whenever the image is modified
(including detaches that do not relocate anything) and it does not identify the backing
store. **The reallocation count and every conclusion drawn from it are withdrawn.**

**Corrected probe.** Pixel-storage identity is read with `QImage::constBits()`, which
Qt documents as *"does not perform a deep copy of the shared pixel data, because the
returned data is const"* and implements as `return d ? d->data : nullptr;`. The probe
samples the address before the first write of a capture and after the last write of the
same capture, so it brackets every write that could detach the image.

Three independent safeguards make the measurement trustworthy:

- the probes cannot perturb the buffer (they do not detach), which the negative control
  below confirms empirically: an untouched-by-consumers buffer keeps one address for all
  150 captures while being probed on every capture;
- a synthetic **positive control** keeps a shallow copy alive and then writes: the write
  must relocate the storage. It was detected in 170 of 170 and 520 of 520 control runs;
- a synthetic **negative control** writes with no other reference: the address must not
  move. It stayed identical in 170 of 170 and 520 of 520 control runs.

Measured on the reused `QImage` producer target (`widgets` case):

| Configuration | captures with the consumer holding the previous frame | probes | storage replacements |
|---|---|---|---|
| 150 frames, frame handed off every 10th capture | 15 | 150 | **15** |
| 500 frames, frame handed off every 20th capture | 25 | 500 | **25** |
| 3 x 60 frames, frame handed off every 10th capture | 6 per run | 60 per run | **6 per run** |
| 150 frames, hand-off probe disabled (negative control) | 0 | 150 | **0** |
| borrowed raw pixel buffer, 150 frames | (content rewritten) | 150 | **0** (stable by construction) |

The number of storage replacements equals the number of captures that follow a hand-off,
in every run, with zero probe self-check anomalies.

**What this does and does not establish.**

- Established (Qt semantic fact): `QImage` uses implicit data sharing, and
  `QImage::detach()` copies the pixel data when the reference count is not 1. A producer
  that publishes a shared `QImage` therefore pays for a new allocation on exactly those
  captures where a consumer still holds the previous frame. This is a copy-on-write
  consequence of the hand-off contract, and it is also why a consumer that keeps a
  `QImage` copy never observes torn pixels (section 7.1).
- Not established (and not claimed): a fixed per-frame allocation rate. With no consumer
  holding a frame, the same buffer is reused with **zero** replacements - the negative
  control shows the opposite of "every frame reallocates".
- Also not established: the cost of the reallocation in isolation. The reused-target path
  now measures clear + render together (3.14 ms) and the fresh-target path measures
  allocation + clear + render (3.41 ms); the 0.27 ms difference is what a producer
  actually saves by reusing a buffer under this hand-off pattern, but the sample isolates
  the allocation itself no further than that.
- Address stability is **not** safety: the borrowed raw pixel buffer kept one address in
  all 150 captures and its content was still rewritten in 100 % of the hand-off probes.
  An address check cannot replace an ownership contract.

Consequence for `RemoteFrame`: the ownership/lifetime contract is the design problem, not
the pixel format. The v0.1 baseline should publish frames with an explicit ownership
transfer (a pool buffer handed to the consumer and returned after use) instead of a shared
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
`grabWindow()` from the render-thread hooks: Qt documents the call as GUI-thread only and
the experiment carries a deadlock risk. That path is `unverified` and belongs to the
GL/PBO follow-up ([#16](https://github.com/skawu/HyRemote/issues/16)).

What can safely be handed to another thread: a completed, immutable frame. Both a
`QImage` snapshot and an explicitly transferred buffer qualify; a borrowed pointer does
not (section 7.1). The capture calls themselves are not thread-safe and must stay on the
GUI thread for every public API tested here.

### 8.2 Direct3D 11 makes per-frame `grabWindow()` unusable on this host

| Configuration | Evidence file | loop fps | app event time / iteration | capture call |
|---|---|---|---|---|
| Quick, no capture at all (control) | `...-default-quick-no-capture-control.json` | 57.1 | 17.50 ms | - |
| Quick + `grabWindow()`, Direct3D 11 | `...-default-quick-grabwindow-only.json` | **1.0** | **1000.62 ms** | 15.84 ms |
| Quick + `grabWindow()`, OpenGL RHI | `...-opengl-quick-quick3d.json` | 28.7 | 17.50 ms | 17.05 ms |
| Quick + `grabToImage()` only, Direct3D 11 | `...-default-quick-grabtoimage-only.json` | 17.9 | 20.03 ms | 35.48 ms |
| Quick, no capture at all (control), OpenGL | `...-opengl-quick-no-capture-control.json` | 56.3 | 17.85 ms | - |

With the default Direct3D 11 backend, capturing once per iteration drops the
application's own event loop to about one iteration per second, while the measured
capture call still reports only 15.84 ms and the GUI latency probe reaches a p95 of
1003.9 ms. The stall is therefore not inside `grabWindow()`; it delays the application's
event processing. With the same window, the same scene and no capture at all, the loop
runs at its requested cadence, so the throttling is induced by the capture path rather
than by window occlusion.

This is recorded as a host/Windows/Direct3D 11 limitation. Whether an equivalent effect
exists on EGLFS/OpenGL ES is unknown, and it is one of the reasons the embedded half of
issue #3 must still be executed.

### 8.3 The public asynchronous path does not suffer from the Direct3D 11 stall

The asynchronous follow-up
([#16](https://github.com/skawu/HyRemote/issues/16)) measured the public
`QQuickItem::grabToImage()` path in the same session and on the same scenes. The results are
recorded in [`async-capture-spike.md`](async-capture-spike.md); the parts that change this
document's conclusions are:

| Path | Direct3D 11 | OpenGL RHI |
|---|---|---|
| `QQuickWindow::grabWindow()` once per iteration | loop collapses to **1.0-1.1 iterations/s** (GUI latency p95 ~1003 ms) | 27.8 iterations/s (GUI latency p95 18.9 ms) |
| `contentItem()->grabToImage()`, 4 in flight | **119.1 captures/s**, ~32 ms latency, GUI latency p95 17.6 ms | 120.9 captures/s, ~30 ms latency, GUI latency p95 5.3 ms |

So the Direct3D 11 pathology of section 8.2 is specific to the **synchronous** path, and the
asynchronous path is a genuine alternative on that backend. It does not remove the gaps this
document records: `grabToImage()` still needs a visible window, exposes no damage region,
cannot capture a `QOpenGLWidget` at all, and its pipelined requests share one frame's content.

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
5. **Damage is only partially solved even for Widgets.** The mapped child-region stream
   has no coalescing policy, excludes separate windows, and overlaps with
   capture-induced repaints (section 6.4).
6. **Buffer ownership is the central design problem**, not the pixel format
   (section 7).
7. **Extra top-level windows** (popups, menus, dialogs, tooltips, native child windows)
   are excluded from a top-level widget capture; a composition policy is required.
8. **Measurement spread of 20-40 %** on this uncontrolled desktop, and
   `QQuickWidget::grabFramebuffer()` moved by a factor of three between sessions
   (section 2.5).
9. **Multiple native GL windows** are not addressed by any path tested here.
10. **`QQuickWidget`/`QOpenGLWidget` on EGLFS** may behave differently because there is
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
- Any producer allocation rate beyond the storage-identity facts in section 7.2: the
  "519 reallocations in 520 captures" figure from Round 1 is withdrawn and no replacement
  allocation-rate number is claimed.

## 11. Conclusions and recommendations (S7)

### 11.1 Recommended v0.1 baseline for QWidget / Widgets

**`QWidget::render()` into a caller-owned buffer, driven from the GUI thread, with
`QWidget::grab()` as the convenience equivalent.**

Rationale: it is the cheapest and most controllable path measured (2.7-3.4 ms at
960x600 including the buffer clear), it lets the caller choose the pixel format and a pool
buffer, it composes GL children (`QOpenGLWidget`) through the parent capture, and it is
the only family that yields region-level damage that can be mapped into the shared target
(verified by controls).

Required companion decisions:

- publish frames with an explicit ownership transfer instead of a shared `QImage`
  (section 7.2);
- take the damage snapshot before capturing and exclude capture-induced repaints, and
  define a coalescing policy (section 6.4);
- define the policy for popups, menus, dialogs and other top-level windows;
- define the shared surface format explicitly: `grab()` returns `RGB32`, `render()`
  returns the format of the target the caller provides.

### 11.2 Recommended v0.1 baseline for Qt Quick / EGLFS

**`QQuickWindow::grabWindow()` from the GUI thread, full frames only, at a paced
cadence with backpressure, as an explicitly experimental correctness baseline.**

Rationale: it is the only public API that returns the final composed content for a native
Quick window, it covers Qt Quick 2D, Quick3D and custom `QQuickFramebufferObject` content
(the fidelity check in section 5 passed), and it needs no Qt private API. Its limits must
be stated: ~17 ms per 960x600 frame (about 59 fps ceiling), a synchronous GUI-thread
block, no damage regions, and a full-frame update for every viewer refresh.

`QQuickItem::grabToImage()` is not the baseline: its latency is bound to the event loop
and it delivers one frame later, but it is the first thing the asynchronous follow-up
([#16](https://github.com/skawu/HyRemote/issues/16)) must evaluate because it does not
block the GUI thread.

`QQuickWidget` applications should be captured with the parent `QWidget::render()` /
`grab()` path: it returns the whole window in one image, it stayed stable across sessions,
and its damage region is already expressed in the shared target coordinate system, whereas
`QQuickWidget::grabFramebuffer()` moves by a factor of three between sessions and returns
a sub-region at the child's size.

### 11.3 Recommended `RemoteFrame` requirements (no ABI freeze)

- geometry and stride;
- pixel format, or an external-buffer description;
- presentation timestamp;
- **explicit buffer ownership/lifetime state**: producer-owned-and-recycled (only with a
  release callback), ownership-transferred, or immutable snapshot. A borrowed pointer is
  not sufficient and the evidence shows it is corrupted within one frame;
- optional damage region with an explicit `unknown`/`invalid` state and a defined
  full-frame fallback (required for every Qt Quick target);
- capture capability metadata on the backend: needs-a-current-graphics-context,
  blocks-the-GUI-thread, allocates-per-capture, supports-region-damage;
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
- no public API to make `QQuickWindow::grabWindow()` cheap on backends like Direct3D 11.

### 11.5 What a zero-code integration mode can and cannot do

An in-process observer that may run code inside the application process but must not
change the application can, using public APIs only:

- find the application's top-level `QWidget`s and `QWindow`s
  (`QApplication::topLevelWidgets()`, `QGuiApplication::topLevelWindows()`);
- capture them (`QWidget::render()`, `QQuickWindow::grabWindow()`);
- observe and map raster damage application-wide through an application-level event
  filter, exactly as the harness's damage tracker does (it needs a handle to the target
  widget, which the observer has).

It cannot, using public APIs only:

- obtain damage regions for Qt Quick windows;
- capture a Quick window from a non-GUI thread;
- capture without paying a synchronous full render plus readback on the GUI thread;
- compose popups, menus and dialogs, which are separate top-level windows.

A truly out-of-process zero-code proxy (QPA plugin, `LD_PRELOAD`, platform hook) was not
tested and remains an open architectural question for a dedicated issue.

## 12. Follow-up work

1. **Asynchronous capture path** - [#16](https://github.com/skawu/HyRemote/issues/16):
   the public asynchronous path was evaluated first and is recorded in
   [`async-capture-spike.md`](async-capture-spike.md). It resolves whole-window
   composition (grab `QQuickWindow::contentItem()`, not the root item), Quick3D and
   custom-FBO fidelity, and it does not suffer from the Direct3D 11 stall of section 8.2.
   Three gaps remain open rather than solved: no public asynchronous path for
   `QOpenGLWidget`, no asynchronous whole-window composition for `QQuickWidget`, and a
   visible-window precondition. GL/PBO or render-thread work is therefore **not** justified
   by this evidence yet; it would have to show a material gap against a target frame budget.
2. **GBM/DMA-BUF feasibility** from Qt/EGLFS, plus the explicit `RemoteFrame` ownership
   model; the corrected storage-identity evidence in section 7.2 is its input - [#17](https://github.com/skawu/HyRemote/issues/17).
3. **Qt Quick damage**: private renderer integration versus application-declared dirty
   hints, with a full-frame fallback.
4. **RK3588/EGLFS validation** of the whole matrix in this document, which is the actual
   acceptance gate of issue #3. This is the blocker that keeps SPIKE-01 open.
5. **Damage coalescing policy** for the Widgets region stream, including how
   capture-induced repaints are tagged or suppressed (section 6.4).
6. **Buffer pool and frame lifetime API design** for `hyremote-core`, driven by the
   ownership result rather than by a pixel-format discussion.

## 13. Evidence index and reproduction

The machine-readable evidence files (one per invocation) were produced by that retired harness. They are no longer in
the working tree; retrieve any of them from the same historical commit (`3e6e191`), for example
`git show 3e6e191:research/capture/evidence/host-windows-rhi-default-widgets-and-gl-widgets.json`. The index below is
kept so a reader can tell which file backs which table:

| File | Content |
|---|---|
| `host-windows-rhi-default-widgets-and-gl-widgets.json` | `widgets`, `openglwidget`, `quickwidget`, 150 frames each, default (Direct3D 11) backend |
| `host-windows-rhi-default-widgets-storage-control-no-consumer.json` | `widgets`, 150 frames with the hand-off probe disabled: storage-identity negative control and damage mapping controls |
| `host-windows-rhi-default-repeatability.json` | `widgets` and `quickwidget`, 3 x 60 frames, repeated start/stop |
| `host-windows-rhi-default-openglwidget-repeatability.json` | `openglwidget`, 3 x 60 frames, repeated start/stop |
| `host-windows-rhi-default-widgets-memory-run.json` | `widgets`, 500 frames, interval 0, memory trend and 25 storage probes |
| `host-windows-rhi-opengl-quick-quick3d.json` | `quick`, `quick3d`, 150 frames, OpenGL RHI |
| `host-windows-rhi-opengl-custom-fbo.json` | `customfbo`, 150 frames, OpenGL RHI |
| `host-windows-rhi-d3d11-custom-fbo-unsupported.json` | `customfbo` on Direct3D 11; renderer never ran |
| `host-windows-rhi-default-quick-grabwindow-only.json` | the ~1 s per-frame stall of section 8.2 |
| `host-windows-rhi-default-quick-grabtoimage-only.json` | `grabToImage()` on Direct3D 11 |
| `host-windows-rhi-opengl-quick-grabtoimage-only.json` | `grabToImage()` on OpenGL |
| `host-windows-rhi-default-quick-no-capture-control.json` | control run without any capture, Direct3D 11 |
| `host-windows-rhi-opengl-quick-no-capture-control.json` | control run without any capture, OpenGL |

Every report contains the raw probe data behind the tables: `paths[].storageProbes` /
`storageReplacements`, `damage.*` with `targetAreaPx`, `mappedPaintEvents`,
`excludedPaintEvents`, and `damageMappingControls[]` with the expected and observed
rectangles.

Build and run (historical). The harness is retired, so these commands no longer work from a checkout; the harness
documentation - including the full option list, the sample names and the platform notes - and the build entry it
described are retrievable from history:

```bash
git show 3e6e191:research/capture/README.md        # option list, samples, platform notes
git show 3e6e191:research/capture/CMakeLists.txt   # the build entry those commands configured
git log --diff-filter=D --name-only -- research/capture   # every file that was removed
```

## 14. Compatibility matrix

The status column uses the definitions of [`docs/compatibility.md`](../compatibility.md).
The same host rows are recorded there; every embedded row stays `Unverified`.

| Qt | OS / target | QPA / graphics | Application type | Capture backend | Status | Notes |
|---|---|---|---|---|---|---|
| 6.8.3 | Windows 11 / x86_64 | `windows` / Direct3D 11 | QWidget/raster | `QWidget::render()` into a caller buffer (primary), `QWidget::grab()` | Experimental | 2.7-3.7 ms at 960x600 including the buffer clear; region damage observable and mapped (controls pass); popups excluded |
| 6.8.3 | Windows 11 / x86_64 | `windows` / Direct3D 11 | QWidget with custom `QPainter` | same | Experimental | Covered by the `widgets` scene |
| 6.8.3 | Windows 11 / x86_64 | `windows` / Direct3D 11 | QOpenGLWidget in a hierarchy | parent `QWidget::grab()` (primary), `grabFramebuffer()` | Experimental | Parent grab composes GL content at 5.2-9.9 ms; `grabFramebuffer()` returns only the GL widget at 3.8-4.9 ms; first call 70-141 ms |
| 6.8.3 | Windows 11 / x86_64 | `windows` / Direct3D 11 | QQuickWidget in a hierarchy | parent `QWidget::grab()` (primary), `grabFramebuffer()` | Experimental | Parent grab stable at 4.9-8.2 ms; `grabFramebuffer()` 4.1-12.7 ms across sessions |
| 6.8.3 | Windows 11 / x86_64 | `windows` / OpenGL RHI | Qt Quick 2D | `QQuickWindow::grabWindow()` | Experimental | 17.1 ms at 960x600; blocks the GUI thread; no damage region |
| 6.8.3 | Windows 11 / x86_64 | `windows` / OpenGL RHI | Quick3D | `QQuickWindow::grabWindow()` | Experimental | 16.4 ms; content fidelity verified |
| 6.8.3 | Windows 11 / x86_64 | `windows` / OpenGL RHI | custom `QQuickFramebufferObject` | `QQuickWindow::grabWindow()` | Experimental | Freshness verified with an encoded render counter (24 -> 723 over 723 rendered frames) |
| 6.8.3 | Windows 11 / x86_64 | `windows` / Direct3D 11 | custom `QQuickFramebufferObject` | none | Unsupported | Renderer never ran on a non-OpenGL RHI backend |
| 6.8.3 | Windows 11 / x86_64 | `windows` / Direct3D 11 | Qt Quick 2D | `QQuickWindow::grabWindow()` once per frame | Unsupported | Application loop fell to ~1 fps (section 8.2) |
| 6.8.x | Embedded Linux / RK3588 | EGLFS / OpenGL ES | all | recommended by SPIKE-01, unverified on target | Unverified | Acceptance gate of issue #3; not executed |
