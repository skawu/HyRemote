# HyRemote Test Coverage Gap Register

> Final #274 gap authority, reconciled after Phase D against `develop@bee774e76a48c7a23020b5386b42ac7a2bbb56f8`.
>
> `CONFIRMED` means current implementation/registration/evidence proves a real gap. `CLOSED` findings remain for provenance. `DEFERRED` findings are real lower-severity debt intentionally outside #274 closeout. `FUTURE-OWNED` obligations belong to admitted future capabilities/releases.

## Status / severity

- `CLOSED` — fixed during Phase A or a later #274 slice.
- `CONFIRMED` — trustworthy evidence is absent or the current contract/registration is wrong.
- `DEFERRED` — confirmed lower-severity debt with an owner/rationale, intentionally not expanded into #274.
- `FUTURE-OWNED` — test obligation belongs to a future capability/release.
- P0/P1/P2/P3 describe evidence/correctness risk, not scheduling priority by themselves.

There are no unresolved `REVIEW` findings and no remaining `CONFIRMED` P0/P1 finding.

## Findings

| ID | Status / severity | Area | Final evidence / disposition |
| --- | --- | --- | --- |
| TG-001 | CLOSED P1 | Widgets HiDPI capture | `hyremote-widgets-capture-forced-dpr-test` executes the existing capture binary with deterministic DPR=1.5; ordinary registration remains unchanged. |
| TG-002 | CLOSED P1 | Quick HiDPI capture | `hyremote-quick-capture-forced-dpr-test` provides the equivalent software-Quick DPR=1.5 execution. |
| TG-003 | CLOSED P1 quality | ownership | B1 moved Runtime/RFB/security/network tests, B2 moved Widgets/Quick adapters, B4 split listener facade vs Runtime/RFB reachability without duplicated rows. |
| TG-004 | CLOSED P2 quality | repository ownership | B3 moved `hyremote-build-authority-selftest` registration to top-level repository/T3 while preserving its historical automatic-runtime guard. |
| TG-005 | CLOSED | test docs | `tests/README.md` current-layout truth was corrected and is now final-authority reconciled. |
| TG-006 | CLOSED P1 quality | semantic execution metadata | Phase C #344/#357 added stable additive CTest type/owner/cost labels while preserving names, guards and selector policy. |
| TG-007 | CLOSED P1 quality | release-readiness ownership | T3/T4/T6 semantic ownership is explicit and machine-visible through labels; legacy physical `release-readiness` layout is non-blocking debt rather than semantic ambiguity. |
| TG-008 | CLOSED | authoritative catalog | `TEST_CATALOG.md` is final-authority reconciled and configure-time checked against every registered CTest identity, preventing silent catalog drift. |
| TG-009 | CLOSED P1 evidence | candidate RFB product fit | `hyremote-v01-rfb-product-fit` is registered fail-closed with `candidate-evidence`; maintained-viewer/mainline evidence remains complementary. |
| TG-010 | DEFERRED P2 | historical app E2E | `tests/product-e2e/example_product_fit.py` retains unique app-level assertions but historical fixture terminology/no execution authority must be repaired before activation. Owner: future non-fast T5 E2E cleanup. |
| TG-011 | DEFERRED P2 | preview/showcase E2E | `qml_product_fit.py` and `showcase_product_fit.py` contain distinct user-level assertions but no explicit hosted/non-fast owner. Keep assets; assign future T5 E2E authority. |
| TG-012 | CLOSED P0 evidence | clean-consumer acquisition | Shared per-cache-element acquisition audit plus `hyremote-acquisition-audit-self-test` closes the false-pass path. |
| TG-013 | CLOSED P1 robustness | fragmented RFB input | `hyremote-rfb-wire-robustness-test` fragments RFB 3.8 version and representative Key/Pointer messages byte-by-byte through the production socket transport and proves accepted dispatch. |
| TG-014 | CLOSED P1 robustness/security | malformed/oversized RFB input | The same Runtime/RFB test drives over-limit SetEncodings, over-limit ClientCutText, unsupported input/recovery and bounded client-input buffering fail-closed branches. |
| TG-015 | DEFERRED P2 | paths with spaces | Static quoting defenses exist; no full source/build/install/deploy run from a path containing spaces. Owner: bounded future T4 robustness case. |
| TG-016 | DEFERRED P2 | repeated deployment | Existing fixtures prove clean deployment to fresh destinations, not repeated install/deploy into the same destination. Owner: future T4 idempotence case. |
| TG-017 | DEFERRED P2 | Generic capability-off | No deterministic package/deploy negative proving a Generic-disabled SDK reports absence/rejects `GENERIC` deployment for the intended reason. Owner: one future T4 negative, not an all-flags matrix. |
| TG-018 | DEFERRED P2 intent | stale V1-only vocabulary | Some durable public/package comments/assertion messages still use V1-only wording for cross-release contracts. Semantics remain valid; wording cleanup is maintenance debt. |
| TG-019 | CLOSED P1 quality | QPA popup timing | Fixed sleeps were replaced by bounded condition/event waits; fresh qpa-only Linux/Windows evidence passed without retry. |
| TG-020 | CLOSED P1 build matrix | RFB+Widgets capability guard | `hyremote-rfb-widget-disconnect-backpressure-test` requires Widgets + VNC at registration/build time; Widgets-on/VNC-off has zero registration rather than a skip/unbuildable target. |
| TG-021 | CLOSED P1 evidence | V0.1 example capability guard | `hyremote-v01-example-smoke` requires C++ API + Runtime + Python; qpa-only hosted evidence proves it is absent when C++ is off while the remaining suite stays nonzero/pass. |

