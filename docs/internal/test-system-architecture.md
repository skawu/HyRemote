# HyRemote Test System Architecture

Status: **detailed design — no existing test/CI migration is authorized by this document alone**

Authority: #393

Parent strategy: [`test-strategy.md`](test-strategy.md)

Product architecture: [`../architecture.md`](../architecture.md)

This document turns the product-level test strategy into an implementation-ready **test-system architecture**. It deliberately does not rename, remove, consolidate, re-register or reschedule any current test. The existing CTest inventory and CI behavior remain authoritative until later focused implementation work is explicitly admitted.

The design rule for this phase is:

> Design the evidence system completely before changing the current executable test system.

The purpose is to avoid a second round of test sprawl caused by implementing local optimizations before the product evidence model, selection model, qualification model and development-velocity budgets are frozen.

---

## 1. Scope and non-goals

### 1.1 This document designs

- the logical objects in the HyRemote test system;
- how product contracts become evidence requirements;
- how code/product changes propagate into risk domains;
- how risk selects suites, environments and execution gates;
- how test cases are grouped into suites without losing failure meaning;
- how product-validation paths are represented;
- how support/compatibility cells are qualified;
- how evidence is bound to candidate/artifact/environment identity;
- how physical/native, hosted, deployment, viewer, network, performance, security and reliability evidence coexist;
- how test cost is budgeted so testing does not become the project bottleneck;
- how current tests will later be audited and migrated without a big-bang rewrite;
- how implementation work is sequenced into the existing product roadmap.

### 1.2 This document does not authorize

- deleting or renaming existing CTests;
- changing `TEST_CATALOG.md` KEEP/MOVE/SPLIT status;
- changing `.github/workflows/ci.yml` selection;
- changing build/test labels;
- changing release-train mandatory children;
- changing current compatibility claims;
- creating a second runtime/test-only product architecture;
- adding test-only public API;
- replacing current release evidence rules;
- weakening an existing release gate in order to meet a timing budget.

Those are later implementation decisions after this architecture is accepted.

---

## 2. Authority hierarchy

The test system has four authority levels. Lower levels implement higher levels; they do not redefine them.

```text
Product roadmap / product baseline / architecture
                    |
                    v
         Product test strategy
        docs/internal/test-strategy.md
                    |
                    v
       Detailed test-system architecture
   docs/internal/test-system-architecture.md
                    |
                    v
Current executable inventory / selectors / baselines
 tests/TEST_CATALOG.md
 tests/TEST_MATRIX.md
 tests/EXECUTION_BASELINE.md
 CMake/CTest/workflows/scripts
```

Rules:

1. Product roadmap and compatibility authorities define **what HyRemote promises**.
2. `test-strategy.md` defines **what classes of evidence are required**.
3. This document defines **how the evidence system is structured**.
4. Current test inventory and CI implement the accepted design incrementally.
5. A current CTest identity is not a permanent architecture object merely because it exists today.
6. A design object is not evidence merely because it is documented.

---

## 3. Core conceptual model

The test system is built from ten stable concepts.

```text
Product Acceptance Contract (PAC)
        |
        v
Risk Domain
        |
        v
Evidence Requirement
        |
        +--------------------------+
        |                          |
        v                          v
Test Suite / Case          Qualification Cell
        |                          |
        v                          v
Execution Environment <---- Execution Gate
        |                          |
        +-------------+------------+
                      v
               Evidence Record
                      |
                      v
                Support Claim
```

The concepts are intentionally independent of CTest, GitHub Actions, Python, CMake or a particular laboratory tool.

### 3.1 Product Acceptance Contract (PAC)

A PAC is a stable product promise from `test-strategy.md`.

Current contracts:

- `PAC-1` Low Intrusion;
- `PAC-2` Native Non-interference;
- `PAC-3` Remote Experience;
- `PAC-4` Frontend Equivalence;
- `PAC-5` Deployability;
- `PAC-6` Reliability & Boundedness;
- `PAC-7` Security Truth;
- `PAC-8` Responsiveness & Efficiency;
- `PAC-9` Compatibility Truth.

PAC identifiers are stable architecture identifiers. They are not test names.

### 3.2 Risk Domain

A risk domain describes **how a product promise can be broken by a change**.

The initial risk-domain vocabulary is:

| Risk ID | Domain | Typical examples |
| --- | --- | --- |
| `R-CORE` | Core semantic invariants | lifetime, scheduling, backpressure, state machine |
| `R-RUNTIME` | Shared Runtime policy/composition | surface model, common lifecycle, listener, common diagnostics |
| `R-CAPTURE` | target/surface capture | Widgets/Quick capture, DPR, resize, damage |
| `R-INPUT` | input correctness/safety | routing, focus, text, held-state cleanup |
| `R-FRONTEND` | public/zero-code entry mapping | C++, QML, Generic, QPA activation/config mapping |
| `R-NATIVE` | local/native non-interference | qwindows/qxcb/Wayland/EGLFS, GPU, local input/display |
| `R-TRANSPORT` | transport protocol/delivery | RFB parsing, update semantics, viewer flow |
| `R-NETWORK` | listener/network behavior | bind, reachability, stalls, disconnects, impairment |
| `R-SECURITY` | security-policy truth | fail-closed, auth, downgrade, secrets |
| `R-PACKAGE` | package/export/acquisition | Config.cmake, installed payload metadata, source/install isolation |
| `R-DEPLOY` | deployment/runtime closure | DLL/ELF/plugin closure, relocation, Qt provenance |
| `R-COMPAT` | support-matrix correctness | Qt/OS/CPU/platform/UI/frontend compatibility |
| `R-PERF` | responsiveness/resource efficiency | latency, freshness, CPU/memory/bandwidth |
| `R-RELIABILITY` | long-run/resource stability | reconnect cycles, leak, churn, soak, fuzz |
| `R-DIAG` | diagnostics/supportability | actionable errors, logs, runtime facts |
| `R-ADOPTION` | user journey/self-service | discover -> integrate -> deploy -> connect -> operate |

