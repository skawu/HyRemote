# #967 P0-B A/A and P0-C reversed B/A result

Status: evidence only. No product-code change, no CTest suite, no 9x120 s
matrix, no Actions/workflow edit. `PRODUCT_GO=NO`, experimental GL default OFF.

Fixed identity: `develop@e2336c139eff78d6ed2b384eb1a61a49adcc2e8b`, clean main
work tree used as CMake `-S`, `git rev-parse HEAD` verified. Plans were published
on #967 before each run: P0-B comment `6841178760`, P0-C comment `6841252251`.

Binary identity (SHA-256), shared by every segment that used the given arm:

| binary | sha256 |
| --- | --- |
| OFF `HyRemoteRemoteAccess.dll` | `da305b8e11c18d9be224d2f055a728d662cb49147a9adfabddb3b70d0c581ad8` |
| ON `HyRemoteRemoteAccess.dll` | `109cea5fed3c0fbebd2ec8627f55ade3ac9c733357edc02859ebc59ac4d3efd7` |
| OFF `host.exe` | `f7af832e09b2df0e3986c5efafb846129f3eb524383d9442b1f98a2c8e6c70bd` |
| ON `host.exe` | `5c73a0decf8b858ff287853c172587a3263430320cd0c0a82ee5ea281f8b2d32` |
| `vncviewer.exe` | `39195c0abe53dc8fc2a57beeb6df9accf72226d46688845bf864c4d639030d84` |

Both DLLs were configured with the parameters recorded in
`docs/internal/evidence/962-realvnc-matrix/run_fixed_matrix.ps1:34-45`,
differing only in `HYREMOTE_EXPERIMENTAL_QUICK_GL_PBO`. `host.cpp` and the
library export surface are unchanged, so the two existing host executables were
reused and not relinked. `measure_matrix.py:287-290` self-checks that arm B
produces `gpu_readback_submitted` and no `quick_grab_submitted`.

## Environment pre-check and per-segment verification

Before segment 1: console session `maoxu` ID 1 Active, single screen
`\\.\DISPLAY1` 1920x1080 32 bpp, Viewer and Qt 6.8.3 Kit present, tracked work
tree clean. Win32 did not return a numeric refresh rate, so refresh rate stays
`NOT_INSTRUMENTED`; the Qt log reports animation-driver vsync 16.67 ms.

All five segments report threaded render loop, QRhi backend OpenGL, Intel UHD
Graphics 770, `swapInterval 1`, MSAA 1, seeded pipeline cache with 2 program
binaries, and the same explicit script environment (`QSG_RHI_BACKEND=opengl`,
`QSG_RENDER_LOOP=threaded`, `QT_SCALE_FACTOR=1`). No segment has the
`qt.qpa.screen: Unable to open monitor interface` warning seen in the older
`99e6fd1e` set. Every segment has `host_exit=0`, `stop_ok=true`, viewer n=29,
empty `viewer_missed_sequences`, and one normal Viewer window title. No stop
condition was triggered; no extra segment was added.

## P0-B: two consecutive A/A segments, public OFF, 35 s each

| metric | aa-1 | aa-2 | delta |
| --- | --- | --- | --- |
| raw trace sha256 | `6c0d4ab2031711e10e5c536028044b8d3cd58b2a05b442ee32463632d7784e40` | `414f965227a32377de91b045789f8dda79c56411dcfb158d66fde79d7213bc30` | distinct files |
| `marker→swap` P50 / P95 (ms) | 6.2987 / 7.6751 | 7.0268 / 8.5886 | 0.73 / 0.91 |
| `frame_gap` P50 (ms) | 103.41 | 104.59 | 1.18 |
| `frame_swaps` | 275 | 278 | 3 |
| `wm_key_to_qt` P50 (ms) | 0.339 | 0.273 | 0.066 |
| `capture_request→submit` P50 / P95 (ms) | 0.173 / 1.719 | 0.158 / 1.661 | 0.015 / 0.058 |
| `submit→source_completed` P50 / P95 (ms) | 7.956 / 10.170 | 7.964 / 9.925 | 0.008 / 0.245 |
| Viewer detect P50 / P95 / P99 (ms) | 150.11 / 266.99 / 284.58 | 137.26 / 269.64 / 269.67 | 12.85 / 2.65 / 14.92 |
| Host CPU one core (%) | 3.53 | 2.95 | 0.58 |
| Host RSS P50 (MB) | 149.1 | 89.6 | 59.5 |

