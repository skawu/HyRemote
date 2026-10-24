# HyRemote Performance Optimization Programme

> Language / 语言: **English** | [中文](../performance-optimization.md)

Status: long-lived technical programme. GitHub authority: #370.

This document defines HyRemote's performance architecture, latency contract, optimization order, and regression discipline. It is maintained across releases so later feature work cannot silently erode accepted responsiveness, frame freshness, or local Qt behavior.

## 1. Product position

HyRemote is a low-intrusion remote-access Runtime and SDK for existing Qt applications. Performance work makes remote use responsive without turning performance engineering into a new burden for application developers, deployment engineers, or remote operators.

Normal users continue to use product concepts:

- target and application surface;
- listener address and port;
- access and security mode;
- remote input policy;
- start/stop and diagnostics.

Normal users do not choose or tune:

- RFB encodings;
- compression levels;
- damage modes;
- capture FPS;
- capture in-flight depth;
- flow-control windows;
- socket buffer sizes;
- GPU, DMA-BUF, or vendor backends.

HyRemote optimizes interactive Qt remote access, not maximum benchmark throughput or video-streaming FPS.

Performance priority:

1. remote interaction remains responsive;
2. the viewer receives the newest useful state, not a backlog of stale frames;
3. local Qt rendering and input remain healthy;
4. static or localized UI avoids unnecessary capture, encode, and network work;
5. queues, memory, and per-viewer work remain bounded;
6. once those are satisfied, increase sustainable delivery cadence.

Core principle:

> **Freshness > throughput > frame completeness.**

Dropping or coalescing stale intermediate frames is better than delivering every historical frame late.

## 2. Primary user-facing metric: remote interaction latency

Capture time alone is not a product latency metric. The primary metric is the time from a remote user's action to the corresponding visible result reaching the viewer.

```text
viewer sends input
  -> network / RFB input
  -> Runtime / Core / input adapter
  -> Qt application handles event
  -> UI/render state changes
  -> capture
  -> Core / Runtime scheduling
  -> encode / delivery
  -> viewer receives / decodes / displays corresponding visual change
```

Definition:

```text
T_interaction =
  t(viewer observes first corresponding visual change)
  - t(viewer sends input)
```

Where possible, record both timestamps on the viewer or test-client side to avoid cross-machine clock synchronization.

Useful segments:

- input-to-Qt-event latency;
- Qt-event/render-to-capture availability;
- capture request-to-completion;
- frame age or PTS age;
- encode latency;
- bytes per update and bytes per time;
- application-level RTT or Fence RTT;
- socket and backlog observations;
- viewer decode and display latency;
- CPU, GPU, memory, and local UI impact.

Decomposition:

```text
T_interaction =
  T_network_in
+ T_input
+ T_render
+ T_capture
+ T_queue
+ T_encode
+ T_network_out
+ T_viewer
```

Segment budgets are diagnostic. When a candidate misses the end-to-end SLO, identify the dominant component before choosing a lower-level optimization.

## 3. Initial pre-GA reference SLO

Ordinary interactive Qt UI reference profile:

```text
1920x1080 target
one qualified maintained viewer
direct wired LAN
RTT <= 20 ms
effective available bandwidth >= 100 Mbps
ordinary Widgets/HMI/form/low-to-moderate-change Quick workload
selected supported RFB delivery profile
```

Initial target:

```text
Interaction latency P50 <= 200 ms
Interaction latency P95 <= 500 ms
```

`500 ms` is the acceptance ceiling for this bounded reference profile. It is not a promise for every network, viewer, high-motion workload, or unsupported platform.

Every published result records:

- candidate SHA;
- Qt version and anchor;
- OS and CPU architecture;
- QPA and graphics stack;
- UI family;
- HyRemote integration route;
- resolution and DPR;
- viewer and version;
- RTT and effective bandwidth or shaping profile;
- selected encoding and update strategy;
- workload class.

High-motion or media-like workloads may have a different qualified envelope. Ordinary RFB GUI guarantees must not be presented as video-streaming guarantees.

## 4. Standard workloads

Performance evidence is meaningful only when the workload is repeatable.

### W1: static

A fully synchronized UI with no visible changes.

Required behavior:

