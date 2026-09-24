# HyRemote Product Test Strategy

Status: **canonical product-test architecture**

Authority: #393

This document defines how HyRemote proves product quality over the lifetime of the project. It is intentionally organized around the **product vision, product acceptance contracts, product risks and support claims**, not around the current CTest inventory or the current implementation details.

`tests/TEST_CATALOG.md`, `tests/TEST_MATRIX.md` and `tests/EXECUTION_BASELINE.md` describe the current executable inventory and its present selection/registration facts. They are important execution records, but they do not make the current number or shape of CTest identities a permanent architecture constraint. This strategy is the long-lived authority for deciding what evidence the product needs, how expensive that evidence may be, and where that evidence belongs.

A design document, test program, workflow, checklist or empty test registration is **not evidence by itself**. Product evidence exists only when the applicable test/qualification action actually executes against the identified product build and records a result.

## 1. Product vision under test

HyRemote is a low-intrusion remote-access Runtime / SDK for existing Qt applications. Its product goal is:

> Add reliable, secure and responsive remote viewing and optional remote control to an existing Qt application with minimal integration burden, while preserving the application's native local display, local input, lifecycle and platform behavior, and without exposing remote-desktop implementation mechanics as normal application configuration.

The test system therefore exists to prove product outcomes, not merely source-code correctness.

The stable application model is one Core, one Shared Runtime and four peer integration frontends:

- C++ API;
- QML API;
- Generic Plugin;
- QPA.

Widgets and Qt Quick are Runtime target/surface dimensions, not separate products. RFB is the current correctness transport, not the definition of the HyRemote product. Qt 6.8.3, qwindows/qxcb, CPU-readable capture and later graphics/acceleration backends are qualification/implementation choices beneath the product contracts.

A future transport, capture backend, graphics path, embedded platform or hardware acceleration path must preserve the same product contracts unless the product contract is intentionally changed by an explicit architecture/product decision.

## 2. Mandatory design principles

The test architecture follows these rules in priority order.

### 2.1 Product-contract driven

Every permanent product test must protect a named product acceptance contract or an explicit support claim. A test that cannot explain the user-visible/product risk it protects should not automatically become a permanent product gate.

### 2.2 Risk proportionality

The scope and cost of testing must be proportional to the risk introduced by the change. A documentation-only change must not trigger graphics qualification; a QML mapping change must not automatically rebuild every QPA deployment fixture; a deployment-runtime-resolution change must run real Windows/Linux deployment evidence even if all Core unit tests are unaffected.

### 2.3 Cheapest sufficient evidence

Use the lowest-cost deterministic layer that can honestly prove the property:

1. model/contract check before a process-level test;
2. unit/component test before a full application;
3. synthetic application before a real-world application when the property is implementation-local;
4. hosted/headless evidence before physical evidence when host-native behavior is not part of the claim;
5. physical/real-environment evidence when display, input, GPU, loader, native platform, viewer or network behavior is part of the claim.

Cheap evidence must not be substituted for stronger evidence when the product claim requires the stronger environment.

### 2.4 Development velocity is a first-class constraint

The test system must not become the project's development bottleneck. Quality is protected by staged evidence and risk-based selection, not by running the whole qualification universe on every commit.

The project therefore treats test execution cost as an architectural budget, not as an incidental CI concern.

### 2.5 No duplicate proof across peer frontends

Shared Runtime/Core semantics are proved once at the shared layer. C++/QML/Generic/QPA frontend tests prove only frontend-specific mapping, activation, packaging and native-platform behavior, plus a bounded number of vertical product paths proving that each frontend is actually connected to the shared product implementation.

### 2.6 Cases are not automatically test identities

A new input combination, negative configuration or release profile is normally a new **case** in an existing suite. It becomes a new CTest identity only when independent selection, capability registration, platform scheduling, isolation, timeout or evidence ownership materially requires a separate identity.

### 2.7 Support claims require exact evidence

A `Supported`/`Limited` compatibility row requires explicit qualification evidence for the exact declared product/environment boundary. A nearby Qt patch, another OS, a similar viewer, a different graphics backend or a headless CI result does not expand the support claim by inference.

### 2.8 Local/native behavior is part of the product

HyRemote adds remote access to a native Qt application. A test system that proves remote pixels while ignoring the application's native local display/input behavior is incomplete for claims that promise local+remote coexistence.

### 2.9 Failures must be diagnosable

A failing suite must identify the violated product contract/case and provide enough evidence to classify the defect. Large opaque aggregate tests and large collections of fragmented identities are both undesirable when they make failure meaning ambiguous.

### 2.10 Test architecture is also maintained

Runtime, flake rate, duplicate coverage, obsolete cases and support-matrix value are reviewed over time. Tests can be consolidated, moved to a different gate or removed when another cheaper/stronger proof supersedes them. The existence of a historical test is not by itself a reason for permanent execution.