Risk domains may evolve, but they must stay product-oriented rather than mirror every source directory.

### 3.3 Evidence Requirement

An evidence requirement says what must be demonstrated for a PAC/risk combination.

Example:

```text
ER-DEPLOY-QT-ORIGIN-LINUX
PAC: PAC-5, PAC-9
Risk: R-DEPLOY, R-COMPAT
Statement:
  A deployed Linux consumer resolves its HyRemote/Qt runtime closure from
  the selected qualified Qt SDK/deployed application tree and is not
  contaminated by unrelated distribution Qt runtimes.
Evidence class:
  P for affected deployment PRs
  Q/R for declared supported Linux cells
Environment strength:
  clean/deployment Linux host with foreign distribution Qt present
```

Evidence requirements, not current test names, are the main traceability unit.

### 3.4 Test Suite

A suite is an executable evidence unit with one owner, one scheduling shape and one failure domain.

A suite may contain many named cases.

Examples of intended suite shapes:

- Core lifecycle suite;
- Runtime listener-policy suite;
- QML frontend mapping suite;
- QPA native-delegate suite;
- deployment positive suite;
- deployment negative suite;
- RFB protocol robustness suite;
- product-path C++ Widgets suite;
- performance W3 interaction suite.

A suite is **not automatically a CTest executable**. The implementation may use one executable, one script, multiple processes or an orchestrator depending on isolation needs.

### 3.5 Test Case

A case is one named input/scenario within a suite.

Examples:

- `missing-qml-payload`;
- `stale-qpa-metadata`;
- `bind-specific-local-ip`;
- `wrong-vnc-password`;
- `stop-while-held-button`;
- `develop-runtime-only-profile`.

Cases become separate executable identities only when scheduling/isolation/capability reasons require it.

### 3.6 Execution Environment

An environment describes the facts needed to interpret evidence.

Environment classes:

- `E-SYNTHETIC` — deterministic local/hosted process tests;
- `E-HOSTED-LINUX` — hosted Linux CI/headless;
- `E-HOSTED-WINDOWS` — hosted Windows CI;
- `E-DEPLOY-LINUX` — clean Linux deployment/runtime-loader environment;
- `E-DEPLOY-WINDOWS` — clean Windows deployment/runtime-loader environment;
- `E-NATIVE-LINUX` — qualified physical/native Linux desktop/device;
- `E-NATIVE-WINDOWS` — qualified physical/native Windows desktop;
- `E-NETWORK-LAB` — controllable RTT/bandwidth/loss/slow-client environment;
- `E-PERF-REF` — stable benchmark/reference environment;
- `E-EMBEDDED` — qualified ARM64/EGLFS/Wayland/device environment;
- `E-THIRDPARTY` — pinned representative external Qt application environment.

An environment is described by facts, not merely by a runner label.

### 3.7 Execution Gate

A gate controls **when evidence is collected**, not what the product means.

Current architecture gates:

- `G0` Developer;
- `G1` PR Verification;
- `G2` PR Product;
- `G3` Merge Sentinel;
- `G4` Nightly Qualification-lite;
- `G5` Weekly/Extended;
- `G6` Release Qualification.

### 3.8 Qualification Cell

A qualification cell is one explicit compatibility/support boundary.

Conceptual dimensions include:

```text
Qt family / exact anchor
x OS
x CPU architecture
x native Qt platform/delegate
x graphics path
x UI family
x HyRemote frontend
x deployment form
x viewer/security/network profile where material
```

The product does **not** execute a full Cartesian product. Cells are selected as anchor, interaction-risk or representative/pairwise cells.

### 3.9 Evidence Record

An evidence record is the immutable result of an executed requirement/suite/cell.

An evidence record contains enough identity to decide whether it can support a claim.

### 3.10 Support Claim

A support claim is the product-facing result, such as one row/cell in `docs/compatibility.md`.

A claim is downstream of evidence. Tests do not become mandatory merely because they exist; they are mandatory when a product claim or risk requires their evidence.

---

## 4. Traceability graph

The required traceability direction is:

```text
Product vision
  -> PAC
  -> risk domain
  -> evidence requirement
  -> suite/case
  -> environment
  -> gate execution
  -> evidence record
  -> support/release claim
```

Reverse questions must also be answerable:

```text
Why does this test exist?
  <- evidence requirement
  <- risk
  <- PAC/support claim

Why did this PR run this test?
  <- changed area
  <- risk propagation
  <- selector rule

Why is this compatibility row Supported?
  <- qualification cell
  <- exact evidence records
```

Any current or future test that cannot eventually participate in this traceability should be classified as:

- repository/governance check;
- temporary investigation/preflight;
- redundant historical evidence;
- or a candidate for removal/consolidation.

---

## 5. Evidence strength model

Evidence has two independent properties:

1. **class** — Verification, Product Validation, Qualification or Release Acceptance;
2. **environment strength** — synthetic/hosted/deployed/native/reference/etc.

A higher-cost environment is not automatically better for every question.

Examples:

- Core mailbox ordering is best proved deterministically, not on a physical desktop.
- Linux loader/Qt-origin correctness requires a real deployment/loader environment.
- local+remote native coexistence requires native/physical evidence.
- a release claim requires exact-candidate evidence even if an older candidate passed the same test.

### 5.1 Evidence substitution rules

Allowed:

- a stronger exact-environment run may satisfy a weaker environment requirement if it exercises the same deterministic property and remains diagnosable;
- a deterministic shared-layer test may replace four duplicate frontend copies when the frontend contributes no unique semantics.

Not allowed:

