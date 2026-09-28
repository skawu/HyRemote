# HyRemote Product Evidence Requirements

Status: **TS-0 canonical ER catalog — no existing test/CI migration is authorized by this document alone**

Authority: #393

Parent strategy: [`test-strategy.md`](test-strategy.md)

Detailed system design: [`test-system-architecture.md`](test-system-architecture.md)

This document freezes the product-level **Evidence Requirements (ERs)** that HyRemote must be able to prove. TS-1 audits the current test inventory **against this catalog**; current CTest identities do not define the catalog.

An ER is a stable product property/failure meaning, not a test executable, case, CI job, platform row or viewer version. One ER may have many cases/cells. One suite may prove several ERs. Adding another OS, Qt anchor, viewer or negative input normally expands a cell/fixture/case — not the ER count.

The objective is the smallest stable requirement set that still expresses HyRemote's product promise.

---

## 1. Field model

Each ER defines:

- **PAC / Risk** — owning product contract and risk domain;
- **Activation** — when the ER exists as a product obligation;
- **Evidence obligation** — which evidence classes are required in which decision context;
- **Statement** — product property to prove;
- **Oracle** — explicit PASS/FAIL condition;
- **Invalidation** — material changes that make retained evidence inapplicable to a newer claim.

### 1.1 Activation vocabulary

| Value | Meaning |
| --- | --- |
| `INVARIANT` | Fundamental whenever the owning capability/product path exists. |
| `CLAIM` | Required when a positive product/support claim depends on it. |
| `CAPABILITY` | Required when the named optional/conditional capability is actually implemented/available in the artifact or is positively claimed for the cell. An unavailable-capability request does **not** activate a success-path ER; its rejection/fallback behavior belongs to the applicable fail-closed ER. |
| `MAINTENANCE` | Required only for an update/rollback edge explicitly admitted by product/version authority. |
| `RELEASE` | Required when exact-candidate release authority names it. |

Multiple activation tokens separated by `/` are logical OR: the ER becomes applicable if any listed activation condition is true. Activation never means "run on every commit"; execution frequency is controlled by change risk and gates.

### 1.2 Evidence classes

| Class | Meaning |
| --- | --- |
| `V` | deterministic Verification |
| `P` | real Product Validation path |
| `Q` | environment/cell Qualification |
| `R` | exact-candidate Release Acceptance |

These classes are independent evidence dimensions, not a scalar strength ladder. A physical Q result cannot replace a deterministic state-machine V oracle, and V cannot replace native/loader Q evidence.

### 1.3 Evidence-obligation grammar

The catalog deliberately avoids ambiguous expressions such as `V+P(+Q/R)`. Every row uses explicit context clauses:

- `base X` — X is the minimum retained evidence family while the ER's activation condition is true;
- `affected X` — X is required only for an affected-path decision; `affected +X` means add X to the standing `base` obligation;
- `claim X` — X is required only when the ER is being used to support a positive environment/product claim and there is no standing requirement for X outside that context;
- `claim +X` — when a positive claim is being supported, add X to the standing `base` obligation;
- `release X` — X is required only when canonical release authority selects the ER for exact-candidate acceptance and there is no standing requirement for X outside that context;
- `release +X` — when release authority selects the ER, add X to the already-required base/claim obligations.

The clauses are cumulative. For example:

```text
base V+P; claim +Q; release +R
```

means V and P are standing evidence families, Q becomes additionally mandatory for a positive support/qualification claim, and R becomes additionally mandatory only when release authority selects the ER for exact-candidate acceptance.

By contrast:

```text
claim Q; release +R
```

means the ER has no standing V/P family outside a claim; Q is mandatory whenever the positive claim exists, and R is additionally mandatory for exact-candidate release acceptance.

A missing conditional class is **not** a gap unless its trigger is active. A gate name does not activate a class by itself: a G4 hosted run does not become Q unless the ER's claim context, environment and oracle require/qualify it.

### 1.4 ER design rule

Do **not** create a new ER merely because a platform, Qt version, frontend, viewer, gate or input variant differs. Create a new ER only when the product statement/failure meaning is materially different.

### 1.5 Current listener-authority resolution

The current IPv4 listener product authority is #174. It explicitly states that the former loopback default is **superseded** and freezes the current default as `0.0.0.0:5921`, with exact-address and interface binding as narrowing modes.

Therefore, until a later product decision supersedes #174:

- ER/test oracles use #174 for listener-default/binding semantics;
- older repository text or historical tests that still require loopback are **authority drift to report**, not an alternate valid oracle;
- TS-1 must not choose product truth from whichever existing test currently passes;
- stale security prose must be reconciled by its owning product/doc work, but it does not redefine the listener ER.

This is evidence-system clarification of an explicit supersession, not a new listener product decision.

### 1.6 Authoritative claim-obligation sets

A positive compatibility/support claim must not derive its required ERs from PAC membership, risk membership, current tests or whichever suites happen to exist. Those are reverse/index relationships and cannot prove completeness.

The reusable **Claim Obligation Profile** is supplied by the owning product/compatibility authority, while the authoritative forward lookup is **atomic-cell specific**: `(claim_id, cell_id) -> profile_id`. `claim_id` is an authority row/family identity and may own multiple atomic cells; `cell_id` is the indivisible evidence boundary. Cells under one claim may reuse the same profile or intentionally select different profiles. A profile is upstream product data and contains at minimum:

```yaml
profile_id: <stable authority-owned id>
authority: <document/issue/version that owns the claim>
claim_scope: <product/frontend/deployment/support family>
cell_schema: CCS-1
required_dimensions: [<registered CCS-1 dimension names>]
base_er: [<canonical product ER ids>]
conditional_er:
  - when: <predicate over declared material cell dimensions/capabilities>
    require: [<canonical product ER ids>]
```

#### Claim-cell schema `CCS-1`

All `CCS-1` dimension values are non-empty, case-sensitive UTF-8 strings. A dimension omitted from a cell is **not material to that claim** and cannot be referenced by that profile's predicates. A profile lists every dimension it requires; a missing required dimension is incomplete/fail-closed.

| Dimension | Meaning | Current normalized values/examples |
| --- | --- | --- |
| `product_line` | product claim line | `V0.2` |
| `qt` | authority-normalized Qt anchor | `6.8.3`, `6.8.3-exact` |
| `os_arch` | OS/architecture qualification family | `Windows x86_64`, `Linux x86_64` |
| `integration` | application integration route | `C++ API`, `QML API`, `Generic Plugin`, `QPA` |
| `ui_target` | atomic UI target | `Widgets`, `Qt Quick` |
| `graphics_scope` | graphics claim scope | `portable-baseline` |
| `deployment_form` | deployment contract/form | `hyremote_deploy` |
| `native_platform` | native Qt platform/delegate when material | `qwindows`, `qxcb` |
| `transport` | transport claim | `RFB-3.8` |
| `network_profile` | named network/exposure qualification profile | `default-trusted-lan` |
| `remote_input_policy` | declared view/control policy | `optional-control-default-view-only` |
| `security_profile` | product security profile | `Insecure`, `Authenticated`, `AuthenticatedEncrypted` |
| `transport_security` | transport-security build capability state when material | `available`, `unavailable` |
| `viewer` | pinned viewer identity when a named-viewer claim exists | authority-owned pinned string |

`CCS-1` contains only the dimension names above. A new dimension name or incompatible value-type/semantic change requires an explicitly versioned successor schema; TS-2 must not accept an unknown key by guessing its meaning.

Every positive atomic cell also has an immutable **cell-definition revision**. `claim_cell_revision` content-addresses the normalized authority-owned target cell definition: exact `claim_id`, `cell_id`, `cell_schema`, authority `claim_status` when present, and the canonical key-sorted `material_dimensions` map. A claim-bound record retains that revision plus the exact `target_material_dimensions` snapshot. `claim_profile_revision` remains independent and covers the resolved profile inheritance graph plus the exact `(claim_id, cell_id) -> profile_id` binding. Both revisions must resolve and reconcile with their retained snapshots; a material cell-definition change therefore creates a new evidence identity even when the profile binding is unchanged.

#### CCS-1 material-evidence binding

**Every evidence record consumed by positive claim completeness is claim-bound, regardless of whether its evidence class is V, P, Q or R.** Ordinary V/P developer/PR results that are not used to support a product/support claim do not need claim-cell bookkeeping. But a V/P result cannot become claim evidence merely because the same ER passed somewhere else: once consumed for a claim it must reference immutable execution/observation facts and satisfy the same cell binding rules as Q/R evidence.

For a claim-bound record, every material dimension declared by its resolved cell must be joined to an immutable fact from the referenced Execution Manifest / Observation Context and the canonical observed value must match the cell value. Record fields never create those facts.

The current `CCS-1` dimension resolvers are:

| Dimension | Canonical evidence fact |
| --- | --- |
| `product_line` | Execution Manifest candidate/artifact product-line identity, normalized from authoritative build/version metadata. |
| `qt` | Execution Manifest exact Qt identity, normalized to the cell's registered Qt anchor. Current `6.8.3` and `6.8.3-exact` anchors both require an actually observed Qt 6.8.3 identity; the `-exact` form is valid only for the exact-QPA claim/profile whose product ERs require exact private-ABI evidence. |
| `os_arch` | Execution Manifest OS + CPU identity normalized to the registered `os_arch` value. |
| `integration` | Observation Context integration route actually exercised. |
| `ui_target` | Observation Context atomic UI target actually exercised. |
| `graphics_scope` | Observation Context graphics/backend scope actually exercised. |
| `deployment_form` | Observation Context deployment form/path actually exercised. |
| `native_platform` | Execution Manifest / Observation Context effective Qt native platform/delegate identity. |
| `transport` | Observation Context transport actually exercised. |
| `network_profile` | Observation Context controlled network/exposure profile actually exercised. |
| `remote_input_policy` | Observation Context remote-input policy scenario actually exercised. |
| `security_profile` | Observation Context requested/selected security profile actually exercised; a fail-closed negative cell records the requested profile even when no session is established. |
| `transport_security` | Execution Manifest artifact capability state; capability-on/off requires the corresponding artifact fact and cannot be relabelled per record. |
| `viewer` | Observation Context pinned viewer identity actually exercised. |

A claim-bound record that proves one or more product ERs must explicitly bind each proved ER to the Observation Context(s) whose observations support that ER, conceptually:

```yaml
er_observation_bindings:
  ER-SOME-PROPERTY:
    observation_context_ids: [ctx-...]
    observation_artifact_refs: [artifact-or-measurement-ref, ...]
```

`observation_context_ids` at record level, if serialized, is only the sorted unique union of all per-ER bindings. It is not an alternate source-selection mechanism.

For each proved ER independently:

1. every bound context must belong to the record's immutable Execution Manifest;
2. every observation/artifact used for that ER must be reachable from one of that ER's bound contexts;
3. for every material phase-varying CCS-1 dimension, **all contexts contributing to that ER must resolve to the same normalized value**;
4. that normalized value, together with execution-wide Manifest-derived dimensions, must exactly equal the resolved cell's material dimensions.

If an ER genuinely needs several phases (for example an enabled/disabled policy scenario), those contexts must still normalize to the same cell-level material scenario value. If contributing contexts disagree on a material cell dimension, the record is BLOCKED/invalid for that ER/cell; the evaluator may not choose whichever context value happens to match. The execution must instead use a correctly normalized common scenario dimension, split the evidence/record, or use a different explicit claim cell if product authority actually intends distinct claims.

The evidence framework may materialize a normalized `observed_material_dimensions` map for convenience, but it must derive that map from the sources above and retain the source linkage. For every material dimension in the cell, comparison is exact after canonical `CCS-1` normalization. A missing source fact, an unknown normalization, a disagreement across an ER's contributing contexts, or a mismatch between the observed fact and cell value makes that claim-used record **BLOCKED/invalid for that ER/cell**. A record cannot satisfy an `Authenticated` cell from an `Insecure` context, a capability-on cell from a capability-off artifact, or any other cell merely by copying the cell's value into record metadata.

The TS-0 predicate language is intentionally small and deterministic:

- `dimension == "scalar"` — exact case-sensitive string equality against the normalized cell value;
- `dimension in ["scalar", ...]` — exact case-sensitive membership in a finite declared string set;
- multiple atomic conditions in one `when` object are logical **AND**;
- separate `conditional_er` entries are evaluated independently and every matching `require` set is unioned;
- a predicate may reference only a `CCS-1` dimension declared in that profile's inherited/own `required_dimensions`;
- no negation, regex, arbitrary code or implicit version-range comparison exists in the profile language. A range/family rule must first be normalized by its owning authority into explicit cell data/rules before profile expansion.

Rules:

1. Every positive support/compatibility **atomic cell** must have exactly one applicable authority-owned `(claim_id, cell_id) -> profile_id` binding before it can be considered evidence-complete. `claim_id` alone is never a profile lookup key.
2. `required_product_ERs(cell)` is the union of the selected profile's inherited/`base_er`, every matching `conditional_er.require`, and any maintenance/release **product** obligations explicitly selected by their owning authorities.
3. `required_dimensions` is inherited by union; child profiles may add dimensions but may not silently remove inherited dimensions.
4. PAC/risk membership may index or explain an ER but must never silently add, remove or substitute a required product ER.
5. The test registry, selectors and suites consume the expanded required product set; they cannot amend it to fit the available tests.
6. Unknown profile IDs, unknown schema/dimensions, missing required dimensions, malformed predicates, missing/ambiguous `(claim_id, cell_id)` bindings, missing/unresolvable `claim_cell_revision`, missing or mismatched retained `target_material_dimensions`, or positive cells for which no profile applies are **incomplete/fail-closed**, never implicit PASS.
7. Profiles may inherit/reuse another authority-owned profile to avoid per-cell duplication, but the final expanded product ER set must be deterministic and inspectable.
8. TS-2 may implement storage/parsing for this relation, but it does not invent the relation. Product/compatibility authority owns the exact `(claim_id, cell_id) -> profile_id` binding; this catalog freezes the current bindings those authorities expose. Multiple cells under one `claim_id` may resolve to the same or different profiles.
9. A material profile/schema/conditional-rule/cell-specific binding **or normalized target-cell definition/material-dimension** change invalidates prior claim-completeness decisions that depended on the older profile/binding or cell revision.
10. A positive claim family is not allowed to defer any currently required atomic cell's concrete profile binding to TS-2: TS-0 must contain a mechanically resolvable `(claim_id, cell_id) -> profile_id` binding for every positive atomic cell it declares frozen.
11. A claim/cell identity is stable evidence metadata. Renaming/rekeying one without an authority-declared identity migration creates a new evidence identity; old records remain historical and cannot silently satisfy the new identity.
12. **Claim-meta ERs never appear in `base_er` or `conditional_er.require`.** Profiles describe product obligations only; claim composition/non-inference/binding rules are evaluated outside the product ER expansion.
13. A V/P/Q/R result that is not claim-bound may remain useful verification/validation evidence, but it cannot be counted toward a positive claim's completeness until a valid claim-bound record exists for the exact cell and ER.
14. Atomic cell completeness is evaluated independently. When one authority-level `claim_id` owns multiple positive cells, claim-level aggregation occurs only after cell completeness; for the current frozen bindings all bound positive atomic cells are required unless the owning authority explicitly defines another aggregation rule. One cell's PASS never substitutes for another cell.

#### Claim-meta evaluation is outside profile expansion

The following PAC-9 requirements are **claim-meta ERs**, not members of `required_product_ERs(cell)`:

- `ER-COMPAT-EXPLICIT-CELL`;
- `ER-COMPAT-EVIDENCE-BINDING`;
- `ER-COMPAT-PLATFORM-INDEPENDENCE-TRUTH`;
- `ER-COMPAT-GRAPHICS-NO-INFERENCE`.

This separation is deliberate. It prevents both direct self-recursion and cross-cell/cross-claim completeness cycles.

For an active claim/cell, the completeness engine performs these outer checks after the profile has expanded:

```text
META-CELL:
  validate claim_id/cell_id/schema/status/material dimensions
  resolve the normalized authority-owned target cell to exactly one immutable claim_cell_revision
  require the retained target_material_dimensions snapshot to equal that revision's canonical material_dimensions exactly
  resolve exactly one (claim_id, cell_id) -> profile_id binding + immutable claim_profile_revision
  validate required_product_ERs(cell) is deterministic and inspectable

META-BINDING:
  for every V/P/Q/R record consumed by this cell, require exact claim_id/cell_id/schema + claim_profile_revision + claim_cell_revision
  require the record's target_material_dimensions to equal the retained cell revision's canonical material_dimensions exactly
  resolve execution_id + execution_manifest_digest to one immutable retained manifest
  require every ER proved by the record to have an explicit ER -> observation-context/observation binding
  require every ER-bound observation_context_id to belong to that manifest
  require every ER-bound raw/observation artifact reference to be reachable from one of that ER's bound contexts
  require record-level candidate/artifact/environment/fixture facts to equal their manifest/context sources
  for each proved ER and every material phase-varying CCS-1 dimension, require all of that ER's contributing contexts to normalize to one value
  require every material CCS-1 dimension's canonical observed value to equal the retained target_material_dimensions value
  validate exact claim/cell/profile binding revision, cell-definition revision, invalidation and exact-candidate compatibility

META-PLATFORM:
  evidence from another os_arch cannot satisfy this cell
  a passing/existing cell on one platform cannot synthesize a claim/cell on another platform

META-GRAPHICS-SCOPE:
  portable-baseline evidence cannot synthesize a specialized graphics/native-surface claim
  a specialized positive claim must exist explicitly with its own profile/cell and targeted product obligations

PRODUCT-EVIDENCE:
  satisfy every triggered V/P/Q/R evidence obligation for every ER in required_product_ERs(cell) using only claim-bound records that passed META-BINDING for this cell/ER

cell_complete = META-CELL PASS
             && META-BINDING PASS
             && META-PLATFORM PASS
             && META-GRAPHICS-SCOPE PASS
             && PRODUCT-EVIDENCE complete

claim_complete = authority aggregation over its bound positive atomic cells
```

`META-BINDING` is fail-closed: an unresolved manifest/digest, missing per-ER observation binding, foreign or missing Observation Context, unreachable observation reference, record/manifest/context disagreement, disagreement among an ER's contributing contexts on a material dimension, missing material-dimension source fact, material-dimension mismatch, missing/unresolvable or mismatched `claim_cell_revision`, `target_material_dimensions` disagreement with the retained canonical cell, or missing/ambiguous cell-specific profile binding is **BLOCKED/invalid evidence**, not a record that can be repaired by relabelling metadata.

