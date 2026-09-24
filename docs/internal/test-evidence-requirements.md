# HyRemote Product Evidence Requirements

Status: **TS-0 canonical evidence-requirement design; no existing test/CI migration is authorized by this document alone**

Authority: #393

Parent strategy: [`test-strategy.md`](test-strategy.md)

Detailed system design: [`test-system-architecture.md`](test-system-architecture.md)

This document freezes the **product-level evidence requirements** that the HyRemote test system must be able to prove. It exists so later inventory audit and test implementation work do not derive product requirements from whatever CTest identities happen to exist today.

The unit here is an **Evidence Requirement (ER)**, not a test executable, CTest identity, CI job, fixture or platform row.

One ER may be proved by several layers of evidence. One suite may prove several ERs. One ER may apply to many qualification cells without creating a new ER ID for every OS/Qt/frontend combination.

The design objective is the smallest stable requirement set that still expresses HyRemote's product promise.

---

## 1. Activation model

An ER is executed only when its product/risk activation condition applies.

| Activation | Meaning |
| --- | --- |
| `INVARIANT` | Fundamental product/architecture property whenever the owning capability exists. |
| `CLAIM` | Required when a positive compatibility/support cell claims the property. |
| `CAPABILITY` | Required only when the named optional/conditional capability is implemented or requested. |
| `MAINTENANCE` | Required only for an update/rollback edge explicitly admitted by product/version authority. |
| `RELEASE` | Required for exact-candidate release acceptance where the release authority names it. |

Activation is deliberately separate from execution frequency. An `INVARIANT` may be proved cheaply in G1 while its platform manifestation is qualified in G6. A `CLAIM` does not imply every PR runs every qualification cell.

Product/version/compatibility authorities decide which claims/capabilities/maintenance edges exist. This catalog does not create new product promises.

---

## 2. Evidence-strength notation

| Symbol | Meaning |
| --- | --- |
| `V` | deterministic Verification |
| `P` | Product Validation through a real artifact/user path |
| `Q` | environment/cell Qualification |
| `R` | exact-candidate Release Acceptance |

`V -> P -> Q -> R` is **not** a simple strength ladder. Each answers a different question. A physical run cannot replace a deterministic state-machine oracle, and a unit test cannot replace native/display/loader qualification.

The `Evidence` column below states the normal minimum combination. Exact release composition remains owned by release authorities.

---

## 3. Requirement design rules

Every ER has:

- a stable product statement;
- one or more PACs and risk domains;
- an activation rule;
- normal evidence classes;
- an explicit oracle/pass condition;
- explicit invalidators.

Do **not** create another ER merely because:

- the OS differs;
- a Qt patch differs;
- the frontend differs;
- a negative input value differs;
- a viewer version differs;
- the same rule is exercised from another gate.

Those differences normally become cases, qualification-cell dimensions or fixtures under the same ER.

Create a new ER only when the product statement/failure meaning itself is materially different.

---

# 4. PAC-1 — Low Intrusion

## ER-INTEGRATE-PUBLIC-BOUNDARY

- **PAC:** PAC-1
- **Risk:** R-FRONTEND, R-ADOPTION
- **Activation:** INVARIANT
- **Statement:** Normal source-integrated applications consume only documented HyRemote public surfaces; internal Core/Runtime/transport/QPA implementation types are not required application dependencies.
- **Evidence:** V + representative P
- **Preferred seam:** installed/public consumer compile/link surface
- **Oracle:** consumer builds using only documented public package/API and contains no prohibited internal linkage/include dependency.
- **Invalidation:** public API/export target/header/package-boundary changes.

## ER-INTEGRATE-ZEROCODE-QT-ONLY

- **PAC:** PAC-1, PAC-4
- **Risk:** R-FRONTEND, R-PACKAGE
- **Activation:** CLAIM for Generic/QPA cells
- **Statement:** Generic/QPA zero-code consumers remain Qt-only at application source/link level and activate HyRemote through their declared Qt integration mechanism.
- **Evidence:** V + P; Q for claimed platform cells
- **Preferred seam:** clean Qt-only consumer
- **Oracle:** application has no HyRemote source/link dependency and the declared plugin/platform activation reaches the Shared Runtime.
- **Invalidation:** Generic/QPA activation, packaging or application-link contract changes; relevant Qt/platform changes for Q evidence.

## ER-INTEGRATE-ONE-DEPLOY-ENTRY

- **PAC:** PAC-1, PAC-5
- **Risk:** R-DEPLOY, R-ADOPTION
- **Activation:** INVARIANT
- **Statement:** Supported consumer forms use the documented HyRemote deployment entry rather than manual knowledge of internal payload files.
- **Evidence:** V + P
- **Preferred seam:** package/deployment consumer
- **Oracle:** documented consumer path succeeds using the public deploy interface without manual internal-copy steps.
- **Invalidation:** deploy API, payload composition or public deployment documentation contract changes.

## ER-INTEGRATE-NO-BACKEND-TUNING

