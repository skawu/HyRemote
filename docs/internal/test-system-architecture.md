# HyRemote Test System Architecture

Status: **TS-0 detailed design — no existing test/CI migration is authorized by this document alone**

Authority: #393

Parent strategy: [`test-strategy.md`](test-strategy.md)

Evidence Requirement catalog: [`test-evidence-requirements.md`](test-evidence-requirements.md)

Product architecture: [`../architecture.md`](../architecture.md)

This document defines **how HyRemote's product evidence system is structured**. It deliberately does not rename, remove, consolidate, re-register or reschedule any current test.

The current CTest inventory and CI behavior remain executable authority until later focused migration is explicitly admitted.

The TS-0 rule is:

> Design and review the evidence system completely before changing the executable test system.

The architecture is intentionally independent of CTest, GitHub Actions, CMake, QtTest, Python or any particular laboratory tool.

---

## 1. Scope and non-goals

### This document designs

- logical test-system objects and their relationships;
- PAC/risk/ER traceability;
- suite/case identity and ownership;
- change-risk propagation and selection;
- execution environments and G0–G6 gates;
- product-path and qualification orchestration;
- evidence record generation, identity and invalidation;
- physical/native/manual evidence handling;
- supported maintenance-transition representation;
- flaky/infrastructure/quarantine semantics;
- runtime and maintenance-cost budgets;
- incremental migration of the current inventory;
- placement of TS-0..TS-8 in the existing product roadmap.

### This document does not authorize

- deleting/renaming existing CTests;
- changing `TEST_CATALOG.md` dispositions;
- changing `.github/workflows/ci.yml` selection;
- changing current labels/registration;
- changing release-train mandatory children;
- changing compatibility status/claims;
- inventing upgrade/rollback promises;
- adding test-only public API;
- weakening existing release gates;
- creating a second Runtime or test-only product architecture.

---

## 2. Authority hierarchy

```text
Product roadmap / architecture / compatibility / version policy
                         |
                         v
              Product test strategy
          docs/internal/test-strategy.md
                         |
                         v
             Test-system architecture
    docs/internal/test-system-architecture.md
                         |
                         v
          Product Evidence Requirements
 docs/internal/test-evidence-requirements.md
                         |
                         v
     Current executable inventory / selectors
 TEST_CATALOG / TEST_MATRIX / CTest / workflows
```

Rules:

1. Upstream product authorities define **what HyRemote promises**.
2. Test Strategy defines the stable PAC/evidence philosophy and execution economics.
3. This document defines the test-system object model and orchestration.
4. The ER catalog defines the stable product properties to prove and their oracle/invalidation rules.
5. Current executable tests implement those requirements incrementally after audit.
6. A current CTest identity is not a permanent architecture object merely because it exists.
7. A test-system object never creates a product compatibility, security, upgrade or rollback promise by itself.
8. Compatibility status values are opaque upstream product data; test infrastructure does not maintain a competing status enum.

---

## 3. Core conceptual model

```text
Product Acceptance Contract (PAC)
        |
        v
Risk Domain
        |
        v
Evidence Requirement (ER)
  + oracle + invalidation
        |
        +-------------------------------+
        |                               |
        v                               v
Test Suite / Case           Qualification Cell / Maintenance Edge
        |                               |
        v                               v
Execution Environment <------ Execution Gate
        |                               |
        +---------------+---------------+
                        v
                 Evidence Record
                        |
                        v
                  Product Claim
```

### 3.1 PAC

PACs are stable product promises from `test-strategy.md`:

- PAC-1 Low Intrusion;
- PAC-2 Native Non-interference;
- PAC-3 Remote Experience;
- PAC-4 Frontend Equivalence;
- PAC-5 Deployability;
- PAC-6 Reliability & Boundedness;
- PAC-7 Security Truth;
- PAC-8 Responsiveness & Efficiency;
- PAC-9 Compatibility Truth;
- PAC-10 Operability & Maintainability.

PAC IDs are not test names or CI jobs.

### 3.2 Risk Domain

Risk domains describe **how a product promise may be broken**.

Initial vocabulary:

| Risk | Meaning |
| --- | --- |
| `R-CORE` | Core lifetime/state/scheduling/backpressure invariants |
| `R-RUNTIME` | Shared Runtime lifecycle/policy/composition |
| `R-CAPTURE` | Widgets/Quick capture, DPR, damage, resize |
| `R-INPUT` | input normalization/routing/focus/text/held state |
| `R-FRONTEND` | C++/QML/Generic/QPA activation/mapping |
| `R-NATIVE` | local display/input/platform/graphics non-interference |
| `R-TRANSPORT` | transport/RFB protocol and delivery |
| `R-NETWORK` | listener/reachability/stall/disconnect/impairment |
| `R-SECURITY` | security policy, auth, downgrade, secrets |
| `R-PACKAGE` | package/export/acquisition contract |
| `R-DEPLOY` | deployment/runtime closure/relocation/origin |
| `R-COMPAT` | support-matrix truth |
| `R-PERF` | latency/freshness/resources |
| `R-RELIABILITY` | repetition/leak/churn/soak/fuzz |
| `R-OPERATE` | diagnostics/supportability/safe disable |
| `R-UPGRADE` | authorized maintenance transitions/artifact coherence |
| `R-ADOPTION` | end-to-end self-service journey |

Risk domains remain product-oriented; they do not mirror every source directory.

### 3.3 Evidence Requirement

ER is the principal traceability unit. The canonical ER list is in `test-evidence-requirements.md`.

Conceptually:

```yaml
requirement_id: ER-...
pac: [PAC-...]
risk: [R-...]
activation: INVARIANT|CLAIM|CAPABILITY|MAINTENANCE|RELEASE
statement: product property to prove
evidence_classes: [V|P|Q|R]
required_environment_strength: ...
required_dimensions: ...
preferred_technical_seam: ...
oracle:
  type: deterministic|protocol-observation|origin-audit|measurement|human-observation
  pass_condition: explicit condition
invalidation:
  - changes that make retained evidence inapplicable to a newer claim
```

ERs are upstream of current test identities. TS-1 maps tests to ERs; it does not invent the ER model from current tests.

### 3.4 Test Suite

A suite is an executable evidence unit with one primary owner, one scheduling shape and one failure domain. It may contain many named cases.

A suite is not automatically one CTest executable. It may be a QtTest binary, CMake/Python orchestrator, several cooperating processes or another implementation appropriate to the evidence.

### 3.5 Test Case

A case is a named scenario/input within a suite. Separate executable identity is justified only by real differences in platform/capability registration, environment, isolation, timeout/cost, external resource, selector ownership or evidence retention.

### 3.6 Execution Environment

Environment describes facts required to interpret evidence, not merely a runner label.

Environment classes may include:

- `E-SYNTHETIC`;
- `E-HOSTED-LINUX`;
- `E-HOSTED-WINDOWS`;
- `E-DEPLOY-LINUX`;
- `E-DEPLOY-WINDOWS`;
- `E-NATIVE-LINUX`;
- `E-NATIVE-WINDOWS`;
- `E-NETWORK-LAB`;
- `E-PERF-REF`;
- `E-EMBEDDED`;
- `E-THIRDPARTY`;
- `E-MAINTENANCE`.

Material facts include OS, CPU, Qt/toolchain, native platform, graphics stack, DPR/display, external viewer/application/network fixture and product artifact identity as required by the ER.

### 3.7 Execution Gate

A gate determines **when evidence executes**, not what evidence class it automatically becomes:

- G0 Developer;
- G1 PR Verification;
- G2 PR Product;
- G3 Merge Sentinel;
- G4 Nightly Qualification-lite;
- G5 Weekly/Extended;
- G6 Release Qualification.

A hosted synthetic G4 run remains V/P if the ER requires native Q evidence.

### 3.8 Qualification Cell

A qualification cell represents one explicit environment/product boundary. Material dimensions may include:

```text
Qt anchor
x OS
x CPU
x native platform/delegate
x graphics path
x UI family
x frontend
x deployment form
x viewer/security/network/performance profile where material
```

The product does not execute a full Cartesian product.

### 3.9 Maintenance Edge

A maintenance edge exists only when product/version authority declares a supported transition:

```text
accepted source release/artifact
        -> target candidate/release
        -> optional rollback target if rollback is promised
```

An absent edge creates no implied compatibility/incompatibility claim.

### 3.10 Evidence Record

An Evidence Record is the immutable machine/human record produced by an executed ER/suite/cell/edge. It must contain enough identity and oracle outcome to determine whether it can support a claim.

### 3.11 Product Claim

A Product Claim is downstream of evidence, for example a compatibility row/status, viewer interoperability statement, maintenance transition or release acceptance decision.

The status vocabulary/meaning comes from the product authority (`docs/compatibility.md` today), not this architecture.

---

## 4. Traceability requirements

Forward traceability:

```text
Product vision
 -> PAC
 -> Risk
 -> ER + oracle + invalidation
 -> Suite / Case
 -> Environment / Cell / Maintenance Edge
 -> Gate execution
 -> Evidence Record
 -> Product Claim
```

