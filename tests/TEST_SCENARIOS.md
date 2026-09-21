# HyRemote Test Scenario Appendix

> Scenario-level appendix to [`TEST_CATALOG.md`](TEST_CATALOG.md) for #274 Phase A.
>
> This file records meaningful scenarios inside multi-case executables/scripts. It is intentionally behavior-neutral: it does not create, remove or reselect tests.

## 1. C++ `RemoteAccess` facade

CTest: `hyremote-remoteaccess-test` — owner T2/C++ frontend — decision: KEEP.

| Scenario | Why it is necessary | Failure meaning |
| --- | --- | --- |
| `testSafeDefaultsAndNoConstructionSideEffect` | proves inert construction, loopback/input-safe defaults, secure-profile fail-before-compose behavior and stopped-only security mutation | public facade can create side effects or unsafe listener/security behavior before explicit start |
| `testMissingTargetAndMissingAdapterFailCleanly` | proves product-level errors for absent target/adapter | embedding application receives undefined/crashing behavior instead of a stable error |
| `testProductLifecycleAndConfigurationForwarding` | proves configuration reaches the one Runtime, live config is immutable, stop is deterministic/idempotent and config becomes mutable after stop | public API no longer maps deterministically onto Shared Runtime lifecycle |
| `testMoveTransfersOwnershipAndQuiescesReplacedRuntime` | proves move-only facade ownership transfers without leaked/running duplicate Runtime | SDK move semantics can leak capture/transport/input ownership |
| `testConnectedClientCountUsesTransportNeutralEvents` | proves count updates only from neutral transport events and cannot resurrect after stop | frontend leaks RFB-specific counting or stale callbacks mutate stopped state |
| `testRemoteInputIsIndependentAndOffByDefault` | proves input is opt-in and absence of an input sink is a clean product error | safe default or optional-input contract regresses |
| `testRecoverableRuntimeErrorCanBeAcknowledgedAndReappearsOnNewFailure` | proves `lastError/clearError` behavior for recoverable failures | application cannot reliably acknowledge and observe new diagnostics |
| `testFaultedRuntimeRequiresExplicitStopAndKeepsFatalDiagnostic` | proves fatal diagnostics survive Faulted state until explicit cleanup | fatal cause can be lost or Runtime can silently self-reset |
| `testBackendStartFailureIsMappedAndCleanedUp` | proves backend start failure maps to public error and partially started pieces are cleaned | facade exposes internal failure inconsistently or leaks resources |
| `testInvalidPublicConfigurationIsProductLevel` | proves invalid public values are rejected at facade contract level | callers can feed invalid state into lower layers |
| `testAuthenticatedEncryptedFailsClosedBeforeListen` | proves declared encrypted profile never downgrades to weaker VNC/None before TLS/VeNCrypt exists | serious security contract violation |

These scenarios are related by one public-facade contract and do not need splitting merely because the file is large. Split only if future diagnostics/ownership become materially clearer.

## 2. Core Session lifecycle/concurrency

CTest: `hyremote-core-test-session-lifecycle` — owner T1/Core — decision: KEEP.

The current file is intentionally a regression collection around one state machine. Scenario families observed in the source:

| Scenario/family | Why necessary |
| --- | --- |
| `the_session_follows_the_documented_state_sequence` | freezes Stopped → Starting → Running → Stopping → Stopped ordering visible to components |
| `repeated_start_stop_is_deterministic` | detects per-run counter/state leakage and duplicate teardown |
| `a_capture_start_failure_faults_before_running` | partial-start failure before transport must be diagnosable/cleanable |
| `a_transport_start_failure_faults_and_cleans_the_capture_source` | capture started before transport failure must be stopped exactly once |
| recoverable/non-recoverable capture-event scenarios | only fatal target/capture failures may fault the Session |
| `transport_events_escalate_only_when_fatal` | normal disconnect/recoverable events must not fault shared Runtime |
| `configuration_and_component_errors_fail_before_starting` | invalid bounds/missing components fail before worker activity |
| `start_requires_the_stopped_state_and_stop_is_idempotent` | prevents overlapping runs/double teardown |
| `the_capture_in_flight_bound_is_respected` | protects memory/work bounds |
| rejected-request retry scenario | capture backpressure can recover without faulting or spinning unboundedly |
| backend request/enqueue exception scenarios | plugin/backend exceptions cannot terminate host process |
| `r2b1_concurrent_stop_callers_produce_exactly_one_teardown` | multiple stop callers must converge on one teardown owner |
| `r2b2_stop_while_the_capture_source_start_is_in_progress` | deterministic cancellation while external capture start is blocked |
| `r2b2_stop_while_the_transport_start_is_in_progress` | same for transport startup |
| `r2b3_a_setter_replacing_a_component_during_start_is_safe` | replacement racing with startup cannot produce dangling component use |
| `r3b1_stop_wins_at_the_earliest_moment_after_starting_is_published` | protects earliest externally observable stop/start race window |
| `r3b1_stop_wins_after_the_transport_startup_commits_before_running` | protects commit-before-Running race window |
| `r3b1_concurrent_start_and_stop_complete_without_deadlock` | bounded race regression for lock-order/deadlock failure mode |
| `b1_workers_created_during_starting_survive_the_running_publication` | worker threads created during Starting must not exit before Running publication |
| `b2_replacement_is_rejected_while_starting_and_while_faulted` | ownership mutation is forbidden while state cannot accept it |
| `b2_replacement_is_rejected_while_stopping` | prevents component replacement racing with teardown |
| plugin-boundary throwing start/capability scenarios | exceptions at external component boundaries must map to deterministic faults/cleanup |