A/A self-drift floor: Viewer detect P50 12.85 ms, P95 2.65 ms, P99 14.92 ms;
Host `marker→swap` P50 0.73 ms; Host `capture_request→submit` P95 0.058 ms.

Both segments reproduce the older `a43b76e1` baseline cadence (6-7 ms
`marker→swap`, 1.7 ms `req→submit` P95, 10 ms `submit→source` P95), not the
~60 ms cadence of the `99e6fd1e` set. That older set's three-arm Viewer P95 increase is **consistent with**
a non-reproducible display-session or measurement-environment drift; these
A/A runs do not prove its exact cause or exclude every product effect. RSS P50 differs by 59.5 MB, recorded as a start-up allocation OBS; it is
not used as a comparability failure because cadence, sample count, QSG identity
and Viewer detection all agree.

## P0-C: reversed short B/A, three segments, 35 s each, 1 Viewer

Order: `b-first` (GL ON, first position) -> `a-reverse` (public OFF) ->
`b-last` (GL ON, last position). Raw trace SHA-256: b-first
`f138bc55cfce9020eccddd71c59a7bd09eb4168d10bd21eb4f97937d89341ed0`, a-reverse
`1e54d012e7f89ab452ea5f2e2988002df77c2729ef6281e2c4b19ff35aa0118f`, b-last
`a1172fb25fc5976810e790e3d0aa9f3479f25e5523db9a1b93a95f1c87b09c99`.

| metric | b-first (B) | a-reverse (A) | b-last (B) |
| --- | --- | --- | --- |
| `marker→swap` P50 / P95 (ms) | 16.352 / 18.241 | 6.488 / 7.429 | 6.193 / 7.306 |
| `frame_gap` P50 (ms) | 32.392 | 108.691 | 79.600 |
| `frame_swaps` | 322 | 281 | 289 |
| `capture_request→submit` P50 / P95 (ms) | 14.066 / 21.045 | 0.175 / 1.867 | 6.740 / 9.495 |
| `submit→source_completed` P50 / P95 (ms) | 1.077 / 5.504 | 7.970 / 10.099 | 1.489 / 11.174 |
| `>100 ms` req→submit tails | 2 | 0 | 2 |
| missing submit | 0 | 0 | 0 |
| Viewer detect P50 / P95 / P99 (ms) | 145.74 / 304.20 / 310.33 | 151.66 / 268.64 / 269.43 | 119.90 / 236.08 / 252.35 |
| Host CPU one core (%) | 3.52 | 3.26 | 1.95 |

The public A segment matches both A/A segments (detect P95 268.64 vs 266.99 /
269.64; `marker→swap` 6.488 vs 6.299 / 7.027). The two B segments do not match
each other: detect P95 differs by 68.12 ms, P99 by 57.99 ms, `marker→swap` P50
by 10.16 ms, `frame_gap` P50 by 47.21 ms and `req→submit` P95 by 11.55 ms. All
of those exceed the A/A self-drift floor. Their end-to-end directions are also
opposite: b-first is 35.6 ms worse than public at P95, b-last is 32.6 ms
better. b-first additionally shows a 32 ms frame cadence with 322 swaps, which
neither the other B segment nor any A segment shows.

## CPU probe and RFB update accounting

Final `runtime-cpu.jsonl` sample counts, with trace event counts alongside:

| segment | `quick_gl_explicit_render` | `quick_gl_gpu_submit` | `quick_gl_render_callback` | `frame_swaps` | `capture_request` | `rfb_socket_queued` |
| --- | --- | --- | --- | --- | --- | --- |
| aa-1 (A) | 0 | 0 | 0 | 275 | 245 | 37 |
| aa-2 (A) | 0 | 0 | 0 | 278 | 247 | 37 |
| b-first (B) | 294 | 291 | 322 | 322 | 338 | 37 |
| a-reverse (A) | 0 | 0 | 0 | 281 | 251 | 37 |
| b-last (B) | 257 | 255 | 289 | 289 | 266 | 37 |

`quick_native_fallback` and `quick_stale_drop` are 0 in every B segment, so the
extra work did not trigger the `TemporarilyUnavailable` public-fallback path.
`render_callback` equals `frame_swaps` exactly in both B segments, confirming
that nearly every GL request drives one Qt render frame and one swap.

Every segment produced **37** RFB socket-queue events. The additional GL
capture work did **not increase counted socket updates**, consistent with
unchanged-content suppression by RFB compare; count equality alone does not
prove identical payload, delivery timing or Viewer presentation. Nor does it
exclude latency effects from extra Qt rendering or GPU work.