Reverse questions must also be answerable:

```text
Why does this test exist?
 <- ER <- risk <- PAC/claim

Why did this PR run it?
 <- changed ownership/contract <- risk propagation <- selector

Why is this compatibility claim valid?
 <- cell <- required ERs <- valid evidence records

Why is this maintenance transition supported?
 <- declared edge <- exact transition evidence
```

A current/future check that cannot map into product traceability is classified instead as:

- repository/governance control;
- temporary investigation/preflight;
- redundant historical evidence;
- or candidate for consolidation/removal.

---

## 5. Evidence strength, oracle and substitution

Evidence has independent attributes:

1. class — V/P/Q/R;
2. environment strength;
3. oracle type/strength;
4. artifact/environment/fixture applicability;
5. invalidation conditions.

### 5.1 Oracle types

**Deterministic assertion** — exact state/model/output condition.

**Protocol observation** — exact wire/session behavior or maintained-viewer observable result.

**Origin/artifact audit** — loaded files/package/deployment closure match expected lineage.

**Measurement** — numeric result compared with declared workload/SLO/baseline.

**Human observation** — only when native/physical/user-visible behavior cannot be faithfully automated; checklist and PASS/FAIL conditions are predefined.

A screenshot/video may support a human observation but is not the oracle definition by itself.

### 5.2 Allowed substitution

- stronger exact-environment execution may satisfy a weaker requirement if it proves the same property and remains diagnosable;
- deterministic shared-layer proof may replace redundant frontend copies when frontends add no unique semantics.

### 5.3 Forbidden substitution

- hosted/headless for native local-display/input evidence;
- Windows for Linux or vice versa when platform-sensitive;
- nearby Qt patch for exact QPA private ABI;
- third-party application for controlled compatibility cells;
- physical run for clean package/deployment proof;
- successful target launch for an explicitly claimed source→target upgrade transition;
- older candidate SHA for exact RC evidence;
- zero relevant execution for evidence.

### 5.4 Evidence invalidation

Typical invalidators include:

- protected implementation/behavior changes;
- public API/package/deployment metadata changes;
- Qt/toolchain/native-platform/graphics anchor changes;
- viewer/application fixture changes when named by the claim;
- performance workload/reference-host changes;
- product status/maintenance-policy changes;
- candidate/artifact change for exact Release Acceptance.

An invalidated record remains historical truth for what it measured but cannot satisfy a new claim without re-execution or an explicit authority-approved equivalence rule.

---

## 6. Suite architecture

Logical suite families:

| Family | Responsibility | Normal class |
| --- | --- | --- |
| `S-CORE` | platform/UI/transport-neutral invariants | V |
| `S-RUNTIME` | shared Qt-aware semantics | V |
| `S-ADAPTER` | Widgets/Quick capture/input target contracts | V |
| `S-FRONTEND` | unique C++/QML/Generic/QPA mapping | V/P |
| `S-TRANSPORT` | transport-neutral + RFB-specific behavior | V/P |
| `S-PACKAGE` | package/export/acquisition | V/P |
| `S-DEPLOY` | deployment closure/origin/relocation | P/Q |
| `S-PRODUCT-PATH` | vertical user journeys | P/Q |
| `S-OPERABILITY` | diagnostics/effective state/safe disable | V/P/Q |
| `S-UPGRADE` | authorized maintenance transitions | V/P/Q/R |
| `S-SECURITY` | policy + mechanism security | V/P/Q |
| `S-RELIABILITY` | stress/soak/fuzz/resource bounds | V/Q |
| `S-PERFORMANCE` | SLO/resource trend | V/Q/R |
| `S-COMPAT` | qualification-cell orchestration | Q/R |
| `S-REALWORLD` | pinned third-party pressure test | Q/R as required |

Repository/release governance remains outside product-suite counts.

### Shared versus frontend ownership

```text
Core invariant               -> S-CORE
Qt-aware common behavior     -> S-RUNTIME
surface/capture/input detail -> S-ADAPTER
entry syntax/activation      -> S-FRONTEND
wire/protocol                -> S-TRANSPORT
package/export               -> S-PACKAGE
artifact closure             -> S-DEPLOY
user journey                 -> S-PRODUCT-PATH
runtime support facts        -> S-OPERABILITY
version transition           -> S-UPGRADE
support-cell orchestration   -> S-COMPAT
```

### Identity split policy

Separate case identity only when materially required by:

- platform/capability registration;
- external resource/viewer/device;
- environment image/host;
- timeout/cost class;
- destructive/fuzz/process isolation;
- source/target maintenance setup;
- selector ownership;
- incompatible expected-result model;
- evidence-retention requirements.

Consolidation must preserve exact case name, ER/PAC, oracle/observation, environment/fixture identity and actionable failure output.

Do not trade test-count reduction for an opaque mega-test.

---

## 7. Metadata architecture

Later implementation should expose the minimum machine-readable metadata needed for selection/scheduling/traceability.

Conceptual suite metadata:

```yaml
suite_id: S-DEPLOY-LINUX-RUNTIME-CLOSURE
owner: deployment
er: [ER-DEPLOY-RUNTIME-ORIGIN, ER-DEPLOY-FOREIGN-QT-ISOLATION-LINUX]
pac: [PAC-5, PAC-9]
risk: [R-DEPLOY, R-COMPAT]
evidence_class: [P, Q, R]
cost_class: installed
platform_sensitivity: linux
environments: [E-DEPLOY-LINUX]
gates: [G2, G4, G6]
triggers: [deploy-helper, install-export, runtime-dependency-resolution]
isolation: clean-prefix
parallel_group: deployment-linux
expected_max_wall: 60s
```

Case-specific metadata contains only deltas from suite/ER metadata where possible.

Principles:

- suite-level metadata is inherited rather than copied per case;
- metadata describes product evidence meaning, not only mechanics;
- a logical suite may map to several current CTests during migration;
- product status/version policy is referenced, never duplicated;
- malformed/unknown metadata fails conservatively for product lanes;
- ordinary developers should not need release-governance knowledge for normal edits.

---

## 8. Change-impact and selector architecture

Selection is a graph problem:

```text
changed files/options/public contract
 -> ownership areas
 -> risk domains
 -> ERs
 -> suite/case candidates
 -> capability filter
 -> environment filter
 -> gate policy
 -> execution manifest
```

Initial ownership areas:

- core;
- runtime-common;
- widgets-adapter;
- quick-adapter;
- cpp/qml/generic/qpa frontend;
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

### Example propagation

Core lifecycle:

```text
core -> R-CORE/R-RELIABILITY
 -> Core V
 -> affected Runtime/transport seam V
 -> perf smoke only if frame-age/scheduling semantics changed
```

Deployment helper:

```text
deploy -> R-DEPLOY/R-PACKAGE/R-COMPAT/R-UPGRADE
 -> package/deploy contract suites
 -> Win/Linux clean deployment paths
 -> authorized maintenance edge only if transition semantics are affected
 -> no automatic full Core replay
```

QML frontend:

```text
qml -> R-FRONTEND/R-ADOPTION
 -> QML mapping V
 -> QML vertical P
 -> no automatic QPA matrix
```

QPA frontend:

```text
qpa -> R-FRONTEND/R-NATIVE/R-COMPAT/R-DEPLOY
 -> QPA seam V
 -> applicable native/deploy P/Q
 -> exact Qt private-ABI requalification when invalidated
```

Diagnostics:

```text
diagnostics -> R-OPERATE/R-SECURITY/R-ADOPTION
 -> effective-state/secret-safe V
 -> installed product-path P
 -> no automatic graphics qualification
```

### Selector invariants

- selected product risk executes at least one relevant suite;
- zero relevant execution is invalid evidence;
- unknown ownership broadens conservatively;
- selector failure never silently returns empty;
- path matching is only one input;
- public package/API/support-policy changes may propagate broadly;
- full-gate override exists for uncertainty/RC;
- selector logic is deterministic-testable;
- execution manifest explains why each expensive suite is present.

---

## 9. Gate architecture

### G0 Developer — <= 10 s normal target

Changed module + direct deterministic dependencies. No clean reinstall/real viewer/device by default.

### G1 PR Verification — <= 90 s target

Affected deterministic evidence, duplicating OS only when platform sensitivity justifies it. Independent suites run in parallel.

### G2 PR Product — <= 3–5 min target

Activated when risk crosses a user/artifact boundary, including public API/frontend, install/export/deploy, QPA/native plugin loading, transport/security, canonical adoption, diagnostics or authorized maintenance behavior.

### G3 Merge Sentinel — <= 60 s target

Answers only whether integrated mainline is obviously broken or selection produced a false-green zero-test state. It does not replay PR qualification.

### G4 Nightly Qualification-lite — <= 30 min target

Carries broad product paths, clean deploy, viewer/network subset, stress-lite, performance trend, diagnostics/maintenance subset and selected compatibility anchors.

Only executions whose ER/environment/oracle/identity satisfy Q semantics become Qualification records.

