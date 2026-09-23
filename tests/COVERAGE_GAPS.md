# HyRemote Test Coverage Gap Register

> Phase A authority for #274, refreshed against `develop@e3fcf2fd06f8feca00b83b6c49c48588264cfd81`.
>
> `CONFIRMED` means current code/registration/evidence proves a real gap. `CLOSED` findings remain for provenance. `DEFERRED` findings remain real but are intentionally outside #274 closeout. Future features are `FUTURE-OWNED`, not current defects.

## Status / severity

- `CLOSED` — found during Phase A and fixed before the audit merged or by a later #274 slice.
- `CONFIRMED` — trustworthy evidence is absent or the test contract/registration is wrong now.
- `DEFERRED` — confirmed lower-severity debt with an owner/rationale, intentionally not expanded into #274 closeout.
- `FUTURE-OWNED` — test obligation belongs to an admitted future capability/release.
- P0/P1/P2/P3 indicate release/evidence risk, not implementation priority by themselves.

There are no unresolved `REVIEW` findings.

## Findings

| ID | Status / severity | Area | Evidence and required action |
| --- | --- | --- | --- |
| TG-001 | CLOSED P1 | Widgets HiDPI capture | Phase D #345 adds `hyremote-widgets-capture-forced-dpr-test`, a second execution of the existing capture binary with `QT_SCALE_FACTOR=1.5` and `HYREMOTE_EXPECT_DPR=1.5`; the ordinary registration remains unchanged. |
| TG-002 | CLOSED P1 | Quick HiDPI capture | Phase D #345 adds the equivalent `hyremote-quick-capture-forced-dpr-test` with software Quick backend plus deterministic DPR=1.5 expectation. |
| TG-003 | CLOSED P1 quality | ownership | Phase B closed the misownership without changing behavior: B1 moved Runtime/RFB/security/network tests, B2 moved Widgets/Quick adapter tests to Runtime ownership, and B4 split listener facade vs Runtime/RFB reachability by semantic owner. |
| TG-004 | CLOSED P2 quality | repository ownership | B3 moved `hyremote-build-authority-selftest` registration from Runtime tests to top-level repository/T3 CTest ownership while preserving its effective automatic-runtime guard; the configure-local capability is reset fail-closed before optional Runtime configuration. |
| TG-005 | CLOSED | test docs | Stale `tests/README.md` directory map corrected by Phase A. |
| TG-006 | CLOSED P1 quality | semantic execution metadata | Phase C #344/#357 adds additive executable CTest type/owner/cost labels while preserving names, guards and existing selection policy; Win/Linux evidence shows nonzero semantic label classes. |
| TG-007 | CLOSED P1 quality | release-readiness ownership | Phase A/B5 establish the real T3/T4/T6 semantic owners and Phase C #344/#357 makes them machine-visible through labels without bulk physical/selector churn. Legacy physical layout remains non-blocking deferred debt. |
| TG-008 | CLOSED | authoritative catalog | Phase A establishes catalog/scenario/matrix/execution/gap authority. |
| TG-009 | CLOSED P1 evidence | candidate RFB product fit | #281 registered `hyremote-v01-rfb-product-fit` as fail-closed `candidate-evidence` CTest and preserved maintained-viewer harness behavior; #229 completed. Mainline hosted authority remains complementary. |
| TG-010 | DEFERRED P2 | historical app E2E | `tests/product-e2e/example_product_fit.py` still uses historical `widgets-basic`/`quick-basic` terminology and has no current execution authority. KEEP contract; retarget to canonical fixture/path before activation in a separately owned E2E cleanup. |
| TG-011 | DEFERRED P2 | preview/showcase E2E | `qml_product_fit.py` and `showcase_product_fit.py` contain distinct user-level assertions but currently have no explicit hosted/non-fast execution owner. KEEP assets; assign a future non-fast E2E owner rather than expanding #274. |
| TG-012 | CLOSED P0 evidence | clean-consumer acquisition | #280 replaced line-level cache skipping with shared per-element acquisition audit and added `hyremote-acquisition-audit-self-test`; #230 completed. |
| TG-013 | CLOSED P1 robustness | fragmented RFB input | Phase D #345 adds `hyremote-rfb-wire-robustness-test`, which fragments the real RFB 3.8 client version and representative Key/Pointer messages byte-by-byte through the production socket transport and proves accepted dispatch. |
| TG-014 | CLOSED P1 robustness/security | malformed/oversized RFB input | The same Runtime/RFB test drives production fail-closed branches for over-limit SetEncodings, over-limit ClientCutText, unsupported messages with recovery, and the bounded client-input burst guard. |
| TG-015 | DEFERRED P2 | paths with spaces | Static quoting defenses exist; no real source/build/install/deploy execution from a path containing spaces was found. Retain as separately owned T4 robustness debt. |
| TG-016 | DEFERRED P2 | repeated deployment | Existing fixtures prove clean deployment into fresh destinations, not repeated install/deploy into the same destination. Retain as separately owned T4 idempotence debt. |
| TG-017 | DEFERRED P2 | Generic capability-off | No equivalent deterministic package/deploy negative proving a Generic-disabled SDK reports absence and rejects `GENERIC` deployment for the intended reason. Retain one bounded future T4 negative; do not expand to an all-flags matrix. |
| TG-018 | DEFERRED P2 intent | stale V1-only vocabulary | Some durable public/package test comments/assertion messages still call cross-release contracts “V1”. Assertions remain valid; wording cleanup is non-blocking maintenance debt. |
| TG-019 | CLOSED P1 quality | QPA popup timing | #282/#296 replaced fixed `350/300 ms` synchronization with bounded condition/event-driven waits. Fresh first hosted qpa-only run `35676277901`: Linux 72 discovered / 59 executed / 59 PASS, popup 0.73s; Windows 71 / 58 / 58 PASS, popup 1.21s. No retry. |
| TG-020 | CLOSED P1 build matrix | RFB+Widgets capability guard | #327/#328 moved `hyremote-rfb-widget-disconnect-backpressure-test` to Runtime ownership and made registration/build require both `HYREMOTE_REMOTEACCESS_WITH_WIDGETS` and `HYREMOTE_WITH_VNC`; Widgets-on/VNC-off therefore has no RFB-specific registration instead of a runtime skip/unbuildable target. |
| TG-021 | CLOSED P1 evidence | V0.1 example capability guard | QPA-only lanes used to register the combined 01/02/03 adoption smoke even though 01/02 require C++ API. #298/#300 now require C++ API + Runtime target + Python. #296 first fresh qpa-only hosted run proves smoke absent on both platforms while suites remain nonzero/pass. |

