# HyRemote Test Scenario Appendix

> Scenario-level appendix to [`TEST_CATALOG.md`](TEST_CATALOG.md) for #274 Phase A. Facts refreshed against `develop` at `6dc244a8717bdc520544c71d9756499033857c73`.
>
> This file records meaningful scenarios inside multi-case executables/scripts. It does not create, remove, move or reselect tests.

## 1. C++ `RemoteAccess` facade

CTest: `hyremote-remoteaccess-test` — T2/C++ — KEEP.

| Scenario | Why necessary | Failure meaning |
| --- | --- | --- |
| `testSafeDefaultsAndNoConstructionSideEffect` | inert construction, loopback/input-safe defaults, stopped-only security mutation | public facade causes side effects or unsafe default state |
| `testMissingTargetAndMissingAdapterFailCleanly` | stable errors for absent target/adapter | embedding app gets undefined/crashing behavior |
| `testProductLifecycleAndConfigurationForwarding` | config reaches one Runtime; live config frozen; deterministic/idempotent stop | facade no longer maps deterministically to Shared Runtime |
| `testMoveTransfersOwnershipAndQuiescesReplacedRuntime` | move-only ownership without duplicate/leaked Runtime | capture/transport/input ownership can leak |
| `testConnectedClientCountUsesTransportNeutralEvents` | neutral client events; no stale post-stop resurrection | public API leaks backend-specific/stale counting |
| `testRemoteInputIsIndependentAndOffByDefault` | control opt-in and clean unavailable-input error | safe default regresses |
| `testRecoverableRuntimeErrorCanBeAcknowledgedAndReappearsOnNewFailure` | clear/ack + new-error observability | diagnostics become stale or lossy |
| `testFaultedRuntimeRequiresExplicitStopAndKeepsFatalDiagnostic` | fatal cause persists until cleanup | fault cause can disappear or auto-reset unexpectedly |
| `testBackendStartFailureIsMappedAndCleanedUp` | partial backend start failure maps/cleans | facade leaks internals/resources |
| `testInvalidPublicConfigurationIsProductLevel` | invalid values rejected before lower layers | invalid state enters Runtime |
| `testAuthenticatedEncryptedFailsClosedBeforeListen` | unavailable encrypted profile never downgrades | security contract violation |

One executable remains justified because all scenarios qualify one facade/state contract.

## 2. Core Session lifecycle/concurrency

CTest: `hyremote-core-test-session-lifecycle` — T1/Core — KEEP.

Meaningful scenarios/families:

- documented Stopped→Starting→Running→Stopping→Stopped sequence;
- repeated deterministic start/stop and exactly-once teardown;
- capture-start and transport-start partial-failure cleanup;
- recoverable vs fatal capture/transport event escalation;
- invalid configuration/missing component rejection before activity;
- start only from Stopped and idempotent stop;
- capture in-flight bounds and rejected-request retry;
- capture/transport exceptions contained at backend boundary;
- concurrent stop callers produce one teardown owner;
- stop while capture start is blocked;
- stop while transport start is blocked;
- component replacement racing with startup is safe;
- earliest externally observable stop-win after Starting publication;
- stop after transport startup commit but before Running publication;
- bounded concurrent start/stop deadlock regression;
- workers created during Starting survive Running publication;
- component replacement rejected in Starting/Faulted/Stopping;
- throwing capture/transport start and capability queries map to deterministic cleanup/fault.

Do not split merely because the file is large; split only if diagnosis/ownership materially improves without losing deterministic race fixtures.

## 3. RFB authentication / multi-viewer / product-fit

### Registered VNC Auth handshake

CTest `hyremote-rfb-vnc-auth-handshake-test` — MOVE to T1 Runtime/RFB.

- correct credential succeeds;
- wrong credential is rejected;
- client selecting None while auth required is refused (no downgrade);
- stalled auth is bounded by handshake timeout;
- explicit insecure profile offers None only when deliberately selected.

