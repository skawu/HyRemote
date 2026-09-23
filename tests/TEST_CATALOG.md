# HyRemote Test Catalog

> Phase A authority for #274. Repository facts refreshed against `develop@e3fcf2fd06f8feca00b83b6c49c48588264cfd81`.
>
> This catalog answers **why each durable test exists, who owns the protected contract, and what Phase A decided to do with it**. CMake/CTest/workflows remain execution authority. `EXECUTION_BASELINE.md` records what is actually registered/executed.

## Taxonomy and dispositions

| Layer | Meaning | Typical owner |
| --- | --- | --- |
| T1 | deterministic module/component/private-seam behavior | Core, Runtime, RFB, Widgets/Quick adapters |
| T2 | behavior unique to one peer integration frontend | C++, QML, Generic, QPA |
| T3 | repository/product contract independent of one candidate | build/layout/CI/public-contract governance |
| T4 | external consumer/package/deploy/acquisition contract | installed/source consumers and deployment |
| T5 | user/adoption/product end-to-end flow | examples and maintained-viewer product fit |
| T6 | release/candidate-specific truth | release scope/profile/readiness/candidate evidence |
| PRE | bounded technical-decision preflight | release-entry spike, not normal product regression |

Dispositions: `KEEP`, `MOVE`, `SPLIT`, `MERGE`, `RETIRE`, `GAP`. Phase A leaves no unresolved `REVIEW` item.

## T1 — Core: KEEP

All of the following remain Core-owned because a failure invalidates transport/UI-neutral lifetime, timing, queue, input, callback or Session semantics:

- `hyremote-core-test-frame-lifetime` — asynchronous frame ownership/lifetime.
- `hyremote-core-test-damage` — damage tri-state/region semantics.
- `hyremote-core-test-timing` — PTS/capture scheduling semantics.
- `hyremote-core-test-mailbox` — bounded queue/backpressure/drop accounting.
- `hyremote-core-test-transport-handoff` — Core↔transport frame/input/event handoff.
- `hyremote-core-test-session-lifecycle` — state machine, failures, races and deterministic teardown.
- `hyremote-core-test-session-defaults` — safe valid default SessionConfig.
- `hyremote-core-test-input-routing` — configured sink/lifecycle routing.
- `hyremote-core-test-input-normalization` — backend-independent input vocabulary.
- `hyremote-core-test-callback-lifetime` — callbacks cannot outlive stop/destruction.
- `hyremote-core-test-callback-exception-boundary` — backend exceptions do not terminate host.
- `hyremote-core-test-dependency-boundary` — Core remains Qt-GUI/protocol/platform neutral.

`hyremote-core-test-session-lifecycle` contains many related concurrency regressions by design. File size alone is not a reason to split it.

## T1 — Shared Runtime / RFB / adapters

### Correctly Runtime-owned: KEEP

- `hyremote-runtime-automatic-surface-model-test` — automatic surface discovery/model.
- `hyremote-runtime-automatic-composite-capture-test` — composite geometry/capture.
- `hyremote-runtime-automatic-composite-input-test` — composite input routing.
- `hyremote-runtime-listener-binding-test` — #338 deterministic IPv4 binding mode/interface resolution/locality contract without real listener I/O.

### Phase-A MOVE/SPLIT set and later closure status

