# HyRemote Test System Architecture

Status: **detailed design — no existing test/CI migration is authorized by this document alone**

Authority: #393

Parent strategy: [`test-strategy.md`](test-strategy.md)

Product architecture: [`../architecture.md`](../architecture.md)

This document turns the product-level test strategy into an implementation-ready **test-system architecture**. It deliberately does not rename, remove, consolidate, re-register or reschedule any current test. The existing CTest inventory and CI behavior remain authoritative until later focused implementation work is explicitly admitted.

The design rule for this phase is:

> Design the evidence system completely before changing the current executable test system.

The purpose is to avoid a second round of test sprawl caused by implementing local optimizations before the product evidence model, selection model, qualification model, maintenance model and development-velocity budgets are frozen.

---

## 1. Scope and non-goals

### 1.1 This document designs

- the logical objects in the HyRemote test system;
- how product contracts become evidence requirements;
- how code/product changes propagate into risk domains;
- how risk selects suites, cases, environments and execution gates;
- how test cases are grouped without losing failure meaning;
- how product-validation paths are represented;
- how support/compatibility cells and supported maintenance transitions are qualified;
- how evidence is bound to candidate/artifact/environment/fixture identity;
- how every retained evidence requirement defines an explicit oracle and invalidation rule;
- how physical/native, hosted, deployment, viewer, network, performance, security, reliability, diagnostics and upgrade evidence coexist;
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
- inventing new upgrade/rollback promises;
- creating a second runtime/test-only product architecture;
- adding test-only public API;
- replacing current release evidence rules;
- weakening an existing release gate in order to meet a timing budget.

Those are later implementation decisions after this architecture is accepted.

---

## 2. Authority hierarchy

The test system has four authority levels. Lower levels implement higher levels; they do not redefine them.

```text
Product roadmap / product baseline / version policy / architecture
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

1. Product roadmap, compatibility and version authorities define **what HyRemote promises**.
2. `test-strategy.md` defines **what classes of evidence are required**.
3. This document defines **how the evidence system is structured**.
4. Current test inventory and CI implement the accepted design incrementally.
5. A current CTest identity is not a permanent architecture object merely because it exists today.
6. A design object is not evidence merely because it is documented.
7. Test infrastructure never creates product support/upgrade commitments by itself.

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
Test Suite / Case          Qualification Cell / Maintenance Edge
        |                          |
        v                          v
Execution Environment <---- Execution Gate
        |                          |
        +-------------+------------+
                      v
               Evidence Record
                      |
                      v
                Product Claim
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
- `PAC-9` Compatibility Truth;
- `PAC-10` Operability & Maintainability.

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
| `R-OPERATE` | diagnostics/supportability | effective facts, last error, troubleshooting, safe disable |
| `R-UPGRADE` | maintenance/version transition | stale/mixed artifacts, API/package break, rollback coherence |
| `R-ADOPTION` | user journey/self-service | discover -> integrate -> deploy -> connect -> operate -> maintain |

Risk domains may evolve, but they must stay product-oriented rather than mirror every source directory.

### 3.3 Evidence Requirement

An evidence requirement is the principal traceability unit. It says **what must be demonstrated**, not merely which executable should run.

Every retained product evidence requirement has these conceptual fields:

```yaml
requirement_id: ER-...
pac: [PAC-...]
risk: [R-...]
statement: product property to prove
evidence_classes: [V|P|Q|R]
required_environment_strength: ...
required_dimensions: ...
preferred_technical_seam: ...
oracle:
  type: deterministic|protocol-observation|origin-audit|measurement|human-observation
  pass_condition: explicit condition
invalidation:
  - change classes / artifact / environment / fixture facts that make old evidence inapplicable
```

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
Oracle:
  loaded Qt/HyRemote runtime origins are all inside the expected deployed lineage
Invalidation:
  deployment/runtime-resolution/package metadata changes; selected Qt anchor changes;
  release candidate identity changes for exact-R evidence
```

Evidence requirements, not current test names, are the main traceability unit.

### 3.4 Test Suite

A suite is an executable evidence unit with one owner, one scheduling shape and one failure domain. A suite may contain many named cases.

Examples of intended suite shapes:

- Core lifecycle suite;
- Runtime listener-policy suite;
- QML frontend mapping suite;
- QPA native-delegate suite;
- deployment positive suite;
- deployment negative suite;
- RFB protocol robustness suite;
- product-path C++ Widgets suite;
- diagnostics/effective-state suite;
- supported-version-transition suite;
- performance W3 interaction suite.

