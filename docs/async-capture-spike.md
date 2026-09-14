# SPIKE-02 Public Asynchronous Capture Evaluation

Status: **public asynchronous path evaluated; no lower-level path implemented**

Issue: [#16 [ARCH] Asynchronous GL/PBO capture path for Widgets and Quick targets](https://github.com/skawu/HyRemote/issues/16)

This document records the evidence produced by the throwaway harness in
[`spikes/async-capture/`](../spikes/async-capture/). It answers the questions of issue #16
in the order the issue prescribes: the **public asynchronous path is evaluated first**,
requirement by requirement, and lower-level mechanisms (GL/PBO, render-thread, RHI,
version-specific adapters) are **not implemented** here. Section 6 states exactly which
requirement the public path fails to satisfy and what a follow-up would have to prove.

It does not freeze any `RemoteFrame` ABI and does not define a public HyRemote API.

> **Governance note.** Scope discipline applied: no DMA-BUF/GBM (#17), no VNC/RFB, no
> RKMPP/H.264, no QPA proxy, no Core public API, and no private Qt API. Every probe in this
> spike uses public Qt APIs only, and only public APIs are recommended.

> **Embedded validation is still missing.** All numbers below come from the same Windows 11
> host used for SPIKE-01. RK3588/EGLFS/OpenGL ES remains `unverified`, and desktop results
> are not presented as a substitute.

## 1. Scope

In scope:

- what `QQuickItem::grabToImage()` is and is not, from the Qt 6.8.3 implementation and from
  measurement;
- whether it captures the **complete intended shared content** of a `QQuickWindow`,
  including Quick3D, custom `QQuickFramebufferObject` content and overlay/sibling items;
- its latency, sustainable cadence, outstanding-request behaviour, memory behaviour,
  completion ordering, duplication, drop semantics and backpressure interaction;
- whether an equivalent public asynchronous strategy exists for `QOpenGLWidget` and
  `QQuickWidget`;
- an explicit pass/fail list against HyRemote requirements.

Out of scope (and deliberately not implemented): GL/PBO readback, render-thread capture,
RHI-level readback, `QQuickWindow::setGraphicsDevice`, any Qt private API, DMA-BUF/GBM,
hardware encoding, transports, and any public API freeze.

## 2. Environment

| Item | Value |
|---|---|
| OS | Windows 11, kernel 10.0.26200 |
| GPU / driver | Intel(R) UHD Graphics 770, driver 32.0.101.7088 (OpenGL 4.6) |
| Qt | 6.8.3 Open Source (runtime and compile version) |
| Compiler / build | MSVC 19.44.35227.0, CMake 3.30.5 + Ninja, Release, C++17 |
| QPA platform | `windows` |
| Qt Quick RHI backend | `<default>` (Direct3D 11 on this host) and explicit `opengl` where recorded |
| Screen | 1920x1080, devicePixelRatio 1 |
| Embedded | not available (no RK3588, no EGLFS) |

Scenes are 960x600 unless noted, and the widget-based scenes use a 1000x640 window.

## 3. What `grabToImage()` actually is

Read from the Qt 6.8.3 sources (`qtdeclarative/src/quick/items/qquickitemgrabresult.cpp`,
`qquickitem.cpp`, `qquickview.cpp`); every statement below is also consistent with the
measurements in section 5.

1. `QQuickItem::grabToImage()` creates a `QQuickItemGrabResult`, connects it to
   `QQuickWindow::beforeSynchronizing` and `QQuickWindow::afterRendering` with
   **`Qt::DirectConnection`**, and returns immediately.
2. `setup()` (render thread, `beforeSynchronizing`) creates a `QSGLayer`, attaches the
   **item's scene graph node** to it and calls `refFromEffectItem()` on the item.
3. `render()` (render thread, `afterRendering`) renders that layer and reads it back with
   `QSGLayer::toImage()`, then posts an event so that `ready()` is emitted **on the GUI
   thread**.
4. Both connections run on the render thread, which is why the call does not block the GUI
   thread, but it does add a full extra render of the item subtree plus a GPU->CPU readback
   to that frame.
5. `create()` calls `item->window()->update()`, so a request forces a frame even for an
   otherwise idle scene.
6. `create()` rejects the request and returns a **null** `QSharedPointer` (with a
   `qmlWarning`) when the item has invalid dimensions, is not attached to a window, or its
   (render-control aware) window is not visible.
7. `QQuickViewPrivate::setRootObject()` parents the QML root object to
   `QQuickWindow::contentItem()`. Overlays, popups and tooltips are children of that same
   `contentItem()`, so they are **siblings** of the application's root item.

Point 7 is the composition trap this spike was built around: the QML root item is *not* the
whole window.

## 4. Method

### 4.1 Scenes

| Case | Scene | What it exists for |
|---|---|---|
| `quick2d` | `AsyncScene.qml`: 960x600 animated item tree | 2D composition, fidelity, pipeline |
| `quick3d` | `Quick3DScene.qml`: `View3D` + MSAA + 2D QML content in the same root item | Quick3D composition/fidelity |
| `customfbo` | `CustomFboScene.qml`: a `QQuickFramebufferObject` whose renderer encodes its own render counter in its clear colour | custom FBO composition, freshness, fidelity |
| `quickwidget` | `QQuickWidget` (same 2D scene) inside a 1000x640 QWidget hierarchy | QOpenGLWidget/QQuickWidget equivalent async strategy |
| `openglwidget` | `QOpenGLWidget` inside a QWidget hierarchy | control: no QML item tree at all |

Two deterministic landmarks are built into every QML scene:

- a 48x48 **tick patch** whose colour encodes `tick` in an 8-bit exact way, where `tick` is
  written by the harness before each request. A captured image can therefore be attributed
  to a specific harness-visible scene state, which is what makes ordering/duplication/
  freshness measurable rather than inferred.
- an **overlay sibling**: a QML rectangle parented to `QQuickWindow::contentItem()`, i.e.
  the same parent chain QML overlays, popups and tooltips use. It is deliberately not a
  descendant of the application's root item.

### 4.2 Probes

| Probe | What it measures |
|---|---|
| composition | where the overlay lands in a root-item grab, a `contentItem()` grab and a synchronous `grabWindow()` grab |
| fidelity | pixel comparison of the frozen scene: async `contentItem()` vs async root item vs synchronous whole-window capture, including a repeat pair |
| pipeline | the asynchronous loop: latency, cadence, in-flight behaviour, completion order, tick lag, duplication, memory, GUI-thread impact |
| sync baseline | the synchronous public capture for the same scene in the same session, for comparison |
| failure modes | the documented rejections (hidden window, detached item) and recovery afterwards |

### 4.3 Controls

- **frozen scene**: for fidelity only; the scene stops animating and `tick` is pinned to
  4242, so the only legitimate difference between two captures is composition, not time.
- **paced requests** (`--paced`): pump the event loop between individual requests, so each
  request lands in its own rendered frame. This is the control that separates "pipelining
  collapses content" from "the harness issued requests faster than frames".
- **dry run** (`--dry-run --interval-ms 32`): the same loop, scene updates and bookkeeping
  with **no capture at all**, to separate the capture path's memory cost from the scene's.
- **consumer window** (`--consumer-window N`): stop issuing while N completed frames are
  still held by a slow consumer, i.e. the producer-side half of backpressure.
- **child-process hidden-window probe**: the synchronous `grabWindow()` on a hidden window
  is executed in a separate process, because it can stall and must not take the evidence
  run with it.

## 5. Measurements

### 5.1 Composition: which item must be grabbed

`quick2d`, `quick3d`, `quickwidget`; identical outcome on both RHI backends.

| Grab | Overlay present | Size (QQuickView scenes) | Size (QQuickWidget) |
|---|---|---|---|
| `rootItem()->grabToImage()` | **no** (background `#101418` / `#0b151e` sampled at the overlay centre) | 960x600 | 699x557 |
| `contentItem()->grabToImage()` | **yes** (`#ff00ffff`) | 960x600 | 699x557 |
| `QQuickWindow::grabWindow()` (sync) | **yes** (`#ff00ffff`) | 960x600 | null for a QQuickWidget |

The quantitative version from the fidelity probe (frozen scene, `quick2d`):

| Comparison | Sizes match | Max channel diff | Differing pixels | Differing pixels outside the overlay rect |
|---|---|---|---|---|
| `contentItem` async vs sync whole-window | yes | **0** | **0.00 %** | 0.00 % |
| root item async vs sync whole-window | yes | 239 | 3.31 % | **0.00 %** |
| root item async vs `contentItem` async | yes | 239 | 3.31 % | **0.00 %** |
| `contentItem` async repeat vs `contentItem` async | yes | **0** | **0.00 %** | 0.00 % |

The overlay rectangle is 160x120, i.e. 3.33 % of the 960x600 window, and it is the *only*
place where a root-item grab differs from the true window content. `qquickview.cpp` explains
why: the root item is a child of `contentItem()`.

**Conclusion:** the correct asynchronous grab target is **`QQuickWindow::contentItem()`**,
not the QML root item. This is a public API and covers sibling/overlay composition.

### 5.2 Content fidelity

Frozen scene, `tick` pinned to 4242.

| Scene | Comparison | Max channel diff | Differing pixels (> 2) | Notes |
|---|---|---|---|---|
| `quick2d` (D3D11 and OpenGL) | `contentItem` async vs sync | **0** | **0.00 %** | pixel-identical |
| `quick3d` (D3D11) | `contentItem` async vs sync | **0** | **0.00 %** | Quick3D + MSAA reproduced exactly |
| `customfbo` (OpenGL) | `contentItem` async vs sync | 2 | **0.00 %** | the encoded FBO counter advanced between the two calls (74 vs 76) |
| `customfbo` (OpenGL) | `contentItem` async repeat vs async | 4 | 13.66 %, all inside the FBO rect | the FBO rectangle is 280x280 = 13.6 % of the window; only its own render counter differs |

The asynchronous path therefore reproduces the composed result, including Quick3D and custom
`QQuickFramebufferObject` content. For a scene that renders continuously on its own (the
custom FBO), two captures legitimately contain different FBO counters; the difference is
confined to that rectangle.

### 5.3 Freshness of custom FBO content over the async path

`customfbo` pipeline run: 60 requests produced `distinctFboCounters = 26` and
`distinctTicks = 26` over 26 rendered frames, with the encoded counter advancing 80 -> 105.
Each rendered frame carried a *fresh* custom-FBO render, so the asynchronous layer path does
not return a stale or blank FBO texture.

### 5.4 Synchronous baseline in the same session

| Scene | Backend | Synchronous API | avg | p95 | Rate | GUI latency avg / p95 |
|---|---|---|---|---|---|---|
| `quick2d` | D3D11 | `grabWindow()` | 17.70 ms | 31.44 ms | **1.1 /s** | 583.64 / 1002.76 ms |
| `quick2d` | OpenGL | `grabWindow()` | 17.35 ms | 20.93 ms | 27.8 /s | 4.52 / 18.94 ms |
| `quick3d` | D3D11 | `grabWindow()` | 13.61 ms | 20.68 ms | **1.0 /s** | 730.39 / 1014.95 ms |
| `customfbo` | OpenGL | `grabWindow()` | 15.86 ms | 18.42 ms | 30.1 /s | 8.12 / 18.17 ms |
| `quickwidget` | D3D11 | parent `QWidget::grab()` | 15.06 ms | 18.68 ms | 30.8 /s | 3.88 / 15.16 ms |
| `openglwidget` | D3D11 | `QOpenGLWidget::grabFramebuffer()` | 4.43 ms | 4.44 ms | 46.9 /s | 1.26 / 2.27 ms |

This reproduces the SPIKE-01 Direct3D 11 result independently: a synchronous
`grabWindow()` per iteration collapses the application loop to about one iteration per
second while the call itself reports ~14-18 ms, and the GUI latency probe reaches 1003 ms.
The `QQuickWidget` parent `grab()` and the `QOpenGLWidget` `grabFramebuffer()` are not
affected by that stall.

### 5.5 Asynchronous pipeline: latency, cadence, in-flight behaviour

`quick2d`, Direct3D 11, `contentItem()`, 240 requests per configuration, no consumer delay.
The `tick` is written before each request, so `tick lag = tick in image - tick at request`.

| In flight (K) | Completed/s | Latency avg | Latency p95 | Distinct ticks | Duplicate completions | Tick lag avg / max | Frames rendered | GUI latency avg / p95 | RSS growth |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 26.7 | 31.96 ms | 39.18 ms | **240** | 0 | 0.00 / 0.00 | 539 | 1.80 / 12.23 ms | +27.2 MiB |
| 2 | 56.9 | 31.98 ms | 41.85 ms | 120 | 120 | 0.50 / 1.00 | 253 | 3.97 / 15.98 ms | +31.9 MiB |
| 4 | 115.5 | 33.22 ms | 43.93 ms | 60 | 180 | 1.50 / 3.00 | 120 | 7.75 / 19.04 ms | +43.9 MiB |
| 8 | 134.8 | 57.69 ms | 74.81 ms | 30 | 210 | 3.50 / 7.00 | 60 | 20.10 / 45.45 ms | +40.1 MiB |
| 16 | 152.4 | 101.71 ms | 131.73 ms | 15 | 225 | 7.50 / 15.00 | 32 | 36.91 / 92.86 ms | +32.5 MiB |

Readings:

- **Throughput scales with K** (26.7 -> 152.4 captures/s) because one rendered frame serves K
  grabs. The scene's own frame rate *falls* (539 -> 32 frames) because each frame now carries
  K extra item renders and readbacks.
- **Latency grows with K** (32 -> 102 ms) and every request pays at least one frame.
- **Pipelining does not produce more distinct frames.** Distinct ticks are exactly
  `requests / K`, duplicate completions are `requests - requests/K`, and the average tick lag
  is exactly `(K-1)/2`: the pending requests are all rendered from the same frame, so the
  content is shared and lags the request by up to K-1 ticks. Content is **never newer than
  the request** (no negative lag was observed).
- **Completion order is FIFO**: 0 out-of-order completions in every configuration, on both
  backends. There is **no drop or dedup mechanism**; every request completes with a full
  image, duplicates included. Deciding what to do with duplicated/stale frames is the
  caller's responsibility.
- **GUI-thread cost grows with K** (1.80 -> 36.91 ms average, 12.23 -> 92.86 ms p95). At
  K >= 8 the local UI jitter is comparable to the SPIKE-01 capture-induced damage figures,
  i.e. it is no longer negligible.
- **Outstanding requests have no built-in limit.** Each in-flight request owns a
  `QSGLayer` render target plus a readback image, so K is bounded only by memory and by the
  render thread's ability to serve K grabs per frame.

The `paced` control (one request per rendered frame, K=4, 60 requests) gives
**60 distinct ticks, 0 duplicates, tick lag 0.00**, 33.28 ms latency, 61 frames and GUI
latency 1.29 / 3.01 ms. That isolates the effect: the collapse in the table above is caused by
issuing several requests before a frame is rendered, not by the API itself.

### 5.6 Memory behaviour

| Configuration | Requests | Frames rendered | RSS growth | Growth per request |
|---|---|---|---|---|
| K=4, burst | 240 | 120 | +43.9 MiB | 187 KiB |
| K=4, burst | 1000 | 566 | **+45.5 MiB** | **47 KiB** |
| dry run (no capture), 32 ms pacing | 240 | 614 | **+0.9 MiB** | 4 KiB |

Growth does not scale with the number of captures: quadrupling the request count from 240 to
1000 leaves the total growth at ~+45 MiB, and the no-capture control in the same loop grows
0.9 MiB. The residual is a bounded warm-up/caching cost of the layer path, **not a
per-request leak**. (The harness itself was fixed during this spike: an earlier probe
retained a result/connection reference cycle and measured its own retention. The numbers
above are from the corrected probe, which releases the `QQuickItemGrabResult` and its pixels
as soon as it has decoded them.)

### 5.7 Backpressure with a slow consumer

| Producer configuration | Consumer | Completed/s | Max frames held | Peak retained | Frames that must be dropped for a 10 /s consumer |
|---|---|---|---|---|---|
| K=4, unbounded | 100 ms per frame | 82.4 | 16 | 36.0 MiB | ~72 of every 82 |
| K=4, consumer window 2 | 100 ms per frame | 26.1 | 4 | 9.0 MiB | ~16 of every 26 |

Without a consumer window the producer runs at full speed and the completed frames pile up
(16 frames, 36 MiB) even though the consumer only takes 10/s. With a consumer window the
producer is throttled (340 throttled loop slices), retention stays bounded (9 MiB) and the
rate settles at the consumer's rate. **A bounded in-flight-to-consumer window is required;
it is not provided by the API.**

### 5.8 Failure modes and recovery

| Probe | Result | Qt warning |
|---|---|---|
| async grab of a hidden window | **null result**, no completion at all | `grabToImage: item's window is not visible` |
| async grab of an item not attached to a window | **null result** | `grabToImage: item is not attached to a window` |
| async grab again after re-showing the window | completes, valid image | none |
| synchronous `grabWindow()` on the hidden QQuickView window | completes in 3-6 ms with a valid image | none |
| synchronous `grabWindow()` on a hidden `QQuickWidget` render-control window | **null image** | none |

The asynchronous API can therefore fail **synchronously at request time** rather than
through a failed completion callback, and it requires the target's window to be visible. On
this host the synchronous path still captures a hidden `QQuickView` window, so a minimized or
occluded desktop window is capturable synchronously but not asynchronously. On the reference
EGLFS target the window is always visible, so this matters mainly for desktop-hosted use.

### 5.9 Are there public asynchronous equivalents for the widget targets?

| Target | Public async strategy | Evidence |
|---|---|---|
| `QQuickWidget` | **partial**: `rootObject()->grabToImage()` (or its window's `contentItem()`) works and captures the embedded Quick content plus overlays, at the QuickWidget's own size (699x557). It does **not** include the surrounding raster widgets, and `grabWindow()` on the QQuickWidget's render-control window returns null, so the whole-window composition still needs the synchronous parent `QWidget::grab()` (15.06 ms) | `host-windows-rhi-default-quickwidget-all.json` |
| `QOpenGLWidget` | **none**: there is no QML item tree, so `QQuickItem::grabToImage()` is not applicable at all. The only public capture is the synchronous `grabFramebuffer()` (4.43 ms avg, p95 4.44 ms, 46.9 /s) | `host-windows-rhi-default-openglwidget-all.json` |

## 6. Requirement-by-requirement result

The list below is the issue's public-async-first gate. `PASS` means measured with public Qt
APIs on this host.

| # | Requirement | Result | Evidence |
|---|---|---|---|
| R1 | Capture the **complete** window content (not just the application's root item) | **PASS**, with `QQuickWindow::contentItem()`; `FAIL` if the root item is used | 5.1 |
| R2 | Include overlay/sibling content (popups, tooltips, Overlay) | **PASS** with `contentItem()`: 3.31 % differing pixels, all inside the overlay rect, 0.00 % elsewhere | 5.1 |
| R3 | Reproduce composed Quick3D content | **PASS**: 0 differing pixels vs the synchronous whole-window capture | 5.2 |
| R4 | Reproduce custom `QQuickFramebufferObject` content, and see fresh FBO renders | **PASS**: counter advances 80 -> 105 over 26 distinct rendered frames | 5.2, 5.3 |
| R5 | Pixel fidelity against the synchronous capture | **PASS** for 2D/Quick3D (identical); **PASS within tolerance** for the custom FBO scene (max channel diff 2) | 5.2 |
| R6 | Latency | **PASS with a caveat**: >= 1 frame period; measured 18.2-31.9 ms across scenes/backends at K=1-4, growing to ~102 ms at K=16 | 5.5 |
| R7 | Sustainable cadence | **PASS**: 26.7 /s (K=1) up to 152.4 /s (K=16) on `quick2d`; 205.7 /s on `quickwidget`; 123.4 /s on `quick3d`; 137.0 /s on `customfbo` | 5.5 |
| R8 | Outstanding-request behaviour | **PASS with a caveat**: no built-in limit; K trades throughput against latency, freshness and GUI-thread load. Operating point from this evidence: K=2-4 | 5.5 |
| R9 | Ordering / drop semantics | **PASS with a caveat**: completions are FIFO and nothing is dropped, but duplicated content is returned for pipelined requests, so the caller must drop or coalesce | 5.5 |
| R10 | Content freshness / staleness | **PASS with a caveat**: tick lag average is exactly `(K-1)/2`; content is never newer than the request and is up to K-1 ticks old | 5.5 |
| R11 | Memory behaviour | **PASS**: bounded, no per-request leak (+45.5 MiB for 1000 requests vs +0.9 MiB in the no-capture control) | 5.6 |
| R12 | Backpressure against a slow client | **PASS with a caveat**: requires a caller-imposed bounded consumer window; unbounded production retains 36 MiB and would have to drop ~87 % of frames | 5.7 |
| R13 | Work while the target window is hidden/minimized | **FAIL**: request returns a null result and warns; the synchronous path still works on this host | 5.8 |
| R14 | Whole-window async for `QOpenGLWidget` | **FAIL**: no public asynchronous API exists for this target family | 5.9 |
| R15 | Whole-window async for `QQuickWidget` | **FAIL (partial)**: the embedded Quick content can be captured asynchronously, the surrounding widget composition cannot | 5.9 |
| R16 | Public API only, no private Qt API | **PASS**: every probe and every recommendation uses public APIs | — |

### 6.1 What the public asynchronous path fails to provide

Three real gaps, each stated as a specific requirement rather than as a general
dissatisfaction:

1. **R14 - `QOpenGLWidget` has no public asynchronous capture path.** The only option is the
   synchronous `grabFramebuffer()` (4.43 ms avg, p95 4.44 ms on this host). A GL/PBO path
   would be the only way to make this asynchronous.
2. **R15 - `QQuickWidget` cannot be composed asynchronously.** The embedded Quick content is
   available asynchronously, but the surrounding widget tree requires `QWidget::grab()`.
3. **R13 - the asynchronous path requires a visible window.** A hidden or minimized target
   cannot be captured asynchronously at all.

Two things the public path does **not** fail on, despite the concern in the issue text:
whole-window composition (R1-R2, solved by grabbing `contentItem()`) and fidelity
(R3-R5, pixel-identical for 2D and Quick3D).

### 6.2 Why no lower-level path was implemented

The issue permits GL/PBO, render-thread or RHI mechanisms only when the public path fails a
specific HyRemote requirement. On the evidence above:

- The whole-window composition, Quick3D, custom-FBO and fidelity requirements are **met** by
  the public path, so a private/RHI path is not justified for them.
- The two widget-target gaps (R14, R15) have not been shown to be *material*: the
  synchronous alternatives measure 4.4-15.1 ms with no loop stall, and SPIKE-01 already
  established that the same `QOpenGLWidget`/`QQuickWidget` paths are stable. A GL/PBO path
  would have to beat that by a margin that matters for the target's frame budget, and no
  such budget exists yet for RK3588/EGLFS.
- The visibility gap (R13) cannot be fixed by GL/PBO at all: it is a window-system
  precondition of the Quick render loop, not a readback concern.

Therefore this spike stops at the public path and records the three gaps as the input for a
future decision. If a lower-level path is pursued, the issue requires evidence that it
**materially solves a measured requirement gap**; per this evidence the candidate would be
"asynchronous `QOpenGLWidget` capture" and "asynchronous whole-window `QQuickWidget`
capture", not "make the Quick path faster".

## 7. Recommended v0.1 asynchronous design

1. **Grab `QQuickWindow::contentItem()`**, never the application's root item. This is public,
   covers overlays/popups/tooltips, and was pixel-identical to the synchronous whole-window
   capture for 2D and Quick3D.
2. **Bound the in-flight window at K = 2-4.** Above that, latency, staleness and GUI-thread
   jitter grow faster than the throughput gain (5.5).
3. **Pace the producer per delivered frame** when freshness matters: the paced control gave
   one distinct frame per request with zero duplicates and zero lag.
4. **Impose a bounded in-flight-to-consumer window** and a drop policy (drop-oldest or
   latest-frame-wins) instead of letting completed frames accumulate (5.7).
5. **Handle the synchronous rejection path**: `grabToImage()` can return null before any
   callback exists (detached item, invisible window), so the caller must treat "no result" and
   "null image" as first-class states and keep a full-frame/synchronous fallback.
6. **`RemoteFrame` consequences**: because a pipelined request can return a frame that is
   shared with other requests and up to K-1 ticks old, the frame needs a presentation
   timestamp and an explicit ownership/lifetime state (transfer or snapshot). The
   asynchronous delivery also means the capture completion, not the capture request, is the
   moment at which a buffer enters the transport's ownership.
7. **Report the staleness**: a `RemoteFrame` produced this way should carry "content age"
   implicitly through its PTS so the transport can drop stale frames rather than transmit
   them.
8. **Keep the synchronous path** for the widget-based targets and as a correctness
   cross-check; it is the only whole-window option for `QQuickWidget` and the only option at
   all for `QOpenGLWidget`.

## 8. Open risks

1. **Embedded validation missing.** All numbers are from one Windows host; EGLFS/OpenGL ES
   behaviour (especially the render-loop coupling and layer cost) is unverified.
2. **The render loop is the real bottleneck.** Each in-flight request costs one extra item
   render plus one readback in that frame, so the achievable cadence depends on the scene's
   own frame cost; a heavier scene will scale differently.
3. **Duplicated content is returned silently.** A naive integration that writes every
   completion to the transport would waste bandwidth on identical frames.
4. **The visibility precondition (R13)** may or may not matter depending on how HyRemote is
   deployed; it is unresolved for desktop hosts.
5. **`QQuickWidget`/`QOpenGLWidget` remain synchronous** for whole-window sharing.
6. **No `RemoteFrame` API exists yet**, so the ownership/recycle rules above are
   requirements, not an implementation.
7. The custom-FBO fidelity comparison is tolerance-based rather than exact, because that
   scene renders continuously by design.

## 9. Explicitly unverified

- RK3588, EGLFS, OpenGL ES: every case.
- Linux desktop (X11/Wayland) host behaviour.
- Non-zero device pixel ratios and resolutions above 960x600/1000x640.
- Multiple simultaneous `QQuickWindow`s, multiple native GL windows.
- Behaviour when several `grabToImage()` requests target different items or windows at once.
- Input delivery and transport integration.
- Any GL/PBO, render-thread, RHI or version-specific mechanism: not implemented in this
  spike by design.

## 10. Evidence index and reproduction

Evidence files live in
[`spikes/async-capture/evidence/`](../spikes/async-capture/evidence/) and each report contains
the raw probe data (`composition.overlayPresence`, `fidelity[]`, `pipeline.*`,
`pipeline.content.*`, `syncBaseline`, `failureModes[]`, `sceneInfo`).

| File | Content |
|---|---|
| `host-windows-rhi-default-quick2d-all.json` | `quick2d` composition + fidelity + pipeline (K=4) + failure modes, Direct3D 11 |
| `host-windows-rhi-opengl-quick2d-all.json` | same on the OpenGL RHI (backend comparison) |
| `host-windows-rhi-default-quick3d-all.json` | `quick3d` composition + fidelity + pipeline + failure modes |
| `host-windows-rhi-opengl-customfbo-all.json` | `customfbo` composition + fidelity + pipeline + failure modes |
| `host-windows-rhi-default-quickwidget-all.json` | `quickwidget` composition + fidelity + pipeline + failure modes |
| `host-windows-rhi-default-openglwidget-all.json` | `openglwidget`: no QML item tree, so composition/pipeline are not applicable |
| `host-windows-rhi-default-quick2d-pipeline-k{1,2,4,8,16}.json` | in-flight sweep, 240 requests each |
| `host-windows-rhi-default-quick2d-pipeline-k4-1000requests.json` | memory scaling |
| `host-windows-rhi-default-quick2d-pipeline-dryrun-control.json` | no-capture memory control |
| `host-windows-rhi-default-quick2d-pipeline-paced-k4.json` | paced control (one request per frame) |
| `host-windows-rhi-default-quick2d-pipeline-consumer-window2.json` | bounded consumer window |
| `host-windows-rhi-default-quick2d-pipeline-consumer-unbounded.json` | unbounded consumer window |
| `host-windows-rhi-*-quick2d-sync-baseline.json`, `*-quick3d-sync-baseline.json`, `*-customfbo-sync-baseline.json`, `*-quickwidget-sync-baseline.json` | synchronous baselines per scene/backend |

Build and run:

```bash
cmake -S spikes/async-capture -B build/async-spike -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=<qt-install>
cmake --build build/async-spike
./build/async-spike/hyremote-async-spike --list
./build/async-spike/hyremote-async-spike --scene quick2d --mode all --requests 60 --max-inflight 4 --json /tmp/quick2d.json
./build/async-spike/hyremote-async-spike --scene quick2d --mode pipeline --requests 240 --max-inflight 8 --json /tmp/k8.json
./build/async-spike/hyremote-async-spike --scene quick2d --mode pipeline --requests 240 --dry-run --interval-ms 32 --json /tmp/control.json
ctest --test-dir build/async-spike --output-on-failure
```

See [`spikes/async-capture/README.md`](../spikes/async-capture/README.md) for the full option
list.

## 11. Compatibility matrix rows

The status column uses the definitions of [`docs/compatibility.md`](compatibility.md).

| Qt | OS / target | QPA / graphics | Application type | Capture backend | Status | Notes |
|---|---|---|---|---|---|---|
| 6.8.3 | Windows 11 / x86_64 | `windows` / D3D11 and OpenGL | Qt Quick 2D | `QQuickWindow::contentItem()->grabToImage()` | Experimental (host) | Pixel-identical to the synchronous whole-window capture; 32 ms latency at K=1, 115 /s at K=4; requires a visible window |
| 6.8.3 | Windows 11 / x86_64 | `windows` / D3D11 | Quick3D | `contentItem()->grabToImage()` | Experimental (host) | 0 differing pixels, 123 /s at K=4 |
| 6.8.3 | Windows 11 / x86_64 | `windows` / OpenGL | custom `QQuickFramebufferObject` | `contentItem()->grabToImage()` | Experimental (host) | Fresh FBO content per rendered frame; max channel diff 2 |
| 6.8.3 | Windows 11 / x86_64 | `windows` / D3D11 | QQuickWidget | `rootObject()->grabToImage()` (Quick content only) | Experimental (host) | 699x557 Quick content; the surrounding widget composition still needs the synchronous parent `grab()` |
| 6.8.3 | Windows 11 / x86_64 | `windows` / D3D11 | QOpenGLWidget | none | Unsupported | No public asynchronous capture API exists for this family |
| 6.8.x | Embedded Linux / RK3588 | EGLFS / OpenGL ES | all | not evaluated | Unverified | The public asynchronous path was never executed on the reference target |