`META-PLATFORM` does **not** require another platform cell to PASS before this cell can PASS. It enforces exact platform binding/non-substitution. Whether the public product authority chooses to publish independent Windows and Linux claims is upstream product data. This avoids the cross-cell cycle where Windows would depend on Linux and Linux would depend back on Windows.

Likewise, `META-GRAPHICS-SCOPE` checks claim boundaries and non-inference; it does not require an unrelated specialized graphics claim to exist or pass.

The completeness engine may emit derived Q/R evidence records for these claim-meta ERs, but those records are outputs of evaluating the product ER set and its bindings; they are never inputs to the product-set expansion and never require themselves in order to evaluate themselves.

This gives TS-6 a non-recursive forward chain:

```text
positive claim/status authority
 -> atomic (claim_id, cell_id)
 -> authority-owned profile binding
 -> immutable target-cell definition/revision + material dimensions
 -> expanded required_product_ERs(cell)
 -> required evidence classes + valid claim-bound product evidence records
 -> outer claim-meta checks
 -> atomic cell completeness
 -> authority-level claim aggregation when applicable
```

### 1.7 Frozen V0.2 desktop integration profiles and bindings

`docs/compatibility.md` remains the authority for the current V0.2 compatibility rows and their opaque `Supported` / `Limited` status values. This section freezes the **evidence-obligation projection** of those already-declared rows; it does not broaden their product scope or invent support for absent dimensions.

All cells in this section use `cell_schema=CCS-1`, `product_line=V0.2`, `graphics_scope=portable-baseline`, and `deployment_form=hyremote_deploy`. Public-Qt rows normalize `qt=6.8.3`; QPA rows normalize `qt=6.8.3-exact` and additionally bind the qualified `native_platform` (`qwindows` or `qxcb`). A source row written as `Widgets / Quick` expands to two atomic qualification cells, one `Widgets` and one `Qt Quick`, so one half cannot silently substitute for the other.

Reusable common profile:

```yaml
profile_id: COP-V020-DESKTOP-BASE
authority: docs/compatibility.md current V0.2 reference matrix + deployment compatibility; #393 evidence mapping
claim_scope: current V0.2 desktop integration-row evidence
cell_schema: CCS-1
abstract: true
required_dimensions: [product_line, qt, os_arch, integration, ui_target, graphics_scope, deployment_form]
base_er:
  - ER-INTEGRATE-ONE-DEPLOY-ENTRY
  - ER-INTEGRATE-NO-BACKEND-TUNING
  - ER-NATIVE-LOCAL-DISPLAY
  - ER-NATIVE-LOCAL-INPUT
  - ER-NATIVE-STOP-SURVIVAL
  - ER-REMOTE-VIEW-CONTENT
  - ER-REMOTE-SURFACE-LIFECYCLE
  - ER-FRONTEND-ONE-SHARED-RUNTIME
  - ER-FRONTEND-UNIQUE-MAPPING
  - ER-PACKAGE-CLEAN-CONSUMER
  - ER-DEPLOY-ARTIFACT-CLOSURE
  - ER-DEPLOY-ISOLATED-LAUNCH
  - ER-DEPLOY-RELOCATION
  - ER-DEPLOY-RUNTIME-ORIGIN
conditional_er:
  - when: os_arch == "Linux x86_64"
    require: [ER-DEPLOY-FOREIGN-QT-ISOLATION-LINUX]
```

Public-Qt frontend profile:

```yaml
profile_id: COP-V020-DESKTOP-PUBLIC-QT
authority: docs/compatibility.md current V0.2 C++/QML/Generic rows; #393 evidence mapping
claim_scope: current V0.2 C++ API, QML API and Generic Plugin rows
cell_schema: CCS-1
inherits: COP-V020-DESKTOP-BASE
required_dimensions: []
base_er: []
conditional_er:
  - when: integration in ["C++ API", "QML API"]
    require: [ER-INTEGRATE-PUBLIC-BOUNDARY]
  - when: integration == "Generic Plugin"
    require: [ER-INTEGRATE-ZEROCODE-QT-ONLY, ER-GENERIC-NATIVE-PRESERVATION]
```

Exact-QPA profile:

```yaml
profile_id: COP-V020-DESKTOP-QPA-EXACT
authority: docs/compatibility.md current V0.2 QPA Limited rows; #57/#343 QPA qualification policy; #393 evidence mapping
claim_scope: current V0.2 QPA exact-Qt desktop rows
cell_schema: CCS-1
inherits: COP-V020-DESKTOP-BASE
required_dimensions: [native_platform]
base_er:
  - ER-INTEGRATE-ZEROCODE-QT-ONLY
  - ER-QPA-NATIVE-DELEGATE-EXACTNESS
  - ER-COMPAT-QPA-EXACT-ABI
conditional_er: []
```

The current positive matrix rows normalize to the following stable claim/cell identities. **Each table row is an authoritative `(claim_id, cell_id) -> profile_id` binding.** For a source row that already names one UI target, `claim_id == cell_id`; a combined `Widgets / Quick` source row has one `claim_id` and two atomic `cell_id` values.

| `claim_id` | `cell_id` | Qt | Platform | Integration | Atomic UI target | Status | `profile_id` |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `COMPAT-V020-QT683-WIN-CPP-WIDGETS` | `COMPAT-V020-QT683-WIN-CPP-WIDGETS` | 6.8.3 | Windows x86_64 | C++ API | Widgets | Supported | `COP-V020-DESKTOP-PUBLIC-QT` |
| `COMPAT-V020-QT683-WIN-CPP-QUICK` | `COMPAT-V020-QT683-WIN-CPP-QUICK` | 6.8.3 | Windows x86_64 | C++ API | Qt Quick | Supported | `COP-V020-DESKTOP-PUBLIC-QT` |
| `COMPAT-V020-QT683-LINUX-CPP-WIDGETS` | `COMPAT-V020-QT683-LINUX-CPP-WIDGETS` | 6.8.3 | Linux x86_64 | C++ API | Widgets | Supported | `COP-V020-DESKTOP-PUBLIC-QT` |
| `COMPAT-V020-QT683-LINUX-CPP-QUICK` | `COMPAT-V020-QT683-LINUX-CPP-QUICK` | 6.8.3 | Linux x86_64 | C++ API | Qt Quick | Supported | `COP-V020-DESKTOP-PUBLIC-QT` |
| `COMPAT-V020-QT683-WIN-GENERIC` | `COMPAT-V020-QT683-WIN-GENERIC-WIDGETS` | 6.8.3 | Windows x86_64 | Generic Plugin | Widgets | Supported | `COP-V020-DESKTOP-PUBLIC-QT` |
| `COMPAT-V020-QT683-WIN-GENERIC` | `COMPAT-V020-QT683-WIN-GENERIC-QUICK` | 6.8.3 | Windows x86_64 | Generic Plugin | Qt Quick | Supported | `COP-V020-DESKTOP-PUBLIC-QT` |
| `COMPAT-V020-QT683-LINUX-GENERIC` | `COMPAT-V020-QT683-LINUX-GENERIC-WIDGETS` | 6.8.3 | Linux x86_64 | Generic Plugin | Widgets | Supported | `COP-V020-DESKTOP-PUBLIC-QT` |
| `COMPAT-V020-QT683-LINUX-GENERIC` | `COMPAT-V020-QT683-LINUX-GENERIC-QUICK` | 6.8.3 | Linux x86_64 | Generic Plugin | Qt Quick | Supported | `COP-V020-DESKTOP-PUBLIC-QT` |
| `COMPAT-V020-QT683-WIN-QML-QUICK` | `COMPAT-V020-QT683-WIN-QML-QUICK` | 6.8.3 | Windows x86_64 | QML API | Qt Quick | Supported | `COP-V020-DESKTOP-PUBLIC-QT` |
| `COMPAT-V020-QT683-LINUX-QML-QUICK` | `COMPAT-V020-QT683-LINUX-QML-QUICK` | 6.8.3 | Linux x86_64 | QML API | Qt Quick | Supported | `COP-V020-DESKTOP-PUBLIC-QT` |
| `COMPAT-V020-QT683-WIN-QPA` | `COMPAT-V020-QT683-WIN-QPA-WIDGETS` | 6.8.3 exact | Windows x86_64 | QPA | Widgets | Limited | `COP-V020-DESKTOP-QPA-EXACT` |
| `COMPAT-V020-QT683-WIN-QPA` | `COMPAT-V020-QT683-WIN-QPA-QUICK` | 6.8.3 exact | Windows x86_64 | QPA | Qt Quick | Limited | `COP-V020-DESKTOP-QPA-EXACT` |
| `COMPAT-V020-QT683-LINUX-QPA` | `COMPAT-V020-QT683-LINUX-QPA-WIDGETS` | 6.8.3 exact | Linux x86_64 | QPA | Widgets | Limited | `COP-V020-DESKTOP-QPA-EXACT` |
| `COMPAT-V020-QT683-LINUX-QPA` | `COMPAT-V020-QT683-LINUX-QPA-QUICK` | 6.8.3 exact | Linux x86_64 | QPA | Qt Quick | Limited | `COP-V020-DESKTOP-QPA-EXACT` |

For the QPA rows above, Windows cells also carry `native_platform=qwindows` and Linux cells carry `native_platform=qxcb`.

`TODO` and `Unsupported` rows are not positive support claims and therefore do not become positive evidence-completeness obligations merely by existing in the compatibility document. Any future positive row/family must receive an authority-owned cell-specific profile binding plus stable claim/cell identity before it can be considered evidence-complete.