A suite is **not automatically a CTest executable**. The implementation may use one executable, one script, multiple processes or an orchestrator depending on isolation needs.

### 3.5 Test Case

A case is one named input/scenario within a suite.

Examples:

- `missing-qml-payload`;
- `stale-qpa-metadata`;
- `bind-specific-local-ip`;
- `wrong-password`;
- `stop-while-held-button`;
- `foreign-distribution-qt-present`;
- `upgrade-from-authorized-predecessor`;
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
- `E-THIRDPARTY` — pinned representative external Qt application environment;
- `E-MAINTENANCE` — isolated source-release -> target-candidate transition environment.

An environment is described by facts, not merely by a runner label.

### 3.7 Execution Gate

A gate controls **when evidence is collected**, not what the product means.

- `G0` Developer;
- `G1` PR Verification;
- `G2` PR Product;
- `G3` Merge Sentinel;
- `G4` Nightly Qualification-lite;
- `G5` Weekly/Extended;
- `G6` Release Qualification.

### 3.8 Qualification Cell / Maintenance Edge

A qualification cell is one explicit environment/product support boundary.

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

A maintenance edge is one explicitly supported transition:

```text
accepted source release/artifact
        -> target candidate/release
        -> optional rollback target when rollback is promised
```

The product does **not** execute full Cartesian products of either environment dimensions or historical versions. Cells are selected as anchor, interaction-risk or representative/pairwise cells; maintenance edges exist only when product/version authorities declare them.

### 3.9 Evidence Record

An evidence record is the immutable result of an executed requirement/suite/cell/maintenance edge. It contains enough identity and oracle outcome to decide whether it can support a claim.

### 3.10 Product Claim

A product claim is the downstream statement supported by evidence, for example:

- a compatibility cell/status in `docs/compatibility.md`;
- a declared viewer interoperability statement;
- an explicit supported maintenance transition;
- an exact release-acceptance decision.

A claim is downstream of evidence. Tests do not become mandatory merely because they exist; they are mandatory when a product claim or risk requires their evidence.

---

## 4. Traceability graph

Required direction:

```text
Product vision
  -> PAC
  -> risk domain
  -> evidence requirement + oracle + invalidation
  -> suite/case
  -> environment / qualification cell / maintenance edge
  -> gate execution
  -> evidence record
  -> product/release claim
```

Reverse questions must also be answerable:

```text
Why does this test exist?
  <- evidence requirement
  <- risk
  <- PAC/support claim

Why did this PR run this test?
  <- changed area/config/contract
  <- risk propagation
  <- selector rule

Why is this compatibility row Supported?
  <- qualification cell
  <- exact evidence records

Why is this upgrade path supported?
  <- declared maintenance edge
  <- exact transition evidence
```

Any current or future test that cannot eventually participate in this traceability should be classified as:

- repository/governance check;
- temporary investigation/preflight;
- redundant historical evidence;
- or a candidate for removal/consolidation.

---

## 5. Evidence strength, oracle and substitution model

Evidence has independent properties:

1. **class** — Verification, Product Validation, Qualification or Release Acceptance;
2. **environment strength** — synthetic/hosted/deployed/native/reference/etc.;
3. **oracle strength** — what observation decides correctness;
4. **applicability** — artifact/environment/fixture facts under which the record remains relevant.

A higher-cost environment is not automatically better for every question.

Examples:

- Core mailbox ordering is best proved deterministically, not on a physical desktop.
- Linux loader/Qt-origin correctness requires a real deployment/loader environment.
- local+remote native coexistence requires native/physical evidence.
- interaction latency requires a measurement oracle with a defined workload/SLO.
- a release claim requires exact-candidate evidence even if an older candidate passed the same scenario.

### 5.1 Oracle types

**Deterministic assertion** — exact model/state/output condition.

**Protocol observation** — exact wire/session behavior or maintained-viewer observable result.

**Origin/artifact audit** — loaded files, package metadata or deployment closure match the expected lineage.

**Measurement** — numeric result compared with a declared workload and threshold/baseline.

**Human observation** — used only where native/physical/user-visible behavior cannot be faithfully automated. The observation checklist and PASS/FAIL criteria must be explicit; a screenshot/video alone is supporting material, not the oracle definition.

### 5.2 Evidence substitution rules

Allowed:

- a stronger exact-environment run may satisfy a weaker environment requirement if it exercises the same property and remains diagnosable;
- a deterministic shared-layer test may replace four duplicate frontend copies when the frontend contributes no unique semantics.

Not allowed:

- hosted/headless replacing native local-display/input evidence;
- Windows replacing Linux or vice versa for platform-sensitive claims;
- nearby Qt patch replacing exact QPA private-ABI evidence;
- a third-party application replacing controlled compatibility cells;
- physical evidence replacing clean package/deployment evidence;
- an older SHA replacing exact RC evidence;
- a successful target-version launch replacing an explicitly declared upgrade-transition test;
- CI workflow success with zero relevant execution replacing evidence.

### 5.3 Evidence invalidation

Every evidence requirement declares what invalidates retained evidence.

Typical invalidators include:

- protected source/behavior changes;
- public API/package/deployment metadata changes;
- Qt/toolchain/native-platform/graphics anchor changes;
- viewer/fixture revision changes where the claim names that fixture;
- performance workload/reference-host changes;
- product support-status or maintenance-policy changes;
- exact candidate/artifact changes for Release Acceptance.

A record never becomes false retroactively: it remains historical truth for the artifact/environment it measured. Invalidation means it can no longer satisfy a newer claim without re-execution or an explicit authority-approved equivalence rule.

---

## 6. Suite architecture

The future executable system is organized into logical suite families. This is not a command to move files immediately.

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
| `S-OPERABILITY` | effective diagnostics, disable/recovery/supportability | V/P/Q |
| `S-UPGRADE` | authorized version transitions and rollback coherence | V/P/Q/R |
| `S-SECURITY` | policy truth + mechanism-specific security | V/P/Q |
| `S-RELIABILITY` | stress/soak/fuzz/resource bounds | V/Q |
| `S-PERFORMANCE` | interaction SLO/resource regression | V/Q/R |
| `S-COMPAT` | qualification-cell orchestration | Q/R |
| `S-REALWORLD` | representative third-party pressure tests | Q |

Repository/release governance remains outside this product suite taxonomy.

### 6.1 Shared semantics versus frontend semantics

```text
Core invariant               -> S-CORE
Qt-aware shared behavior     -> S-RUNTIME
surface/capture/input detail -> S-ADAPTER
entry syntax/activation      -> S-FRONTEND
wire/protocol behavior       -> S-TRANSPORT
artifact closure             -> S-PACKAGE/S-DEPLOY
user journey                 -> S-PRODUCT-PATH
runtime support facts        -> S-OPERABILITY
version transition           -> S-UPGRADE
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
- source/target version-transition setup;
- selector ownership;
- expected-result model;
- evidence retention requirement.

Otherwise prefer named table-driven cases.

### 6.3 Failure granularity

Consolidation must preserve:

- exact case name;
- violated evidence requirement/PAC;
- environment/fixture identity;
- oracle/pass condition and actual observation;
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

Conceptual maintenance case:

```yaml
case_id: upgrade-authorized-predecessor-to-candidate
requirement: ER-UPGRADE-ARTIFACT-COHERENCE
source_release: policy-defined
expected: one-target-lineage-no-stale-payload
```

### 7.1 Metadata principles

- metadata describes evidence meaning, not implementation mechanics only;
- a suite may map to one or many current CTest identities during migration;
- selectors consume metadata rather than hard-coded test-name folklore;
- PAC/risk mapping must be reviewable in normal code review;
- cost/platform/capability metadata is explicit;
- unknown/malformed metadata fails closed for product lanes;
- metadata must not require every developer to understand release governance;
- product support/upgrade policy is referenced, not duplicated as a second authority.

---

## 8. Change-impact and selection architecture

Selection is a graph problem, not a filename-glob problem.

```text
changed files / build options / product config / public contract
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
- diagnostics/operability;
- public-version/package-transition;
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
deploy-helper -> R-DEPLOY/R-PACKAGE/R-COMPAT/R-UPGRADE
              -> deploy/package contract suites
              -> Windows + Linux clean product deployment paths
              -> authorized maintenance transition if runtime/package replacement semantics changed
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

Diagnostics/public runtime facts:

```text
diagnostics -> R-OPERATE/R-SECURITY/R-ADOPTION
            -> effective-state/secret-safe deterministic suites
            -> representative installed product-path evidence
            -> no automatic graphics qualification
```

Public package/version compatibility:

```text
public package/version contract -> R-PACKAGE/R-UPGRADE/R-COMPAT
                                -> API/package contract V
                                -> authorized predecessor transition P/Q when the change can affect it
                                -> no arbitrary all-history matrix
```

### 8.3 Selector invariants

- a product lane with selected risk must execute at least one relevant suite;
- zero relevant execution is invalid evidence;
- a selector may broaden conservatively when ownership is unknown;
- selector failure must not silently return an empty test set;
- full-gate override remains available for uncertainty/RC use;
- path-based selection is only one input; public API/package metadata/config/support-policy changes may propagate beyond direct file paths;
- selector rules themselves are tested deterministically;
- selection output records **why** each expensive suite was included.

