# HyRemote Test Scenario Appendix

> Scenario-level companion to `TEST_CATALOG.md`, refreshed against `develop@e3fcf2fd06f8feca00b83b6c49c48588264cfd81`.
>
> This records meaningful cases inside multi-scenario executables/scripts. It creates no new tests and changes no selection.

## C++ `RemoteAccess` facade

`hyremote-remoteaccess-test` — T2/C++ — KEEP as one facade/state-contract executable.

Necessary scenarios include:
- inert construction and safe loopback/input-off defaults;
- missing target/adapter clean failures;
- configuration forwarding and stopped-only mutation;
- deterministic/idempotent stop;
- move ownership without duplicate Runtime;
- transport-neutral client-count events;
- input opt-in independent from view;
- recoverable error acknowledgement and reappearance;
- fatal/Faulted diagnostic retention until explicit stop;
- backend-start failure mapping/cleanup;
- invalid public configuration without mutating last accepted value;
- unavailable `AuthenticatedEncrypted` fails closed before listener exposure.

A failure invalidates public C++ integration semantics, not a private RFB implementation detail.

## Core Session lifecycle/concurrency

`hyremote-core-test-session-lifecycle` — T1/Core — KEEP grouped.

It protects related state-machine/regression families: documented transitions; repeated start/stop; partial capture/transport start cleanup; recoverable/fatal event escalation; invalid configuration; capture in-flight bounds; backend exceptions; concurrent stop ownership; stop during blocked capture/transport start; component replacement races; stop-win windows before Running; concurrent start/stop deadlock regression; worker startup during Starting; replacement rejection in Starting/Faulted/Stopping; and deterministic exception cleanup.

Splitting by file size would weaken shared race fixtures; only split if ownership/diagnosis materially improves.

## RFB authentication, viewers and parser coverage

### Conditional VNC authentication tests

`hyremote-vnc-auth-test` proves the VNC Auth crypto primitive when Security capability exists.

`hyremote-rfb-vnc-auth-handshake-test` separately proves wire behavior: correct credential, wrong credential, no downgrade to None, bounded stalled auth and deliberate insecure profile. These are not TLS tests.

### Multi-viewer state

`hyremote-rfb-multi-client-input-test` proves two viewers cannot prematurely release each other's held key/button state, disconnect removes only the departing viewer's references, repeat semantics survive, and final release uses correct modifiers/pointer state.

### Candidate maintained-viewer product fit — TG-009 CLOSED