| Test | Phase-A decision / current status | Necessity / failure meaning |
| --- | --- | --- |
| `hyremote-security-descriptor-test` | MOVE Runtime/security | descriptor/credential parsing and fail-closed configuration |
| `hyremote-vnc-auth-test` | MOVE Runtime/RFB | VNC authentication crypto primitive; conditional on Security |
| `hyremote-rfb-vnc-auth-handshake-test` | MOVE Runtime/RFB | wire negotiation/auth/no-downgrade/timeout; conditional on Security |
| `hyremote-rfb-multi-client-input-test` | MOVE Runtime/RFB | per-viewer held key/button ownership and disconnect cleanup |
| `hyremote-input-mailbox-admission-test` | MOVE Runtime | bounded input admission/backpressure before GUI delivery |
| `hyremote-target-component-provider-test` | MOVE Runtime/adapters | target→capture/input adapter selection |
| `hyremote-widgets-capture-test` | MOVE Runtime/Widgets | Widgets capture/lifetime/resize; forced-DPR gap TG-001 |
| `hyremote-widgets-input-routing-test` | MOVE Runtime/Widgets | Widgets input delivery |
| `hyremote-widgets-input-backpressure-test` | MOVE Runtime/Widgets | bounded GUI dispatch and protected releases |
| `hyremote-quick-capture-test` | MOVE Runtime/Quick | Quick capture/lifetime/resize; forced-DPR gap TG-002 |
| `hyremote-quick-input-routing-test` | MOVE Runtime/Quick | Quick input delivery |
| `hyremote-quick-input-backpressure-test` | MOVE Runtime/Quick | bounded Quick GUI dispatch |
| `hyremote-rfb-widget-disconnect-backpressure-test` | MOVE Runtime/RFB+Widgets; TG-020 CLOSED by #327/#328 | disconnect cleanup under saturation; registers only with Widgets + VNC and is absent when VNC is unavailable |
| `hyremote-listener-address-matrix-test` | SPLIT / KEEP T2 identity after B4 | public RemoteAccess lifecycle/error mapping plus IPv6 fail-closed configuration stay C++-owned |
| `hyremote-rfb-listener-reachability-test` | ADD Runtime/RFB identity from B4 split | real wildcard/explicit IPv4 reachability and interface reconciliation, independent of the C++ frontend |

The listener split after #338 is deliberately three proof layers: `hyremote-runtime-listener-binding-test` owns deterministic private binding decisions; `hyremote-listener-address-matrix-test` owns public C++ configuration/error/lifecycle behavior; `hyremote-rfb-listener-reachability-test` owns real Runtime/RFB socket reachability/reconciliation. The B4 split adds exactly one CTest identity and duplicates no row. Pre-#338 IPv6 reachability/dual-stack scenarios are obsolete because IPv6 is now an explicit public rejection contract.

Support code `rfb_test_server.cpp` and its maintained-viewer harness are T5 rather than C++ facade behavior.

## T2 — C++ frontend: KEEP facade-only contracts

- `hyremote-remoteaccess-test` — public defaults/config/lifecycle/move ownership/client count/input opt-in/errors/fail-closed encrypted-profile facade behavior.
- `hyremote-remoteaccess-error-ack-test` — public diagnostic acknowledgement/reappearance.
- `hyremote-remoteaccess-target-loss-test` — public facade behavior when target is destroyed.
- `hyremote-listener-address-matrix-test` — public listener lifecycle/error/configuration contract after B4; no private RFB reachability rows remain here.

These may use controlled Runtime seams but must not remain the default home for RFB/security/adapter internals.

## T2 — QML frontend

- `hyremote-qml-module-test` — **KEEP**: import/type registration, safe defaults, transactional enabled/config/error and target lifetime.
- `hyremote-qml-deploy-helper-non-qml` — **MOVE T4**: ordinary deploy dispatch and unchanged QML import path.
- `hyremote-qml-deploy-helper-qml` — **MOVE T4**: QML-aware deploy dispatch/import-root preservation.

The module test, installed QML proof and viewer E2E are distinct T2/T4/T5 layers.

## T2 — Generic frontend

- `hyremote-generic-plugin-smoke` — **KEEP**: zero-code activation through public Qt plugin APIs while native QPA remains selected.

Installed Generic Widgets/Quick tests remain separate T4 contracts because in-tree activation cannot prove clean package acquisition/deployment.

## T2 — QPA frontend

All remain **KEEP** because they protect behavior unique to the private-ABI delegate frontend:

- `hyremote-qpa-proxy-smoke` — platform-plugin load/delegate path.
- `hyremote-qpa-native-semantics` — exact-private-ABI native delegate semantics.
- `hyremote-qpa-remote-config-test` — QPA launch/config vocabulary.
- `hyremote-qpa-auto-remoteaccess-smoke` — QPA starts the same Shared Runtime.
- `hyremote-qpa-remote-failure-native-survival-smoke` — remote failure preserves native/local app.
- `hyremote-qpa-multi-surface-connection-smoke` — multiple QWidget surfaces.
- `hyremote-qpa-widget-popup-connection-smoke` — popup/transient surface behavior. TG-019 is **CLOSED** by #282/#296; synchronization is now bounded/condition-driven.
- `hyremote-qpa-widget-opengl-capture-smoke` — conditional QOpenGLWidget classification/capture.
- `hyremote-qpa-quick-multi-window-connection-smoke` — multiple Quick windows.

QPA deployment/configuration fixtures below are T4, not T2 runtime behavior.

## T3 — repository/product contracts

| Test/fixture | Decision | Necessity |
| --- | --- | --- |
| `hyremote-build-authority-selftest` | MOVE T3 | canonical `build.cmd`/CMake/build.yml authority; B3 moves registration out of Runtime ownership without semantic change |
| `hyremote-ci-scope-self-test` | KEEP | classifier cannot silently select a false-green/incorrect lane |
| `hyremote-mainline-audit-self-test` | KEEP | mainline audit retry/verification logic executes deterministically |
| `hyremote-branch-name-gate-self-test` | KEEP | branch-family governance is executable |
| `tests/public-api-contract` | KEEP | exported target/API/dependency shape; TG-018 is wording debt only |
| `hyremote-release-readiness-runtime-contract` | SPLIT | permanent architecture/dependency invariants move T3; candidate-only residue remains T6 |
| `hyremote-release-readiness-repository-layout` | MOVE T3 | canonical source/dependency layout |
| `hyremote-release-readiness-documentation-paths` | MOVE T3 | maintained Markdown links resolve |
| `hyremote-release-readiness-ci-environment-baseline` | MOVE T3 | hosted/reference CI environment truth |
| `hyremote-release-readiness-licensing-boundary` | MOVE T3 | durable dependency/license boundary |

## T4 — consumer / package / deploy

### Clean consumers and persistent package contracts

- `hyremote-acquisition-audit-self-test` — **KEEP**; #280 regression for per-cache-element source/build/run-prefix isolation (TG-012 CLOSED).
- `hyremote-cpp-installed-consumers` — **KEEP**; real installed Widgets+C++ and Quick+C++ lifecycle.
- `hyremote-generic-installed-consumers` — **KEEP**; real installed Generic Widgets/Quick and native platform identity.
- `tests/consumer-installed-sdk` / `installed-sdk` cell — **KEEP, rename later**; smallest public package/export/deploy closure, distinct from adapter apps.
- `tests/consumer-source` / `source-consumer` — **KEEP**; external add_subdirectory/source acquisition.
- installed QML evidence — **KEEP preview**; package/import/deploy closure.
- installed QPA evidence — **KEEP preview**; exact-private-ABI package/deploy behavior.
- `hyremote-release-readiness-deployment-relocation` — **MOVE T4**.
- `hyremote-release-readiness-deploy-helper-contract` — **MOVE T4**; static deploy/package contract.
- `hyremote-release-readiness-consumer-simplicity` — **MOVE T4**.
- `hyremote-release-readiness-package-acquisition-isolation` — **MOVE T4**.
- `hyremote-release-readiness-source-qpa-authority` — **MOVE T4**.

### QPA deploy-helper matrix: all KEEP semantics, MOVE T4/QPA deploy owner

Each entry protects a distinct source/installed/configuration or negative failure reason:

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
- `hyremote-qpa-source-payload-relocation` — Linux-only by explicit platform guard.

Static scan, configure-negative matrix, installed fixture and exact-SHA release evidence are different proof layers; none is a deletion candidate merely to shorten CI.

## T5 — adoption and product E2E

