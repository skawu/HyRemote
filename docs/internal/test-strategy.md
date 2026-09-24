# HyRemote Product Test Strategy

Status: **canonical product-test strategy**

Authority: #393

Detailed system design: [`test-system-architecture.md`](test-system-architecture.md)

Product evidence requirements: [`test-evidence-requirements.md`](test-evidence-requirements.md)

This document defines **why and at what strength HyRemote tests the product**. It is intentionally organized around product promises, product risks, evidence and development economics — not around the current CTest inventory.

`tests/TEST_CATALOG.md`, `tests/TEST_MATRIX.md` and `tests/EXECUTION_BASELINE.md` remain the authority for the current executable inventory until later migration work is explicitly accepted. Their current shape is not a permanent architecture constraint.

A design document, workflow, checklist, registered test or green CI status is not evidence by itself. Product evidence exists only when the applicable action executes against an identified product/artifact/environment and its declared oracle produces a result.

---

## 1. Product vision under test

HyRemote is a low-intrusion Remote Access Runtime / SDK for existing Qt applications.

The product goal is:

> Add reliable, secure, responsive, diagnosable and maintainable remote viewing and optional remote control to an existing Qt application with minimal integration burden, while preserving the application's native local display, local input, lifecycle and platform behavior, and without exposing remote-desktop implementation mechanics as normal application configuration.

The stable application model remains:

```text
C++ API -----\
QML API ------\
Generic Plugin ---> one Shared Runtime ---> one Core
QPA ----------/
```

Widgets and Qt Quick are target/surface families, not separate products. RFB is the current transport implementation, not the long-term product identity. Qt versions, qwindows/qxcb/Wayland/EGLFS, graphics paths and future acceleration/transport choices are implementation and qualification dimensions beneath stable product promises.

A future transport, capture backend, embedded platform or hardware path must preserve the same product contracts unless product/architecture authority deliberately changes those contracts.

---

## 2. Mandatory principles

### 2.1 Product-contract driven

Every permanent product test protects a named Product Acceptance Contract (PAC), an Evidence Requirement (ER), or an explicit upstream product/support claim.

If a test cannot explain the product risk it protects, it does not automatically deserve permanent blocking status.

### 2.2 Risk proportionality

Test scope and cost follow change risk, not test count or blanket habit.

Examples:

- a docs-only change does not run graphics qualification;
- a QML mapping change does not automatically run the QPA deploy matrix;
- a deployment/runtime-resolution change does run real Windows/Linux deployment evidence even if Core code is untouched;
- a QPA private-ABI change may invalidate exact Qt/platform qualification even if public-Qt frontends are unaffected.

### 2.3 Cheapest sufficient evidence

Use the lowest-cost seam that can **honestly** prove the property:

1. deterministic model/contract check;
2. unit/component check;
3. synthetic application/process integration;
4. clean product/deployment journey;
5. maintained viewer/network/reference environment;
6. native/physical qualification where the claim includes native behavior.

Cheap evidence must never substitute for stronger evidence when the product claim requires the stronger environment.

### 2.4 Development velocity is a first-class constraint

The test system must not become a project bottleneck.

Velocity includes both:

- **execution cost** — wall time, setup/build/install time, queue time, retries;
- **maintenance/cognitive cost** — metadata duplication, manual ledgers, per-case CI jobs, issue/test identity sprawl and unclear failure ownership.

Quality is protected by staged evidence, risk selection, parallelism and explicit qualification — not by running the entire universe on every commit.

### 2.5 No duplicate proof across peer frontends

Shared Runtime/Core behavior is proved once at the shared layer. C++/QML/Generic/QPA tests prove their unique activation/mapping/package/native-platform boundary plus a bounded number of vertical product paths.

The four frontends do not each receive a copy of the full Runtime/transport/security suite.

### 2.6 A case is not automatically a test identity

A new input, negative variant, Qt row or release profile is normally a **case** inside a suite.

A separate executable/CTest identity is justified only when scheduling, platform/capability registration, isolation, timeout/cost class, external resource, selector ownership or evidence-retention semantics materially differ.

