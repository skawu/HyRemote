# HyRemote Performance Optimization Program

> Status: long-lived technical programme. GitHub authority: #370.
>
> This document describes HyRemote's performance architecture, latency contract, optimization order and regression discipline. It is intentionally maintained across releases so later feature work cannot silently erode accepted responsiveness, frame freshness or local Qt behavior.

## 1. Product position

HyRemote is a **low-intrusion remote-access Runtime / SDK for existing Qt applications**. Performance work exists to make remote use responsive without turning performance engineering into a new integration burden for application developers, deployment engineers or remote operators.

Normal users should continue to think in product concepts such as:

- target/application surface;
- listener address/port;
- access/security mode;
- remote input policy;
- start/stop and diagnostics.

They should **not** be required to choose or tune:

- RFB encodings;
- compression levels;
- damage modes;
- capture FPS;
- capture in-flight depth;
- flow-control windows;
- socket buffer sizes;
- GPU/DMA-BUF/vendor-specific backends.

HyRemote therefore optimizes for interactive Qt remote access, not for maximum benchmark throughput or video-streaming FPS.

Performance priority is:

1. remote interaction remains responsive;
2. the viewer receives the newest useful state rather than a backlog of stale frames;
3. local Qt rendering/input remains healthy;
4. static/localized UI avoids unnecessary capture/encode/network work;
5. queues, memory and per-viewer work remain bounded;
6. only after those are satisfied, maximize sustainable delivered cadence.

Canonical principle:

> **Freshness > throughput > frame completeness.**

Dropping/coalescing stale intermediate frames is preferred to delivering every historical frame late.

## 2. Primary user-facing metric: Remote Interaction Latency

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

Define:

```text
T_interaction =
  t(viewer observes first corresponding visual change)
  - t(viewer sends input)
```

Where possible, both timestamps should be recorded on the viewer/test-client side so no cross-machine clock synchronization is required.

Useful segment measurements include:

- input-to-Qt-event latency;
- Qt-event/render-to-capture availability;
- capture request-to-completion latency;
- frame age / PTS age;
- encode latency;
- bytes/update and bytes/time;
- application-level RTT / Fence RTT;
- socket/backlog observations;
- viewer decode/display latency where measurable;
- CPU/GPU/memory and local UI impact.

A useful decomposition is:

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

The purpose of segment budgets is diagnosis: when a candidate misses the end-to-end SLO, the project must identify the dominant component rather than guessing at a lower-level optimization.

## 3. Initial pre-GA reference SLO

For the ordinary interactive Qt UI reference profile:

```text
1920x1080 target
one qualified maintained viewer
direct wired LAN
RTT <= 20 ms
effective available bandwidth >= 100 Mbps
ordinary Widgets/HMI/Form/low-to-moderate-change Quick workload
selected supported RFB delivery profile
```

Initial target:

```text
Interaction latency P50 <= 200 ms
Interaction latency P95 <= 500 ms
```

`500 ms` is an acceptance ceiling for this bounded reference profile. It is **not** a promise for every network, every viewer, every high-motion workload or every unsupported platform.

Every published result must record the exact environment:

- candidate SHA;
- Qt version/anchor;
- OS and CPU architecture;
- QPA / graphics stack;
- UI family;
- HyRemote integration route;
- resolution / DPR;
- viewer/version;
- RTT and effective bandwidth or shaping profile;
- selected encoding/update strategy;
- workload class.

High-motion/media-like workloads may have a different qualified envelope. Ordinary RFB GUI guarantees must not be presented as video-streaming guarantees.

## 4. Standard workloads

Performance evidence is meaningful only when the workload is repeatable.

### W1 — Static

A fully synchronized UI with no visible changes.

Required behavior:

- connection remains alive;
- an incremental request may remain pending;
- no repeated identical framebuffer payload is sent merely to keep the connection alive;
- capture/encode work becomes quiescent or near-zero according to the active policy;
- memory and queued work remain bounded.

### W2 — Localized UI

Examples: button state, text update, small status region, localized widget repaint.

Measure:

- changed region/bytes;
- bytes/update;
- encode latency;
- frame age;
- interaction latency.

This workload validates whether damage/incremental delivery avoids full-screen work when that work would be wasteful.

### W3 — Interactive

Examples: click, typing, focus change, drag or control action that produces a deterministic visible response.

This is the primary P50/P95 interaction-latency workload and validates input-triggered freshness.

### W4 — High Motion

Examples: Qt Quick animation, fade, swipe, large dashboard movement or sustained full-area change.

