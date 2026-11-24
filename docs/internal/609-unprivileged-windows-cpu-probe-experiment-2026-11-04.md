# HyRemote #609 — no-admin Windows Host CPU stage attribution

Status: **diagnostic tool proposed, actual stage measurements NOT_RUN**. This is a develop-only experiment. It does **not** modify frozen `release/v0.3.5.0@b0680d5d` or certify a source optimization.

## Reason and measurement boundary

The real same-machine static 1920×1080 B1→B2 Host CPU deltas were +5.4 pp (Widgets/Generic) and +9.7 pp (Qt Quick) of one i7-13700K core. WPR/xperf sampling was **NOT_RUN** under the current Medium-integrity Windows session (`wpr 0xc5585011`, `xperf 0x3ec`). An earlier process-thread-enumeration snapshot also missed short-lived CPU work and must not be reused.

The opt-in probes now time **the executing thread around named source stages**, using `GetThreadTimes`, and separately sample **full process CPU time** using `GetProcessTimes`. They do not see the detailed Qt/GPU/driver worker call stacks, so that CPU remains **UNKNOWN remainder**. The asynchronous Quick submit→grab-ready duration is **wall latency, never CPU**.

## Safe build and collection

- Private CMake option `HYREMOTE_ENABLE_CPU_PROBE` is **OFF by default**. Disabled code builds without Windows profiling dependencies and stage macros are no-ops. Opt in on a **separate clean Windows x64 diagnostic worktree/config** using the existing qualified Qt 6.8.3 MSVC2022 Release Kit, plus `-DHYREMOTE_ENABLE_CPU_PROBE=ON`. Do not use a diagnostic binary as release-qualifying evidence.
- Set `HYREMOTE_CPU_PROBE_FILE` to a distinct local ASCII-named path for each Host process, e.g. `C:\temp\hyremote-perf\quick-b2.jsonl`; create the parent directory. Each process appends 1 Hz JSONL snapshots for up to 300 seconds. Keep source SHA, Qt Kit, sample window, RFB framebuffer dimensions and separate Host/Viewer identities.
- Every line holds only relative time, PID, full-process CPU, aggregate stage calls, CPU/wall nanoseconds and byte counters; **no pixels, input text, credentials, tokens, or remote session addresses**. Do not upload raw private traces; submit sanitized summaries to #374.
- First validate the formatter with `python tools/perf/aggregate_cpu_probe.py --self-test`. Run the **same static 1920×1080 Widgets Generic and QML Quick harnesses** as the accepted #374 measurement, with B1 (0 Viewer) and B2 (1 maintained RealVNC Viewer) separately. Allow 10 s warm-up, 40 s sample, and enough process lifetime (~55 s) for snapshots.
- Analyze each pair: `python tools/perf/aggregate_cpu_probe.py --b1 <b1.jsonl> --b2 <b2.jsonl> --warmup-seconds 10 --seconds 40`. Repeat the instrumented pair if small differences are noisy. Include a **same-source default-OFF control** to quantify probe overhead.
- The direct-stage thread CPU costs may be below the clock's effective precision: look at `cpu_samples_missing` and counts, avoid treating a zero result as definitive. Sum only non-overlapping named scopes; the output explicitly reports **UNKNOWN/OVERLAP remainder** rather than forcing six buckets to 100%. Never sum async Quick wall time into CPU time. No WPR/call-stack PASS claimed.

## What each counter means

| Counter | Measured region | Important boundary |
| --- | --- | --- |
| `widget_buffer` | owned full-frame buffer allocation | not all wrapper/layout work |
| `widget_paint` | transparent fill and QWidget::render | GUI callbacks included |
| `quick_grab_submit` | calling grabToImage | driver/render worker CPU not included |
| `quick_grab_ready_wall` | asynchronous grab submit→ready | WALL ONLY, not CPU |
| `quick_convert` | convertToFormat | may return without deep conversion |
| `quick_buffer`, `quick_copy` | owned storage allocation and row copy | copied RGBA bytes counted |
| `generic_compose` | automatic composite frame assembly | includes fallback/publish path work |
| `rfb_compare` | RFB changedRfbTiles comparison | CPU on RFB worker thread |
| `rfb_encode`, `rfb_write` | selected rectangles encoded and queued | asynchronous network delivery not counted |

## Decision and validation rule

Report B1/B2 matched process CPU delta and per-stage thread CPU delta on #609, with unknown remainder and actual counts; raw sanitized samples on #374. Identify the dominant stage **before** changing production. Possible fixes: event-assisted dirty-frame caching with bounded fallback and immediate input/new Viewer/resize/reconnect wake; guarded single-opaque-child Generic fast path; immutable Quick buffer reuse; region-damage accuracy. No indefinite freeze, no weakening of fail-closed watermark, no new H.264/GPU architecture based only on hypotheses.

Any optimization is a **separate reviewed PR on develop**, measured against an equivalent build with the same screen, Viewer, input and scene (including active motion, reconnect, two Viewers and watermark/oversize negative tests). P1 RK3588 outcome verification stays open and separate. Product release candidate and version roadmap remain unchanged.