### 2.7 Evidence has an oracle and invalidation rule

Every retained ER states:

- what observation decides PASS/FAIL;
- which product/artifact/environment/fixture facts make that evidence applicable;
- what changes invalidate reuse for a later claim.

A screenshot, log, benchmark value or manual observation is not self-interpreting evidence.

### 2.8 Product authorities own product status vocabulary

The test system does **not** define a second compatibility-status enum.

`docs/compatibility.md` and its owning product authorities define the current status vocabulary and semantics. Test/qualification tooling treats the status value as upstream product data and only proves whether the evidence required for that status is present.

This keeps today's `Supported / Limited / TODO / Unsupported` truth intact while allowing product authority to evolve it deliberately later.

### 2.9 Local/native behavior is part of the product

HyRemote augments a native Qt application. A test system that proves remote pixels while ignoring local display/input/platform behavior is incomplete for claims that promise local+remote coexistence.

### 2.10 Test architecture itself is maintained

Runtime, critical path, flake, duplicate proof, obsolete cases and evidence value are engineering metrics. Tests may be consolidated, moved to a later gate or removed when stronger/cheaper evidence supersedes them.

Historical existence is not a permanent KEEP reason.

---

## 3. Product Acceptance Contracts

The highest-level taxonomy is ten PACs.

| PAC | Product promise | Typical failure meaning |
| --- | --- | --- |
| **PAC-1 Low Intrusion** | Existing Qt applications gain remote access without being rebuilt around HyRemote internals. | Integration burden or backend details leak into the application. |
| **PAC-2 Native Non-interference** | Local display/input/native platform/lifecycle remain healthy while remote access is active. | HyRemote damages the host application it is meant to augment. |
| **PAC-3 Remote Experience** | Remote view/control reflects current application state correctly and safely. | The remote product is functionally wrong or unsafe to operate. |
| **PAC-4 Frontend Equivalence** | C++/QML/Generic/QPA are peer entries into one Shared Runtime. | A frontend becomes a second product/runtime personality. |
| **PAC-5 Deployability** | Installed/deployed applications run from their own tree without SDK/build-tree crutches. | Build success does not produce a usable delivered product. |
| **PAC-6 Reliability & Boundedness** | Remote activity cannot create unsafe teardown or unbounded resource growth. | A peer can destabilize the host process. |
| **PAC-7 Security Truth** | Requested security is actually established or fails closed, with no secret leak/downgrade. | Security behavior is weaker than the product claim. |
| **PAC-8 Responsiveness & Efficiency** | Remote interaction is fresh/responsive while native Qt/resource health remains acceptable. | The product technically works but is operationally unusable. |
| **PAC-9 Compatibility Truth** | Every positive support statement is backed by environment-specific evidence and no inference. | Documentation/support promise exceeds verified reality. |
| **PAC-10 Operability & Maintainability** | Users can diagnose, safely disable/recover, and perform explicitly supported maintenance transitions coherently. | The product only works with project-team knowledge or becomes unserviceable across maintenance. |

### 3.1 PAC-1 — Low Intrusion

Required properties include:

- C++ consumers use the public `HyRemote::RemoteAccess` surface;
- QML consumers use the public `HyRemote` module;
- Generic/QPA zero-code applications remain Qt-only at application source/link level;
- normal deployment uses the public `hyremote_deploy()` entry;
- normal users do not configure internal capture/encoding/queue/GPU implementation choices;
- public headers/packages do not leak internal Core/transport/private-platform types.

Synthetic clean consumers prove the package/API boundary. Representative real applications pressure-test whether the low-intrusion promise survives realistic complexity.

### 3.2 PAC-2 — Native Non-interference

For claimed native cells, verify applicable behavior before/during/after remote access:

- local visible rendering;
- local pointer/keyboard/text/focus;
- resize, dialogs, menus, popup and supported multi-window behavior;
- supported graphics/GPU path;
- application shutdown;
- HyRemote stop while the application continues;
- slow/stalled viewer without indefinite local UI/render blockage.

Headless/Xvfb evidence remains useful regression evidence but cannot independently prove a native coexistence claim.