## 3. Product Acceptance Contracts (PAC)

The highest-level test taxonomy is nine Product Acceptance Contracts. Every product-quality test maps to at least one PAC.

| Contract | Product promise | Typical failure meaning |
| --- | --- | --- |
| **PAC-1 Low Intrusion** | Existing Qt applications gain remote access without being rebuilt around HyRemote internals. | Integration burden or implementation details leak into the application. |
| **PAC-2 Native Non-interference** | Local display, local input, native platform behavior and application lifecycle remain healthy while remote access is active. | HyRemote damages the host application it is meant to augment. |
| **PAC-3 Remote Experience** | Remote view/control reflects the current application state correctly and input semantics remain safe. | The remote product is functionally wrong or unsafe to operate. |
| **PAC-4 Frontend Equivalence** | C++/QML/Generic/QPA are peer entries into one Shared Runtime with the same product semantics. | A frontend becomes a separate product/runtime personality. |
| **PAC-5 Deployability** | A built/installed/deployed application runs from its own tree without SDK/build-tree/runtime-path crutches. | CI build success does not produce a usable delivered product. |
| **PAC-6 Reliability & Boundedness** | Slow, malformed, disconnecting or repeated clients cannot create unbounded resource growth or unsafe teardown. | Remote activity can destabilize the host process. |
| **PAC-7 Security Truth** | Requested security policy is either actually established or fails closed; no silent downgrade or secret leak occurs. | Security behavior is weaker than the product claim. |
| **PAC-8 Responsiveness & Efficiency** | Remote interaction is fresh/responsive while native Qt behavior and resource budgets remain healthy. | The product technically works but is not operationally usable. |
| **PAC-9 Compatibility Truth** | Every support statement is backed by environment-specific evidence and is not broadened by inference. | Documentation/support promise exceeds verified product reality. |

### 3.1 PAC-1 — Low Intrusion

Required product properties include:

- a normal C++ application integrates through the public `HyRemote::RemoteAccess` facade rather than internal modules;
- a QML application uses the public `HyRemote` module rather than backend types;
- a Generic consumer remains Qt-only at the application linkage level and activates HyRemote through Qt's generic-plugin mechanism;
- a QPA consumer remains Qt-only at application source/link level and enters through `-platform hyremote`;
- applications use `hyremote_deploy()` rather than manually discovering/copying Core, capture, input, RFB, plugin or backend files;
- normal applications do not tune transport encodings, queue depths, capture FPS, platform buffers or hardware backends as part of ordinary integration;
- public headers/configuration do not leak transport/private-platform implementation types.

Evidence should include minimal clean consumers and representative real-world applications. Tiny synthetic fixtures prove API/package contracts; they do not alone prove the low-intrusion claim for realistic applications.

### 3.2 PAC-2 — Native Non-interference

The defining product invariant is:

```text
existing Qt application + HyRemote
= existing Qt application behavior + remote access
```

For supported cells, qualification must exercise applicable native behavior before, during and after remote access:

- visible local rendering;
- local pointer, keyboard and text input;
- resize/focus;
- dialogs, menus, popups and supported multi-window behavior;
- applicable GPU/OpenGL/graphics behavior;
- application shutdown;
- HyRemote stop while the application keeps running;
- slow/stalled/disconnected viewer without indefinite native UI/render blockage.

Hosted offscreen/Xvfb evidence may complement these checks but cannot replace a support claim whose meaning includes physical/native local behavior.

### 3.3 PAC-3 — Remote Experience

Remote-view evidence covers, as applicable:

- correct content and geometry;
- DPR/scaling semantics;
- resize;
- supported surface create/show/hide/destroy;
- popup/dialog/multi-window composition;
- newest useful state rather than stale backlog;
- reconnect without rebuilding the application Runtime.

Remote-control evidence covers:

- pointer movement/buttons;
- wheel/scroll;
- keys/modifiers;
- committed text where the transport supplies sufficient information;
- focus/routing;
- drag/held-state behavior.

Terminal input-state safety must be proved for applicable paths:

- abrupt viewer disconnect;
- explicit Runtime stop;
- target/surface destruction;
- policy disable/transition;
- process teardown.

No supported terminal transition may leave synthetic held key/button state stuck in the application or deliver forbidden late queued input after terminal stop.

### 3.4 PAC-4 — Frontend Equivalence

The Shared Runtime owns common product behavior once. Frontend tests prove the mapping into that behavior:

| Frontend | Unique test responsibility |
| --- | --- |
| C++ | facade construction/config/start/stop/state/error mapping |
| QML | import/type/property/binding/component lifecycle/thread-visible notification mapping |
| Generic | Qt-only linkage, plugin activation/config mapping, native platform remains authoritative |
| QPA | factory trampoline, exact private ABI, native delegate preservation, QPA-specific surface interception |

