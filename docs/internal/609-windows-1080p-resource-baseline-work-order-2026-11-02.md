# HyRemote #609 — Windows 1080p Host resource baseline work order

**Execution status: NOT_RUN / LOCAL OWNER ACK NOT OBSERVED.** This is a reviewed repository work order for real-machine execution, not a machine-worker task or evidence that measurements exist. The current opt-in Local Owner worker only supports release L2/G6/installed-unicode tasks; do not add an unsupported task to its strict JSON inbox.

- Authority: [#609](https://github.com/skawu/HyRemote/issues/609), [#370](https://github.com/skawu/HyRemote/issues/370), [#374](https://github.com/skawu/HyRemote/issues/374)
- Priority: **P1 product quality**, in parallel with V0.3.5.0 RC7 finalization; **not** a new RC7 release gate, source-code change or V0.3.6.0 Feature scope change.
- **Primary measurement source (current product):** immutable `release/v0.3.5.0@ae4677f321b140050813d35479639c302f710c2e` (tree `6e5a929d688e22de1a45770019416201a66d3162`) in a **separate clean local worktree**. This source includes independently reviewed #797 static-capture demand and #798 zero-viewer frame-memory fixes. The release branch and candidate must not be modified by measurements; this is a resource profile, not a rerun of G6/L2.
- **Optional historical comparison source:** `develop@14c5b4e05b75711f5e17f720170bf6704d1af185`, as previously planned. If available, measure it in its own clean worktree on the *same* machine, app, scene, Kit, resolution, Viewer and workload, with separately labeled source SHA. Never combine old-SHA counters with new-SHA observations as a single same-SHA baseline, or call any old-vs-new change a measured fix if the conditions differ.
- For each actual cohort record Git SHA and tree, clean status, build identity, compiler/Qt Kit, OS, CPU, RAM, display DPI/DPR, Viewer version, network topology and exact timestamps; **no test/report equals actual ACK unless Local Owner reports it**.
- Real machine and installed Qt application required; no hosted CI or synthetic-only data may be substituted for Host/Viewer field measurements.

## First executable slice — produce actual measurements before expanding scope

Run **B0 → B1 → B2 first**, using the same host application/scene under each exact-SHA cohort, **Qt6.8.3 Windows 1920×1080 Quick and Widgets separately**. B0 has the original app running without HyRemote, B1 enables HyRemote with zero Viewer, B2 has one actual maintained Viewer and completely static remote content. Obtain 30–60 seconds of steady state each (plus warm-up noted separately); capture timestamped Host CPU *process-time/wall-time* one-core-equivalent, Working Set/RSS and Private Bytes, Viewer CPU/memory separately, and visibly responsive local Qt UI. Provide sample counts and raw JSON alongside screenshots where meaningful. Post raw evidence to [#374](https://github.com/skawu/HyRemote/issues/374) and a concise B0→B1 and B1→B2 incremental Host CPU/memory table to [#609](https://github.com/skawu/HyRemote/issues/609).

Then complete B3/B4/B5 from the matrix below without requiring a fresh priority decision. Check **whether the current RC7 source actually improved** static capture and frame ownership, do not infer it from merged source alone. Only project-owned equivalent RK3588/Buildroot/EGLFS testing can qualify #789/#790/#567 target outcomes; the user reporting those issues is **not** the QA executor. If host acquisition is unavailable, explicitly report `NOT_RUN/BLOCKED`; avoid further prose-only status loops or claiming a machine task was dispatched.

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
