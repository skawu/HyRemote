# HyRemote Test Coverage Gap Register

> Phase A authority for #274, refreshed against `develop@e3fcf2fd06f8feca00b83b6c49c48588264cfd81`.
>
> `CONFIRMED` means current code/registration/evidence proves a real gap. `CLOSED` findings remain for provenance. Future features are `FUTURE-OWNED`, not current defects.

## Status / severity

- `CLOSED` — found during Phase A and fixed before the audit merged or by a later #274 slice.
- `CONFIRMED` — trustworthy evidence is absent or the test contract/registration is wrong now.
- `FUTURE-OWNED` — test obligation belongs to an admitted future capability/release.
- P0/P1/P2/P3 indicate release/evidence risk, not implementation priority by themselves.

There are no unresolved `REVIEW` findings.

## Findings

| ID | Status / severity | Area | Evidence and required action |
| --- | --- | --- | --- |
| TG-001 | CONFIRMED P1 | Widgets HiDPI capture | `test_widgets_capture.cpp` expects a forced DPR=1.5 execution (`QT_SCALE_FACTOR=1.5`, `HYREMOTE_EXPECT_DPR=1.5`), but current 95/94 registration has only the ordinary Widgets capture test. Phase D: restore deterministic forced-DPR execution before changing product code. |
| TG-002 | CONFIRMED P1 | Quick HiDPI capture | Same problem for `test_quick_capture.cpp`; no forced-DPR second registration. Phase D. |
| TG-003 | CONFIRMED P1 quality | ownership | Runtime/RFB/security/network/Widgets/Quick tests live under `src/integrations/cpp/tests` and reach Runtime internals. Phase B: move/split by semantic owner without changing behavior. |
| TG-004 | CONFIRMED P2 quality | repository ownership | `hyremote-build-authority-selftest` is repository/build governance but is registered by Runtime tests. Phase B: move T3 registration. |
| TG-005 | CLOSED | test docs | Stale `tests/README.md` directory map corrected by Phase A. |
| TG-006 | CONFIRMED P1 quality | semantic execution metadata | No repository-wide type/owner/cost CTest labels; selectors still rely heavily on names/path classification. Phase C: one semantic label/tier mechanism; capability guards remain authoritative. |
| TG-007 | CONFIRMED P1 quality | release-readiness ownership | `tests/release-readiness` mixes permanent T3/T4 contracts with true candidate T6 gates. Phase B/C split by semantic owner. |
| TG-008 | CLOSED | authoritative catalog | Phase A establishes catalog/scenario/matrix/execution/gap authority. |
| TG-009 | CLOSED P1 evidence | candidate RFB product fit | #281 registered `hyremote-v01-rfb-product-fit` as fail-closed `candidate-evidence` CTest and preserved maintained-viewer harness behavior; #229 completed. Mainline hosted authority remains complementary. |
| TG-010 | CONFIRMED P2 | historical app E2E | `tests/product-e2e/example_product_fit.py` still uses historical `widgets-basic`/`quick-basic` terminology and has no current execution authority. KEEP contract; retarget to canonical fixture/path before activation. |
| TG-011 | CONFIRMED P2 | preview/showcase E2E | `qml_product_fit.py` and `showcase_product_fit.py` contain distinct user-level assertions but currently have no explicit hosted/non-fast execution owner. KEEP assets; Phase C/D assigns authority. |
| TG-012 | CLOSED P0 evidence | clean-consumer acquisition | #280 replaced line-level cache skipping with shared per-element acquisition audit and added `hyremote-acquisition-audit-self-test`; #230 completed. |
| TG-013 | CONFIRMED P1 robustness | fragmented RFB input | Runtime buffers arbitrary socket fragments, but current registered tests/product-fit helpers do not deliberately split protocol fields/messages at boundaries. Phase D: bounded fragmentation tests. |
| TG-014 | CONFIRMED P1 robustness/security | malformed/oversized RFB input | Limits such as `kMaxClientInputBytes`, `kMaxEncodings`, `kMaxCutTextBytes` exist but deterministic tests do not drive all reject/cleanup branches. Phase D negative tests. |
| TG-015 | CONFIRMED P2 | paths with spaces | Static quoting defenses exist; no real source/build/install/deploy execution from a path containing spaces was found. Phase D T4 Win/Linux case. |
| TG-016 | CONFIRMED P2 | repeated deployment | Existing fixtures prove clean deployment into fresh destinations, not repeated install/deploy into the same destination. Phase D idempotence case. |
| TG-017 | CONFIRMED P2 | Generic capability-off | No equivalent deterministic package/deploy negative proving a Generic-disabled SDK reports absence and rejects `GENERIC` deployment for the intended reason. Add one bounded T4 negative, not an all-flags matrix. |
| TG-018 | CONFIRMED P2 intent | stale V1-only vocabulary | Some durable public/package test comments/assertion messages still call cross-release contracts “V1”. Keep assertions; clean wording/ownership in Phase B. |
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
| saturated adapter disconnect cleanup | Runtime-owned `hyremote-rfb-widget-disconnect-backpressure-test`, registered only for Widgets + VNC; TG-020 closed |
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

No open Phase-A finding currently reopens V0.1. TG-009/TG-012/TG-019/TG-020/TG-021 were closed by their owning issues/slices. Remaining gaps belong to later #274 phases unless restored deterministic coverage exposes a separate product/security/release defect; such a defect must be handed to its owning release issue rather than silently expanding #274.