### 1.8 Frozen current transport, input and security claim families

The current compatibility/security authorities already make product statements outside the integration table. They are not future claims: `docs/compatibility.md` declares bounded RFB 3.8 remote viewing, default LAN-capable listener behavior, reconnect, optional remote input, `Insecure`, conditional `Authenticated`, no stream encryption, and `AuthenticatedEncrypted` fail-closed; `docs/security.md` defines the current effective security/input defaults and capability conditions; #174 owns the current listener binding semantics.

These claim families stay **orthogonal** to frontend integration rows. They use representative/shared-Runtime qualification cells rather than multiplying every security/input case by every frontend. Frontend equivalence remains owned by the integration profiles above. Every row in the cell tables below is likewise an authoritative `(claim_id, cell_id) -> profile_id` binding.

#### Bounded RFB 3.8 / listener / reconnect

```yaml
profile_id: COP-V020-RFB38
authority: docs/compatibility.md Transport and viewer boundary; docs/security.md Resource boundaries; #174 listener contract
claim_scope: bounded RFB 3.8 viewing, default trusted-LAN listener semantics and reconnect
cell_schema: CCS-1
required_dimensions: [product_line, qt, os_arch, transport, network_profile]
base_er:
  - ER-REMOTE-VIEW-CONTENT
  - ER-REMOTE-RECONNECT
  - ER-TRANSPORT-RFB-SEMANTICS
  - ER-NETWORK-LISTENER-BINDING
  - ER-RELIABILITY-SLOW-CLIENT-ISOLATION
  - ER-RELIABILITY-MALFORMED-INPUT-BOUNDS
  - ER-SECURITY-ADMISSION-RESOURCE-BOUNDS
conditional_er: []
```

Cells use `product_line=V0.2`, `qt=6.8.3`, `transport=RFB-3.8`, `network_profile=default-trusted-lan`:

| `claim_id` | `cell_id` | `os_arch` | `profile_id` |
| --- | --- | --- | --- |
| `COMPAT-V020-RFB38` | `COMPAT-V020-RFB38-WIN` | Windows x86_64 | `COP-V020-RFB38` |
| `COMPAT-V020-RFB38` | `COMPAT-V020-RFB38-LINUX` | Linux x86_64 | `COP-V020-RFB38` |

This is a protocol/product claim, not a named-viewer support claim. A viewer fixture used to exercise Q evidence is recorded as a fixture; it does not silently create a broader viewer compatibility status.

#### Optional remote input / default view-only policy

```yaml
profile_id: COP-V020-REMOTE-INPUT
authority: docs/compatibility.md Input compatibility + Transport and viewer boundary; docs/security.md Remote viewing versus remote control
claim_scope: optional remote control with remote input disabled by default and correct held-state cleanup
cell_schema: CCS-1
required_dimensions: [product_line, qt, os_arch, transport, remote_input_policy]
base_er:
  - ER-REMOTE-INPUT-SEMANTICS
  - ER-REMOTE-INPUT-NEUTRALITY
  - ER-SECURITY-SAFE-DEFAULTS
conditional_er: []
```

Cells use `product_line=V0.2`, `qt=6.8.3`, `transport=RFB-3.8`, `remote_input_policy=optional-control-default-view-only`:

| `claim_id` | `cell_id` | `os_arch` | `profile_id` |
| --- | --- | --- | --- |
| `COMPAT-V020-REMOTE-INPUT` | `COMPAT-V020-REMOTE-INPUT-WIN` | Windows x86_64 | `COP-V020-REMOTE-INPUT` |
| `COMPAT-V020-REMOTE-INPUT` | `COMPAT-V020-REMOTE-INPUT-LINUX` | Linux x86_64 | `COP-V020-REMOTE-INPUT` |

The ER oracle itself exercises enabled delivery **and** view-only rejection/lifecycle policy; these are cases inside the cell, not separate product support rows. Their contexts normalize to the same `remote_input_policy=optional-control-default-view-only` scenario value for claim binding.

#### `Insecure` profile semantics

```yaml
profile_id: COP-V020-SECURITY-INSECURE
authority: docs/compatibility.md Transport and viewer boundary; docs/security.md Insecure/current capability matrix; #174 listener contract
claim_scope: Insecure RFB behavior is unauthenticated and unencrypted, explicitly trusted-LAN only, with truthful defaults/exposure
cell_schema: CCS-1
required_dimensions: [product_line, qt, os_arch, transport, security_profile]
base_er:
  - ER-SECURITY-SAFE-DEFAULTS
  - ER-TRANSPORT-RFB-SEMANTICS
  - ER-NETWORK-LISTENER-BINDING
conditional_er: []
```

Cells use `product_line=V0.2`, `qt=6.8.3`, `transport=RFB-3.8`, `security_profile=Insecure`:

| `claim_id` | `cell_id` | `os_arch` | `profile_id` |
| --- | --- | --- | --- |
| `COMPAT-V020-SECURITY-INSECURE` | `COMPAT-V020-SECURITY-INSECURE-WIN` | Windows x86_64 | `COP-V020-SECURITY-INSECURE` |
| `COMPAT-V020-SECURITY-INSECURE` | `COMPAT-V020-SECURITY-INSECURE-LINUX` | Linux x86_64 | `COP-V020-SECURITY-INSECURE` |

The listener oracle uses #174 (`0.0.0.0:5921` default plus exact narrowing); any historical “loopback-only” prose remains authority drift and is not imported into this profile.

#### `Authenticated` when transport-security capability is available

```yaml
profile_id: COP-V020-SECURITY-AUTH-AVAILABLE
authority: docs/compatibility.md conditional authentication statement; docs/security.md Authenticated
claim_scope: RFB VNC authentication in a transport-security-enabled artifact with valid descriptor, still unencrypted
cell_schema: CCS-1
required_dimensions: [product_line, qt, os_arch, transport, security_profile, transport_security]
base_er:
  - ER-SECURITY-AUTH-MECHANISM
  - ER-SECURITY-SECRET-HYGIENE
  - ER-TRANSPORT-RFB-SEMANTICS
conditional_er: []
```

Cells use `product_line=V0.2`, `qt=6.8.3`, `transport=RFB-3.8`, `security_profile=Authenticated`, `transport_security=available`:

| `claim_id` | `cell_id` | `os_arch` | `profile_id` |
| --- | --- | --- | --- |
| `COMPAT-V020-SECURITY-AUTH-AVAILABLE` | `COMPAT-V020-SECURITY-AUTH-AVAILABLE-WIN` | Windows x86_64 | `COP-V020-SECURITY-AUTH-AVAILABLE` |
| `COMPAT-V020-SECURITY-AUTH-AVAILABLE` | `COMPAT-V020-SECURITY-AUTH-AVAILABLE-LINUX` | Linux x86_64 | `COP-V020-SECURITY-AUTH-AVAILABLE` |

Valid credentials plus wrong/missing credentials are mechanism cases inside these transport-bound cells: RFB 3.8 authentication negotiation is actually exercised, correct credentials establish the session, wrong/missing credentials are rejected, and no weaker downgrade is accepted.

#### `Authenticated` with capability available but descriptor invalid (pre-session fail closed)

```yaml
profile_id: COP-V020-SECURITY-AUTH-CONFIG-INVALID
authority: docs/compatibility.md conditional authentication statement; docs/security.md Authenticated/current fail-closed configuration policy
claim_scope: requesting Authenticated on a transport-security-enabled artifact with a missing/invalid security descriptor fails closed before listener/session creation
cell_schema: CCS-1
required_dimensions: [product_line, qt, os_arch, security_profile, transport_security]
base_er:
  - ER-SECURITY-FAIL-CLOSED
  - ER-SECURITY-SECRET-HYGIENE
conditional_er: []
```

These cells stay in the same authority-level `COMPAT-V020-SECURITY-AUTH-AVAILABLE` claim family but deliberately have their own atomic identities and their own `(claim_id, cell_id) -> COP-V020-SECURITY-AUTH-CONFIG-INVALID` bindings. They use `product_line=V0.2`, `qt=6.8.3`, `security_profile=Authenticated`, `transport_security=available`:

| `claim_id` | `cell_id` | `os_arch` | `profile_id` |
| --- | --- | --- | --- |
| `COMPAT-V020-SECURITY-AUTH-AVAILABLE` | `COMPAT-V020-SECURITY-AUTH-AVAILABLE-WIN-DESCRIPTOR-INVALID` | Windows x86_64 | `COP-V020-SECURITY-AUTH-CONFIG-INVALID` |
| `COMPAT-V020-SECURITY-AUTH-AVAILABLE` | `COMPAT-V020-SECURITY-AUTH-AVAILABLE-LINUX-DESCRIPTOR-INVALID` | Linux x86_64 | `COP-V020-SECURITY-AUTH-CONFIG-INVALID` |

Missing/invalid descriptor contents are fixture cases, not a new public compatibility status or CCS-1 dimension. Because these cases must fail before a usable listener/session exists, `transport` is intentionally **not** material to these cells and no transport observation may be invented merely to satisfy binding.

#### `Authenticated` when the build capability is absent

