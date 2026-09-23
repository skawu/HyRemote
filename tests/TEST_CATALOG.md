# HyRemote Test Catalog

> Final #274 authority. Reconciled against `develop@bee774e76a48c7a23020b5386b42ac7a2bbb56f8` after Phase D. CMake/CTest remain execution authority; this catalog is the semantic/necessity authority for every CTest registered by the top-level product build.
>
> `tests/semantic_ctest_labels.cmake` contains a configure-time drift guard: every configured CTest identity must appear here as an exact Markdown code span, otherwise configuration fails. Standalone PRE projects under `tests/preflight/` are intentionally outside this product CTest inventory.

## Taxonomy

| Layer | Meaning | Normal owner |
| --- | --- | --- |
| T1 | deterministic module/component/private-seam behavior | Core, Runtime, RFB, Widgets/Quick adapters |
| T2 | behavior unique to one peer integration frontend | C++, QML, Generic, QPA |
| T3 | repository/product contract | build/layout/CI/public-contract governance |
| T4 | external consumer/package/deploy/acquisition | clean consumers and deployment |
| T5 | user/adoption/product end-to-end | examples and maintained-viewer product fit |
| T6 | release/candidate truth | release scope/profile/readiness |
| PRE | bounded technical-decision preflight | standalone preflight project/workflow |

All currently registered tests are `KEEP`. Historical `MOVE`/`SPLIT` decisions were completed during Phase B; their current semantic owner is shown below. Capability guards remain registration authority and labels never replace them.

## T1 — Core

| CTest identity | Owner / labels | Necessity and failure meaning |
| --- | --- | --- |
| `hyremote-core-test-frame-lifetime` | Core / unit-fast | Proves asynchronous frame storage ownership. Failure makes captured frame lifetime unsafe. |
| `hyremote-core-test-damage` | Core / unit-fast | Proves damage tri-state/region semantics. Failure makes incremental capture correctness untrustworthy. |
| `hyremote-core-test-timing` | Core / unit-fast | Proves PTS/capture scheduling semantics. Failure invalidates timing/order assumptions. |
| `hyremote-core-test-mailbox` | Core / component-fast | Proves bounded queue/backpressure/drop accounting. Failure invalidates bounded-resource behavior. |
| `hyremote-core-test-transport-handoff` | Core / component-fast | Proves Core↔transport frame/input/event handoff. Failure invalidates the module boundary. |
| `hyremote-core-test-session-lifecycle` | Core / integration-fast | Proves lifecycle, races, partial-start cleanup, stop ownership and deterministic teardown. Failure invalidates Session state/concurrency safety. |
| `hyremote-core-test-session-defaults` | Core / unit-fast | Proves safe valid defaults. Failure makes default construction/configuration unsafe. |
| `hyremote-core-test-input-routing` | Core / component-fast | Proves configured sink/lifecycle routing. Failure invalidates backend-neutral input delivery. |
| `hyremote-core-test-input-normalization` | Core / unit-fast | Proves backend-independent input vocabulary. Failure invalidates cross-transport input semantics. |
| `hyremote-core-test-callback-lifetime` | Core / component-fast | Proves callbacks cannot outlive stop/destruction. Failure exposes use-after-lifetime risk. |
| `hyremote-core-test-callback-exception-boundary` | Core / component-fast | Proves backend callback exceptions do not terminate the host. Failure invalidates exception containment. |
| `hyremote-core-test-dependency-boundary` | Core / contract-fast | Proves Core remains protocol/GUI/platform neutral. Failure invalidates architecture dependency direction. |

## T1 — Shared Runtime / RFB / adapters

