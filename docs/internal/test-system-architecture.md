# HyRemote Test System Architecture

Status: **TS-0 detailed design — no current test/CI migration is authorized by this document alone**

Authority: #393

Parent strategy: [`test-strategy.md`](test-strategy.md)

Canonical Evidence Requirement identities: [`test-evidence-requirements.md`](test-evidence-requirements.md)

Product architecture: [`../architecture.md`](../architecture.md)

This document defines how HyRemote turns product contracts into executable, selectable, environment-bound evidence **without making the test system itself the development bottleneck**.

The design rule for TS-0 is:

> Freeze the evidence architecture before changing the current executable inventory.

`tests/TEST_CATALOG.md`, `tests/TEST_MATRIX.md`, `tests/EXECUTION_BASELINE.md`, CMake/CTest and current workflows remain the executable authority until later focused migration work is explicitly admitted.

---

## 1. Scope and non-goals

This architecture defines:

- logical test/evidence objects and their authority relationships;
- product-risk propagation and selection;
- suite/case identity rules;
- execution environments and gates;
- qualification cells, authority-owned claim-obligation profiles and supported maintenance edges;
- evidence-record identity, oracle and invalidation semantics;
- native/deployment/viewer/network/security/reliability/performance/operability evidence placement;
- development-velocity and maintenance-cost budgets;
- staged migration from the current test inventory.

It does **not** authorize:

- deleting, renaming, consolidating, re-registering or rescheduling current CTests;
- changing current CI selectors or `release-trains.json`;
- changing compatibility/support claims;
- inventing upgrade/rollback promises;
- weakening existing release gates;
- adding test-only public API or a second product/runtime architecture.

---

## 2. Authority hierarchy

Authority flows downward:

```text
product roadmap / architecture / compatibility / version / release authorities
                              |
                              v
                 docs/internal/test-strategy.md
                              |
                              v
             docs/internal/test-system-architecture.md
                              |
                              v
            docs/internal/test-evidence-requirements.md
                              |
                              v
       current tests / selectors / CMake / CI / runbooks
```

Rules:

1. Product authorities define **what HyRemote promises**.
2. Test Strategy defines **which product contracts and evidence classes matter**.
3. This document defines **how evidence is structured, selected and retained**.
4. The ER Catalog defines stable **requirement identities, activation/evidence-obligation semantics, oracles, invalidation, versioned claim-cell schema/predicate rules, Claim Obligation Profile schema/rules, and the frozen concrete profile/binding projection for current positive V0.2 claim families**.
5. Current tests implement those requirements incrementally; current test names do not redefine them.
6. The test system consumes compatibility status vocabulary from `docs/compatibility.md`; it never creates a parallel status enum. Product-behavior claim families that have no separate status enum remain valid claims without inventing one.
7. When an explicit newer product authority supersedes older prose/test assumptions, TS-1 reports the older assertion as authority drift rather than choosing product truth from the current test inventory.
8. PAC/risk membership and current suite coverage are reverse/index relationships; they never substitute for the authority-owned forward `claim -> obligation profile -> cell -> required product ERs` relation.
9. A profile binding is product/claim metadata, not suite metadata. TS-2/TS-6 may serialize and validate it but may not invent a missing binding from the tests that happen to exist.
10. Claim-completeness meta checks are outputs around the product-ER graph; `ER-COMPAT-EXPLICIT-CELL`, `ER-COMPAT-EVIDENCE-BINDING`, `ER-COMPAT-PLATFORM-INDEPENDENCE-TRUTH` and `ER-COMPAT-GRAPHICS-NO-INFERENCE` are never recursively included in the profile-expanded product ER set they evaluate.

---

## 3. Core model

```text
Product Acceptance Contract (PAC)
            |
            v
       Risk Domain
            |
            v
   Evidence Requirement (ER)
            |
      +-----+------------------+
      |                        |
      v                        v
 Suite / named Case    Qualification Cell / Maintenance Edge
      |                        |
      +-----------+------------+
                  v
         Execution Environment
                  |
                  v
            Execution Gate
                  |
                  v
             Execution
                  |
                  v
          Evidence Record(s)
                  |
                  v
          Product/Release Claim
```

The ER Catalog is the identity authority for ER IDs. Architecture examples, metadata and later TS-1/TS-2 mappings may only reference IDs that exist there.

For **positive claim completeness**, the forward relation is separate from the reverse ER/PAC graph and deliberately non-recursive:

```text
product / compatibility claim authority
            |
            v
   Claim Obligation Profile
            |
            v
qualification cell + material dimensions/capabilities
            |
            v
 expanded required product ER set
            |
            v
 required evidence classes + valid product evidence records
            |
      +-----+---------+---------------+----------------+
      |               |               |                |
      v               v               v                v
 META-CELL      META-BINDING    META-PLATFORM    META-GRAPHICS-SCOPE
      |               |               |                |
      +---------------+---------------+----------------+
                              v
                       claim completeness
```

The owning product/compatibility authority selects the applicable reusable profile. The ER Catalog defines the versioned claim-cell schema, profile schema, fail-closed expansion rules, and the current V0.2 profile instances/bindings for integration plus already-declared transport/input/security claim families. TS-2 may implement storage/parsing but cannot infer a profile from current tests, PAC overlap or available suite coverage.

A claim-bound qualification/evidence object preserves both directions:

```text
claim_id -> claim_profile_id -> cell_id + material dimensions -> expanded required_product_ERs
                                                                       |
                                                                       +-> evidence records proving subsets of product ERs

claim/cell/profile/binding identities + valid records
        -> derived compatibility meta results
```

