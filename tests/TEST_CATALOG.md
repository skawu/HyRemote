# HyRemote Test Catalog

> Phase A audit for #274. Repository facts refreshed against `develop` at `6dc244a8717bdc520544c71d9756499033857c73`.
>
> This file is the authoritative **necessity / ownership / disposition** catalog. It does not replace CTest/CMake as execution authority. The exact hosted all-frontends registration baseline is in [`EXECUTION_BASELINE.md`](EXECUTION_BASELINE.md), scenario details are in [`TEST_SCENARIOS.md`](TEST_SCENARIOS.md), capability/cost mapping is in [`TEST_MATRIX.md`](TEST_MATRIX.md), and missing coverage is in [`COVERAGE_GAPS.md`](COVERAGE_GAPS.md).

## 1. Taxonomy and final Phase-A decisions

| Layer | Meaning | Typical owner |
| --- | --- | --- |
| T1 | deterministic module/component behavior, including private implementation seams | Core, Runtime, RFB, Widgets/Quick Runtime adapters |
| T2 | behavior unique to one peer integration frontend | C++, QML, Generic, QPA |
| T3 | repository/product contract independent of one release candidate | build authority, layout, CI classifier, public API shape |
| T4 | external consumer/package/deploy contract | installed/source consumers, relocation, deploy |
| T5 | user/adoption/product end-to-end flow | examples, viewer/product-fit harnesses |
| T6 | release/candidate-specific authority and qualification | release scope/profile/metadata/readiness |

Final disposition values:

- `KEEP`: distinct necessary contract remains with its current semantic owner.
- `MOVE`: necessary contract, wrong semantic/physical owner.
- `SPLIT`: one current test combines contracts with different owners/failure meanings.
- `MERGE`: keep the contract but consolidate it into a stronger equivalent test.
- `RETIRE`: no distinct necessary contract remains.
- `GAP`: required evidence is absent or a claimed scenario is not actually executed.

Phase A leaves **no `REVIEW` disposition**. A future uncertainty is recorded as a GAP with an owner rather than left as an unbounded review state.

## 2. T1 Core — all KEEP

| CTest | Decision | Protected contract / failure meaning |
| --- | --- | --- |
| `hyremote-core-test-frame-lifetime` | KEEP | frame storage/lifetime survives async handoff; failure means unsafe/dangling ownership |
| `hyremote-core-test-damage` | KEEP | deterministic damage tri-state/regions |
| `hyremote-core-test-timing` | KEEP | PTS/capture timing/scheduling semantics |
| `hyremote-core-test-mailbox` | KEEP | bounded queue/drop/backpressure mechanics |
| `hyremote-core-test-transport-handoff` | KEEP | Core↔transport frame/input/event handoff |
| `hyremote-core-test-session-lifecycle` | KEEP | Session state machine, fault escalation, concurrency and deterministic teardown |
| `hyremote-core-test-session-defaults` | KEEP | safe valid default SessionConfig |
| `hyremote-core-test-input-routing` | KEEP | transport input reaches only the configured sink under lifecycle bounds |
| `hyremote-core-test-input-normalization` | KEEP | backend-independent input vocabulary |
| `hyremote-core-test-callback-lifetime` | KEEP | callbacks cannot outlive stop/destruction |
| `hyremote-core-test-callback-exception-boundary` | KEEP | backend callback exceptions do not terminate the host |
| `hyremote-core-test-dependency-boundary` | KEEP | Core remains Qt-GUI/protocol/platform neutral |

`hyremote-core-test-session-lifecycle` intentionally contains many related state-machine regressions. File size alone is not a reason to split it; scenario-level rationale is in `TEST_SCENARIOS.md`.

## 3. T1 Shared Runtime / RFB / target adapters

### Correctly Runtime-owned

| CTest | Decision | Protected contract |
| --- | --- | --- |
| `hyremote-runtime-automatic-surface-model-test` | KEEP | automatic application-surface model |
| `hyremote-runtime-automatic-composite-capture-test` | KEEP | composite geometry/capture |
| `hyremote-runtime-automatic-composite-input-test` | KEEP | composite input routing |

### Necessary tests currently misowned by C++