- hosted/headless replacing native local-display/input evidence;
- Windows replacing Linux or vice versa for platform-sensitive claims;
- nearby Qt patch replacing exact QPA private-ABI evidence;
- a third-party application replacing controlled compatibility cells;
- physical evidence replacing clean package/deployment evidence;
- an older SHA replacing exact RC evidence;
- CI workflow success with zero relevant execution replacing evidence.

---

## 6. Suite architecture

The future executable system is organized into suite families. This is a logical design, not a command to move files immediately.

| Suite family | Primary responsibility | Normal evidence class |
| --- | --- | --- |
| `S-CORE` | platform/transport-neutral invariants | V |
| `S-RUNTIME` | shared Qt-aware product semantics | V |
| `S-ADAPTER` | Widgets/Quick surface/input contracts | V |
| `S-FRONTEND` | C++/QML/Generic/QPA unique mapping | V/P |
| `S-TRANSPORT` | transport-neutral + RFB-specific protocol/delivery | V/P |
| `S-PACKAGE` | package/export/acquisition contracts | V/P |
| `S-DEPLOY` | install/deploy/runtime closure/relocation | P/Q |
| `S-PRODUCT-PATH` | vertical user journeys | P/Q |
| `S-SECURITY` | policy truth + mechanism-specific security | V/P/Q |
| `S-RELIABILITY` | stress/soak/fuzz/resource bounds | V/Q |
| `S-PERFORMANCE` | interaction SLO/resource regression | V/Q/R |
| `S-COMPAT` | qualification-cell orchestration | Q/R |
| `S-REALWORLD` | representative third-party pressure tests | Q |

Repository/release governance remains outside this product suite taxonomy.

### 6.1 Shared semantics versus frontend semantics

The default ownership rule is:

```text
Core invariant               -> S-CORE
Qt-aware shared behavior     -> S-RUNTIME
surface/capture/input detail -> S-ADAPTER
entry syntax/activation      -> S-FRONTEND
wire/protocol behavior       -> S-TRANSPORT
artifact closure             -> S-PACKAGE/S-DEPLOY
user journey                 -> S-PRODUCT-PATH
support claim                -> S-COMPAT
```

This prevents the four peer frontends from becoming four copies of the product test suite.

### 6.2 Suite isolation policy

A case should be split into a separate executable identity only if one of these differs materially:

- platform/capability registration;
- external resource/viewer/device;
- environment image/host;
- timeout/cost class;
- process isolation requirement;
- destructive/fuzz behavior;
- selector ownership;
- expected-result model;
- evidence retention requirement.

Otherwise prefer named table-driven cases.

### 6.3 Failure granularity

Consolidation must preserve:

- exact case name;
- violated evidence requirement/PAC;
- environment identity;
- actionable error/log location;
- independent retry ability when external infrastructure is involved.

Do not create one giant opaque test merely to reduce identity count.

---

## 7. Metadata model

Later implementation should expose enough machine-readable metadata for selection, scheduling and traceability. The design does not require a specific storage format yet.

Conceptual suite metadata:

```yaml
suite_id: S-DEPLOY-LINUX-RUNTIME-CLOSURE
owner: deployment
pac: [PAC-5, PAC-9]
risk: [R-DEPLOY, R-COMPAT]
evidence_class: [P, Q, R]
cost_class: installed
platform_sensitivity: linux
capabilities: [runtime, cpp]
environments: [E-DEPLOY-LINUX]
gates: [G2, G4, G6]
triggers:
  - deploy-helper
  - install-export
  - runtime-dependency-resolution
isolation: clean-prefix
parallel_group: deployment-linux
expected_max_wall: 60s
```

Conceptual case metadata:

```yaml
case_id: foreign-distribution-qt-present
requirement: ER-DEPLOY-QT-ORIGIN-LINUX
expected: selected-sdk-lineage-only
```

### 7.1 Metadata principles

- metadata describes evidence meaning, not implementation mechanics only;
- a suite may map to one or many current CTest identities during migration;
- selectors consume metadata rather than hard-coded test-name folklore;
- PAC/risk mapping must be reviewable in normal code review;
- cost/platform/capability metadata is explicit;
- unknown/malformed metadata fails closed for product lanes;
- metadata must not require every developer to understand release governance.

---

## 8. Change-impact and selection architecture

Selection is a graph problem, not a filename glob problem.

```text
changed files / build options / product config
            |
            v
       ownership areas
            |
            v
        risk domains
            |
            v
     evidence requirements
            |
            v
 candidate suites/cases
            |
      capability filter
            |
      environment filter
            |
        gate policy
            |
            v
     execution manifest
```

### 8.1 Ownership areas

Initial ownership areas should follow architecture/product seams rather than every folder:

- core;
- runtime-common;
- widgets-adapter;
- quick-adapter;
- cpp-frontend;
- qml-frontend;
- generic-frontend;
- qpa-frontend;
- rfb-transport;
- security;
- package-export;
- deploy-helper;
- examples/adoption;
- compatibility/qualification;
- performance;
- CI/test-system;
- docs-only;
- repository-governance.

### 8.2 Risk propagation examples

`src/core/session.*`:

```text
core -> R-CORE/R-RELIABILITY
     -> Core deterministic suites
     -> selected Runtime/transport seam suites
     -> performance smoke only if scheduling/frame-age semantics changed
```

`cmake/HyRemoteDeploy.cmake`:

```text
deploy-helper -> R-DEPLOY/R-PACKAGE/R-COMPAT
              -> deploy/package contract suites
              -> Windows + Linux clean product deployment paths
              -> no automatic full Core replay
```

`src/integrations/qml/*`:

```text
qml-frontend -> R-FRONTEND/R-ADOPTION
             -> QML mapping suite
             -> QML vertical product path
             -> shared Runtime suites only where shared code changed
             -> no automatic QPA deploy matrix
```

