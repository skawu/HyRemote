# #967 P3 — one unchanged comparison ends the 60fps burst (single control point, evidence from archived traces)

Date: 2026-11-14
Base: `develop@23f80843f19d52b666f0934a25f6be9258fbc8dc` (#998 merged).
Authority: #967 comment 6867326715 ("首要 P0 = 单 Viewer 必需的公共 Qt Quick offscreen render + `toImage()` 读回；用已有 trace 识别单一可控机制；必要时检查 250ms 静态探针是否导致连发真实多余抓帧，但没有真实 Viewer 收敛证据时不得直接调 250ms").
No new measurement run was made. All numbers come from the archived fixed-SHA
`4eb45ec1` frame traces already used by the registered analyzers.

## 1. What the archived traces answer

Per-frame steps from `tmp/win962/matrix-4eb45ec/viewers-{1,2}-A1/runtime-frame-trace.jsonl`
(schema 3, whole-run), joined by `frame_id` between `rfb_compared` and
`rfb_encoded` (a compared frame absent from the encoded set is byte-identical):

| Trace | requests | compared | encoded (changed) | unchanged runs |
| --- | ---: | ---: | ---: | --- |
| 1 Viewer | 935 | 830 | 122 | **121 runs, all length 5–7** (31×5, 77×6, 13×7) = 708 unchanged grabs |
| 2 Viewers | — | 821 | 124 | 57×5, 48×6, 14×7, 1×18, 2×3, **1×1** — short runs are 3/821 ≈ 0.4 % |

Request-gap distribution (1 Viewer): p5=0 ms, p50=27 ms, p95=285 ms, max=301 ms;
536 gaps < 50 ms (60fps burst segments), 395 gaps ≥ 150 ms (static-probe
segments). Capture cadence ≈ 7 Hz while the served content only changed 122
times: **about 26 % of all captures (242/935 at 1 Viewer) are the 60fps burst
confirming stillness after each change**, each one a full Qt offscreen render
plus `toImage()` GPU→CPU copy — the exact dominant cost named by the Owner.

## 2. The single control point

`ViewerCaptureDemand::kConsecutiveUnchangedFrames` gates how many unchanged
worker comparisons are spent in the 60fps burst before returning to the
bounded static probe. It was 3; the traces show every 1-Viewer stillness
period already outlives the burst, so the two extra grabs confirmed nothing:

- A changed frame is always captured either way; shortening the burst only
  moves the earliest re-probe inside a period that is already static.
- The static discovery ceiling stays **250 ms** (`kStaticProbeInterval` keeps
  its #789 A/B/A evidence, 9.2 %→4.5 % one-core CPU, and is deliberately
  untouched — no value of that probe was changed without convergence evidence).
- Expected reduction: 2 grabs per stillness run ≈ 242/935 ≈ **26 % of all
  public grabs at 1 Viewer** (≈ 29 % of comparisons at 2 Viewers).
- Known bounded trade-off, stated honestly: the 2-Viewer trace contains one
  length-1 run (0.4 %); for that class of event the next change can be found
  up to ~234 ms later (250 ms ceiling instead of 16 ms). The 1-Viewer trace
  (#789's actual field shape) contains none.

## 3. Product change and invariants

One constant and its evidence comment in
`src/runtime/src/detail/viewer_capture_demand.hpp`
(`kConsecutiveUnchangedFrames` 3→1). No capture contract, no RFB wire, no
H2/damage semantics, no backend routing, no native admission (#995 stays
`Admitted/Transient/Unsupported + Reason`, native default OFF, public Qt
fallback untouched), no #996/#997 zero-viewer lifecycle change, no GUI-thread
comparison (the unchanged verdict still comes from the RFB worker).
Remote input, new viewers and backpressure escapes reset idle immediately and
keep their 60fps first-frame responsiveness.

The existing `viewer-capture-demand` CTest is parameterized over the constant
(`enterStaticGate`, Qt-activity and remote-input wake tests), so the new value
is exercised by the same assertions; no new test target, matrix or probe was
added.

## 4. Honest measurement boundary

`GRAB_REDUCTION = 26 % (1V archived trace, analysis only)`;
`HOST_CPU_DELTA = NOT_MEASURED` (no new same-SHA run; the 66 % `UNATTRIBUTED`
attribution is indirect, as the Owner noted);
`VIEWER_P95_DELTA = NOT_MEASURED`; `FIELD_* = NOT_MEASURED`. This PR claims a
provable reduction in the number of dominant-cost captures, not a measured CPU
percentage. CTest/CI results are correctness evidence only.

## 5. Single user candidate

- Freeze: exact PR head (reported on GitHub), target Windows x86_64 /
  Qt 6.8.3 `msvc2022_64`, Qt 5.15.2 covered by exact-head CI; install via the
  canonical `build.cmd build/install` + `build.yml` profile in the single
  workspace `build` tree; rollback = revert this PR and rebuild to
  `develop@23f80843`.
- Files: `src/runtime/src/detail/viewer_capture_demand.hpp` and this document.
- Applicable CI: one canonical `build.cmd test` (82 discovered) + exact-head
  PR CI (Windows Qt6/Qt5, Generic real RFB, GL-RHI real CaptureSource,
  review/topology gates).
- Required field acceptance (not run here, `FIELD_NOT_RUN`): same-SHA 1V/2V
  Host CPU with the CPU probe, RealVNC Viewer detect P95/P99, one authorized
  A→user-upgrade→B convergence on #789/#567/#790, disconnect RSS/PSS/threads,
  reconnect. Test Codebuddy B measures only the running program and never
  upgrades for the user; unverifiable runtime SHA is reported
  `RUNTIME_SHA=NOT_VERIFIED`.
- Release stays `HOLD`: `release/v0.3.5.0@b0680d5d` and the #876 five-file
  whitelist are untouched; develop merge is not release qualification (#95
  owns the gate; `PRODUCT_GO=NO` until exact-SHA field evidence).