### G5 Weekly/Extended

Long repetition, fuzz, broader viewer/network/performance, soak, broader authorized maintenance edges and selected third-party applications.

### G6 Release Qualification

Exact frozen candidate against the declared release/support envelope, including required hosted, clean deployment, native/physical, maintained viewer, security, performance, reliability, real-world, operability and maintenance evidence.

G6 optimizes for evidence completeness, not developer feedback time.

---

## 10. Development-velocity architecture

### Budgets

| Metric | Policy |
| --- | --- |
| local focused feedback | <= 10 s normal target |
| PR verification | <= 90 s target |
| affected PR product path | <= 3–5 min target |
| merge sentinel | <= 60 s target |
| nightly qualification-lite | <= 30 min target |
| blocking flake | effectively zero; repeated instability is a defect |
| selected lane with zero relevant execution | invalid |
| unrelated expensive suite execution | selector/test-architecture defect |

### Measure the critical path

Track:

- per-suite P50/P95 runtime;
- gate wall time;
- critical-path suite/setup;
- queue/wait versus execution time;
- configure/build/install time;
- viewer/device/reference-host setup;
- maintenance source/target setup;
- retry/flake cost;
- expensive installed/E2E selection rate;
- unnecessary cross-OS duplication.

### Optimization order

1. remove unrelated selection;
2. parallelize independent work;
3. reuse safe artifacts;
4. share setup inside suites;
5. parameterize cases;
6. separate cheap proof from expensive product proof;
7. move breadth/repetition later;
8. optimize test implementation;
9. only then re-evaluate whether an ER is stronger than the product claim actually requires.

### Maintenance/cognitive cost

- do not create one Issue/CTest/job per ER/case;
- inherit suite/ER metadata;
- automated runs capture their own evidence metadata;
- ordinary PR authors do not maintain qualification ledgers;
- manual records exist only for genuinely manual evidence;
- adding a platform/Qt/viewer generally adds cells/fixtures, not ERs.

---

## 11. Product-path architecture

Canonical journey:

```text
acquire
 -> evaluate compatibility
 -> choose/integrate
 -> build/install
 -> deploy
 -> isolate/relocate
 -> launch
 -> connect
 -> view / optional control
 -> disconnect/reconnect
 -> stop/disable
 -> diagnose
 -> update/rollback only where explicitly supported
 -> maintain
```

Controlled anchor paths include applicable combinations of:

- C++ + Widgets/Quick;
- QML + Quick;
- Generic + Widgets/Quick;
- QPA + Widgets/Quick;
- deliberate supported combined payload forms.

Each product-path record captures the material acquisition, source/link contract, Qt/HyRemote identity, deployment form, runtime origins, native platform, policy, viewer, launch/view/control/reconnect/stop/diagnostic result and oracle.

The existence of an anchor path does not imply every PR executes it.

---

## 12. Deployment and maintenance architecture

Deployment proof is layered:

1. package/export contract;
2. deployment planning/validation;
3. artifact closure;
4. isolation/relocation;
5. runtime dependency origin;
6. actual product fit with launch/viewer use.

Negative payload/metadata/QPA mismatch cases normally share table-driven suites unless they require different environments.

### Linux Qt lineage

For Qt libraries, the selected qualified Qt SDK/deployed copy is the positive trust source. Distribution Qt is unrelated runtime for system applications, not an alternate HyRemote SDK.

Prove origin positively instead of maintaining global `/lib`/`/usr/lib` blacklists.

### Maintenance edges

Only product-authorized transitions are tested:

```text
known source artifact
 -> documented update/install/deploy mechanism
 -> target candidate
 -> origin/artifact audit
 -> target product path
 -> optional documented rollback
```

Check for stale/mixed libraries, plugins, QML modules, package metadata and incompatible QPA payloads. Do not test every historical release pair.

---

## 13. Native, compatibility, viewer and network qualification

### Native coexistence

For claimed native cells:

```text
native baseline
 -> HyRemote view-only
 -> control-enabled where supported
```

Verify local display/input/focus/resize/supported graphics, remote behavior, reconnect, input cleanup, Runtime stop and slow-viewer/native responsiveness.

### Qualification-cell classes

- anchor;
- interaction-risk;
- representative/pairwise.

### Compatibility status handling

The cell references whatever status `docs/compatibility.md` currently defines. Test tooling stores/renders it as upstream product data and does not assume its own enum.

### Public Qt versus QPA

- public-Qt C++/QML/Generic claims may use product-authorized family/range anchors;
- QPA requires exact Qt private-ABI/platform evidence for every claimed row.

