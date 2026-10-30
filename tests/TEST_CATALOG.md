# HyRemote Test Catalog

> Final #274 authority. Reconciled against `develop@bee774e76a48c7a23020b5386b42ac7a2bbb56f8` after Phase D. CMake/CTest remain execution authority; this catalog is the semantic/necessity authority for every CTest registered by the top-level product build.
>
> `tests/semantic_ctest_labels.cmake` contains a configure-time drift guard: every configured CTest identity must appear here as an exact Markdown code span, otherwise configuration fails. Standalone PRE projects under `tests/preflight/` are intentionally outside this product CTest inventory.
>
> **TS-1 measured pre-MERGE_CASES baseline:** 94 Qt 6.8.3 / 50 Qt 5.15.2 CTest identities,
> 43 test executables. The new registrations predict 79 / 37 identities if the same capabilities
> resolve; **these are projections, not measured evidence** until the canonical G2 run. TS-1 evidence:
> [test-inventory-ts1-2026-10-30.md](../docs/internal/test-inventory-ts1-2026-10-30.md).
> **When** to run what: [EXECUTION_MANUAL.md](EXECUTION_MANUAL.md). Numbers quoted in older sections below are history.

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
| `hyremote-runtime-diagnostics-test` | Runtime | Shared bounded diagnostic report formatting, explicit unknown/none semantics, host-path resolution facts with provenance (#591) and secret-safe single-line error rendering. Failure invalidates the common support report consumed by peer frontends. |
| `hyremote-security-descriptor-test` | Runtime | Descriptor/credential parsing and fail-closed configuration. |
| `hyremote-host-paths-test` | Runtime | #591 host path contract: explicit > environment > config-relative > platform default, fail-closed on an unusable explicit or environment selection, no working-directory fallback. |
| `hyremote-watermark-policy-test` | Runtime | #589 W1 static text watermark policy: Owner defaults (disabled, bottom-right single line, 0.35 opacity), bounded text/opacity/font-size/colour validation, UTF-8 text, and refusal of an explicitly enabled watermark that cannot be rendered. |
| `hyremote-watermark-compositor-test` | Runtime (Qt Gui) | #589 W2 server-side compositor: paints a copy of the frame, never the capture buffer; default bottom-right single line, explicit top-left and tiled layouts, 0.35 opacity blending, DPR scaling, CJK text, deterministic output and refusal of a malformed plane. |
| `hyremote-watermark-frame-decorator-test` | Runtime (Qt Gui) | #589 W2b transport hook: disabled policy installs nothing, composition preserves frame id/geometry/damage, the capture buffer stays byte-identical, the target DPR changes the pixels, and an unsupported format, missing storage or text that cannot fit are refused instead of streamed unwatermarked. |
| `hyremote-target-component-provider-test` | Runtime | Target→capture/input adapter selection. Failure invalidates adapter composition. |
| `hyremote-input-mailbox-admission-test` | Runtime | Bounded input admission/backpressure before GUI delivery. |
| `hyremote-rfb-multi-client-input-test` | VNC | Per-viewer held key/button ownership and disconnect cleanup. Failure allows one viewer to corrupt another viewer's input state. |
| `hyremote-rfb-wire-robustness-test` | VNC | Production-socket fragmentation plus malformed/oversized/unsupported input fail-closed behavior. Closes TG-013/TG-014. |
| `hyremote-vnc-auth-test` | VNC + transport security | VNC Authentication crypto primitive. Failure invalidates credential challenge/response correctness. **Condition:** registered only in a security-on build (`HYREMOTE_WITH_VNC` and the transport-security capability), so it is absent from the default product matrix. |
| `hyremote-rfb-vnc-auth-handshake-test` | VNC + transport security | Wire auth negotiation, wrong credential, no downgrade and bounded stalled auth. Failure invalidates authenticated RFB behavior. **Condition:** security-on build only, like the test above. |
| `hyremote-widgets-capture-test` | Widgets | Ordinary Widgets capture/lifetime/resize/cancellation/target-loss contract. |
| `hyremote-widgets-native-popup-capture-test` | Widgets (native Windows) | #404: a real QLineEdit::createStandardContextMenu() popup captured while it lies completely inside the root, so the framebuffer geometry cannot stand in for popup pixels; asserts a significant pixel change in the menu region and a return to baseline after close. |
| `hyremote-widgets-capture-forced-dpr-test` | Widgets | Executes the same capture contract at deterministic DPR=1.5. Closes TG-001; failure exposes HiDPI geometry/pixel mapping regression. |
| `hyremote-widgets-capture-dpr2-test` | Widgets | Executes the same Widgets capture and cursor-coordinate contract at deterministic DPR=2. |
| `hyremote-widgets-input-routing-test` | Widgets | Widgets input delivery/local coexistence. |
| `hyremote-widgets-input-backpressure-test` | Widgets | Bounded pointer admission without semantic-loss coalescing, protected releases and shutdown balancing under GUI pressure. |
| `hyremote-widgets-input-preflight-test` | Widgets | Behavioral acceptance for the shared Qt window-system pointer ingress: child routing, Qt-owned double-click classification, accepted-time queue-delay independence and wheel propagation. |
| `hyremote-rfb-widget-disconnect-backpressure-test` | Widgets + VNC | Real RFB disconnect cleanup while Widgets input delivery is saturated. TG-020 requires absence when VNC is unavailable. |
| `hyremote-rfb-input-preflight-test` | Widgets + VNC | Production RFB pointer facts reach the shared Qt window-system ingress; Qt owns interval/distance double-click classification. |
| `hyremote-quick-capture-test` | Quick | Ordinary Qt Quick capture/lifetime/resize/target-loss contract. |
| `hyremote-quick-capture-forced-dpr-test` | Quick | Executes the same Quick capture contract at deterministic DPR=1.5/software backend. Closes TG-002. |
| `hyremote-quick-capture-dpr2-test` | Quick | Executes the same Quick capture and cursor-coordinate contract at deterministic DPR=2/software backend. |
| `hyremote-quick-input-routing-test` | Quick | Qt Quick input delivery/local coexistence. |
| `hyremote-quick-input-backpressure-test` | Quick | Bounded Quick GUI dispatch/backpressure without semantic-loss pointer coalescing. |
| `hyremote-quick-input-preflight-test` | Quick | Behavioral acceptance that Qt Quick owns item hit testing, grabs and double-click classification below the shared Qt window-system ingress. |
| `hyremote-quick-controls-preflight-test` | Quick + QtQuick Controls | Quick control matrix (Button/CheckBox/Slider/Popup), registered only when the QtQuick.Controls QML module is available. It remains product coverage, not Runtime control-specific input logic. |
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
| `hyremote-qpa-runtime` | One CTest runs basic (plugin/ABI/config/OpenGL), surfaces (multi-window/popup), and recovery (auto-remoteaccess/native-survival) in their individual smoke executables and plugin environments. Named case failures remain fatal. |
| `hyremote-qpa-quick-multi-window-connection-smoke` | Multiple Quick windows through the QPA integration. Kept as its own CTest because its 500 ms startup contract is sensitive to being run inside a group. |

## T3 — repository/product contracts

| CTest identity | Necessity and failure meaning |
| --- | --- |
| `hyremote-build-authority-selftest` | Canonical `build.cmd`/CMake/build.yml behavior; repository-owned after B3. Failure makes build/bootstrap authority untrustworthy. |
| `hyremote-ci-scope-self-test` | CI classifier self-test. Failure can create false-green or over-broad lane selection. |
| `hyremote-mainline-audit-self-test` | Mainline audit retry/verification behavior. |
| `hyremote-branch-name-gate-self-test` | Branch-family governance remains executable rather than prose-only. |
| `hyremote-release-readiness-facts-layout` | Grouped, fail-closed set of 10 cases: release metadata, exact-cell matrix, GA reconciliation, Shared Runtime contract, release authority policy, repository layout, release docs layout, documentation paths, licensing, CI environment baseline. Each former assertion still executes and logs its case name. |

The configure-time catalog-drift assertion in `tests/semantic_ctest_labels.cmake` is also a T3 authority mechanism, but intentionally is not a CTest and therefore does not change inventory/counts.

## T4 — consumer / package / deploy

| CTest identity | Semantic owner / necessity |
| --- | --- |
| `hyremote-acquisition-audit-self-test` | Consumer/repository — proves clean-consumer cache auditing cannot hide source/build-tree acquisition. TG-012 closed. |
| `hyremote-cpp-installed-consumers` | Consumer/C++ — real installed Widgets+C++ and Quick+C++ lifecycle through a clean SDK. |
| `hyremote-generic-installed-consumers` | Consumer/Generic — real installed Generic Widgets/Quick with native platform identity. |
| `hyremote-qml-installed-consumer` | Consumer/QML — clean installed QML-only module is deployed, loaded and executes its shared diagnostic contract. |
| `hyremote-release-readiness-product-claims` | Grouped, fail-closed set of five cases: security/runtime deploy, build/install contract, deployment relocation, consumer simplicity, package-acquisition isolation. Each original script executes in its own function scope. |
| `hyremote-qpa-installed-product-fit` | Consumer/QPA — clean installed QPA payload/delegate chain reaches a live RFB listener and emits correlated shared diagnostics. |
| `hyremote-qml-deploy-helper-non-qml` | Consumer/QML — ordinary deploy dispatch remains non-QML. |
| `hyremote-qml-deploy-helper-qml` | Consumer/QML — QML-aware deploy dispatch/import-root preservation. |
| `hyremote-release-readiness-deploy-helper-contract` | Consumer — static deploy/package helper contract. |
| `hyremote-linux-qt-runtime-conflict` | Consumer/Linux deploy — deterministic same-SONAME Qt runtime conflict resolution stays within the selected Qt/deployed lineage and fails closed for ambiguous or foreign candidates. **Condition:** Linux-only; it does not exist on the Windows lanes. |
| `hyremote-release-readiness-source-qpa-authority` | Consumer/QPA — source-QPA deployment authority and negative metadata behavior. |

### QPA deploy-helper matrix — T4/QPA

All entries are distinct positive/negative deployment contracts; a failure means the exact deployment mode or rejection reason is no longer trustworthy:

- `hyremote-qpa-deploy-helper-positive` — One table-driven run for the positive deploy-helper matrix: ordinary, QML-only, QML-composed, installed-payload variants and the explicit single/multi-config Ninja generators when available.
- `hyremote-qpa-deploy-helper-negative` — One table-driven run for the fail-closed rejections: missing/stale QML payload, missing QML import root or module dir, stale QPA metadata, Qt-version mismatch and missing QPA package. Each case must match its expected failure fragment.
- `hyremote-qpa-source-payload-relocation` — Linux-only source-payload relocation contract.

`tests/consumer-installed-sdk` remains a distinct minimal release-evidence cell rather than a normal CTest: it proves the smallest public package/export/deploy closure and is not redundant with the stronger adapter consumers.

## T5 — adoption / product E2E

| CTest identity | Necessity and failure meaning |
| --- | --- |
| `hyremote-v01-example-smoke` | Installs the SDK then independently builds/runs canonical learning paths; proves developer adoption rather than protocol correctness. TG-021 guards registration by C++ API + Runtime + Python. |
| `hyremote-tool-rfb-client` | Headless fake RFB 3.8 server drives the real HyRemoteTool client core: handshake/None, Raw and DesktopSize frames, pointer/key encoding and FPS/byte accounting. Fails on protocol or telemetry regressions without opening a window. |
| `hyremote-tool-client-ui` | Constructs the real HyRemoteTool client window offscreen and verifies the connection bar, framebuffer view and telemetry surface exist; it must not connect or open a listener. |

`hyremote-v01-rfb-product-fit` is listed under C++ above because its current registration lives with the C++ maintained-viewer harness, but semantically it is T5/RFB E2E evidence.

Unregistered assets `tests/product-e2e/example_product_fit.py` and `qml_product_fit.py` remain explicit deferred P2 execution-ownership debt in `COVERAGE_GAPS.md`; source files without execution authority are not counted as evidence.

## T6 — release/candidate authority

| CTest identity | Necessity and failure meaning |
| --- | --- |
| `hyremote-release-preparation-correction` | Protected release-branch correction rule: re-basing onto the exact develop candidate is admitted; extra product drift, CMake changes beyond VERSION, or edited pending notes are rejected. |
| `hyremote-release-scope-self-test` | Exact release-train scope selection/rejection logic. |
| `hyremote-release-authority-v02-user-first` | V0.2 user-first release authority, including conditional secure-evidence semantics. |
| `hyremote-release-profiles` | One table-driven CTest for every release-profile case: development sentinel, retired pre-GA labels, progressive V0.1/V0.2/V0.3 profiles, and V1 all/C++/Generic subsets. Each case is named in the PASS/FAIL output; rejected cases must fail closed. |

The release-profile matrix intentionally uses separate CTest identities because each representable or retired profile is an independent fail-closed release-authority contract.

## Current registration reconciliation

Reference configuration: top-level tests, `cpp,qml,generic,qpa`, VNC enabled, Qt 6.8.3; transport security availability determines two conditional auth tests. The following numeric history predates MERGE_CASES and is not its acceptance result.

- current #335 exact-head security-enabled inventory: Linux **113**, Windows **112**;
- security-off equivalent: Linux **111**, Windows **110**; CI #581 executed those exact full-config counts;
- the one platform-only identity remains Linux `hyremote-qpa-source-payload-relocation`;
- the post-#358 graph includes `hyremote-runtime-diagnostics-test`, then #335 adds `hyremote-qml-installed-consumer` and `hyremote-qpa-installed-product-fit` cross-platform;
- the two security-only identities are `hyremote-vnc-auth-test` and `hyremote-rfb-vnc-auth-handshake-test`;
- #274 final reconciliation adds no CTest identity, so these counts remain unchanged.

The historical Phase-A/#300 95/94 inventory is retained only as a before-refactor baseline in `EXECUTION_BASELINE.md`; it is no longer current registration authority.

## Closed and deferred findings

Closed by the #274 workstream: TG-001/002/003/004/005/006/007/008/009/012/013/014/019/020/021. There is no remaining confirmed P0/P1 gap.

Deferred P2 findings TG-010/011/015/016/017/018 retain explicit owner/rationale in `COVERAGE_GAPS.md`. They are intentionally not implementation scope for this closeout.

## PRE — outside normal inventory

`tests/preflight/` remains a standalone CMake project for bounded technical-risk evidence (currently TLS transition/VeNCrypt feasibility). PRE results do not count toward product CTest registration and do not replace product/release acceptance.