### 8.4 Change-risk overrides

Some changes are automatically high-propagation regardless of file count:

- public API contract;
- Shared Runtime state/security semantics;
- build/install/export/package metadata;
- deployment runtime resolution;
- QPA private ABI/native delegate logic;
- transport parser/security framing;
- public diagnostic/effective-state schema;
- supported maintenance/version-transition contract;
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

Goal: prove affected vertical product/deployment/maintenance path, target <= 3–5 minutes wall time.

Typical triggers:

- public frontend/API behavior;
- package/install/export;
- deployment helper/runtime closure;
- QPA/native plugin loading;
- security/transport behavior;
- canonical adoption/example path;
- public diagnostics/effective-state behavior;
- supported update/rollback/package-transition behavior;
- compatibility-critical changes.

G2 may run independent platform paths where loader/native behavior differs.

### 9.4 G3 — Merge Sentinel

Goal: <= 60 seconds and nonzero.

It answers only:

> Did integrated mainline become obviously unusable or did selection produce a false green?

It does not replay full PR qualification.

### 9.5 G4 — Nightly Qualification-lite

Goal: broad evidence <= 30 minutes target through parallelism and bounded matrices.

Includes selected:

- peer frontend product paths;
- clean deploy/relocation;
- viewer interoperability subset;
- network impairment subset;
- stress-lite;
- performance trend;
- diagnostics/product-state path;
- authorized maintenance-transition subset;
- broader compatibility anchors.

### 9.6 G5 — Weekly/Extended

Carries expensive evidence that should not affect daily development:

- long stress/repetition;
- fuzz campaigns;
- extended viewer/network matrix;
- extended benchmark workloads;
- longer soak;
- broader authorized maintenance edges if needed;
- selected third-party applications where automation is practical.

### 9.7 G6 — Release Qualification

G6 is not optimized for developer feedback time. It proves the exact frozen candidate against the declared release/support envelope.

Requirements include as applicable:

- hosted deterministic evidence;
- clean package/install/deploy evidence;
- exact qualification cells;
- supported maintenance-transition evidence;
- physical/native local+remote evidence;
- maintained viewer evidence;
- security boundary;
- performance SLO;
- required stress/soak;
- real-world representative evidence;
- diagnostics/supportability evidence;
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

Track at least:

- per-suite P50/P95 runtime;
- gate wall time;
- critical-path suite(s);
- queue/wait time versus execution time;
- setup/configure/build/install time;
- external viewer/device acquisition time;
- maintenance source/target artifact setup time;
- retry/flake cost;
- selected-suite count by risk domain;
- percentage of runs executing expensive installed/E2E suites;
- duplicate execution across OS where no platform sensitivity exists.

Do not optimize only by counting tests or summing durations.

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

Clean deployment/qualification/maintenance-transition evidence must explicitly control the environment being validated. A cache must not reintroduce:

- source/build-tree search paths;
- stale installed metadata;
- foreign plugin paths;
- SDK runtime crutches;
- mixed candidate artifacts;
- source-release artifacts not deliberately part of an upgrade/rollback case.

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
 -> diagnose
 -> update/upgrade where supported
 -> verify coherent target state
 -> rollback where explicitly supported
 -> maintain
```

Not every product line must support every maintenance step. The product/version authority decides which transition edges exist.

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

Each vertical path records as applicable:

- acquisition form;
- application source/link contract;
- exact Qt/HyRemote identity;
- deployment form;
- runtime dependency origin;
- native platform identity;
- listener/security/input configuration;
- viewer/version;
- launch success;
- view/control/reconnect result;
- stop/application-survival result;
- diagnostic/effective-state result;
- source/target version-transition identity when applicable;
- rollback result when explicitly promised;
- oracle and failure evidence.

---

## 12. Deployment and maintenance evidence architecture

Deployment is a first-class product subsystem. Maintenance transitions build on deployment correctness rather than bypass it.

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
- illegal frontend combination where prohibited;
- missing package/payload;
- runtime closure contaminated by foreign Qt lineage;
- accidental dependency on original build/SDK tree.

Negative inputs normally belong in table-driven suites unless they require a different environment.

### 12.4 Linux Qt-lineage rule

For Qt libraries, the selected qualified Qt SDK/deployed copy is the positive trust source. Distribution Qt is environmental runtime for unrelated applications, not an alternate HyRemote development SDK.

The test system should prove positive ownership/origin rather than maintain brittle global blacklists of `/lib` or `/usr/lib`.

### 12.5 Supported update/rollback transitions

The test system models a maintenance transition only when product/version authorities declare it.

A transition case establishes:

```text
known source artifact
 -> normal documented update/install/deploy mechanism
 -> target candidate artifact
 -> runtime/deployment origin audit
 -> launch/product-path validation
 -> optional documented rollback