```yaml
profile_id: COP-V020-SECURITY-AUTH-UNAVAILABLE
authority: docs/compatibility.md default-build warning; docs/security.md Authenticated/current capability matrix
claim_scope: requesting Authenticated without transport-security capability fails closed before listener/session creation
cell_schema: CCS-1
required_dimensions: [product_line, qt, os_arch, security_profile, transport_security]
base_er:
  - ER-SECURITY-FAIL-CLOSED
  - ER-SECURITY-SAFE-DEFAULTS
conditional_er: []
```

Cells use `product_line=V0.2`, `qt=6.8.3`, `security_profile=Authenticated`, `transport_security=unavailable`:

| `claim_id` | `cell_id` | `os_arch` | `profile_id` |
| --- | --- | --- | --- |
| `COMPAT-V020-SECURITY-AUTH-UNAVAILABLE` | `COMPAT-V020-SECURITY-AUTH-UNAVAILABLE-WIN` | Windows x86_64 | `COP-V020-SECURITY-AUTH-UNAVAILABLE` |
| `COMPAT-V020-SECURITY-AUTH-UNAVAILABLE` | `COMPAT-V020-SECURITY-AUTH-UNAVAILABLE-LINUX` | Linux x86_64 | `COP-V020-SECURITY-AUTH-UNAVAILABLE` |

This negative capability cell intentionally does **not** activate `ER-SECURITY-AUTH-MECHANISM`; its oracle is the fail-closed result. Because no transport session is established, `transport` is intentionally not a material dimension of this negative capability claim.

#### `AuthenticatedEncrypted` unavailable / no downgrade

```yaml
profile_id: COP-V020-SECURITY-AUTHENC-UNAVAILABLE
authority: docs/compatibility.md no-encryption/fail-closed statement; docs/security.md AuthenticatedEncrypted/current capability matrix
claim_scope: AuthenticatedEncrypted is unavailable in the current line and always fails closed before listener creation with no downgrade
cell_schema: CCS-1
required_dimensions: [product_line, qt, os_arch, security_profile, transport_security]
base_er:
  - ER-SECURITY-FAIL-CLOSED
  - ER-SECURITY-SAFE-DEFAULTS
conditional_er: []
```

Cells use `product_line=V0.2`, `qt=6.8.3`, `security_profile=AuthenticatedEncrypted`; both capability states are material because even an artifact capable of `Authenticated` must not downgrade the encrypted request:

| `claim_id` | `cell_id` | `os_arch` | `transport_security` | `profile_id` |
| --- | --- | --- | --- | --- |
| `COMPAT-V020-SECURITY-AUTHENC-UNAVAILABLE` | `COMPAT-V020-SECURITY-AUTHENC-UNAVAILABLE-WIN-CAP-ON` | Windows x86_64 | available | `COP-V020-SECURITY-AUTHENC-UNAVAILABLE` |
| `COMPAT-V020-SECURITY-AUTHENC-UNAVAILABLE` | `COMPAT-V020-SECURITY-AUTHENC-UNAVAILABLE-WIN-CAP-OFF` | Windows x86_64 | unavailable | `COP-V020-SECURITY-AUTHENC-UNAVAILABLE` |
| `COMPAT-V020-SECURITY-AUTHENC-UNAVAILABLE` | `COMPAT-V020-SECURITY-AUTHENC-UNAVAILABLE-LINUX-CAP-ON` | Linux x86_64 | available | `COP-V020-SECURITY-AUTHENC-UNAVAILABLE` |
| `COMPAT-V020-SECURITY-AUTHENC-UNAVAILABLE` | `COMPAT-V020-SECURITY-AUTHENC-UNAVAILABLE-LINUX-CAP-OFF` | Linux x86_64 | unavailable | `COP-V020-SECURITY-AUTHENC-UNAVAILABLE` |

These profiles bind claims the repository already makes. They do **not** create named-viewer support, Internet-safe deployment, encryption support, live per-client authorization, or any frontend-specific security personality.

---

## 2. PAC-1 — Low Intrusion

| ER | PAC / Risk | Activation | Evidence obligation | Statement | Oracle | Invalidation |
| --- | --- | --- | --- | --- | --- | --- |
| `ER-INTEGRATE-PUBLIC-BOUNDARY` | PAC-1 / R-FRONTEND,R-ADOPTION | INVARIANT | base V+P | Source-integrated applications need only documented public HyRemote surfaces, not Core/Runtime/transport/QPA internals. | Clean consumer builds/links using only documented public package/API; prohibited internal dependencies absent. | Material public API/export/header/package-boundary change. |
| `ER-INTEGRATE-ZEROCODE-QT-ONLY` | PAC-1,PAC-4 / R-FRONTEND,R-PACKAGE | CLAIM | base V+P; claim +Q | Generic/QPA zero-code applications remain Qt-only at application source/link level and activate through the declared Qt mechanism. | Qt-only consumer has no HyRemote source/link dependency and reaches Shared Runtime via declared plugin/platform activation. | Material Generic/QPA activation, packaging, app-link or claimed Qt/platform change. |
| `ER-INTEGRATE-ONE-DEPLOY-ENTRY` | PAC-1,PAC-5 / R-DEPLOY,R-ADOPTION | INVARIANT | base V+P | Supported consumers use the documented HyRemote deployment entry rather than manual internal payload knowledge. | Documented deployment succeeds without manual internal-file copy/discovery. | Deploy API/payload/public deployment-contract change. |
| `ER-INTEGRATE-NO-BACKEND-TUNING` | PAC-1,PAC-8 / R-FRONTEND,R-PERF | INVARIANT | base V+P | Ordinary integration does not require selecting internal capture/encoding/queue/GPU/vendor implementation controls. | Normal supported journey completes without backend-specific application configuration or leaked internal types. | Material public configuration/API/product-policy change exposing such control. |

---

## 3. PAC-2 — Native Non-interference

| ER | PAC / Risk | Activation | Evidence obligation | Statement | Oracle | Invalidation |
| --- | --- | --- | --- | --- | --- | --- |
| `ER-NATIVE-LOCAL-DISPLAY` | PAC-2 / R-NATIVE,R-CAPTURE | CLAIM | base V; claim +Q; release +R | HyRemote does not replace/hide/corrupt claimed native local display behavior. | Declared local rendering/window behavior matches native baseline while remote access is active. | Material frontend/native-delegate/capture/graphics/platform or cell change. |
| `ER-NATIVE-LOCAL-INPUT` | PAC-2,PAC-3 / R-NATIVE,R-INPUT | CLAIM | base V; claim +Q | Local pointer/keyboard/text/focus remains usable during remote access. | Local actions remain correctly delivered before/during/after remote sessions and are not blocked by remote state. | Material input routing/frontend/native/focus/text or cell change. |
| `ER-NATIVE-STOP-SURVIVAL` | PAC-2,PAC-10 / R-RUNTIME,R-NATIVE,R-OPERATE | INVARIANT | base V+P; claim +Q | Stopping/disabling HyRemote ceases remote effects and leaves the host application alive/usable. | Runtime reaches truthful stopped/disabled state; listener/input/capture effects cease; whole-session remote held state is neutral; no forbidden late effect occurs; host native operation continues. | Material Runtime teardown/input cleanup/frontend activation/native delegate/effective-state reporting change. |
| `ER-NATIVE-SLOW-REMOTE-ISOLATION` | PAC-2,PAC-6,PAC-8 / R-NATIVE,R-RELIABILITY,R-PERF | CLAIM | base V+P; claim +Q | Slow/stalled remote peers do not indefinitely block claimed local Qt UI/render behavior. | Remote backlog stays bounded and local responsiveness remains inside declared policy. | Scheduling/backpressure/transport/capture/native-graphics change. |
| `ER-NATIVE-GRAPHICS-TRUTH` | PAC-2,PAC-9 / R-NATIVE,R-COMPAT | CLAIM | claim Q; release +R | Specialized graphics/backend support is claimed only after targeted local+remote qualification. | Exact graphics cell satisfies its declared native+remote oracle; unsupported surfaces are classified, not inferred. | Material Qt/OS/GPU/driver/graphics/capture or claim change. |

---

## 4. PAC-3 — Remote Experience & Transport Semantics

