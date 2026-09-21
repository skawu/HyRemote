# HyRemote Test Catalog

> Phase A audit for #274. Baseline: `develop` at `97083f874491c8c4987856340c83c4157c7d3d6f`.
>
> This file is an ownership/necessity catalog, not a new execution authority. Phase A does not move, delete, rename or reselect tests. Matrix-dependent tests remain guarded by their existing capabilities.

## 1. Taxonomy

| Layer | Meaning | Typical owner |
| --- | --- | --- |
| T1 | deterministic module/component behavior, including private implementation seams | Core, Runtime, RFB backend, Widgets/Quick Runtime adapters |
| T2 | behavior unique to one peer integration frontend | C++, QML, Generic, QPA |
| T3 | repository/product contract independent of one release candidate | build authority, source layout, CI classifier, public API shape |
| T4 | external consumer/package/deploy contract | installed/source consumer, relocation, deployment |
| T5 | user/adoption/product end-to-end flow | examples, product-fit harnesses |
| T6 | release/candidate-specific authority and qualification | release profile/scope/metadata/readiness |

Decision values used in Phase A:

- `KEEP`: correct purpose and no proven redundant replacement.
- `MOVE`: necessary test, but semantic owner/location is wrong.
- `SPLIT`: one test currently protects multiple unrelated contracts that need clearer ownership/diagnostics.
- `MERGE`: distinct file/cell exists, but its contract is already covered more strongly elsewhere.
- `RETIRE`: no unique necessary contract remains.
- `REVIEW`: evidence is insufficient to choose KEEP/MOVE/MERGE/RETIRE yet.

A test is necessary only when it protects observable product behavior, an expensive architecture/security invariant, a known regression, external install/deploy behavior, real user/interoperability flow, or release truth.

## 2. Core — T1

All Core tests are correctly owned under `src/core/tests`. They are host-runnable and protect transport/UI/platform-neutral invariants.

| CTest / executable | Decision | Necessity / protected contract | Failure meaning |
| --- | --- | --- | --- |
| `hyremote-core-test-frame-lifetime` | KEEP | `RemoteFrame` storage/lifetime survives asynchronous handoff without dangling buffers | Core frame ownership contract is unsafe |
| `hyremote-core-test-damage` | KEEP | damage tri-state/region semantics remain deterministic | future incremental delivery can send wrong regions |
| `hyremote-core-test-timing` | KEEP | PTS/capture scheduling/timing semantics remain bounded | pacing and observability become unreliable |
| `hyremote-core-test-mailbox` | KEEP | bounded queue/drop/backpressure mechanics | memory/resource bound or ordering contract regresses |
| `hyremote-core-test-transport-handoff` | KEEP | Core-to-transport frame/input/event handoff semantics | Runtime backends can observe invalid ordering/lifetime |
| `hyremote-core-test-session-lifecycle` | KEEP | state machine, fault escalation, deterministic teardown, concurrent stop/start edges, counters | one Shared Runtime cannot rely on deterministic Core lifecycle |
| `hyremote-core-test-session-defaults` | KEEP | default `SessionConfig` and contract-safe defaults | callers can enter invalid/unsafe behavior without opting in |
| `hyremote-core-test-input-routing` | KEEP | transport input reaches only the configured Core input sink under bounded lifecycle | remote input can be dropped/misrouted after composition |
| `hyremote-core-test-input-normalization` | KEEP | normalized input representation is stable across backends | adapters/backends disagree on input semantics |
| `hyremote-core-test-callback-lifetime` | KEEP | callbacks do not outlive/dangle across stop/destruction | use-after-free / post-stop callback risk |
| `hyremote-core-test-callback-exception-boundary` | KEEP | backend callback exceptions are contained instead of terminating host | faulty backend can crash embedding Qt process |
| `hyremote-core-test-dependency-boundary` | KEEP | Core remains Qt-GUI/protocol/platform neutral and does not link Qt product targets | canonical `integrations -> runtime -> core` dependency rule is broken |

