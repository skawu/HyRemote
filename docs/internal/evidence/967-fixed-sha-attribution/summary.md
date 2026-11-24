# #967 Host-stage attribution of the fixed-SHA RealVNC matrix

Source under test: `4eb45ec13ceb907966e4619f4e10eec9b6ff468d` (PR #963 merge).
The nine 120 s RealVNC A1/B/A2 runs are the #962 matrix archived in
`docs/internal/evidence/962-realvnc-matrix-4eb45ec/`; **no new long run was made
for this attribution**. Their per-run `runtime-frame-trace.jsonl` files were kept
locally in `tmp/win962/matrix-4eb45ec/viewers-<n>-<arm>/` and are hashed below.

## Reproduction

```powershell
python tools/perf/attribute_frame_stages.py --self-test
python tools/perf/attribute_frame_stages.py --matrix tmp/win962/matrix-4eb45ec --out <run-output-dir>
```

`attribute_frame_stages.py` reuses `tools/perf/analyze_frame_trace.py`'s schema=3
`(pid, requestId)` pairing and percentile logic; it only adds the request→submit
split, duplicate content-PTS counting, per-viewer socket fork and raw hashes.
Outputs: `matrix-stages.csv` (committed), `raw/matrix-stages.json` (full official
intervals + extra split for all nine cells), `raw/stages-compact.csv` (the compact
P50/P95/P99 table used while diagnosing).

All intervals are **single-process Host monotonic nanoseconds**. The Viewer
desktop-pixel detection from `result.json` is an independent QPC observation and
is never subtracted from them. `rfb_socket_queued` is not a Viewer present; Qt
activity hints are non-causal nearest samples. `physical_present = NOT_MEASURED`,
Host GPU utilization `NOT_MEASURED` (the local `typeperf` GPU Engine wildcard
returned no counter for the Host PID; reported as unavailable, never 0).

## Stage latency (ms, P50 / P95)

| Cell | req→submit | submit→source | req→source | core→RFB | compare→encode | req→socket |
|---|---|---|---|---|---|---|
| 0V A1 (n=3) | 0.61 / 49.18 | 7.44 / 75.79 | 56.61 / 74.42 | 0.19 / 0.23 | – | – |
| 0V B (n=3) | 367.05 / 392.70 | 4.84 / 4.86 | 371.90 / 394.97 | 0.73 / 0.92 | – | – |
| 0V A2 (n=3) | 0.58 / 78.45 | 7.20 / 94.95 | 85.64 / 94.46 | 0.36 / 0.50 | – | – |
| 1V A1 (n=935/122) | 1.86 / 6.51 | 17.81 / 27.11 | 20.51 / 29.47 | 0.57 / 2.86 | 0.93 / 2.66 | 23.90 / 30.61 |
| 1V B (n=1176/122) | **41.55 / 52.73** | **1.12 / 3.46** | **43.22 / 54.73** | 0.16 / 2.30 | 0.76 / 2.66 | **52.23 / 59.04** |
| 1V A2 (n=936/122) | 1.69 / 5.52 | 17.18 / 24.67 | 19.95 / 27.07 | 0.56 / 2.61 | 0.85 / 2.37 | 23.49 / 29.65 |
| 2V A1 (n=933/244) | 1.70 / 3.56 | 17.27 / 21.60 | 19.64 / 23.60 | 0.51 / 1.98 | 1.46 / 3.68 | 22.98 / 27.93 |
| 2V B (n=1078/244) | **42.59 / 51.38** | **0.98 / 2.69** | **43.77 / 52.40** | 0.12 / 1.71 | 1.26 / 3.34 | **52.02 / 58.15** |
| 2V A2 (n=821/244) | 0.26 / 2.08 | 8.37 / 14.79 | 8.78 / 18.56 | 0.43 / 1.62 | 1.03 / 2.39 | 10.52 / 25.79 |

P99 and sample counts for every metric are in `raw/matrix-stages.json`. B's
`render_pts→source_completed` P50 is ~1.0 ms and `render_pts→socket_queued` P50
is 2.31 / 2.61 ms. Two-Viewer socket fork P50 is 0.76–0.82 ms in all arms.

Overflow events: 0; request-identity collisions: 0; negative/out-of-order pairs:
0 in all nine traces. Duplicate `content_pts_ns` requests (several Core requests
completed from one glReadPixels): **0 in every public arm**, **393/1176 (1V B)**
and **342/1078 (2V B)**. Encoded frames are 122 (1V) / 244 (2V) in all arms.

## Falsifiable hypotheses

1. *GPU fence/PBO map dominates.* **Refuted**: `submit→source` is 1.12/0.98 ms
   P50 in B; the readback itself is far cheaper than public grab's 17–18 ms.
2. *RFB/encode/2-Viewer service dominates.* **Refuted**: B's `core→RFB`
   0.12–0.16 ms, `compare→encode` 0.76–1.26 ms, `encode→socket` ~0.01 ms,
   2-Viewer fork <1 ms; identical encoded frame counts.
3. *Requests wait for an extra Qt frame, and stale requests share one content.*
   **Confirmed**: B's `req→submit` is 41.6/42.6 ms because
   `quick_gl_pbo_capture_source.cpp` waits
   `QuickReadbackTiming::normalFrameOpportunityMs = 17` then calls
   `window->update()` and only submits in `afterRendering`; A's public route
   submits in 1.7–1.9 ms. The duplicate content-PTS counts above show the
   queueing cost. The extra 28–41 ms on `req→socket` matches the direction of
   B's 8–24 ms worse Viewer-detection P50 and worse P95/P99.

0 Viewer: all three arms have zero steady-state capture. B's three startup
requests cost `req→source` 371.9 ms (GL session init) versus 56.6/85.6 ms for
the public arms — not "absolute zero GPU work".

## Raw trace SHA-256 (files retained locally, not committed)

| Cell | SHA-256 |
|---|---|
| viewers-0-A1 | `28acf05cfebd186d94aebca5853e9e9550978728a80ba98e0d9a011659e42409` |
| viewers-0-B | `9ef4a4db34d68d0642b15790cb7324b19603a72c894c6a39947ea794432fbd4c` |
| viewers-0-A2 | `41ae501856bff543ae6cad624e2b6687321d93194853a3a7620aba2b380df68a` |
| viewers-1-A1 | `505d42fd0c7516daefd159b61d2a37e1cc97b7ade95014ad44f33def7279589b` |
| viewers-1-B | `d8cf4cb8b61464c0685961d2ea5d4d8dd229d19d9df25e0b4a669291dbc38bf4` |
| viewers-1-A2 | `0e2b016c8ccc3051aceec17dfe002f6cfe59548610a958b0b072bea39616120a` |
| viewers-2-A1 | `7070e63e7035a60f691c103d959d82ef7c9f59013e1d4f78e066b8164ab8d023` |
| viewers-2-B | `5d88c4d4a78880465a4ded01a25ff0aa0e92b0dcfeafdedb2488e47101e25151` |
| viewers-2-A2 | `ca8867b7c3f39a5b7617f26a8d8f80199d4e6bf82d61db089f699de1357f0dc0` |

## Verdict of stage A

`RESULT=PASS_SCOPED` for the attribution: the controllable bottleneck is the
request-to-render-callback wait plus stale in-flight requests, not the GPU
readback or the RFB/Viewer path. A single ordered A1→B→A2 pass cannot separate
the 13–16 ms same-build A1/A2 drift from a real B penalty, so no product latency
claim follows from this table alone. `PRODUCT_GO=NO` until counter-evidence.

## B. Minimal fix that survived the full suite

An initial patch also coalesced multiple unsubmitted Core requests into the
newest one. The canonical 82-test run then failed
`hyremote-runtime-dll-lifecycle-test` with
`SDK_DLL sampled=0 ... gl_resources_drained=0`: the
`TemporarilyUnavailable`/empty-frame terminal outcomes of revoked native
requests are counted as native failures by
`quick_adaptive_capture_source.hpp`, which forced the public fallback and
removed every GL/PBO resource. That coalescing was withdrawn rather than
working around the adaptive contract or changing a test threshold.

The retained patch changes only the GL fallback-render policy:

- `QuickReadbackTiming::glFallbackRenderDelayMs = 0` is used by
  `quick_gl_pbo_capture_source.cpp`; the D3D11 staging route keeps
  `normalFrameOpportunityMs = 17`. No fixed 17ms wait before the bounded
  `window->update()` on a static surface.
- `State::fallbackRenderTimerArmed` is released by `RenderCallbackGuard` when
  a real Qt render callback occurs, not by the QTimer callback. One forced
  update per render generation; a natural frame still wins. No new catch-up
  or retirement frames, no spin, no change to the 250ms static probe, default
  OFF and the public Qt fallback remain intact.
- `testQuickGlPboFallbackPolicy()` is a GPU-window-free state-machine
  regression. With the policy reverted it fails three CHECKs
  (`armReleasedByRender`, `glFallbackDelayMs == 0`, `ok`); with the fix it
  passes. The initially failing DLL lifecycle case passes after withdrawal.

Duplicate content-PTS requests (393/1176 and 342/1078 in B) are therefore
**not** fixed by this patch; removing them needs a Core-level freshness or
backpressure change that does not consume native failure credits. It is left
as a follow-up, explicitly not claimed as solved.