Measure:

- full-frame compressed behavior;
- frame age/freshness;
- delivered cadence;
- CPU/memory;
- bytes/time;
- local UI impact.

Do not force expensive partial-update analysis when measurement shows the scene is effectively full-frame motion.

## 5. Architecture responsibilities

Performance must preserve HyRemote's one-Core / one-Shared-Runtime architecture.

### 5.1 Core

Core owns only protocol/platform-neutral facts and boundedness:

- `RemoteFrame` ownership and PTS;
- `Damage` = Unknown / FullFrame / Regions;
- bounded capture/frame queue semantics;
- nonblocking transport handoff;
- latest/freshness-first backpressure semantics.

Core must not learn:

- RFB encodings;
- Continuous Updates / Fence protocol objects;
- Qt platform/graphics backend details;
- vendor GPU/encoder APIs;
- network tuning policy.

### 5.2 Shared Runtime

Shared Runtime owns automatic product performance policy shared by C++ / QML / Generic / QPA:

- viewer activity;
- remote-input activity;
- capture demand/pacing;
- adaptive policy coordination;
- product diagnostics/observations where appropriate.

There is no second Widgets, Quick, Embedded or Accelerated Runtime personality.

### 5.3 Qt target adapters

Widgets and Quick adapters own the most appropriate supported capture path and truthful damage capability.

Rules:

- public Qt API first;
- preserve local display/input behavior;
- report damage capability truthfully;
- lower-level/private/platform capture only after measured SLO/resource evidence demonstrates a real blocker.

### 5.4 RFB transport

RFB transport owns protocol/network-specific performance mechanisms:

- encoding negotiation and Raw fallback;
- practical compression;
- incremental/non-incremental request semantics;
- accumulated per-viewer damage/update state;
- Continuous Updates / Fence where interoperable;
- bounded per-viewer flow/backpressure;
- stale encoded-work suppression;
- cursor pseudo-encoding where useful;
- protocol-specific RTT/delivery observations.

## 6. Demand-driven capture

Capture should happen because it improves a connected user's experience, not simply because a periodic timer exists.

Target policy:

```text
0 viewers
-> no unnecessary continuous capture

viewer connected + static scene
-> no repeated identical framebuffer payload

ordinary interaction
-> bounded interactive cadence

remote input occurs
-> prioritize the next fresh result after the corresponding Qt update/render opportunity

sustained high motion
-> stable bounded cadence, not unbounded capture production
```

Connection liveness, capture activity and framebuffer-delivery activity are three separate concerns. A static screen is not a disconnected screen.

## 7. Input-triggered fresh capture

Remote input is an interaction-priority signal.

Preferred behavior:

```text
remote input received
-> delivered to Qt
-> application state/render changes
-> prioritize next fresh capture/update
-> viewer receives newest corresponding state
```

This lowers perceived interaction latency without forcing a permanently high capture FPS on mostly static HMI/configuration interfaces.

The policy must remain bounded: input bursts must not create unbounded capture requests or timers.

## 8. Shallow capture pipeline

Remote access should optimize freshness, not offline throughput.

Historical Quick evidence shows that deeper asynchronous capture pipelines can increase throughput while also increasing request latency, duplicate content and GUI jitter. Interactive defaults should therefore stay shallow, normally around one or two outstanding captures unless an exact reference cell proves another value is better.

A later platform may tune this internally, but normal users do not choose it.

## 9. Stale-work suppression

Expensive work on a frame that is already obsolete hurts latency twice: it consumes CPU and delays the newer useful frame.

Target per-viewer encoder scheduling:

```text
<= 1 update currently encoding/sending
+ <= 1 latest pending useful update
```

If a newer useful frame supersedes pending work before expensive encoding begins, replace/drop the stale pending work.

Do not build a historical framebuffer playlist.

## 10. Adaptive ordinary-GUI vs high-motion delivery

HyRemote uses two complementary strategies rather than one universal update mode.

### 10.1 Region path

For static/localized ordinary Qt UI:

```text
capture/change observation
-> accumulated pending damage
-> viewer requested region intersection
-> encode/send changed rectangles
```

This is the preferred path for forms, controls, ordinary Widgets/HMI screens and low-change Quick interfaces when the damage savings are material.

### 10.2 Full-frame path

For sustained large/full-area motion:

```text
latest full frame
-> practical compression
-> bounded delivery
```

This avoids spending excessive CPU on damage analysis when nearly every pixel changes anyway.

### 10.3 Automatic selection

