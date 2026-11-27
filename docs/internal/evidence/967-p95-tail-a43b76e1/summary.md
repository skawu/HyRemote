# #967 P95 tail attribution on fixed SHA a43b76e1

Status: read-only attribution, no new measurement run. `PRODUCT_GO=NO`,
experimental GL route stays default OFF.

| Field | Value |
| --- | --- |
| Performance source | `a43b76e1d95821d47f2703db945351ea6cd19daf` (merge of #971) |
| Analysed develop | `fe758d9ef012d3248b8a60a1cb34f6384d3896f3`; `a43b76e1..HEAD -- src/` is empty |
| Trace set A | `tmp/win967/matrix-35s/viewers-<n>-<arm>/runtime-frame-trace.jsonl` (35 s short runs) |
| Trace set B | `tmp/win962/matrix-4eb45ec/viewers-<n>-<arm>/runtime-frame-trace.jsonl` (120 s baseline, comparison only) |
| Analyzer | `tools/perf/analyze_frame_trace.py` schema=3 plus `tools/perf/attribute_frame_stages.py`, reused without modification |
| Existing summary | `tmp/win967/matrix-35s-analysis/matrix-stages.{json,csv}` |
| Clock domain | Host monotonic nanoseconds, one process, paired by `(pid, requestId)` |

Viewer desktop-pixel detection in `result.json` is a separate end-to-end
observation. `rfb_socket_queued` is a socket write, not a Viewer decode or a
physical present. Both `Viewer_present` and Host GPU counters without a Host
PID remain `NOT_MEASURED`. No Host timestamp is subtracted from a Viewer
timestamp.

## Raw trace identity (SHA-256)

| arm | sha256 | bytes |
| --- | --- | --- |
| viewers-0-A1 | `e601125e7246a41098c95f45865e3c48a21926922274ccbae298027e0ff69cdd` | 3475 |
| viewers-0-A2 | `46f8787d0c7093041c45f71bd79597b9021104ac65a6fb423078fd55b80f3634` | 3475 |
| viewers-0-B | `97768efd3e30025322beb01f9ec616608f82ed1f7a60c09ce6cd26919346340d` | 3526 |
| viewers-1-A1 | `d75a8e0ca30cc7bde910a0d70773b0f49ac5b92659bd37572da911dde042bfc6` | 391262 |
| viewers-1-A2 | `a0b8b262ab06c1106196686cb984a9b1540908db4a21f7c59c34ea2488cdf9e7` | 389743 |
| viewers-1-B | `0806782288ea22ef318b63c1bdaa3811336a86cc24925fed09ac18e2c81fcc0d` | 354320 |
| viewers-2-A1 | `08f4da1fc2aaab44875eee9ea4d7d8daced3a3d29bbf4d08376d3ff67c1f14e1` | 408416 |
| viewers-2-A2 | `5a383663400e3a7fb3eb82bd9f3e7498de2a2ac27b7aafbaed66d6ce78541bc6` | 408936 |
| viewers-2-B | `6f0775fd05bf49858041170440fb4c752a2431f5fa7b875a81bcb18327c75e53` | 350152 |

Zero `frame_trace_overflow` events, zero request-identity collisions in all
nine arms.

## Instrumented Host stages

Only eight `frame_step` kinds exist in schema=3 traces: `capture_request`,
`quick_grab_submitted` (public arm) or `gpu_readback_submitted` (GL arm),
`source_completed`, `core_frame_queued`, `rfb_observed`, `rfb_compared`,
`rfb_encoded`, `rfb_socket_queued`.

The following are `NOT_INSTRUMENTED`: Qt `afterRendering` entry and exit,
forced-update arming and firing, window expose/visibility transitions, scene
graph invalidation and worker/context rebuild, fence retirement attempts,
Viewer decode, Viewer physical present, GPU engine counters bound to a Host
PID. No `qt_activity_hint` event exists in the 35 s set, so no nearest-activity
claim is made. Code-level inferences below are labelled as such and are
validated only by the state-machine regression and the fixed-SHA short
A/B, not by a fabricated trace event.

## Stage delays (ms, Host monotonic)

| arm | req->submit P50 / P95 | submit->source P50 / P95 | req->socket P50 / P95 | Viewer detect P50 / P95 / P99 |
| --- | --- | --- | --- | --- |
| 1-A1 public | 0.219 / 2.202 | 8.247 / 10.663 | 9.809 / 13.852 | 137.38 / 255.50 / 270.88 |
| 1-A2 public | 0.214 / 2.134 | 8.101 / 10.829 | 9.761 / 13.511 | 157.02 / 274.10 / 274.66 |
| 1-B GL | 6.787 / **145.072** | 1.273 / 9.979 | 9.585 / 303.05 | 98.02 / 261.21 / 277.77 |
| 2-A1 public | 0.215 / 2.063 | 8.141 / 10.363 | 10.173 / 16.021 | 178.90 / 281.08 / 282.00 |
| 2-A2 public | 0.224 / 2.194 | 8.111 / 10.648 | 10.291 / 15.858 | 149.82 / 282.29 / 284.99 |
| 2-B GL | 7.080 / **154.032** | 2.062 / 9.930 | 12.866 / 157.819 | 103.61 / 207.97 / 240.66 |

`source_completed -> core_frame_queued` is 0.018-0.043 ms P50/P95 in every
arm. Core dispatch and RFB compare/encode are not the tail. The GL arm's
`submit -> source_completed` is 1-2 ms P50, so the GPU readback itself is not
the tail. The tail sits in `capture_request -> gpu_readback_submitted`, i.e.
waiting for a Qt render opportunity.

Steady state is fast (6-7 ms). The median Viewer detect of GL B is better than
both public arms (98.02 / 103.61 ms), but P95 and P99 do not improve in the
one Viewer cell. `PRODUCT_GO` therefore stays NO.

## Tail samples

`capture_request -> gpu_readback_submitted` above 100 ms:

* 1 Viewer B: 14 of 234 requests, 16 further requests have no submit event.
* 2 Viewer B: 28 of 230 requests, 30 further requests have no submit event.
* public arms: none above 100 ms.

All tails come in adjacent pairs. Representative 1 Viewer B window
(milliseconds relative to first request):

```text
19791.5  source_completed 131, 132  (content_pts_ns = 0, empty terminal frames)
19791.5  capture_request 133
19808.6  capture_request 134
         ... no trace event of any kind for 198 ms ...
20006.7  gpu_readback_submitted 133 and 134 (same instant)
20007.6  source_completed 133 and 134 (identical content_pts_ns)
20007.7  capture_request 135
20014.3  gpu_readback_submitted 135 (6.6 ms)
```

The 2 Viewer B arm repeats this at roughly two-second intervals; submit times
line up with the Host `CHANGE` markers that trigger natural Qt frames. Host
`local_frame_gap_ms` P95 is 900.0 ms in that run. Duplicate
`content_pts_ns` counts are 15 (1 Viewer B) and 17 (2 Viewer B): one physical
readback covers two Core request identities. The 0 Viewer B arm has three
startup requests only and no steady-state readback.

CPU probe counters show `quick_gl_explicit_render` roughly 1:1 with
`quick_gl_gpu_submit` per second (3-7 per second) while 6-7 Core requests are
issued per second. Some requests therefore never obtained a forced update.

## Falsifiable hypotheses

**H1 (selected). Zombie fallback-render arm.** `requestFallbackRenderIfNoNormalFrame`
sets `fallbackRenderTimerArmed` and only `RenderCallbackGuard` clears it. When
the QTimer callback decides not to call `window->update()` because the queue
is empty (or GPU/CPU work is pending), the arm stays set without any Qt frame
being requested. Every later Core request then fails
`shouldArmFallbackRender()` and waits for the next natural Qt frame, up to
about one second on a static surface. Supporting evidence: fully silent tail
windows, paired requests submitted together by one natural frame, submit times
aligned with `CHANGE` markers, explicit-render count below request count,
zero events between the paired request and the natural frame. Falsified if a
short fixed-SHA A/B after the arm fix still shows an unchanged P95 tail.

**H2 (rejected). Core pacing.** If Core itself paused, requests would stop
arriving but already-admitted requests would still be submitted by an
independent Qt frame. The trace shows admitted requests with no submit event
and a completely silent window, so pacing alone cannot explain it.

**H3 (not supported as primary). Forced update issued but Qt ignored it.**
`quick_gl_render_callback` increments at least as often as
`quick_gl_explicit_render` in every CPU probe sample, so `update()` normally
produces a render callback. This remains a secondary risk for the short A/B;
it is not promoted to a conclusion without an instrumented callback.

**H4 (rejected as a fix, not as a cause). Same-generation request merging.**
One readback covering two request identities is visible in the trace, but
restoring the earlier merge behaviour is explicitly out of scope: it caused
`TemporarilyUnavailable` to trigger public fallback. The fix does not touch
request batching, does not wait on the Qt render thread, and does not increase
`update()` frequency on the path that actually requests a frame.

## Constraints on the fix

Qt 5.15.2 and Qt 6.8.3 share one Core and one Shared Runtime. The cross
platform public `grabToImage` fallback, 0 Viewer steady-state zero readback,
session worker reuse, old-frame freshness rules and default-OFF experimental
GL selection are unchanged. No Qt render-thread GPU wait and no high-frequency
`update()` burst are introduced.
