# HyRemote #609 — Windows 1080p Host resource baseline work order

**Execution status: NOT_RUN / LOCAL OWNER ACK NOT OBSERVED.** This is a reviewed repository work order for real-machine execution, not a machine-worker task or evidence that measurements exist. The current opt-in Local Owner worker only supports release L2/G6/installed-unicode tasks; do not add an unsupported task to its strict JSON inbox.

- Authority: [#609](https://github.com/skawu/HyRemote/issues/609), [#370](https://github.com/skawu/HyRemote/issues/370), [#374](https://github.com/skawu/HyRemote/issues/374)
- Priority: **P1 product quality**, run **in parallel** with V0.3.5.0 RC4 qualification; no change to release scope or V0.3.6.0 feature identity.
- Exact baseline source: `develop@14c5b4e05b75711f5e17f720170bf6704d1af185` (Windows Qt6.8.3, clean Release build). Record Git tree, build identity, compiler, Kit, OS, CPU model, RAM, screen DPI/DPR, Viewer version, network topology, and source SHA in every report.
- Real machine and installed Qt application required; no hosted CI or synthetic-only data may be substituted for Host/Viewer field measurements.

## Controlled workload matrix

Run the **same host application**, same scene/data, same resolution 1920×1080, same Kit and hardware, with Widgets and Qt Quick separately. Capture first-start transient separately, then sample 30–60 seconds of steady state per case; repeat comparable cases and document warm-up/soak lengths.

| Case | Workload | Comparison |
| --- | --- | --- |
| B0 | Original application without HyRemote enabled | Baseline Host and local UI |
| B1 | HyRemote enabled, **zero Viewer** | Idle/capture overhead over B0 |
| B2 | One real maintained Viewer, static scene | Demand, idle and unnecessary full-frame work |
| B3 | One Viewer, localized input/partial UI updates | Local responsiveness and tile/damage behavior |
| B4 | One Viewer, full-screen/high-motion changes | Capture, readback, encode and network costs |
| B5 | Two simultaneous Viewers, same scene | Per-client encode/queue scaling |

For each case measure Host process CPU **processor time / wall time** (one-core equivalent, may exceed 100%), Host RSS/working set, private committed bytes and peak, frame age/FPS and network bytes if instrumented, local Qt UI input latency/freeze, Viewer process CPU/memory separately, and capture/convert/tile/Raw/TRLE/queue timing P50/P95 where instrumentation actually exists. Preserve provenance of unavailable counters as NOT_MEASURED; do not present a test-process `std::clock()` figure as Host CPU.

## Evidence and disposition

Record reproducible commands, JSON raw samples, sample count, timestamps, workload description, baseline vs enabled **incremental** CPU and memory, observed local responsiveness, and any failed case in #374; link a concise summary in #609. Attribute a hotspot only after matching the same source/workload. Optimize one measured dominant portable bottleneck in a separately reviewed PR with before/after Host and Viewer evidence; do not infer GPU/DMA-BUF/H.264 need or RK3588 support. A reproducible severe violation of an existing supported resource contract is escalated to #95 for release-blocker adjudication; otherwise #609 proceeds independently without blocking RC4 by assertion.

If no physical Windows environment or Local Owner is available, report **BLOCKED / NOT_RUN**; never fabricate CPU percentages, RSS deltas, or machine ACK.