| CTest | Final decision | Target owner / rationale |
| --- | --- | --- |
| `hyremote-security-descriptor-test` | MOVE | Runtime/security; descriptor parsing/fail-closed credentials are not C++ facade behavior |
| `hyremote-vnc-auth-test` | MOVE | Runtime/RFB security primitive |
| `hyremote-rfb-vnc-auth-handshake-test` | MOVE | Runtime/RFB wire negotiation/auth/no-downgrade/timeout |
| `hyremote-rfb-multi-client-input-test` | MOVE | Runtime/RFB viewer-held-state isolation |
| `hyremote-listener-address-matrix-test` | SPLIT | keep public `RemoteAccess` config/error mapping rows under C++; move bind/address-family/wildcard/IPv6 behavior to Runtime/RFB when extracted; do not duplicate behavior |
| `hyremote-input-mailbox-admission-test` | MOVE | Runtime admission/backpressure before GUI adapter delivery |
| `hyremote-target-component-provider-test` | MOVE | Runtime target→capture/input adapter selection |
| `hyremote-widgets-capture-test` | MOVE | Runtime/Widgets capture; forced-HiDPI execution is GAP TG-001 |
| `hyremote-widgets-input-routing-test` | MOVE | Runtime/Widgets input routing |
| `hyremote-widgets-input-backpressure-test` | MOVE | Runtime/Widgets bounded GUI dispatch and held-release reserve |
| `hyremote-quick-capture-test` | MOVE | Runtime/Quick capture; forced-HiDPI execution is GAP TG-002 |
| `hyremote-quick-input-routing-test` | MOVE | Runtime/Quick input routing |
| `hyremote-quick-input-backpressure-test` | MOVE | Runtime/Quick bounded GUI dispatch |
| `hyremote-rfb-widget-disconnect-backpressure-test` | MOVE | Runtime/RFB+Widgets cross-component disconnect cleanup under saturation |

`rfb_test_server.cpp` is support code and `rfb_product_fit.py` is a T5 harness; both leave C++ physical ownership when Phase B/D establishes the shared RFB harness owner.

## 4. T2 C++ frontend — facade only

| CTest | Decision | Protected contract |
| --- | --- | --- |
| `hyremote-remoteaccess-test` | KEEP | safe defaults, inert construction, config mutability, lifecycle forwarding, move ownership, client count/error mapping, fail-closed encrypted-profile facade behavior |
| `hyremote-remoteaccess-error-ack-test` | KEEP | public error acknowledgement/reappearance contract |
| `hyremote-remoteaccess-target-loss-test` | KEEP | public facade response to target destruction/loss |

C++ tests may use controlled Runtime fakes; they must not become the home for RFB parser/security or GUI-adapter internals.

## 5. T2 QML frontend

| CTest | Decision | Protected contract |
| --- | --- | --- |
| `hyremote-qml-module-test` | KEEP | import/type registration, safe declarative defaults, transactional enabled/config/error semantics, target lifetime |
| `hyremote-qml-deploy-helper-non-qml` | MOVE -> T4 | ordinary deploy dispatch must not mutate application QML import paths |
| `hyremote-qml-deploy-helper-qml` | MOVE -> T4 | QML deploy dispatch must preserve app import paths and add installed HyRemote import root |

The two deploy-helper tests stay behaviorally distinct from the QML module test and from exact-SHA installed-QML evidence.

## 6. T2 Generic frontend

| CTest | Decision | Protected contract |
| --- | --- | --- |
| `hyremote-generic-plugin-smoke` | KEEP | Qt generic-plugin activation with native QPA preserved and no application HyRemote API/link requirement |

Installed Generic Widgets/Quick tests remain separate T4 contracts because in-tree activation cannot prove clean SDK acquisition/deployment.

## 7. T2 QPA frontend behavior

| CTest | Decision | Protected contract |
| --- | --- | --- |
| `hyremote-qpa-proxy-smoke` | KEEP | platform plugin load/delegate path |
| `hyremote-qpa-native-semantics` | KEEP | exact-private-ABI native delegate semantics |
| `hyremote-qpa-remote-config-test` | KEEP | QPA-only launch/config vocabulary |
| `hyremote-qpa-auto-remoteaccess-smoke` | KEEP | QPA starts the one Shared Runtime |
| `hyremote-qpa-remote-failure-native-survival-smoke` | KEEP | remote failure does not destroy local/native app |
| `hyremote-qpa-multi-surface-connection-smoke` | KEEP | multiple QWidget surfaces through proxy |
| `hyremote-qpa-widget-popup-connection-smoke` | KEEP | popup/transient semantics through proxy |
| `hyremote-qpa-widget-opengl-capture-smoke` | KEEP conditional | QOpenGLWidget capture classification when Qt OpenGLWidgets exists |
| `hyremote-qpa-quick-multi-window-connection-smoke` | KEEP | multiple Quick windows through proxy |

QPA deployment tests are not T2 behavior tests; they move as a group to T4 while preserving their scenarios.