The following semantics are primarily Shared Runtime/Core/transport evidence and should not be independently reimplemented as complete frontend suites: frame lifetime, RFB malformed-packet handling, generic held-input state machine, listener policy, queue boundedness, security-policy semantics and common performance scheduling.

A small number of frontend vertical E2E cells prove that each entry route is truly wired to the product, but they do not duplicate the entire shared behavior matrix.

### 3.5 PAC-5 — Deployability

A product deployment is valid only when the delivered application can run from its own deployment tree without accidental assistance from:

- the HyRemote source tree;
- the HyRemote build tree;
- the original HyRemote SDK path;
- a Qt SDK plugin/runtime path used as a runtime crutch;
- manually copied internal implementation files.

The canonical product evidence path is:

```text
clean environment
  -> selected qualified Qt SDK
  -> HyRemote configure/build/install
  -> clean consumer acquisition
  -> consumer configure/build/install
  -> hyremote_deploy()
  -> relocate/isolate deployed tree
  -> construct minimal runtime environment
  -> launch
  -> connect viewer
  -> view/control where applicable
```

Deployment changes are platform-sensitive product changes. Windows DLL/plugin discovery and Linux ELF/RUNPATH/loader behavior are different and require their own evidence.

On Linux, the presence of distribution Qt runtimes must not influence HyRemote's selected development/deployment Qt identity. The selected qualified Qt SDK is authoritative for the product's Qt lineage; foreign loader-visible Qt installations are environmental noise, not alternate development SDK candidates.

The Deployment Qualification Suite covers positive and negative cases, including missing optional payloads, incompatible QPA Qt private ABI, illegal frontend combinations, source-vs-installed acquisition isolation, relocation and runtime closure. Related configurations should normally be table-driven cases inside a bounded number of suites rather than one CTest identity per input variant.

### 3.6 PAC-6 — Reliability & Boundedness

HyRemote is a long-running Runtime embedded in another application. Remote activity must not destabilize the host.

Deterministic invariants include:

- frame ownership lifetime;
- bounded Core/frame/input queues;
- latest/freshness-first overload behavior;
- deterministic teardown ownership;
- cancellable startup;
- callback lifetime gating;
- exception containment;
- target destruction safety;
- slow transport/client isolation;
- per-viewer bounded delivery state.

Long-duration/repetition evidence complements deterministic tests:

- repeated start/stop;
- repeated viewer connect/disconnect;
- window/surface churn;
- resize churn;
- input flood;
- slow/stalled clients;
- multiple viewers;
- idle soak;
- active soak.

Soak/stress evidence records RSS, thread count, FD/handle count, socket count, queue/backlog observations and crashes/hangs where applicable. Long-duration tests are not ordinary PR blockers unless a specific change requires them; they belong primarily to nightly/weekly/release qualification.

### 3.7 PAC-7 — Security Truth

The stable security product rule is policy-oriented rather than tied to one authentication implementation:

```text
requested security policy
  -> required capability present?
  -> configuration valid?
  -> requested protection actually established?
  -> only then may the listener/session become available
```

If any required condition fails, the product must fail closed with no silent downgrade, no partial listener, no fallback to a weaker profile and no secret leakage.

Security evidence includes:

- safe defaults;
- missing/invalid/unavailable capability cases;
- correct/wrong credential behavior where applicable;
- downgrade resistance;
- descriptor/config parsing;
- listener exposure semantics;
- malformed/stalled/oversized transport clients;
- resource/admission bounds;
- secret redaction/logging policy;
- dependency/runtime provenance where relevant.

RFB VNC Authentication, future encrypted transport, certificate handling or another future transport/security mechanism are implementation suites beneath this contract.

### 3.8 PAC-8 — Responsiveness & Efficiency

The primary user metric is **Remote Interaction Latency**, not capture FPS or encoder throughput in isolation:

```text
viewer input
  -> network / transport input
  -> Runtime/Core/input adapter
  -> Qt application update/render
  -> capture
  -> scheduling/encode/delivery
  -> viewer observes corresponding visual change
```

The performance programme defines the current reference profile and SLO. Segment metrics such as input-to-Qt latency, capture latency, frame age, encode duration, bytes/update, CPU/GPU/memory and local UI impact are diagnostic metrics used to explain the end-to-end result.

Performance evidence also protects:

- newest useful state over stale backlog;
- local Qt responsiveness;
- quiescent/static efficiency;
- bounded memory/queues;
- slow-viewer isolation;
- multi-viewer isolation;
- automatic fallback when an optimization is unavailable.

Performance regression monitoring compares exact candidate/build/environment records against an accepted baseline. A functional-green result does not hide a material performance regression.

### 3.9 PAC-9 — Compatibility Truth