| CTest identity | Guard / semantic owner | Necessity and failure meaning |
| --- | --- | --- |
| `hyremote-runtime-listener-binding-test` | Runtime | Deterministic IPv4 bind-mode/interface/locality decisions from injected interface facts. Failure invalidates listener policy independently of real sockets. |
| `hyremote-runtime-automatic-surface-model-test` | automatic Runtime available | Automatic surface discovery/model contract. |
| `hyremote-runtime-automatic-composite-capture-test` | automatic Runtime available | Composite geometry/capture contract. |
| `hyremote-runtime-automatic-composite-input-test` | automatic Runtime available | Composite input routing contract. |
| `hyremote-runtime-notifications-test` | automatic Runtime available | Typed Runtime state/client/error notification ordering on real event boundaries. Failure invalidates frontend observability source semantics. |
| `hyremote-security-descriptor-test` | Runtime | Descriptor/credential parsing and fail-closed configuration. |
| `hyremote-target-component-provider-test` | Runtime | Target→capture/input adapter selection. Failure invalidates adapter composition. |
| `hyremote-input-mailbox-admission-test` | Runtime | Bounded input admission/backpressure before GUI delivery. |
| `hyremote-rfb-multi-client-input-test` | VNC | Per-viewer held key/button ownership and disconnect cleanup. Failure allows one viewer to corrupt another viewer's input state. |
| `hyremote-rfb-wire-robustness-test` | VNC | Production-socket fragmentation plus malformed/oversized/unsupported input fail-closed behavior. Closes TG-013/TG-014. |
| `hyremote-vnc-auth-test` | VNC + transport security | VNC Authentication crypto primitive. Failure invalidates credential challenge/response correctness. |
| `hyremote-rfb-vnc-auth-handshake-test` | VNC + transport security | Wire auth negotiation, wrong credential, no downgrade and bounded stalled auth. Failure invalidates authenticated RFB behavior. |
| `hyremote-widgets-capture-test` | Widgets | Ordinary Widgets capture/lifetime/resize/cancellation/target-loss contract. |
| `hyremote-widgets-capture-forced-dpr-test` | Widgets | Executes the same capture contract at deterministic DPR=1.5. Closes TG-001; failure exposes HiDPI geometry/pixel mapping regression. |
| `hyremote-widgets-input-routing-test` | Widgets | Widgets input delivery/local coexistence. |
| `hyremote-widgets-input-backpressure-test` | Widgets | Pointer coalescing, protected releases and shutdown balancing under GUI pressure. |
| `hyremote-rfb-widget-disconnect-backpressure-test` | Widgets + VNC | Real RFB disconnect cleanup while Widgets input delivery is saturated. TG-020 requires absence when VNC is unavailable. |
| `hyremote-quick-capture-test` | Quick | Ordinary Qt Quick capture/lifetime/resize/target-loss contract. |
| `hyremote-quick-capture-forced-dpr-test` | Quick | Executes the same Quick capture contract at deterministic DPR=1.5/software backend. Closes TG-002. |
| `hyremote-quick-input-routing-test` | Quick | Qt Quick input delivery/local coexistence. |
| `hyremote-quick-input-backpressure-test` | Quick | Bounded Quick GUI dispatch/backpressure. |
| `hyremote-rfb-listener-reachability-test` | Widgets + VNC | Real Runtime/RFB wildcard/explicit IPv4 reachability and interface reconciliation. Failure invalidates socket-level reachability while remaining independent of the C++ facade. |

## T2 — C++ frontend

| CTest identity | Capability | Necessity and failure meaning |
| --- | --- | --- |
| `hyremote-remoteaccess-test` | C++ API | Public defaults/config/lifecycle/move/client-count/input/error/fail-closed facade behavior. Failure invalidates the public C++ embedding contract. |
| `hyremote-remoteaccess-error-ack-test` | C++ API | Public diagnostic acknowledgement/reappearance semantics. |
| `hyremote-remoteaccess-target-loss-test` | C++ API | Public facade behavior when the target is destroyed. |
| `hyremote-listener-address-matrix-test` | C++ API + Widgets | Public listener lifecycle/error/configuration including IPv6 fail-closed behavior. Runtime/RFB reachability rows were split out in B4. |
| `hyremote-v01-rfb-product-fit` | C++ API + VNC + Python/viewer tooling | Maintained-viewer framebuffer/input/reconnect/timeout/held-input product-fit evidence. It keeps the special `candidate-evidence` label in addition to semantic labels. |

## T2 — QML frontend

| CTest identity | Necessity and failure meaning |
| --- | --- |
| `hyremote-qml-module-test` | Import/type registration, safe defaults, transactional enable/config/error and target lifetime. Failure invalidates the public declarative wrapper. |
| `hyremote-qml-notifications-test` | Proves QML observes the shared Runtime's typed state/client/error notifications on the object thread. Failure invalidates declarative parity without duplicating Runtime mechanics. |

## T2 — Generic frontend