| ER | PAC / Risk | Activation | Evidence obligation | Statement | Oracle | Invalidation |
| --- | --- | --- | --- | --- | --- | --- |
| `ER-REMOTE-VIEW-CONTENT` | PAC-3 / R-CAPTURE,R-RUNTIME,R-TRANSPORT | INVARIANT | base V+P; claim +Q | Remote view shows current supported application content with correct dimensions/scaling semantics. | Expected state/geometry/DPR is observed remotely within freshness policy. | Material capture/composition/DPR/update/viewer-interoperability change. |
| `ER-REMOTE-SURFACE-LIFECYCLE` | PAC-3 / R-RUNTIME,R-CAPTURE | CLAIM | base V+P; claim +Q | Claimed surface create/show/hide/resize/destroy and popup/dialog/multi-window semantics are represented correctly. | Declared transitions produce expected remote composition with no stale/destroyed surface. | Material discovery/composition/adapter/native-window change. |
| `ER-REMOTE-INPUT-SEMANTICS` | PAC-3,PAC-7 / R-INPUT,R-TRANSPORT,R-SECURITY | CAPABILITY | base V+P; claim +Q | Remote input obeys both normalized action semantics and the effective view/control policy. | With remote input enabled, each declared pointer/button/wheel/key/modifier/text action produces the expected target event/effect and no unrelated delivery. With remote input disabled/view-only, the same attempted actions are dropped before target delivery and are not queued for later enablement. Any policy change that the product contract restricts to stopped state is rejected/deferred according to that lifecycle rather than silently changing active-session authorization. | Material remote-input policy/default, transport input decoding, normalization, routing/admission, lifecycle transition or adapter change. |
| `ER-REMOTE-INPUT-NEUTRALITY` | PAC-3,PAC-6 / R-INPUT,R-RELIABILITY | CAPABILITY | base V+P | Remote held-state ownership is cleaned correctly across client/session terminal transitions. | Disconnect removes only the disconnecting viewer's held contribution; shared logical key/button stays held while another viewer still contributes; target becomes globally neutral after final holder leaves or whole-target/session teardown/stop/policy-disable destroys all contributions; forbidden late delivery is zero. | Material per-viewer input ownership, admission/mailbox, disconnect/teardown or target-lifecycle change. |
| `ER-REMOTE-RECONNECT` | PAC-3,PAC-6 / R-RUNTIME,R-TRANSPORT,R-NETWORK | INVARIANT | base V+P; claim +Q | Viewer reconnect works without application/Runtime reconstruction outside documented lifecycle. | Reconnect returns to usable remote state with no leaked prior-client state. | Material listener/client/session/transport lifecycle change. |
| `ER-TRANSPORT-RFB-SEMANTICS` | PAC-3,PAC-6,PAC-9 / R-TRANSPORT,R-COMPAT | CLAIM | base V+P; claim +Q; release +R | The declared bounded RFB 3.8 transport performs successful version/security negotiation, framing, update-request semantics, declared encoding/fallback/incremental behavior and maintained-viewer interoperability for the mechanisms actually claimed. | Deterministic protocol fixtures complete the expected handshake/framing/update state machine; each claimed encoding/extension/fallback path produces valid decodable framebuffer updates; maintained-viewer cells negotiate/use the declared interoperable behavior; unsupported mechanisms are not inferred from another passing path. | Material RFB version/security negotiation, parser/framing, encoding/update state, extension, viewer-compatibility or transport-support claim change. |

---

## 5. PAC-4 — Frontend Equivalence

| ER | PAC / Risk | Activation | Evidence obligation | Statement | Oracle | Invalidation |
| --- | --- | --- | --- | --- | --- | --- |
| `ER-FRONTEND-ONE-SHARED-RUNTIME` | PAC-4 / R-FRONTEND,R-RUNTIME | INVARIANT | base V+P | C++/QML/Generic/QPA enter one Shared Runtime/Core semantics rather than separate product personalities. | Frontend maps into common Runtime and no prohibited parallel Session/transport/security/perf implementation is required. | Material dependency topology/frontend-runtime ownership/product architecture change. |
| `ER-FRONTEND-UNIQUE-MAPPING` | PAC-4 / R-FRONTEND | CLAIM | base V+P | Each frontend maps its unique activation/configuration/state/error semantics to Shared Runtime correctly. | Frontend-specific input/property/option produces expected common Runtime transition/configuration. | Material frontend public surface/mapping change. |
| `ER-GENERIC-NATIVE-PRESERVATION` | PAC-2,PAC-4 / R-FRONTEND,R-NATIVE | CLAIM | base V+P; claim +Q | Generic keeps the application's native Qt platform authoritative. | Native platform identity remains the expected native platform while Generic activates Shared Runtime. | Material Generic/native-platform interaction/deployment/Qt-platform change. |
| `ER-QPA-NATIVE-DELEGATE-EXACTNESS` | PAC-2,PAC-4,PAC-9 / R-FRONTEND,R-NATIVE,R-COMPAT | CLAIM | base V; claim +Q; release +R | QPA uses the declared exact compatible private ABI/native delegate and fails closed for incompatible pairs. | Exact qualified pair passes with expected delegate; mismatched/unqualified pair is not accepted as supported. | Material Qt patch/private ABI/QPA factory/delegate/platform-plugin/package change. |

---

## 6. PAC-5 — Deployability

| ER | PAC / Risk | Activation | Evidence obligation | Statement | Oracle | Invalidation |
| --- | --- | --- | --- | --- | --- | --- |
| `ER-PACKAGE-CLEAN-CONSUMER` | PAC-1,PAC-5 / R-PACKAGE,R-ADOPTION | INVARIANT | base V+P | Clean external consumers acquire/configure/link supported package forms without repo/build-tree dependency. | Clean consumer succeeds using only declared acquisition contract. | Material install/export/public-target/acquisition change. |
| `ER-DEPLOY-ARTIFACT-CLOSURE` | PAC-5 / R-DEPLOY,R-PACKAGE | INVARIANT | base V+P; claim +Q | Delivered tree contains complete required HyRemote/Qt/native payload for the selected form. | Required payload/origin audit is complete with no missing required component. | Material payload manifest/deploy helper/Qt/plugin/runtime-dependency change. |
| `ER-DEPLOY-ISOLATED-LAUNCH` | PAC-5 / R-DEPLOY | CLAIM | base P; claim +Q; release +R | Delivered application launches from isolated/minimal environment without source/build/SDK-path assistance. | Process reaches declared product path with prohibited runtime/search-path crutches absent. | Material deploy/loader/plugin/package or claimed environment change. |
| `ER-DEPLOY-RELOCATION` | PAC-5 / R-DEPLOY | CLAIM | base P; claim +Q | Relocating delivered tree does not depend on original paths. | Relocated tree launches and runtime origins remain inside allowed locations. | Material RPATH/RUNPATH/DLL/plugin/path/deploy change. |
| `ER-DEPLOY-RUNTIME-ORIGIN` | PAC-5,PAC-9 / R-DEPLOY,R-COMPAT | CLAIM | base P; claim +Q; release +R | Loaded HyRemote/Qt/native components originate from declared delivered/qualified lineage. | Runtime origin audit exactly matches allowed lineage. | Material deployment resolution/package metadata/Qt anchor/candidate artifact change. |
| `ER-DEPLOY-FOREIGN-QT-ISOLATION-LINUX` | PAC-5,PAC-9 / R-DEPLOY,R-COMPAT | CLAIM | base P; claim +Q; release +R | Unrelated distro Qt cannot become an alternate candidate for selected Linux development/deployment Qt lineage. | With foreign distro Qt present, loaded/deployed Qt/HyRemote origins stay in selected qualified lineage and selection is unchanged. | Material Linux runtime discovery/deploy logic/Qt anchor/package metadata/loader policy change. |
| `ER-DEPLOY-NEGATIVE-FAIL-CLOSED` | PAC-5,PAC-9 / R-PACKAGE,R-DEPLOY,R-COMPAT | INVARIANT | base V; affected +P | Missing/inconsistent/incompatible requested payloads/QPA metadata are rejected rather than producing accepted partial deployment. | Each invalid condition fails with intended classification before an accepted incomplete artifact exists. | Material validation rules/metadata schema/supported combination/QPA policy change. |

---

## 7. PAC-6 — Reliability & Boundedness

| ER | PAC / Risk | Activation | Evidence obligation | Statement | Oracle | Invalidation |
| --- | --- | --- | --- | --- | --- | --- |
| `ER-RELIABILITY-BOUNDED-QUEUES` | PAC-6,PAC-8 / R-CORE,R-RELIABILITY,R-PERF | INVARIANT | base V; claim +Q | Frame/input/per-viewer pending work stays bounded under overload. | Queue/backlog/resource counts stay within declared bounds and intended drop/coalesce/admission policy. | Material mailbox/backpressure/admission/per-viewer-flow change. |
| `ER-RELIABILITY-DETERMINISTIC-TEARDOWN` | PAC-6 / R-CORE,R-RUNTIME,R-RELIABILITY | INVARIANT | base V | Start/stop/fault/target destruction has deterministic ownership; callbacks/resources do not outlive teardown. | Final state/resources/callback gates match lifecycle contract across ordered/racing transitions. | Material Session/async/cancellation/ownership change. |
| `ER-RELIABILITY-EXCEPTION-CONTAINMENT` | PAC-6 / R-CORE,R-RUNTIME,R-RELIABILITY | INVARIANT | base V | Adapter/callback exceptions are contained at the Core/Runtime boundary and become the owning lifecycle/error outcome rather than escaping into or terminating the host application. | Startup callback/adapter exceptions produce deterministic start failure with already-started resources cleaned; runtime request/enqueue exceptions remain contained and enter the contract-defined fault/error path; input-post failures remain contained/recoverable and cannot abort unrelated frame delivery; no tested exception crosses the Core callback boundary. | Material adapter/callback invocation boundary, exception translation, Session error/fault policy or input-post failure handling change. |
| `ER-RELIABILITY-SLOW-CLIENT-ISOLATION` | PAC-6,PAC-8 / R-TRANSPORT,R-RELIABILITY,R-PERF | INVARIANT | base V+P; claim +Q | Slow client cannot create unbounded retention or indefinitely block healthy clients/native Qt. | Pending state bounded; healthy/local progress continues under policy. | Material flow-control/encoding queue/scheduling/client-state change. |
| `ER-RELIABILITY-REPETITION-RESOURCE-STABILITY` | PAC-6 / R-RELIABILITY | CLAIM/RELEASE | claim Q; release +R | Repeated lifecycle/client/surface operations do not cause unbounded RSS/thread/FD/handle/socket growth. | Defined repetition completes with resource trend inside tolerance and no crash/hang/corruption. | Material lifecycle/resource-ownership or measurement-baseline environment change. |
| `ER-RELIABILITY-SOAK-STABILITY` | PAC-6,PAC-8 / R-RELIABILITY,R-PERF | CLAIM/RELEASE | claim Q; release +R | Idle/active long-duration operation has no material leak/hang/latency/resource drift. | Declared duration completes inside resource/latency trend envelope. | Material Runtime/transport/capture/resource/perf or reference-environment change. |
| `ER-RELIABILITY-MALFORMED-INPUT-BOUNDS` | PAC-6,PAC-7 / R-TRANSPORT,R-SECURITY,R-RELIABILITY | INVARIANT | base V; claim +Q | Malformed/oversized/fragmented/stalled external input remains bounded. | Invalid input is rejected/contained inside explicit size/time/resource bounds with no crash/unsafe state. | Material parser/framing/config schema/timeout/resource-bound change. |