## 8. T3 repository/product contracts

| CTest / fixture | Decision | Protected contract |
| --- | --- | --- |
| `hyremote-build-authority-selftest` | MOVE | canonical `compile.cmd -> CMake/build.yml`; currently registered by Runtime |
| `hyremote-ci-scope-self-test` | KEEP | classifier cannot create false-green/incorrect evidence lanes |
| `hyremote-mainline-audit-self-test` | KEEP | mainline audit retry/verification logic is executable |
| `hyremote-branch-name-gate-self-test` | KEEP | branch-family governance |
| `tests/public-api-contract` | KEEP | exported target/API/dependency surface; reword stale V1-only vocabulary without weakening assertions |
| `hyremote-release-readiness-runtime-contract` | SPLIT | move cross-release architecture/dependency invariants to T3; retain only candidate-specific truth in T6 |
| `hyremote-release-readiness-repository-layout` | MOVE | canonical source/dependency layout is repository truth |
| `hyremote-release-readiness-documentation-paths` | MOVE | verifies maintained Markdown file links resolve; no candidate-specific semantics |
| `hyremote-release-readiness-ci-environment-baseline` | MOVE | hosted/reference CI environment truth |
| `hyremote-release-readiness-licensing-boundary` | MOVE | dependency/license boundary across releases |

## 9. T4 external consumer / package / deploy

### Consumers and persistent package contracts

| Test/cell | Decision | Distinct necessity |
| --- | --- | --- |
| `hyremote-cpp-installed-consumers` | KEEP | clean installed Widgets+C++ and Quick+C++ primary paths |
| `hyremote-generic-installed-consumers` | KEEP | clean installed Generic Widgets/Quick with native platform identity |
| `tests/consumer-installed-sdk` / `installed-sdk` | KEEP, rename later | minimal package/export/deploy closure without an adapter-specific application; catches failures stronger consumers can mask |
| `tests/consumer-source` / `source-consumer` | KEEP | source/add_subdirectory consumption remains independent of installed-package assumptions |
| installed QML consumer/evidence | KEEP preview | clean declarative payload consumption |
| installed QPA consumer/product-fit | KEEP preview | clean exact-private-ABI QPA package/deploy behavior |
| `hyremote-release-readiness-deployment-relocation` | MOVE | relocation is a persistent package/deploy contract |
| `hyremote-release-readiness-deploy-helper-contract` | MOVE | persistent static deploy/package contract; keep static proof layer distinct from execution fixtures |
| `hyremote-release-readiness-consumer-simplicity` | MOVE | external apps must not learn internal targets/choices |
| `hyremote-release-readiness-package-acquisition-isolation` | MOVE | installed package must not acquire source/build tree |
| `hyremote-release-readiness-source-qpa-authority` | MOVE | negative QPA source/installed acquisition-authority scenario, not release-specific truth |

### QPA deploy-helper registered matrix — all MOVE to T4/QPA deploy-contract

Each name protects a distinct configuration or negative failure reason, so none is approved for deletion:

- `hyremote-qpa-deploy-helper-ordinary`
- `hyremote-qpa-deploy-helper-qml-only`
- `hyremote-qpa-deploy-helper-qml-composed`
- `hyremote-qpa-deploy-helper-installed-payload`
- `hyremote-qpa-deploy-helper-installed-payload-qml-only`
- `hyremote-qpa-deploy-helper-installed-payload-qml`
- `hyremote-qpa-deploy-helper-reject-missing-qml`
- `hyremote-qpa-deploy-helper-reject-stale-qml-metadata`
- `hyremote-qpa-deploy-helper-reject-missing-qml-root`
- `hyremote-qpa-deploy-helper-reject-missing-qml-module-dir`
- `hyremote-qpa-deploy-helper-reject-stale-qpa-metadata`
- `hyremote-qpa-deploy-helper-reject-qt-mismatch`
- `hyremote-qpa-deploy-helper-reject-missing-package`
- `hyremote-qpa-deploy-helper-single-config-generator`
- `hyremote-qpa-deploy-helper-multi-config-generator`
- `hyremote-qpa-source-payload-relocation` — Linux-only by explicit guard.

Phase B may share fixture/runner code with QML deploy tests, but static contract scan, deterministic negative configure matrix and real exact-SHA release evidence are **different proof layers**, not duplicates.

## 10. T5 product/adoption E2E