- connection remains alive;
- an incremental request may remain pending;
- no repeated identical framebuffer payload is sent for keepalive;
- capture and encode work becomes quiescent or near-zero under the active policy;
- memory and queued work remain bounded.

### W2: localized UI

Examples: button state, text update, small status region, localized widget repaint.

Measure:

- changed region and bytes;
- bytes per update;
- encode latency;
- frame age;
- interaction latency.

This workload validates that damage and incremental delivery avoid full-screen work when that work is wasteful.

### W3: interactive

Examples: click, typing, focus change, drag, or a control action with a deterministic visible response.

This is the primary P50/P95 interaction-latency workload and validates input-triggered freshness.

### W4: high motion

Examples: Qt Quick animation, fade, swipe, large dashboard movement, or sustained full-area change.

Measure:

- full-frame compression behavior;
- frame age and freshness;
- delivered cadence;
- CPU and memory;
- bytes per time;
- local UI impact.

Do not force expensive partial-update analysis when measurement shows the scene is effectively full-frame motion.

## 5. Architecture responsibilities

Performance must preserve one Core and one Shared Runtime.

### 5.1 Core

Core owns only protocol-neutral and platform-neutral facts plus boundedness:

- `RemoteFrame` ownership and PTS;
- `Damage` as Unknown, FullFrame, or Regions;
- bounded capture and frame queue semantics;
- nonblocking transport handoff;
- latest-frame and freshness-first backpressure semantics.

Core must not learn:

- RFB encodings;
- Continuous Updates or Fence protocol objects;
- Qt platform or graphics backend details;
- vendor GPU or encoder APIs;
- network tuning policy.

### 5.2 Shared Runtime

Shared Runtime owns automatic performance policy shared by C++, QML, Generic, and QPA:

- viewer activity;
- remote-input activity;
- capture demand and pacing;
- adaptive policy coordination;
- suitable product diagnostics and observations.

There is no second Widgets, Quick, Embedded, or Accelerated Runtime variant.

### 5.3 Qt target adapters

Widgets and Quick adapters own the most appropriate supported capture path and accurate damage capability.

Rules:

- public Qt API first;
- preserve local display and input behavior;
- report damage capability accurately;
- use lower-level, private, or platform capture only after measured SLO or resource evidence proves a blocker.

### 5.4 RFB transport

RFB transport owns protocol-specific and network-specific performance mechanisms:

- encoding negotiation and Raw fallback;
- practical compression;
- incremental and non-incremental request semantics;
- accumulated per-viewer damage and update state;
- Continuous Updates and Fence where interoperable;
- bounded per-viewer flow control and backpressure;
- stale encoded-work suppression;
- cursor pseudo-encoding where useful;
- protocol-specific RTT and delivery observations.

## 6. Demand-driven capture

Capture should happen because it improves a connected user's experience, not only because a periodic timer exists.

Target policy:

```text
0 viewers
-> no unnecessary continuous capture

viewer connected + static scene
-> no repeated identical framebuffer payload

ordinary interaction
-> bounded interactive cadence

remote input occurs
-> prioritize the next fresh result after the corresponding Qt update/render

sustained high motion
-> stable bounded cadence, not unbounded capture
```

Connection liveness, capture activity, and framebuffer delivery are separate concerns. A static screen is not a disconnected screen.

## 7. Input-triggered fresh capture

Remote input is an interaction-priority signal.

Preferred behavior:

```text
remote input received
-> delivered to Qt
-> application state or render changes
-> prioritize the next fresh capture/update
-> viewer receives the newest corresponding state
```

This lowers perceived interaction latency without forcing a permanently high capture FPS on mostly static HMI or configuration interfaces.

The policy remains bounded. Input bursts must not create unbounded capture requests or timers.

## 8. Shallow capture pipeline

Remote access optimizes freshness, not offline throughput.

Historical Quick evidence shows that deeper asynchronous capture pipelines can increase throughput while also increasing request latency, duplicate content, and GUI jitter. Interactive defaults should stay shallow, normally around one or two outstanding captures unless an exact reference cell proves another value is better.

A later platform may tune this internally. Normal users do not choose it.

## 9. Stale-work suppression

Expensive work on an obsolete frame hurts latency twice: it consumes CPU and delays the newer useful frame.

Target per-viewer encoder scheduling:

```text
<= 1 update currently encoding or sending
+ <= 1 latest pending useful update
```

If a newer useful frame supersedes pending work before expensive encoding begins, replace or drop the stale pending work.

Do not build a historical framebuffer playlist.

## 10. Adaptive delivery for ordinary GUI and high motion

HyRemote uses two complementary strategies, not one universal update mode.

### 10.1 Region path

Static or localized ordinary Qt UI:

```text
capture/change observation
-> accumulated pending damage
-> intersect with the viewer-requested region
-> encode/send changed rectangles
```

This is the preferred path for forms, controls, ordinary Widgets/HMI screens, and low-change Quick interfaces when damage savings are material.

### 10.2 Full-frame path

Sustained large-area or full-area motion:

```text
latest full frame
-> practical compression
-> bounded delivery
```

This avoids spending excessive CPU on damage analysis when almost every pixel changes.

### 10.3 Automatic selection

Runtime or transport may use measured change ratio and hysteresis to choose the path. This is an implementation detail, not a normal-user `DamageMode` setting.

## 11. Compression policy

Before GA:

- Raw remains a deterministic interoperability fallback;
- at least one practical maintained-viewer-compatible compressed path is required;
- selection is based on end-to-end latency and resource behavior, not compression ratio alone.

Candidate comparison includes:

```text
encode P50/P95
wire bytes / serialization time
viewer compatibility and decode behavior
CPU and memory cost
final interaction latency and frame age
cross-platform dependency and packaging cost
```

A fast LAN may favor lower compression effort because encoding dominates. A constrained network may favor stronger compression because wire time dominates. Runtime and transport make the normal decision automatically; users do not need to understand compression internals.

Tight/JPEG or another lossy profile is added only if representative high-motion Quick or embedded evidence shows the selected lossless profile is insufficient.

## 12. Correct incremental semantics

Per-viewer state contains only the smallest bounded facts required for correct RFB updates:

- requested region;
- incremental or non-incremental mode;
- pending accumulated damage;
- resize or full-refresh invalidation;
- negotiated encoding capability and order;
- outstanding request, update, or continuous state;
- bounded coalescing and reset state.

Correctness property:

```text
A/B/C change while the viewer is not ready
-> the next eligible incremental update contains all still-required change
   inside the viewer-requested region
```

For `incremental=true` with no damage:

- keep the request pending;
- send no redundant framebuffer payload;
- later required damage wakes or satisfies the request.

For `incremental=false`:

- return the current requested content even when it matches previously delivered content.

Resize or format invalidation forces a correct refresh.

## 13. Connection liveness is not framebuffer traffic

A static session may produce no framebuffer payload for a long time while remaining connected.

Do not use repeated full frames as keepalive.

Where liveness support is required, use appropriate TCP keepalive and/or lightweight RFB synchronization such as Fence where interoperable. Network liveness policy remains independent of framebuffer change detection.

## 14. RFB latency control

### 14.1 Continuous Updates

When a maintained viewer supports RFB Continuous Updates, HyRemote should use it to avoid a new FramebufferUpdateRequest round trip for every update.

Unsupported viewers fall back automatically to standard request-driven behavior.

### 14.2 Fence

Fence may be used where supported for stream synchronization and bounded application-level RTT observation.

Protocol objects remain transport-private.

### 14.3 Per-viewer flow control

`socket bytesToWrite()==0` is useful but does not prove that kernel, network, proxy, or viewer buffers contain no stale framebuffer data.

A bounded per-viewer controller may observe:

- base, current, and Fence RTT;
- recent bytes per update;
- encode duration;
- socket backlog;
- frame age;
- outstanding delivery state;
- Continuous Updates state.

The goal is to minimize stale framebuffer age in the end-to-end path, not maximize TCP throughput.

HyRemote must not implement a second TCP congestion-control algorithm. TCP manages packet congestion. HyRemote decides whether an old framebuffer is still worth sending.

### 14.4 Multi-viewer isolation

Encoding negotiation, pending damage, RTT and flow state, and delivery windows are per viewer. A slow or high-RTT viewer must not force a fast LAN viewer to receive stale frames.

## 15. Cursor and perceived latency

Evaluate RFB cursor pseudo-encoding so a supporting viewer can render the cursor locally without waiting for a framebuffer round trip on every pointer movement.

