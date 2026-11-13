# #967 P1 implementation design freeze — one common Qt-backend architecture

Date: 2026-11-13
Base: `develop@0f5e8333c27556e25d8cbbec98b2bc543532d7ab` (#995 + #996 merged)
Authority: #967 comment 6852819210 (section 1) and #594 comment 6846405899 (Human route decision)
Scope: freeze the remaining design delta, then implement P1 product source. D0/D1 (#995)
and D2/D3 (#996) are completed, merged and deliberately NOT re-done.

## 1. Frozen invariants (unchanged by this increment)

One Qt-free Core + one Shared Runtime. All four integrations (C++/QML/Generic/QPA)
share `CaptureSource / RemoteFrame / FrameStorage / PTS / Damage` and the request
identity. Capability-driven `Admitted / Transient / Unsupported + Reason`
(`quick_capture_capability.hpp`) selects capture; capture -> encode -> per-viewer
pending merge -> TCP delivery; 0/1/2 viewer resource release; public Qt
`grabToImage()` fallback always available. There is no second runtime, no
per-SoC/per-Qt-version capture algorithm and no new capability table.

Backend qualification is stated per backend, never inferred from a different RHI:

| Backend / Kit | Native status | Product route in this increment |
| --- | --- | --- |
| Qt 5.15.2 DirectGL | small-target real GPU PASS, default OFF | public Qt fallback unchanged |
| Qt 6.8.3 desktop GL-RHI | small-target real GPU PASS, default OFF | public Qt fallback unchanged |
| Qt 6.8.3 D3D11 staging | Windows static-1V CPU/private-bytes gain, viewer latency `NOT_QUALIFIED` | public Qt fallback unchanged |
| Qt 6 GLES / EGLFS, redirected FBO, high DPI, other RHI | `NOT_QUALIFIED` | public Qt fallback; no native claim |

## 2. The single frozen design delta

(A) **Session-state and selection.** `AdaptiveQuickCaptureSource::State`
(`generation/surfaceEpoch/nativeEnabled/pending/failures`) plus the one
`evaluateQuickCaptureCapability()` table are the ONLY session/state owners.
Product selection is: public Qt baseline first; a native provider is preferred
only when its exact backend facts are proven; every failure falls back one-way
to public Qt. Experimental native switches remain default OFF. This increment
adds no backend, no new capability table and no "experiment fully on" route.

(B) **What is controllable in a whole-screen A -> B paint.** The controllable
Host cost is the work repeated on every accepted frame: whole-frame pixel
ownership, watermark recomposition, whole-frame visible-pixel comparison and
whole-frame Raw/TRLE encoding. Viewer decode / physical present remain
`NOT_MEASURED` and are never claimed as an RFB stage. Frozen rule: when the
source frame's visible RGBA bytes are **byte-for-byte equal** to the previously
published frame with the same geometry/format/alpha/DPR, publish a NEW
`RemoteFrame` (new request identity, PTS and completion time) that shares the
SAME immutable `FrameStorage`; otherwise take the unchanged existing path.
Consequences of that single rule, all already supported by existing code:

- `changedRfbTiles()` returns empty damage on `previous->storage == current.storage`;
- `WatermarkReuseCache` hits on the same source storage, so no 8 MiB allocate,
  no whole-frame memcpy and no QPainter recomposition;
- no RFB rectangle is selected, encoded or written, so no repeated full-screen
  bytes and no stale pending update is created;
- any real byte change still uses the #991/H2 served-frame snapshot, per-viewer
  accumulated/committed damage, `coalesceRfbDamageForPaint`, Raw/TRLE and the
  `bytesToWrite()==0` / `m_encodingSocket` backpressure unchanged;
- no already-written TCP byte is truncated, no FBU request is left unanswered
  by a new protocol behaviour (an empty incremental update was already legal).

(C) **#996 release invariant.** The one-slot equivalence cache is a new
SDK-owned pixel reference. It MUST clear on the same single zero-viewer edge via
`CaptureSource::releaseIdleResources()`, and on `start()`/`stop()`/generation
change. It never pins pixels after the last Viewer leaves; allocator high-water
is still not an SDK leak. No `malloc_trim`, no board-specific release.

(D) **Success conditions (CPU and latency are one gate).** Host CPU must not
rise; Viewer freshness must not degrade (reuse only on exact pixel equality);
no GPU fence wait or Qt render-thread blocking is added (the comparison is a
CPU read of an already completed Qt grab); 0-viewer SDK-owned bytes stay
released; RFB correctness (#991/H2 A->B/S->B regressions) stays intact. A CPU
win with worse Viewer P95 is a failure. Physical present and Host GPU counters
remain `NOT_MEASURED`.

## 3. Implementation boundary (one product PR)

- `src/runtime/src/quick/quick_frame_reuse.hpp` (new, Runtime-private, Qt-free):
  exact visible-pixel equivalence + one-slot cache with generation and release.
- `src/runtime/src/quick/quick_target.cpp`: the public Qt Quick `CaptureSource`
  (used by the default route AND by `AdaptiveQuickCaptureSource::m_public`,
  therefore by every Qt graphics backend) calls the cache and implements
  `releaseIdleResources()`.
- `src/runtime/tests/test_quick_capture.cpp`: assertions inside the existing
  `quick-capture` CTest target only. No new test target, matrix or probe.

`rfb_transport.cpp`, `rfb_delivery_state.hpp`, `rfb_trle.hpp`,
`watermark_*`, `composite_target.cpp` and all native providers are NOT changed.
This is not a second #567 "old frame pretending to be new" fix: the wire is
byte-identical for equal pixels, and unchanged for changed pixels.

## 4. Verification and falsification (no new research loop)

Existing CTest assertions (equal pixels reuse / one-byte change does not reuse /
geometry change does not reuse / padded stride / 0V release / generation),
one canonical `build.cmd test`, exact-head PR CI on the existing Qt6/Qt5 lanes,
and source review of the unchanged H2/Raw/TRLE path. No 9x120s matrix, no new
synthetic suite, no temporary probe program. Falsifiers: a byte change that is
not delivered, a decorated frame that loses its watermark, or a 0-viewer cache
that retains bytes. Missing real Viewer/device evidence is reported as
`NOT_MEASURED` / `NOT_RUN_OWNER_DEPENDENCY`, never as PASS.

## 5. Product behaviour decision

No major product behaviour change: visible pixels, wire bytes, default route,
native switches and release semantics are unchanged. No Human decision is
required for this increment; Human is only involved for release scope or a real
customer-site authorization.