This makes missing obligations detectable without requiring one record or one suite per ER and without making a meta ER depend on itself or another cell's meta PASS.

### 3.1 Product Acceptance Contracts

The stable PAC set is defined by the strategy:

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

PAC IDs are product-contract identifiers, never test names.

### 3.2 Risk domains

Initial stable vocabulary:

| Risk | Meaning |
| --- | --- |
| `R-CORE` | Core lifecycle/ownership/scheduling/backpressure semantics |
| `R-RUNTIME` | Shared Runtime lifecycle/policy/composition |
| `R-CAPTURE` | target/surface capture, damage, DPR, resize |
| `R-INPUT` | routing, focus, text, held-state safety |
| `R-FRONTEND` | C++/QML/Generic/QPA boundary mapping |
| `R-NATIVE` | local/native platform/display/input/graphics coexistence |
| `R-TRANSPORT` | transport protocol and delivery semantics |
| `R-NETWORK` | listener/reachability/stall/disconnect behavior |
| `R-SECURITY` | security-policy truth, auth, downgrade, secrets |
| `R-PACKAGE` | export/acquisition/installed metadata |
| `R-DEPLOY` | runtime closure, relocation, dependency origin |
| `R-COMPAT` | support-matrix/environment truth |
| `R-PERF` | latency/freshness/resource efficiency |
| `R-RELIABILITY` | long-run/resource/robustness stability |
| `R-OPERATE` | diagnostics/supportability/safe operation |
| `R-UPGRADE` | authorized maintenance/version transitions |
| `R-ADOPTION` | self-service user journey |

Risk domains remain product-oriented and do not mirror every source folder.

### 3.3 Evidence Requirement

An ER answers **what must be demonstrated**. Its canonical fields and conditional evidence obligations are defined in the ER Catalog.

Every retained ER has:

- stable ID;
- PAC/risk mapping;
- activation rule;
- explicit evidence obligation by context;
- product statement;
- explicit oracle/pass condition;
- explicit invalidation rule.

A log, screenshot, benchmark number or manual observation is not evidence by itself unless interpreted through the ER's oracle.

### 3.4 Suite and case

A **suite** is an executable evidence unit with one owner, scheduling shape and failure domain. A **case** is a named scenario/input within a suite.

A case becomes a separate executable/CTest identity only when one of these materially differs:

- capability/platform registration;
- environment/host/device/viewer;
- timeout/cost class;
- process/destructive/fuzz isolation;
- selector ownership;
- expected-result model;
- evidence-retention requirement.

Otherwise use named table-driven cases. Consolidation must preserve exact case name, ER/PAC, oracle, environment/fixture and actionable failure output.

### 3.5 Execution is not evidence identity

An **execution** is one actual invocation of a suite/case set in one concrete environment/artifact context. An **evidence record** is one retained interpretation of observations from that execution against a specific ER and, when claim-bound, a specific claim/cell/profile identity.

Therefore one expensive execution may legitimately generate more than one evidence record when all of the following are true:

- the same execution actually exercised each claimed oracle;
- candidate/artifact/environment/fixture identity is compatible with every emitted record;
- each record names the exact ER subset it proves;
- each claim-bound record has its own `claim_id` / `cell_id` / profile revision and obligation snapshot;
- no record infers an unobserved platform, graphics path, viewer, capability or product state;
- failures remain attributable to the individual oracle/case.

A shared execution is an optimization of **observation cost**, not permission to weaken evidence identity. The orchestrator must deliberately materialize claim-specific records; a database join must never silently reinterpret one claim's record as another claim's evidence.

---

## 4. Evidence class and substitution model

Evidence classes are defined by the strategy/ER Catalog:

- V — deterministic Verification;
- P — Product Validation;
- Q — environment/cell Qualification;
- R — exact-candidate Release Acceptance.

An **execution gate controls when work runs; it does not confer evidence class**.

Examples:

- a hosted G4 run may still be only V/P;
- a physical desktop run is not automatically Q unless it exercises the exact ER/cell/oracle;
- a release workflow result is not R if it is not bound to the exact frozen candidate/artifact required by release authority.

Allowed substitution is narrow:

- a stronger exact environment may satisfy a weaker environment requirement only if it exercises the same property and remains diagnosable;
- a deterministic shared-layer proof may replace duplicate frontend copies when the frontend adds no unique semantics.

Not allowed:

- hosted/headless replacing native local-display/input evidence;
- Windows replacing Linux or vice versa for platform-sensitive claims;
- nearby Qt patch replacing exact QPA private-ABI evidence;
- one viewer/encoding replacing another claimed viewer/encoding path;
- physical evidence replacing clean package/deployment evidence;
- older candidate evidence replacing exact RC evidence;
- target-version launch replacing an explicitly supported maintenance-transition test;
- zero relevant execution replacing evidence.

---

## 5. Suite-family architecture

Logical families guide ownership; they do not require immediate directory/test moves.