`src/integrations/qpa/*`:

```text
qpa-frontend -> R-FRONTEND/R-NATIVE/R-COMPAT/R-DEPLOY
             -> QPA deterministic/native/deploy suites
             -> Windows/Linux applicable product paths
             -> exact Qt private-ABI qualification when support evidence is invalidated
```

### 8.3 Selector invariants

- a product lane with selected risk must execute at least one relevant suite;
- zero relevant execution is invalid evidence;
- a selector may broaden conservatively when ownership is unknown;
- selector failure must not silently return an empty test set;
- full-gate override remains available for uncertainty/RC use;
- path-based selection is only one input; public API/package metadata/config changes may propagate beyond direct file paths;
- selector rules themselves are tested deterministically.

### 8.4 Change-risk overrides

Some changes are automatically high-propagation regardless of file count:

- public API contract;
- Shared Runtime state/security semantics;
- build/install/export/package metadata;
- deployment runtime resolution;
- QPA private ABI/native delegate logic;
- transport parser/security framing;
- release compatibility/support statements;
- test selector/test metadata implementation itself.

---

## 9. Execution gates

### 9.1 G0 — Developer

Goal: keep edit/test feedback under 10 seconds for normal focused work.

Characteristics:

- changed module + direct deterministic dependencies;
- no clean SDK reinstall by default;
- no real viewer/device by default;
- easy explicit suite selection;
- stable local invocation through project build authority.

G0 is developer feedback, not release evidence.

### 9.2 G1 — PR Verification

Goal: deterministic affected evidence, target <= 90 seconds wall time.

Characteristics:

- affected V suites;
- required OS duplication only for platform-sensitive behavior;
- parallel execution by independent suite family;
- no broad physical/soak matrix;
- selector/execution manifest visible in logs.

### 9.3 G2 — PR Product

Goal: prove affected vertical product path, target <= 3–5 minutes wall time.

Activated when risk crosses user/product artifact boundaries.

Typical triggers:

- public frontend/API behavior;
- package/install/export;
- deployment helper/runtime closure;
- QPA/native plugin loading;
- security/transport behavior;
- canonical adoption/example path;
- compatibility-critical changes.

G2 may run Windows and Linux independently where loader/native behavior differs.

### 9.4 G3 — Merge Sentinel

Goal: <= 60 seconds and nonzero.

It answers only:

> Did integrated mainline become obviously unusable or did selection produce a false green?

It does not replay full PR qualification.

### 9.5 G4 — Nightly Qualification-lite

Goal: broad evidence <= 30 minutes target through parallelism and bounded matrices.

Includes selected:

- all four frontend product paths;
- clean deploy/relocation;
- viewer interoperability subset;
- network impairment subset;
- stress-lite;
- performance trend;
- broader compatibility anchors.

### 9.6 G5 — Weekly/Extended

Carries expensive evidence that should not affect daily development:

- long stress/repetition;
- fuzz campaigns;
- extended viewer/network matrix;
- extended benchmark workloads;
- longer soak;
- selected third-party applications where automation is practical.

### 9.7 G6 — Release Qualification

G6 is not optimized for developer feedback time.

It proves the exact frozen candidate against the declared release/support envelope.

Requirements include as applicable:

- hosted deterministic evidence;
- clean package/install/deploy evidence;
- exact qualification cells;
- physical/native local+remote evidence;
- maintained viewer evidence;
- security boundary;
- performance SLO;
- required stress/soak;
- real-world representative evidence;
- final compatibility/known-limitations truth.

---

## 10. Development-velocity architecture

Testing must not become the dominant cost of ordinary development.

### 10.1 Velocity budgets

| Metric | Budget / policy |
| --- | --- |
| focused local feedback | <= 10 s normal target |
| affected PR verification | <= 90 s target |
| affected PR product lane | <= 3–5 min target |
| merge sentinel | <= 60 s target |
| nightly qualification-lite | <= 30 min target |
| blocking flaky-test rate | effectively zero; repeated instability requires test/system ownership action |
| zero-test selected lane | invalid |
| unrelated expensive suite execution | treated as selector/test-architecture defect |

### 10.2 Cost is measured at wall-time critical path

Do not optimize only by counting tests or summing test durations.

Track at least:

- per-suite P50/P95 runtime;
- gate wall time;
- critical-path suite(s);
- queue/wait time versus execution time;
- setup/configure/build/install time;
- external viewer/device acquisition time;
- retry/flake cost;
- selected-suite count by risk domain;
- percentage of runs executing expensive installed/E2E suites;
- duplicate execution across OS where no platform sensitivity exists.

### 10.3 Optimization order when a gate exceeds budget

1. remove unrelated selection;
2. parallelize independent work;
3. reuse safe build artifacts while preserving test isolation;
4. share expensive fixture setup inside a suite;
5. parameterize cases instead of repeated process/configure startup;
6. split cheap deterministic proof from expensive product proof;
7. move breadth/repetition to later gate;
8. optimize the test implementation itself;
9. only then reconsider whether the product evidence requirement is unnecessarily strong.

Never start by deleting evidence needed for a real product risk.

### 10.4 Clean evidence versus caching

Caching is allowed for developer/verification speed where it cannot alter the property being proved.

Clean deployment/qualification must explicitly control the environment being validated. A cache must not reintroduce:

- source/build-tree search paths;
- stale installed metadata;
- foreign plugin paths;
- SDK runtime crutches;
- mixed candidate artifacts.

---

## 11. Product-path validation architecture

Product validation is defined by user journeys, not test executables.

### 11.1 Canonical journey

```text
acquire
 -> configure/integrate
 -> build
 -> install
 -> deploy
 -> isolate/relocate
 -> launch
 -> connect viewer
 -> view
 -> optional control
 -> disconnect/reconnect
 -> stop/disable
 -> diagnose if failure
```

