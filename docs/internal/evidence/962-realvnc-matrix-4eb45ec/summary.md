# #962 fixed-SHA RealVNC matrix — develop `4eb45ec13ceb907966e4619f4e10eec9b6ff468d`

Fixed source SHA: `4eb45ec13ceb907966e4619f4e10eec9b6ff468d` (merge commit of PR #963
`feature/962-session-gl-reuse` into `develop`, 2026-11-12T07:41:53Z). The source
checkout was clean of tracked modifications at run time; the harness verifies that
SHA and cleanliness before building.

Archived harness (byte-exact, restored from `6b340b4e2698df9410b7a73c2556e79ea83fb0bf`):
`docs/internal/evidence/962-realvnc-matrix/`. Command actually executed:

```powershell
./docs/internal/evidence/962-realvnc-matrix/run_fixed_matrix.ps1 `
  -Source <clean checkout of 4eb45ec13ceb907966e4619f4e10eec9b6ff468d> `
  -ExpectedSha 4eb45ec13ceb907966e4619f4e10eec9b6ff468d `
  -Out <new output directory>
```

Arms: **A1**/**A2** = public Qt `grabToImage` runtime, `HYREMOTE_EXPERIMENTAL_QUICK_GL_PBO=OFF`;
**B** = the same runtime with `HYREMOTE_EXPERIMENTAL_QUICK_GL_PBO=ON` (session-scoped shared
`QOpenGLContext`/offscreen surface/retire worker/PBO, PR #963). `HYREMOTE_ENABLE_CPU_PROBE=ON`,
Release, MSVC 19.44, Qt `C:/Qt/6.8.3/msvc2022_64`, `QSG_RHI_BACKEND=opengl`,
`QSG_RENDER_LOOP=threaded`, `QT_SCALE_FACTOR=1`. Run order per viewer count is A1 → B → A2,
120 s each, content changes every 1 s, first 5 and the final partially observed change excluded
from the steady window.

Build identity (identical for all runs of the same arm; `host.exe` differs between arms because
it links the corresponding runtime):

| Arm | `HyRemoteRemoteAccess.dll` SHA-256 | `host.exe` SHA-256 | `vncviewer.exe` SHA-256 |
|---|---|---|---|
| A1/A2 (OFF) | `74fae196b0b0dc314e1303090f3349c9d97c009f13ae22677c753266e78fe136` | `79765d94c75901b4e4542e88ee95f56144607ab120b3dd60a328ec69fc5019ba` | `39195c0abe53dc8fc2a57beeb6df9accf72226d46688845bf864c4d639030d84` |
| B (ON) | `3afa3b7e9987938d4fcd173e2f30c2c82aea6a8b8abc06c213c78cd9535c296c` | `e3a999f67f0c5a4451298f0c966adedb8c8f836357ee521dbe8586c9faf73d25` | `39195c0abe53dc8fc2a57beeb6df9accf72226d46688845bf864c4d639030d84` |

Host environment: Windows 11 Pro, console session, Intel(R) UHD Graphics 770,
driver 32.0.101.7088, `OpenGL VENDOR: Intel RENDERER: Intel(R) UHD Graphics 770 VERSION: 4.6.0`,
QRhi backend OpenGL (compatibility profile 4.6 as reported by Qt), RealVNC Viewer 7.15.1.

## Result

Detection latency = RealVNC client window surface color observed on the desktop after a
`CHANGE` marker was printed by the host fixture. Percentiles are nearest-rank over the pooled
steady detections; for two Viewers both windows are pooled (`detect n` = 114 × 2).

| Viewers | Arm | native/public frames | active Viewers | steady Hz | n | P50 ms | P95 ms | P99 ms | missed | Host CPU %1 core | RSS P95 MiB | Qt swaps | swap-gap P95 ms | WM_KEYDOWN→Qt P50 ms |
|---:|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 0 | A1 | 0/3 | 0 | 0.00 | – | – | – | – | 0 | 0.71 | 85.9 | 122 | 1008.7 | 0.941 |
| 0 | B | 3/0 | 0 | 0.00 | – | – | – | – | 0 | 0.94 | 84.8 | 125 | 1009.8 | 0.567 |
| 0 | A2 | 0/3 | 0 | 0.00 | – | – | – | – | 0 | 0.73 | 86.2 | 99 | 1011.4 | 0.773 |
| 1 | A1 | 0/935 | 1 | 7.73 | 113 | 172.4 | 287.9 | 304.7 | 0 | 6.10 | 89.9 | 939 | 286.2 | 0.738 |
| 1 | B | 1176/0 | 1 | 9.54 | 113 | 180.7 | 314.6 | 336.6 | 0 | 3.80 | 87.1 | 876 | 315.9 | 1.156 |
| 1 | A2 | 0/936 | 1 | 7.71 | 113 | 156.5 | 303.3 | 315.9 | 0 | 5.90 | 88.1 | 933 | 286.2 | 0.893 |
| 2 | A1 | 0/933 | 2 | 7.69 | 228 | 172.1 | 295.4 | 315.6 | 0 | 4.55 | 88.4 | 925 | 284.9 | 1.253 |
| 2 | B | 1078/0 | 2 | 8.88 | 228 | 177.2 | 332.7 | 440.7 | 0 | 3.91 | 85.4 | 835 | 315.2 | 0.622 |
| 2 | A2 | 0/822 | 2 | 6.71 | 228 | 158.5 | 280.6 | 832.3 | 0 | 4.50 | 88.6 | 919 | 261.9 | 0.536 |

Per-window P50/P95/P99 (ms), two-Viewer runs: A1 `177.6/293.0/315.0` and `166.7/297.8/324.3`;
B `177.9/320.6/440.7` and `167.4/336.9/425.9`; A2 `156.1/286.7/832.3` and `160.3/269.7/326.3`.
The A2 single-window P99 = 832.3 ms is one maximum sample, not a distribution shift
(`viewer_samples` in `raw/viewers-2-A2.json`).

Machine-readable form of the same numbers: `matrix-summary.csv`. Per-run raw probe output:
`raw/viewers-<n>-<arm>.json`.

## Reading of the numbers

- Route attribution is unambiguous in every run: A1/A2 show only `quick_grab_submitted`
  (public `grabToImage`), B shows only `gpu_readback_submitted`, never a public grab. Active
  RealVNC viewers matched the planned 0/1/2 in all nine runs; `stop_ok` and `host_exit=0` in all.
- **0 Viewer** performs zero steady-state capture in all three arms (only the three startup
  requests in B), so the demand gate holds.
- **No latency win for B is demonstrated.** P50 is `+8.3 ms` vs A1 and `+24.2 ms` vs A2 at one
  Viewer, `+5.1 ms` / `+18.7 ms` at two Viewers. The same-build A1↔A2 spread in this pass is
  `15.9 ms` and `13.7 ms`, i.e. of the same order as the B effect, so a single ordering pass
  cannot separate a real B penalty from drift; what it does exclude is any sizeable improvement.
  Tails are worse for B (P95 `+27…+37 ms`, P99 `+21…+125 ms`).
- **B uses clearly less host CPU at one Viewer** (3.80 % vs 6.10 %/5.90 % of one core, ≈ −38 %)
  and slightly less at two Viewers (3.91 % vs 4.55 %/4.50 %), with marginally lower RSS. At
  zero Viewers B is slightly higher (0.94 % vs 0.71 %/0.73 %), consistent with its idle worker.
- B captures at a higher steady rate (9.54 Hz vs 7.73/7.71 Hz at one Viewer) while delivering
  the same 113 encoded frames; the extra readbacks do not buy lower viewer-visible latency.
- Fixed ordering (A1 → B → A2 within each viewer count), one 120 s pass per cell, and a
  ~1 s content-change cadence bound what this evidence can claim. A repeat with rotated or
  interleaved ordering would be required to attribute the A1/A2 spread itself.

## Not measured

- `physical_present = NOT_MEASURED`: detection is a client-surface color scan, not a photon-level
  present or input-to-photon measurement.
- Host GPU utilization: `typeperf "\GPU Engine(*)\Utilization Percentage"` returned only counters
  of other PIDs and none for the host PID on this machine
  (`host_gpu_counter_available=false` in every `result.json`). It is reported as unavailable, not
  as 0. The raw `gpu-counters.csv` files (≈1 MB each) stay in the local run directory.
- Viewer-side decode/render timings: the archived `measure_matrix.py` records client-surface
  detection only; it does not parse the per-frame decode trace that the single-Viewer
  `measure.py` probe supports.

## Retained locally (not committed)

`tmp/win962/matrix-4eb45ec/viewers-<n>-<arm>/` keeps the full run directory for each cell:
`runtime-frame-trace.jsonl` (1.3–1.4 MB), `runtime-cpu.jsonl`, `host.stdout.log`,
`host.stderr.log` (includes the `QSG`/`qt.rhi` renderer lines quoted above), `gpu-counters.csv`,
`viewer-<k>.log`, `result.json`. Only `result.json` and the recomputed CSV are committed here.

## Verdict

`RESULT=PASS_SCOPED` for the matrix execution itself (9/9 cells valid, routes and viewer counts
proven, zero missed changes, clean stop). `PRODUCT_GO=NO` for the session-scoped GL readback
route: this fixed-SHA evidence shows lower host CPU but no viewer-visible latency improvement and
worse tails, with present and GPU utilization unmeasured.
