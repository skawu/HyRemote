# #967 P2 — root cause from existing evidence, product patch and single user candidate

Date: 2026-11-13
Base: `develop@b221eaf64a3496fc2f1f50b3ee349755d5db5949` (#995/#996/#997 merged)
Authority: #967 comment 6855340624; Human route decision #594 comment 6846405899.
**No new measurement run was made for this increment.** Every number below comes
from already archived fixed-SHA traces and the registered CPU-probe analyzer.

## 1. ROOT_CAUSE_WITH_EVIDENCE

Reused, unmodified: `tools/perf/aggregate_cpu_probe.py` over
`tmp/win962/matrix-4eb45ec/viewers-<n>-<arm>/runtime-cpu.jsonl` (source SHA
`4eb45ec13ceb907966e4619f4e10eec9b6ff468d`, 10 s warm-up + 40 s window), plus
`docs/internal/evidence/962-realvnc-matrix-4eb45ec/summary.md`,
`967-fixed-sha-attribution/summary.md`,
`967-counter-evidence-a43b76e1/summary.md`, `967-p95-tail-a43b76e1/summary.md`
and `967-qt-source-p0a/summary.md`. Analytical output retained in `tmp/`.

| Existing 40 s window | public grabs / batches | rfb_compare | rfb_encode | encoded bytes | UNATTRIBUTED CPU |
| --- | ---: | ---: | ---: | ---: | ---: |
| public Qt, 1 Viewer A1 | 279 | 280 | 41 | 15,114,896 | 2.6719 s |
| public Qt, 2 Viewers A1 | 270 | 270 | **82** | **30,229,792** | 1.9531 s |
| native GL, 1 Viewer B | 0 | 293 | 41 | 15,114,896 | 1.7812 s |

- **The dominant, unavoidable cost of the default public route is Qt's own
  offscreen render + full-frame `toImage()` GPU→CPU copy.** Tagged Qt 6.8.3
  source (`qquickitemgrabresult.cpp:262`, recorded in `967-qt-source-p0a`) runs
  `texture->toImage()` on the Qt render thread. HyRemote's own instrumented
  stages (`quick_convert`, `quick_buffer`, `rfb_compare`, `rfb_encode`) together
  account for well under 0.1 s of the 2.67 s process CPU; the remaining
  `UNATTRIBUTED` ~66 % is Qt render/driver threads. `req→submit` P50 is only
  0.21 ms while `submit→source` is 8.1–8.2 ms in the 35 s public traces.
- **A provable duplicate does exist in the encoder path:** adding one Viewer
  leaves the capture count at ~270–279 but exactly doubles encode calls
  (41→82) and encoded bytes (15,114,896→30,229,792). Each extra Viewer
  re-runs the whole-frame pixel conversion / TRLE palette work and allocates a
  second multi-MiB `QByteArray` for the SAME served frame. This is on the RFB
  worker, not the Qt GUI thread.
- **The native readback route really does remove the dominant cost** (public
  grabs 279→0, `UNATTRIBUTED` 2.6719→1.7812 s, Host CPU 1.68 % vs 3.11/3.84 %
  of one core, Viewer detect P50 98 ms vs 137/157 ms in the 35 s
  counter-evidence). It still does **not** earn a default: `req→submit` P95 is
  145/154 ms, 1-Viewer Viewer P95 261 ms vs public 255 ms, P99 does not
  improve, and `PRODUCT_GO=NO` in both archived reports. Host GPU counters and
  physical present are `NOT_MEASURED`.

## 2. PRODUCT_PATCH_PR / NO_PATCH_WITH_REASON

Implemented (two deletions of provable duplicate work, both on non-GUI
threads, both common to every Qt backend and all four integrations):

1. `rfb_transport.cpp`: `SharedFramebufferUpdate` holds ONE encoded
   FramebufferUpdate for one served frame. A later Viewer reuses it only when
   every wire-byte input matches: source `FrameStorage` (strong reference, so a
   recycled address cannot serve stale pixels), full `PixelSpec`, Raw/TRLE
   selection, geometry/resize, pixel format, alpha mode and the exact damage
   rectangle list. Each Viewer still writes its own identical bytes to its own
   socket, still commits its own accumulated damage and still gets its own FBU
   trace. **No TCP byte is truncated, no pending update is dropped, H2
   served-frame snapshot / per-viewer committed damage / `bytesToWrite()==0`
   backpressure are untouched.** The cache is enabled ONLY with >=2 Normal
   Viewers, is reset at every `frameAvailable()`, zero-viewer edge,
   `failDecoration()` and `shutdown()`, so a single-Viewer or idle Runtime never
   retains an extra encoded framebuffer (#790/#996/#997 invariant).
2. `composite_target.cpp`: `transparentFrame()` already zeroes the exact output
   buffer; the multi-surface path then called `output.fill(Qt::transparent)` on
   the same buffer, writing one whole 1080p canvas twice before the first
   `drawImage()`. The second write is removed. Premultiplied transparent is all
   bytes zero, so SourceOver output is byte-identical.

`NO_PATCH_WITH_REASON` for the single-Viewer default capture cost: reducing
`grabToImage()` frequency would change the frozen 250 ms freshness probe and
needs a new real-Viewer interleaved study (explicitly out of scope); promoting
the native provider to default is refused by the archived P95/P99 tails and by
"do not extrapolate the D3D11 single-point gain to GLES/EGLFS". No benchmark
was invented to keep a patch alive, and no full-frame comparison was moved to
the Qt GUI thread.

## 3. CPU_AND_VIEWER_FRESHNESS (honest boundaries)

- Deleted work is proven by the archived counters above: with >=2 Viewers the
  second whole-frame encode and its multi-MiB allocation no longer run.
- `HOST_CPU_DELTA = NOT_MEASURED`, `VIEWER_P95_DELTA = NOT_MEASURED`,
  `EFFECTIVE_THROUGHPUT = NOT_MEASURED`. No new same-SHA A/B or 9×120 s matrix
  was executed, and no CPU gain is claimed from CTest results. The archived
  encode stage is 0.0469 s / 40 s at 2 Viewers, i.e. small next to the 2.67 s
  Qt render/driver cost: this patch is a bounded duplicate-work removal, not a
  claim to have solved the dominant cost.
- Wire bytes per socket, write ordering, damage commit and reconnect semantics
  are unchanged, so no Viewer freshness regression is introduced by
  construction; real Viewer decode / physical present remain `NOT_MEASURED`.
- `rfb_socket_queued` is not a Viewer present. `HOST_GPU = NOT_MEASURED`.

## 4. BACKEND_PUBLIC_FALLBACK

Default route is public Qt `grabToImage()`; log identity
`HYREMOTE_EXPERIMENTAL_QUICK_CAPTURE=PUBLIC_QT_FALLBACK`. Native GL PBO and
D3D11 staging stay opt-in / default OFF, with the #995 single
`Admitted/Transient/Unsupported + Reason` table and one-way public fallback.
Qualification unchanged: Qt 5.15.2 DirectGL / Qt 6.8.3 desktop GL small-target
real-GPU PASS (native OFF), Windows D3D11 static-1V CPU/private-bytes gain with
viewer latency `NOT_QUALIFIED`, Qt 6 GLES/EGLFS, redirected FBO, high DPI,
multi-GPU and other RHI `NOT_QUALIFIED` on the public fallback. No SoC, QPA,
Qt-version or customer-specific branch. Windows evidence is not extrapolated
to ARM64.

## 5. 0V_RESOURCE

The shared encoded buffer exists only while >=2 Normal Viewers are being served
in one frame. It is released by `frameAvailable()` at the start of every new
frame, by `releaseIdleFrameCaches()` on the last disconnect / initial
clientless bootstrap edge (#997), by `failDecoration()` when viewers are
dropped, and by `shutdown()`. A single Viewer never allocates it. No change to
`CaptureSource::releaseIdleResources()`, no new idle timer or viewer counter.

## 6. FIELD_CANDIDATE_STATUS

One candidate only: exact head of the single implementation PR, reported on
GitHub (a file cannot contain the hash of the commit containing it).

- Required source files: `src/runtime/src/transport/rfb_transport.cpp`,
  `src/runtime/src/automatic/composite_target.cpp`, and this document. No new
  public SDK header, ABI, config, schema or Viewer-side change.
- Target: Windows x86_64 / Qt 6.8.3 `msvc2022_64`; Qt 5.15.2 covered by the
  existing exact-head PR CI lane. Install via the same `build.cmd build` /
  `build.cmd install` and `build.yml` profile in the single workspace `build`
  tree. Rollback = revert this PR and rebuild to `develop@b221eaf6`.
- Applicable CI: `build.cmd test` 82/82 at the local head; exact-head PR CI on
  Windows Qt6/Qt5, Linux Quick CPU probe OFF/ON, Generic real RFB OFF/ON,
  GL-RHI real CaptureSource pixels/lifetime, review gate and topology.
- Required real Windows Viewer acceptance (not done here): same-SHA 1V/2V
  Host CPU and Viewer P95/P99, A→B large-page convergence, disconnect
  RSS/PSS/threads, reconnect, plus a `rfb_encode` stage read to confirm the
  second Viewer no longer re-encodes. No customer package is claimed signed
  or deployed.
- Single-link A/B: development Codebuddy A owns integration/build/report;
  the User decides and manually upgrades; test Codebuddy B measures only the
  currently running program (desensitised 0V/1V/2V CPU, one authorized A→B→A
  convergence, last-disconnect CPU/RSS/PSS/threads, reconnect), needs no build
  manifest from A and does not upgrade for the User. If the running SHA cannot
  be proven independently, write `RUNTIME_SHA=NOT_VERIFIED`.
- Human is asked only for a real customer-site authorization or a major
  release-scope decision.

## 7. RELEASE_GATE

`HOLD`. `release/v0.3.5.0@b0680d5df4f76d8b84b02a1b950030a65a89ddca` and the
OPEN #876 five-file whitelist are untouched; no full develop backmerge, no
admin bypass, no topology-check or independent-App ruleset bypass. `develop`
merge is not release qualification; #95 owns that gate. `PRODUCT_GO = NO` for
#789/#567/#790 until exact-SHA field evidence exists. PR merge / green CI is
not a field GO.