```

Required checks include as applicable:

- no stale/mixed HyRemote library/plugin/QML/package metadata;
- no stale QPA payload from an incompatible Qt private-ABI line;
- effective diagnostic version/artifact identity matches the target;
- documented public API/package compatibility remains true;
- rollback does not mix source and target artifacts.

Do not test every historical release pair. The default matrix is the minimum set of product-authorized predecessor edges needed to support the declared maintenance policy.

---

## 13. Native non-interference qualification

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

Headless/offscreen/Xvfb environments intentionally remove or alter compositor, native platform plugin, physical scaling, GPU/driver and local input/display properties. They remain valuable regression environments but cannot independently prove PAC-2 for native support cells.

---

## 14. Compatibility and qualification-cell design

### 14.1 Three environment-cell classes

**Anchor cells** — complete reference cells required for every relevant release line.

**Interaction-risk cells** — selected where dimensions strongly interact, for example:

- QPA x exact Qt patch x OS;
- graphics backend x OS/GPU;
- deploy x OS loader;
- Qt family x public frontend;
- security x transport;
- DPR x native display stack.

**Representative/pairwise cells** — used to avoid an unbounded Cartesian matrix while covering meaningful interactions.

### 14.2 Qualification statuses

Status semantics are owned by compatibility/product authorities, for example:

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

### 14.5 Maintenance-edge matrix

Maintenance transitions are a separate sparse graph, not another compatibility Cartesian product.

```text
release A ----> release B candidate
release B ----> release C candidate
```

Edges are admitted only by version/product policy. An absent edge means “not claimed/tested by this policy,” not automatic compatibility or incompatibility.

---

## 15. Viewer interoperability and network architecture

### 15.1 Viewer interoperability

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

### 15.2 Network impairment

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

Expected results are expressed as correctness, boundedness, freshness/frame age, recovery, native UI health, resource growth and interaction latency — not merely “TCP remained connected”.

---

## 16. Reliability, stress, soak and fuzz

### 16.1 Deterministic first

Every reliability property that can be deterministically modeled should be proved cheaply first:

- bounded queue size;
- one teardown owner;
- cancellation semantics;
- held-input cleanup;
- late callback suppression;
- parser bounds;
- per-viewer pending-work bounds.

### 16.2 Repetition/stress second

Reference scenario families:

- repeated start/stop;
- repeated connect/disconnect;
- resize/surface churn;
- input flood;
- slow/stalled client;
- multiple viewers;
- target destruction during activity.

Exact counts are executable-plan parameters, not permanent architecture constants.

### 16.3 Soak

Idle and active soak observe:

- RSS;
- thread count;
- FD/handle count;
- sockets;
- queue/backlog;
- CPU/GPU;
- crash/hang;
- latency drift.

### 16.4 Fuzz

Fuzz targets private/parsing/config seams such as:

- RFB protocol parser;
- security/config descriptors;
- selected metadata parsers where malformed external input exists.

Fuzzing is an extended gate and should not block ordinary PR feedback unless a specific regression test becomes deterministic and cheap.

---

## 17. Security evidence architecture

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
- secrets/logging/diagnostic redaction;
- malformed/stalled client resource bounds;
- dependency/runtime provenance;
- future encrypted transport/certificate policy when implemented.

Security mechanism-specific suites sit beneath this product policy contract.

---

## 18. Performance evidence architecture

### 18.1 Primary metric

The product metric is Remote Interaction Latency:

```text
viewer sends action
 -> HyRemote receives/routes input
 -> Qt application changes
 -> capture becomes available
 -> schedule/encode/send
 -> viewer observes corresponding state