- **PAC:** PAC-1, PAC-8
- **Risk:** R-FRONTEND, R-PERF
- **Activation:** INVARIANT
- **Statement:** Ordinary application integration does not require selecting internal capture implementations, RFB encodings, queue depths, GPU backends or vendor acceleration paths.
- **Evidence:** V + representative P
- **Preferred seam:** public configuration/package surface
- **Oracle:** normal supported journey completes without backend-specific application configuration; internal types/options do not leak into required public API.
- **Invalidation:** public configuration/API changes or product decision promoting an implementation control to supported public policy.

---

# 5. PAC-2 — Native Non-interference

## ER-NATIVE-LOCAL-DISPLAY

- **PAC:** PAC-2
- **Risk:** R-NATIVE, R-CAPTURE
- **Activation:** CLAIM
- **Statement:** Enabling HyRemote does not replace, hide or corrupt the supported application's native local display behavior.
- **Evidence:** V where deterministic seams exist + Q; R for release cells that require physical/native evidence
- **Preferred seam:** native qualification host
- **Oracle:** declared local rendering/window behavior matches the native baseline while remote access is active.
- **Invalidation:** frontend/native delegate/capture/graphics/platform changes; relevant OS/Qt/GPU/driver cell changes.

## ER-NATIVE-LOCAL-INPUT

- **PAC:** PAC-2, PAC-3
- **Risk:** R-NATIVE, R-INPUT
- **Activation:** CLAIM
- **Statement:** Local pointer/keyboard/text/focus behavior remains usable while supported remote access is active.
- **Evidence:** V + Q
- **Preferred seam:** input adapter + native host
- **Oracle:** local input actions remain correctly delivered before/during/after remote sessions and are not blocked by remote state.
- **Invalidation:** input routing, frontend/native platform or focus/text handling changes; relevant native cell changes.

## ER-NATIVE-STOP-SURVIVAL

- **PAC:** PAC-2, PAC-10
- **Risk:** R-RUNTIME, R-NATIVE, R-OPERATE
- **Activation:** INVARIANT; CLAIM for native cells
- **Statement:** Stopping/disabling HyRemote leaves the host application alive and usable with no late remote side effects.
- **Evidence:** V + P/Q where native behavior matters
- **Preferred seam:** Shared Runtime lifecycle + vertical product path
- **Oracle:** Runtime reaches stopped/disabled state, listener/input/capture effects cease, application continues native operation.
- **Invalidation:** Runtime stop/teardown, frontend activation or native delegate changes.

## ER-NATIVE-SLOW-REMOTE-ISOLATION

- **PAC:** PAC-2, PAC-6, PAC-8
- **Risk:** R-NATIVE, R-RELIABILITY, R-PERF
- **Activation:** CLAIM
- **Statement:** Slow/stalled remote peers do not indefinitely block the supported local Qt UI/render path.
- **Evidence:** V + P/Q; extended stress where appropriate
- **Preferred seam:** bounded transport/runtime + native host
- **Oracle:** bounded remote backlog and local responsiveness remain within the declared policy while peer delivery stalls.
- **Invalidation:** scheduling/backpressure/transport/capture/native graphics changes.

## ER-NATIVE-GRAPHICS-TRUTH

- **PAC:** PAC-2, PAC-9
- **Risk:** R-NATIVE, R-COMPAT
- **Activation:** CLAIM for specialized graphics cells
- **Statement:** A graphics/backend configuration is claimed only when native+remote coexistence is explicitly qualified for that configuration.
- **Evidence:** Q/R
- **Preferred seam:** exact graphics qualification cell
- **Oracle:** declared native and remote behavior passes the cell-specific checklist/oracle; unsupported surfaces are classified rather than inferred.
- **Invalidation:** Qt/OS/GPU/driver/graphics backend/capture path changes.

---

# 6. PAC-3 — Remote Experience

## ER-REMOTE-VIEW-CONTENT

- **PAC:** PAC-3
- **Risk:** R-CAPTURE, R-RUNTIME, R-TRANSPORT
- **Activation:** INVARIANT; CLAIM for supported cells
- **Statement:** Remote view presents the current supported application content with correct dimensions/scaling semantics.
- **Evidence:** V + P; Q for claimed environment cells
- **Preferred seam:** adapter/runtime deterministic fixtures + maintained-viewer path
- **Oracle:** expected rendered state/geometry/DPR is observed remotely within the applicable freshness policy.
- **Invalidation:** capture, composition, DPR, transport update or viewer interoperability changes.

## ER-REMOTE-SURFACE-LIFECYCLE

- **PAC:** PAC-3
- **Risk:** R-RUNTIME, R-CAPTURE
- **Activation:** CLAIM according to supported surface types
- **Statement:** Supported surface create/show/hide/resize/destroy, popup/dialog and multi-window behavior is represented correctly in the remote application view.
- **Evidence:** V + P/Q
- **Preferred seam:** automatic surface model + representative applications
- **Oracle:** declared lifecycle transitions produce the expected remote composition with no stale/destroyed surface state.
- **Invalidation:** surface discovery/composition/adapter/native-window behavior changes.

## ER-REMOTE-INPUT-SEMANTICS

- **PAC:** PAC-3
- **Risk:** R-INPUT, R-TRANSPORT
- **Activation:** CAPABILITY when remote control is enabled/claimed
- **Statement:** Supported remote pointer/button/wheel/key/modifier/text actions reach the intended Qt target with normalized semantics.
- **Evidence:** V + P/Q
- **Preferred seam:** normalized input model + maintained viewer
- **Oracle:** each declared input action yields the expected application-level event/effect without unrelated delivery.
- **Invalidation:** transport input decoding, normalization, routing, adapter or Qt input behavior changes.