| CTest identity | Necessity and failure meaning |
| --- | --- |
| `hyremote-generic-plugin-smoke` | Zero-code activation through public Qt plugin APIs while native QPA stays selected. |
| `hyremote-generic-config-test` | Generic launch-syntax/config mapping independent of the plugin loader. Failure invalidates Generic configuration interpretation. |

## T2 — QPA frontend

| CTest identity | Necessity and failure meaning |
| --- | --- |
| `hyremote-qpa-proxy-smoke` | Platform-plugin load/delegate path. |
| `hyremote-qpa-native-semantics` | Exact-private-ABI native delegate semantics. |
| `hyremote-qpa-remote-config-test` | QPA launch/config vocabulary. |
| `hyremote-qpa-auto-remoteaccess-smoke` | QPA starts the same Shared Runtime. |
| `hyremote-qpa-remote-failure-native-survival-smoke` | Remote failure preserves the native/local application. |
| `hyremote-qpa-multi-surface-connection-smoke` | Multiple QWidget surfaces share the intended remote session/composite semantics. |
| `hyremote-qpa-widget-popup-connection-smoke` | Popup/transient surface behavior with bounded event-driven synchronization; TG-019 closed. |
| `hyremote-qpa-widget-opengl-capture-smoke` | Conditional QOpenGLWidget classification/capture. |
| `hyremote-qpa-quick-multi-window-connection-smoke` | Multiple Quick windows through the QPA integration. |

## T3 — repository/product contracts

| CTest identity | Necessity and failure meaning |
| --- | --- |
| `hyremote-build-authority-selftest` | Canonical `build.cmd`/CMake/build.yml behavior; repository-owned after B3. Failure makes build/bootstrap authority untrustworthy. |
| `hyremote-ci-scope-self-test` | CI classifier self-test. Failure can create false-green or over-broad lane selection. |
| `hyremote-mainline-audit-self-test` | Mainline audit retry/verification behavior. |
| `hyremote-branch-name-gate-self-test` | Branch-family governance remains executable rather than prose-only. |
| `hyremote-release-readiness-runtime-contract` | Durable Runtime architecture/dependency contract; semantically T3 despite legacy physical path. |
| `hyremote-release-readiness-repository-layout` | Canonical source/dependency layout. |
| `hyremote-release-readiness-documentation-paths` | Maintained Markdown paths resolve. |
| `hyremote-release-readiness-ci-environment-baseline` | Hosted/reference environment assumptions remain explicit. |
| `hyremote-release-readiness-licensing-boundary` | Durable dependency/license boundary. |

The configure-time catalog-drift assertion in `tests/semantic_ctest_labels.cmake` is also a T3 authority mechanism, but intentionally is not a CTest and therefore does not change inventory/counts.

## T4 — consumer / package / deploy

| CTest identity | Semantic owner / necessity |
| --- | --- |
| `hyremote-acquisition-audit-self-test` | Consumer/repository — proves clean-consumer cache auditing cannot hide source/build-tree acquisition. TG-012 closed. |
| `hyremote-cpp-installed-consumers` | Consumer/C++ — real installed Widgets+C++ and Quick+C++ lifecycle through a clean SDK. |
| `hyremote-generic-installed-consumers` | Consumer/Generic — real installed Generic Widgets/Quick with native platform identity. |
| `hyremote-qml-deploy-helper-non-qml` | Consumer/QML — ordinary deploy dispatch remains non-QML. |
| `hyremote-qml-deploy-helper-qml` | Consumer/QML — QML-aware deploy dispatch/import-root preservation. |
| `hyremote-release-readiness-security-runtime-deploy` | Consumer — deployed Runtime/security closure and runtime dependency availability. |
| `hyremote-release-readiness-build-install-contract` | Consumer — promised build/install entry-point contract. |
| `hyremote-release-readiness-deployment-relocation` | Consumer — installed/deployed payload remains relocatable. |
| `hyremote-release-readiness-deploy-helper-contract` | Consumer — static deploy/package helper contract. |
| `hyremote-release-readiness-consumer-simplicity` | Consumer — public consumption remains bounded/simple. |
| `hyremote-release-readiness-package-acquisition-isolation` | Consumer — package acquisition stays isolated from product source/build tree. |
| `hyremote-release-readiness-source-qpa-authority` | Consumer/QPA — source-QPA deployment authority and negative metadata behavior. |

### QPA deploy-helper matrix — T4/QPA