Phase A disposition: KEEP as one executable for now. A later SPLIT would need to preserve deterministic shared fixtures and demonstrably improve diagnosis/runtime isolation.

## 3. RFB security handshake

CTest: `hyremote-rfb-vnc-auth-handshake-test` — current location C++, target owner T1 Runtime/RFB — decision MOVE.

| Scenario | Necessity |
| --- | --- |
| correct credential succeeds | proves SecurityType 2 challenge/response works end-to-end |
| wrong credential rejected | proves failure result + `AuthenticationRejected` + disconnect |
| client selects None despite auth requirement | proves no downgrade to SecurityType None |
| stalled auth client | proves bounded handshake timeout |
| explicit insecure profile | proves None is offered only for the deliberate insecure mode |

This is distinct from the VNC primitive unit test: primitive correctness cannot prove wire negotiation/downgrade behavior.

## 4. RFB product-fit harness

Script: `rfb_product_fit.py` — target owner T5/RFB product-fit — decision MOVE from C++ test ownership.

| Scenario | Unique value |
| --- | --- |
| `verify_occupied_port_failure` | real listener conflict is bounded and reported as startup failure |
| `verify_handshake_slots_expire` | eight incomplete handshakes cannot permanently consume all bounded client slots; a legitimate viewer connects after expiry |
| `verify_abrupt_disconnect_releases_input` | abrupt socket loss releases only still-held key/button state in correct modifier order and next viewer is clean |
| `verify_standard_client` | maintained-tool (`vncdotool`) framebuffer pixel, pointer/button/wheel, keyboard/text, reconnect, clean stop/listener release |

Do not add duplicate tests for these behaviors unless a lower-layer deterministic test protects a different invariant.

## 5. RFB multi-viewer input

CTest: `hyremote-rfb-multi-client-input-test` — target owner T1 Runtime/RFB — decision MOVE.

Single high-value scenario `testConcurrentViewerHeldStateIsolation` covers multiple aspects of one invariant:

- two viewers holding the same logical key/button emit only one global down transition;
- an out-of-order release from a viewer that did not own the hold cannot release another viewer's contribution;
- one viewer disconnect decrements only its references;
- repeated key-down from the remaining viewer remains repeat input, not a new global hold;
- only the final holder emits key/button up;
- modifiers and last pointer coordinates are preserved on final release.

KEEP as one deterministic state-isolation scenario unless future per-viewer delivery/session work introduces a separate owner.

## 6. Widgets target adapter

CTest executable: `hyremote-widgets-capture-test` — target owner T1 Runtime/Widgets adapter — decision MOVE.

| Scenario | Necessity / current audit result |
| --- | --- |
| `testWidgetsFactoryAndOwnedFrame` | capture capability, owned frame storage, async publication, geometry/format/timing/damage, resize and logical-coordinate behavior |
| intended forced-DPR 1.5 variant | **CONFIRMED GAP TG-001:** source explicitly relies on a second CTest registration with `QT_SCALE_FACTOR=1.5` + `HYREMOTE_EXPECT_DPR`; current CMake has no such registration |
| `testStopCancelsQueuedPublication` | queued capture must not publish after stop |
| `testDestroyedTargetReportsTargetLost` | target destruction produces a terminal target-lost event rather than UAF/stale frame publication |

Input routing/backpressure are separate executables and remain necessary because capture behavior cannot prove GUI-thread input behavior.

## 7. Quick target adapter

CTest executable: `hyremote-quick-capture-test` — target owner T1 Runtime/Quick adapter — decision MOVE.