The product's compatibility matrix is an evidence-backed statement, not an inference table.

Each supported/limited row must identify enough of the qualification boundary to reproduce the claim, including as applicable:

- candidate/release identity;
- OS/architecture;
- Qt version;
- integration frontend;
- surface/UI family;
- native platform/delegate;
- graphics backend/DPR where material;
- deployment form;
- maintained viewer/version;
- security profile;
- network/performance reference profile;
- qualification result/date/evidence location.

Public-Qt frontends and QPA follow different compatibility rules: public API compatibility may be qualified by supported Qt-family/version ranges, while QPA private ABI qualification is per exact Qt patch/platform combination.

No test result on Windows substitutes for Linux, no desktop x86 result substitutes for embedded, and no basic Widgets/Quick result qualifies unrelated graphics combinations.

## 4. Evidence classes

HyRemote uses four evidence classes. These classes are more important than traditional unit/integration/E2E naming.

| Evidence class | Question answered | Typical execution |
| --- | --- | --- |
| **V — Verification** | Does the implementation satisfy its deterministic design contracts? | developer/PR |
| **P — Product Validation** | Can a user complete the affected real product path? | affected PR/nightly |
| **Q — Qualification** | May this exact environment/product cell be described as Supported/Limited? | nightly/reference/physical qualification |
| **R — Release Acceptance** | May this exact frozen candidate be released with the stated product claims? | RC only |

### 4.1 Verification (V)

Verification includes model, unit, component and focused integration evidence. It should be deterministic, fast and highly diagnosable.

Examples:

- Core state/ownership/backpressure contracts;
- Runtime policy/model tests;
- RFB parser/handshake/protocol correctness;
- Widgets/Quick adapter contracts;
- frontend mapping tests;
- static package/API boundary checks.

### 4.2 Product Validation (P)

Product validation exercises a real vertical path through product artifacts:

- configure/build/install;
- clean consumer;
- deployment;
- launch;
- viewer connection;
- view/control/reconnect/stop where applicable.

It proves usability of the product path, not the support status of every environment.

### 4.3 Qualification (Q)

Qualification runs on the actual environment characteristics needed by a support claim. It includes platform/native display/input/graphics/loader/viewer/performance facts that hosted synthetic CI cannot honestly infer.

Qualification may be automated on dedicated reference hosts, semi-automated with recorded evidence, or manual only where automation would change the property being observed. Manual/physical evidence must still be reproducible, candidate-bound and recorded.

### 4.4 Release Acceptance (R)

Release acceptance consumes already-defined verification/validation/qualification requirements against one exact RC-FROZEN candidate. Physical evidence and hosted build/install/deploy evidence are complementary and cannot substitute for each other.

A release check should not rediscover product scope. It confirms that the exact candidate retains the already-defined product contracts and support evidence.

## 5. Technical evidence ownership

Product contracts map down into technical layers so that evidence is collected at the cheapest correct seam.

### 5.1 Core

Core proves transport/platform/UI-neutral invariants:

- Session lifecycle and concurrency;
- frame/storage ownership;
- timing and damage semantics;
- bounded mailbox and freshness/backpressure;
- normalized input abstractions;
- deterministic teardown/start cancellation;
- callback lifetime/exception containment;
- capability compatibility.

Core tests must not grow Qt frontend, native platform, deployment or RFB-wire knowledge merely to increase coverage numbers.

### 5.2 Shared Runtime

Shared Runtime owns the majority of product-semantic integration evidence:

- Qt-aware lifecycle;
- Widgets/Quick target selection;
- automatic surface discovery/composition;
- listener/network policy;
- security-profile policy;
- client lifecycle/notifications;
- input routing/admission;
- capture demand/pacing;
- common diagnostics;
- common performance policy.

When behavior is intentionally identical across all frontends, prefer proving it here instead of four times at frontend level.

### 5.3 Widgets / Quick target adapters

Adapters prove the Qt surface contract:

- capture correctness/lifetime;
- resize/DPR;
- truthful damage capability;
- target loss;
- input routing/focus/text;
- asynchronous Quick capture bounds where applicable.

They do not own listener, authentication, package or common Runtime lifecycle semantics.

### 5.4 Frontends

Frontends prove only their unique boundary plus a minimal end-to-end connection to Shared Runtime.

The frontend test count should remain bounded even as Shared Runtime behavior grows.

### 5.5 Transport

Transport evidence is split into:

1. **transport-neutral contract evidence** — connection lifecycle, frame delivery, normalized input return, bounded flow, disconnect/error reporting;
2. **transport-specific evidence** — for current RFB: RFB 3.8 handshake, encodings, update semantics, Continuous Updates/Fence, malformed input, VNC Authentication, viewer interoperability.

This separation keeps future transports from forcing a rewrite of the entire product test architecture.

### 5.6 Packaging/deployment

