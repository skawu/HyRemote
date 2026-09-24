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
| `CAPABILITY` | Required only when the named optional/conditional capability exists or is requested. |
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
| `ER-SECURITY-AUTH-MECHANISM` | PAC-7 / R-SECURITY,R-TRANSPORT | CAPABILITY | base V+P; claim +Q | Correct credentials establish declared auth; wrong/missing credentials do not. | Expected accept/reject session result with no downgrade. | Material auth mechanism/credential source/wire/viewer-interoperability change. |
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
| `ER-COMPAT-EXPLICIT-CELL` | PAC-9 / R-COMPAT | CLAIM | claim Q; release +R | Every positive support claim names material dimensions and has required valid evidence. | Claim→cell→ERs→evidence records is complete with no missing material dimension; status value is consumed from product authority. | Material claim/cell/candidate/environment/required-ER validity change. |
| `ER-COMPAT-QPA-EXACT-ABI` | PAC-9,PAC-4 / R-COMPAT,R-NATIVE | CLAIM | claim Q; release +R | QPA compatibility is exact Qt private-ABI/platform evidence, not family inference. | Claimed exact pair passes; mismatched/unqualified pair is not represented as supported. | Material exact Qt/toolchain/platform/QPA change. |
| `ER-COMPAT-PLATFORM-INDEPENDENCE-TRUTH` | PAC-9 / R-COMPAT,R-NATIVE,R-DEPLOY | CLAIM | claim Q; release +R | One platform family does not substitute for another where product behavior is platform-sensitive. | Every claimed independent platform family has its own required valid cell evidence. | Material support-matrix/platform-sensitivity policy change. |
| `ER-COMPAT-GRAPHICS-NO-INFERENCE` | PAC-9 / R-COMPAT,R-NATIVE,R-CAPTURE | CLAIM | claim Q; release +R | Basic Widgets/Quick success does not qualify specialized graphics/native-surface configurations. | Positive specialized claim has targeted evidence; absent evidence remains non-positive according to product authority. | Material graphics claim/capture/backend change. |
| `ER-COMPAT-THIRDPARTY-NONINFERENCE` | PAC-1,PAC-9 / R-COMPAT,R-ADOPTION | CLAIM/RELEASE | claim Q; release +R | Third-party success pressure-tests declared matrix but does not create named-app/broader support. | Record pins app/revision/environment/routes and stays labelled verification rather than inferred support expansion. | Material fixture/environment/support-policy change. |
| `ER-COMPAT-EVIDENCE-BINDING` | PAC-9 / R-COMPAT | CLAIM/RELEASE | claim Q; release +R | Claims consume only evidence valid for relevant artifact/environment/fixture identity. | No invalidated/mismatched record satisfies a positive claim; release records obey exact-candidate rules. | Material evidence-schema/identity/invalidation/release-binding rule change. |

Compatibility status vocabulary itself is owned by `docs/compatibility.md`; the evidence system stores/validates the authority's value and does not invent a parallel enum.

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
| `ER-JOURNEY-SELF-SERVICE` | PAC-1,3,5,9,10 / R-ADOPTION,R-PACKAGE,R-DEPLOY,R-OPERATE | CLAIM | base P; claim +Q; release +R | Normal target user can follow documented compatibility/acquisition/integration/deployment/connect/use/diagnosis journey without project-team-only knowledge or hidden runtime crutches. | Declared executable journey completes successfully; all required external inputs/assumptions are explicit. | **Only material changes** to executable journey steps, commands, required inputs, assumptions, product defaults/behavior, acquisition/deployment/config/diagnostic semantics, or claimed support boundary. Spelling, translation, formatting and unrelated prose/examples do not invalidate retained journey evidence by themselves. |
| `ER-JOURNEY-REALWORLD-PRISTINE` | PAC-1,2,3,5,9 / R-ADOPTION,R-NATIVE,R-COMPAT | CLAIM/RELEASE | claim Q; release +R | Pinned representative third-party apps pressure-test applicable product paths without HyRemote-specific source modifications/hidden repair. | Upstream worktree pristine; exact route/environment/artifact recorded; declared view/input/reconnect/deploy/native observations pass or limitations classified. | Material HyRemote candidate, upstream fixture revision, Qt/environment/route applicability or support-claim change. |

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
3. Automated executions generate their own machine-consumable evidence records; ordinary PR authors do not transcribe qualification results.
4. Manual records are reserved for genuinely manual/physical observations.
5. Selectors map changed ownership/risk to ERs/suites; developers do not memorize the catalog for normal edits.
6. New platform/Qt/viewer rows normally add cells/fixtures, not ERs.
7. Editorial-only documentation changes do not invalidate expensive product evidence unless they materially change an executable journey, claim or oracle.
8. TS-1 may propose ER merge/split only when product statement/failure meaning is genuinely too broad/ambiguous.
9. Success is stronger product evidence per unit development time and maintenance effort, not maximum ER/test count.

---

## 15. TS-1 audit contract

After TS-0 is accepted, TS-1 performs a **read-only** mapping of every current test identity.

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