```

### 18.2 Workloads

Reuse the long-lived performance programme workload classes:

- W1 Static;
- W2 Localized UI;
- W3 Interactive;
- W4 High Motion.

### 18.3 Diagnostic metrics

- input-to-Qt latency;
- render-to-capture/capture latency;
- frame age;
- encode latency;
- bytes/update/time;
- RTT/backlog;
- CPU/GPU/memory;
- local UI impact.

These explain the primary metric; they do not replace it.

### 18.4 Performance gates

- G1: deterministic/perf-smoke only where change risk requires it;
- G4: trend/reference subset;
- G5: broader benchmark workloads;
- G6: exact qualified reference SLO and release envelope.

A reproducible performance regression remains visible even when functional tests pass.

---

## 19. Operability and diagnostics evidence architecture

The top-level test object is whether a normal user/operator can understand and safely operate the installed product without source-code or CI knowledge.

Required evidence, according to the public product surface, includes:

- product/build identity;
- Qt/OS/architecture facts;
- active integration route/UI family facts where relevant;
- Runtime lifecycle state;
- effective listener endpoint(s);
- effective security/access policy;
- remote-input policy;
- connected-client count;
- bounded last error/startup failure;
- installed/deployed artifact identity;
- no secret material in diagnostic output;
- distinction between stopped/configuration/listener/deployment/runtime classes where technically observable;
- safe stop/disable with application survival.

The detailed user-facing diagnostics capability remains owned by its product authority; this architecture defines how its claims become evidence.

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

## 21. Evidence record and validity model

Every retained Q/R evidence record should conceptually contain:

```yaml
record_id: immutable-id
candidate_sha: exact-source-sha
artifact_identity: exact-built/staged-artifact-id
source_artifact_identity: ...      # maintenance edge only
target_artifact_identity: ...      # maintenance edge only
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
fixtures:
  viewer: ...
  application_revision: ...
  network_profile: ...
oracle:
  type: ...
  pass_condition: ...
  observation: ...
support_claims: [...]
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

- prior exact-candidate physical/native or other required R evidence is historical/preflight for the new candidate unless the canonical release authority explicitly defines an equivalent artifact identity rule;
- this architecture does **not** permit mixing different candidate SHAs into one release PASS merely because a change was described as evidence-neutral;
- the release change-control authority decides whether the candidate remains frozen, is re-cut, or affected cells must be rerun.

### 21.2 Qualification versus release evidence

Long-lived qualification results may inform future planning and reduce discovery work, but release acceptance still applies exact-candidate rules defined by the release authority.

### 21.3 Evidence validity

A retained record is valid only for the product/environment/fixture facts named by its requirement and record.

When invalidated, the record remains historical evidence but cannot satisfy a new positive support/release claim until:

- the requirement is re-executed; or
- the owning product/release authority explicitly records an equivalence decision that preserves the relevant artifact/environment semantics.

The test system must not infer equivalence from matching filenames, version-family proximity or an “evidence-neutral” label alone.

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

Quarantine cannot silently shrink a positive support claim.

### 22.4 Blocked evidence

`BLOCKED` is not PASS and must remain visible to qualification/release decisions.

---

## 23. Test lifecycle governance

### 23.1 Adding a test/case

Every permanent addition answers:

1. Which PAC/support/maintenance claim does it protect?
2. What risk domain and failure mode does it cover?
3. Why is existing evidence insufficient?
4. What is the cheapest sufficient evidence layer?
5. What is the explicit oracle/pass condition?
6. What invalidates previously retained evidence?
7. Is this a case or truly a new suite/identity?
8. Which gates/environments need it?
9. What is its expected wall-time/setup cost?
10. What changes trigger it?
11. What is the failure meaning?

### 23.2 Consolidating tests

Consolidation is preferred when cases share setup, owner, scheduling, capability/environment and failure category. It must retain precise named-case diagnostics and oracle output.

### 23.3 Moving a test to another gate

Move when evidence remains required but its frequency/cost is disproportionate to normal change risk.

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
- accepted oracle/invalidation model;
- accepted velocity budgets;
- accepted evidence-binding rules.

### Phase M1 — Read-only inventory mapping

No behavior/scheduling changes.

For every current test identity record:

- PAC(s);
- risk domain(s);
- evidence class;
- evidence requirement(s) currently proved or intended;
- current oracle/pass condition quality;
- suite-family candidate;
- platform/capability sensitivity;
- current triggers/gates;
- current cost/setup cost;
- current evidence retention/invalidation relevance;
- duplicate/unique evidence assessment;
- candidate disposition: retain identity / parameterize / move gate / governance / investigate.

M1 produces a migration report, not test edits.

### Phase M2 — Metadata/registry foundation

Introduce machine-readable logical metadata without initially changing test behavior.

Goals:

- selector can reason about suites/PAC/risk/cost;
- current CTest names can map many-to-one into future suites during transition;
- current execution remains comparable;
- product policies are referenced rather than copied into a second authority.

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
- visible execution manifest/reasoning;
- measurable PR critical-path improvement;
- no product evidence loss.

### Phase M5 — Product-validation orchestration