### 11.2 Frontend anchor paths

The controlled anchor set should cover, when applicable to the product line:

- C++ + Widgets;
- C++ + Quick;
- QML + Quick;
- Generic + Widgets;
- Generic + Quick;
- QPA + Widgets;
- QPA + Quick;
- supported combined optional payload forms such as QML + QPA.

The existence of an anchor path does not imply every path runs on every PR.

### 11.3 Product-path assertions

Each vertical path records:

- acquisition form;
- application source/link contract;
- exact Qt/HyRemote identity;
- deployment form;
- runtime dependency origin;
- native platform identity where relevant;
- listener/security/input configuration;
- viewer/version;
- launch success;
- view/control/reconnect result;
- stop/application-survival result;
- diagnostics on failure.

---

## 12. Deployment evidence architecture

Deployment is a first-class product subsystem.

### 12.1 Deployment evidence layers

1. **Package contract** — exported targets/metadata/payload availability.
2. **Deployment planning** — `hyremote_deploy()` accepts/rejects requested payloads correctly.
3. **Artifact closure** — the deployed tree contains required product/Qt/native payloads.
4. **Isolation/relocation** — runtime succeeds without source/build/SDK-path assistance.
5. **Runtime origin** — loaded dependencies come from the selected product tree/qualified Qt lineage.
6. **Product fit** — deployed consumer actually launches and supports viewer use.

### 12.2 Positive cases

Examples:

- installed C++ consumer;
- installed QML consumer;
- Generic consumer preserving native platform;
- QPA consumer with exact compatible Qt;
- supported combined QML+QPA;
- source/add_subdirectory consumption where supported;
- relocated deployment tree.

### 12.3 Negative cases

Examples:

- requested optional payload absent;
- stale/inconsistent metadata;
- QPA Qt private-ABI mismatch;
- illegal Generic+QPA combination where prohibited;
- missing package/payload;
- runtime closure contaminated by foreign Qt lineage;
- accidental dependency on original build/SDK tree.

Negative inputs normally belong in table-driven suites unless they require a different environment.

### 12.4 Linux Qt-lineage rule

For Qt libraries, the selected qualified Qt SDK/deployed copy is the positive trust source. Distribution Qt is environmental runtime for unrelated applications, not an alternate HyRemote development SDK.

The test system should prove positive ownership/origin rather than maintain brittle global blacklists of `/lib` or `/usr/lib`.

---

## 13. Native non-interference qualification

### 13.1 Required observation model

For supported native cells, observe the same application in three states:

```text
native baseline
    -> HyRemote view-only
    -> HyRemote remote-control-enabled (where supported)
```

Verify applicable:

- local visible rendering;
- local pointer/keyboard/text;
- focus/resize;
- dialogs/menus/popups/multi-window;
- graphics/GPU path;
- remote view;
- remote input policy;
- reconnect;
- held-state cleanup;
- Runtime stop with application survival;
- slow-viewer/native responsiveness.

### 13.2 Why hosted CI cannot replace this

Headless/offscreen/Xvfb environments intentionally remove or alter properties such as compositor, native platform plugin, physical scaling, GPU/driver and local input/display coexistence.

They remain valuable regression environments but cannot independently prove PAC-2 for native support cells.

---

## 14. Compatibility and qualification-cell design

### 14.1 Three cell classes

**Anchor cells**

Complete reference cells required for every relevant release line.

**Interaction-risk cells**

Cells selected because two or more dimensions strongly interact, for example:

- QPA x exact Qt patch x OS;
- graphics backend x OS/GPU;
- deploy x OS loader;
- Qt family x public frontend;
- security x transport;
- DPR x native display stack.

**Representative/pairwise cells**

Used to avoid an unbounded Cartesian matrix while covering meaningful pairwise interactions.

### 14.2 Qualification statuses

The test architecture supports product status values owned by compatibility/product authorities, such as:

- Supported;
- Qualified;
- Preview;
- Experimental;
- Unsupported;
- Not Applicable.

The test system supplies evidence; it does not invent status semantics independently.

### 14.3 Cell identity

A qualification cell record includes the dimensions material to the claim. Unused dimensions are explicitly marked not material rather than silently omitted.

### 14.4 Public Qt versus QPA

- C++/QML/Generic public-Qt claims may qualify a version family/range using selected anchors according to product policy.
- QPA requires exact private-ABI Qt patch/platform evidence for every claimed exact row.

---

## 15. Viewer interoperability architecture

A viewer compatibility claim is explicit and versioned.

Per maintained viewer/version, relevant evidence includes:

- handshake/connect;
- update negotiation;
- framebuffer correctness;
- resize;
- pointer/key/text;
- reconnect/disconnect;
- held-state cleanup;
- authentication/security mechanism where applicable;
- compression/extension behavior where claimed;
- performance relevance where used as a reference viewer.

A single viewer path may serve as the primary reference cell while other viewers are compatibility cells. Passing one viewer never implies all RFB clients.

---

## 16. Network impairment architecture

Network testing is split from transport parser testing.

Representative profiles:

- localhost protocol baseline;
- direct LAN reference;
- added RTT;
- bandwidth cap;
- bounded packet loss/burst loss;
- slow reader;
- stalled handshake;
- abrupt disconnect;
- reconnect storm;
- multi-viewer mixed latency.

Expected results are expressed as:

- correctness;
- boundedness;
- freshness/frame age;
- recovery;
- native UI health;
- resource growth;
- interaction latency.

Not merely "TCP remained connected".

---

## 17. Reliability, stress, soak and fuzz

### 17.1 Deterministic first

Every reliability property that can be deterministically modeled should be proved cheaply first:

- bounded queue size;
- one teardown owner;
- cancellation semantics;
- held-input cleanup;
- late callback suppression;
- parser bounds;
- per-viewer pending-work bounds.