Distinct from the VNC primitive test: crypto primitive correctness cannot prove wire negotiation.

### Registered multi-viewer held-state isolation

CTest `hyremote-rfb-multi-client-input-test` — MOVE to T1 Runtime/RFB.

One scenario intentionally combines one invariant:

- two viewers holding same logical key/button cause one global down;
- out-of-order release from a non-owner cannot release another viewer's hold;
- disconnect removes only that viewer's references;
- surviving-viewer repeat remains repeat input;
- final holder emits the only up transition;
- final release preserves modifiers and last pointer position.

### Dormant `rfb_product_fit.py`

T5/RFB product-fit — MOVE from C++ physical ownership; GAP TG-009 because it is not currently executed.

- `verify_occupied_port_failure`: bounded real bind failure;
- `verify_handshake_slots_expire`: eight stalled sockets expire and a legitimate viewer then connects;
- `verify_abrupt_disconnect_releases_input`: held modifiers/keys/buttons are synthesized exactly once and next viewer starts clean;
- `verify_standard_client`: maintained `vncdotool` framebuffer pixel, pointer/button/wheel, keyboard/text, reconnect and listener release.

Do not duplicate these behaviors in another E2E harness just because this script lacked registration.

### Confirmed parser/fragmentation gaps

`rfb_transport.cpp` buffers arbitrary `readAll()` fragments and enforces `kMaxClientInputBytes`, `kMaxEncodings`, `kMaxCutTextBytes`. Inspected registered RFB tests send complete protocol messages and do not drive those bound failures. Therefore TG-013/TG-014 are confirmed Phase-D Runtime/RFB gaps.

## 4. Widgets adapter

### Capture

CTest `hyremote-widgets-capture-test` — MOVE to T1 Runtime/Widgets.

- factory/capabilities + owned asynchronous frame;
- geometry/pixel/timing/damage and resize/logical-coordinate semantics;
- stop cancels queued publication;
- destroyed target reports target-lost rather than publishing stale data;
- **GAP TG-001:** intended forced DPR=1.5 execution is not registered.

### Input backpressure

CTest `hyremote-widgets-input-backpressure-test` — MOVE to T1 Runtime/Widgets.

- 10k pointer moves while GUI queue is not drained coalesce to one newest coordinate rather than 10k queued deliveries;
- accepted held button/Shift releases cross a saturated normal mailbox through protected release capacity;
- a rejected press cannot later consume protected capacity via unmatched releases;
- `shutdown()` drops pending undelivered input but balances already-delivered held state exactly once;
- destruction after terminal shutdown does not synthesize a second release sequence.

These are distinct from normal input-routing tests and from the RFB+Widgets disconnect-backpressure cross-component test.

## 5. Quick adapter

CTest `hyremote-quick-capture-test` — MOVE to T1 Runtime/Quick.

- factory/capabilities + asynchronous owned frame;
- geometry/pixel/timing/damage and resize;
- destroyed QQuickWindow reports target loss;
- **GAP TG-002:** intended forced DPR=1.5 execution is not registered.

Quick routing/backpressure stays a distinct T1 contract for Quick event delivery; it is not replaced by Widgets tests.

## 6. Listener / address matrix — SPLIT ownership

Current CTest `hyremote-listener-address-matrix-test` uses public `RemoteAccess` and a QWidget target, but its scenarios have two semantic owners:

### C++ facade-facing rows — remain T2 C++

- explicit loopback address + OS-selected port starts and stop immediately releases endpoint;
- occupied port fails before Running with public product error;
- unavailable TEST-NET address fails before Running without silent fallback.

These protect `RemoteAccess::setListenAddress/setPort/start/lastError` mapping.

### Runtime/RFB network rows — move/extract when Phase B/#174 needs them