### Viewer interoperability

Explicit maintained viewer/version evidence may include handshake, updates, framebuffer, resize, input, reconnect, cleanup, security/encoding/extensions and performance relevance.

Passing one viewer does not imply all RFB clients.

### Network impairment

Representative profiles include LAN, added RTT, bandwidth cap, bounded loss, slow reader, stalled handshake, abrupt disconnect, reconnect storm and mixed-latency multi-viewer.

Oracles concern correctness, boundedness, freshness, recovery, native health and interaction latency.

---

## 14. Reliability, security, performance and operability

### Reliability

Deterministic first: queue bounds, teardown ownership, cancellation, held-input cleanup, callback suppression, parser bounds, per-viewer pending-work bounds.

Stress/soak/fuzz later: repetition/churn/input flood/stalled peers/multi-viewer/resource trends.

### Security

Top-level object is requested policy, not one crypto implementation. Verify capability/configuration/protection establishment before listener/session availability; fail closed otherwise.

Mechanism-specific authentication/encryption/certificate work plugs underneath PAC-7.

### Performance

Primary product metric is Remote Interaction Latency; internal timing/resource metrics explain it.

Reuse workload/SLO authority from `docs/performance-optimization.md` rather than duplicating thresholds here.

### Operability

Prove a normal installed-product user can understand effective version/environment/integration/Runtime/listener/security/input/client/error/deployed-artifact facts where the public diagnostic product surface provides them, without exposing secrets.

Safe stop/disable is a product path, not logging behavior.

---

## 15. Real-world application architecture

Controlled fixtures provide cheap diagnosis. Pinned third-party applications pressure-test low intrusion and realistic integration complexity.

Rules:

- exact upstream revision/environment;
- no HyRemote patch merely to make a pristine-app route pass;
- exact applicable frontend recorded;
- native behavior preserved;
- unsupported custom/media/native surfaces classified;
- no hidden SDK/build-tree repair;
- result remains third-party verification, not an accidental named-app support promise.

Existing #134/#242 own the programme; this architecture supplies common evidence structure only.

---

## 16. Evidence Record architecture

Conceptual retained Q/R record:

```yaml
record_id: immutable-id
candidate_sha: exact-source-sha
artifact_identity: exact-built-or-staged-artifact
source_artifact_identity: ...   # maintenance edge if applicable
target_artifact_identity: ...   # maintenance edge if applicable
er: [ER-...]
suite_id: ...
case_ids: [...]
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
product_claim_refs: [...]
result: PASS|FAIL|BLOCKED
commands: ...
logs_artifacts: ...
started_at: ...
completed_at: ...
```

Only material fields are required for a given ER.

### 16.1 Automated-run rule

**Any automated suite/orchestrator that claims retained Q/R evidence must emit its own evidence record or equivalent machine-consumable record automatically.**

The runner captures, from execution context rather than human transcription:

- candidate/artifact identity;
- suite/case/ER identity;
- material environment and external fixture identity;
- oracle/pass condition identifier;
- actual result/measurement;
- timestamps and log/artifact references.

If the automation cannot establish a required identity, it emits `BLOCKED`/invalid evidence rather than asking an ordinary PR author to guess or manually fill the field.

Hand-authored records are allowed only for genuinely manual/physical/human-observation evidence. Those records still use predefined ER, environment facts, checklist/oracle and candidate/artifact identity.

This rule prevents TS-6 from turning qualification into a manual bookkeeping system.

### 16.2 Exact-candidate rule

For Release Acceptance, evidence required by release procedure to be exact-candidate-bound uses the same RC-FROZEN candidate/artifact identity.

If a post-freeze change changes candidate SHA/artifact:

- prior required R/physical evidence becomes historical/preflight for the new candidate unless canonical release authority explicitly defines an equivalent artifact identity rule;
- different candidate SHAs are not combined into one PASS because an edit is labelled evidence-neutral;
- release change-control decides re-cut/rerun scope but cannot bypass identity semantics.

### 16.3 Record validity

Retained evidence is valid only for the artifact/environment/fixture facts declared by its ER and record. Invalidated records remain historical evidence but cannot satisfy a new positive claim without re-execution or explicit authority-approved equivalence.

---

## 17. Flake, infrastructure and quarantine

Failure classification:

- product defect;
- deterministic test defect;
- runner/environment infrastructure failure;
- external viewer/device/dependency failure;
- unresolved/intermittent.

Automatic retry is diagnostic only for known infrastructure instability. It may not silently convert an unexplained failure into PASS evidence.