### 17.2 Repetition/stress second

Stress catches integration/resource failures not visible in deterministic models.

Reference scenario families:

- 1k start/stop;
- 10k connect/disconnect;
- resize/surface churn;
- input flood;
- slow/stalled client;
- multiple viewers;
- target destruction during activity.

Exact counts are benchmark/configuration values, not permanent architecture constants.

### 17.3 Soak

Idle and active soak observe:

- RSS;
- thread count;
- FD/handle count;
- sockets;
- queue/backlog;
- CPU/GPU;
- crash/hang;
- latency drift.

### 17.4 Fuzz

Fuzz targets private/parsing/config seams such as:

- RFB protocol parser;
- security/config descriptors;
- selected metadata parsers where malformed external input exists.

Fuzzing is an extended gate and should not block ordinary PR feedback unless a specific regression test becomes deterministic and cheap.

---

## 18. Security evidence architecture

The top-level security test object is the requested policy, not a particular cryptographic mechanism.

```text
requested profile
 -> capability available?
 -> configuration valid?
 -> required protection established?
 -> listener/session permitted
```

Required categories:

- safe defaults;
- absent capability;
- malformed/missing configuration;
- correct/wrong credential;
- downgrade prevention;
- listener creation ordering;
- partial-start cleanup;
- secrets/logging;
- malformed/stalled client resource bounds;
- dependency/runtime provenance;
- future encrypted transport/certificate policy when implemented.

Security mechanism-specific suites sit beneath this product policy contract.

---

## 19. Performance evidence architecture

### 19.1 Primary metric

The product metric is Remote Interaction Latency:

```text
viewer sends action
 -> HyRemote receives/routes input
 -> Qt application changes
 -> capture becomes available
 -> schedule/encode/send
 -> viewer observes corresponding state
```

### 19.2 Workloads

Reuse the long-lived performance programme workload classes:

- W1 Static;
- W2 Localized UI;
- W3 Interactive;
- W4 High Motion.

### 19.3 Diagnostic metrics

- input-to-Qt latency;
- render-to-capture/capture latency;
- frame age;
- encode latency;
- bytes/update/time;
- RTT/backlog;
- CPU/GPU/memory;
- local UI impact.

These explain the primary metric; they do not replace it.

### 19.4 Performance gates

- G1: only deterministic/perf-smoke where change risk requires it;
- G4: trend/reference subset;
- G5: broader benchmark workloads;
- G6: exact qualified reference SLO and release envelope.

A reproducible performance regression remains visible even when functional tests pass.

---

## 20. Real-world application programme

Controlled fixtures prove specific properties cheaply. Representative third-party applications pressure-test low intrusion and real integration complexity.

The programme consumes, rather than defines, the support matrix.

Rules:

- pinned upstream revision/environment;
- no HyRemote patch to make the app pass unless the test is explicitly source-integration research;
- exact applicable frontend selection;
- upstream/native behavior preserved;
- unsupported custom/media/native surfaces classified truthfully;
- no hidden SDK/build-tree runtime assistance;
- result is third-party verification, not an automatic named-app support promise.

This design aligns with existing #134/#242 ownership rather than creating a parallel real-world programme.

---

## 21. Evidence record schema

Every retained Q/R evidence record should conceptually contain:

```yaml
record_id: immutable-id
candidate_sha: exact-source-sha
artifact_identity: exact-built/staged-artifact-id
suite_id: logical-suite
case_ids: [executed-cases]
evidence_requirements: [ER-...]
pac: [PAC-...]
risk: [R-...]
evidence_class: Q|R
environment:
  os: ...
  cpu: ...
  qt: ...
  native_platform: ...
  graphics: ...
  display_dpr: ...
  toolchain: ...
viewer: ...
network_profile: ...
commands: ...
result: PASS|FAIL|BLOCKED
logs_artifacts: ...
started_at: ...
completed_at: ...
```

Not every field is required for every evidence type; material dimensions must be present.

### 21.1 Exact-candidate rule

For **Release Acceptance**, all evidence that the release procedure requires to be exact-candidate-bound must use the same RC-FROZEN candidate/artifact identity.

If any post-freeze change changes the candidate SHA/artifact:

- prior physical/native evidence is historical/preflight for the new candidate unless the canonical release authority explicitly defines a same-artifact identity rule;
- this test architecture does **not** permit mixing different candidate SHAs into one release PASS merely because a change was described as evidence-neutral;
- the release change-control authority decides whether the candidate remains frozen, is re-cut, or affected cells must be rerun.

This rule intentionally aligns with the existing release-candidate/physical acceptance requirement and avoids evidence drift.

### 21.2 Qualification evidence versus release evidence

Long-lived qualification results may inform future planning and reduce discovery work, but release acceptance still applies exact-candidate rules defined by the release authority.

---

## 22. Flaky-test and infrastructure policy

A flaky blocking test is a test-system defect, not normal background noise.

### 22.1 Classification

A failure is classified as one of:

- product defect;
- deterministic test defect;
- environmental/runner infrastructure failure;
- external dependency/viewer/device failure;
- unresolved/intermittent.

### 22.2 Retry policy

Automatic retries may be used only to diagnose known external/infrastructure instability. A retry must not silently convert an unexplained product/test failure into PASS evidence.

### 22.3 Quarantine

Temporary quarantine is allowed only with:

- owner;
- reason;
- affected PAC/evidence gap;
- replacement/compensating evidence if required;
- expiry/review condition;
- visible non-blocking status.

Quarantine cannot silently shrink a Supported claim.

### 22.4 Blocked evidence

`BLOCKED` is not PASS and must remain visible to qualification/release decisions.

---

## 23. Test lifecycle governance

### 23.1 Adding a test/case