- IPv4 wildcard reachability;
- IPv6 loopback reachability;
- IPv6 wildcard reachability;
- IPv6 wildcard explicitly not treated as dual-stack on the measured platform;
- rule: success must be reachable on requested address; failure must not silently fall back to loopback/another scope.

Phase A does not create duplicate transport tests. The current executable is marked SPLIT so Phase B can separate facade mapping from bind/address-family behavior without semantic loss.

## 7. QML frontend: module vs installed vs E2E

### Registered `hyremote-qml-module-test` — T2 QML KEEP

Source scenarios:

1. `testDeclarativeImportAndSafeDefaults` — import/type registration, stopped/client-count/listen/security defaults and read-only diagnostics;
2. `testInvalidConfigurationDoesNotMutateAcceptedValue` — invalid port rejection, error clear, security-profile/config surface, absence of raw secret API;
3. `testEnabledStartFailureIsTransactional` — declarative `enabled:true` failure rolls back to disabled/Stopped with stable error;
4. `testInitialEnabledDoesNotRaceLaterTargetBinding` — QQmlParserStatus/component-complete ordering prevents setter-order start race;
5. `testTargetDestructionNotifiesDeclarativeProperty` — target lifetime updates observable QML property exactly once.

### Installed-QML evidence — T4 KEEP preview

Proves clean package/import/deploy/runtime closure from an installed SDK. It cannot be replaced by the in-tree module test.

### `qml_product_fit.py` — T5 KEEP, currently dormant

Proves real-viewer/QML-observable client count, view-only isolation, same-process stop→configure→start, control input and reconnect. It is neither import smoke nor package acquisition evidence. TG-011 records missing non-fast execution ownership.

Conclusion: three layers overlap by feature, not by proof type; none is a duplicate.

## 8. QPA unique frontend scenarios

T2 QPA KEEP:

- proxy load/native delegate smoke;
- exact-private-ABI native semantics;
- QPA launch/config vocabulary;
- automatic Shared Runtime start;
- native/local application survival when remote start fails;
- QWidget multi-surface connection;
- popup/transient QWidget connection;
- conditional QOpenGLWidget capture classification;
- Quick multi-window connection.

Runtime automatic-composition tests cannot replace these because QPA uniquely promises native platform semantics while inserting the frontend.

## 9. Deploy-helper proof layers

### QML dispatch fixtures — MOVE to T4, KEEP semantics

- non-QML route must call ordinary Qt deploy API and leave app QML import path unchanged;
- QML route must call QML-aware Qt deploy API, preserve the app's existing import path and add installed HyRemote import root.

### QPA source/installed matrix — MOVE to T4/QPA deploy-contract

- ordinary non-QML;
- QML-only without QPA;
- QML+QPA composition;
- installed-payload ordinary/QML-only/QML+QPA;
- reject missing QML capability;
- reject stale QML metadata;
- reject missing QML import root/module directory;
- reject stale QPA metadata;
- reject exact-Qt mismatch;
- reject installed SDK missing QPA;
- single-config and multi-config generator shape when Ninja exists;
- Linux source-payload relocation.

### Root static contract scan — MOVE to T4

Checks implementation/package/install structure: required/forbidden dispatch and metadata, native platform vs QPA/Generic separation, fail-closed contract, fixture capabilities.

### Exact-SHA release evidence `deploy-helper` cell — KEEP evidence production

Proves actual clean install/deploy output rather than configure doubles or token/static assertions.

Conclusion: these are four different proof layers. Phase B may share fixture plumbing but no semantic layer is approved for deletion.

## 10. Release evidence cells

Runner `tests/release-readiness/run_release_evidence.cmake`:

| Cell | Distinct necessity |
| --- | --- |
| `clean-install` | produces clean installed prefix all consumer cells use |
| `installed-sdk` | minimal package/export/deploy closure without Widgets/Quick-specific consumer; KEEP, later rename for clarity |
| `installed-qml-qpa` | clean installed declarative + QPA composition preview |
| `installed-qpa-product-fit` | installed QPA product behavior, not payload presence only |
| `source-consumer` | source/add_subdirectory acquisition independent of dev-only assumptions |
| `source-qpa-product-fit` | source-acquired QPA product behavior |
| `deploy-helper` | real one-family deploy closure |
| `installed-generic-widgets` | primary clean Generic Widgets path |
| `installed-generic-quick` | primary clean Generic Quick path |
| `installed-cpp-widgets` | primary clean C++ Widgets lifecycle |
| `installed-cpp-quick` | primary clean C++ Quick lifecycle |

TG-012 records the acquisition-isolation audit bug discovered here and handed back to reopened #230.

## 11. V0.1 adoption vs app/product-fit

### `hyremote-v01-example-smoke` — T5 KEEP

Installs the SDK, independently configures/builds/deploys canonical 01/02/03, launches them, checks RemoteAccess Running→Stopped and Generic native-platform preservation. It proves the **developer adoption path**, not complete remote-control correctness.

### `tests/product-e2e/example_product_fit.py` — T5 KEEP

Its unique app-level contract is view-only→control policy transition, real framebuffer, buttons/wheel/modifiers/text, reconnect and listener release across Widgets/Quick applications. The old internal labels `widgets-basic`/`quick-basic` are historical drift; Phase D should point the harness at canonical 01/02 or a canonical app fixture without restoring removed teaching paths.

### `showcase_product_fit.py` — T5 KEEP non-fast

The showcase still exists and its README explicitly claims standard-viewer/client-count lifecycle acceptance. The harness protects showcase-specific `SHOWCASE_CLIENTS 0/1/reconnect`, remote pointer/key delivery and listener release. It is not a V0.1 primary acceptance requirement.

## 12. Release profile / authority scenarios

Registered T6 cases remain distinct:

- development sentinel with all frontends;
- development sentinel runtime-only/reduced capability;
- retired `0.0.1.0`, `0.0.2.0`, `0.0.3.0` rejected;
- V0.1, V0.2, V0.3, V0.4 selectable;
- V0.4 maintenance digit accepted;
- V1.0 all/C++-only/Generic-only capability subsets accepted.

#277 expanded exact Feature-release selection/policy behavior inside the existing `hyremote-release-scope-self-test` and authority-policy registrations. Those internal scenarios belong here; they are not new CTest names.

## 13. Hosted execution reconciliation

The all-frontends hosted baseline in run `35577711774` proved:

- Linux Qt 6.8.3 Release: 93 discovered / 93 executed / PASS;
- Windows Qt 6.8.3 Release: 92 / 92 / PASS;
- the only platform-set difference is Linux-only `hyremote-qpa-source-payload-relocation`;
- none of `rfb_product_fit.py`, `qml_product_fit.py`, `showcase_product_fit.py`, `example_product_fit.py` is in that executed CTest set;
- there is no second forced-DPR Widgets or Quick capture registration.

See [`EXECUTION_BASELINE.md`](EXECUTION_BASELINE.md) for the exact 93-name superset.

## 14. Phase-A overlap conclusions

All previously open scenario questions are closed:

- QML module / installed-QML / QML product-fit: **all KEEP**, distinct T2/T4/T5 proof layers;
- root / QML / QPA / release deploy-helper tests: **KEEP semantics, MOVE persistent contracts to T4**, no duplicate layer deletion;
- V0.1 adoption smoke vs app product-fit: **both KEEP**, adoption vs real viewer/control correctness;
- showcase product-fit: **KEEP non-fast** while showcase remains maintained;
- listener matrix: **SPLIT** C++ facade mapping from Runtime bind/address-family behavior;
- fragmented/malformed RFB: **GAP TG-013/TG-014**;
- capability/platform execution: reconciled in `TEST_MATRIX.md` + `EXECUTION_BASELINE.md`.

Phase A therefore has no unresolved scenario-level `REVIEW` item.