Quarantine requires owner, reason, affected PAC/ER gap, compensating evidence where necessary, expiry/review condition and visible non-blocking status.

Quarantine cannot silently preserve a positive support claim if required evidence is missing.

`BLOCKED` is not PASS.

---

## 18. Test lifecycle governance

Every permanent addition answers:

1. Which PAC/ER/claim?
2. What failure mode?
3. Why is existing evidence insufficient?
4. Cheapest honest seam?
5. Explicit oracle?
6. Invalidation?
7. Case or new suite/identity?
8. Gates/environments?
9. Trigger/risk propagation?
10. Wall/setup/maintenance cost?
11. Failure meaning?

Consolidate cases sharing setup/owner/scheduling/environment/failure category while preserving case diagnostics/oracle output.

Move evidence when frequency/cost is disproportionate to normal risk, e.g. long soak G1→G5 while cheap deterministic resource-bound regression remains G1.

Remove only when risk/claim is removed or stronger/cheaper evidence fully supersedes it. Record replacement/reason.

---

## 19. Current-inventory migration

Migration is staged and reversible.

### M0 / TS-0 — Architecture freeze

No executable test changes.

Deliverables:

- strategy;
- detailed architecture;
- ER catalog;
- PAC/risk/evidence/gate vocabulary;
- oracle/invalidation model;
- velocity/cognitive-cost budgets;
- evidence-binding rules.

### M1 / TS-1 — Read-only inventory mapping

For every current test identity record:

- ERs actually/partially/intended to be proved;
- PAC/risk;
- actual oracle quality;
- evidence class/environment strength;
- suite-family candidate;
- platform/capability sensitivity;
- current gates/triggers;
- runtime/setup cost;
- evidence-retention/invalidation relevance;
- duplicate/unique evidence value;
- candidate disposition: retain identity / parameterize / move gate / governance / investigate.

Also report **unmapped ERs** where required product evidence has no current executable proof.

TS-1 edits no tests or scheduling.

### M2 / TS-2 — Logical metadata/registry

Introduce minimal machine-readable mapping without initially changing behavior. Avoid a second authority and prefer inherited suite/ER metadata.

### M3 / TS-3 — Evidence-driven consolidation

Only after M1/M2 facts. Priorities may include fragmented policy/profile cases, deploy positive/negative variants, governance separation and duplicated frontend proof.

Do not consolidate Core/Runtime correctness merely to reduce count.

### M4 / TS-4 — Risk-based G0–G3 selection

Implement selectors with self-tests, visible manifests, nonzero/fail-closed behavior, conservative fallback and measured critical-path improvement.

### M5 / TS-5 — Reusable product-path orchestration

Clean acquisition/install/deploy/isolation/launch/viewer/diagnostics and declared maintenance paths.

### M6 / TS-6 — Qualification/evidence framework

Before V0.4 entry:

- qualification cells;
- maintenance-edge references;
- environment/fixture identity;
- ER/oracle/invalidation integration;
- automatic evidence record generation for automated runs;
- physical/manual record schema;
- exact candidate/artifact binding;
- compatibility/maintenance claim traceability.

### M7 / TS-7/TS-8 — Domain integration and continuous health

Integrate existing compatibility/physical/real-world/performance/security/reliability programmes and continuously monitor gate SLO, flake, duplicate proof, invalidated evidence and obsolete tests.

---

## 20. Roadmap placement

This is cross-cutting engineering infrastructure, not a new product version line.

### V0.2

First User Trial remains priority. Allowed: TS-0 design, later read-only TS-1 audit, and concrete V0.2 regression evidence. No broad CTest/CI migration may delay the trial.

### V0.3 family

M2–M6 may land incrementally as self-service/package/platform breadth becomes real. Test infrastructure proves product work; it does not replace it.

### Before V0.4 entry

The system must already represent the declared GA matrix without Cartesian CI explosion and must already support evidence records/qualification cells/product paths.

### V0.4

Qualification only. Execute evidence using existing domain authorities rather than build a hidden test framework.

### V1 GA

Consume qualified evidence and freeze the long-lived support contract. Final GA is not an architecture redesign phase.

---

## 21. Work-package plan