---

## 8. PAC-7 — Security & Network Exposure Truth

| ER | PAC / Risk | Activation | Evidence obligation | Statement | Oracle | Invalidation |
| --- | --- | --- | --- | --- | --- | --- |
| `ER-SECURITY-SAFE-DEFAULTS` | PAC-7 / R-SECURITY | INVARIANT | base V+P; release +R | Effective shipped **security and remote-input defaults** match current owning security/product authorities; listener bind semantics are owned separately by `ER-NETWORK-LISTENER-BINDING`/#174. | Effective security profile/input default and secret-safe diagnostic/public statements agree with current owning authority; this oracle does not choose between stale listener-default prose. | Material security/input defaults or owning security/public-statement authority change. |
| `ER-NETWORK-LISTENER-BINDING` | PAC-7,PAC-9 / R-NETWORK,R-SECURITY,R-COMPAT | INVARIANT/CLAIM | base V+P; claim +Q | Current #174 semantics are honored: default `0.0.0.0:5921`; requested exact IPv4/interface/address/port narrowing is honored; invalid/unavailable binding is rejected; actual socket exposure never silently broadens scope. | Actual listening endpoint(s)/reachability equal #174/effective requested policy; narrowing never falls back to wildcard/loopback/other interface; invalid/unavailable address or occupied port yields intended failure with no unintended listener. | Material listener authority, configuration, address/interface resolution, bind/rebind, port handling or platform network behavior change. |
| `ER-SECURITY-FAIL-CLOSED` | PAC-7 / R-SECURITY,R-RUNTIME | INVARIANT/CAPABILITY | base V; affected +P; claim +Q | Requested unavailable/invalid protection is rejected before a weaker usable listener/session exists. | No accepted weaker listener/session; intended error and cleanup observed. | Material security capability negotiation/listener start/config/fallback change. |
| `ER-SECURITY-AUTH-MECHANISM` | PAC-7 / R-SECURITY,R-TRANSPORT | CAPABILITY | base V+P; claim +Q | When authentication capability is available in the artifact/declared cell, correct credentials establish the declared authentication mechanism and wrong/missing credentials do not. | Available-authentication fixtures observe expected accept/reject session results with no downgrade; requests against artifacts without the capability are evaluated by `ER-SECURITY-FAIL-CLOSED`, not by this success-path oracle. | Material auth mechanism/credential source/wire/viewer-interoperability or capability-availability declaration change. |
| `ER-SECURITY-SECRET-HYGIENE` | PAC-7,PAC-10 / R-SECURITY,R-OPERATE | INVARIANT/CAPABILITY | base V+P; claim +Q; release +R | Secrets are absent from normal logs, diagnostics, command surfaces and retained evidence. | Prohibited secret material/derivatives absent from declared observable outputs/artifacts. | Material logging/diagnostics/evidence capture/credential/security-mechanism change. |
| `ER-SECURITY-ADMISSION-RESOURCE-BOUNDS` | PAC-6,PAC-7 / R-SECURITY,R-RELIABILITY,R-NETWORK | INVARIANT | base V; claim +Q | Connection/handshake/input admission stays bounded under abusive/stalled peers. | Limits/timeouts bound socket/thread/memory/input backlog and system recovers after peer removal. | Material listener admission/timeouts/client limits/input mailbox/transport state change. |

---

## 9. PAC-8 — Responsiveness & Efficiency

| ER | PAC / Risk | Activation | Evidence obligation | Statement | Oracle | Invalidation |
| --- | --- | --- | --- | --- | --- | --- |
| `ER-PERF-INTERACTION-SLO` | PAC-8 / R-PERF,R-INPUT,R-CAPTURE,R-TRANSPORT | CLAIM/RELEASE | claim Q; release +R | End-to-end remote interaction latency satisfies the declared reference-profile SLO. | Workload distribution satisfies current authority-defined latency thresholds. | Material perf-critical code or reference viewer/network/workload/host change. |
| `ER-PERF-FRESHNESS` | PAC-8,PAC-6 / R-PERF,R-CORE,R-RUNTIME | INVARIANT | base V; claim +Q | Overload prioritizes newest useful state rather than stale backlog. | Frame age/backlog follows declared drop/coalescing policy and remains bounded. | Material scheduling/capture pacing/mailbox/delivery change. |
| `ER-PERF-QUIESCENT-EFFICIENCY` | PAC-8 / R-PERF | CLAIM | base V; claim +Q | Zero-viewer/static scenes avoid unnecessary continuous capture/transmission beyond declared background behavior. | Capture/update activity stays inside idle/static envelope. | Material capture-demand/update-scheduling/transport-idle/workload change. |
| `ER-PERF-LOCAL-UI-HEALTH` | PAC-2,PAC-8 / R-PERF,R-NATIVE | CLAIM | claim Q; release +R | Remote activity preserves declared local Qt responsiveness envelope. | Local interaction/render metric remains within envelope under reference remote workload. | Material capture/encoding/scheduling/native-graphics/reference environment change. |
| `ER-PERF-RESOURCE-ENVELOPE` | PAC-6,PAC-8 / R-PERF,R-RELIABILITY | CLAIM/RELEASE | claim Q; release +R | CPU/GPU/memory/network remains inside qualified resource envelope. | Current authority-defined limits/trends satisfied with no unbounded growth. | Material perf-sensitive code/workload/viewer/network/reference hardware change. |

---

## 10. PAC-9 — Compatibility Truth

| ER | PAC / Risk | Activation | Evidence obligation | Statement | Oracle | Invalidation |
| --- | --- | --- | --- | --- | --- | --- |
| `ER-COMPAT-EXPLICIT-CELL` | PAC-9 / R-COMPAT | CLAIM | claim Q; release +R | Every positive support/product-behavior atomic cell has stable claim/cell identity, declared material dimensions, an immutable normalized target-cell definition/revision, and exactly one authority-owned `(claim_id, cell_id) -> profile_id` binding whose product ER set expands deterministically. | The outer META-CELL evaluator validates identity/schema/status/material dimensions, resolves the immutable `claim_cell_revision` and exact retained `target_material_dimensions`, resolves the exact cell-specific profile binding/`claim_profile_revision`, and computes the canonical `required_product_ERs(cell)` without consulting current test coverage; this derived meta result does not require itself as an input. | Material claim/cell/schema/status/material-dimension/cell-definition/profile/conditional-rule/cell-specific binding/product-obligation change. |
| `ER-COMPAT-QPA-EXACT-ABI` | PAC-9,PAC-4 / R-COMPAT,R-NATIVE | CLAIM | claim Q; release +R | QPA compatibility is exact Qt private-ABI/platform evidence, not family inference. | Claimed exact pair passes; mismatched/unqualified pair is not represented as supported. | Material exact Qt/toolchain/platform/QPA change. |
| `ER-COMPAT-PLATFORM-INDEPENDENCE-TRUTH` | PAC-9 / R-COMPAT,R-NATIVE,R-DEPLOY | CLAIM | claim Q; release +R | Platform-sensitive support is bound to the explicitly declared platform cell; evidence or status from one platform must not be inferred as another platform's evidence/status. | The outer META-PLATFORM evaluator requires this cell's material `os_arch` to match every record used for it and rejects cross-platform substitution. The existence or PASS of one platform cell creates no implicit cell/status/evidence for another platform, but this cell does not depend on another platform cell passing. | Material support-matrix/platform-sensitivity/schema/binding policy change. |
| `ER-COMPAT-GRAPHICS-NO-INFERENCE` | PAC-9 / R-COMPAT,R-NATIVE,R-CAPTURE | CLAIM | claim Q; release +R | Portable-baseline Widgets/Quick evidence does not create a specialized graphics/native-surface claim by inference. | The outer META-GRAPHICS-SCOPE evaluator verifies the current cell's explicit `graphics_scope`; baseline evidence cannot satisfy or synthesize a specialized graphics claim. Any specialized positive claim must exist explicitly with its own profile/cell and targeted product obligations, but baseline-cell PASS does not depend on that separate claim existing or passing. | Material graphics claim/schema/capture/backend/binding policy change. |
| `ER-COMPAT-THIRDPARTY-NONINFERENCE` | PAC-1,PAC-9 / R-COMPAT,R-ADOPTION | CLAIM/RELEASE | claim Q; release +R | Third-party success pressure-tests declared matrix but does not create named-app/broader support. | Record pins app/revision/environment/routes and stays labelled verification rather than inferred support expansion. | Material fixture/environment/support-policy change. |
| `ER-COMPAT-EVIDENCE-BINDING` | PAC-9 / R-COMPAT | CLAIM/RELEASE | claim Q; release +R | Positive claims consume only evidence valid for the exact relevant atomic claim/cell/profile binding revision, immutable target-cell-definition revision, retained target material-dimension snapshot, and material artifact/environment/fixture identity. | The outer META-BINDING evaluator applies to **every V/P/Q/R product-evidence record consumed by the cell**: it resolves exactly one `(claim_id, cell_id) -> profile_id` binding/`claim_profile_revision`, resolves the immutable `claim_cell_revision` and requires the record's `target_material_dimensions` to exactly equal that revision's canonical cell dimensions, resolves the retained immutable Execution Manifest/digest, requires explicit per-ER Observation Context/observation bindings, requires every ER-bound context/reference to belong to and be reachable from that manifest, requires record-level derived candidate/artifact/environment/fixture facts to equal their canonical sources, requires all contexts contributing to each ER to agree on every material phase-varying dimension, requires every material CCS-1 dimension to resolve from the canonical sources and exactly match the retained target snapshot after normalization, rejects invalidated, mismatched, cross-cell/cross-profile or unresolved records, and enforces exact-candidate rules for release evidence. This derived meta result is evaluated after product records exist and is not part of its own input set. | Material evidence-schema/identity/target-cell definition or revision/material-dimension snapshot/cell-specific profile binding/CCS-1 resolver/invalidation/release-binding rule change. |