| Family | Responsibility | Normal classes |
| --- | --- | --- |
| `S-CORE` | platform/transport-neutral invariants | V |
| `S-RUNTIME` | shared Qt-aware product semantics | V |
| `S-ADAPTER` | Widgets/Quick capture/input surface contracts | V |
| `S-FRONTEND` | C++/QML/Generic/QPA unique mapping | V/P |
| `S-TRANSPORT` | transport-neutral + RFB-specific protocol/delivery | V/P/Q |
| `S-PACKAGE` | export/acquisition/package contracts | V/P |
| `S-DEPLOY` | install/deploy/runtime closure/relocation | P/Q/R |
| `S-PRODUCT-PATH` | vertical user journeys | P/Q/R |
| `S-OPERABILITY` | effective diagnostics/failure classification | V/P/Q |
| `S-MAINTENANCE` | authorized update/rollback edges | V/P/Q/R |
| `S-SECURITY` | policy truth + mechanism-specific security | V/P/Q/R |
| `S-RELIABILITY` | stress/soak/fuzz/resource bounds | V/Q/R |
| `S-PERFORMANCE` | interaction SLO/resource regression | V/Q/R |
| `S-COMPAT` | qualification-cell orchestration and derived claim-meta checks | Q/R |
| `S-REALWORLD` | representative third-party pressure tests | Q/R |

Repository/branch/docs/release-policy governance checks are tracked separately from product-quality evidence.

Shared semantics are proved at the shared layer once. Frontends prove mapping/activation/packaging/native behavior plus a bounded vertical path; they do not each reimplement Core/Runtime/RFB/security/backpressure proof.

---

## 6. Metadata model

Later implementation should expose machine-readable suite metadata without creating a second product authority.

Conceptual example:

```yaml
suite_id: S-DEPLOY-LINUX-RUNTIME-CLOSURE
owner: deployment
er: [ER-DEPLOY-RUNTIME-ORIGIN, ER-DEPLOY-FOREIGN-QT-ISOLATION-LINUX]
pac: [PAC-5, PAC-9]
risk: [R-DEPLOY, R-COMPAT]
cost_class: installed
platform_sensitivity: linux
environments: [E-DEPLOY-LINUX]
gates: [G2, G4, G6]
triggers: [deploy-helper, install-export, runtime-resolution]
isolation: clean-prefix
expected_max_wall: 60s
```

Principles:

- suite-level metadata is inherited by cases where possible;
- metadata references ER/product authorities rather than copying their policy;
- unknown/malformed metadata fails closed for product lanes;
- selectors consume metadata instead of test-name folklore;
- adding one case must not imply one new Issue, CTest identity or CI job.

Claim Obligation Profiles are **not suite metadata**. They are upstream claim-policy data owned by product/compatibility authority; suite metadata may reference the ERs it proves but cannot declare a positive claim complete by omitting required ERs.

---

## 7. Change-impact and selection

Selection is a graph:

```text
changed files/config/public contract
        -> ownership area
        -> risk domain
        -> ERs
        -> candidate suites/cases
        -> capability/environment filter
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
- network;
- security;
- package-export;
- deploy-helper;
- diagnostics/operability;
- public-version/maintenance;
- examples/adoption;
- compatibility/qualification;
- performance;
- CI/test-system;
- docs-only;
- repository-governance.

Examples:

```text
Core lifecycle change
 -> R-CORE/R-RELIABILITY
 -> deterministic Core + affected Runtime seams
 -> no unrelated deploy/QPA matrix

HyRemoteDeploy.cmake/runtime resolution
 -> R-DEPLOY/R-PACKAGE/R-COMPAT/R-UPGRADE
 -> clean Windows + Linux deployment evidence
 -> maintenance edge only when replacement semantics are affected
 -> no automatic full Core replay

QML frontend mapping
 -> R-FRONTEND/R-ADOPTION
 -> QML mapping + QML product path
 -> no automatic QPA deploy matrix

QPA factory/delegate
 -> R-FRONTEND/R-NATIVE/R-COMPAT/R-DEPLOY
 -> QPA deterministic/native/deploy evidence
 -> exact Qt private-ABI qualification when invalidated

material executable documentation/journey change
 -> R-ADOPTION / affected ER
 -> relevant journey only

spelling/translation/format/unrelated prose
 -> docs/governance only
 -> no expensive product-evidence invalidation