## ER-REMOTE-INPUT-NEUTRALITY

- **PAC:** PAC-3, PAC-6
- **Risk:** R-INPUT, R-RELIABILITY
- **Activation:** CAPABILITY when remote control exists
- **Statement:** Disconnect, stop, policy disable, target destruction and teardown leave no synthetic key/button held state and no forbidden late queued input.
- **Evidence:** V + representative P
- **Preferred seam:** Core/Runtime input state machine
- **Oracle:** terminal transitions end in neutral input state and post-terminal delivery count is zero.
- **Invalidation:** input state machine, mailbox/admission, teardown or target lifecycle changes.

## ER-REMOTE-RECONNECT

- **PAC:** PAC-3, PAC-6
- **Risk:** R-RUNTIME, R-TRANSPORT, R-NETWORK
- **Activation:** INVARIANT
- **Statement:** A viewer can disconnect and reconnect without requiring application/Runtime reconstruction outside the documented lifecycle.
- **Evidence:** V + P/Q
- **Preferred seam:** Runtime/transport lifecycle + viewer path
- **Oracle:** connection state returns to usable remote view/control after reconnect with no leaked prior-client state.
- **Invalidation:** listener/client/session lifecycle or transport changes.

---

# 7. PAC-4 — Frontend Equivalence

## ER-FRONTEND-ONE-SHARED-RUNTIME

- **PAC:** PAC-4
- **Risk:** R-FRONTEND, R-RUNTIME
- **Activation:** INVARIANT
- **Statement:** C++/QML/Generic/QPA enter one Shared Runtime/Core behavior model rather than frontend-specific Session/transport/security/performance personalities.
- **Evidence:** V + bounded vertical P
- **Preferred seam:** architecture/dependency contract + frontend seam tests
- **Oracle:** frontend activation maps into the common Runtime contract and no prohibited parallel product implementation is required.
- **Invalidation:** dependency topology, frontend/runtime ownership or product architecture changes.

## ER-FRONTEND-UNIQUE-MAPPING

- **PAC:** PAC-4
- **Risk:** R-FRONTEND
- **Activation:** CLAIM per frontend
- **Statement:** Each frontend correctly maps its unique public/Qt activation semantics to the common Runtime lifecycle/configuration/state/error model.
- **Evidence:** V + minimal P
- **Preferred seam:** frontend boundary
- **Oracle:** frontend-specific inputs/properties/options produce the expected Shared Runtime configuration/state transitions.
- **Invalidation:** frontend public surface or mapping implementation changes.

## ER-GENERIC-NATIVE-PRESERVATION

- **PAC:** PAC-2, PAC-4
- **Risk:** R-FRONTEND, R-NATIVE
- **Activation:** CLAIM for Generic cells
- **Statement:** Generic activation preserves the application's native Qt platform identity.
- **Evidence:** V + P/Q
- **Preferred seam:** Generic Qt-only consumer/native platform observation
- **Oracle:** native platform identity before/after Generic activation remains the expected native platform while Shared Runtime is active.
- **Invalidation:** Generic plugin/native-platform interaction, deployment or Qt platform changes.

## ER-QPA-NATIVE-DELEGATE-EXACTNESS

- **PAC:** PAC-2, PAC-4, PAC-9
- **Risk:** R-FRONTEND, R-NATIVE, R-COMPAT
- **Activation:** CLAIM for QPA cells
- **Statement:** QPA uses the declared compatible native delegate and exact Qt private-ABI combination rather than an inferred family-wide compatibility claim.
- **Evidence:** V + Q/R
- **Preferred seam:** QPA factory/delegate + exact qualification cell
- **Oracle:** exact Qt/platform guard accepts the qualified pair, native delegate is the expected one, and incompatible pairs fail closed.
- **Invalidation:** Qt patch/private ABI, QPA factory/delegate, platform plugin or packaging changes.

---

# 8. PAC-5 — Deployability

## ER-PACKAGE-CLEAN-CONSUMER

- **PAC:** PAC-1, PAC-5
- **Risk:** R-PACKAGE, R-ADOPTION
- **Activation:** INVARIANT; CLAIM per public acquisition form
- **Statement:** A clean external consumer can acquire/configure/link the supported HyRemote package form without repository/build-tree dependencies.
- **Evidence:** V + P
- **Preferred seam:** installed/source consumer
- **Oracle:** clean consumer config/build succeeds using only the declared acquisition contract.
- **Invalidation:** install/export/package/public target/acquisition changes.

## ER-DEPLOY-ARTIFACT-CLOSURE

- **PAC:** PAC-5
- **Risk:** R-DEPLOY, R-PACKAGE
- **Activation:** INVARIANT; CLAIM per deployment form
- **Statement:** The deployed tree contains the complete required HyRemote/Qt/native payload for the selected supported frontend/form.
- **Evidence:** V + P/Q
- **Preferred seam:** deployment planner + artifact audit
- **Oracle:** required payload manifest/origin audit is complete and contains no missing required component.
- **Invalidation:** payload manifest, deployment helper, Qt/plugin/runtime dependency changes.