All entries are distinct positive/negative deployment contracts; a failure means the exact deployment mode or rejection reason is no longer trustworthy:

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
- `hyremote-qpa-source-payload-relocation` — Linux-only source-payload relocation contract.

`tests/consumer-installed-sdk` remains a distinct minimal release-evidence cell rather than a normal CTest: it proves the smallest public package/export/deploy closure and is not redundant with the stronger adapter consumers.

## T5 — adoption / product E2E

| CTest identity | Necessity and failure meaning |
| --- | --- |
| `hyremote-v01-example-smoke` | Installs the SDK then independently builds/runs canonical learning paths; proves developer adoption rather than protocol correctness. TG-021 guards registration by C++ API + Runtime + Python. |

`hyremote-v01-rfb-product-fit` is listed under C++ above because its current registration lives with the C++ maintained-viewer harness, but semantically it is T5/RFB E2E evidence.

Unregistered assets `tests/product-e2e/example_product_fit.py`, `qml_product_fit.py`, and `showcase_product_fit.py` remain explicit deferred P2 execution-ownership debt in `COVERAGE_GAPS.md`; source files without execution authority are not counted as evidence.

## T6 — release/candidate authority

| CTest identity | Necessity and failure meaning |
| --- | --- |
| `hyremote-release-readiness-metadata` | Candidate/version/security metadata truth. |
| `hyremote-release-readiness-release-authority-policy` | Release authority policy remains fail-closed and consistent. |
| `hyremote-release-readiness-release-documentation-layout` | Candidate/release documentation layout required by delivery authority. |
| `hyremote-release-scope-self-test` | Exact release-train scope selection/rejection logic. |
| `hyremote-release-authority-v02-user-first` | V0.2 user-first release authority, including conditional secure-evidence semantics. |
| `hyremote-release-profile-develop-all` | Development all-capability profile acceptance. |
| `hyremote-release-profile-develop-runtime-only` | Development runtime-only profile acceptance. |
| `hyremote-release-profile-retire-v001` | Retired pre-GA label rejection. |
| `hyremote-release-profile-retire-v002` | Retired pre-GA label rejection. |
| `hyremote-release-profile-retire-v003` | Retired pre-GA label rejection. |
| `hyremote-release-profile-v010-cpp-only` | V0.1 representable C++ subset. |
| `hyremote-release-profile-v020-runtime` | V0.2 representable Runtime profile. |
| `hyremote-release-profile-v030-all` | V0.3 all-capability profile. |
| `hyremote-release-profile-v040-all` | V0.4 all-capability profile. |
| `hyremote-release-profile-v040-maintenance` | V0.4 maintenance-line acceptance. |
| `hyremote-release-profile-v100-all` | V1 all-capability profile. |
| `hyremote-release-profile-v100-cpp-only` | V1 C++ subset profile. |
| `hyremote-release-profile-v100-generic-only` | V1 Generic subset profile. |

The release-profile matrix intentionally uses separate CTest identities because each representable or retired profile is an independent fail-closed release-authority contract.

## Current registration reconciliation

Reference configuration: top-level tests, `cpp,qml,generic,qpa`, VNC enabled, Qt 6.8.3; transport security availability determines two conditional auth tests.

- final Phase-D #358 exact-head security-enabled inventory: Linux **108**, Windows **107**;
- security-off equivalent: Linux **106**, Windows **105**;
- the one platform-only identity is Linux `hyremote-qpa-source-payload-relocation`;
- the two security-only identities are `hyremote-vnc-auth-test` and `hyremote-rfb-vnc-auth-handshake-test`;
- #274 final reconciliation adds no CTest identity, so these counts remain unchanged.

The historical Phase-A/#300 95/94 inventory is retained only as a before-refactor baseline in `EXECUTION_BASELINE.md`; it is no longer current registration authority.

## Closed and deferred findings

Closed by the #274 workstream: TG-001/002/003/004/005/006/007/008/009/012/013/014/019/020/021. There is no remaining confirmed P0/P1 gap.

Deferred P2 findings TG-010/011/015/016/017/018 retain explicit owner/rationale in `COVERAGE_GAPS.md`. They are intentionally not implementation scope for this closeout.

## PRE — outside normal inventory

`tests/preflight/` remains a standalone CMake project for bounded technical-risk evidence (currently TLS transition/VeNCrypt feasibility). PRE results do not count toward product CTest registration and do not replace product/release acceptance.