Every permanent addition answers:

1. Which PAC/support claim does it protect?
2. What risk domain and failure mode does it cover?
3. Why is existing evidence insufficient?
4. What is the cheapest sufficient evidence layer?
5. Is this a case or truly a new suite/identity?
6. Which gates/environments need it?
7. What is its expected wall-time/setup cost?
8. What changes trigger it?
9. What is the failure meaning?

### 23.2 Consolidating tests

Consolidation is preferred when cases share:

- setup;
- owner;
- scheduling;
- capability/environment;
- failure category.

Consolidation must retain precise named-case diagnostics.

### 23.3 Moving a test to another gate

Move when the evidence remains required but its frequency/cost is disproportionate to normal change risk.

Typical example: long soak from PR to weekly, while a deterministic resource-bound regression remains in PR.

### 23.4 Removing a test

Removal is allowed when:

- product claim/risk no longer exists;
- stronger/cheaper evidence fully supersedes it;
- it only duplicated shared behavior already proved at the correct seam;
- historical implementation detail no longer exists;
- it belongs to governance rather than product quality and is moved accordingly.

Record the replacement evidence or reason.

---

## 24. Current-inventory migration design

Migration is intentionally staged and reversible.

### Phase M0 — Architecture freeze

No current test changes.

Deliverables:

- accepted strategy;
- accepted detailed architecture;
- accepted PAC/risk/evidence/gate vocabulary;
- accepted velocity budgets;
- accepted evidence-binding rules.

### Phase M1 — Read-only inventory mapping

No behavior/scheduling changes.

For every current test identity record:

- PAC(s);
- risk domain(s);
- evidence class;
- suite-family candidate;
- platform/capability sensitivity;
- current triggers/gates;
- current cost;
- duplicate/unique evidence assessment;
- candidate disposition: retain identity / parameterize / move gate / governance / investigate.

M1 produces a migration report, not test edits.

### Phase M2 — Metadata/registry foundation

Introduce machine-readable logical metadata without initially changing test behavior.

Goals:

- selector can reason about suites/PAC/risk/cost;
- current CTest names can map many-to-one into future suites during transition;
- current execution remains comparable.

### Phase M3 — Suite consolidation

Only after M1/M2 evidence.

Priorities:

- fragmented profile/policy cases;
- deployment positive/negative variants;
- governance/product separation;
- duplicated frontend shared behavior.

Core/runtime correctness tests are not consolidated merely to reduce count.

### Phase M4 — Risk-based gate selection

Implement G0–G3 selection and verify against current/full-suite baselines.

Requirements:

- selector self-tests;
- no zero-test false green;
- conservative fallback;
- measurable PR critical-path improvement;
- no product evidence loss.

### Phase M5 — Product-validation orchestration

Make G2/G4 product-path evidence explicit and reusable:

- clean acquisition;
- install/deploy;
- isolation/relocation;
- launch/viewer;
- frontend product paths.

### Phase M6 — Qualification evidence framework

Prepare the V0.4 qualification machinery before V0.4 entry:

- qualification-cell registry;
- environment identity;
- evidence records;
- exact-SHA/artifact binding;
- physical/native records;
- performance/security/reliability integrations;
- compatibility-row traceability.

### Phase M7 — Continuous test-system health

Long-lived maintenance:

- gate SLO monitoring;
- flake monitoring;
- duplicate evidence reviews;
- obsolete test cleanup;
- qualification refresh policy;
- support-matrix evolution.

---

## 25. Roadmap placement and dependency rules

This test-system programme is **cross-cutting engineering infrastructure**, not a new product version line.

### 25.1 V0.2.0.0

Current First User Trial remains the delivery priority.

Allowed test-system work:

- architecture/design (#393/#394);
- read-only audit work that does not destabilize V0.2;
- urgent regression evidence for concrete V0.2 defects.

Not allowed:

- broad CTest/CI migration that expands V0.2 critical path;
- pulling the complete V0.4 qualification system into the current trial.

### 25.2 V0.3.0.0 self-service baseline

As self-service SDK/deploy/productization becomes real, M2–M5 can be introduced incrementally where they directly support those product paths.

The test-system work must not replace product work such as SDK/deploy/docs/diagnostics; it exists to prove those outcomes efficiently.

### 25.3 V0.3 family breadth convergence

Before V0.4 entry, the test system must be capable of representing/collecting evidence for the mandatory #343 matrix without exploding into an unbounded Cartesian CI matrix.

Therefore M4–M6 should converge during the V0.3 family alongside the relevant Qt/platform/transport/product work.

### 25.4 V0.4.0.0

V0.4 is qualification only.

By entry:

- qualification-cell/evidence machinery must already exist;
- product paths must already be testable;
- test selection/gate architecture must already be stable enough not to become hidden feature development;
- V0.4 executes/collects/decides qualification evidence using #57/#109/#134/#242/#9/#165 and other existing authorities.

### 25.5 V1.0.0.0

GA consumes the qualified evidence and freezes the first long-lived support contract.

Test-system changes during final GA acceptance are limited to release-blocking evidence corrections; they do not redesign the architecture.

---

## 26. Implementation work-package plan

These are **planned work packages**, not yet separate mandatory release-train children and not authorization to edit tests before this design is accepted.

| WP | Work package | Depends on | Main output | Product/release placement |
| --- | --- | --- | --- | --- |
| `TS-0` | Test strategy + detailed architecture freeze | #393 | accepted design, no test changes | now; non-blocking V0.2 architecture work |
| `TS-1` | Current inventory semantic/cost audit | TS-0 | read-only mapping report for every current test | after design; may run alongside V0.2/V0.3 planning |
| `TS-2` | Logical metadata/registry foundation | TS-1 | PAC/risk/suite/gate metadata mapped to current tests | V0.3 family, incremental |
| `TS-3` | Suite/identity consolidation | TS-1, TS-2 | parameterized suites + governance separation, no evidence loss | V0.3 family, bounded PRs |
| `TS-4` | Risk selector + G0/G1/G2/G3 orchestration | TS-2 | change-risk execution manifest and gate budgets | V0.3 family before broad matrix growth |
| `TS-5` | Product-path validation harness | TS-2, productized deploy paths | clean install/deploy/launch/viewer reusable scenarios | aligned with V0.3 self-service/productization |
| `TS-6` | Qualification/evidence-record framework | TS-2, TS-5, #343 matrix | qualification cells, environment/evidence identity, support traceability | must converge before V0.4 entry |
| `TS-7` | Extended reliability/security/performance integration | TS-6 + existing domain authorities | soak/stress/fuzz/security/perf evidence wired into gates | V0.3 breadth/V0.4 qualification, using existing issues |
| `TS-8` | Continuous test-system SLO/governance | TS-4 onward | runtime/flake/duplicate/evidence health review | ongoing, not a separate product feature |

### 26.1 No issue explosion rule

Do not create one GitHub Issue per row/case now.

After TS-0 is accepted:

1. execute TS-1 first;
2. use actual audit findings to decide which implementation packages are independently mergeable;
3. create only the minimum focused Issues needed for those packages;
4. link each package to existing product authorities rather than duplicate #57/#109/#134/#343/#370/etc.;
5. do not add a work package to `release-trains.json` merely because it exists — only exact product/release authorities decide whether it is a mandatory child or prerequisite.

---

## 27. Relationship to existing product authorities

The test-system programme consumes existing ownership instead of replacing it.

| Existing authority | Relationship |
| --- | --- |
| #1 product roadmap | top-level product sequencing; test programme is cross-cutting support |
| #343 GA compatibility/product baseline | defines mandatory support dimensions before V0.4 |
| #57 compatibility qualification | owns Qt/support qualification content |
| #109 physical/native acceptance | owns physical/native acceptance evidence |
| #134 / #242 real-world programme | owns representative third-party pressure testing |
| #370 performance programme / #9 qualification | owns performance metrics/SLO/qualification content |
| #143 and security authorities | own security product capability/truth |
| #165 candidate freeze/change control | owns release candidate invalidation/retest decisions |
| #95/release-train authority | owns version/release mechanics |

The test architecture supplies common evidence plumbing and execution economics; it does not take product scope away from these authorities.

---

## 28. Detailed-design acceptance criteria

TS-0 is complete only when reviewers can answer all of the following without referring to the current CTest count:

1. What product promises are being protected?
2. What risk domains can violate them?
3. What evidence requirement proves each important risk?
4. Which technical seam is the cheapest honest evidence source?
5. When is a variant a case versus a separate suite/identity?
6. How does a code/product change select evidence?
7. How are Windows/Linux/Qt/QPA/platform differences represented?
8. How are clean deployment and runtime origin proved?
9. How are physical/native claims kept separate from hosted evidence?
10. How are viewer/network/security/performance/reliability dimensions handled without Cartesian explosion?
11. How is every retained qualification/release result bound to exact environment/candidate/artifact identity?
12. How does the architecture prevent false-green zero-test selection?
13. How does it prevent testing from slowing normal development unnecessarily?
14. How are flaky/infrastructure failures distinguished from product failures?
15. How can current tests migrate incrementally without a big-bang rewrite?
16. Which work happens before V0.4 entry and which work is qualification execution inside V0.4?
17. Which existing product authorities remain owners of their domain evidence?
18. What must be measured before declaring the new test system better than the current one?

Until these are accepted, no broad current-test migration should begin.

---

## 29. Open design questions requiring explicit closure before TS-1/TS-2 implementation

The following are bounded implementation-design choices, not invitations to redesign the product:

1. **Machine-readable metadata storage** — CMake properties, declarative manifest, generated registry or a hybrid; choose the smallest form that supports traceability and selectors without creating two authorities.
2. **Suite runner boundary** — where table-driven cases are best implemented in C++/QtTest, CMake script or Python orchestration.
3. **Evidence record serialization** — exact schema/location/retention for Q/R evidence while keeping generated evidence out of source control where appropriate.
4. **Environment registry** — how hosted/reference/physical hosts publish immutable environment facts without embedding secrets or machine-specific assumptions in product code.
5. **Selector fallback** — exact policy for unknown files/metadata drift; default must be conservative and nonzero.
6. **Cost telemetry** — where per-suite/gate P50/P95 and critical-path metrics are retained.
7. **Quarantine representation** — how temporary non-blocking status exposes the missing PAC/evidence gap.
8. **Compatibility traceability rendering** — whether `docs/compatibility.md` links generated evidence summaries or consumes a machine-readable qualification registry.

These questions should be answered during the design review or TS-1 audit based on repository facts; do not prematurely implement a framework before the need is measured.

---

## 30. Architecture success criteria

The future implementation is successful only if all of these become true:

- product confidence is at least as strong as today;
- every important product claim is traceable to executed evidence;
- ordinary PRs run materially less unrelated work;
- deployment/native/platform risks still receive the stronger evidence they need;
- Core/Runtime/frontends no longer duplicate shared proof unnecessarily;
- case growth does not automatically cause CTest-identity growth;
- test failures become more diagnosable;
- support qualification becomes evidence-driven and exact-environment bound;
- current V0.2 delivery is not delayed by a broad testing rewrite;
- V0.4 enters qualification with the evidence machinery already ready;
- test-system wall time, flake and duplicate execution are measured and controlled as engineering budgets;
- the architecture remains valid when HyRemote adds Qt versions, ARM64, Wayland/EGLFS, another transport or hardware acceleration.

The target is not a smaller test number. The target is **stronger product evidence per unit of development time**.