## ER-DEPLOY-ISOLATED-LAUNCH

- **PAC:** PAC-5
- **Risk:** R-DEPLOY
- **Activation:** CLAIM
- **Statement:** The deployed application launches from an isolated/minimal runtime environment without source/build/SDK-path assistance.
- **Evidence:** P/Q/R
- **Preferred seam:** clean deployment host
- **Oracle:** process launches and reaches the declared product path with prohibited search-path/runtime crutches absent.
- **Invalidation:** deploy/runtime loader/plugin/package changes or environment cell changes.

## ER-DEPLOY-RELOCATION

- **PAC:** PAC-5
- **Risk:** R-DEPLOY
- **Activation:** CLAIM where relocation is part of deployment contract
- **Statement:** Moving the delivered tree to a different path does not introduce hidden dependency on original build/install locations.
- **Evidence:** P/Q
- **Preferred seam:** relocated deployment fixture
- **Oracle:** relocated tree launches and product path succeeds with runtime origins remaining inside allowed locations.
- **Invalidation:** RPATH/RUNPATH/DLL/plugin/path construction/deployment changes.

## ER-DEPLOY-RUNTIME-ORIGIN

- **PAC:** PAC-5, PAC-9
- **Risk:** R-DEPLOY, R-COMPAT
- **Activation:** CLAIM
- **Statement:** Loaded HyRemote/Qt/native runtime components originate from the declared deployed/qualified lineage.
- **Evidence:** P/Q/R
- **Preferred seam:** runtime dependency-origin audit
- **Oracle:** loaded component origins match the allowed lineage exactly.
- **Invalidation:** deployment resolution, package metadata, selected Qt anchor or candidate artifact changes.

## ER-DEPLOY-FOREIGN-QT-ISOLATION-LINUX

- **PAC:** PAC-5, PAC-9
- **Risk:** R-DEPLOY, R-COMPAT
- **Activation:** CLAIM for Linux Qt deployment cells
- **Statement:** Unrelated distribution Qt runtimes cannot become alternate candidates for the selected development/deployment Qt lineage.
- **Evidence:** P + Q/R for declared Linux cells
- **Preferred seam:** Linux deploy host with foreign distro Qt present
- **Oracle:** deployed Qt/HyRemote runtime origins remain within selected qualified lineage; foreign Qt presence does not change selection.
- **Invalidation:** Linux runtime discovery/deploy logic, selected Qt anchor, package metadata or loader policy changes.

## ER-DEPLOY-NEGATIVE-FAIL-CLOSED

- **PAC:** PAC-5, PAC-9
- **Risk:** R-PACKAGE, R-DEPLOY, R-COMPAT
- **Activation:** INVARIANT
- **Statement:** Missing/inconsistent/incompatible requested payloads and QPA metadata are rejected explicitly rather than producing a partially broken deployment.
- **Evidence:** V; selected P where failure depends on real artifact environment
- **Preferred seam:** table-driven deployment/package negative suite
- **Oracle:** each invalid condition fails with the intended classification before producing an accepted incomplete artifact.
- **Invalidation:** validation rules, package metadata schema, supported frontend combinations or QPA compatibility policy changes.

---

# 9. PAC-6 — Reliability & Boundedness

## ER-RELIABILITY-BOUNDED-QUEUES

- **PAC:** PAC-6, PAC-8
- **Risk:** R-CORE, R-RELIABILITY, R-PERF
- **Activation:** INVARIANT
- **Statement:** Frame/input/per-viewer pending work remains bounded under overload.
- **Evidence:** V + stress P/Q where integration behavior matters
- **Preferred seam:** Core/Runtime/transport queue contracts
- **Oracle:** queue/backlog/resource counts never exceed declared bounds and overload uses the intended drop/coalescing/admission policy.
- **Invalidation:** mailbox/backpressure/admission/per-viewer delivery changes.

## ER-RELIABILITY-DETERMINISTIC-TEARDOWN

- **PAC:** PAC-6
- **Risk:** R-CORE, R-RUNTIME, R-RELIABILITY
- **Activation:** INVARIANT
- **Statement:** Start/stop/fault/target-destruction paths have deterministic ownership and do not permit callbacks/resources to outlive teardown.
- **Evidence:** V + bounded repetition
- **Preferred seam:** Core/Runtime lifecycle
- **Oracle:** final state/resources/callback gates match the lifecycle contract across ordered and racing transitions.
- **Invalidation:** Session lifecycle, async callback, cancellation or ownership changes.

## ER-RELIABILITY-SLOW-CLIENT-ISOLATION

- **PAC:** PAC-6, PAC-8
- **Risk:** R-TRANSPORT, R-RELIABILITY, R-PERF
- **Activation:** INVARIANT
- **Statement:** A slow/stalled client cannot create unbounded encoded/frame retention or block unrelated clients/native Qt indefinitely.
- **Evidence:** V + P/Q stress
- **Preferred seam:** per-viewer delivery flow
- **Oracle:** pending state remains bounded and healthy client/local progress continues under the declared policy.
- **Invalidation:** transport flow control, encoding queue, scheduling or client-state changes.

## ER-RELIABILITY-REPETITION-RESOURCE-STABILITY