### 3.3 PAC-3 — Remote Experience

Remote view covers:

- content, geometry and DPR/scaling;
- supported resize/surface lifecycle/composition;
- newest useful state rather than stale backlog;
- reconnect.

Remote control covers, where enabled:

- pointer/button/wheel;
- key/modifier;
- committed text when transport semantics support it;
- focus/routing/drag/held state.

Disconnect, stop, target destruction, policy disable and teardown must return synthetic input state to neutral and forbid late terminal delivery.

### 3.4 PAC-4 — Frontend Equivalence

Unique frontend responsibilities are bounded:

| Frontend | Unique evidence |
| --- | --- |
| C++ | facade configuration/start/stop/state/error mapping |
| QML | import/type/property/binding/component lifecycle/notification mapping |
| Generic | Qt-only linkage, generic-plugin activation, native platform preservation |
| QPA | platform factory/trampoline, exact private ABI, native delegate preservation/interception |

Common frame lifetime, queue bounds, listener policy, security policy, generic input neutrality and scheduling belong to shared layers rather than four frontend copies.

### 3.5 PAC-5 — Deployability

The canonical product proof is:

```text
qualified Qt SDK
  -> HyRemote configure/build/install
  -> clean external consumer
  -> consumer build/install
  -> hyremote_deploy()
  -> isolate/relocate delivered tree
  -> minimal runtime environment
  -> launch
  -> viewer connect/use
```

A deployment may not accidentally depend on source/build trees, the original HyRemote SDK path, repair environment variables or manually copied internals.

Windows DLL/plugin resolution and Linux ELF/RUNPATH/loader behavior are independent product risks.

On Linux, unrelated distribution Qt runtimes are environmental noise, not alternate development SDK candidates. The selected qualified Qt SDK/deployed lineage is the positive trust source.

### 3.6 PAC-6 — Reliability & Boundedness

Deterministic invariants include:

- bounded frame/input/per-viewer work;
- freshness-first overload behavior;
- deterministic teardown ownership;
- cancellable startup;
- callback lifetime/exception containment;
- target destruction safety;
- slow-client isolation.

Longer evidence adds repeated start/stop, connect/disconnect, surface churn, input flood, multi-viewer, idle/active soak and fuzz where appropriate.

Long stress/soak/fuzz belongs to later gates unless a regression can be reduced to a cheap deterministic PR test.

### 3.7 PAC-7 — Security Truth

The stable security rule is:

```text
requested policy
 -> capability available?
 -> configuration valid?
 -> required protection established?
 -> only then listener/session may become available
```

Failure is fail-closed: no silent downgrade, partial listener, weaker fallback or secret exposure.

Mechanism-specific authentication/encryption/certificate tests sit below this product policy rather than defining it.

### 3.8 PAC-8 — Responsiveness & Efficiency

The primary user metric is **Remote Interaction Latency**:

```text
viewer action
 -> network/transport input
 -> Runtime/Core/Qt input
 -> application render/update
 -> capture
 -> schedule/encode/deliver
 -> viewer observes result
```

Capture FPS, encode duration, frame age, bytes/update, CPU/GPU/memory and local UI impact are diagnostic metrics explaining the end-to-end result.

The product also protects freshness, idle/static efficiency, bounded resources and slow/multi-viewer isolation.

### 3.9 PAC-9 — Compatibility Truth

Every positive compatibility claim is traceable to explicit qualification boundaries such as:

- product/candidate identity;
- OS/CPU;
- Qt version;
- frontend;
- UI/surface family;
- native platform/delegate;
- graphics path/DPR where material;
- deployment form;
- viewer/security/network/performance profile where material.

No Windows result substitutes for Linux, no desktop result substitutes for embedded, no nearby Qt patch substitutes for exact QPA private-ABI evidence, and basic Widgets/Quick does not qualify unrelated graphics paths.

The status attached to a claim remains owned by `docs/compatibility.md`; test infrastructure does not reinterpret or rename it.

### 3.10 PAC-10 — Operability & Maintainability

HyRemote remains supportable after first launch.

As applicable to the product line:

- one documented bounded secret-safe diagnostic path reports effective facts;
- product/build identity, Qt/OS/arch, active integration route, Runtime state, listener/security/input policy, client count, last error and deployed artifact identity can be distinguished where available;
- configuration/runtime/deployment failures are differentiated where technically observable;
- stopping/disabling HyRemote leaves the host application usable;
- an explicitly supported update does not leave stale/mixed libraries, plugins, QML modules, package metadata or incompatible QPA payloads;
- rollback is tested only where product/version authority explicitly promises it.

The test system never creates upgrade/rollback promises by itself.

### 3.11 Self-service is a journey, not PAC-11

V0.3 self-service adoption deliberately spans the owning PACs:

```text
evaluate compatibility -> PAC-9
choose/integrate       -> PAC-1 / PAC-4
build/install/deploy   -> PAC-5
connect/view/control   -> PAC-3
native coexistence     -> PAC-2
troubleshoot/disable   -> PAC-10
```

This avoids a catch-all contract that would duplicate the actual product semantics.

---

## 4. Evidence classes

HyRemote uses four evidence classes.

| Class | Question | Typical use |
| --- | --- | --- |
| **V — Verification** | Does implementation satisfy deterministic design contracts? | developer / PR |
| **P — Product Validation** | Can a user complete the affected real product path? | affected PR / nightly |
| **Q — Qualification** | Does an exact environment/product cell have the evidence required for its intended product claim? | reference/native qualification |
| **R — Release Acceptance** | May this exact frozen candidate ship with the stated claims? | RC only |

These are not a simple strength ladder.

- a physical run cannot replace a deterministic state-machine oracle;
- a unit test cannot replace native display/loader qualification;
- a G4/nightly run is not automatically Q evidence merely because the gate name contains “qualification”.

Evidence class depends on the ER, environment, oracle, fixture identity and artifact binding.

---

## 5. Technical evidence ownership

The default ownership rule is:

```text
Core invariant               -> Core verification
Qt-aware common semantics    -> Shared Runtime verification
surface/capture/input detail -> Widgets/Quick adapters
entry syntax/activation      -> frontend boundary
wire/protocol behavior       -> transport
package/export contract      -> package
runtime artifact closure     -> deployment
user journey                 -> product-path validation
environment support claim    -> qualification
repository/release mechanics -> governance, outside product-quality counts
```

### Core

Core proves platform/UI/transport-neutral lifetime, timing, damage, input abstraction, boundedness, scheduling and teardown invariants.

### Shared Runtime

Runtime owns common Qt-aware lifecycle, target/surface model, listener/security policy, client state, input routing, capture demand, diagnostics and common performance behavior.

### Widgets / Quick adapters

Adapters prove capture, DPR/resize, target loss, truthful damage capability and Qt target/input details.

### Frontends

Frontends prove only their unique public/Qt activation boundary plus minimal vertical wiring to Shared Runtime.

### Transport

Transport evidence is split into:

- transport-neutral lifecycle/frame/input/bounded-flow contract;
- RFB-specific handshake/update/encoding/framing/auth/interoperability behavior.

### Packaging / deployment

Packaging/deployment owns clean consumer acquisition, exported metadata, payload selection, relocation, runtime origin and delivered-tree independence.

### Repository / release governance

Branch naming, release-profile syntax, CI classifier self-tests, docs-path and repository-layout checks may remain mandatory engineering controls, but they are not counted as remote-access product coverage.

---

## 6. Execution gates and velocity budgets

| Gate | Purpose | Target wall time | Normal evidence |
| --- | --- | ---: | --- |
| **G0 Developer** | immediate focused feedback | **<= 10 s** | changed-module V |
| **G1 PR Verification** | affected deterministic regression protection | **<= 90 s target** | selected V |
| **G2 PR Product** | affected vertical product/deployment path | **<= 3–5 min target** | selected P |
| **G3 Merge Sentinel** | detect obvious integrated break / false zero-test green | **<= 60 s target** | minimal nonzero health |
| **G4 Nightly Qualification-lite** | broader product/reference evidence | **<= 30 min target** | broad P + selected Q where environment is sufficient |
| **G5 Weekly/Extended** | expensive repetition/fuzz/perf/compatibility breadth | not a PR budget | extended V/P/Q |
| **G6 Release Qualification** | exact frozen candidate acceptance | evidence completeness | required Q/R |