```

Selector invariants:

- selected product risk must execute at least one relevant suite;
- zero relevant execution is invalid evidence;
- unknown ownership broadens conservatively rather than returning empty;
- selector failure is visible;
- full-gate override exists for uncertainty/RC;
- selection records why expensive suites were included;
- selectors themselves have deterministic self-tests.

High-propagation changes include public API, Shared Runtime state/security semantics, package/export/deploy resolution, QPA private ABI, transport parser/security framing, diagnostics schema, maintenance contract, compatibility claims, claim-cell schema/profile/binding definitions and selector metadata itself.

---

## 8. Execution gates and velocity budgets

| Gate | Purpose | Budget/placement |
| --- | --- | --- |
| G0 Developer | changed module/direct dependencies | <= 10 s normal target |
| G1 PR Verification | affected deterministic evidence | <= 90 s target |
| G2 PR Product | affected user/deploy/product path | <= 3–5 min target |
| G3 Merge Sentinel | nonzero mainline health/false-green guard | <= 60 s target |
| G4 Nightly Qualification-lite | broad P + selected Q/stress/perf | <= 30 min target |
| G5 Weekly/Extended | soak/fuzz/broad viewer/network/perf/real-world | non-PR |
| G6 Release Qualification | exact candidate evidence completeness | no minute budget |

Budgets never authorize weakening evidence required by a real risk.

When a gate exceeds budget, optimize in this order:

1. remove unrelated selection;
2. deduplicate compatible obligations into shared executions while retaining claim-specific records;
3. parallelize independent work;
4. safely reuse build/download artifacts where the property permits;
5. share expensive fixture setup inside a suite;
6. parameterize cases;
7. separate deterministic proof from product proof;
8. move breadth/repetition to later gates;
9. optimize the test implementation;
10. only then reconsider whether the ER itself is unnecessarily strong.

Track wall-time critical path rather than raw test count: suite P50/P95, gate wall time, setup/build/install cost, device/viewer acquisition, retry/flake cost, expensive-suite selection rate, duplicated observation cost and redundant cross-OS execution.

Development velocity also includes **maintenance/cognitive cost**:

- ordinary PR authors do not maintain qualification ledgers;
- automated runs produce automated records;
- suite metadata is inherited;
- reusable Claim Obligation Profiles prevent per-cell ER-list duplication;
- compatible overlapping profiles share executions rather than automatically multiplying expensive runs;
- no per-case Issue/job/CTest explosion;
- editorial changes do not invalidate unrelated expensive evidence.

---

## 9. Execution environments

Environment identity is factual, not just a runner label.

- `E-SYNTHETIC` — deterministic process/model tests;
- `E-HOSTED-LINUX`, `E-HOSTED-WINDOWS` — hosted regression;
- `E-DEPLOY-LINUX`, `E-DEPLOY-WINDOWS` — clean runtime-loader/deployment environments;
- `E-NATIVE-LINUX`, `E-NATIVE-WINDOWS` — qualified native desktop/device;
- `E-NETWORK-LAB` — controlled RTT/bandwidth/loss/stall;
- `E-PERF-REF` — stable benchmark/reference environment;
- `E-EMBEDDED` — qualified embedded/device stack;
- `E-THIRDPARTY` — pinned representative application;
- `E-MAINTENANCE` — isolated source-artifact -> target-candidate transition.

Material environment facts include as applicable OS, CPU, Qt, native platform/delegate, graphics/driver/DPR, toolchain, viewer revision, application revision and network profile.

---

## 10. Qualification cells and maintenance edges

A qualification cell is one explicit support boundary. Claim-bound cells carry explicit forward-mapping identity, conceptually:

```yaml
claim_id: <stable authority row/family identity>
cell_id: <stable atomic qualification-cell identity>
cell_schema: CCS-1
claim_status: <opaque authority value when this claim family has one; omit otherwise>
claim_profile_id: <authority-owned profile id>
claim_profile_revision: <immutable content-addressed resolved profile+binding revision>
material_dimensions:
  product_line: ...
  qt: ...
  os_arch: ...
  integration: ...          # only when material to this claim
  ui_target: ...            # only when material to this claim
  graphics_scope: ...       # only when material to this claim
  deployment_form: ...      # only when material to this claim
  native_platform: ...      # only when material to this claim
  transport: ...            # only when material to this claim
  network_profile: ...      # only when material to this claim
  remote_input_policy: ...  # only when material to this claim
  security_profile: ...     # only when material to this claim
  transport_security: ...   # only when material to this claim
  viewer: ...               # only for a named-viewer claim
expanded_required_er: [ER-...]  # profile-expanded product ER obligations only
```

`CCS-1` names, string value semantics, required-dimension rules and the allowed predicate grammar are canonical in the ER Catalog. The registry validates that exact schema; it does not accept unknown dimensions or invent coercions/range semantics. A future incompatible claim-cell vocabulary uses an explicitly versioned successor schema.

`claim_status` is not invented by the evidence system. Matrix rows carry the opaque compatibility value supplied by `docs/compatibility.md`; current prose product-behavior claim families such as RFB/input/security carry no synthetic status field unless their owning authority later defines one.

`claim_profile_revision` is produced by the profile registry from the normalized, fully resolved profile inheritance graph plus the claim→profile binding. It is immutable/content-addressed system output, not free text supplied by a developer. The full `expanded_required_er` list is the authoritative historical snapshot of **product ER obligations** for that cell. Compatibility claim-meta ERs are evaluated outside that list as derived checks. The revision lets the registry detect that the underlying profile/binding changed.

The conceptual dimension space is:

```text
Qt anchor
x OS / CPU
x native platform/delegate
x graphics path
x UI family
x frontend
x deployment form
x viewer/security/network/input/capability profile where material
```

Do not execute the full Cartesian product. Use:

- anchor cells;
- interaction-risk cells;
- representative/pairwise cells;
- orthogonal claim families where a Shared Runtime property is independent of frontend identity.

Every positive qualification/support/product-behavior cell references an applicable authority-owned Claim Obligation Profile. The cell's declared material dimensions/capabilities deterministically expand that profile into `required_product_ERs(cell)` using the ER Catalog rules. Unknown/no-applicable profiles, unknown schema/dimensions, missing required dimensions, malformed predicates, a profile revision that cannot be resolved, or a cell whose stored expansion does not equal the canonical product expansion are incomplete/fail-closed. PAC/risk membership does not infer missing obligations, and the registry cannot shrink the set to match existing tests.

After product-set expansion, claim completeness additionally requires the non-recursive claim-meta checks defined by the ER Catalog:

- `ER-COMPAT-EXPLICIT-CELL` — identity/schema/profile/dimension truth;
- `ER-COMPAT-EVIDENCE-BINDING` — record/candidate/artifact/environment/fixture/invalidation binding;
- `ER-COMPAT-PLATFORM-INDEPENDENCE-TRUTH` — exact platform binding and non-substitution, without making one platform depend on another platform's PASS;
- `ER-COMPAT-GRAPHICS-NO-INFERENCE` — baseline/specialized graphics claim-boundary truth, without making a baseline cell depend on an unrelated specialized claim.

These meta checks consume the expanded product ER set and valid records; **none is inserted into `required_product_ERs(cell)`**.

The current V0.2 integration matrix plus already-declared RFB/input/security product-behavior families have concrete profile instances and stable claim/cell identities frozen in the ER Catalog. Combined `Widgets / Quick` rows normalize into two atomic cells; security capability on/off is split only where it materially changes the claimed fail-closed result. Future positive claim families must add their own authority-owned binding and stable claim/cell identities before TS-2/TS-6 can call them complete.

Profiles are reusable/inheritable across claim families, so adding another Qt/OS/viewer cell normally adds dimension data rather than another copied ER list. Overlapping profiles do not imply duplicate execution: the execution planner may coalesce compatible required observations, then emit separately bound records for each affected claim/cell.

QPA exact private ABI remains exact Qt patch/platform evidence; public-Qt frontends may use the product authority's declared family/range rules.

A maintenance edge is sparse and exists only when product/version authority declares it:

```text
accepted source release/artifact -> target candidate/release -> optional promised rollback
```

The test system never infers all-history upgrade compatibility.

---

## 11. Product-path architecture

Canonical user journey:

```text
compatibility/evaluate
 -> acquire
 -> integrate/configure
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
 -> supported update/rollback where declared
 -> verify coherent target state