| Test/asset | Decision | Distinct necessity / follow-up |
| --- | --- | --- |
| `hyremote-v01-example-smoke` | KEEP | shipped SDK -> independently build/deploy/run canonical 01/02/03; proves adoption path and Generic native identity |
| `src/integrations/cpp/tests/rfb_product_fit.py` | MOVE + GAP TG-009 | maintained-viewer framebuffer/input/reconnect/timeout/held-input user-level evidence; move to semantic RFB E2E owner and give it explicit execution authority |
| `tests/product-e2e/example_product_fit.py` | KEEP | app-level Widgets/Quick view-only→control, framebuffer, buttons/wheel/modifiers/text, reconnect/listener-release contract is stronger/different than adoption smoke; retarget scenario naming/inputs to canonical 01/02 and never resurrect removed flat examples |
| `tests/product-e2e/qml_product_fit.py` | KEEP | QML-visible client count, view-only isolation, same-process stop→configure→start, control input and reconnect; preview/non-fast E2E |
| `tests/product-e2e/showcase_product_fit.py` | KEEP | maintained showcase-specific `SHOWCASE_CLIENTS` lifecycle, remote input, reconnect and listener release; non-fast and not a V0.1 primary blocker |

Dormant test code is not evidence. TG-009/TG-011 record missing execution ownership where applicable.

## 11. T6 release/candidate authority

| CTest | Decision | Protected release truth |
| --- | --- | --- |
| `hyremote-release-readiness-metadata` | SPLIT | keep actual version/security/release truth in T6; move permanent repository/product assertions to T3/T4 |
| `hyremote-release-readiness-release-authority-policy` | KEEP | machine release-train authority matches governance and fails closed |
| `hyremote-release-readiness-release-documentation-layout` | KEEP | release-note/candidate documentation artifact layout |
| `hyremote-release-scope-self-test` | KEEP | exact train selects its own WBS; unknown/invalid selection fails closed |
| `hyremote-release-profile-develop-all` | KEEP | development sentinel accepts broad capabilities |
| `hyremote-release-profile-develop-runtime-only` | KEEP | development sentinel supports reduced capability build |
| `hyremote-release-profile-retire-v001` | KEEP | retired frontend-coded version rejected |
| `hyremote-release-profile-retire-v002` | KEEP | same |
| `hyremote-release-profile-retire-v003` | KEEP | same |
| `hyremote-release-profile-v010-cpp-only` | KEEP | V0.1 representable without frontend-as-version semantics |
| `hyremote-release-profile-v020-runtime` | KEEP | V0.2 line representable |
| `hyremote-release-profile-v030-all` | KEEP | V0.3 line representable |
| `hyremote-release-profile-v040-all` | KEEP | V0.4 line representable |
| `hyremote-release-profile-v040-maintenance` | KEEP | maintenance digit remains inside V0.4 line |
| `hyremote-release-profile-v100-all` | KEEP | GA all-capability profile |
| `hyremote-release-profile-v100-cpp-only` | KEEP | capability subset does not redefine version meaning |
| `hyremote-release-profile-v100-generic-only` | KEEP | same for Generic-only subset |

#277 expanded Feature-release authority inside the existing release-scope/policy test registrations. Those are scenario-level additions, not new CTest names.

## 12. Coverage GAP decisions produced by Phase A

The catalog does not pretend missing tests exist. Confirmed gaps are owned in `COVERAGE_GAPS.md`, including:

- TG-001/TG-002: Widgets/Quick forced-HiDPI execution missing;
- TG-009: real RFB product-fit code exists but lacks execution authority required by #229 exact-candidate evidence;
- TG-012: clean-consumer acquisition audit can false-pass mixed run/source/build cache entries; returned to reopened #230;
- TG-013/TG-014: fragmented RFB reads and bounded malformed/oversized protocol input lack deterministic registered scenarios;
- package paths containing spaces and same-destination deploy repeatability/idempotence are confirmed T4 robustness gaps;
- semantic CTest labels/tiering are absent and belong to Phase C.

## 13. Phase-A completion state

Phase A is complete when this PR head satisfies all of the following:

- every registered CTest in `EXECUTION_BASELINE.md` has a disposition above;
- every meaningful multi-case executable/script has scenario coverage or a referenced family rationale in `TEST_SCENARIOS.md`;
- capability/platform/cost conditions are mapped in `TEST_MATRIX.md`;
- overlap decisions are explicit; no registered test is removed merely for CI speed;
- missing evidence is a GAP with owner/severity rather than an unresolved REVIEW;
- `tests/README.md` reflects current tree and links this audit set;
- V0.1 blockers discovered by the audit are handed back to their owning release issues rather than fixed opportunistically here.

Phase B may change physical ownership only after this audit PR passes its Review Gate.