## Verified coverage — do not duplicate

| Area | Existing authority |
| --- | --- |
| clean-consumer acquisition judgement | `hyremote-acquisition-audit-self-test` + shared #280 audit |
| maintained-viewer RFB candidate fit | registered `hyremote-v01-rfb-product-fit` (`candidate-evidence`) plus retained mainline hosted harness; TG-009 closed |
| occupied RFB port | `rfb_product_fit.py::verify_occupied_port_failure` |
| max-eight stalled handshakes/recovery | `verify_handshake_slots_expire` |
| abrupt disconnect held-input cleanup | `verify_abrupt_disconnect_releases_input` |
| framebuffer/input/reconnect with maintained viewer | `verify_standard_client` via `vncdotool` |
| VNC Auth positive/negative/no-downgrade/timeout | conditional `hyremote-rfb-vnc-auth-handshake-test` |
| concurrent viewer held state | `hyremote-rfb-multi-client-input-test` |
| fragmented/malformed/oversized RFB input | `hyremote-rfb-wire-robustness-test`; TG-013/TG-014 closed |
| saturated adapter disconnect cleanup | Runtime-owned `hyremote-rfb-widget-disconnect-backpressure-test`, registered only for Widgets + VNC; TG-020 closed |
| Widgets HiDPI capture | ordinary + `hyremote-widgets-capture-forced-dpr-test`; TG-001 closed |
| Quick HiDPI capture | ordinary + `hyremote-quick-capture-forced-dpr-test`; TG-002 closed |
| Widgets input pressure | coalescing, protected releases, shutdown balancing in `hyremote-widgets-input-backpressure-test` |
| Core lifecycle/concurrency | `hyremote-core-test-session-lifecycle` families |
| installed C++ | `hyremote-cpp-installed-consumers` |
| installed Generic | `hyremote-generic-installed-consumers` |
| minimal package closure | `installed-sdk` retained as distinct minimal public-package smoke |
| QPA popup synchronization | TG-019 closed by #296 bounded wait + fresh Win/Linux qpa-only evidence |
| C++-disabled adoption registration | TG-021 closed by #300 + #296 qpa-only evidence |

## Future-owned obligations

| Capability | Owner | Test direction |
| --- | --- | --- |
| VeNCrypt/TLS secure transport | #143 → #271 | positive/negative negotiation, no downgrade, certificate/key failures, maintained-viewer/customer-trial E2E. #258/#297 already provide bounded preflight evidence for TLS transition feasibility; that does not replace product tests. |
| Runtime typed notifications | #259 | state/client/error ordering and public C++/QML observability |
| authenticated Session Registry | #170 / V0.2.1 | session identity/admission/snapshot/events/terminate and cleanup |
| accumulated damage/ZRLE/per-viewer delivery | #261 → #144/#175 | accumulation, negotiation, resize invalidation, encoding equivalence/interoperability |
| Qt 5.15 adaptation | #260/#265/#57 | exact anchor/toolchain seams after preflight |
| RK3588/EGLFS | #236/#154 | physical native display/input + remote coexistence |

## Observability/hardening candidates

- TG-O01: Linux ASan+UBSan non-fast lane if clean enough.
- TG-O02: bounded deterministic concurrency/stress lane; never use retry as correctness.
- TG-O03: coverage report for blind-spot observability, not a percentage gate.
- TG-O04: parser fuzz/property tests only after a bounded deterministic parsing seam exists.

## Release-admission handoff

#274 closeout now has no remaining confirmed P0/P1 gap: Phase B closed TG-003/TG-004/TG-020, Phase C closed TG-006/TG-007, and Phase D closes TG-001/TG-002/TG-013/TG-014 through deterministic executable coverage. TG-010/TG-011/TG-015/TG-016/TG-017/TG-018 remain explicit `DEFERRED P2` findings with their semantic owner/rationale intact; they do not block #274 closure. Future-owned feature obligations remain with their existing release/issues.