Falsifiable candidate H: GL forced updates drive extra Qt frames and therefore
the Viewer P95 tail. The five short segments do not establish this as a
reproducible explanation; nor do they exclude a scheduling effect. b-first has 37 more explicit
renders (13%) than b-last but is 68.12 ms worse at Viewer P95, while b-last is
32.6 ms better than public despite still doing 257 forced renders. The metric
that tracks Viewer P95 across all five segments is the `marker→swap` phase
(6.2-7.0 ms for aa-1/aa-2/a-reverse/b-last, 16.35 ms for b-first), which is a
display-session/Qt swap scheduling quantity and is `NOT_INSTRUMENTED` internally.
H is therefore not established, and the extra forced updates are recorded as an
efficiency OBS, not a product defect to fix.

## Verdict against the registered criteria

The P0-C criterion required B to behave consistently in two different order
positions and to exceed A/A self-drift before any difference can be attributed
to GL remaining cost. That criterion fails on the Viewer P95/P99, on
`marker→swap`, and on Host `req→submit`. Result: **`INCONCLUSIVE`**. No GL
remaining cost is attributed, no Viewer P95 claim is made, and `PRODUCT_GO`
stays NO.

A consistent Host-side observation is still recorded as an OBS, not a product
claim: both B segments have a larger `capture_request→submit` than public
(P95 21.045 / 9.495 ms vs 1.867 ms) and a smaller `submit→source_completed`
(P50 1.077 / 1.489 ms vs 7.970 ms). Tagged Qt source explains the shape: the
GL route can only submit a readback after `QQuickWindow::afterRendering`
(Qt6 `qquickwindow.cpp:694-699`, Qt5 `:614-625`), while the public route calls
`QQuickItem::grabToImage` directly and pays its `toImage()` cost inside the
render-thread callback (Qt6 `qquickitemgrabresult.cpp:249-270`, Qt5 `:259-279`).

## P0-D decision: no product-code change

No root cause is supported by both tagged Qt source and the evidence, so the
precondition for a code change is not met.

* The only stable product-side difference is the wait for a Qt render frame.
  Every allowed replacement is excluded by the P0-D constraints: another signal
  instead of `afterRendering`, polling or high-frequency `update()`, blocking
  the Qt render thread, forcing `QSG_RENDER_LOOP`, Qt private headers, or an
  architectural GL/D3D11 rewrite.
* The readback callback itself matches the Qt 6 contract. It wraps native work
  in `beginExternalCommands()` / `endExternalCommands()`
  (`quick_gl_pbo_capture_source.cpp:1107-1111`), which Qt6 documents at
  `qquickwindow.cpp:3391-3420` as the replacement for `resetOpenGLState()` that
  does **not** reset GL state. The code saves and restores `GL_PACK_ALIGNMENT`,
  `GL_PACK_ROW_LENGTH`, `GL_PACK_SKIP_ROWS`, `GL_PACK_SKIP_PIXELS` and the PACK
  buffer binding at `:1182-1188` and `:1275-1279`, which protects those explicitly handled pixel-pack states; the current
  evidence does **not** establish absence of all possible GL state leaks.
* The #980 arm lifecycle fix already keeps one forced update per render frame
  through the `updateKeepsArm` regression; b-last and all public segments show
  the normal 6-7 ms `marker→swap`, so no persistent high-frequency update
  pattern is evidenced. b-first's 32 ms cadence is a single non-reproduced
  observation and cannot justify a change.

Therefore this turn delivers evidence and a docs-only PR, not a product-code
PR, and does not start the 82-case suite or a 9x120 s matrix.

## Boundaries

`physical_present`, Viewer decode, and Host GPU counters without a Host PID are
`NOT_MEASURED`. DPI and refresh-rate records, window visibility/occlusion/expose,
`sceneGraphInvalidated`, Qt polish/synchronize/render/`endFrame` timestamps,
worker rebuilds and Viewer decode are `NOT_INSTRUMENTED`. D3D11 staging and any
Qt 5.15.2 device-level conclusion are `NOT_VERIFIED`. `frameSwapped` is a
frame-submission signal, not screen light-out; `rfb_socket_queued` is a socket
write, not Viewer present. Host monotonic ns is never subtracted from Viewer
QPC. Each segment has its own raw trace SHA-256 above; no sample set is reused
as another set.
