# #967 P1 — review disposition: default-route optimization WITHDRAWN

Date: 2026-11-13, revised after [PR #997 review 5891712978](https://github.com/skawu/HyRemote/pull/997#pullrequestreview-5891712978)
Base: `develop@0f5e8333c27556e25d8cbbec98b2bc543532d7ab` (#995 + #996 merged)
Original freeze SHA `14c5eb2b0efaa3e05b26685d9724e12d91791945` is **superseded** by this
disposition. Authorities: #967 comment 6852819210, #594 comment 6846405899.

## 1. Decision

```text
DEFAULT_ROUTE_OPTIMIZATION = NO_PRODUCT_PATCH_JUSTIFIED (WITHDRAWN)
PR #997 PRODUCT CHANGE      = initial zero-viewer SDK-owned resource release only
```

The "share byte-identical public Qt grab pixels across frames" implementation
(`quick_frame_reuse.hpp`, the `QuickFrameReuseCache` call in `quick_target.cpp`,
its assertions and every performance claim) has been **removed** from this PR.
`quick_target.cpp` and `test_quick_capture.cpp` are restored exactly to
`develop@0f5e8333`. No default-path pixel comparison, no new cache, no extra
SDK-owned frame reference remains.

## 2. Why the original performance premise was wrong

1. **The default path never encoded or sent identical full-screen frames.**
   `rfb_transport.cpp:654-675` compares each new served frame against the last
   actually observed frame; `rfb_delivery_state.hpp:445-545` returns empty
   damage for equal visible RGB, including the existing
   `previousPresentedFrameUnchanged` contiguous bulk `memcmp` fast path;
   `RfbWorker::trySendUpdate` (`rfb_transport.cpp:1934-1950`) cancels an empty
   incremental update **before any rectangle is encoded or written**. With the
   watermark OFF (the product default) there was no repeated whole-frame
   Raw/TRLE encode or TCP write to remove.
2. **The frozen Runtime design says the comparison belongs on the RFB worker.**
   `viewer_capture_demand.hpp:109-110` states that `noteFrameChange()` is fed
   by the worker's exact `changedRfbTiles()` result precisely to avoid "a
   duplicate 1920x1080 comparison in the Qt GUI capture path". The withdrawn
   implementation contradicted this invariant.
3. **It added GUI-thread work instead of deleting work.** The removed
   `QuickFrameReuseCache::publish()` ran inside `publishImage()`, i.e. in the Qt
   GUI grab-completion callback, and performed up to `1920*1080*4 = 8.29 MiB`
   of `memcmp` per accepted grab under its own mutex. The only saving was that
   the RFB worker could take a storage-identity fast path instead of its own
   existing bulk `memcmp`. Net effect: one whole-frame comparison **moved from
   the RFB worker to the GUI thread**, plus a mutex and one extra pinned frame.
   The GUI thread owns Qt rendering and input, so this is a responsiveness
   risk, not a demonstrated Host CPU reduction.
4. **A conditional watermark-ON benefit was never measured.** With a static
   server-side watermark the worker does recompose one full frame per accepted
   grab, so a correct cross-layer decision might help there. This is a
   different, conditional claim. It was not proven and is not claimed here.

## 3. Measured evidence is explicitly insufficient, not favourable

One short run of the **existing** 1080p public-grab probe
(`runPublicQuickProfile()` in `test_quick_capture.cpp`, evaluated by
`tools/perf/assess_quick_public_profile.py`) at the pre-withdrawal head
`030c031b` produced:

```text
PUBLIC_BASELINE_STABILITY = UNSTABLE_BASELINE
A1_A2_CPU_SPREAD_PERCENT  = 36.36   (gate: <= 15)
CASE public_static_a1     PROCESS_CPU_MS=406.250  REQUEST_P95_MS=21.841
CASE public_a_split_b_a   PROCESS_CPU_MS=593.750  REQUEST_P95_MS=21.117
CASE public_static_a2     PROCESS_CPU_MS=281.250  REQUEST_P95_MS=26.040
renderer = software, context = HOSTED_SYNTHETIC_NOT_DEVICE_OR_NATIVE_GAIN
```

Per the repository test-governance rule, a static A1/A2 drift above 15% is
`UNSTABLE`: no performance claim may be made, and no resampling until it
passes is allowed. The probe also has no Viewer, no RFB wire and no watermark
ON arm, so it cannot establish Host CPU, GUI responsiveness, wire bytes or
P95 business case. **No A/B number is used to justify the withdrawn code.**

## 4. The only product change left in PR #997

`ViewerCaptureDemand::start()` begins with `m_viewers = 0` and
`waitForCapture()` deliberately admits ONE bootstrap capture so RFB can finish
ServerInit for a first client without deadlock. `frameQueued(true)` used to
only record that geometry. A frame that no Viewer will ever consume can fill a
`CaptureSource` idle cache (GL PBO, D3D11 staging, composite), and because no
`>0 -> 0` transition ever happens, `setViewerCount()` never latches
`IdleRelease`. Those SDK-owned bytes stayed pinned for the whole run. This is a
real #996 invariant hole, independent of the withdrawn optimization.

Fix: `frameQueued()` latches the SAME one-shot `m_idleReleasePending` when a
usable clientless bootstrap frame is queued. `waitForCapture()` delivers it on
the capture scheduler thread; `ObservedCaptureSource::requestFrame()` then calls
`CaptureSource::releaseIdleResources()` for every backend. The transport has
already recorded the ServerInit geometry, so the first handshake is unchanged;
a Viewer arriving before the scheduler consumes the latch makes
`waitForCapture()` drop the stale edge; a later reconnect still gets fresh
pixels. No per-backend viewer counter, timer or Qt-version branch is added.

Assertions (existing CTest targets only): `test_viewer_capture_demand.cpp`
covers start -> bootstrap -> 0V idle, the 0->1->0 edge, late callback/reconnect
and unusable bootstrap geometry; `access_instance_notifications_test.cpp` now
asserts an end-to-end `idleCaptureReleases == 1` right after the clientless
bootstrap, then `== 2` / `== 3` across disconnect and reconnect. The previous
assertion "A listener that has never served a Viewer is not a 0-viewer edge"
encoded the defect and was deliberately replaced.

## 5. Corrected claims

- No claim that the default public Qt route removes repeated whole-frame
  encoding, transmission or CPU work. It does not.
- `#789_CPU = NOT_MEASURED`; no Host CPU improvement is claimed by this PR.
- `#567_VIEWER = NOT_MEASURED`; RFB/wire/H2/Raw/TRLE behaviour is untouched.
- `#790_RETAINED_BYTES`: source-level correctness fix for initial 0 viewers,
  proven by existing CTest assertions; no RSS/PSS field claim.
- Backend qualification is unchanged: Qt5 DirectGL / Qt6 desktop GL small-target
  real-GPU PASS (native default OFF), Windows D3D11 static-1V CPU/private-bytes
  gain with viewer latency `NOT_QUALIFIED`, Qt6 GLES/EGLFS, redirected FBO,
  high DPI and other RHI `NOT_QUALIFIED` on the public Qt fallback. No ARM64
  extrapolation, no automatic native promotion, no new capability table.
- `PRODUCT_GO = NO` for #789/#567/#790; PR merge / green CI is not field GO.

## 6. Verification

Existing CTest targets only, through the canonical `build.cmd test` entry with
bounded filters: `viewer-capture-demand` 1/1, `runtime-notifications` 1/1,
`hyremote-quick-capture` 3/3 (including the two DPR variants). Exact-head PR
CI on the existing Qt6/Qt5 lanes. No 9x120s matrix, no new synthetic suite, no
temporary probe program, no new executable script. No watermark-ON A/B was
invented to keep the optimization alive.