Runtime/transport may use measured change ratio and hysteresis to choose the appropriate path. This is an implementation detail, not a normal-user `DamageMode` setting.

## 11. Compression policy

Before GA:

- Raw remains a deterministic interoperability fallback;
- at least one practical maintained-viewer-compatible compressed path is required;
- selection is based on **end-to-end latency/resource behavior**, not compression ratio alone.

Candidate comparison should include:

```text
encode P50/P95
wire bytes / serialization time
viewer compatibility/decode behavior
CPU/memory cost
final interaction latency and frame age
cross-platform dependency/package cost
```

A fast LAN may favor lower compression effort because encoding dominates. A constrained network may favor stronger compression because wire time dominates. The Runtime/transport should make the normal decision automatically rather than requiring users to understand compression internals.

Tight/JPEG or another lossy profile should be added only if representative high-motion Quick/embedded evidence shows the selected ordinary lossless profile is insufficient.

## 12. Correct incremental semantics

Per-viewer state must include the smallest bounded facts required for correct RFB updates:

- requested region;
- incremental/non-incremental mode;
- pending accumulated damage;
- resize/full-refresh invalidation;
- negotiated encoding capability/order;
- outstanding request/update/continuous state;
- bounded coalescing/reset state.

Correctness property:

```text
A/B/C change while viewer is not ready
-> next eligible incremental update contains all still-required change
   inside the viewer's requested region
```

For `incremental=true` and no damage:

- keep the request pending;
- send no redundant framebuffer payload;
- later required damage wakes/satisfies the request.

For `incremental=false`:

- return the current requested content even when it matches previously delivered content.

Resize or format invalidation forces a correct refresh.

## 13. Connection liveness is not framebuffer traffic

A static session may legitimately produce no framebuffer payload for a long time while remaining connected.

Do not use repeated duplicate full frames as a keepalive.

Where liveness support is required, use appropriate TCP keepalive and/or lightweight RFB synchronization such as Fence where interoperable. Network liveness policy remains independent of framebuffer change detection.

## 14. RFB latency control

### 14.1 Continuous Updates

When a maintained viewer supports RFB Continuous Updates, HyRemote should use it to avoid requiring a new FramebufferUpdateRequest round trip for every update.

Unsupported viewers fall back automatically to standard request-driven behavior.

### 14.2 Fence

Fence may be used where supported for stream synchronization and bounded application-level RTT observation.

Protocol objects remain transport-private.

### 14.3 Per-viewer flow control

`socket bytesToWrite()==0` is useful but does not prove that no stale framebuffer data remains in kernel/network/proxy/viewer buffers.

A bounded per-viewer controller may observe:

- base/current/Fence RTT;
- recent bytes/update;
- encode duration;
- socket backlog;
- frame age;
- outstanding delivery state;
- Continuous Updates state.

Its goal is to minimize stale framebuffer age in the end-to-end path, not to maximize TCP throughput.

HyRemote must **not** implement a second TCP congestion-control algorithm. TCP manages packet congestion; HyRemote decides whether an old framebuffer is still worth sending.

### 14.4 Multi-viewer isolation

Encoding negotiation, pending damage, RTT/flow state and delivery windows are per viewer. A slow or high-RTT viewer must not force a fast LAN viewer to receive stale frames.

## 15. Cursor and perceived latency

Evaluate RFB cursor pseudo-encoding so a supporting viewer can render the cursor locally rather than waiting for a framebuffer round trip for every pointer movement.

This is a transport-private optimization with automatic fallback, not an application API.

## 16. Automatic fallback model

Every optimization is additive. Unsupported capability falls back to a correct lower-capability path:

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

Do not start DMA-BUF/RKMPP/VAAPI/D3D/private-RHI work merely because it could be faster.

First implement and measure the portable/product baseline:

- demand-driven capture;
- input-triggered freshness;
- practical compression;
- incremental/damage;
- stale-work suppression;
- Continuous/Fence where selected;
- bounded per-viewer flow control;
- adaptive capture policy.

Only a measured reference-cell failure against this programme may activate bounded platform acceleration through #123.

Valid activation evidence includes:

- capture/readback P95 consumes the latency budget;
- CPU frame copy materially harms the local HMI;
- encoder P95 is the dominant blocker on a declared ARM64/desktop cell;
- power/CPU/GPU budget cannot be met by the portable baseline.

Optimized backends remain private/replaceable and preserve the same public integration semantics.

## 18. High-motion/media boundary

H.264/H.265/AV1 is not an automatic continuation of ordinary RFB optimization.