Budgets are architectural targets, never permission to weaken real product evidence.

When a gate exceeds budget, optimize in this order:

1. remove unrelated selection;
2. parallelize independent suites;
3. reuse safe build artifacts/fixtures;
4. share expensive setup inside a suite;
5. parameterize cases;
6. split cheap deterministic proof from expensive product proof;
7. move breadth/repetition to a later gate;
8. optimize the test implementation;
9. only then revisit whether the product evidence requirement itself is excessive.

Do not start by deleting evidence required by a real risk.

---

## 7. Risk-based execution

Selection follows:

```text
changed code/config/public contract
 -> ownership area
 -> risk domain
 -> Evidence Requirement
 -> suite/case candidates
 -> capability/environment filters
 -> gate policy
 -> execution manifest
```

Selector invariants:

- selected product risk must yield nonzero relevant execution;
- unknown ownership fails conservatively rather than silently empty;
- full-gate override exists for uncertainty/RC;
- public API/package/support changes can propagate beyond filename locality;
- selector rules are themselves deterministic tests;
- logs explain why expensive suites were selected.

Examples:

| Change | Expected evidence | Not implied by default |
| --- | --- | --- |
| Core lifecycle/backpressure | Core + affected Runtime/transport V | QML deployment matrix |
| QML frontend | QML mapping + QML vertical P | QPA private-ABI qualification |
| QPA/native delegate | QPA V + relevant native/deploy cells | QML property tests |
| deploy helper/runtime origin | package/deploy V + clean Win/Linux P | full Core replay |
| security | security/listener/transport evidence | unrelated graphics matrix |
| diagnostics | effective-state V + representative installed P | broad graphics qualification |
| docs only | docs/product-contract consistency | runtime GUI/deploy tests |

---

## 8. Qualification architecture

### 8.1 Hosted versus qualification environments

Hosted CI is optimized for repeatable regression detection. Native/reference environments prove claims that hosted/headless environments cannot.

| Environment | Primary role | Native support proof by itself? |
| --- | --- | --- |
| hosted Linux headless/Xvfb | V/P regression | No |
| hosted Windows runner | V/P regression | Not for all native/physical claims |
| qualified Linux native desktop/device | Q/R | Yes for declared cell |
| qualified Windows desktop | Q/R | Yes for declared cell |
| physical HiDPI/GPU/multi-display/device | targeted Q/R | Yes for declared property |

### 8.2 Avoid Cartesian explosion

Use three cell classes:

- **anchor cells** — complete reference cells;
- **interaction-risk cells** — e.g. QPA × exact Qt × OS, deploy × loader, graphics × OS/GPU;
- **representative/pairwise cells** — meaningful coverage without full combinatorics.

Maintenance transitions use a separate sparse graph of explicitly authorized source→target edges, not an all-history version matrix.

### 8.3 Compatibility status is opaque to test infrastructure

Qualification records the evidence and points to the product claim. It does not invent or hard-code the claim's status vocabulary.

Current status definitions come from `docs/compatibility.md`; if product authority changes that vocabulary later, test architecture need not be redesigned.

---

## 9. Major product evidence programmes

Detailed ERs, oracles and invalidators live in `test-evidence-requirements.md`. The strategy groups them into these programmes.

### 9.1 Deployment

Prove package contract → deployment planning → artifact closure → isolation/relocation → runtime origin → actual launch/viewer use.

Negative deployment inputs should usually be table-driven cases, not permanent one-case identities.

### 9.2 Native non-interference

Observe the same supported application as:

```text
native baseline
 -> HyRemote view-only
 -> remote-control-enabled where supported
```

Confirm local behavior, remote behavior, reconnect, input cleanup, Runtime stop and slow-viewer isolation.

### 9.3 Viewer interoperability

