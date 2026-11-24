# #967 P3 — proposed 1-frame stillness burst RETRACTED (NO_PRODUCT_PATCH_JUSTIFIED)

Date: 2026-11-14. Supersedes the earlier "one unchanged comparison ends the
60fps burst" proposal in this same PR (#999) after Cloud Owner review
[pullrequestreview-5899685032](https://github.com/skawu/HyRemote/pull/999#pullrequestreview-5899685032).
Base: `develop@23f80843f19d52b666f0934a25f6be9258fbc8dc` (#998 merged).
`kConsecutiveUnchangedFrames` is back at the accepted value **3**; the 250ms
probe was never touched.

## 1. Corrected evidence accounting (review P2)

The archived fixed-SHA `4eb45ec1` traces remain useful as a *proxy*, with
corrections now stated in source, doc and PR:

- `rfb_compared` without `rfb_encoded` does **not** prove byte-identical
  pixels. `trySendUpdate()` can skip encoding while a Viewer request is
  outstanding, under `bytesToWrite()!=0` socket backpressure, or for empty
  eligible updates; `RfbWorker::noteFrameChange` is damage-based. Short
  unchanged runs are therefore a proxy, not a measured pixel verdict.
- Arithmetic corrected: 708/935 ≈ **75.7 %** of compared frames were
  non-encoded (not "26 %"); the modelled saving is 2 extra grabs per
  stillness run → 2×121/935 ≈ **25.9 % as an UPPER BOUND at 1 Viewer**
  (~29 % of comparisons at 2 Viewers), never a measured grab reduction, and
  never convertible into Host CPU or Viewer P95.
- The 1-Viewer trace is a **Windows 1 Hz color-change fixture**, not the
  "#789 field shape"; it cannot establish that brief stillness is absent in
  real Qt applications (review names the user RK3588 Generic/3D page).
  The 2-Viewer trace does contain real short runs (1×1, 2×3 of 821).

## 2. Strict worst-case latency of the proposed 1-frame burst (review P1)

Provable from the shipped gate source (`viewer_capture_demand.hpp`), no
measurement needed:

1. `noteFrameChange(false)` on the first unchanged comparison arms
   `m_nextStaticProbe = now + kStaticProbeInterval` (lines 141–144).
2. With a 1-frame burst, `waitForCapture()` fails the
   `m_identicalFrames < kConsecutiveUnchangedFrames` active check (line 292)
   and parks in `wait_until(m_nextStaticProbe)` (lines 296–306).
3. A host-local change right after that brief pause — with no remote input,
   no new viewer, no backpressure escape — is then discovered at the next
   250 ms probe: **discovery latency moves from ≤ ~16–48 ms (two more 16 ms
   burst opportunities under the accepted 3-frame burst) to ≤ 250 ms**, i.e.
   **~202–234 ms later**, while the modelled CPU benefit is an unmeasured
   upper bound (§1).

Against the accepted #410 wired-LAN gate (P95 = 206.1 ms, n=40), a single
short-pause event can add more than the whole P95 budget by itself, and the
event class is common in interactive Qt applications. Under #967's frozen
CPU + Viewer-freshness **joint** gate this does not qualify: benefit
unmeasured upper bound, cost structural. Verdict per the review's fallback:
**revert 3→1 on the original PR and report NO_PRODUCT_PATCH_JUSTIFIED**.

## 3. What remains in the PR

- `kConsecutiveUnchangedFrames` restored to 3; the header comment now carries
  the corrected proxy/upper-bound accounting and the retraction rationale, so
  the same proposal cannot be re-raised on the flawed arithmetic.
- One bounded extension of the **existing** `viewer-capture-demand` CTest
  (no new target, no matrix, no probe): while the burst is not exhausted, one
  unchanged comparison must NOT park the scheduler at the static probe —
  `waitForCapture()` must still return `Capture` at 16 ms granularity. This
  locks the latency side of the joint gate as behavior for any future burst
  reduction.
- `kStaticProbeInterval = 250 ms` untouched, keeping its #789 A/B/A evidence
  (9.2 % → 4.5 % one-core CPU, Windows).

## 4. Measurement boundary

`BURST_REDUCTION_PROPOSAL = RETRACTED`;
`HOST_CPU_DELTA = NOT_MEASURED`; `VIEWER_P95_DELTA = NOT_MEASURED`;
`FIELD_* = NOT_MEASURED`. The archived-trace run-length distribution stands as
proxy evidence only. #789/#567/#790 remain OPEN; release #876 and
`release/v0.3.5.0@b0680d5d` remain HOLD; `PRODUCT_GO = NO`.