```

Representative frontend anchors may include C++ Widgets/Quick, QML Quick, Generic Widgets/Quick, QPA Widgets/Quick and explicitly supported combined payload forms. An anchor path does not run on every PR.

A product-path record captures applicable acquisition form, application link contract, exact Qt/HyRemote identity, deployment/runtime origins, native platform, listener/security/input policy, viewer/version, launch/view/control/reconnect/stop result, diagnostic state and maintenance source/target identities.

---

## 12. Deployment and maintenance evidence

Deployment is a product subsystem, not a CMake-helper-only concern.

Evidence layers:

1. package/export contract;
2. deployment planning/accept-reject semantics;
3. artifact closure;
4. isolation/relocation;
5. runtime origin;
6. launch/viewer product fit.

Linux Qt provenance rule: the selected qualified Qt SDK/deployed copy is the positive trust source for Qt lineage. Distribution Qt is environmental runtime for unrelated applications, not an alternate HyRemote development SDK candidate. Prove positive ownership/origin rather than blacklist `/lib`/`/usr/lib` globally.

Current listener semantics follow the owning network product authority (#174 until superseded): default wildcard LAN reachability and exact-address/interface narrowing. Stale loopback assertions are authority drift, not alternate product truth.

Maintenance transitions additionally verify no stale/mixed libraries/plugins/QML/package metadata, no incompatible QPA payload, coherent diagnostic version/artifact identity and coherent rollback when rollback is promised.

---

## 13. Native/viewer/network qualification

For native support cells observe the same application through:

```text
native baseline -> HyRemote view-only -> control-enabled where supported
```

Verify applicable local rendering/input/focus/resize/dialog/popup/multi-window/graphics behavior, remote view/input, reconnect, per-viewer held-state cleanup, stop/application survival and slow-viewer isolation.

Headless/offscreen/Xvfb remain useful regression environments but cannot independently prove native display/input/graphics claims.

Viewer interoperability is explicit and versioned. Passing one viewer/encoding never implies all RFB clients or extensions.

Network profiles may include localhost baseline, LAN reference, RTT, bandwidth cap, bounded loss, slow reader, stalled handshake, abrupt disconnect, reconnect storm and mixed-latency multi-viewer. Oracles are correctness/boundedness/freshness/recovery/native health/latency — not merely TCP connection survival.

---

## 14. Reliability, security and performance integration

Deterministic reliability proof comes first: bounded queues, teardown ownership, callback/adapter exception containment, cancellation, held-input cleanup, parser bounds and per-viewer work bounds. Exception containment is a distinct Core/Runtime property: an exception must be translated into the declared start/fault/recoverable outcome and must not escape into or terminate the host merely because teardown itself later completes correctly. Stress/soak/fuzz complement these at G4/G5/G6 rather than burden ordinary PRs.

Security is policy-first:

```text
requested profile
 -> capability available?
 -> configuration valid?
 -> protection actually established?
 -> only then listener/session allowed
