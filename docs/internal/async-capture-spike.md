# SPIKE-02 Public Asynchronous Capture Evaluation

Status: **public asynchronous path evaluated; no lower-level path implemented**

Issue: [#16 [ARCH] Asynchronous GL/PBO capture path for Widgets and Quick targets](https://github.com/skawu/HyRemote/issues/16)

This document records the evidence produced by the throwaway harness in
[`research/async-capture/`](../research/async-capture/). It answers the questions of issue #16
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

> **Round-2 correction notice.** The Round-1 review of this spike (PR #19 comment
> `5662534191`) found one evidence-level defect and two over-broad statements. Section 5.7 is
> rewritten around an explicit **single serial consumer** model (one frame serviced at a time,
> 100 ms per frame) with three separate producer configurations, and the measured bounds are
> now `maxQueueDepthObserved <= completedQueueCapacity` with a documented +1 ownership bound.
> The two documentation boundaries are narrowed in sections 3, 5.1, 5.8 and 6: the validated
> claim is **same-scene sibling/overlay-tree content under `contentItem()`**, not all Qt Quick
> popup modes, and only the **hidden** window case was tested - `minimized` stays unverified.
> The positioning of `grabToImage()` is corrected as well: it is a public-API asynchronous
> *baseline / v0.1 candidate*, not a claim about the final high-performance production path.
> The composition, Quick3D and custom-FBO fidelity evidence is unchanged by this round.

> **Round-2 review terminology correction.** The Round-2 review (PR #19 comment
> `5663534836`) accepted the backpressure result but found the timestamp/freshness wording
> reversed. `tick in image - tick at request` is a **signed scene-state advance from the request
> to the captured image**; a positive value means the image is *newer* than the request, so the
> earlier phrasings ("lags the request", "never newer than the request", "up to K-1 ticks old")
> were wrong and have been removed. The architectural consequence is recorded in section 5.5
> and in recommendation 6 of section 7: **the request timestamp is not a valid content PTS**,
> and the v0.1 baseline is the completion / `ready()` timestamp. The consumer's `staleFrames`
> is redefined as a **request-sequence age**, not visual content staleness. No measurement was
> re-run for this round; only the wording and the derivations that depend on it changed.

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

**Positioning.** Qt documents `QQuickItem::grabToImage()` as an asynchronous API that renders
the item to an offscreen surface and copies the result from GPU to CPU memory, and explicitly
warns that this "can be quite costly" for live preview. This spike therefore treats
`contentItem()->grabToImage()` as the **public-API asynchronous baseline and v0.1 candidate**,
not as the final high-performance production path. It is measured here to establish what the
public API does and does not satisfy; the final performance choice remains evidence-driven by
the RK3588/EGLFS validation and by the low-copy investigation in
[#17](https://github.com/skawu/HyRemote/issues/17).

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
   `QQuickWindow::contentItem()`. Same-scene overlay content (an item parented to that
   `contentItem()`, which is what `Popup.Item` and `Overlay` use) is therefore a **sibling** of
   the application's root item.

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
  to a specific harness-visible scene state, which is what makes completion ordering,
  duplication and the request-to-image scene-state advance measurable rather than inferred.
- an **overlay sibling**: a QML rectangle parented to `QQuickWindow::contentItem()`, i.e.
  the same parent chain a same-scene QML overlay / `Popup.Item` uses. It is deliberately not a
  descendant of the application's root item, and it is a plain item rather than a Qt Quick
  Controls `Popup`/`ToolTip` in a specific popup mode (see the boundary table in section 5.1).

### 4.2 Probes

| Probe | What it measures |
|---|---|
| composition | where the overlay lands in a root-item grab, a `contentItem()` grab and a synchronous `grabWindow()` grab |
| fidelity | pixel comparison of the frozen scene: async `contentItem()` vs async root item vs synchronous whole-window capture, including a repeat pair |
| pipeline | the asynchronous loop: latency, cadence, in-flight behaviour, completion order, request-to-image scene-state advance, duplication, memory, GUI-thread impact |
| sync baseline | the synchronous public capture for the same scene in the same session, for comparison |
| failure modes | the documented rejections (hidden window, detached item) and recovery afterwards. Only `hide()` is exercised; `minimized` is not tested |
| consumer | one serial consumer with a service time per frame, its completed queue, the chosen backpressure strategy and the resulting queue-depth, age and drop figures |

### 4.3 Controls

- **frozen scene**: for fidelity only; the scene stops animating and `tick` is pinned to
  4242, so the only legitimate difference between two captures is composition, not time.
- **paced requests** (`--paced`): pump the event loop between individual requests, so each
  request lands in its own rendered frame. This is the control that separates "pipelining
  collapses content" from "the harness issued requests faster than frames".
- **dry run** (`--dry-run --interval-ms 32`): the same loop, scene updates and bookkeeping
  with **no capture at all**, to separate the capture path's memory cost from the scene's.
- **serial consumer** (`--consumer-service-ms N --completed-queue-capacity C --backpressure
  <strategy>`): completed frames enter one queue; a single consumer takes one frame at a time
  and is busy with it for N ms, so it cannot service two frames in parallel and its sustained
  rate is `1000 / N` frames per second. The three strategies below are measured separately so
  that the consumer model and the producer policy cannot be confused with each other.
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
not the QML root item. This is a public API and covers **same-scene sibling content in the
window's overlay tree**.

**Boundary of that claim.** The probe places a plain QML `Rectangle` sibling under
`contentItem()`, which is the parent chain that `Popup.Item`, `Overlay` and tooltips use
inside the same scene. Qt 6.8 distinguishes `Popup.Item` (same-scene overlay),
`Popup.Window` (a separate top-level window) and `Popup.Native`. Only the same-scene case is
measured here:

| Popup mode | Status |
|---|---|
| `Popup.Item` (same scene, parented under `contentItem()`) | **architectural inference** from the measured sibling case; not separately executed |
| `Popup.Window` (separate top-level window) | **unverified** - expected to behave like the popups/dialogs that SPIKE-01 showed are excluded from a top-level widget capture |
| `Popup.Native` | **unverified** |
| Qt Quick Controls `Popup` / `ToolTip` as actually configured by an application | **unverified** as such; only the underlying same-scene parent chain was exercised |

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

### 5.3 Custom FBO content is re-rendered, not a stale texture

`customfbo` pipeline run: 60 requests produced `distinctFboCounters = 29` and
`distinctTicks = 29` over 29 rendered frames, with the encoded counter advancing 222 -> 250.
Every captured frame therefore carried a *newly rendered* custom-FBO render, so the
asynchronous layer path does not hand back a cached, stale or blank FBO texture. This claim is
about the FBO texture being re-rendered per frame; it says nothing about the age of the content
relative to a request, which is governed by the advance semantics of section 5.5.

### 5.4 Synchronous baseline in the same session

| Scene | Backend | Synchronous API | avg | p95 | Rate | GUI latency avg / p95 |
|---|---|---|---|---|---|---|
| `quick2d` | D3D11 | `grabWindow()` | 17.70 ms | 31.44 ms | **1.1 /s** | 583.64 / 1002.76 ms |
| `quick2d` | OpenGL | `grabWindow()` | 17.35 ms | 20.93 ms | 27.8 /s | 4.52 / 18.94 ms |
| `quick3d` | D3D11 | `grabWindow()` | 13.61 ms | 20.68 ms | **1.0 /s** | 730.39 / 1014.95 ms |
| `customfbo` | OpenGL | `grabWindow()` | 15.86 ms | 18.42 ms | 30.1 /s | 8.12 / 18.17 ms |
| `quickwidget` | D3D11 | parent `QWidget::grab()` | 15.06 ms | 18.68 ms | 30.8 /s | 3.88 / 15.16 ms |
| `openglwidget` | D3D11 | `QOpenGLWidget::grabFramebuffer()` | 3.98 ms | 3.29 ms | 48.3 /s | 1.21 / 1.93 ms |

The first five rows come from the dedicated `*-sync-baseline.json` runs (one measurement
purpose per process); the `openglwidget` row comes from its `-all.json` run, which is the only
one that measures that scene.

This reproduces the SPIKE-01 Direct3D 11 result independently: a synchronous
`grabWindow()` per iteration collapses the application loop to about one iteration per
second while the call itself reports ~12-18 ms, and the GUI latency probe reaches 1003-1017 ms.
The `QQuickWidget` parent `grab()` and the `QOpenGLWidget` `grabFramebuffer()` are not
affected by that stall.

**Host variance.** The `-all` runs re-measure the same synchronous baseline in a warmer
process state (after composition, fidelity and pipeline work) and report higher figures for
some scenes - most visibly `customfbo`, where the same configuration measured 39.69 ms avg,
13.2 /s and GUI p95 78.21 ms instead of 15.86 ms / 30.1 /s / 18.17 ms. That is run-to-run
variance on an uncontrolled desktop (SPIKE-01 section 2.5 recorded 20-40 % spread), not a
different configuration; the dedicated per-scene files are the authoritative baseline and the
duplicated measurement is kept in the evidence for transparency.

### 5.5 Asynchronous pipeline: latency, cadence, in-flight behaviour

`quick2d`, Direct3D 11, `contentItem()`, 240 requests per configuration, no consumer delay.

The harness writes a `tick` value into the scene before issuing each request, and the captured
image encodes the `tick` that was rendered into it. The reported metric is therefore

```
scene-state advance (request -> captured image) = tick in image - tick at request
```

**Sign convention.** A **positive** value means the captured image carries a scene state that is
*newer* than the scene state at the moment the request was issued: the request was serviced by a
render that happened later than the request itself. It is **not** an age or a staleness
measurement, and a positive value must never be read as "the image is behind its request".
A negative value would mean the image carried a state older than the request, which did not
occur in any run here. The JSON key is `tickLag*` in the evidence files; it holds exactly this
signed advance, and the name is retained only so the recorded reports stay readable - the
definition above is authoritative.

| In flight (K) | Completed/s | Latency avg | Latency p95 | Distinct ticks | Duplicate completions | Scene-state advance avg / max | Frames rendered | GUI latency avg / p95 | RSS growth |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 25.7 | 32.77 ms | 37.82 ms | **240** | 0 | 0.00 / 0.00 | 560 | 1.23 / 12.54 ms | +25.6 MiB |
| 2 | 56.6 | 32.86 ms | 40.84 ms | 121 | 119 | 0.50 / 1.00 | 254 | 3.14 / 15.72 ms | +31.6 MiB |
| 4 | 119.1 | 32.38 ms | 33.53 ms | 60 | 180 | 1.50 / 3.00 | 120 | 8.05 / 17.55 ms | +37.1 MiB |
| 8 | 158.8 | 48.87 ms | 50.17 ms | 30 | 210 | 3.50 / 7.00 | 60 | 17.94 / 34.39 ms | +37.5 MiB |
| 16 | 191.6 | 80.23 ms | 82.35 ms | 15 | 225 | 7.50 / 15.00 | 30 | 22.77 / 52.44 ms | +32.1 MiB |

No consumer is modelled in these five runs, so every completed frame is released immediately
(`consumer.model = none` in the reports).

Readings:

- **Throughput scales with K** (25.7 -> 191.6 captures/s) because one rendered frame serves K
  grabs. The scene's own frame rate *falls* (560 -> 30 frames) because each frame now carries
  K extra item renders and readbacks.
- **Latency grows with K** (33 -> 80 ms) and every request pays at least one frame.
- **Pipelining does not produce more distinct frames.** Distinct ticks are exactly
  `requests / K`, duplicate completions are `requests - requests/K`, and the average
  scene-state advance is exactly `(K-1)/2`: all pending requests are served by a later shared
  render, so they receive the **same newer** visual state. A pending request does not hold a
  frozen copy of "the scene as it was when I asked" - what it returns is whatever state the
  servicing render produced, which for K>1 is ahead of the request time. This is precisely why
  a request timestamp cannot serve as the content timestamp of the returned frame.
- **Completion order is FIFO**: 0 out-of-order completions in every configuration, on both
  backends. There is **no drop or dedup mechanism**; every request completes with a full
  image, duplicates included. Deciding what to do with duplicated content is the caller's
  responsibility.
- **GUI-thread cost grows with K** (1.23 -> 22.77 ms average, 12.54 -> 52.44 ms p95). At
  K >= 8 the local UI jitter is comparable to the SPIKE-01 capture-induced damage figures,
  i.e. it is no longer negligible.
- **Outstanding requests have no built-in limit.** Each in-flight request owns a
  `QSGLayer` render target plus a readback image, so K is bounded only by memory and by the
  render thread's ability to serve K grabs per frame.

The `paced` control (one request per rendered frame, K=4, 60 requests) gives
**60 distinct ticks, 0 duplicates, scene-state advance 0.00**, 33.35 ms latency, 61 frames and
GUI latency 0.87 / 1.20 ms. That isolates the effect: the collapse in the table above is caused
by issuing several requests before a frame is rendered, not by the API itself.

**Timestamp contract (input for ARCH-01).** Because of the above, **the request timestamp is not
a valid content PTS for `grabToImage()` under pipelining**. The safe v0.1 baseline is to take the
frame's timestamp at **completion / `ready()` time**, which is also the moment the buffer enters
the caller's ownership; the request timestamp is still useful, but only as a latency diagnostic
for that request. A future lower-level backend that can observe the actual rendered or presented
frame time (for example a render-thread or presentation hook) may supply a more accurate content
PTS, and the generic Core must not depend on either choice: it must accept a PTS from the
backend rather than deriving one from the request.

### 5.6 Memory behaviour

| Configuration | Requests | Frames rendered | RSS growth | Growth per request |
|---|---|---|---|---|
| K=4, burst | 240 | 120 | +37.1 MiB | 158 KiB |
| K=4, burst | 1000 | 595 | **+38.7 MiB** | **40 KiB** |
| dry run (no capture), 32 ms pacing | 240 | 613 | **+0.9 MiB** | 4 KiB |

Growth does not scale with the number of captures: quadrupling the request count from 240 to
1000 leaves the total growth at ~+38 MiB, and the no-capture control in the same loop grows
0.9 MiB. The residual is a bounded warm-up/caching cost of the layer path, **not a
per-request leak**. (The harness itself was fixed during this spike: an earlier probe
retained a result/connection reference cycle and measured its own retention. The numbers
above are from the corrected probe, which releases the `QQuickItemGrabResult` and its pixels
as soon as it has decoded them.)

### 5.7 Backpressure with a single serial slow consumer

**Consumer model.** Completed frames enter one queue. A **single serial consumer** takes one
frame at a time out of that queue and is busy with it for `consumerServiceMs`, so two frames
can never be serviced at the same time and the sustained rate is `1000 / consumerServiceMs`
frames per second. With `--consumer-service-ms 100` the target rate is 10 frames/s, which is
what the measurements below confirm (9.4-9.9 frames/s). A frame is owned by the consumer from
the moment it is taken out of the queue until its service ends.

`--completed-queue-capacity C` bounds the **queue depth**. The frame currently being serviced
is owned in addition to the queue, so the total ownership bound is exactly `C + 1` frames.
The former `--consumer-window` setting mixed producer admission with consumer ownership and
allowed an overshoot of up to K frames; it has been replaced by this explicit capacity plus a
separate strategy switch.

Three producer configurations were measured on `quick2d`, Direct3D 11, `contentItem()`, K=4:

| Strategy | Requests | Producer /s | Consumer /s | Delivered | Dropped | Drop ratio | Max queue depth | Max owned frames | Peak retained | Completion-to-service age avg / p95 / max | Max request-sequence age | Load + drain |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| **none** (unbounded queue) | 60 | **116.9** | 9.7 | 60 | 0 | 0 % | **55** | 56 | **126000 KiB (123 MiB)** | 2848 / 5453 / 5786 ms | **55** | 0.51 s + 5.89 s |
| **drop-oldest** (latest-frame-wins), capacity 2 | 240 | **119.3** | 9.9 | 22 | **218** | **90.8 %** | **2** | **3** | **6750 KiB (6.6 MiB)** | 21.8 / 70.2 / 170.2 ms | **2** | 2.01 s + 0.29 s |
| **producer-throttle** (admission control), capacity 2 | 60 | **9.8** | 9.5 | 60 | 0 | 0 % | **2** | **3** | **6750 KiB (6.6 MiB)** | 172.4 / 183.4 / 194.8 ms | **2** | 6.12 s + 0.28 s |

Two different quantities appear in this table and they must not be conflated:

- **Completion-to-service age** (wall clock, measured): the time from the moment the capture
  completed (`ready()`, when the frame entered the consumer's ownership) to the moment the
  consumer started servicing it. This is a real elapsed-time measurement.
- **Request-sequence age** (frame counter, `producedSoFar - frame->index`): how many *later
  requests* the producer had already completed before this frame was serviced. It is an
  ordering/backlog indicator in the request sequence. It is **not** a measurement of visual
  content staleness: the pixels in a frame correspond to whatever scene state its servicing
  render produced, and per section 5.5 that state can be *ahead* of the request time, so "age
  in requests" is not "age of the picture". The JSON key is `staleFrames*` in the evidence
  files; it holds this request-sequence age, and the name is retained only so the recorded
  reports stay readable - the definition above is authoritative.
- **Scene-state freshness of these frames is not measured here** and should not be claimed. The
  capture pipeline measures request-to-image scene-state advance separately (section 5.5);
  the consumer measurements below bound memory, rates and wall-clock age, not visual content
  age.

Readings:

- **The queue bound is now enforced.** With a capacity of 2 the observed queue depth is
  exactly 2 and total ownership is 3 frames (`capacity + 1`, the frame being serviced), i.e.
  `maxQueueDepthObserved <= completedQueueCapacity` holds strictly and the ownership overshoot
  is exactly one frame by construction instead of up to K.
- **Unbounded, the backlog is not a queueing delay but a growing debt.** After only 0.51 s of
  capture the queue held 55 frames (123 MiB); the consumer needed another 5.89 s to drain it,
  and the frames it was still delivering then were 55 positions behind in the request sequence
  and up to 5.8 s old in wall clock since their completion. For a 10 fps consumer, a 117 fps
  producer must **drop about 92 % of frames** to stay bounded.
- **drop-oldest keeps capture and transport latency decoupled.** The producer kept its full
  rate (119.3 /s), retention stayed at 6.6 MiB, 90.8 % of frames were discarded, and the
  delivered frames were at most 2 positions behind in the request sequence with 21.8 ms average
  completion-to-service age. This is the policy that satisfies "transport slowness never blocks
  capture/render" while keeping what the viewer receives as recent as the transport allows.
- **producer-throttle is a different trade-off, not an implementation of that policy.** It
  drops nothing and keeps the same bounded memory, but the capture rate collapses to the
  consumer's rate (9.8 /s) and the maximum in-flight capture concurrency drops below K
  (observed 2), i.e. capture is coupled to the transport. Its frames also wait longer before
  service (172 ms average completion-to-service age versus 22 ms), because nothing is captured
  while the queue is full.
- **Conclusion.** The public API provides no queue, no capacity and no drop policy, so
  HyRemote must own them. The corrected evidence supports the earlier high-level conclusion -
  a bounded completed queue plus a drop/coalesce policy is required so that transport
  slowness never blocks capture or render - and it now also quantifies the cost of each
  policy: ~92 % drops with latest-frame-wins, or a capture rate pinned to the consumer with
  admission control. The accepted default remains the **bounded completed queue with
  drop-oldest / latest-frame-wins**.

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
this host the synchronous path still captures a hidden `QQuickView` window, so a hidden
desktop window is capturable synchronously but not asynchronously.

**Boundary of that claim.** Only `QWidget::hide()` / `QWindow::hide()` was exercised:

| Window state | Status |
|---|---|
| hidden via `hide()` | **measured**: async request rejected with a null result; synchronous `grabWindow()` still returns an image on this host |
| minimized | **unverified** - not tested in this spike |
| occluded / partially covered by another window | **unverified** |
| on the reference EGLFS target (no window manager, always visible) | **unverified** |

On the reference EGLFS target the window is never hidden by a window manager, so this
limitation matters mainly for desktop-hosted use - but that statement is an expectation, not
a measurement.

### 5.9 Are there public asynchronous equivalents for the widget targets?

| Target | Public async strategy | Evidence |
|---|---|---|
| `QQuickWidget` | **partial**: `rootObject()->grabToImage()` (or its window's `contentItem()`) works and captures the embedded Quick content plus overlays, at the QuickWidget's own size (699x557). It does **not** include the surrounding raster widgets, and `grabWindow()` on the QQuickWidget's render-control window returns null, so the whole-window composition still needs the synchronous parent `QWidget::grab()` (15.06 ms) | `host-windows-rhi-default-quickwidget-all.json` |
| `QOpenGLWidget` | **none**: there is no QML item tree, so `QQuickItem::grabToImage()` is not applicable at all. The only public capture is the synchronous `grabFramebuffer()` (3.98 ms avg, p95 3.29 ms, 48.3 /s) | `host-windows-rhi-default-openglwidget-all.json` |

## 6. Requirement-by-requirement result

The list below is the issue's public-async-first gate. `PASS` means measured with public Qt
APIs on this host.

| # | Requirement | Result | Evidence |
|---|---|---|---|
| R1 | Capture the **complete** window content (not just the application's root item) | **PASS**, with `QQuickWindow::contentItem()`; `FAIL` if the root item is used | 5.1 |
| R2 | Include same-scene sibling/overlay-tree content (the parent chain `Popup.Item` and `Overlay` use) | **PASS with `contentItem()`** for a same-scene sibling item: 3.31 % differing pixels, all inside the overlay rect, 0.00 % elsewhere. `Popup.Window`, `Popup.Native` and Control-specific `Popup`/`ToolTip` usage are **unverified** | 5.1 |
| R3 | Reproduce composed Quick3D content | **PASS**: 0 differing pixels vs the synchronous whole-window capture | 5.2 |
| R4 | Reproduce custom `QQuickFramebufferObject` content, and see fresh FBO renders | **PASS**: counter advances 222 -> 250 over 29 distinct rendered frames | 5.2, 5.3 |
| R5 | Pixel fidelity against the synchronous capture | **PASS** for 2D/Quick3D (identical); **PASS within tolerance** for the custom FBO scene (max channel diff 2) | 5.2 |
| R6 | Latency | **PASS with a caveat**: >= 1 frame period; measured 32.4-32.9 ms at K=1-4 on `quick2d`, growing to 80.2 ms at K=16 | 5.5 |
| R7 | Sustainable cadence | **PASS**: 25.7 /s (K=1) up to 191.6 /s (K=16) on `quick2d`; 87.4 /s on `quickwidget`; 89.3 /s on `quick3d`; 38.4 /s on `customfbo` (that scene renders continuously by design and is the most load-sensitive of the set; an earlier run of the same configuration reported 137 /s) | 5.5 |
| R8 | Outstanding-request behaviour | **PASS with a caveat**: no built-in limit; K trades throughput against latency, request/scene-state correspondence and GUI-thread load. Operating point from this evidence: K=2-4 | 5.5 |
| R9 | Ordering / drop semantics | **PASS with a caveat**: completions are FIFO and the API drops nothing, but duplicated content is returned for pipelined requests, so the caller must drop or coalesce | 5.5 |
| R10 | Content freshness / staleness | **PASS with a caveat, redefined**: the measured quantity is the signed scene-state advance from request to captured image, averaging exactly `(K-1)/2`, i.e. for K>1 the returned image is *newer* than the request. There is no measurement of visual content being stale relative to the request; what the caller loses is the correspondence between a request and a specific scene state, so a request timestamp cannot be used as the frame's content timestamp | 5.5 |
| R11 | Memory behaviour | **PASS**: bounded, no per-request leak (+38.7 MiB for 1000 requests vs +0.9 MiB in the no-capture control) | 5.6 |
| R12 | Backpressure against a slow client | **PASS with a caveat**: the API provides no queue, capacity or drop policy; with a caller-owned bounded queue of 2 and latest-frame-wins the producer keeps its rate and ~91 % of frames are dropped, whereas an unbounded queue would have retained 123 MiB and fallen 5.8 s behind | 5.7 |
| R13 | Work while the target window is **hidden** | **FAIL**: the request returns a null result and warns; the synchronous path still works on this host. `minimized` and occluded are **unverified** | 5.8 |
| R14 | Whole-window async for `QOpenGLWidget` | **FAIL**: no public asynchronous API exists for this target family | 5.9 |
| R15 | Whole-window async for `QQuickWidget` | **FAIL (partial)**: the embedded Quick content can be captured asynchronously, the surrounding widget composition cannot | 5.9 |
| R16 | Public API only, no private Qt API | **PASS**: every probe and every recommendation uses public APIs | — |

### 6.1 What the public asynchronous path fails to provide

Three real gaps, each stated as a specific requirement rather than as a general
dissatisfaction:

1. **R14 - `QOpenGLWidget` has no public asynchronous capture path.** The only option is the
   synchronous `grabFramebuffer()` (3.98 ms avg, p95 3.29 ms on this host). A GL/PBO path
   would be the only way to make this asynchronous.
2. **R15 - `QQuickWidget` cannot be composed asynchronously.** The embedded Quick content is
   available asynchronously, but the surrounding widget tree requires `QWidget::grab()`.
3. **R13 - the asynchronous path requires a visible window.** A hidden target cannot be
   captured asynchronously at all; whether `minimized` behaves the same way was not tested.

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
   covers same-scene sibling/overlay-tree content, and was pixel-identical to the synchronous
   whole-window capture for 2D and Quick3D. Treat the specific Qt Quick popup modes as
   separate compatibility cases still to be verified.
2. **Bound the in-flight window at K = 2-4.** Above that, latency, the loss of
   request-to-scene-state correspondence and GUI-thread jitter grow faster than the throughput
   gain (5.5).
3. **Pace the producer per delivered frame** when a request must map to one specific scene
   state: the paced control gave one distinct frame per request with zero duplicates and zero
   request-to-image scene-state advance.
4. **Own a bounded completed queue and an explicit drop policy.** The API provides neither.
   With a single 10 fps consumer the measured choice is between latest-frame-wins (drop
   ~91 %, keep the producer at full rate, 6.6 MiB retained, at most 2 positions behind in the
   request sequence and 21.8 ms average completion-to-service age) and admission control (drop
   nothing, but capture is pinned to the consumer rate and frames wait 172 ms on average for
   service). The recommended default is the bounded queue with latest-frame-wins, because
   transport slowness must not throttle capture (5.7).
5. **Handle the synchronous rejection path**: `grabToImage()` can return null before any
   callback exists (detached item, invisible window), so the caller must treat "no result" and
   "null image" as first-class states and keep a full-frame/synchronous fallback.
6. **`RemoteFrame` timestamp**: **do not use the request time as the frame's content/PTS.**
   Under pipelining the returned image can be *newer* than the request (5.5), so a request
   timestamp does not identify the scene state in the frame. Take the timestamp at capture
   completion / `ready()` time as the safe v0.1 baseline, and let a backend that can observe
   the real rendered or presented frame time provide a more accurate one. Keep the request time
   only as a latency diagnostic for that request.
7. **`RemoteFrame` ownership**: the frame also needs an explicit ownership/lifetime state
   (transfer or snapshot). The asynchronous delivery means the capture completion, not the
   capture request, is the moment at which a buffer enters the transport's ownership, and it
   is also the moment the baseline timestamp is taken.
8. **Frame selection in the transport** follows from the PTS policy of item 6: the transport can
   drop or coalesce frames by comparing their completion timestamps, rather than assuming a
   fixed relationship between a request and the content it produced (5.7).
9. **Keep the synchronous path** for the widget-based targets and as a correctness
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
   deployed; it is unresolved for desktop hosts, and `minimized`/occluded were not tested.
5. **`QQuickWidget`/`QOpenGLWidget` remain synchronous** for whole-window sharing.
6. **No `RemoteFrame` API exists yet**, so the ownership/recycle rules above are
   requirements, not an implementation.
7. The custom-FBO fidelity comparison is tolerance-based rather than exact, because that
   scene renders continuously by design.
8. **The consumer numbers come from a model, not from a real transport.** The serial consumer
   services a frame in a fixed 100 ms and the queue is bounded in frames; a real transport has
   variable service time, encoding, and possibly its own buffering. The measured relations
   (drop ratio, retention bound, completion-to-service age) are the input to that design, not a
   substitute for measuring it.
9. **`grabToImage()` is positioned as the public-API baseline / v0.1 candidate**, not as the
   final high-performance path; Qt documents the offscreen render plus GPU->CPU copy as
   costly, and neither the embedded validation nor the low-copy investigation (#17) has
   happened yet.

## 9. Explicitly unverified

- RK3588, EGLFS, OpenGL ES: every case.
- Linux desktop (X11/Wayland) host behaviour.
- Non-zero device pixel ratios and resolutions above 960x600/1000x640.
- Multiple simultaneous `QQuickWindow`s, multiple native GL windows.
- Behaviour when several `grabToImage()` requests target different items or windows at once.
- Input delivery and transport integration.
- Any GL/PBO, render-thread, RHI or version-specific mechanism: not implemented in this
  spike by design.
- Qt Quick popup modes other than a same-scene sibling under `contentItem()`:
  `Popup.Window`, `Popup.Native`, and Qt Quick Controls `Popup`/`ToolTip` as configured by an
  application.
- `minimized` and occluded target windows: only `hide()` was exercised.
- The **visual content age** of a frame that a consumer receives: the consumer probe measures
  queue depth, completion-to-service wall-clock age, rates and drop accounting, and the capture
  pipeline measures request-to-image scene-state advance separately. No probe in this spike
  measures how old the pixels in a delivered frame are relative to the live scene.
- Pipelining with a content PTS taken from a backend render/presentation hook: not implemented,
  so the recommended completion-time PTS is a v0.1 baseline rather than a measured optimum.
- A real transport or encoder as the consumer; the measured consumer is the explicit serial
  model of section 5.7.

## 10. Evidence index and reproduction

Evidence files live in
[`research/async-capture/evidence/`](../research/async-capture/evidence/) and each report contains
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
| `host-windows-rhi-default-quick2d-pipeline-k{1,2,4,8,16}.json` | in-flight sweep, 240 requests each, no consumer modelled |
| `host-windows-rhi-default-quick2d-pipeline-k4-1000requests.json` | memory scaling |
| `host-windows-rhi-default-quick2d-pipeline-dryrun-control.json` | no-capture memory control |
| `host-windows-rhi-default-quick2d-pipeline-paced-k4.json` | paced control (one request per frame) |
| `host-windows-rhi-default-quick2d-consumer-unbounded-queue.json` | single 10 fps consumer, unbounded completed queue (60 requests) |
| `host-windows-rhi-default-quick2d-consumer-drop-oldest-cap2.json` | single 10 fps consumer, queue capacity 2, latest-frame-wins (240 requests) |
| `host-windows-rhi-default-quick2d-consumer-producer-throttle-cap2.json` | single 10 fps consumer, queue capacity 2, producer admission control (60 requests) |
| `host-windows-rhi-*-quick2d-sync-baseline.json`, `*-quick3d-sync-baseline.json`, `*-customfbo-sync-baseline.json`, `*-quickwidget-sync-baseline.json` | synchronous baselines per scene/backend |

Every pipeline report exposes the consumer model and its outcome under `consumerConfig` and
`consumer` (`serviceMsPerFrame`, `targetConsumerFps`, `completedQueueCapacity`,
`backpressureStrategy`, `delivered`, `dropped`, `dropRatio`, `maxQueueDepthObserved`,
`maxOwnedFramesObserved`, `backlogAtEndOfLoad`, `backlogGrowthPerSecond`, `frameAge*Ms`,
`staleFrames*`), so the tables in section 5.7 can be recomputed from the files alone.

Two report keys carry names that predate the corrected semantics and are defined here so that
no reader has to guess:

| Key | Actual quantity |
|---|---|
| `latency`, `outcome.loadSeconds`, `outcome.drainSeconds` | timing of the request/completion path (request -> `ready()`) and of the load/drain phases |
| `content.tickLag*` | **signed scene-state advance from request to captured image** (`tick in image - tick at request`); positive = the image carries a newer scene state than the request. Not an age, not staleness |
| `consumer.frameAge*Ms` | completion-to-service wall-clock age inside the serial consumer |
| `consumer.staleFrames*` | **request-sequence age** (`producedSoFar - frame index`): how many later requests had completed before this frame was serviced. Not visual content age |

Build and run:

```bash
cmake -S research/async-capture -B build/async-spike -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=<qt-install>
cmake --build build/async-spike
./build/async-spike/hyremote-async-spike --list
./build/async-spike/hyremote-async-spike --scene quick2d --mode all --requests 60 --max-inflight 4 --json /tmp/quick2d.json
./build/async-spike/hyremote-async-spike --scene quick2d --mode pipeline --requests 240 --max-inflight 8 --json /tmp/k8.json
./build/async-spike/hyremote-async-spike --scene quick2d --mode pipeline --requests 240 --dry-run --interval-ms 32 --json /tmp/control.json
# single serial 10 fps consumer, unbounded completed queue
./build/async-spike/hyremote-async-spike --scene quick2d --mode pipeline --requests 60 --max-inflight 4 \
    --consumer-service-ms 100 --json /tmp/consumer-unbounded.json
# single serial 10 fps consumer, bounded queue of 2 with latest-frame-wins
./build/async-spike/hyremote-async-spike --scene quick2d --mode pipeline --requests 240 --max-inflight 4 \
    --consumer-service-ms 100 --completed-queue-capacity 2 --backpressure drop-oldest --json /tmp/consumer-dropoldest.json
# same bound, but the producer is throttled instead of dropping
./build/async-spike/hyremote-async-spike --scene quick2d --mode pipeline --requests 60 --max-inflight 4 \
    --consumer-service-ms 100 --completed-queue-capacity 2 --backpressure producer-throttle --json /tmp/consumer-throttle.json
ctest --test-dir build/async-spike --output-on-failure
```

See [`research/async-capture/README.md`](../../research/async-capture/README.md) for the full option
list.

## 11. Compatibility matrix rows

The status column uses the definitions of [`docs/compatibility.md`](../compatibility.md).

| Qt | OS / target | QPA / graphics | Application type | Capture backend | Status | Notes |
|---|---|---|---|---|---|---|
| 6.8.3 | Windows 11 / x86_64 | `windows` / D3D11 and OpenGL | Qt Quick 2D | `QQuickWindow::contentItem()->grabToImage()` | Experimental (host) | Public-API asynchronous baseline / v0.1 candidate. Pixel-identical to the synchronous whole-window capture; 32.8 ms latency at K=1, 119.1 /s at K=4; requires a visible window; no queue or drop policy |
| 6.8.3 | Windows 11 / x86_64 | `windows` / D3D11 | Quick3D | `contentItem()->grabToImage()` | Experimental (host) | 0 differing pixels, 123 /s at K=4 |
| 6.8.3 | Windows 11 / x86_64 | `windows` / OpenGL | custom `QQuickFramebufferObject` | `contentItem()->grabToImage()` | Experimental (host) | Fresh FBO content per rendered frame; max channel diff 2 |
| 6.8.3 | Windows 11 / x86_64 | `windows` / D3D11 | QQuickWidget | `rootObject()->grabToImage()` (Quick content only) | Experimental (host) | 699x557 Quick content; the surrounding widget composition still needs the synchronous parent `grab()` |
| 6.8.3 | Windows 11 / x86_64 | `windows` / D3D11 | QOpenGLWidget | none | Unsupported | No public asynchronous capture API exists for this family |
| 6.8.x | Embedded Linux / RK3588 | EGLFS / OpenGL ES | all | not evaluated | Unverified | The public asynchronous path was never executed on the reference target |
