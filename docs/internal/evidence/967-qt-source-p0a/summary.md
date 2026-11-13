# #967 P0-A: tagged Qt source basis and read-only normalisation

Status: read-only. No build, no CTest, no RealVNC run, no performance-path edit
was made for this report. `PRODUCT_GO=NO`, experimental GL stays default OFF.

| Field | Value |
| --- | --- |
| Performance baseline | `develop` @ `e2336c139eff78d6ed2b384eb1a61a49adcc2e8b` (#980 merged) |
| Older trace set A | `tmp/win967/matrix-35s/` (source `a43b76e1d95821d47f2703db945351ea6cd19daf`) |
| Newer trace set B | `tmp/win967/matrix-35s-99e6fd1e/` (source `99e6fd1e16e7ea4a6f2aef2d7df7f8cdf3221ca5`) |
| Analyzer | `tools/perf/analyze_frame_trace.py` schema=3 + `tools/perf/attribute_frame_stages.py`, reused unchanged |
| Qt 6.8.3 source | local `C:/Qt/6.8.3/Src/qtdeclarative` (tag `v6.8.3`) |
| Qt 5.15.2 source | `qt/qtdeclarative` tag `v5.15.2`, tag object `90b366ccabb4c7646a0e5f5d32f1b20eb066b48a`, commit `104eae5b17b0ec700391e9539ee3a4f638588194`; five files fetched by `contents` API into `tmp/qt515/` |

Evidence levels used below: **API** = documented Qt public guarantee,
**IMPL** = tagged private implementation detail, **OBS** = this device/run
observation. Qt IMPL facts are not stable API and must not become a HyRemote
dependency.

## Tagged Qt source facts

| # | Tag / commit | path | function / symbol | line | level | fact |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | v6.8.3 | `src/quick/items/qquickwindow.cpp` | `QQuickWindowPrivate::renderSceneGraph` | 694-699 | IMPL | `renderNextFrame(renderer)` then `emit q->afterRendering()` then `runAndClearJobs(&afterRenderingJobs)` then `endNextFrame(renderer)`. |
| 2 | v5.15.2 / 104eae5b | `src/quick/items/qquickwindow.cpp` | `QQuickWindowPrivate::renderSceneGraph` | 614-625 | IMPL | `renderNextRhiFrame`/`renderNextFrame`, `emit q->afterRendering()` at 619, then `endNextRhiFrame`/`endNextFrame` at 622-625. Same ordering as Qt 6. |
| 3 | v6.8.3 | `src/quick/items/qquickwindow.cpp` | `QQuickWindow::afterRendering` docs | 2887-2915 | API | Commands are added to the command buffer **but not yet submitted to the graphics queue**; render passes are already recorded; emitted from the **scene graph rendering thread**, direct connection required; "limited use in Qt 6". |
| 4 | v5.15.2 | `src/quick/items/qquickwindow.cpp` | `afterRendering` docs | 4489-4522 | API | Fn documented at 4489; 4514 warns the slot must leave the OpenGL state intact. |
| 5 | v6.8.3 | `src/quick/items/qquickwindow.cpp` | `QQuickWindow::beginExternalCommands` | 3391-3420 | API | For OpenGL/D3D11 (no native command buffer) this replaces Qt 5 `resetOpenGLState()`; **does not reset OpenGL state** to defaults; `\since 5.14`. `endExternalCommands` at 3457. |
| 6 | v5.15.2 | `src/quick/items/qquickwindow.cpp` | `beginExternalCommands` docs | 5126-5155 | API | Same replacement semantics, `\since 5.14`; `resetOpenGLState()` at 5030. |
| 7 | v6.8.3 | `src/quick/items/qquickwindow.cpp` | `QQuickWindow::frameSwapped` docs | 3017-3020 | API | Emitted after the scene graph **submitted** a frame; last signal of the render thread. This is not a physical display present. |
| 8 | v5.15.2 | `src/quick/items/qquickwindow.cpp` | `frameSwapped` docs | 3990 | API | Same meaning; `sceneGraphInvalidated` docs at 4021. |
| 9 | v6.8.3 | `src/quick/scenegraph/qsgrenderloop.cpp` | `QSGGuiThreadRenderLoop` render/swap | 728-731 | IMPL | `rhi->endFrame(swapchain, flags)` then `cd->fireFrameSwapped()` then `emit window->afterFrameEnd()`. |
| 10 | v6.8.3 | `src/quick/scenegraph/qsgthreadedrenderloop.cpp` | `QSGRenderThread::syncAndRender` | 771-782 | IMPL | `rhi->endFrame(cd->swapchain)` at 771, `d->fireFrameSwapped()` at 782. GUI-thread `polishAndSync` blocking is documented at 50-57 and implemented at 1545. |
| 11 | v5.15.2 | `src/quick/scenegraph/qsgthreadedrenderloop.cpp` | threaded render loop | 869-870 | IMPL | `gl->swapBuffers(window)` (no QRhi `endFrame`); `polishAndSync` at 1546. |
| 12 | v6.8.3 | `src/quick/items/qquickitemgrabresult.cpp` | `event` / `setup` / `render` | 215-270 | IMPL | `Event_Grab_Completed` posts `Q_EMIT ready()`; `setup` reads `effectiveDevicePixelRatio()` at 242; `render` runs in `afterRendering`, calls `texture->updateTexture()` and **`d->texture->toImage()`** at 262, then posts the completed event. This GPU-to-CPU copy runs on the Qt render thread. |
| 13 | v5.15.2 | `src/quick/items/qquickitemgrabresult.cpp` | `render` / `grabToImage` | 259-279, 338-340 | IMPL | Same `toImage()` + `postEvent` structure; `setSize(d->textureSize)` without DPR multiply at 267-268. |
| 14 | v6.8.3 | `src/quick/items/qquickitemgrabresult.cpp` | `QQuickItem::grabToImage` | 330-334 | API | Returns a shared result object; completion is asynchronous via `ready()`. |
| 15 | v6.8.3 | `src/quick/items/qquickwindow.cpp` | `QQuickWindow::maybeUpdate` | 2326 | IMPL | Request path; v5.15.2 equivalent at 3919. |
| 16 | v5.15.2 | `src/quick/items/qquickwindow.cpp` | context invalidation connect | 735 | IMPL | `context->invalidated()` is connected to `sceneGraphInvalidated()` with `Qt::DirectConnection`. |

Repository cross-check: `src/runtime/src/quick/quick_grab_geometry.hpp:8-13` already
records the tagged Qt5/Qt6 `grabToImage` DPR difference from the same upstream
files, so the geometry contract is consistent with the tagged source.

HyRemote intervention points (product source, read-only):
`src/runtime/src/quick/quick_target.cpp:619` issues `grabToImage(qtTargetSize)`
and 631-634 connects `QQuickItemGrabResult::ready` to the GUI dispatcher.
`src/runtime/src/quick/quick_gl_pbo_capture_source.cpp:581-584` connects
`QQuickWindow::afterRendering` directly; 1107-1111 wraps the native work in
`beginExternalCommands()`/`endExternalCommands()`; 1212-1215 issues
`glReadPixels` into a PBO plus `glFenceSync`; the bounded fence wait, PBO map and
single CPU copy run on the session worker (documented at 109-112). No Qt private
header, no forced render loop, no render-thread GPU wait is used.

## P0-A normalisation

Raw trace SHA-256 (schema=3, `(pid, requestId)` paired, Host monotonic ns):

| set / arm | sha256 |
| --- | --- |
| A `viewers-1-A1` | `d75a8e0ca30cc7bde910a0d70773b0f49ac5b92659bd37572da911dde042bfc6` |
| A `viewers-1-B` | `0806782288ea22ef318b63c1bdaa3811336a86cc24925fed09ac18e2c81fcc0d` |
| A `viewers-1-A2` | `a0b8b262ab06c1106196686cb984a9b1540908db4a21f7c59c34ea2488cdf9e7` |
| B `viewers-1-A1` | `d8239b1f5275e381d424101ae875a69ffb4c3936fcbbd5534cffa8b39363773c` |
| B `viewers-1-B` | `d1151bf09303c75a6891e77e67495c2dce057df64b4efeb3748e6789ce3fb673` |
| B `viewers-1-A2` | `3915c57bc4069f3afcebb2c70e1bae3d8931242f6bb30f2bd511bf6188d7349c` |

All six are distinct files. Set B is not a re-labelling of set A's 29 samples.

Environment identity (read-only, `measure_matrix.py:91-98`): both sets used the
same inspector script and the same explicit environment
`QSG_RHI_BACKEND=opengl`, `QSG_RENDER_LOOP=threaded`, `QSG_INFO=1`,
`QT_FORCE_STDERR_LOGGING=1`, `QT_SCALE_FACTOR=1`,
`HYREMOTE_RESEARCH_MATRIX=1`. Both `host.stderr.log` files report a threaded
render loop, an OpenGL RHI on the same UHD 770 device with the same pipeline
cache, `swapInterval 1`, MSAA 1, and animation driver vsync 16.67 ms. Set B adds
one `qt.qpa.screen: "Unable to open monitor interface to \\.\DISPLAY1"` warning
that set A does not have. Host PIDs differ (normal), QPC time bases differ by
about 4.5 hours, so these are different desktop sessions.

| metric (1 Viewer) | A A1 | A B | A A2 | B A1 | B B | B A2 |
| --- | --- | --- | --- | --- | --- | --- |
| Viewer detect n / missed sequences | 29 / 0 | 29 / 0 | 29 / 0 | 28 / 0 | 28 / 0 | 29 / 0 |
| Host PID | 25832 | 32468 | 30700 | 15384 | 14300 | 42228 |
| detect P50 / P95 / P99 (ms) | 137.4/255.5/270.9 | 98.0/261.2/277.8 | 157.0/274.1/274.7 | 214.1/322.0/333.7 | 211.0/372.4/401.6 | 178.4/334.2/336.2 |
| `local_marker_to_next_swap` P50 (ms) | 6.835 | 5.912 | 7.158 | 60.083 | 60.307 | 60.023 |
| same, P95 (ms) | 7.765 | 7.081 | 8.743 | 62.871 | 62.810 | 63.024 |
| `local_wm_key_to_qt` P50 / P95 (ms) | 0.341/4.316 | 0.294/4.566 | 0.327/2.377 | 1.995/31.606 | 1.263/4.644 | 2.457/33.288 |
| `local_frame_gap` P50 / P99 (ms) | 109.6/263.9 | 93.8/910.2 | 105.3/262.7 | 48.1/285.9 | 63.0/329.4 | 48.2/284.8 |
| `local_frame_swaps` | 276 | 231 | 279 | 281 | 263 | 285 |
| Host CPU one core (%) | 3.11 | 1.68 | 3.84 | 7.14 | 2.42 | 9.46 |
| Host RSS P50 (MB) | 132.0 | 86.3 | 87.7 | 87.9 | 86.4 | 87.4 |
| `capture_request` / submit / `source_completed` | 246/246/246 | 234/218/234 | 245/245/244 | 314/314/314 | 295/295/294 | 317/317/315 |
| `req→submit` P95 (ms) | 2.202 | 145.072 | 2.134 | 25.390 | 49.282 | 28.423 |
| `submit→source` P95 (ms) | 10.663 | 9.979 | 10.829 | 50.755 | 48.480 | 50.053 |
| `source→core_frame_queued` P95 (ms) | 0.036 | 0.039 | 0.030 | 0.303 | 0.290 | 0.400 |
| `rfb_socket_queued` | 37 | 37 | 37 | 37 | 36 | 37 |
| `duplicate_content_pts` | 0 | 15 | 0 | 0 | 71 | 0 |
| `physical_present` | NOT_MEASURED | NOT_MEASURED | NOT_MEASURED | NOT_MEASURED | NOT_MEASURED | NOT_MEASURED |
| Host GPU counters with Host PID | NOT_INSTRUMENTED (`host_gpu_counter_available=false`, n=0) | same | same | same | same | same |

## Qt source function -> existing probe -> observed symptom

| Qt stage (tagged source) | Existing probe | Observed symptom |
| --- | --- | --- |
| `polishAndSync` / `syncAndRender` (facts 10, 11, 15) | `capture_request` -> `quick_grab_submitted` / `gpu_readback_submitted` | Public arms 2.1-2.2 ms P95 in A, 25-28 ms in B; GL 145 ms in A (zombie fallback arm, fixed in #980), 49 ms in B. |
| `renderSceneGraph` afterRendering + `toImage()` (facts 1, 2, 12, 13) | `submit -> source_completed` | ~10 ms P95 in A for **all three arms**, ~48-51 ms in B for **all three arms**. The public arm uses Qt's own render-thread `toImage()`, so this is a Qt render-thread / GPU-completion cost, not a GL-route cost. |
| `rhi->endFrame` / `swapBuffers` -> `fireFrameSwapped` (facts 7-11) | `CHANGE` marker -> next `FRAME_SWAPPED` tick | A: 5.9-7.2 ms P50 with ~1 ms spread. B: 60.0-60.3 ms P50 with ~3 ms spread, all three arms. A near-constant 54 ms phase shift, not a random tail. |
| `QCoreApplication::postEvent(Event_Grab_Completed)` -> `ready()` (facts 12-14) | `source_completed -> core_frame_queued` | 0.03-0.04 ms in A, 0.29-0.40 ms in B. Ten times slower but still sub-millisecond; not the P95 driver. |
| `sceneGraphInvalidated` (fact 16) | none | NOT_INSTRUMENTED. Resource rebuild and worker/context retirement cannot be placed on the timeline. |

## NOT_INSTRUMENTED / NOT_VERIFIED / not claimed

- `NOT_INSTRUMENTED`: Qt internal polish / synchronize / render / `endFrame` timestamps, window expose and visibility state, occlusion, DPI and refresh-rate record, `sceneGraphInvalidated` events, worker/context rebuild, Viewer decode, physical display present.
- `NOT_VERIFIED`: any D3D11 staging path conclusion (separate code path, not exercised by these OpenGL traces); any Qt 5.15.2 device-level claim (only tagged source, no Qt5 run).
- `physical_present` stays `NOT_MEASURED`. `frameSwapped` (facts 7-8) is a frame-submission signal, not screen light-out. `rfb_socket_queued` is a socket write, not Viewer decode or present.
- Host monotonic ns is never subtracted from a Viewer QPC timestamp.
- Set A's 29 samples are not reused as set B's samples. No Host stage P95 is
  promoted to a Viewer P95 claim.

## Conclusion and gate for P0-B

The archives **locate** the remaining P95 cost: a common three-arm Host
regression in the Qt render-thread chain (GPU-to-CPU completion, request
admission) plus a stable ~54 ms `marker→swap` phase shift and higher public-arm
CPU. Because the public arms regress equally, this is not evidence of a GL-route
product regression.

The archives **cannot establish the cause**. Same script, same explicit QSG
environment, same reported loop/RHI/vsync; different desktop session, one new
QPA monitor-interface warning, and no visibility/occlusion/DPI/refresh-rate
instrumentation. `NOT_INSTRUMENTED` fields prevent attributing the 54 ms phase
shift to either the Host frame cadence or the Viewer detection chain. Under the
P0-B gate this is an insufficient archive, so a pre-registered minimal 1 Viewer
A/A is required; no product-code change is justified from these two sets alone.