This is a transport-private optimization with automatic fallback, not an application API.

## 16. Automatic fallback model

Every optimization is additive. An unsupported capability falls back to a correct lower-capability path:

```text
Damage unavailable             -> full-frame path
Compression unsupported        -> Raw fallback
Continuous Updates unsupported -> standard FramebufferUpdateRequest
Fence unsupported              -> conservative request-driven flow
Optimized capture unavailable  -> public Qt baseline
GPU/native buffer unavailable  -> CPU frame path
Adaptive policy unavailable    -> bounded conservative policy
```

Optimization failure must not become product failure when a supported fallback exists.

## 17. Platform acceleration gate

Do not start DMA-BUF, RKMPP, VAAPI, D3D, or private RHI work merely because it could be faster.

First implement and measure the portable product baseline:

- demand-driven capture;
- input-triggered freshness;
- practical compression;
- incremental damage;
- stale-work suppression;
- Continuous Updates and Fence where selected;
- bounded per-viewer flow control;
- adaptive capture policy.

Only a measured reference-cell failure against this programme may activate bounded platform acceleration through #123.

Valid activation evidence includes:

- capture or readback P95 consumes the latency budget;
- CPU frame copy materially harms the local HMI;
- encoder P95 is the dominant blocker on a declared ARM64 or desktop cell;
- the portable baseline cannot meet the power, CPU, or GPU budget.

Optimized backends remain private and replaceable, and preserve the same public integration semantics.

## 18. High-motion and media boundary

H.264, H.265, or AV1 is not an automatic continuation of ordinary RFB optimization.

If real product evidence requires video-like or full-screen high-motion behavior beyond the qualified RFB profile, admit a separate high-motion or media capability with an end-to-end transport and client story.

Hardware encoding alone is not an end-to-end product.

## 19. Regression policy

Performance is a long-lived product property, not a one-time milestone.

Later changes touching capture, Runtime scheduling, Core frame lifecycle, encoding, RFB client state, networking, graphics backends, or multi-viewer behavior are evaluated against the applicable accepted baseline.

Rules:

- compare an exact candidate SHA against an accepted exact-SHA baseline;
- record the environment and workload with every number;
- use P50/P95 and P99 where useful, not averages alone;
- correctness or boundedness regressions are hard failures;
- classify a material performance regression even when functional tests are green;
- do not widen thresholds merely to make a regression pass;
- when a feature trades performance for user value, record the Product Owner decision and new accepted envelope explicitly.

Initial automated policy:

> A reproducible degradation greater than 10% in a comparable primary metric is investigation-worthy. The hard release boundary remains the published SLO or resource budget for the qualified profile.

The 10% figure is not a universal release-failure threshold. It is an early warning against silent drift.

### 19.1 Proactive CPU efficiency and hotspot-led optimization (#609)

An operator has observed visibly higher CPU use during remote access. **CPU efficiency is proactive engineering under #370**, unlike the separately feedback-triggered weak-network specialization. Establish credible portable measurements and improve measured bottlenecks before considering GPU/private backends or a newly numbered Feature.

Historical signal only: a **480x240 static Windows 11 / Qt6.8.3 Widgets** host connected to a real GUI viewer consumed around **6.75 CPU seconds over 5 wall seconds (~135% of one core)** before capture pacing; at 60-FPS pacing, short static observations were about **1.3–3.4% of one core**. These older, specific-SHA five-second samples do not establish current sustained Host CPU cost or a universal budget. See [Windows real-viewer performance](../internal/windows-real-viewer-performance-2026-10-22.md).

**Fix measurement validity first:** the original `test_rfb_wire_robustness.cpp` `cpu_ms` used `std::clock()`, which on MSVC measures wall time, not process CPU. #609 replaces the Windows source with user+kernel process CPU from `GetProcessTimes`, retaining `cpu_ms` and adding `wall_ms` and `cpu_one_core_percent` (100% is one full logical core; multithreaded use can exceed 100%). The synthetic RFB profile covers the **whole test process including fixture/client/transport**, not the installed Qt Host, actual Viewer or encoder alone. Historical Windows `cpu_ms` samples using the old clock are not comparable to corrected CPU timing.

