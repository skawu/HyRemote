# #967 counter-evidence: 35 s fixed-SHA matrix on `a43b76e1d95821d47f2703db945351ea6cd19daf`

Final `develop` SHA after PR #971 (`fix(quick-gl): arm static-surface fallback render
without a 17ms wait`). One planned execution only, per the `TEST_PLAN` published on
#967 under #970 §6. No 9×120 s run, no repeat of the 82-case suite.

## Identity and method

- Clean checkout at `a43b76e1d95821d47f2703db945351ea6cd19daf`;
  `run_fixed_matrix.ps1` verifies the 40-hex SHA and tracked cleanliness.
- OFF (A1/A2) and ON (B) Release built from the same source, Qt 6.8.3
  MSVC2022_64, `HYREMOTE_ENABLE_CPU_PROBE=ON`, `QSG_RHI_BACKEND=opengl`,
  `QSG_RENDER_LOOP=threaded`, Intel UHD 770 (32.0.101.7088), RealVNC Viewer
  7.15.1, Windows 11 console session.
- 1/0/2 Viewer × A1/B/A2, **35 s per cell**, 9 cells once. Content changes
  every 1 s; first 5 and final partially observed change excluded from steady.

| Arm | `HyRemoteRemoteAccess.dll` SHA-256 | `host.exe` SHA-256 |
|---|---|---|
| A1/A2 (OFF) | `09c6697d37f239749942b05c9a0f33a5222284c425297ca2ba28e4c9199f5523` | `f7af832e09b2df0e3986c5efafb846129f3eb524383d9442b1f98a2c8e6c70bd` |
| B (ON) | `a81348f60616aff7eec064dc24308c2d6bc1272174429a514d3100f1136472f4` | `5c73a0decf8b858ff287853c172587a3263430320cd0c0a82ee5ea281f8b2d32` |

`vncviewer.exe` = `39195c0abe53dc8fc2a57beeb6df9accf72226d46688845bf864c4d639030d84`
in all nine cells. `source_sha` in every `result.json` is the final SHA above.

## Result

Viewer detection = RealVNC client window desktop color observed after the host
`CHANGE` marker. Percentiles are nearest-rank over pooled steady detections
(two Viewer windows pooled where applicable).

| Viewers | Arm | native/public | active | steady Hz | n | P50 ms | P95 ms | P99 ms | missed | CPU %1c | RSS P95 MiB | Qt swaps | stop |
|---:|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 0 | A1 | 0/3 | 0 | 0.00 | – | – | – | – | 0 | 1.95 | 86.3 | 37 | ok |
| 0 | B | 3/0 | 0 | 0.00 | – | – | – | – | 0 | 0.37 | 85.4 | 39 | ok |
| 0 | A2 | 0/3 | 0 | 0.00 | – | – | – | – | 0 | 0.05 | 85.8 | 37 | ok |
| 1 | A1 | 0/246 | 1 | 6.74 | 29 | 137.4 | 255.5 | 270.9 | 0 | 3.11 | 132.6 | 276 | ok |
| 1 | B | 218/0 | 1 | 6.30 | 29 | **98.0** | 261.2 | 277.8 | 0 | 1.68 | 86.4 | 231 | ok |
| 1 | A2 | 0/245 | 1 | 6.63 | 29 | 157.0 | 274.1 | 274.7 | 0 | 3.84 | 88.4 | 279 | ok |
| 2 | A1 | 0/244 | 2 | 6.63 | 58 | 161.7 | 279.5 | 282.0 | 0 | 2.79 | 88.3 | 275 | ok |
| 2 | B | 200/0 | 2 | 6.20 | 58 | **102.1** | **224.2** | **240.7** | 0 | 2.21 | 84.8 | 205 | ok |
| 2 | A2 | 0/244 | 2 | 6.77 | 58 | 149.8 | 282.3 | 297.4 | 0 | 2.79 | 88.5 | 277 | ok |

Per-window P50/P95/P99 (ms), 2 Viewer: A1 `178.9/281.1/282.0` + `161.7/265.0/265.6`;
B `103.6/208.0/240.7` + `102.1/224.2/225.3`; A2 `149.8/282.3/285.0` +
`163.8/268.2/297.4`.

Machine-readable: `matrix-35s-summary.csv`, `matrix-stages.csv`; full official
intervals: `raw/matrix-stages.json`; per-cell probe output: `raw/viewers-*.json`.

## Stage counter-evidence (same `attribute_frame_stages.py` as #967 stage A)

`capture_request → gpu_readback_submitted` P50:

| Viewers | `4eb45ec1` B (before) | `a43b76e1` B (after) | A1/A2 after |
|---:|---:|---:|---:|
| 1 | 41.55 ms | **6.79 ms** | 0.22 / 0.21 ms |
| 2 | 42.59 ms | **7.08 ms** | 0.22 / 0.22 ms |

`gpu_readback_submitted → source_completed` P50 remains 1.27 / 2.06 ms;
`capture_request → rfb_socket_queued` P50 is 9.59 / 12.87 ms in B versus
9.81 / 10.17 ms in A1. Encoded frames are identical per viewer count
(37 for 1 Viewer, 74 for 2 Viewers in all arms), so the P50 gain is not from
extra frames or a changed RFB cadence.

**Not a clean sweep**: B's `req→submit` P95 is 145 / 154 ms and
`req→socket` P95 303 / 158 ms (A ≈ 2 / 14–16 ms). At 1 Viewer B's P95
(261.2 ms) and P99 (277.8 ms) are not better than A1 (255.5 / 270.9 ms). The
tail likely comes from the bounded fallback arm not being released until a real
render callback plus GL session startup (0 Viewer B `req→submit` P50 = 319.9 ms
for its three startup requests); this evidence localises it but does not
attribute the tail further. Duplicate `content_pts_ns` requests remain
(15 / 234 at 1 Viewer, 17 / 230 at 2 Viewers; 0 in public arms), consistent
with the withdrawn coalescing being left as a Core-level follow-up.

## Reading

- **The stage-A hypothesis survived a real-Viewer test on the final SHA.**
  Removing the fixed 17ms opportunity wait cut `capture_request→submit` P50 by
  ~35 ms and Viewer detection P50 by 39–59 ms (1 Viewer) / 48–60 ms (2 Viewer),
  while Host CPU also fell (1.68 % vs 3.11/3.84 %; 2.21 % vs 2.79/2.79 % of
  one core) and RSS did not grow.
- **Single ordered A1→B→A2 pass, 29 samples per window, 35 s per cell.**
  Same-build A1↔A2 P50 drift is 19.7 ms (1 Viewer) / 11.9 ms (2 Viewer),
  smaller than B's P50 gain but not negligible; the 1-Viewer tail does not
  improve. No interleaved/reversed ordering was run in this pass.
- Route attribution and lifecycle are intact: A1/A2 only public grab, B only
  GPU readback with no public fallback; active Viewer counts match the plan;
  0 Viewer has zero steady capture in every arm; `missed=0`,
  `stop_ok=true`, `host_exit=0` in all nine cells.
- `PRESENT_NOT_MEASURED` (client-surface color scan, not physical present or
  input-to-photon). Host GPU utilization `NOT_MEASURED`: `typeperf` GPU Engine
  wildcard returned no counter for the Host PID on this machine
  (`host_gpu_counter_available=false`); not recorded as 0. Viewer decode/
  present stages are not inferred from Host trace.

## Raw trace SHA-256 (retained locally in `tmp/win967/matrix-35s/`, not committed)

| Cell | SHA-256 |
|---|---|
| viewers-0-A1 | `e601125e7246a41098c95f45865e3c48a21926922274ccbae298027e0ff69cdd` |
| viewers-0-A2 | `46f8787d0c7093041c45f71bd79597b9021104ac65a6fb423078fd55b80f3634` |
| viewers-0-B | `97768efd3e30025322beb01f9ec616608f82ed1f7a60c09ce6cd26919346340d` |
| viewers-1-A1 | `d75a8e0ca30cc7bde910a0d70773b0f49ac5b92659bd37572da911dde042bfc6` |
| viewers-1-A2 | `a0b8b262ab06c1106196686cb984a9b1540908db4a21f7c59c34ea2488cdf9e7` |
| viewers-1-B | `0806782288ea22ef318b63c1bdaa3811336a86cc24925fed09ac18e2c81fcc0d` |
| viewers-2-A1 | `08f4da1fc2aaab44875eee9ea4d7d8daced3a3d29bbf4d08376d3ff67c1f14e1` |
| viewers-2-A2 | `5a383663400e3a7fb3eb82bd9f3e7498de2a2ac27b7aafbaed66d6ce78541bc6` |
| viewers-2-B | `6f0775fd05bf49858041170440fb4c752a2431f5fa7b875a81bcb18327c75e53` |

## Verdict

`RESULT=PASS_SCOPED` for the planned 35 s counter-evidence: the predicted
Host-stage cost was removed and the P50 direction matches, with no CPU/RSS or
lifecycle regression. `PRODUCT_GO=NO`: 35 s samples, one ordering, a 1-Viewer
tail that does not improve, unmeasured present/GPU/viewer-decode, and an
unresolved `req→submit` P95 tail. A long 9×120 s matrix is **not** justified by
this short run and was not executed.