## Verified coverage — do not duplicate

| Area | Existing authority |
| --- | --- |
| clean-consumer acquisition | `hyremote-acquisition-audit-self-test` + shared cache-element audit |
| maintained-viewer RFB product fit | `hyremote-v01-rfb-product-fit` + maintained-viewer harness |
| occupied RFB port / stalled handshake capacity / viewer reconnect | maintained-viewer product-fit scenarios |
| abrupt disconnect held-input cleanup | maintained-viewer product fit + lower-layer multi-client ownership tests |
| VNC auth positive/negative/no-downgrade/timeout | conditional `hyremote-rfb-vnc-auth-handshake-test` |
| concurrent viewer held state | `hyremote-rfb-multi-client-input-test` |
| fragmented/malformed/oversized RFB input | `hyremote-rfb-wire-robustness-test` |
| saturated adapter disconnect cleanup | `hyremote-rfb-widget-disconnect-backpressure-test` under Widgets + VNC |
| Widgets HiDPI capture | ordinary + `hyremote-widgets-capture-forced-dpr-test` |
| Quick HiDPI capture | ordinary + `hyremote-quick-capture-forced-dpr-test` |
| Widgets/Quick input pressure | adapter-specific backpressure tests |
| Core lifecycle/concurrency | `hyremote-core-test-session-lifecycle` scenario family |
| installed C++ | `hyremote-cpp-installed-consumers` |
| installed Generic | `hyremote-generic-installed-consumers` |
| minimal public package closure | `consumer-installed-sdk` release-evidence cell, intentionally distinct from adapter apps |
| QPA popup synchronization | bounded waits + fresh hosted qpa-only evidence |
| C++-disabled adoption registration | `hyremote-v01-example-smoke` guard + qpa-only evidence |
| catalog completeness | configure-time reconciliation of CMake `TESTS` properties against exact identities in `TEST_CATALOG.md` |

## Future-owned obligations

| Capability | Owner | Test direction |
| --- | --- | --- |
| VeNCrypt/TLS secure transport | #143 → #271 | positive/negative negotiation, no downgrade, certificate/key failures, maintained-viewer/customer-trial E2E; existing preflight proves feasibility only |
| Runtime typed notifications evolution | #259 follow-up as needed | extend state/client/error ordering only when new public events are admitted; current Runtime/QML notification tests cover the existing contract |
| authenticated Session Registry | #170 / V0.2.1 | session identity/admission/snapshot/events/terminate/cleanup |
| accumulated damage/ZRLE/per-viewer delivery | #261 → #144/#175 | accumulation, negotiation, resize invalidation, encoding equivalence/interoperability |
| Qt 5.15 adaptation | #260/#265/#57 | exact anchor/toolchain seams after preflight |
| RK3588/EGLFS | #236/#154 | physical native display/input + remote coexistence |

## Observability / hardening candidates

- TG-O01: Linux ASan+UBSan non-fast lane if clean enough.
- TG-O02: bounded deterministic concurrency/stress lane; never retry correctness failures.
- TG-O03: coverage reporting for blind-spot observability, never a vanity percentage gate.
- TG-O04: parser fuzz/property tests only after a bounded deterministic parsing seam exists; do not redesign production architecture merely to add fuzzing.

## Final #274 admission statement

No confirmed P0/P1 gap remains. TG-010/TG-011/TG-015/TG-016/TG-017/TG-018 remain explicit deferred P2 findings with owner/rationale. Future-owned feature obligations remain with their existing release/issues. This register therefore permits #274 closure once the final authority-reconciliation PR passes its exact-head review gate.