### Core scenario-level note

`test_session_lifecycle.cpp` is intentionally multi-scenario. Current meaningful scenario families all earn their place: documented state transitions; repeated start/stop; capture/transport start failure cleanup; recoverable vs fatal event escalation; invalid configuration; idempotent stop; in-flight capture bounds/retry; backend exception containment; concurrent stop ownership; and stop-during-start races. Phase B may split it only if failure diagnosis remains too broad; splitting is not required merely because the file is large.

## 3. Shared Runtime / RFB / target adapters

### 3.1 Correctly Runtime-owned today

| CTest | Current owner | Decision | Necessity |
| --- | --- | --- | --- |
| `hyremote-runtime-automatic-surface-model-test` | Runtime | KEEP | proves application-surface discovery/model decisions independent of QPA/Generic frontend |
| `hyremote-runtime-automatic-composite-capture-test` | Runtime | KEEP | proves composite capture geometry/composition behavior used by automatic access |
| `hyremote-runtime-automatic-composite-input-test` | Runtime | KEEP | proves input routing across the same composite model |
| `hyremote-build-authority-selftest` | Runtime registration | MOVE -> T3 repository | necessary, but build/bootstrap authority is not Runtime behavior |

### 3.2 Runtime/backend tests currently misowned by C++ frontend

These tests are necessary, but their implementation includes Runtime private headers/source and they protect Runtime/backend/adapter behavior rather than C++ facade semantics.

| Current CTest | Target owner | Decision | Necessity / protected contract |
| --- | --- | --- | --- |
| `hyremote-security-descriptor-test` | Runtime/security | MOVE | descriptor parsing/validation and fail-closed credential configuration |
| `hyremote-vnc-auth-test` | Runtime/RFB security | MOVE | VNC challenge/response primitive correctness |
| `hyremote-rfb-vnc-auth-handshake-test` | Runtime/RFB | MOVE | RFB security negotiation and positive/negative VNC Auth handshake |
| `hyremote-rfb-multi-client-input-test` | Runtime/RFB | MOVE | simultaneous viewer input ownership/cleanup semantics |
| `hyremote-listener-address-matrix-test` | Runtime/network | MOVE | loopback/non-loopback bind policy and address handling |
| `hyremote-input-mailbox-admission-test` | Runtime | MOVE | Runtime-side bounded admission of transport input before GUI dispatch |
| `hyremote-target-component-provider-test` | Runtime/adapters | MOVE | target -> capture/input component selection independent of public C++ facade |
| `hyremote-widgets-capture-test` | Runtime/Widgets adapter | MOVE | QWidget capture correctness |
| `hyremote-widgets-input-routing-test` | Runtime/Widgets adapter | MOVE | QWidget input injection/routing |
| `hyremote-widgets-input-backpressure-test` | Runtime/Widgets adapter | MOVE | bounded GUI-thread input delivery under pressure |
| `hyremote-quick-capture-test` | Runtime/Quick adapter | MOVE | QQuickWindow capture correctness |
| `hyremote-quick-input-routing-test` | Runtime/Quick adapter | MOVE | QQuickWindow input injection/routing |
| `hyremote-quick-input-backpressure-test` | Runtime/Quick adapter | MOVE | bounded Quick input delivery under pressure |
| `hyremote-rfb-widget-disconnect-backpressure-test` | Runtime/RFB + Widgets integration | MOVE | disconnected/slow viewer must not break bounded capture/input lifecycle |

Support-only files `rfb_test_server.cpp` and `rfb_product_fit.py` are not ordinary CTest owners; they should move with the RFB/product-fit owner if Phase B centralizes the Runtime RFB harness.

## 4. C++ frontend — T2

Only facade/public behavior unique to `HyRemote::RemoteAccess` should remain here.

