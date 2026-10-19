# HyRemote Test Scenario Appendix

> Final #274 scenario authority, reconciled after Phase D against `develop@bee774e76a48c7a23020b5386b42ac7a2bbb56f8`.
>
> `TEST_CATALOG.md` owns exact registered identities and necessity. This appendix records the meaningful scenario groups inside multi-case executables/scripts so one executable is not mistaken for one behavior.

## Core Session lifecycle / concurrency

`hyremote-core-test-session-lifecycle` remains one grouped Core integration executable because the scenarios share state-machine/race fixtures. It covers:

- documented transitions and repeated start/stop;
- partial capture/transport start cleanup;
- recoverable/fatal event escalation;
- invalid configuration;
- capture in-flight bounds;
- backend exceptions;
- concurrent stop ownership;
- stop while capture/transport start is blocked;
- component replacement races;
- stop-win windows before Running;
- worker startup while Starting;
- replacement rejection in Starting/Faulted/Stopping;
- deterministic exception cleanup.

Splitting by source-file size alone would weaken shared race fixtures and is not justified.

## Runtime typed notifications

`hyremote-runtime-notifications-test` drives the Runtime's private typed-notification seam from real state/client/error events rather than fixed delays. It proves ordering, one shared Runtime source of truth, event-boundary delivery and object lifetime behavior.

`hyremote-qml-notifications-test` separately proves that the QML wrapper observes those same Runtime events on the declarative object's thread. It does not re-test Runtime state-machine mechanics.

## RFB authentication and viewer state

`hyremote-vnc-auth-test` proves the VNC Authentication crypto primitive when transport security is enabled.

`hyremote-rfb-vnc-auth-handshake-test` proves wire behavior separately:

- correct credentials;
- wrong credentials;
- no downgrade to None;
- bounded stalled-auth handling;
- deliberate insecure/basic profile behavior where allowed.

`hyremote-rfb-multi-client-input-test` proves simultaneous viewers cannot prematurely release each other's held key/button state, and that disconnect removes only the departing viewer's references while preserving repeat/modifier/pointer semantics.

## RFB wire robustness — TG-013/TG-014 CLOSED

`hyremote-rfb-wire-robustness-test` drives the production socket parser rather than a test-only parser seam. It covers:

- fragmented RFB 3.8 client-version bytes;
- fragmented representative KeyEvent and PointerEvent messages byte-by-byte;
- accepted dispatch after fragmentation;
- over-limit SetEncodings rejection;
- over-limit ClientCutText rejection;
- unsupported client message rejection followed by server recovery for a new client;
- bounded client-input buffering while handshake progress intentionally prevents consumption.

TCP write boundaries are not assumed to equal receive boundaries; the fixture structures state so the production bound is deterministic regardless of packet coalescing/fragmentation.

## Widgets adapter — TG-001 CLOSED

`hyremote-widgets-capture-test` proves ordinary asynchronous owned-frame capture, geometry/pixel/timing/damage/resize, cancellation after stop and target-loss handling.

`hyremote-widgets-capture-forced-dpr-test` executes the same binary with `QT_SCALE_FACTOR=1.5` and `HYREMOTE_EXPECT_DPR=1.5`, proving the capture contract under deterministic HiDPI scaling rather than merely keeping DPR-aware source assertions dormant.

`hyremote-widgets-input-routing-test` proves ordinary delivery/local coexistence.

`hyremote-widgets-input-backpressure-test` proves pointer-flood coalescing, protected release capacity, unmatched-release handling, shutdown dropping of pending input and exactly-once balancing of already delivered held state.

`hyremote-rfb-widget-disconnect-backpressure-test` composes the real RFB transport with the Widgets adapter under saturation. It requires both Widgets and VNC; VNC-off means no registration, not skip/fail. TG-020 remains closed.

## Quick adapter — TG-002 CLOSED

`hyremote-quick-capture-test` proves ordinary asynchronous owned-frame capture, geometry/pixel/timing/damage/resize and target loss under the software Quick backend used by hosted tests.

`hyremote-quick-capture-forced-dpr-test` repeats that contract with deterministic DPR=1.5.

`hyremote-quick-input-routing-test` and `hyremote-quick-input-backpressure-test` remain separate delivery versus pressure/bounded-dispatch contracts.

## Listener contract after B4

The listener behavior is intentionally three non-overlapping proof layers.

`hyremote-runtime-listener-binding-test` — deterministic T1 Runtime policy, no real listener I/O:

- IPv4 binding-mode inference;
- interface resolution and ambiguity;
- unavailable interface/address behavior;
- locality decisions from an injected interface snapshot.

`hyremote-listener-address-matrix-test` — T2 C++ public facade:

- explicit loopback + selected-port start/stop/rebind lifecycle;
- occupied-port public error mapping;
- unavailable explicit IPv4 failure without silent fallback;
- IPv6 `::1` / `::` rejected as invalid public configuration without mutating accepted IPv4 configuration.