```

A success-path mechanism ER is active only for a capability actually available in the artifact/declared positive cell. Requests for unavailable protection are evaluated by the applicable fail-closed ER; requesting a missing capability never creates an impossible success obligation.

View/control policy is also enforced behavior: view-only input attempts must be dropped, not merely configured as disabled.

Performance's primary product metric is remote interaction latency:

```text
viewer action -> route to Qt -> application update -> capture -> delivery -> viewer observation
```

Frame age, capture/encode time, bytes, CPU/GPU/memory and local UI impact are diagnostic metrics. Performance authority owns the exact workloads/SLOs.

These domain programmes retain their existing product authorities; TS-7 only wires their already-defined evidence into the common ER/cell/record model.

---

## 15. Operability and real-world evidence

Operability proves that an installed user can obtain truthful effective product facts without source/CI knowledge, including applicable product/build identity, Qt/OS/architecture, route/UI facts, Runtime state, listener/security/input policy, client count, bounded last error and deployed artifact identity, without secret leakage.

Representative third-party applications pressure-test low intrusion and integration complexity. They are pinned fixtures and do not automatically create named-application support claims.

---

## 16. Evidence records and validity

Every actual invocation has one immutable `execution_id`. Raw logs/screenshots/measurements may be attached once to that execution and referenced by multiple evidence records; retaining multiple claim interpretations does not require copying the raw artifact payload.

Every retained Q/R evidence record conceptually contains material identity such as:

```yaml
record_id: immutable-record-id
execution_id: immutable-execution-id
candidate_sha: exact-source-sha
artifact_identity: exact-built-or-staged-artifact
claim_id: <stable claim identity when record is claim-bound>
cell_id: <stable atomic qualification-cell identity when record is claim-bound>
cell_schema: <CCS-1 or later explicit version when record is claim-bound>
claim_profile_id: <profile id when record is claim-bound>
claim_profile_revision: <immutable content-addressed resolved profile+binding revision>
expanded_required_er: [ER-...]   # full canonical product-obligation snapshot for this claim/cell
required_er_set_digest: sha256:<lowercase-hex>
suite_id: logical-suite
case_ids: [...]
er: [ER-...]                     # product ER subset actually proved by this record
pac: [PAC-...]
risk: [R-...]
evidence_class: Q|R
environment: {os, cpu, qt, native_platform, graphics, toolchain, ...}
fixtures: {viewer, application_revision, network_profile, ...}
oracle: {type, pass_condition, observation}
result: PASS|FAIL|BLOCKED
commands: ...
logs_artifacts: ...
started_at: ...
completed_at: ...
```

For a claim-bound execution record, `expanded_required_er` is the lexicographically sorted unique list of canonical **product ER IDs** produced by the resolved profile+cell expansion. It excludes all claim-meta ERs. `required_er_set_digest` is a consistency checksum only: SHA-256 over the UTF-8 bytes of the compact JSON array encoding of that exact sorted list (double-quoted JSON strings, comma separators, no insignificant whitespace), rendered as lowercase hexadecimal after `sha256:`. The full list, not the digest, is the historical product-obligation snapshot.

`claim_profile_revision` is an immutable content-addressed revision emitted by the registry for the normalized resolved profile inheritance graph plus the applicable claim→profile binding. A retained record never relies on a mutable “latest profile” lookup to explain its historical decision.

Material fields vary by ER. For a claim-bound Q/R execution record, missing/unresolvable claim/profile/cell/schema identity, a profile revision that cannot be reconciled with the retained snapshot, or a digest that does not match `expanded_required_er` makes evidence invalid/blocked rather than silently reusable. One execution record may prove only a subset of the cell's required product ERs.

Claim completeness is evaluated in two stages:

1. union valid execution records that satisfy every triggered evidence obligation for every ER in `required_product_ERs(cell)` for the same claim/cell/schema/profile revision and compatible candidate/artifact/environment identity;
2. run the outer `META-CELL`, `META-BINDING`, `META-PLATFORM` and `META-GRAPHICS-SCOPE` evaluators. Their derived compatibility-meta Q/R results may be retained in a claim-evaluation artifact, but are not inserted into `expanded_required_er` and are never prerequisites for their own or another cell's product-evidence evaluation.

### 16.1 Shared execution / multi-record rule

A scheduler/orchestrator should coalesce compatible expensive observations before creating duplicate executions. A single execution may fan out to multiple records across ERs or claim/cell identities only when the actual observation covers every emitted oracle.

Examples that may be coalesced when identities align:

- one native desktop run can observe native display, native input, remote view and stop-survival, then emit separate records for those ERs;
- one RFB session can prove protocol/update behavior and an applicable security mechanism when both were actually exercised;
- one clean deployed launch can supply deployment-origin/isolated-launch observations to more than one compatible claim family;
- one physical/manual session may support multiple claims only when the predeclared checklist contains every required oracle and each claim receives its own bound record.

Forbidden reuse includes:

- relabelling a Windows observation as Linux;
- relabelling a portable-baseline observation as specialized graphics evidence;
- treating an Insecure session as proof of Authenticated behavior;
- treating a viewer fixture as a named-viewer support claim that was never declared;
- reusing a record across a different claim/cell/profile revision solely because the underlying ER ID matches.

The planner should measure `executions_saved_by_coalescing`, duplicated observation wall time and fan-out ratio so evidence reuse remains visible rather than becoming hidden coupling.

### 16.2 Automated-record rule

Any automated suite/orchestrator that claims retained Q/R evidence **must generate its own machine-consumable execution evidence record from execution context**. It must automatically bind execution/candidate/artifact identity, ER/suite/case, material environment/fixture identity, oracle/result, timestamps and logs/artifacts. When the run is claim-bound it must also bind `claim_id`, `cell_id`, `cell_schema`, `claim_profile_id`, immutable profile/binding revision, the full canonical `expanded_required_er` product snapshot and its checksum.

When one execution supports multiple claims/cells, the orchestrator emits separate claim-bound records that share the same `execution_id`; it never asks an ordinary PR author to duplicate or hand-edit the record.

The claim-completeness orchestrator must likewise generate the derived claim-evaluation result for all applicable outer compatibility meta checks; ordinary PR authors do not author or edit those derived PASS/FAIL results.

Ordinary PR authors must not manually transcribe evidence identity facts. If required identity cannot be captured, the automated result is BLOCKED/invalid evidence rather than a form to fill in later.

Hand-authored records are reserved for genuinely manual/physical/human-observation evidence and still use a predefined ER oracle/checklist plus the same claim/profile/cell/schema identity and expanded product-obligation snapshot when the observation supports a positive claim.

### 16.3 Invalidation

An evidence record remains historical truth for exactly what it measured. Invalidation means it cannot satisfy a newer claim without re-execution or an explicit equivalence decision by the owning product/release authority.

Typical invalidators: protected behavior/source change, public package/deploy metadata, Qt/toolchain/native/graphics anchor, named viewer/fixture revision, performance workload/reference host, product support policy, claim-cell schema/profile/rule/binding/cell identity, maintenance source/target identity and exact candidate/artifact for R evidence.

Editorial spelling/formatting/translation/unrelated prose is not an invalidator unless it materially changes an executable journey, product claim or oracle.

Invalidation is evaluated per evidence record/ER/claim binding. Multiple records may share one `execution_id` yet have different validity later because a product claim/profile changed without invalidating the raw historical observation for another ER.

### 16.4 Exact-candidate release rule

For Release Acceptance, every evidence item that canonical release procedure requires to be exact-candidate-bound must use the same RC-FROZEN candidate/artifact identity.

A post-freeze SHA/artifact change makes prior required R/physical evidence historical/preflight for the new candidate unless canonical release authority explicitly defines equivalent artifact identity. An `evidence-neutral` label cannot combine different candidate SHAs into one release PASS.

Change control decides re-cut/rerun scope; it cannot bypass identity binding.

---

## 17. Failure, retry and quarantine policy

Failure classification:

- product defect;
- deterministic test defect;
- environment/runner infrastructure failure;
- external viewer/device/dependency failure;
- unresolved/intermittent.

Retries are diagnostic for known infrastructure/external instability only; an unexplained failure cannot become PASS by retry.

Temporary quarantine requires owner, reason, affected ER/PAC gap, compensating evidence where required, expiry/review condition and visible non-blocking state. Quarantine cannot silently shrink a positive support claim.

`BLOCKED` is never PASS.

---

## 18. Test lifecycle governance

A permanent addition answers:

1. Which ER/PAC/support claim does it protect?
2. What concrete failure escapes without it?
3. Why is existing evidence insufficient?
4. What is the cheapest sufficient seam?
5. What is the oracle?
6. What invalidates retained evidence?
7. Is it a case or truly a new suite/identity?
8. Which gates/environments need it?
9. What is the cost/critical-path effect?
10. What changes trigger it?

Tests may be consolidated/moved/removed when the product risk disappears, stronger/cheaper proof supersedes it, duplicate frontend/shared proof exists, implementation detail disappears, or the check is governance rather than product evidence. Historical existence alone is not a KEEP reason.

---

## 19. Migration phases

Migration is staged and reversible.

### M0 / TS-0 — Architecture freeze

Deliver accepted Strategy, Detailed Architecture and ER Catalog. No current test/CI behavior changes.

### M1 / TS-1 — Read-only inventory audit

TS-1 starts by pinning an explicit `develop` inventory snapshot SHA. Every identity/count/registration/gate/cost fact belongs to that snapshot. If `develop` moves while the audit is open, record a separate delta and reconcile it explicitly; never silently mix evidence from different repository states.

For every current test map ER/PAC/risk, actual oracle, evidence class/environment, platform/capability, gate/trigger, cost, duplication/unique value, authority drift and candidate disposition.

Report unmapped ER gaps. **No test edits or scheduling changes.**

### M2 / TS-2 — Metadata/registry foundation

Introduce minimal machine-readable mapping while keeping current behavior comparable. ER Catalog remains identity authority. Versioned claim-cell schema, Claim Obligation Profiles and current concrete bindings are authority-owned input data; the registry validates/expands them but does not invent or weaken them.

### M3 / TS-3 — Evidence-driven consolidation

Use M1/M2 facts to parameterize fragmented cases, separate governance and remove duplicate frontend/shared proof. Do not consolidate correctness tests merely to reduce count.

### M4 / TS-4 — Risk-based G0–G3 selection

Implement fail-closed selectors, self-tests, visible manifests, conservative fallback and measured critical-path improvement.

### M5 / TS-5 — Reusable product-path orchestration

Create reusable clean acquisition/install/deploy/isolation/launch/viewer/diagnostics/declared-maintenance scenarios.

### M6 / TS-6 — Qualification/evidence framework

**Complete before V0.4 entry.** Provide qualification cells, versioned claim-cell schema validation, authority-owned Claim Obligation Profile expansion, concrete claim/profile/cell binding identity, maintenance-edge references, environment/fixture identity, ER/oracle/invalidation integration, execution-vs-record separation with safe fan-out/coalescing, self-describing automatic records, non-recursive claim-meta evaluation, physical/manual record schema, exact artifact binding and compatibility traceability.

### M7 / TS-7 — Domain-programme integration

**Complete evidence wiring before V0.4 entry.** Integrate the already-defined compatibility/physical/real-world/performance/security/reliability programmes into the common ER/cell/record model. V0.4 must not be used to invent or wire missing domain evidence plumbing.

### TS-8 — Continuous health

From TS-4 onward, monitor gate SLO, flake, duplicate proof, invalidated evidence and obsolete tests. This continues through V0.4/V1 because it is health monitoring, not qualification-framework feature development.

---

## 20. Roadmap placement

This programme is cross-cutting engineering infrastructure, not a new product version line.

### V0.2

First User Trial remains priority. TS-0 design, later read-only TS-1 and concrete V0.2 regression evidence may proceed without broad migration that delays delivery.

### V0.3 family

TS-2 through TS-7 land incrementally as self-service/package/platform breadth becomes real. The test system proves product work; it does not replace product work.

### Before V0.4 entry

All machinery needed to represent and collect the declared GA matrix must already exist, including:

- risk selectors/gates needed by qualification;
- product-path orchestration;
- versioned qualification-cell schema/environment identity;
- authority-owned claim-obligation profile expansion with concrete claim/profile/cell binding identity;
- execution-vs-record separation so overlapping claims do not force duplicate expensive observations;
- self-describing automatic/manual evidence records carrying schema identity plus the full expanded product-obligation snapshot;
- non-recursive derived claim-meta evaluation;
- exact candidate binding;
- domain-programme evidence wiring for compatibility, physical/native, security, reliability, performance and real-world evidence.

### V0.4

**Qualification only.** V0.4 executes already-wired evidence, records results, resolves product defects/qualification gaps and performs continuous-health monitoring. It does not build new test-framework/domain-plumbing features except a release-blocking correction to evidence machinery itself.

### V1 GA

Consume qualified evidence and freeze the long-lived support contract. Final GA is not an architecture redesign phase.

---

## 21. Work-package plan

| WP | Work | Depends on | Output | Placement |
| --- | --- | --- | --- | --- |
| TS-0 | strategy + architecture + ER catalog freeze | #393 | accepted design; no test changes | now, non-blocking V0.2 |
| TS-1 | current inventory semantic/cost audit | TS-0 | snapshot-bound read-only mapping + gaps/delta | after accepted design |
| TS-2 | logical metadata/registry | TS-1 | ER/risk/suite/gate + versioned concrete claim-profile/cell map | V0.3 incremental |
| TS-3 | suite/identity consolidation | TS-1/2 | parameterized suites + governance separation | V0.3 bounded PRs |
| TS-4 | risk selector + G0–G3 | TS-2 | execution manifest + budgets | V0.3 before matrix growth |
| TS-5 | product-path harness | TS-2 + productized paths | clean reusable journeys | V0.3 self-service |
| TS-6 | qualification/evidence records | TS-2/5 + product matrix | cells, schema/profile product-ER expansion identity, shared-execution fan-out, automatic records, derived meta checks, support traceability | complete before V0.4 |
| TS-7 | domain-programme integration | TS-6 + existing authorities | compatibility/physical/security/reliability/perf/real-world evidence wiring | complete before V0.4 |
| TS-8 | continuous test-system health | TS-4 onward | runtime/flake/duplicate/evidence governance | ongoing, including V0.4/V1 |

Do not create one Issue per test, ER or case. TS-1 facts determine the smallest independently mergeable implementation packages.

No TS work package enters `release-trains.json` merely because it exists; normal product/release authority decides release gating.

---

## 22. Existing authority ownership

| Authority | Remains owner of |
| --- | --- |
| #1 | product roadmap / sequencing |
| #24 | version semantics / maintenance boundary |
| #343 | GA compatibility/product baseline |
| #57 | Qt/support qualification content, including exact-QPA qualification policy |
| #109 | physical/native evidence |
| #134 / #242 | real-world programme |
| #370 / #9 | performance SLO / qualification |
| #143 and security authorities | security capability/truth |
| #174 | current IPv4 listener product contract until superseded |
| #335 | self-service diagnostics capability |
| #165 | candidate freeze/change control |
| #95 / release authority | release/version mechanics |
| `docs/compatibility.md` | compatibility status vocabulary/current public compatibility rows and current RFB/input/security product-boundary statements; their evidence-profile projection is frozen by the ER Catalog |

Other product/domain authorities select obligation profiles for their own future positive claim families. The test architecture supplies common evidence plumbing and execution economics; it does not take scope ownership away from these authorities.

---

## 23. TS-0 acceptance criteria

TS-0 is complete only when review can answer, independently of current CTest count:

1. What product promises/PACs are protected?
2. What risk domains can violate them?
3. What stable ER and oracle proves each product property, including exception containment as distinct from teardown?
4. What invalidates retained evidence?
5. Which seam is the cheapest honest proof?
6. When is a variant a case versus a suite/identity?
7. How does a change select ERs/suites without false-green zero execution?
8. How are Windows/Linux/Qt/QPA/platform differences represented?
9. How are clean deployment and runtime origin proved?
10. How are native claims kept separate from hosted evidence?
11. How are viewer/network/security/performance/reliability dimensions handled without Cartesian explosion?
12. How does every currently positive integration or product-behavior claim deterministically obtain a concrete authority-owned profile and required product ER set without inferring from current tests/PAC overlap?
13. How are versioned claim-cell schema plus stable claim/cell/profile identity and the full expanded product-obligation snapshot retained in Q/R records so historical completeness is reproducible?
14. How are all compatibility claim-meta ERs evaluated non-recursively outside the product-obligation set, including cross-platform and graphics non-inference rules?
15. How can overlapping claims share one expensive execution without weakening claim/cell-specific evidence identity?
16. How are diagnostics and maintenance paths proved without inventing product promises?
17. How are automated records produced without manual PR bookkeeping?
18. How is exact-candidate release binding preserved?
19. How are flaky/infrastructure failures distinguished from product failures?
20. How does TS-1 remain snapshot-consistent if `develop` moves during the audit?
21. How does migration avoid a big-bang rewrite?
22. How does the system protect developer wall time **and** maintenance/cognitive cost?
23. Are TS-6 and TS-7 complete before V0.4 so V0.4 remains qualification-only?
24. What must be measured before declaring the new test system better than the current one?

Until these are accepted, broad current-test migration must not begin.

The target is not a smaller test count. The target is **stronger product evidence per unit of development time and maintenance effort**.