| WP | Work | Depends on | Output | Placement |
| --- | --- | --- | --- | --- |
| TS-0 | strategy + architecture + ER catalog freeze | #393 | accepted design; no test changes | now, non-blocking V0.2 |
| TS-1 | current inventory semantic/cost audit | TS-0 | read-only mapping + gaps | after accepted design |
| TS-2 | logical metadata/registry | TS-1 | ER/risk/suite/gate map | V0.3 incremental |
| TS-3 | suite/identity consolidation | TS-1/2 | parameterized suites + governance separation | V0.3 bounded PRs |
| TS-4 | risk selector + G0–G3 | TS-2 | execution manifest + budgets | V0.3 before matrix growth |
| TS-5 | product-path harness | TS-2 + productized paths | clean reusable journeys | V0.3 self-service |
| TS-6 | qualification/evidence records | TS-2/5 + product matrix | cells, automatic records, support traceability | before V0.4 |
| TS-7 | domain-programme integration | TS-6 + existing authorities | reliability/security/perf/real-world evidence plumbing | V0.3→V0.4 |
| TS-8 | continuous test-system health | TS-4 onward | runtime/flake/duplicate/evidence governance | ongoing |

Do not create one Issue per test, ER or case. TS-1 facts determine the smallest independently mergeable implementation packages.

No TS work package enters `release-trains.json` merely because it exists; normal product/release authority decides release gating.

---

## 22. Existing authority ownership

| Authority | Remains owner of |
| --- | --- |
| #1 | product roadmap / sequencing |
| #24 | version semantics / user-visible maintenance boundary |
| #343 | GA compatibility/product baseline |
| #57 | Qt/support qualification content |
| #109 | physical/native evidence |
| #134 / #242 | real-world representative programme |
| #370 / #9 | performance SLO / qualification |
| #143 and security authorities | security capability/truth |
| #335 | self-service diagnostics product capability |
| #165 | candidate freeze/change control |
| #95 / release authority | release/version mechanics |
| `docs/compatibility.md` authority | compatibility status vocabulary and public claims |

The test architecture supplies common evidence plumbing and execution economics. It does not take scope ownership away from these authorities.

---

## 23. TS-0 acceptance criteria

Before TS-1 begins, reviewers must be able to answer:

1. Which product promises/PACs are protected?
2. Which risk domains can violate them?
3. Which stable ERs exist and when do they activate?
4. What oracle decides each retained type of evidence?
5. What invalidates it?
6. What is the cheapest honest proof seam?
7. When is a variant a case versus separate identity?
8. How does a code/product change select ERs/suites?
9. How are Windows/Linux/Qt/QPA/native differences represented?
10. How are clean deployment/runtime origins proved?
11. How are physical/native claims separated from hosted evidence?
12. How are viewer/network/security/performance/reliability represented without Cartesian explosion?
13. How are diagnostics and safe disable product-tested?
14. How are maintenance transitions sparse and authority-driven?
15. How are Q/R records bound to exact artifact/environment/fixture identity?
16. How do automated runs generate records without manual transcription?
17. How is false-green zero execution prevented?
18. How are flaky/infrastructure failures classified?
19. How does migration avoid a big-bang rewrite?
20. How are runtime and cognitive/maintenance cost controlled?
21. What must be ready before V0.4 entry?
22. Which upstream product authorities retain ownership?

Until these are accepted, no broad test migration begins.

---

## 24. Bounded implementation choices for later closure

The architecture deliberately does not pre-build a framework. TS-1 may inform these choices before the relevant TS-2+ implementation package begins:

- metadata storage: CMake properties vs declarative manifest vs hybrid;
- suite runner boundary: C++/QtTest vs CMake vs Python/orchestrator;
- evidence-record serialization and artifact retention;
- environment registry representation;
- exact selector fallback implementation;
- cost telemetry storage;
- quarantine representation;
- compatibility traceability rendering;
- maintenance-edge registry rendering.

Choose the smallest solution that meets the architecture without creating duplicate authorities or routine manual work.

---

## 25. Success criteria

The future implementation succeeds only when:

- product confidence is at least as strong as today;
- important claims trace to ERs and executed evidence;
- every retained ER has an oracle and invalidation rule;
- ordinary PRs run materially less unrelated work;
- deployment/native/platform risks retain their required strong evidence;
- shared behavior is not redundantly copied across frontends;
- case growth does not automatically grow CTest/job/Issue count;
- failures are more diagnosable;
- automated qualification does not create manual ledgers;
- compatibility status remains product-authority owned;
- diagnostics/maintenance claims are verified where declared;
- exact release acceptance never mixes candidate identities;
- V0.2 is not delayed by a broad test rewrite;
- V0.4 starts with qualification machinery ready;
- runtime, flake, duplicate proof and cognitive cost are measured/controlled;
- the architecture remains valid for more Qt versions, ARM64, Wayland/EGLFS, other transports and future acceleration.

The target is not fewer tests. The target is **stronger product evidence per unit of development time and maintenance effort**.