Packaging/deployment owns consumer-visible artifact closure, install/export metadata, optional payload selection, relocation, runtime-origin correctness and clean-environment launch.

Deployment evidence is a product test, not merely a CMake helper test.

### 5.7 Repository/release governance

Branch naming, documentation-path checks, release-profile syntax, CI classifier self-tests and repository layout/license-policy checks may be mandatory engineering controls, but they are tracked separately from product-quality evidence.

Do not report governance-check counts as if they were remote-access product coverage.

## 6. Execution gates and velocity budgets

Expensive evidence is staged so that ordinary development remains fast while release confidence remains high.

| Gate | Purpose | Default target wall time | Evidence |
| --- | --- | ---: | --- |
| **G0 Developer** | Immediate local implementation feedback | **<= 10 s** | changed-module focused V |
| **G1 PR Verification** | Prevent deterministic logic/integration regressions | **<= 90 s target** | affected V on required OS/capability |
| **G2 PR Product** | Prove affected vertical product/deployment path | **<= 3–5 min target** | selected P, platform-specific where required |
| **G3 Merge Sentinel** | Detect broken mainline integration without replaying PR qualification | **<= 60 s target** | minimal nonzero health sentinel |
| **G4 Nightly Qualification-lite** | Broader cross-product/platform evidence | **<= 30 min target** | broad P + selected Q + stress/perf-lite |
| **G5 Weekly/Extended** | Expensive robustness/performance/fuzz/repetition | not a PR budget | extended V/P/Q |
| **G6 Release Qualification** | Accept the exact frozen candidate | evidence completeness, not minute budget | complete required R/Q |

These are architectural **budgets**, not permission to skip required correctness evidence. If a gate regularly exceeds its budget, first improve selection, parallelism, fixture architecture, caching or test identity granularity. Do not silently weaken product evidence just to make the timer green.

### 6.1 G0 Developer

Run the changed component and direct contract dependencies. Normal local edit/compile/test loops should not reinstall the full SDK or launch a real viewer unless the developer is actively working on that product path.

### 6.2 G1 PR Verification

G1 is risk-selected. It may execute on both Windows/Linux when the affected behavior is platform-sensitive, but pure deterministic model/static checks need not be duplicated across operating systems.

### 6.3 G2 PR Product

G2 is activated for changes whose risk crosses a user/product boundary, including as applicable:

- public API/frontend integration;
- install/export/package metadata;
- deployment helpers/runtime dependency resolution;
- native plugin loading;
- security/transport behavior;
- canonical examples/user adoption path.

A deployment/loader change requires Windows/Linux product evidence even if G1 deterministic tests are platform-neutral.

### 6.4 G3 Merge Sentinel

A merge/push sentinel confirms that the integrated branch is not obviously broken and that the selector cannot produce a false-green zero-test run. It does not replay an already-passed full PR matrix.

### 6.5 G4/G5

Nightly/weekly gates carry breadth and expensive repetition that would damage daily throughput:

- all peer frontend vertical paths;
- clean install/deploy/relocation;
- maintained viewer matrix subset;
- network impairment subset;
- stress-lite/full stress;
- performance baseline/trend;
- fuzz/protocol robustness;
- repeated connect/disconnect/start/stop;
- broader Qt/graphics cells as defined by the support plan.

### 6.6 G6 Release Qualification

Release qualification uses an exact frozen candidate and completes all required support evidence including real/native Windows/Linux cells, package/deployment evidence, security boundary, performance SLO, viewer/product path, compatibility disposition and required soak/stress results.

## 7. Risk-based selection model

Selection is based on **risk propagation**, not merely filename proximity and not a blanket full-suite rule.

A change classifier maps changed areas into risk domains, then risk domains into required suites/gates.

Example policy:

| Change domain | Required evidence | Evidence normally not implied |
| --- | --- | --- |
| Core frame/mailbox/lifecycle | Core V + affected Runtime/transport V | QML deploy matrix, release docs |
| Shared Runtime policy/model | Runtime V + affected adapter/frontend vertical smoke | unrelated package negative cases |
| Widgets/Quick adapter | adapter V + applicable frontend vertical paths | unrelated security package cases |
| C++ frontend | C++ mapping + affected P | QPA private-ABI qualification |
| QML frontend | QML mapping + affected P | Generic/QPA deployment unless shared package code changed |
| Generic frontend | Generic activation/native-platform V/P | QPA private-ABI cases |
| QPA/native delegate | QPA V + required Win/Linux native/deploy P/Q | QML property tests |
| RFB/transport | protocol V + maintained-viewer P + relevant perf/security | unrelated frontend package negatives |
| security | security V + listener/transport P/Q | unrelated graphics qualification |
| deployment/package | package/deploy V + clean Win/Linux P | full Core unit replay unless shared code changed |
| performance scheduler/capture | affected V + perf smoke, later nightly trend | all release governance |
| docs only | doc/link/product-contract consistency | GUI/viewer/deployment runtime |