Viewer claims are explicit and versioned. Relevant evidence covers handshake, framebuffer/update behavior, resize, input, reconnect/disconnect, auth/encoding/extensions where claimed.

One working viewer does not imply every RFB client.

### 9.4 Network impairment

Representative profiles include direct LAN, added RTT, bandwidth cap, bounded loss, slow reader, stalled handshake, abrupt disconnect and reconnect storm.

Results are judged by correctness, boundedness, freshness, recovery, native UI health and interaction latency — not merely TCP survival.

### 9.5 Reliability / stress / soak / fuzz

Deterministic resource/lifecycle properties stay in cheap verification. Repetition and long-running evidence moves to G4/G5/G6.

Soak records RSS, threads, FD/handles, sockets, queues/backlog, CPU/GPU, crash/hang and latency drift where applicable.

### 9.6 Security

Security tests request a policy and verify exact establishment or fail-closed behavior. Authentication, future encryption/certificates and protocol-specific mechanisms are subordinate suites.

### 9.7 Performance

`docs/performance-optimization.md` remains the detailed SLO/workload authority.

- G1: deterministic/perf smoke when affected;
- G4: standard trend/reference subset;
- G5: broader viewer/network/high-motion/multi-viewer profiles;
- G6: exact qualified release profile.

Thresholds are not widened merely to keep builds green.

### 9.8 Operability / diagnostics / maintenance

Diagnostics verify effective product facts and secret-safe failure classification from installed artifacts. Supported update/rollback edges are tested only when declared by version/product authority.

### 9.9 Real-world applications

Pinned third-party applications pressure-test integration assumptions but do not define new product requirements or named-app support by accident.

Upstream worktrees remain pristine; exact revision/Qt/OS/route/artifact identity is recorded; limitations are classified rather than hidden.

---

## 10. Evidence records and exact-candidate binding

Retained Q/R evidence identifies enough of the following to decide applicability:

- candidate/source identity;
- built/staged artifact identity;
- ER / suite / cases;
- PAC / risk;
- environment and material dimensions;
- external fixtures such as viewer/application/network profile;
- oracle/pass condition and actual observation;
- result (`PASS`, `FAIL`, `BLOCKED` or the owning evidence schema's equivalent);
- logs/artifacts;
- execution timestamps.

### 10.1 Automated evidence is automatically captured

When a suite/orchestrator is automated, the automated execution **must emit its own evidence record or machine-consumable result metadata** for every retained Q/R result it claims.

It must capture candidate/artifact identity, relevant environment/fixture identity, ER/suite/case identity, oracle/result and artifact/log references without requiring a developer to retype them into a qualification ledger.

Hand-authored evidence records are reserved for properties that are genuinely manual/physical/human-observation based. Even then, the checklist and PASS/FAIL oracle are predefined.

This is a development-velocity and evidence-integrity requirement, not an optional reporting enhancement.

### 10.2 Exact release identity

For Release Acceptance, evidence that canonical release procedure requires to be exact-candidate-bound must use the same RC-FROZEN candidate/artifact identity.

If a post-freeze change changes that identity, old required release/physical evidence becomes historical/preflight for the new candidate unless canonical release authority explicitly defines an equivalent artifact identity rule.

Different candidate SHAs cannot be composed into one release PASS merely because a change was labelled “evidence-neutral”.

### 10.3 Invalidation

An old record remains historical truth for the artifact/environment it measured. “Invalidated” means it may no longer satisfy a newer claim without re-execution or an explicit authority-approved equivalence rule.

---

## 11. Flake, infrastructure and quarantine

A flaky blocking test is a test-system defect.

Failures are classified as:

- product defect;
- deterministic test defect;
- runner/environment failure;
- external viewer/device/dependency failure;
- unresolved/intermittent.

Retries may diagnose known infrastructure instability but must not silently turn an unexplained failure into PASS evidence.

Temporary quarantine requires:

- owner/reason;
- affected PAC/ER gap;
- compensating evidence where necessary;
- expiry/review condition;
- visible non-blocking status.

`BLOCKED` is not PASS and cannot silently shrink or preserve a positive support claim.

---

## 12. Test lifecycle governance

Every permanent addition answers:

1. Which PAC/ER/claim does it protect?
2. What failure escapes without it?
3. What is the cheapest honest seam?
4. Is existing evidence sufficient?
5. Is this a case or truly a new suite/identity?
6. What is the oracle/pass condition?
7. What invalidates retained evidence?
8. What gates/environments need it?
9. What changes trigger it?
10. What execution/setup/maintenance cost does it add?

Consolidate when cases share owner/setup/scheduling/environment/failure meaning. Do not consolidate into opaque mega-tests that lose case diagnostics.

Move expensive evidence to a later gate when fast deterministic coverage can protect normal PRs without weakening the product claim.

Remove only when the risk/claim no longer exists or stronger/cheaper evidence fully supersedes it. Record the replacement/reason.

---

## 13. Product-quality reporting

Raw CTest count is not a quality KPI.

Prefer reporting:

- PAC / ER coverage and gaps;
- affected V suites executed;
- product journeys executed;
- qualification cells and their upstream status values;
- deployment/native/viewer/security/performance/reliability/operability disposition;
- maintenance edges where declared;
- gate wall time and critical path;
- flake/quarantine/evidence debt;
- invalidated or blocked qualification evidence.

A meaningful release statement is that all required claims for the exact candidate have valid evidence — not merely that “N tests passed”.

---

## 14. Relationship to repository documents

| Document / mechanism | Role |
| --- | --- |
| `docs/architecture.md` | product architecture; links this hierarchy |
| `docs/internal/test-strategy.md` | **product-test strategy authority** |
| `docs/internal/test-system-architecture.md` | logical objects, selection, gates, evidence records, migration |
| `docs/internal/test-evidence-requirements.md` | stable product ER catalog, oracle/invalidation baseline |
| `docs/performance-optimization.md` | performance/SLO authority |
| `docs/compatibility.md` | public compatibility/status authority |
| physical/release runbooks | execution procedures owned by their domain authorities |
| `tests/TEST_CATALOG.md` | current registered CTest inventory |
| `tests/TEST_MATRIX.md` | current capability/selection facts |
| `tests/EXECUTION_BASELINE.md` | current hosted execution baseline |

Existing executable documents migrate incrementally; adopting this strategy does not by itself delete or reschedule anything.

---

## 15. Roadmap placement

This is cross-cutting engineering infrastructure, not a new version line.

- **V0.2** remains current delivery priority; TS-0 design must not expand its critical path.
- **V0.3 family** may incrementally introduce metadata, selection and reusable product-path foundations as self-service/product breadth becomes real.
- **Before V0.4 entry**, qualification/evidence machinery must already exist because V0.4 is qualification-only.
- **V0.4** executes the declared qualification programme rather than building a hidden test framework.
- **V1 GA** consumes the qualified evidence and freezes the long-lived support contract.

Detailed migration phases and TS-0..TS-8 work packages live in `test-system-architecture.md` and Issue #393.

---

## 16. Definition of a healthy HyRemote test system

The system is healthy when:

- product promises are traceable to explicit ERs and executed evidence;
- deterministic defects are found at cheap seams;
- native/deploy/platform risks run only where the revealing environment is required;
- Shared Runtime behavior is not copied across frontends;
- expensive qualification is staged away from normal edit loops;
- ordinary developer feedback remains fast;
- automated evidence does not create manual bookkeeping;
- PR selection is risk-proportional and never false-green zero;
- qualification status remains owned by product authority;
- exact RC evidence never mixes incompatible candidate identities;
- flake, runtime and duplicate proof are measured engineering debt;
- adding new Qt/platform/viewer cells does not automatically multiply ERs/CTest identities;
- V0.4 begins with the qualification machinery already ready.

The canonical loop is:

```text
Product vision
 -> PAC
 -> risk
 -> Evidence Requirement + oracle + invalidation
 -> cheapest sufficient technical proof
 -> risk-based execution gate
 -> qualification cell / maintenance edge where applicable
 -> exact artifact/environment/fixture evidence
 -> upstream product claim/status decision
```

That loop — not the number of registered CTests — is the HyRemote product test strategy.