`hyremote-v01-rfb-product-fit` now registers `rfb_product_fit.py` as `candidate-evidence` (#281) and fails closed if required Python/viewer tooling is unavailable. Mainline hosted execution remains complementary.

Its distinct user-level scenarios are:
- occupied listener port fails boundedly;
- eight incomplete handshakes expire and capacity recovers;
- abrupt disconnect releases held modifiers/keys/buttons exactly once;
- standard `vncdotool` reads framebuffer pixels and drives pointer/buttons/wheel/keyboard/text;
- reconnect works and listener release is clean.

Do not duplicate these end-to-end behaviors in another harness merely to change ownership.

### Confirmed parser gaps

Current implementation buffers arbitrary socket fragments and enforces input/encoding/cut-text limits, but registered tests do not deliberately split all protocol fields/messages or drive all oversize reject branches. TG-013/TG-014 remain Phase-D gaps.

## Widgets adapter

`hyremote-widgets-capture-test` — Runtime/Widgets after #327/#328. It proves asynchronous owned frame, geometry/pixel/timing/damage/resize, cancellation after stop and target-loss handling. TG-001 remains because the source expects a forced DPR=1.5 run that is not registered.

`hyremote-widgets-input-backpressure-test` — Runtime/Widgets after #327/#328. It proves pointer-flood coalescing, protected release capacity, unmatched-release handling, shutdown dropping of pending input, and exactly-once balancing of already-delivered held state.

`hyremote-widgets-input-routing-test` — Runtime/Widgets after #327/#328; ordinary delivery/local coexistence remains distinct from saturation/backpressure.

`hyremote-rfb-widget-disconnect-backpressure-test` — Runtime/RFB+Widgets after #327/#328. It requires both `HYREMOTE_REMOTEACCESS_WITH_WIDGETS` and `HYREMOTE_WITH_VNC`; TG-020 is closed and VNC-off leaves the RFB-specific test absent from registration.

## Quick adapter

`hyremote-quick-capture-test` — Runtime/Quick after #327/#328; asynchronous owned frame, geometry/pixel/timing/damage/resize and target-loss. TG-002 remains for missing forced-DPR run.

`hyremote-quick-input-routing-test` and `hyremote-quick-input-backpressure-test` remain distinct Quick delivery/pressure contracts and are Runtime/Quick after #327/#328.

## Listener/address matrix — SPLIT

Current `hyremote-listener-address-matrix-test` combines two owners.

C++ facade rows stay T2:
- explicit loopback + selected port lifecycle;
- occupied-port public error;
- unavailable address fails without silent fallback.

Runtime/RFB rows move/extract when ownership is refactored:
- IPv4 wildcard reachability;
- IPv6 loopback/wildcard reachability;
- measured dual-stack behavior;
- success must bind requested scope, failure must not fall back silently.

Do not duplicate rows during the split.

## QML proof layers

`hyremote-qml-module-test` — T2 KEEP:
- import/type registration and safe defaults;
- invalid config does not mutate accepted value;
- enabled/start failure is transactional;
- component-complete ordering avoids target-binding race;
- target destruction updates declarative property exactly once.

Installed QML evidence — T4 KEEP — proves package/import/deploy closure.

`qml_product_fit.py` — T5 KEEP asset — contains real-viewer client-count, view-only isolation, stop→configure→start, control input and reconnect scenarios, but TG-011 records missing execution authority.

## QPA behavior and popup stabilization

Unique T2 QPA scenarios remain necessary: native delegate load/semantics; QPA config; automatic Shared Runtime startup; native app survival after remote failure; QWidget multi-surface; popup/transient; conditional QOpenGLWidget capture; Quick multi-window.

### TG-019 CLOSED

`hyremote-qpa-widget-popup-connection-smoke` previously used fixed event-pump delays as synchronization. #282/#296 replaced those assumptions with bounded condition/event-driven waits for popup appearance/removal while preserving the same one-session composite-canvas contract.

Fresh first-attempt qpa-only CI run `35676277901`:
- Linux: popup PASS 0.73s, 59/59 selected tests PASS;
- Windows: popup PASS 1.21s, 58/58 selected tests PASS.

No automatic retry, skip or product-code workaround was used.

## Deploy-helper proof layers

These overlap by subject but not by failure class:

- root static deploy/package scan — permanent required/forbidden implementation/package structure;
- QML dispatch fixtures — ordinary vs QML-aware Qt deploy API and import-path preservation;
- QPA source/installed matrix — positive combinations plus missing/stale metadata, Qt mismatch, missing package, generator shape and Linux relocation;
- exact-SHA release evidence cell — actual clean install/deploy outputs.

Phase B may consolidate plumbing, not semantic proof layers.

## Repository/build/acquisition authority

`hyremote-build-authority-selftest` protects canonical build argument/config behavior and current security truth; it should move to T3 repository ownership.

`hyremote-acquisition-audit-self-test` — TG-012 CLOSED — drives the same shared per-cache-element logic used by clean-consumer evidence, including mixed `<run-prefix>;<forbidden-source/build-path>` values that the old line-level audit could miss.

## V0.1 adoption smoke — TG-021 CLOSED

`hyremote-v01-example-smoke` installs the SDK and independently builds/runs canonical 01/02/03 paths. It proves developer adoption, not full remote-control correctness.

Phase A discovered that a QPA-only build registered this combined smoke even though 01/02 require the C++ API. #298/#300 changed registration to:

- `HYREMOTE_BUILD_CPP_API`;
- Runtime target exists;
- Python interpreter exists.

The first fresh qpa-only #296 run then proved the smoke is absent on both platforms while the remaining suites execute nonzero/pass. The longer-term decision whether to split C++ 01/02 and Generic 03 into separate capability-owned CTests remains Phase B/C, not a current blocker.

## Product E2E assets without current execution authority

`example_product_fit.py` retains useful app-level framebuffer, view-only→control, buttons/wheel/modifiers/text, reconnect and listener-release assertions, but historical fixture labels must be retargeted before activation (TG-010).

`showcase_product_fit.py` protects showcase-specific client/input/reconnect lifecycle; `qml_product_fit.py` protects declarative user-flow semantics. Both remain T5 assets with TG-011 execution-ownership debt.

## Release scope/profile scenarios

T6 cases intentionally remain separate for:
- development all-capability and runtime-only sentinels;
- retired 0.0.1.0/0.0.2.0/0.0.3.0 rejection;
- representable V0.1/V0.2/V0.3/V0.4 and V0.4 maintenance;
- V1.0 all/C++-only/Generic-only subsets.

#277 expanded Feature-release scenario coverage inside authority scripts; #281 separately added candidate RFB product fit.

## V0.2 preflight scenarios — separate PRE evidence

`hyremote-tls-transition-preflight` is a standalone test project, not a product CTest. It proves on Qt 6.8.3/OpenSSL:
- plaintext phase on one socket then TLS transition on that same socket;
- OpenSSL backend identity and TLS>=1.2;
- certificate/key prevalidation;
- bounded handshake timeout with listener survival;
- no fallback on failure;
- reconnect and deterministic shutdown.

VeNCrypt probe scripts supply bounded protocol/viewer feasibility evidence for #258. These preflights de-risk #143 but do not replace product security/customer-trial tests.

## Phase-A overlap conclusions

- QML module / installed QML / QML product-fit: all have distinct T2/T4/T5 contracts.
- root/QML/QPA/release deploy proof layers: retain semantics, move persistent contracts to T4.
- acquisition audit: KEEP shared #280 regression; TG-012 closed.
- adoption smoke and viewer product fit: both KEEP; one proves SDK use, one remote behavior.
- RFB candidate product fit: TG-009 closed by #281/#229.
- QPA popup: TG-019 closed by #296.
- C++-disabled adoption registration: TG-021 closed by #300/#296 evidence.
- listener matrix: SPLIT by semantic owner.
- RFB+Widgets disconnect/backpressure: KEEP Runtime/RFB+Widgets; TG-020 closed by #327/#328 capability guard.
- fragmented/malformed RFB: TG-013/TG-014 remain.