- **PAC:** PAC-6
- **Risk:** R-RELIABILITY
- **Activation:** CLAIM/RELEASE according to support programme
- **Statement:** Repeated start/stop, connect/disconnect, resize/surface churn and target destruction do not produce unbounded RSS/thread/FD/handle/socket/resource growth.
- **Evidence:** Q/R extended repetition
- **Preferred seam:** G4/G5/G6 stress environment
- **Oracle:** resource trend remains within defined tolerance and no crash/hang/state corruption occurs.
- **Invalidation:** lifecycle/resource ownership changes; environment/tooling changes that affect measured resource baseline.

## ER-RELIABILITY-SOAK-STABILITY

- **PAC:** PAC-6, PAC-8
- **Risk:** R-RELIABILITY, R-PERF
- **Activation:** CLAIM/RELEASE where long-run support requires it
- **Statement:** Idle/active long-duration operation remains stable without material leak, hang or latency/resource drift.
- **Evidence:** Q/R soak
- **Preferred seam:** reference/native soak host
- **Oracle:** declared duration completes within resource/latency trend bounds with no crash/hang.
- **Invalidation:** Runtime/transport/capture/resource/performance changes or reference environment changes.

## ER-RELIABILITY-MALFORMED-INPUT-BOUNDS

- **PAC:** PAC-6, PAC-7
- **Risk:** R-TRANSPORT, R-SECURITY, R-RELIABILITY
- **Activation:** INVARIANT
- **Statement:** Malformed, oversized, fragmented or stalled external protocol/config input is bounded and cannot corrupt or indefinitely consume the host.
- **Evidence:** V + fuzz/extended Q where appropriate
- **Preferred seam:** parser/protocol/config boundaries
- **Oracle:** invalid input is rejected/contained within explicit size/time/resource bounds without crash or unsafe state.
- **Invalidation:** parser/framing/config schema/timeouts/resource bounds changes.

---

# 10. PAC-7 — Security Truth

## ER-SECURITY-SAFE-DEFAULTS

- **PAC:** PAC-7
- **Risk:** R-SECURITY
- **Activation:** INVARIANT
- **Statement:** Shipped defaults match documented security/input exposure truth and do not silently enable stronger/weaker behavior than stated.
- **Evidence:** V + P/R release truth
- **Preferred seam:** Runtime defaults + installed diagnostics/docs consistency
- **Oracle:** effective default listener/security/input state equals the declared product default and diagnostic/public statements.
- **Invalidation:** defaults, security profile, listener/input policy or public security statement changes.

## ER-SECURITY-FAIL-CLOSED

- **PAC:** PAC-7
- **Risk:** R-SECURITY, R-RUNTIME
- **Activation:** INVARIANT/CAPABILITY
- **Statement:** Requested protection that is unavailable/invalid is rejected before a usable weaker listener/session is exposed.
- **Evidence:** V + selected P/Q
- **Preferred seam:** security policy/state machine + listener startup
- **Oracle:** no listener/session reaches an accepted weaker state; failure classification and cleanup match policy.
- **Invalidation:** security capability negotiation, listener start ordering, configuration or fallback logic changes.

## ER-SECURITY-AUTH-MECHANISM

- **PAC:** PAC-7
- **Risk:** R-SECURITY, R-TRANSPORT
- **Activation:** CAPABILITY when authentication is supported
- **Statement:** Correct credentials establish the declared authenticated mode; wrong/missing credentials do not.
- **Evidence:** V + P/Q with maintained viewer when interoperability is claimed
- **Preferred seam:** mechanism primitive + wire/viewer handshake
- **Oracle:** exact expected accept/reject session outcome with no downgrade.
- **Invalidation:** authentication mechanism, credential source, wire framing or viewer interop changes.

## ER-SECURITY-SECRET-HYGIENE

- **PAC:** PAC-7, PAC-10
- **Risk:** R-SECURITY, R-OPERATE
- **Activation:** INVARIANT/CAPABILITY
- **Statement:** Secrets are not emitted through normal logs, diagnostics, command surfaces or retained evidence artifacts.
- **Evidence:** V + P/Q/R where secret-bearing capability exists
- **Preferred seam:** diagnostics/log/evidence artifact inspection
- **Oracle:** prohibited secret values/derivatives are absent from declared observable outputs/artifacts.
- **Invalidation:** logging, diagnostics, evidence capture, credential handling or security mechanism changes.

## ER-SECURITY-ADMISSION-RESOURCE-BOUNDS

- **PAC:** PAC-6, PAC-7
- **Risk:** R-SECURITY, R-RELIABILITY, R-NETWORK
- **Activation:** INVARIANT
- **Statement:** Connection/handshake/input admission paths remain bounded under abusive or stalled peers.
- **Evidence:** V + stress/fuzz Q
- **Preferred seam:** listener/transport admission
- **Oracle:** configured/default limits/timeouts prevent unbounded sockets/threads/memory/input backlog and recover after peer removal.
- **Invalidation:** listener admission, timeouts, client limits, input mailbox or transport state changes.

---

# 11. PAC-8 — Responsiveness & Efficiency

## ER-PERF-INTERACTION-SLO