Selectors are themselves tested because an incorrect selector can create both false green and unnecessary project drag. A selected product lane must execute a nonzero relevant suite; zero execution is non-evidence.

## 8. Test identity and suite design

### 8.1 Prefer table-driven suites

Use one suite with named cases when cases share setup, ownership and scheduling. Examples include:

- release/profile policy cases;
- deployment negative configurations;
- listener input matrices;
- frontend config parsing;
- security descriptor invalid variants.

Each case must retain precise failure reporting.

### 8.2 Separate identities only for real execution reasons

Create a separate test identity when at least one of the following is materially different:

- platform/capability registration;
- execution environment;
- timeout/cost class;
- isolation requirement;
- external resource/viewer/physical host;
- selector ownership;
- expected result semantics that cannot remain clear inside one suite.

### 8.3 Avoid mega-tests

Consolidation must not create one opaque hour-long test that loses failure isolation. The goal is a smaller number of meaningful suites with explicit named cases, not the smallest possible test count.

### 8.4 Existing inventory migration

The current registered CTest inventory remains valid until focused migration work changes it. Migration should prioritize:

1. keeping Core/Runtime/adapter deterministic coverage;
2. consolidating fragmented policy/governance/profile cases where independent identities add no scheduling value;
3. consolidating deploy-helper positive/negative fixtures where table-driven execution keeps failure meaning;
4. moving repository/release-governance checks out of product-quality counts;
5. ensuring expensive installed/example/product-fit tests are path/risk gated;
6. preserving exact platform-specific tests such as Linux relocation and QPA/native behavior where separate scheduling is meaningful.

Reducing the raw identity count is not a KPI. Reducing duplicate proof and critical-path cost while preserving product evidence is the objective.

## 9. Platform and environment qualification

### 9.1 CI environment versus qualification environment

Hosted CI is optimized for repeatability and fast regression detection. Dedicated/native qualification proves host behavior that hosted/headless environments cannot.

| Environment | Primary role | May independently prove native support? |
| --- | --- | --- |
| hosted Linux + headless/Xvfb | V/P regression | No |
| hosted Windows Server | V/P regression | No, not for all physical/native claims |
| qualified Ubuntu/Linux native desktop | Q/R | Yes for the declared Linux cell |
| qualified Windows desktop | Q/R | Yes for the declared Windows cell |
| physical HiDPI/GPU/multi-display host | targeted Q/R | Yes for declared graphics/display cells |

### 9.2 Windows/Linux are independent claims

Qt plugin loading, graphics, sockets, input, DLL/ELF runtime resolution and native window semantics differ. Passing one OS cannot be substituted for the other when the product claim names both.

### 9.3 Qt qualification

For public-Qt frontends (C++/QML/Generic), qualification may use selected Qt-family/version boundaries defined by the product support policy.

For QPA, each exact Qt private-ABI/platform pair requires explicit qualification. A passing nearby patch does not imply compatibility.

### 9.4 Graphics/surface qualification

Basic Widgets/Quick support does not automatically qualify QOpenGLWidget, QQuickWidget, Quick3D, custom FBO/render-node pipelines, foreign/native windows or other specialized graphics ownership. Add support rows only after targeted qualification.

## 10. Product validation matrices

### 10.1 Deployment anchor matrix

For currently supported desktop platforms, the anchor deployment programme covers, as applicable:

- installed SDK + C++ Widgets;
- installed SDK + C++ Quick;
- installed SDK + QML Quick;
- installed SDK + Generic Widgets/Quick;
- installed SDK + QPA Widgets/Quick;
- deliberate supported combined optional payload forms such as QML + QPA;
- source/add_subdirectory acquisition where publicly supported;
- relocation/isolation;
- minimal runtime environment;
- no original SDK/build-tree search assistance;
- platform-specific runtime dependency origin checks.

### 10.2 Native coexistence anchor matrix

Release qualification maintains representative native cells for the supported integration/surface combinations and verifies:

- native baseline before HyRemote;
- view-only remote session;
- control-enabled remote session where supported;
- local input/display coexistence;
- reconnect;
- held-input terminal cleanup;
- runtime stop/application survival;
- slow-viewer bounded responsiveness.

### 10.3 Viewer interoperability

Viewer compatibility is explicit. For each viewer/version the project chooses to qualify, record:

- connect/handshake;
- framebuffer/update behavior;
- resize;
- pointer/key/text input as applicable;
- reconnect;
- disconnect cleanup;
- authentication/security profile where applicable;
- relevant extension/encoding behavior.

One successful viewer does not imply compatibility with every RFB viewer.

### 10.4 Network impairment