Make G2/G4 product-path evidence explicit and reusable:

- clean acquisition;
- install/deploy;
- isolation/relocation;
- launch/viewer;
- frontend product paths;
- diagnostics;
- supported maintenance transitions when declared.

### Phase M6 — Qualification evidence framework

Prepare the V0.4 qualification machinery before V0.4 entry:

- qualification-cell registry;
- maintenance-edge registry/reference;
- environment/fixture identity;
- evidence requirements/oracles/invalidation;
- evidence records;
- exact-SHA/artifact binding;
- physical/native records;
- performance/security/reliability integrations;
- compatibility/maintenance-claim traceability.

### Phase M7 — Continuous test-system health

Long-lived maintenance:

- gate SLO monitoring;
- flake monitoring;
- duplicate evidence reviews;
- obsolete test cleanup;
- qualification refresh/invalidation policy;
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

As self-service SDK/deploy/diagnostics/productization becomes real, M2–M5 can be introduced incrementally where they directly support those product paths.

The test-system work must not replace product work; it exists to prove those outcomes efficiently.

### 25.3 V0.3 family breadth convergence

Before V0.4 entry, the test system must be capable of representing/collecting evidence for the mandatory GA matrix without exploding into an unbounded Cartesian CI matrix.

Therefore M4–M6 should converge during the V0.3 family alongside the relevant Qt/platform/transport/product work.

### 25.4 V0.4.0.0

V0.4 is qualification only.

By entry:

- qualification-cell/evidence machinery must already exist;
- product paths must already be testable;
- test selection/gate architecture must already be stable enough not to become hidden feature development;
- any declared maintenance policy must already be representable as sparse transition edges;
- V0.4 executes/collects/decides qualification evidence using existing product/domain authorities.

### 25.5 V1.0.0.0

GA consumes the qualified evidence and freezes the first long-lived support/compatibility contract.

Test-system changes during final GA acceptance are limited to release-blocking evidence corrections; they do not redesign the architecture.

---

## 26. Implementation work-package plan

These are **planned work packages**, not yet separate mandatory release-train children and not authorization to edit tests before this design is accepted.

| WP | Work package | Depends on | Main output | Product/release placement |
| --- | --- | --- | --- | --- |
| `TS-0` | Test strategy + detailed architecture freeze | #393 | accepted design, no test changes | now; non-blocking V0.2 architecture work |
| `TS-1` | Current inventory semantic/cost audit | TS-0 | read-only mapping report for every current test | after design; may run alongside V0.2/V0.3 planning |
| `TS-2` | Logical metadata/registry foundation | TS-1 | PAC/risk/requirement/suite/gate metadata mapped to current tests | V0.3 family, incremental |
| `TS-3` | Suite/identity consolidation | TS-1, TS-2 | parameterized suites + governance separation, no evidence loss | V0.3 family, bounded PRs |
| `TS-4` | Risk selector + G0/G1/G2/G3 orchestration | TS-2 | change-risk execution manifest and gate budgets | V0.3 family before broad matrix growth |
| `TS-5` | Product-path validation harness | TS-2, productized deploy paths | clean install/deploy/launch/viewer/diagnostics reusable scenarios | aligned with V0.3 self-service/productization |
| `TS-6` | Qualification/evidence-record framework | TS-2, TS-5, product support matrix | cells/maintenance edges, environment/fixture/evidence identity, support traceability | must converge before V0.4 entry |
| `TS-7` | Extended reliability/security/performance/real-world integration | TS-6 + existing domain authorities | domain evidence wired into common gates/records | V0.3 breadth/V0.4 qualification |
| `TS-8` | Continuous test-system SLO/governance | TS-4 onward | runtime/flake/duplicate/evidence health review | ongoing, not a separate product feature |

### 26.1 No issue explosion rule

Do not create one GitHub Issue per row/case now.

After TS-0 is accepted:

1. execute TS-1 first;
2. use actual audit findings to decide which implementation packages are independently mergeable;
3. create only the minimum focused Issues needed for those packages;
4. link each package to existing product authorities rather than duplicate them;
5. do not add a work package to release-train machine authority merely because it exists — only exact product/release authorities decide whether it is a mandatory child or prerequisite.

---

## 27. Relationship to existing product authorities

The test-system programme consumes existing ownership instead of replacing it.