Compatibility status vocabulary itself is owned by `docs/compatibility.md`; the evidence system stores/validates an authority status when the claim family has one and does not invent a parallel enum for prose product-behavior claims.

---

## 11. PAC-10 — Operability & Maintainability

| ER | PAC / Risk | Activation | Evidence obligation | Statement | Oracle | Invalidation |
| --- | --- | --- | --- | --- | --- | --- |
| `ER-OPERATE-DIAGNOSTIC-SNAPSHOT` | PAC-10 / R-OPERATE,R-ADOPTION | CLAIM | base V+P; claim +Q | Normal user can obtain one bounded documented secret-safe effective-state report from installed/deployed artifact. | Required effective facts are accurate/present and prohibited secret material absent. | Material diagnostic schema/Runtime fact source/package/troubleshooting-path behavior change. |
| `ER-OPERATE-FAILURE-CLASSIFICATION` | PAC-10 / R-OPERATE,R-ADOPTION | CLAIM | base V+P | Common stopped/config/listener/security/input/viewer/deployment/runtime failures are distinguishable where observable. | Induced failure maps to intended user-facing classification/effective facts, not misleading success/generic state. | Material error/state/diagnostic/deployment-failure reporting change. |
| `ER-MAINTENANCE-TARGET-COHERENCE` | PAC-5,PAC-10 / R-UPGRADE,R-DEPLOY,R-PACKAGE | MAINTENANCE | base P; claim +Q; release +R | Authorized update yields one coherent target artifact lineage with no stale/mixed payload. | Artifact/runtime-origin audit identifies only allowed target lineage and target product path succeeds. | Material source/target identity, package/deploy/update mechanism, Qt/QPA payload policy change. |
| `ER-MAINTENANCE-PUBLIC-COMPATIBILITY` | PAC-1,PAC-9,PAC-10 / R-UPGRADE,R-PACKAGE,R-COMPAT | MAINTENANCE/CLAIM | base V+P; claim +Q | Declared public API/package compatibility across an authorized transition remains true. | Exact supported compatibility operation succeeds without undocumented source/internal dependency change. | Material public API/package/version policy or source/target release change. |
| `ER-MAINTENANCE-ROLLBACK-COHERENCE` | PAC-10 / R-UPGRADE,R-DEPLOY | MAINTENANCE | base P; claim +Q; release +R | Promised rollback restores a coherent known-good lineage, not mixed old/new runtime pieces. | Rollback origin audit matches declared target and documented product path succeeds. | Material rollback policy/mechanism/source/target artifact change. |
| `ER-OPERATE-VERSION-IDENTITY` | PAC-9,PAC-10 / R-OPERATE,R-UPGRADE,R-COMPAT | CLAIM/MAINTENANCE/RELEASE | base V+P; claim +Q; release +R | Diagnostics/artifact metadata identify running/deployed HyRemote accurately enough for support/evidence correlation. | Reported identity matches exact artifact/candidate and changes coherently across authorized transitions. | Material build/version metadata/diagnostics/package/release-identity rule change. |

`PAC-10` safe disable/application-survival evidence is intentionally **not duplicated**: `ER-NATIVE-STOP-SURVIVAL` already maps to PAC-2 and PAC-10 and owns truthful stopped/disabled state plus whole-session input cleanup.

---

## 12. Cross-PAC product journeys

| ER | PAC / Risk | Activation | Evidence obligation | Statement | Oracle | Invalidation |
| --- | --- | --- | --- | --- | --- | --- |
| `ER-JOURNEY-SELF-SERVICE` | PAC-1,PAC-3,PAC-5,PAC-9,PAC-10 / R-ADOPTION,R-PACKAGE,R-DEPLOY,R-OPERATE | CLAIM | base P; claim +Q; release +R | Normal target user can follow documented compatibility/acquisition/integration/deployment/connect/use/diagnosis journey without project-team-only knowledge or hidden runtime crutches. | Declared executable journey completes successfully; all required external inputs/assumptions are explicit. | **Only material changes** to executable journey steps, commands, required inputs, assumptions, product defaults/behavior, acquisition/deployment/config/diagnostic semantics, or claimed support boundary. Spelling, translation, formatting and unrelated prose/examples do not invalidate retained journey evidence by themselves. |
| `ER-JOURNEY-REALWORLD-PRISTINE` | PAC-1,PAC-2,PAC-3,PAC-5,PAC-9 / R-ADOPTION,R-NATIVE,R-COMPAT | CLAIM/RELEASE | claim Q; release +R | Pinned representative third-party apps pressure-test applicable product paths without HyRemote-specific source modifications/hidden repair. | Upstream worktree pristine; exact route/environment/artifact recorded; declared view/input/reconnect/deploy/native observations pass or limitations classified. | Material HyRemote candidate, upstream fixture revision, Qt/environment/route applicability or support-claim change. |

---

## 13. Gate policy

ER IDs and evidence obligations do not encode execution frequency. Risk selection determines when fresh evidence is collected.

| ER shape | Earliest normal gate | Breadth/final evidence |
| --- | --- | --- |
| deterministic invariant | G0/G1 | G1; no unnecessary later duplication |
| affected public/product path | G2 | G4/G6 only when claim/release obligation requires |
| clean deploy/runtime loader | G2 when affected | G4/G6 platform cells |
| listener/network/viewer/transport integration | G2 when directly affected | G4/G5/G6 |
| physical/native cell | not ordinary PR | dedicated Q/G6 |
| performance reference | smoke when affected | G4/G5/G6 |
| stress/fuzz/soak | cheap regression only in PR | G4/G5/G6 |
| maintenance edge | G2 only when directly affected/economical | G4/G6 by version policy |
| real-world third-party | not ordinary PR | G5/G6 by authority |

A gate does not promote evidence automatically. A hosted G4 execution stays V/P when an ER requires native Q evidence.

---

## 14. Development-velocity rules

1. ER/suite metadata is declared once and inherited by cases where possible.
2. No GitHub Issue, CTest identity or CI job per ER/case by default.
3. Automated executions generate their own machine-consumable evidence records when their results are retained for Q/R **or consumed by positive claim completeness at any evidence class**; ordinary PR authors do not transcribe qualification/claim results.
4. Ordinary V/P feedback that is not used for a positive claim may remain lightweight and need not carry claim/cell metadata.
5. Manual records are reserved for genuinely manual/physical observations.
6. Selectors map changed ownership/risk to ERs/suites; developers do not memorize the catalog for normal edits.
7. New platform/Qt/viewer rows normally add cells/fixtures, not ERs.
8. Editorial-only documentation changes do not invalidate expensive product evidence unless they materially change an executable journey, claim or oracle.
9. Claim Obligation Profiles are reusable/inheritable authority-owned mappings; do not copy a full ER list into every qualification cell.
10. Claim-meta checks stay outside profile product obligations so claim-policy validation never forces recursive/cross-cell product reruns.
11. TS-1 may propose ER merge/split only when product statement/failure meaning is genuinely too broad/ambiguous.
12. Success is stronger product evidence per unit development time and maintenance effort, not maximum ER/test count.

---

## 15. TS-1 audit contract

After TS-0 is accepted, TS-1 performs a **read-only** mapping of every current test identity.

TS-1 starts by pinning one explicit `develop` inventory snapshot SHA. Every test count, registration fact, current-gate fact and measured cost in the audit is attributed to that snapshot. If `develop` moves while the audit is open, the audit records a separate delta and reconciles it explicitly; it never silently mixes identities/counts/costs from different repository states.

For each test record:

- ER(s) actually proved / partially proved / duplicated / not proved;
- PAC/risk;
- actual oracle/pass condition;
- evidence class/environment strength;
- platform/capability sensitivity;
- current gates/triggers;
- available P50/P95/setup cost;
- unique versus duplicate evidence value;
- mismatch between intended ER and actual oracle;
- authority drift where an existing test asserts superseded product semantics;
- candidate disposition: `retain identity`, `parameterize`, `move gate`, `governance`, `investigate`.

TS-1 also reports **unmapped ERs** for which required product evidence has no current executable proof. It does not add, remove, merge, rename or reschedule tests.

That boundary keeps product design upstream of inventory cleanup.