Nightly/extended qualification should include representative profiles such as:

- localhost/protocol baseline;
- direct LAN reference;
- added RTT;
- bandwidth cap;
- bounded packet loss/burst loss where tooling permits;
- slow reader;
- stalled handshake/client;
- abrupt disconnect;
- connection/reconnection storm.

The expected outcome is defined in terms of correctness, freshness, boundedness and recovery, not merely connection survival.

### 10.5 Real-world application programme

Synthetic fixtures remain necessary for deterministic diagnosis, but product qualification also maintains representative real applications/workloads, including over time:

- ordinary Widgets application;
- complex Widgets/dialog/table/tree application;
- QML Controls/Quick application;
- animation-heavy Quick workload;
- mixed Widgets/Quick where it becomes a claim;
- selected OpenGL/GPU path where supported;
- multi-window application;
- representative third-party/open-source Qt applications.

The purpose is to detect integration assumptions that tiny fixtures cannot represent.

## 11. Reliability, stress and soak programme

Exact repetition counts are maintained by executable plans and may evolve with product scale, but the programme includes bounded versions of:

- start/stop cycles;
- connect/disconnect cycles;
- window create/destroy and resize churn;
- popup/dialog churn;
- input bursts/floods;
- slow clients;
- concurrent clients;
- idle soak;
- active interactive soak.

A stress/soak result records environment and resource trends. Stable correctness with steadily increasing RSS/handles/FDs/threads/queues is a failure to be classified, not a pass.

PRs use small deterministic/repetition bounds; nightly/weekly/RC own long repetitions.

## 12. Performance programme integration

`docs/performance-optimization.md` remains the detailed performance authority. This strategy defines when that evidence participates in the product test architecture.

Performance execution is staged:

- PR: deterministic performance-sensitive correctness + small regression smoke for affected code;
- nightly: standard workloads and baseline/trend comparison;
- weekly/extended: broader viewers/network/high-motion/multi-viewer profiles;
- release: required reference profile SLO and resource envelope on the exact candidate.

A reproducible material regression is investigated even when the candidate remains inside the hard release ceiling. Thresholds are not widened simply to preserve a green build.

## 13. Security programme integration

Security testing is similarly staged:

- PR verification: deterministic policy/parser/fail-closed/protocol cases;
- affected product path: real listener/handshake/security configuration where changed;
- nightly/extended: malformed/stalled/fuzz/resource-admission cases;
- release: exact candidate security boundary, supported profiles, dependency/runtime closure and diagnostic/secret checks.

When encrypted transport/session identity or new authorization capabilities land, they extend PAC-7; they do not create a separate frontend-specific security model.

## 14. Support evidence and release binding

A support claim should be traceable from documentation to executable/recorded evidence:

```text
compatibility row
  -> Product Acceptance Contracts
  -> required qualification cells
  -> exact candidate SHA/build identity
  -> environment record
  -> executed evidence bundle
  -> PASS/FAIL disposition
```

A release candidate change invalidates only the evidence whose protected behavior/environment may have changed, according to release change-control classification. Material Runtime/package/default/security/QPA ABI/shared-example changes may require broad or full requalification; evidence-neutral edits do not automatically replay every expensive cell.

Historical evidence from another candidate remains useful engineering information but cannot be silently reused as exact release acceptance when the relevant product behavior changed.

## 15. Test cost, flake and observability policy

### 15.1 Record execution cost

CI records at minimum:

- discovered tests/suites;
- actually selected/executed tests/suites;
- zero-test detection;
- per-suite runtime where available;
- gate wall time;
- slowest/critical-path suites;
- platform/capability selection reason.

Trend P50/P95 wall time for important gates/suites where enough samples exist.

### 15.2 Cost regressions are engineering regressions

A new permanent test must declare its expected cost class and gate. If it materially increases a PR critical path, the change should justify why the stronger/new evidence belongs there instead of nightly/qualification.

### 15.3 Flaky tests

A flaky blocking test is a defect in the test system. Do not normalize repeated blind retries as a passing strategy.

When a test is confirmed flaky:

1. keep the underlying product risk visible;
2. classify whether the flake is product nondeterminism, environment nondeterminism or fixture defect;
3. fix/isolate/quarantine according to severity;
4. retain an alternative deterministic/qualification gate when required so quarantine does not erase coverage;
5. restore blocking status when reliability is acceptable.

### 15.4 Parallelism and isolation

Independent suites should run in parallel. Expensive configure/build/install work should reuse safe build artifacts or fixtures where this does not invalidate clean-consumer/deployment evidence. Product qualification that specifically proves isolation must build/run in an isolated environment even if that costs more.

### 15.5 Caching boundary

Compilation/download caching is permitted for developer/CI efficiency when artifact identity/integrity is verified. Runtime/deployment tests must not let cached SDK/build paths leak into the process search environment and accidentally satisfy dependencies.