- **PAC:** PAC-8
- **Risk:** R-PERF, R-INPUT, R-CAPTURE, R-TRANSPORT
- **Activation:** CLAIM/RELEASE for declared performance profile
- **Statement:** End-to-end remote interaction latency meets the declared reference profile SLO.
- **Evidence:** Q/R measurement
- **Preferred seam:** stable performance reference environment + maintained reference viewer
- **Oracle:** workload distribution satisfies declared P50/P95 (or current authority-defined) thresholds.
- **Invalidation:** performance-critical Runtime/capture/transport/input changes; viewer/network/workload/reference host changes.

## ER-PERF-FRESHNESS

- **PAC:** PAC-8, PAC-6
- **Risk:** R-PERF, R-CORE, R-RUNTIME
- **Activation:** INVARIANT
- **Statement:** Under overload the system prioritizes newest useful state instead of replaying an unbounded stale frame backlog.
- **Evidence:** V + P/Q measurement where needed
- **Preferred seam:** Core/Runtime scheduling/backpressure
- **Oracle:** frame age/backlog follows declared drop/coalescing policy and remains bounded under induced overload.
- **Invalidation:** scheduling, capture pacing, mailbox or transport delivery changes.

## ER-PERF-QUIESCENT-EFFICIENCY

- **PAC:** PAC-8
- **Risk:** R-PERF
- **Activation:** CLAIM for performance-qualified cells
- **Statement:** Zero-viewer/static scenes avoid unnecessary continuous capture/transmission beyond declared background behavior.
- **Evidence:** V + Q trend
- **Preferred seam:** Runtime capture demand + transport update policy
- **Oracle:** capture/update activity remains at or below declared idle/static envelope.
- **Invalidation:** capture demand, update scheduling, transport keepalive/update behavior or workload definition changes.

## ER-PERF-LOCAL-UI-HEALTH

- **PAC:** PAC-2, PAC-8
- **Risk:** R-PERF, R-NATIVE
- **Activation:** CLAIM
- **Statement:** Remote activity does not materially violate the declared local Qt responsiveness envelope.
- **Evidence:** Q/R measurement/native observation
- **Preferred seam:** native/reference workload host
- **Oracle:** local interaction/render metric remains within declared envelope under the tested remote workload.
- **Invalidation:** capture/encoding/scheduling/native graphics changes or reference environment/workload changes.

## ER-PERF-RESOURCE-ENVELOPE

- **PAC:** PAC-6, PAC-8
- **Risk:** R-PERF, R-RELIABILITY
- **Activation:** CLAIM/RELEASE for declared profile
- **Statement:** CPU/GPU/memory/network usage remains within the declared qualified resource envelope for reference workloads.
- **Evidence:** Q/R measurement
- **Preferred seam:** performance reference environment
- **Oracle:** measured resource metrics satisfy current authority-defined limits/trends with no unbounded growth.
- **Invalidation:** performance-sensitive code, workload/viewer/network/reference hardware changes.

---

# 12. PAC-9 — Compatibility Truth

## ER-COMPAT-EXPLICIT-CELL

- **PAC:** PAC-9
- **Risk:** R-COMPAT
- **Activation:** CLAIM
- **Statement:** Every positive support status names the material environment/product dimensions and is backed by executed evidence for those dimensions.
- **Evidence:** Q/R
- **Preferred seam:** qualification registry/compatibility review
- **Oracle:** claim -> qualification cell -> required ERs -> valid evidence records is complete with no missing material dimension.
- **Invalidation:** claim dimensions/status, candidate, environment or required ER validity changes.

## ER-COMPAT-QPA-EXACT-ABI

- **PAC:** PAC-9, PAC-4
- **Risk:** R-COMPAT, R-NATIVE
- **Activation:** CLAIM for QPA
- **Statement:** QPA compatibility is qualified per exact Qt private-ABI/platform pair rather than inferred from family proximity.
- **Evidence:** Q/R
- **Preferred seam:** exact QPA qualification cell
- **Oracle:** claimed exact pair passes and unqualified/mismatched pair is not represented as supported.
- **Invalidation:** exact Qt patch/toolchain/platform/QPA implementation changes.

## ER-COMPAT-PLATFORM-INDEPENDENCE-TRUTH

- **PAC:** PAC-9
- **Risk:** R-COMPAT, R-NATIVE, R-DEPLOY
- **Activation:** CLAIM
- **Statement:** Evidence from one OS/CPU/native-platform family does not substitute for another where the claim is platform-sensitive.
- **Evidence:** Q/R policy + cell evidence
- **Preferred seam:** qualification plan/registry
- **Oracle:** every claimed independent platform family has its own required valid cell evidence.
- **Invalidation:** compatibility/support matrix or platform-sensitivity policy changes.

## ER-COMPAT-GRAPHICS-NO-INFERENCE

- **PAC:** PAC-9
- **Risk:** R-COMPAT, R-NATIVE, R-CAPTURE
- **Activation:** CLAIM
- **Statement:** Basic Widgets/Quick success does not automatically qualify specialized graphics/native-surface configurations.
- **Evidence:** Q/R policy + targeted cells
- **Preferred seam:** graphics qualification matrix
- **Oracle:** positive specialized graphics claims have targeted evidence; absent evidence remains explicitly non-positive/limited according to product policy.
- **Invalidation:** graphics support claims or capture/backend implementation changes.