| Scenario | Necessity / current audit result |
| --- | --- |
| `testQuickFactoryAndOwnedFrame` | Quick capture capability, asynchronous owned frame, geometry/format/timing/damage and resize |
| intended forced-DPR 1.5 variant | **CONFIRMED GAP TG-002:** source states it is registered with `QT_SCALE_FACTOR=1.5` + `HYREMOTE_EXPECT_DPR`; current CMake does not register it |
| `testDestroyedQuickTargetReportsTargetLost` | destruction of QQuickWindow must report target loss cleanly |

Quick input routing/backpressure remain separate Runtime-adapter contracts.

## 8. QPA unique frontend scenarios

Owner T2/QPA. Current high-level CTests remain KEEP unless noted in `TEST_CATALOG.md`.

- proxy load/delegate smoke;
- exact-private-ABI native semantics;
- QPA-only remote configuration vocabulary;
- automatic Shared Runtime start through QPA;
- local/native survival when remote access fails;
- QWidget multi-surface connection;
- QWidget popup/transient connection;
- conditional QOpenGLWidget capture classification;
- Quick multi-window connection.

These must not be replaced by Runtime automatic-composition tests: QPA's unique contract is preserving native platform semantics while injecting the peer frontend.

## 9. QPA deploy-helper matrix

Owner under review: T4/QPA-specific deployment vs central deploy contracts. Current scenarios are necessary until overlap is proved:

- ordinary non-QML deployment;
- QML-only deployment without QPA payload;
- composed QML + QPA deployment;
- installed-payload ordinary/QML-only/QML+QPA variants;
- reject missing QML capability;
- reject stale QML metadata;
- reject missing QML import root;
- reject missing QML module directory;
- reject stale QPA metadata;
- reject Qt private-ABI mismatch;
- reject installed SDK without QPA payload;
- single-config and multi-config generator behavior when Ninja is available;
- Linux source-payload relocation.

Phase A must compare these against root `check_deploy_helper_contract.cmake` before MERGE/RETIRE decisions.

## 10. Release evidence cells

Runner: `tests/release-readiness/run_release_evidence.cmake`.

| Cell | Necessity |
| --- | --- |
| `clean-install` | creates the clean installed product prefix every consumer cell depends on |
| `installed-sdk` | minimal installed package/export/deploy closure independent of Widgets/Quick adapter selection; keep but clarify historical name |
| `installed-qml-qpa` | clean installed declarative + QPA payload composition preview path |
| `installed-qpa-product-fit` | black-box installed QPA product behavior, not just package presence |
| `source-consumer` | external source/add_subdirectory acquisition does not inherit developer-only assumptions |
| `source-qpa-product-fit` | source-acquired QPA product behavior remains usable |
| `deploy-helper` | deployed payload closure through the one SDK deployment helper |
| `installed-generic-widgets` | clean zero-code Generic Widgets primary V0.1 path |
| `installed-generic-quick` | clean zero-code Generic Quick primary V0.1 path |
| `installed-cpp-widgets` | clean installed C++ Widgets primary V0.1 path with actual lifecycle |
| `installed-cpp-quick` | clean installed C++ Quick primary V0.1 path with actual lifecycle |

Evidence cells prove external delivery/acquisition/runtime-isolation claims. They do not replace lower-layer unit/component tests.

## 11. Release-profile / authority scenarios

Owner T6/release.

KEEP current cases because each protects a distinct version-authority rule:

- development sentinel: all frontends enabled;
- development sentinel: runtime-only/reduced capability;
- retired `0.0.1.0`, `0.0.2.0`, `0.0.3.0` rejected;
- V0.1 representable without frontend-as-version encoding;
- V0.2, V0.3, V0.4 representable;
- V0.4 maintenance digit accepted;
- V1.0 all-frontends, C++-only and Generic-only capability subsets accepted.

As #263 adds Feature releases (`0.2.1.0`, `0.3.1.0`, `0.3.2.0`), corresponding exact-scope authority tests belong here rather than in frontend tests.

## 12. Phase-A scenario audit still open

Before Phase A PASS this appendix still needs scenario-level overlap decisions for:

- QML module vs installed-QML vs QML product-fit;
- root deploy-helper contract vs QML/QPA deploy-helper fixtures;
- V0.1 example smoke vs `product-e2e/example_product_fit.py`;
- showcase product-fit unique value;
- listener/address and input-backpressure executables after they move to Runtime ownership;
- candidate fragmented/malformed RFB input gaps from `COVERAGE_GAPS.md`;
- exact capability/platform guard matrix from a canonical all-frontends CTest listing.