## 16. Rules for adding a test

Every proposal for a permanent test/suite answers:

1. Which PAC or explicit support claim does it protect?
2. What concrete failure/risk would escape without it?
3. At what layer is the cheapest sufficient proof?
4. Does an existing test/suite already protect the same risk?
5. Is this a new case or does it require a new execution identity?
6. Which change/risk domains trigger it?
7. Which platform/capability guards are real requirements?
8. What gate and cost class does it belong to?
9. What is the expected failure message/evidence?
10. Does it affect a support/qualification/release claim?

If these questions cannot be answered, do not add a permanent blocking identity by default.

## 17. Rules for consolidating, moving or removing a test

A test may be consolidated/moved/removed when:

- a stronger or cheaper proof covers the same product risk;
- multiple identities are merely parameter variants with identical ownership/scheduling;
- a historical implementation detail is no longer a product/support contract;
- the feature/support claim was explicitly removed;
- a repository-governance check is being separated from product-quality metrics;
- an expensive PR test is retained in nightly/qualification with adequate fast PR coverage.

Before removal, document the risk/evidence replacement. Do not delete a test merely because it is slow; first decide whether the protected product contract still exists and move the expensive evidence to the proper gate if necessary.

## 18. Product-quality reporting

Do not use raw CTest identity count as the primary quality metric.

Preferred reporting includes:

- PAC coverage/status;
- affected verification suites executed;
- product validation paths executed;
- qualified support cells;
- unsupported/TODO cells;
- performance SLO/trend;
- reliability/stress/soak disposition;
- security boundary disposition;
- deployment qualification result;
- gate wall-time/critical-path health;
- flaky/quarantined evidence debt.

A useful release statement is "all required support cells for this exact candidate are qualified", not "N tests passed".

## 19. Relationship to current repository test documents

The repository currently contains detailed CTest inventory and execution documentation. Their roles under this strategy are:

| Document / mechanism | Role |
| --- | --- |
| `docs/architecture.md` | product architecture; references this strategy |
| `docs/internal/test-strategy.md` | **product-test architecture authority** |
| `docs/performance-optimization.md` | detailed performance/SLO authority |
| `docs/compatibility.md` | public support/compatibility truth |
| `docs/internal/v1-physical-acceptance.md` | current physical/native RC execution runbook |
| `docs/internal/release-candidate-checklist.md` | current release acceptance procedure |
| `tests/TEST_CATALOG.md` | current registered CTest inventory/semantic mapping |
| `tests/TEST_MATRIX.md` | current capability/selection facts |
| `tests/EXECUTION_BASELINE.md` | current hosted execution baseline |

Existing test documents are migrated incrementally; #393 does not declare tests deleted or CI behavior changed merely because this design is adopted.

## 20. Target test-system shape

The long-term repository organization should make the product/evidence boundary obvious. The exact directories may evolve, but the conceptual ownership is:

```text
tests/
  product/          Core + Shared Runtime product semantics
  adapters/         Widgets / Quick surface contracts
  integrations/     C++ / QML / Generic / QPA unique frontend mapping
  transport/        transport-neutral + RFB-specific evidence
  deployment/       package / clean consumer / relocation / runtime closure
  e2e/              viewer / examples / real-world vertical paths
  qualification/    platform / Qt / graphics / compatibility cells
  reliability/      stress / soak / fault injection / fuzz
  performance/      product performance fixtures/benchmarks
  security/         policy / auth / protocol / secret handling
  repository/       CI / docs / release / governance checks
```

This organization is descriptive rather than a mandatory immediate mass move. Physical directory churn is justified only when it improves ownership/selection/maintainability.

## 21. Definition of a healthy HyRemote test system

The test system is healthy when all of the following are true:

- every supported product claim has explicit evidence;
- deterministic defects are found in cheap verification layers;
- platform/deployment/native defects are exercised in the environments that can reveal them;
- Shared Runtime behavior is not redundantly reimplemented across frontends;
- expensive product/qualification evidence is staged away from ordinary edit loops;
- ordinary developer feedback remains fast;
- PR scope is risk-proportional and never silently zero-test;
- nightly/extended gates carry broad stress/performance/compatibility work;
- exact release candidates receive complete required product evidence;
- test runtime/flake/debt is measured and maintained;
- adding tests does not monotonically increase the PR critical path without review;
- raw test count is never treated as a substitute for product confidence.

The canonical product-quality loop is:

```text
Product vision
  -> Product Acceptance Contract
  -> explicit risk
  -> cheapest sufficient evidence
  -> technical owner
  -> risk-based execution gate
  -> qualification cell
  -> exact candidate evidence
  -> Supported / Limited / Unsupported decision
```

That loop, rather than the current number of CTest registrations, is the HyRemote testing architecture.