| Test/asset | Decision | Necessity / current authority |
| --- | --- | --- |
| `hyremote-v01-example-smoke` | KEEP | installs SDK then builds/runs canonical 01/02/03 adoption paths. Since #300 it registers only with C++ API + Runtime target + Python; TG-021 CLOSED |
| `hyremote-v01-rfb-product-fit` / `rfb_product_fit.py` | KEEP contract, MOVE semantic owner | maintained-viewer framebuffer/input/reconnect/timeout/held-input proof; now registered as `candidate-evidence` by #281 and also retains mainline hosted authority. TG-009 CLOSED; #229 completed |
| `tests/product-e2e/example_product_fit.py` | KEEP | stronger app-level view-only→control/input/reconnect semantics; retarget historical labels to canonical fixtures before activation |
| `tests/product-e2e/qml_product_fit.py` | KEEP | QML observable client/view-only/control/reconfigure/reconnect flow; explicit non-fast owner still needed |
| `tests/product-e2e/showcase_product_fit.py` | KEEP | showcase-specific client/input/reconnect lifecycle; non-fast, not a V0.1 primary gate |

A source file without an execution authority is not evidence. The last three scripts remain assets, not current hosted coverage.

## T6 — release/candidate authority

The following remain **KEEP** because each guards release selection or candidate truth rather than ordinary product mechanics:

- `hyremote-release-readiness-metadata` — **SPLIT** permanent assertions to T3/T4, candidate/version/security truth remains T6.
- `hyremote-release-readiness-release-authority-policy`
- `hyremote-release-readiness-release-documentation-layout`
- `hyremote-release-scope-self-test`
- `hyremote-release-profile-develop-all`
- `hyremote-release-profile-develop-runtime-only`
- `hyremote-release-profile-retire-v001`
- `hyremote-release-profile-retire-v002`
- `hyremote-release-profile-retire-v003`
- `hyremote-release-profile-v010-cpp-only`
- `hyremote-release-profile-v020-runtime`
- `hyremote-release-profile-v030-all`
- `hyremote-release-profile-v040-all`
- `hyremote-release-profile-v040-maintenance`
- `hyremote-release-profile-v100-all`
- `hyremote-release-profile-v100-cpp-only`
- `hyremote-release-profile-v100-generic-only`

The release-profile matrix is intentionally many CTests: each representable/retired version is an independent fail-closed release-authority contract.

## PRE — V0.2 technical preflight evidence

`tests/preflight/` is a **standalone CMake project**, deliberately outside the normal product test graph:

- `hyremote-tls-transition-preflight` — Qt 6.8.3/OpenSSL same-socket plaintext→TLS transition, TLS>=1.2, cert prevalidation, bounded handshake timeout, reconnect/shutdown and no fallback.
- `run_vencrypt_interop_probe.py` / `vencrypt_interop_spike.py` — bounded maintained-viewer/protocol feasibility probes used by #258.
- `.github/workflows/tls-preflight.yml` — Win/Linux evidence authority for the OpenSSL transition spike.

Decision: **KEEP as bounded preflight evidence**. Do not count it in the normal 95/94 product CTest inventory and do not turn it into a second product build/test system.

## Current registration authority and closed findings

The Phase-A default all-frontends/transport-security-off baseline was **Linux 95 / Windows 94**. Linux-only QPA relocation explained the one-name platform difference at that baseline. Later focused slices add explained identities; security-enabled configurations additionally register `hyremote-vnc-auth-test` and `hyremote-rfb-vnc-auth-handshake-test`.

Closed during Phase A or later #274 slices:
- TG-009 — candidate maintained-viewer RFB evidence binding, closed #281/#229.
- TG-012 — clean-consumer acquisition false-pass, closed #280/#230.
- TG-019 — QPA popup timing instability, closed #282/#296.
- TG-020 — RFB+Widgets test under-guarding, closed #327/#328 with registration/build-time Widgets + VNC capability guard.
- TG-021 — V0.1 adoption smoke registered in C++-disabled lanes, closed #298/#300 and proved by #296 qpa-only evidence.

Still open and owned by later #274 phases include TG-001/002, TG-003/004/006/007, TG-010/011 and TG-013–018. See `COVERAGE_GAPS.md`.