| Existing authority | Relationship |
| --- | --- |
| #1 product roadmap | top-level product sequencing; test programme is cross-cutting support |
| #24 version semantics | defines user-visible version/support boundary; test system does not invent upgrade promises |
| #343 GA compatibility/product baseline | defines mandatory support dimensions before V0.4 |
| #57 compatibility qualification | owns Qt/support qualification content |
| #109 physical/native acceptance | owns physical/native acceptance evidence |
| #134 / #242 real-world programme | owns representative third-party pressure testing |
| #370 performance programme / #9 qualification | owns performance metrics/SLO/qualification content |
| #143 and security authorities | own security product capability/truth |
| #335 diagnostics | owns minimum self-service diagnostic product capability |
| #165 candidate freeze/change control | owns release candidate invalidation/retest decisions |
| #95/release-train authority | owns version/release mechanics |

The test architecture supplies common evidence plumbing and execution economics; it does not take product scope away from these authorities.

---

## 28. Detailed-design acceptance criteria

TS-0 is complete only when reviewers can answer all of the following without referring to the current CTest count:

1. What product promises are being protected?
2. What risk domains can violate them?
3. What evidence requirement proves each important risk?
4. What is the explicit oracle/pass condition for retained evidence?
5. What invalidates retained evidence?
6. Which technical seam is the cheapest honest evidence source?
7. When is a variant a case versus a separate suite/identity?
8. How does a code/product change select evidence?
9. How are Windows/Linux/Qt/QPA/platform differences represented?
10. How are clean deployment and runtime origin proved?
11. How are physical/native claims kept separate from hosted evidence?
12. How are viewer/network/security/performance/reliability dimensions handled without Cartesian explosion?
13. How are diagnostics/operability proved as product behavior rather than logging implementation detail?
14. How are supported version transitions represented without an all-history Cartesian matrix?
15. How is every retained qualification/release result bound to exact environment/candidate/artifact/fixture identity?
16. How does the architecture prevent false-green zero-test selection?
17. How does it prevent testing from slowing normal development unnecessarily?
18. How are flaky/infrastructure failures distinguished from product failures?
19. How can current tests migrate incrementally without a big-bang rewrite?
20. Which work happens before V0.4 entry and which work is qualification execution inside V0.4?
21. Which existing product authorities remain owners of their domain evidence?
22. What must be measured before declaring the new test system better than the current one?

Until these are accepted, no broad current-test migration should begin.

---

## 29. Bounded implementation choices to close before TS-2

The architecture is now intended to be stable enough for TS-1. The following are implementation-storage/tooling choices that TS-1 may inform; they must be closed before the affected implementation package starts, not by inventing a framework now:

1. **Machine-readable metadata storage** — CMake properties, declarative manifest, generated registry or a hybrid; choose the smallest form that supports traceability/selectors without two authorities.
2. **Suite runner boundary** — where table-driven cases belong in C++/QtTest, CMake script or Python orchestration.
3. **Evidence record serialization/retention** — exact machine format and artifact location for Q/R evidence.
4. **Environment registry** — how hosted/reference/physical hosts publish stable non-secret facts.
5. **Selector fallback** — exact conservative policy for unknown files/metadata drift.
6. **Cost telemetry retention** — where per-suite/gate P50/P95 and critical-path metrics live.
7. **Quarantine representation** — how temporary non-blocking status exposes the missing PAC/evidence gap.
8. **Compatibility traceability rendering** — generated evidence summaries versus machine-readable qualification registry links.
9. **Maintenance-edge registry rendering** — how product-authorized source->target transitions are referenced without duplicating version authority.

None of these choices justifies changing current test scheduling during TS-0 or TS-1.

---

## 30. Architecture success criteria

The future implementation is successful only if all of these become true:

- product confidence is at least as strong as today;
- every important product claim is traceable to executed evidence;
- every retained evidence requirement has an explicit oracle and invalidation rule;
- ordinary PRs run materially less unrelated work;
- deployment/native/platform risks still receive the stronger evidence they need;
- Core/Runtime/frontends no longer duplicate shared proof unnecessarily;
- case growth does not automatically cause CTest-identity growth;
- test failures become more diagnosable;
- self-service diagnostics are verified as a product capability;
- declared maintenance transitions are qualified without an all-history test explosion;
- support qualification becomes evidence-driven and exact-environment bound;
- exact release acceptance never mixes incompatible candidate identities;
- current V0.2 delivery is not delayed by a broad testing rewrite;
- V0.4 enters qualification with the evidence machinery already ready;
- test-system wall time, flake and duplicate execution are measured and controlled as engineering budgets;
- the architecture remains valid when HyRemote adds Qt versions, ARM64, Wayland/EGLFS, another transport, hardware acceleration or later long-lived release lines.

The target is not a smaller test number. The target is **stronger product evidence per unit of development time**.