| CTest | Decision | Necessity |
| --- | --- | --- |
| `hyremote-remoteaccess-test` | KEEP | safe defaults, inert construction, public config mutability, lifecycle forwarding, move ownership, client-count/error mapping and security fail-closed behavior of the C++ facade |
| `hyremote-remoteaccess-error-ack-test` | KEEP | public `lastError()/clearError()` acknowledgement contract; prevents stale/accidentally cleared error semantics |
| `hyremote-remoteaccess-target-loss-test` | KEEP | public facade response when the bound QObject/target disappears; protects embedding-app safety |

The C++ facade tests may use controlled Runtime fakes. They should not own RFB/security parser or GUI adapter implementation tests.

## 5. QML frontend — T2

| CTest | Decision | Necessity |
| --- | --- | --- |
| `hyremote-qml-module-test` | KEEP | QML module/import/type registration and declarative facade semantics are available from the built payload |
| `hyremote-qml-deploy-helper-non-qml` | REVIEW | validates deployment helper route selection; contract may belong to central T4 deploy tests rather than QML ownership |
| `hyremote-qml-deploy-helper-qml` | REVIEW | same as above for QML deployment; retain until overlap with root/QPA deploy-helper tests is mapped |

QML tests must not duplicate Shared Runtime lifecycle/security behavior merely through the declarative wrapper. V0.2 typed notifications should add parity tests only for QML-observable semantics.

## 6. Generic frontend — T2

| CTest | Decision | Necessity |
| --- | --- | --- |
| `hyremote-generic-plugin-smoke` | KEEP | proves Qt generic-plugin activation from `QT_QPA_GENERIC_PLUGINS`, with native QPA preserved and no application HyRemote API/link requirement |

The stronger clean installed Generic Widgets/Quick paths are T4 and remain separately necessary because in-tree plugin activation cannot prove install/deploy isolation.

## 7. QPA frontend — T2 plus QPA-specific deployment contracts

Runtime/QPA behavior that is unique to the exact-private-ABI Factory-Trampoline remains justified.

| CTest | Decision | Necessity |
| --- | --- | --- |
| `hyremote-qpa-proxy-smoke` | KEEP | platform plugin loads and delegates through native platform path |
| `hyremote-qpa-native-semantics` | KEEP | exact Qt-private delegate/native QPA semantics remain intact |
| `hyremote-qpa-remote-config-test` | KEEP | QPA launch/config vocabulary parses correctly without re-owning Runtime composition |
| `hyremote-qpa-auto-remoteaccess-smoke` | KEEP | zero-code QPA route actually starts shared Runtime |
| `hyremote-qpa-remote-failure-native-survival-smoke` | KEEP | remote-access failure must not destroy local/native Qt application survival |
| `hyremote-qpa-multi-surface-connection-smoke` | KEEP | multiple QWidget surfaces remain usable through QPA automatic composition |
| `hyremote-qpa-widget-popup-connection-smoke` | KEEP | popup/transient QWidget behavior survives proxy/delegate path |
| `hyremote-qpa-widget-opengl-capture-smoke` | KEEP conditional | protects real QOpenGLWidget capture classification when Qt OpenGLWidgets exists |
| `hyremote-qpa-quick-multi-window-connection-smoke` | KEEP | multiple Quick windows survive QPA proxy path |

QPA deploy-helper tests (`ordinary`, `qml-only`, `qml-composed`, installed-payload variants, negative missing/stale metadata cases, Qt mismatch, generator matrix, Linux source-payload relocation) are necessary contracts but are marked `REVIEW` for Phase B physical ownership. They may remain QPA-specific T4 tests or move under a central deploy-contract area; the criterion is unique QPA deployment value, not file location aesthetics.

## 8. Repository/product contracts — T3