## ER-COMPAT-THIRDPARTY-NONINFERENCE

- **PAC:** PAC-1, PAC-9
- **Risk:** R-COMPAT, R-ADOPTION
- **Activation:** CLAIM/RELEASE when real-world programme is required
- **Statement:** Representative third-party success pressure-tests the declared matrix but does not create named-application or broader platform support by itself.
- **Evidence:** Q/R review
- **Preferred seam:** pinned real-world programme
- **Oracle:** result records exact app/revision/environment/routes and remains labelled verification rather than an inferred support expansion.
- **Invalidation:** representative fixture revision/environment or product support policy changes.

## ER-COMPAT-EVIDENCE-BINDING

- **PAC:** PAC-9
- **Risk:** R-COMPAT
- **Activation:** CLAIM/RELEASE
- **Statement:** Qualification/release claims consume only evidence valid for the relevant artifact/environment/fixture identity.
- **Evidence:** Q/R meta-validation
- **Preferred seam:** evidence registry/release gate
- **Oracle:** no expired/invalidated/mismatched record satisfies a positive claim; exact-candidate release records obey release identity rules.
- **Invalidation:** evidence schema/identity/invalidation policy or release binding rules change.

---

# 13. PAC-10 — Operability & Maintainability

## ER-OPERATE-DIAGNOSTIC-SNAPSHOT

- **PAC:** PAC-10
- **Risk:** R-OPERATE, R-ADOPTION
- **Activation:** CLAIM once self-service diagnostics are part of the product line
- **Statement:** A normal user can obtain one bounded documented secret-safe snapshot/report of effective product state from an installed/deployed artifact.
- **Evidence:** V + P/Q
- **Preferred seam:** Shared Runtime effective-state model + installed product path
- **Oracle:** required effective facts are present/accurate and prohibited secret material is absent.
- **Invalidation:** diagnostic schema, Runtime effective-state sources, packaging or documented troubleshooting path changes.

## ER-OPERATE-FAILURE-CLASSIFICATION

- **PAC:** PAC-10
- **Risk:** R-OPERATE, R-DIAG, R-ADOPTION
- **Activation:** CLAIM once diagnostics are productized
- **Statement:** Common stopped/configuration/listener/security/input/viewer/deployment/runtime failure classes are distinguishable where technically observable.
- **Evidence:** V + representative P
- **Preferred seam:** deterministic failure injection + installed troubleshooting path
- **Oracle:** induced failure maps to the intended user-facing classification/effective facts rather than a misleading generic success/state.
- **Invalidation:** error/state/diagnostics/deployment failure reporting changes.

## ER-OPERATE-SAFE-DISABLE

- **PAC:** PAC-2, PAC-3, PAC-10
- **Risk:** R-OPERATE, R-RUNTIME, R-INPUT
- **Activation:** INVARIANT
- **Statement:** Users can stop/disable HyRemote safely and the host application remains operational without late synthetic input/listener activity.
- **Evidence:** V + P/Q
- **Preferred seam:** Runtime lifecycle + installed/native product path
- **Oracle:** disabled state is truthful; remote listener/input/capture effects cease; host application remains usable.
- **Invalidation:** lifecycle, input cleanup, frontend activation or diagnostics state changes.

## ER-MAINTENANCE-TARGET-COHERENCE

- **PAC:** PAC-5, PAC-10
- **Risk:** R-UPGRADE, R-DEPLOY, R-PACKAGE
- **Activation:** MAINTENANCE
- **Statement:** An authorized update produces one coherent target artifact lineage with no stale/mixed HyRemote/Qt/plugin/QML/package payload.
- **Evidence:** P/Q; R where release authority requires transition evidence
- **Preferred seam:** isolated source-release -> target-candidate transition environment
- **Oracle:** artifact/runtime-origin audit identifies only the allowed target lineage and the target product path succeeds.
- **Invalidation:** source/target release identity, package/deploy/update mechanism, Qt/QPA payload policy changes.

## ER-MAINTENANCE-PUBLIC-COMPATIBILITY

- **PAC:** PAC-1, PAC-9, PAC-10
- **Risk:** R-UPGRADE, R-PACKAGE, R-COMPAT
- **Activation:** MAINTENANCE/CLAIM
- **Statement:** Declared public API/package compatibility across an authorized transition remains true for clean consumers.
- **Evidence:** V + P/Q
- **Preferred seam:** source-release consumer rebuilt/used according to declared compatibility policy
- **Oracle:** the exact supported compatibility operation succeeds without undocumented source/internal dependency changes.
- **Invalidation:** public API/package/version compatibility policy or source/target release changes.

## ER-MAINTENANCE-ROLLBACK-COHERENCE

- **PAC:** PAC-10
- **Risk:** R-UPGRADE, R-DEPLOY
- **Activation:** MAINTENANCE only where rollback is explicitly promised
- **Statement:** Documented rollback restores a coherent known-good artifact rather than mixing source/target runtime pieces.
- **Evidence:** P/Q/R according to claim
- **Preferred seam:** isolated maintenance transition environment
- **Oracle:** rollback artifact/runtime-origin audit matches the declared rollback target and the documented product path succeeds.
- **Invalidation:** rollback policy/mechanism or source/target artifact changes.

## ER-OPERATE-VERSION-IDENTITY