If real product evidence requires video-like or full-screen high-motion behavior beyond the qualified RFB profile, admit a separate high-motion/media capability with an end-to-end transport/client story.

Hardware encoding by itself is not an end-to-end product.

## 19. Regression policy

Performance is a long-lived product property, not a one-time milestone.

Later changes touching capture, Runtime scheduling, Core frame lifecycle, encoding, RFB client state, networking, graphics backends or multi-viewer behavior must be evaluated against the applicable accepted baseline.

Rules:

- compare exact candidate SHA against an accepted exact-SHA baseline;
- record environment and workload with every number;
- use P50/P95 and P99 where useful, not averages alone;
- correctness/boundedness regressions are hard failures;
- a material performance regression must be classified even when functional tests are green;
- do not widen thresholds merely to make a regression pass;
- if a feature intentionally trades performance for user value, record the Product Owner decision and new accepted envelope explicitly.

Initial automated policy:

> A reproducible degradation greater than 10% in a comparable primary metric is investigation-worthy. The hard release boundary remains the published SLO/resource budget for the qualified profile.

The 10% number is not a universal release failure threshold; it is an early warning against silent drift.

## 20. Machine evidence and GitHub Workflow

GitHub Workflow is the default evidence path when it can reproduce the behavior honestly.

Suitable hosted evidence includes:

- deterministic/synthetic input-to-visible latency fixtures;
- encoder benchmarks;
- damage/incremental correctness;
- stale-work/coalescing bounds;
- Continuous/Fence/fallback protocol behavior;
- slow-viewer and multi-viewer isolation;
- memory/queue/backlog bounds;
- controlled network shaping where runner capabilities permit it;
- Windows/Linux x86 performance regressions.

Performance artifacts should be machine-readable and record candidate/environment/workload/strategy/results.

See #374 for the regression-gate implementation programme.

## 21. Physical/reference evidence

Hosted CI must not substitute for physical evidence when hardware/graphics reality is part of the claim.

Real-environment validation is required for claims involving:

- RK3588 or another declared ARM64 reference board;
- real EGLFS/Wayland graphics stacks;
- GPU/readback/BSP behavior;
- local physical HMI display/input responsiveness;
- board-specific CPU/GPU/resource budgets;
- network/device behavior that cannot be reproduced faithfully in CI.

When physical validation is required, the task must state the exact environment, steps and evidence Human/CodeBuddy must return.

## 22. Issue map

Current authority/work split:

- **#370** — long-lived performance/SLO/regression programme;
- **#343** — product/competitive baseline consumes the performance contract;
- **#261** — select practical compression/update profile;
- **#144** — implement compressed/incremental/damage delivery;
- **#371** — Continuous Updates, Fence and bounded per-viewer RFB delivery flow;
- **#372** — demand-driven/input-triggered/adaptive Shared Runtime capture;
- **#374** — machine-readable performance evidence and regression gates;
- **#9** — final V1 production performance qualification;
- **#236/#18/#154** — physical ARM64/EGLFS performance evidence;
- **#123** — only measured post-baseline platform/vendor acceleration;
- **#180** — deeper metrics only when a concrete operational/performance diagnosis needs them.

## 23. Version placement

- **V0.2** remains First User Trial; do not derail its critical path with this full programme.
- **V0.3 family** implements the mandatory practical performance baseline before RC.
- **V0.4** is qualification only; it must not discover missing performance architecture.
- **V1** publishes/fixes qualified support and performance envelopes.
- **Post-GA** feature work continues to run the regression programme so feature accumulation cannot silently erode accepted performance.

## 24. Current status table

Do not invent measurements. Update this table only with exact accepted evidence.

| Release/candidate | Reference cell/workload | Exact SHA | Result | Evidence / remaining gap |
| --- | --- | --- | --- | --- |
| Current development baseline | Host-side historical capture research | historical spike SHAs | Partial evidence only | Useful for architecture direction; not an end-to-end product SLO claim |
| V0.3-family practical delivery | W1-W4 / declared desktop + embedded cells | TBD | Pending | #261/#144/#371/#372/#374 |
| V0.4 RC | complete declared RC matrix | TBD | Pending | qualification only |
| V1 GA | qualified support matrix | TBD | Pending | #9 + physical/reference evidence |

## 25. Maintenance rule

This document must be updated when an accepted performance strategy, SLO, workload definition, fallback policy or release performance envelope changes.

The performance programme is intentionally long-lived. Completing one optimization does not close the responsibility to prevent later regressions.