| CTest / asset | Decision | Necessity |
| --- | --- | --- |
| `hyremote-build-authority-selftest` | MOVE here | canonical `compile.cmd -> CMake/build.yml` behavior must not drift |
| `hyremote-ci-scope-self-test` | KEEP | CI path classifier decides what evidence executes; a bad classifier can create false-green lanes |
| `hyremote-mainline-audit-self-test` | KEEP | retry/verification behavior of mainline audit must be executable, not prose-only |
| `hyremote-branch-name-gate-self-test` | KEEP | branch-family policy is executable repository contract |
| `tests/public-api-contract` | KEEP | freezes exported C++ target/API shape and prevents Core/QPA/Widgets/Quick/QML dependency leakage |
| `hyremote-release-readiness-runtime-contract` | MOVE/SPLIT -> T3 | current filename/location says release, but much of it protects cross-release Runtime architecture |
| `hyremote-release-readiness-repository-layout` | MOVE -> T3 | canonical source/dependency layout is repository truth, not release-only truth |
| `hyremote-release-readiness-documentation-paths` | REVIEW -> T3 | keep only assertions that protect live navigability/authority; remove wording/history policing |
| `hyremote-release-readiness-ci-environment-baseline` | MOVE -> T3 | CI reference environment/toolchain truth is repository automation contract |
| `hyremote-release-readiness-licensing-boundary` | MOVE -> T3 | dependency/license boundary is product/repository contract across releases |

## 9. External consumer / package / deploy — T4

| Test/cell | Decision | Necessity |
| --- | --- | --- |
| `hyremote-cpp-installed-consumers` (`installed-cpp-widgets`, `installed-cpp-quick`) | KEEP | V0.1 primary C++ product promise from clean installed SDK with real Widgets and Quick lifecycle |
| `hyremote-generic-installed-consumers` (`installed-generic-widgets`, `installed-generic-quick`) | KEEP | V0.1 primary zero-code promise from clean installed SDK while preserving native QPA |
| `tests/consumer-installed-sdk` / release cell `installed-sdk` | KEEP but RENAME/clarify | distinct minimal installed public-package/deploy smoke: no Widgets/Quick adapter dependency; catches package/export/deploy closure failures that adapter consumers can mask. Current name is ambiguous |
| `tests/consumer-source` / `source-consumer` | KEEP | source/add_subdirectory consumption must not inherit developer-only switches or require installed-package assumptions |
| `tests/consumer-installed-qml` / installed QML evidence | KEEP preview | clean declarative payload consumption remains distinct from in-tree QML module smoke |
| `tests/consumer-installed-qpa` / installed QPA product-fit | KEEP preview | exact private-ABI QPA package/deploy path cannot be proved by in-tree plugin tests |
| `hyremote-release-readiness-deployment-relocation` | MOVE -> T4 | relocation is a package/deploy contract, not inherently release-only |
| `hyremote-release-readiness-deploy-helper-contract` | MOVE/SPLIT -> T4 | one deploy family is a persistent SDK contract; split by acquisition/frontend only when it improves diagnosis |
| `hyremote-release-readiness-consumer-simplicity` | MOVE -> T4/T3 | proves applications do not learn internal targets/choices |
| `hyremote-release-readiness-package-acquisition-isolation` | MOVE -> T4 | installed package must not leak source/build-tree acquisition |

Release evidence runner cells remain evidence production, not substitutes for lower-layer behavior tests. Every cell must state the unique external-delivery claim it proves.

## 10. Product/adoption E2E — T5

| Test/asset | Decision | Necessity |
| --- | --- | --- |
| `hyremote-v01-example-smoke` | KEEP | installs SDK, configures/builds 01/02/03 independently, launches them and checks adoption path/native platform/lifecycle |
| `tests/product-e2e/example_product_fit.py` | REVIEW | keep only user-level behavior not already covered by `hyremote-v01-example-smoke` plus lower layers |
| `tests/product-e2e/qml_product_fit.py` | KEEP/REVIEW | QML preview user-flow has value, but overlap with installed-QML and module tests must be made explicit |
| `tests/product-e2e/showcase_product_fit.py` | REVIEW | showcase-specific UX/product behavior must be identified; retire if it only repeats lower-level RFB checks |
| `src/integrations/cpp/tests/rfb_product_fit.py` | MOVE -> T5/RFB harness | black-box protocol/product-fit value exists, but C++ frontend is the wrong semantic owner |

