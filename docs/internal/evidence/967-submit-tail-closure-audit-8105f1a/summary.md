# #967 P3 — native req→submit tail closure audit on develop@8105f1a

Date: 2026-11-14. Authority: #967 comment 6868318641 ("用可证伪的一个剩余真实
状态竞争/请求陈旧机制识别当前 req→submit 尾部（未能在当前源码复现则如实标注
历史疑点）……若没有新可控缺陷，则写 NO_PATCH_WITH_EVIDENCE 并直接转入候选
联合验收"). No new measurement in this audit; one bounded same-SHA A/B is run
separately (see §5). Baseline `develop@8105f1a0067865672747e2b3d5728bb87b98e063`
(#999 merged). The H1 zombie-fallback-arm fix (#980) is NOT re-litigated.

## 1. Arm lifecycle closure audit (all source lines from `quick_gl_pbo_capture_source.cpp` @8105f1a)

Every path that can leave an admitted request without a forced update was
checked for a driver that releases it without a natural Qt frame:

| # | Path | Release driver | Closed? |
| --- | --- | --- | --- |
| 1 | `evaluateFallbackRender` refuses because `gpuPending` (timer callback, lines 814–833) → arm cleared, no update | `onRetireResult` tail posts `requestFallbackRenderIfNoNormalFrame` again (retire-completion re-arm) | YES |
| 2 | `evaluateFallbackRender` refuses because `cpuConversionPending` | `finishCpuConversion` (lines 1055–1088) runs **synchronously on the retire worker thread** and clears the flag before the re-arm in the same `onRetireResult` call | YES |
| 3 | Request arrives while `kickPosted` is true (capture() line 704–706) | The already-posted dispatcher task re-runs `requestFallbackRenderIfNoNormalFrame` and arms for the new queue (line 746) | YES |
| 4 | GUI dispatch fails | Every accepted-but-unscheduled ID gets a terminal `reportUnavailable` (lines 748–769); Core retries | YES |
| 5 | Window hidden at dispatch time | Terminal failure for every queued ID (lines 730–744) | YES |
| 6 | 750 ms terminal-outcome timer | Expires only that request's ID; other IDs and later requests keep their own timers (lines 684–703) | YES |
| 7 | Render callback capability verdict | `Unsupported` → public fallback with terminal IDs; `Transient` (null context) → keep queue, next frame retries | YES |

## 2. The single path that does not self-heal (and why it is not a product defect)

After `evaluateFallbackRender` returns true, the code calls
`window->update()` and deliberately keeps `fallbackRenderTimerArmed` until the
next render callback (`RenderCallbackGuard`, lines 505–516). Qt only renders
an **exposed** window: `QQuickWindow::update()` → `QSGThreadedRenderLoop::
update` → `polishAndSync`, which aborts with `"- not exposed, abort"`
(qtdeclarative `qsgthreadedrenderloop.cpp`, Qt 6.8.3 local source
`C:\Qt\6.8.3\Src\qtdeclarative` — source-verified). `fallbackWindowState`
classifies by `isVisible()` only (lines 800–809), so a **visible but
non-exposed** window (minimized on Windows: `isVisible()==true`,
`exposed==false`) passes the Usable check, `update()` is swallowed by the
loop, no render callback ever clears the arm, and the 750 ms terminal timer
expires request IDs **without clearing the arm**, so
`shouldArmFallbackRender` (line 790) stays false until one natural frame.

Impact analysis — this window degrades to the same behavior as the public
route, not to a native-only hang:

- While not exposed, **no** capture route can render (public `grabToImage`
  needs the same frame); the 750 ms terminal outcome
  (`TemporarilyUnavailable`, `recoverable=true`) is the designed backstop and
  Core falls back / retries.
- The moment exposure returns, Qt's expose handling renders
  (`polishAndSync(inExpose=true)`), `RenderCallbackGuard` clears the arm, and
  the gate self-heals before the next Core request.
- No request is lost, no GPU resource leaks, no 0V/release interaction
  (terminal expiry already calls `maybeQueueIdleRelease`, line 698).

Adding an arm-self-heal timer here would change nothing user-visible while
the window stays non-exposed (requests still end terminal at 750 ms) — so
per the review constraint "只在具备明确代码证据……实施最小修复", no patch is
warranted for this path. It is documented, not fixed.

## 3. Why the archived 145/154 ms tail is a historical suspicion, not a current defect

The archived trace set (`967-p95-tail-a43b76e1`) predates the H1 arm fix. Its
own falsification clause — "Falsified if a short fixed-SHA A/B after the arm
fix still shows an unchanged P95 tail" — is the remaining open item. Under
the audited closure table, the original 17 ms GL fixed wait has already
been removed (`glFallbackRenderDelayMs = 0`). There is **no deterministic
<50 ms bound** on request→submit: Qt exposure, render-loop scheduling,
in-flight GPU/CPU work and queueing affect this interval. The 750 ms
terminal timer bounds request liveness, not a GPU-submit percentile.
Silent-tail windows and paired submissions are compatible with the fixed H1
arm fault, but not uniquely diagnostic of it. Verdict: **NO_PATCH_WITH_EVIDENCE**
for this audit's specific source paths; the old 145/154 ms sample remains
historical evidence until a truly matched workload can evaluate it.

## 4. Qt version coverage

- Qt 6.8.3: `"- not exposed, abort"` verified in local exact-version source.
- Qt 5.15.2: the same abort exists in `qt/qtdeclarative`
  `qsgthreadedrenderloop.cpp` on the main line; the v5.15.2 tag line was not
  diffed line-by-line locally (no 5.15.2 Src installed) — marked
  `NOT_VERIFIED_ON_5_15_2_TAG`, and immaterial: this path receives no code
  change either way.

## 5. Bounded same-SHA A/B result (run 2026-11-14, one bounded pair)

Setup per the Owner comment: current SHA `8105f1a`, Windows Qt 6.8.3, 1 Viewer,
35 s `local` fixture scenario (250 ms marker-phase default, distinct from the older 1 Hz RealVNC workload), `QSG_RHI_BACKEND=opengl`,
registered instruments only (`win949_probe.py` measure mode + registered
analyzers). Arm A = public route (canonical `build.cmd install` identity);
arm B = `HYREMOTE_EXPERIMENTAL_QUICK_GL_PBO=ON` measurement build in a
temporary fixed-SHA tree under `tmp/` (probe-confirmed route
`GL_RHI_PBO_SOURCE`, `pixel_truth=true`, 120/120 markers). Arm B's first run
was instrument-invalid (probe macros absent in the measurement configure) and
was rerun once as B2 with `-DHYREMOTE_ENABLE_CPU_PROBE=ON`; only B2 carries
probe/trace data. `PYTHONIOENCODING=utf-8` needed for the analyzer output
(console-only; scripts unchanged).

Stage attribution (`attribute_frame_stages.py`, schema=3, 24 s effective
window, whole-run traces):

| stage (ms) | A public | B GL PBO |
| --- | ---: | ---: |
| capture requests | 694 | 1047 |
| req→submit P50 / **P95** | 47.9 / 84.4 | 12.9 / **48.4** |
| submit→source P50 / P95 | 46.6 / 83.7 | 33.3 / 62.6 |
| req→socket P50 / P95 | 127.6 / 169.8 | 72.8 / 111.5 |

Host CPU (`aggregate_cpu_probe.py`, normalized 24 s window):
B1(A) = **21.92 %** of one core (process CPU 5.30 s, `UNATTRIBUTED` 5.19 s =
Qt render/driver), B2(B) = **13.13 %** (3.16 s).

Probe-client delivery latency (`latency_upper`, real RFB 3.8 poll client, NOT
RealVNC):
A = P50 106.5 / P95 122.7 / P99 141.1; B2 = P50 103.2 / **P95 176.5 / P99
209.8**.

Verdict against the Owner's joint criterion:

- `REQ_TO_SUBMIT_P95 = 48.4 ms` on the current source in this **250 ms-phase
  local fixture**: the archived 145/154 ms tail was **not observed under these
  conditions**, consistent with the already-fixed H1 arm issue. This is not a
  same-scenario RealVNC replication, and neither a universal <50 ms bound
  nor the global absence of the historical tail is established.
- `HOST_CPU_DELTA` improves (**21.9 % → 13.1 %**, −40 %) **within this one
  local A/B pair**; the magnitude is not a stable RealVNC/customer estimate.
- `VIEWER_P95/P99` **regresses** in the probe-client caliber
  (P95 +53.8 ms, P99 +68.7 ms); the B arm issues ~1.5× capture requests, so
  its delivery pacing differs. The archived RealVNC caliber showed the same
  "CPU better, P95 not better" shape at a43b76e1. RealVNC was not rerun this
  round (`FIELD_VIEWER_REALVNC_SAME_SHA = NOT_MEASURED`).

The Owner's gate ("CPU drops AND Viewer P95/P99 do not regress") is therefore
**not met for default-ON**: `PRODUCT_GO=NO` stands, the native route stays
default OFF behind `HYREMOTE_EXPERIMENTAL_QUICK_GL_PBO`, and the remaining
Viewer-tail component (request pacing vs delivery) is recorded as a known
open item — no new controllable defect was found in the submit/retire path
this audit owns. Next step per the Owner comment: joint candidate
acceptance of `develop@8105f1a` (#998/#999 included), with #789/#567/#790
remaining OPEN.