Follow-on #609/#374 measurements should cover an installed Windows x86_64 Qt Host **local-only baseline → zero viewers → connected unchanged/static → W2 localized → W3 interactive → W4 high motion → sustained soak**. Separate Host and Viewer CPU/wall time, normalized one-core-equivalent usage, RSS, resolution, bytes, FPS/frame age, P50/P95 user latency and local Qt responsiveness. Label physical environment, viewer and exact SHA; a single five-second run is not sustained assurance.

Profile before changing code: prioritize avoiding no-viewer/static duplicate captures and unnecessary wakeups, keep bounded capture/backpressure, then measure full-frame comparisons, pixel conversion/copying, TRLE encode, allocations and per-viewer duplicated work. Every fix needs **same-workload before/after CPU plus freshness and local-UI evidence**; reduced CPU with unusable interaction is not success. #123 still requires a proven portable-baseline SLO/resource blocker before any private/GPU acceleration. No speculative weak-network/extra-platform RC/GA release gate is created.

## 20. Machine evidence and GitHub Workflow

GitHub Workflow is the default evidence path when it can reproduce the behavior honestly.

Suitable hosted evidence:

- deterministic or synthetic input-to-visible latency fixtures;
- encoder benchmarks;
- damage and incremental correctness;
- stale-work and coalescing bounds;
- Continuous Updates, Fence, and fallback protocol behavior;
- slow-viewer and multi-viewer isolation;
- memory, queue, and backlog bounds;
- controlled network shaping where runner capabilities permit;
- Windows and Linux x86 performance regressions.

Performance artifacts should be machine-readable and record candidate, environment, workload, strategy, and result.

See #374 for the regression-gate implementation programme.

## 21. Physical and reference evidence

Hosted CI must not replace physical evidence when hardware or graphics reality is part of the claim.

Real-environment validation is required for claims involving:

- RK3588 or another declared ARM64 reference board;
- real EGLFS or Wayland graphics stacks;
- GPU, readback, or BSP behavior;
- local physical HMI display and input responsiveness;
- board-specific CPU, GPU, and resource budgets;
- network or device behavior that CI cannot reproduce faithfully.

When physical validation is required, the task states the exact environment, steps, and evidence that a Human or CodeBuddy must return.

## 22. Issue map

Current authority and work split:

- **#370:** long-lived performance, SLO, and regression programme;
- **#343:** product and competitive baseline consumes the performance contract;
- **#261:** select the practical compression and update profile;
- **#144:** implement compressed, incremental, and damage delivery;
- **#371:** Continuous Updates, Fence, and bounded per-viewer RFB delivery flow;
- **#372:** demand-driven, input-triggered, and adaptive Shared Runtime capture;
- **#374:** machine-readable performance evidence and regression gates;
- **#9:** final V1 production performance qualification;
- **#236/#18/#154:** physical ARM64 and EGLFS performance evidence;
- **#123:** post-baseline platform or vendor acceleration only after measurement;
- **#180:** deeper metrics only when a concrete operational or performance diagnosis needs them.

## 23. Version placement

- **V0.2** remains First User Trial; do not derail its critical path with the full programme.
- **V0.3 family** implements the mandatory practical performance baseline before RC.
- **V0.4** is qualification only; it must not discover missing performance architecture.
- **V1** publishes and fixes qualified support and performance envelopes.
- **Post-GA** feature work continues to run the regression programme so feature accumulation cannot silently erode accepted performance.

## 24. Current status

Do not invent measurements. Update this table only with exact accepted evidence.

| Release or candidate | Reference cell/workload | Exact SHA | Result | Evidence or remaining gap |
| --- | --- | --- | --- | --- |
| Current development baseline | Host-side historical capture research | historical spike SHAs | Partial evidence only | Useful for architecture direction; not an end-to-end product SLO claim |
| V0.3-family practical delivery | W1-W4 / declared desktop and embedded cells | TBD | Pending | #261/#144/#371/#372/#374 |
| V0.4 RC | complete declared RC matrix | TBD | Pending | qualification only |
| V1 GA | qualified support matrix | TBD | Pending | #9 plus physical/reference evidence |

## 25. Maintenance rule

Update this document when an accepted performance strategy, SLO, workload definition, fallback policy, or release performance envelope changes.

The performance programme is long-lived. Completing one optimization does not close the responsibility to prevent later regressions.