`hyremote-rfb-listener-reachability-test` — real T1 Runtime/RFB socket integration:

- wildcard IPv4 reachable through loopback;
- explicit loopback reachable;
- interface-following rebind/unavailable/recovery with deterministic resolution injection;
- when the host exposes a suitable non-loopback IPv4 interface, wildcard / explicit-address / interface modes are measured against it.

A host without a suitable LAN interface reports that platform fact; deterministic binding policy remains covered by `hyremote-runtime-listener-binding-test`.

## C++ `RemoteAccess` facade

`hyremote-remoteaccess-test` keeps related public facade/state semantics together:

- inert construction and safe loopback/input-off defaults;
- missing target/adapter clean failures;
- configuration forwarding and stopped-only mutation;
- deterministic/idempotent stop;
- move ownership without duplicate Runtime;
- transport-neutral client count events;
- input opt-in independent from view;
- recoverable error acknowledgement/reappearance;
- fatal/Faulted diagnostic retention until explicit stop;
- backend-start failure mapping/cleanup;
- invalid public configuration without mutating the last accepted value;
- unavailable authenticated/encrypted capability fails closed before listener exposure.

`hyremote-remoteaccess-error-ack-test` and `hyremote-remoteaccess-target-loss-test` remain separate because their failure diagnosis is narrower and externally observable.

## Maintained-viewer product fit

`hyremote-v01-rfb-product-fit` remains distinct from lower-layer RFB tests because it proves user-visible interoperability with maintained viewer tooling:

- occupied listener port fails boundedly;
- eight incomplete handshakes expire and capacity recovers;
- abrupt disconnect releases held modifiers/keys/buttons exactly once;
- standard viewer reads framebuffer pixels and drives pointer/buttons/wheel/keyboard/text;
- reconnect works and listener release is clean.

It retains `candidate-evidence` in addition to semantic labels.

## QML

`hyremote-qml-module-test` covers:

- import/type registration and safe defaults;
- invalid config does not mutate accepted value;
- enabled/start failure is transactional;
- component-complete ordering avoids target-binding races;
- target destruction updates the declarative property exactly once.

`hyremote-qml-notifications-test` owns observable typed-notification parity as described above.

QML deploy-helper CTests are T4 package/deploy contracts rather than QML runtime semantics.

## Generic

`hyremote-generic-config-test` isolates launch/config parsing from Qt plugin loading.

`hyremote-generic-plugin-smoke` proves zero-code activation through public Qt plugin APIs while the application's native platform integration remains selected.

Installed Generic consumers remain separate T4 proof because in-tree plugin activation cannot prove package acquisition/deployment.

## QPA

QPA-specific T2 scenarios remain distinct because they depend on the exact Qt 6.8.3 private-ABI frontend:

- proxy/delegate load;
- native delegate semantics;
- QPA config vocabulary;
- automatic Shared Runtime startup;
- native app survival after remote failure;
- QWidget multi-surface behavior;
- popup/transient surface behavior;
- conditional QOpenGLWidget capture;
- Quick multi-window behavior.

`hyremote-qpa-widget-popup-connection-smoke` uses bounded condition/event-driven synchronization; TG-019 remains closed with no retry-based correctness.

## Package / deploy proof layers

These are intentionally not consolidated merely because they all mention deployment:

- QML deploy dispatch: ordinary versus QML-aware helper selection/import roots;
- QPA source/installed deploy matrix: positive combinations plus missing/stale metadata, Qt mismatch, missing package and generator shape;
- installed C++/Generic consumers: clean package acquisition plus real application lifecycle;
- release-readiness deployment/relocation/build-install/package-isolation checks: persistent cross-module T4 contracts;
- `consumer-installed-sdk` release-evidence cell: smallest package/export closure, distinct from adapter applications.

TG-015 path-with-spaces, TG-016 repeated destination and TG-017 Generic-off negative package behavior remain deferred P2 rather than hidden gaps.

## Release authority

Release profile cases remain separate CTests because each accepted/retired version shape is an independent fail-closed authority decision. `hyremote-release-authority-v02-user-first` separately protects V0.2's user-first evidence rule and does not replace generic release-profile acceptance.

## Product E2E assets without current execution authority

`tests/product-e2e/example_product_fit.py`, `qml_product_fit.py`, and `showcase_product_fit.py` still contain useful user-level assertions, but they remain unregistered assets with explicit deferred P2 ownership in TG-010/TG-011. They are not counted as current evidence.

## Final #274 status

TG-001/TG-002/TG-013/TG-014 are closed by executable coverage. TG-003/TG-004/TG-006/TG-007/TG-020 are closed by ownership/metadata/guard work. No confirmed P0/P1 gap remains. Deferred P2 findings stay in `COVERAGE_GAPS.md` and are not silently promoted into this closeout.