## 11. Release authority / readiness — T6

| CTest | Decision | Necessity |
| --- | --- | --- |
| `hyremote-release-readiness-metadata` | SPLIT/KEEP | keep release/version/security truth; move cross-release documentation/repository checks to T3 |
| `hyremote-release-readiness-release-authority-policy` | KEEP | machine release-train authority must match governance and fail closed |
| `hyremote-release-readiness-release-documentation-layout` | KEEP | candidate/release note/package documentation presence/layout is release-specific |
| `hyremote-release-scope-self-test` | KEEP | exact requested train resolves only its own mandatory WBS and unknown trains fail closed |
| `hyremote-release-readiness-source-qpa-authority` | REVIEW | likely valid QPA acquisition-authority negative test; determine whether T4 is the better permanent owner |
| `hyremote-release-profile-develop-all` | KEEP | 0.0.0 development sentinel accepts broad capability build |
| `hyremote-release-profile-develop-runtime-only` | KEEP | sentinel also supports reduced capability build |
| `hyremote-release-profile-retire-v001` | KEEP | retired frontend-coded version must remain rejected |
| `hyremote-release-profile-retire-v002` | KEEP | same |
| `hyremote-release-profile-retire-v003` | KEEP | same |
| `hyremote-release-profile-v010-cpp-only` | KEEP | progressive V0.1 train is representable independent of frontend-as-version semantics |
| `hyremote-release-profile-v020-runtime` | KEEP | V0.2 train remains representable |
| `hyremote-release-profile-v030-all` | KEEP | V0.3 train remains representable |
| `hyremote-release-profile-v040-all` | KEEP | V0.4 train remains representable |
| `hyremote-release-profile-v040-maintenance` | KEEP | four-part maintenance digit is accepted within V0.4 line |
| `hyremote-release-profile-v100-all` | KEEP | first GA train with all capabilities |
| `hyremote-release-profile-v100-cpp-only` | KEEP | frontend subset must not redefine version meaning |
| `hyremote-release-profile-v100-generic-only` | KEEP | same for Generic-only capability subset |

## 12. First Phase-A ownership conclusions

1. **Core test ownership is healthy.** No move is currently justified.
2. **Runtime test ownership is incomplete.** RFB/security/network/Widgets/Quick adapter tests accumulated under C++ frontend and should move in Phase B without changing semantics.
3. **C++ frontend should shrink to facade-only tests.** This restores the product architecture in the test architecture.
4. **QPA has many justified unique tests**, but its deploy-helper matrix should be reviewed alongside central deploy tests before any consolidation.
5. **`release-readiness` is overloaded.** Persistent repository/package contracts should execute earlier and live outside T6; release-specific scope/profile/metadata stays T6.
6. **The legacy `consumer-installed-sdk` still has a distinct contract**, but its name hides that contract. It is not yet approved for retirement.
7. **No test is approved for deletion in Phase A.** `MERGE/RETIRE` requires scenario-level equivalence evidence first.

## 13. Phase-A remaining work before Review Gate

- expand multi-scenario executables/scripts to scenario-level entries, especially Core Session, C++ RemoteAccess, RFB, QPA deploy matrix and release-evidence cells;
- map all current CTest registrations to capability guards and platform conditions;
- map overlap among product-fit/example/installed-consumer suites;
- complete `COVERAGE_GAPS.md` severity/owner/release assignment;
- reconcile source-derived catalog against one canonical full all-frontends CTest listing before Phase A PASS.