- **PAC:** PAC-9, PAC-10
- **Risk:** R-OPERATE, R-UPGRADE, R-COMPAT
- **Activation:** CLAIM/MAINTENANCE/RELEASE
- **Statement:** Effective diagnostics/artifact metadata identify the running/deployed HyRemote candidate/version accurately enough to correlate support evidence and defect reports.
- **Evidence:** V + P/Q/R
- **Preferred seam:** build/package metadata + Runtime diagnostics
- **Oracle:** reported identity matches the exact artifact/candidate under test and changes coherently across authorized transitions.
- **Invalidation:** version/build metadata, diagnostics, packaging or release identity rules change.

---

# 14. Cross-PAC product-journey requirements

These requirements deliberately span several PACs. They are not extra PACs and do not duplicate lower-level deterministic proof.

## ER-JOURNEY-SELF-SERVICE

- **PAC:** PAC-1, PAC-3, PAC-5, PAC-9, PAC-10
- **Risk:** R-ADOPTION, R-PACKAGE, R-DEPLOY, R-OPERATE
- **Activation:** CLAIM for self-service product stages
- **Statement:** A normal target user can follow the documented product journey from compatibility evaluation/acquisition through integration/deployment/connect/use/diagnosis without project-team-only knowledge or hidden runtime crutches.
- **Evidence:** P; selected Q/R for declared release journeys
- **Preferred seam:** canonical docs/examples + clean installed product path
- **Oracle:** documented steps complete successfully in the declared environment and every required external input/assumption is explicit.
- **Invalidation:** public docs/examples/acquisition/deploy/config/defaults/diagnostics or support-matrix changes.

## ER-JOURNEY-REALWORLD-PRISTINE

- **PAC:** PAC-1, PAC-2, PAC-3, PAC-5, PAC-9
- **Risk:** R-ADOPTION, R-NATIVE, R-COMPAT
- **Activation:** CLAIM/RELEASE when the real-world programme is required
- **Statement:** Representative pinned third-party Qt applications can pressure-test applicable zero-code/product paths without HyRemote-specific source modifications or hidden environment repair.
- **Evidence:** Q/R according to the real-world authority
- **Preferred seam:** #134/#242 representative programme
- **Oracle:** upstream worktree remains pristine; exact applicable route/environment/artifact is recorded; declared view/input/reconnect/deployment/native observations pass or limitations are classified truthfully.
- **Invalidation:** HyRemote candidate, upstream fixture revision, Qt/environment/route applicability or support claim changes.

---

# 15. Requirement-to-gate policy

ER IDs do not encode gates. Normal scheduling follows risk and cost:

| Requirement shape | Normal earliest gate | Breadth / final evidence |
| --- | --- | --- |
| deterministic invariant | G0/G1 | G1; no unnecessary later duplication |
| affected public/product path | G2 | G4/G6 where the claim requires it |
| clean deploy/runtime loader | G2 when affected | G4/G6 platform cells |
| maintained viewer/network integration | G2 when directly affected | G4/G5/G6 |
| physical/native cell | not ordinary PR | G6 or dedicated qualification |
| performance reference | smoke only when affected | G4/G5/G6 |
| stress/fuzz/soak | deterministic regression only in PR | G4/G5/G6 |
| maintenance edge | G2 only when directly affected and economical | G4/G6 according to version policy |
| real-world third-party | not ordinary PR | G5/G6 according to authority |

A gate does not promote evidence automatically. A G4 run that uses only a hosted synthetic environment remains V/P if the ER requires native Q evidence.

---

# 16. Development-velocity and maintenance-cost rules

This catalog must not become another source of process drag.

1. ER metadata is declared **once at requirement/suite level** and inherited by cases where possible.
2. Do not require a GitHub Issue, CTest identity or CI job per ER/case.
3. Automated runs generate automated evidence records; ordinary PR authors do not hand-maintain qualification ledgers.
4. Human/physical evidence is manual only where the property itself genuinely requires human/native observation.
5. Selectors reason from changed ownership/risk to ERs/suites; developers should not manually memorize an ER matrix for routine edits.
6. Adding a new platform/Qt/viewer normally expands qualification cells/fixtures under existing ERs, not the ER count.
7. TS-1 may propose ER merges/splits only when actual inventory analysis proves the product statement/failure meaning is too broad or ambiguous; raw test count is not a reason.
8. The success metric is stronger product evidence per unit of development time, not maximum requirement/test count.

---

# 17. TS-1 audit contract

After TS-0 is accepted, TS-1 performs a read-only mapping of every current test identity.

For each current test, TS-1 records:

- ER(s) it actually proves, partially proves, duplicates or does not prove;
- PAC/risk mapping;
- actual oracle/pass condition;
- evidence class/environment strength;
- platform/capability sensitivity;
- current execution gates/triggers;
- P50/P95 or available cost/setup evidence;
- duplication/uniqueness;
- gaps between intended ER and actual oracle;
- candidate disposition (`retain identity`, `parameterize`, `move gate`, `governance`, `investigate`).

TS-1 also reports **unmapped ERs**: required product evidence for which no current executable evidence exists.

TS-1 does not edit, remove, merge, reschedule or add tests.

This is the boundary that keeps product design upstream of inventory cleanup.