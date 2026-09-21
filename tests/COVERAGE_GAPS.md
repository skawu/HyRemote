# HyRemote Test Coverage Gap Register

> Phase A audit for #274. Repository facts refreshed against `develop` at `6dc244a8717bdc520544c71d9756499033857c73`.
>
> A `GAP` means trustworthy evidence is absent for an existing invariant, a claimed scenario is not actually executed, or a meaningful robustness contract has no deterministic owner. Future features remain `FUTURE-OWNED` and are not current defects.

## Severity/status

- `CONFIRMED`: code/registration/hosted execution proves the gap now.
- `FUTURE-OWNED`: planned capability; test obligation belongs to its future release.
- `OBSERVABILITY`: quality improvement, not itself product acceptance.
- `P0`: current release can false-pass or a current security/correctness gate is untrustworthy.
- `P1`: important current invariant/evidence path is untested or misleading.
- `P2`: robustness/maintainability gap with no evidence of current release unsafety.
- `P3`: optional quality improvement.

There are no unresolved `REVIEW` gaps after Phase A.

## Confirmed gaps

| ID | Severity | Area | Evidence / why it matters | Owner / action |
| --- | --- | --- | --- | --- |
| TG-001 | P1 | Widgets HiDPI capture | `test_widgets_capture.cpp` explicitly expects a second `QT_SCALE_FACTOR=1.5` + `HYREMOTE_EXPECT_DPR=1.5` execution to prevent DPR=1 vacuous assertions. Current CMake and hosted 93/92 inventory contain only one normal Widgets capture test. | Runtime/Widgets, #274 Phase D. Restore deterministic forced-DPR execution first; product code changes only if it fails. |
| TG-002 | P1 | Quick HiDPI capture | `test_quick_capture.cpp` makes the same forced-DPR claim; only one normal software/offscreen Quick capture test is registered/executed. | Runtime/Quick, Phase D. |
| TG-003 | P1 quality | ownership | Runtime/RFB/network/Widgets/Quick tests live under `src/integrations/cpp/tests` and reach Runtime internals. | Phase B: move/split by semantic owner without behavior changes. |
| TG-004 | P2 quality | repository ownership | `hyremote-build-authority-selftest` is repository/build-governance behavior registered from Runtime tests. | Phase B: move registration to T3 repository owner. |
| TG-005 | P2 quality | test docs | Previous `tests/README.md` omitted active test areas and named historical `third_party`. | Closed by #276 documentation. |
| TG-006 | P1 quality | execution semantics | No repository-wide semantic CTest labels for type/owner/cost/release relevance; selectors rely heavily on names/path classification. | Phase C: add one label/tier mechanism while preserving capability guards and #250 zero-test fail-closed behavior. |
| TG-007 | P1 quality | release-readiness ownership | `tests/release-readiness` mixes T3/T4 persistent contracts with true T6 candidate truth. | Phase B/C: relocate/split according to `TEST_CATALOG.md`. |
| TG-008 | P2 quality | authoritative catalog | Before #276 there was no full necessity/owner/overlap/execution map. | Closed by catalog/scenarios/matrix/execution-baseline set; Phase C may make metadata machine-checkable. |
| TG-009 | P1 evidence | real RFB product-fit execution | `rfb_product_fit.py` implements occupied-port, max-eight handshake expiry, abrupt held-input cleanup and maintained-viewer framebuffer/input/reconnect/stop checks, but it is not a CTest and is absent from normal hosted/release-evidence execution. #229 still requires this evidence category for the exact V0.1 candidate. | Reported to #229 in comment `5758145724`; #274 later supplies semantic T5 ownership, not the V0.1 release fix. |
| TG-010 | P2 quality | historical E2E authority | `tests/product-e2e/example_product_fit.py` retains historical `widgets-basic`/`quick-basic` labels while canonical V0.1 paths are learning 01/02/03, and the script has no execution authority. Its app-level standard-viewer assertions remain useful. | KEEP T5 contract; retarget to canonical 01/02 and define a non-fast/candidate owner. Never resurrect removed flat examples. |
| TG-011 | P2 quality | preview/showcase E2E ownership | `qml_product_fit.py` and `showcase_product_fit.py` contain meaningful viewer/user-flow assertions but are unregistered and not release-evidence cells. | KEEP as T5 non-fast harnesses; Phase C/D gives explicit manual/e2e execution ownership. Not V0.1 primary-path blockers. |
| TG-012 | P1 release evidence | clean-consumer acquisition audit | `run_release_evidence.cmake` skips an entire cache line if it contains `RUN_DIR`; a mixed `<run-prefix>;<forbidden-source/build-path>` entry can evade source/build hit counting. | Returned to reopened #230 in comment `5758106114`; narrow fix + negative regression belongs to V0.1 SDK evidence, not #276. |
| TG-013 | P1 robustness | fragmented RFB input | RFB worker buffers arbitrary socket fragments by protocol phase, but inspected registered tests and dormant product-fit helpers send complete version/security/client messages. No deterministic registered scenario deliberately splits fields/messages at boundaries. | Runtime/RFB Phase D: bounded split-at-boundary tests. Not automatically a V0.1 blocker absent failing restored evidence. |
| TG-014 | P1 robustness/security | malformed/oversized RFB input | Runtime enforces `kMaxClientInputBytes`, `kMaxEncodings`, `kMaxCutTextBytes`, but registered tests do not deliberately drive those reject branches or unsupported-message cleanup. | Runtime/RFB Phase D: deterministic negative parser/bound tests. |
| TG-015 | P2 package robustness | paths containing spaces | Deploy/package code contains explicit quoting/path-splitting defenses, but no test executes source/build/install/deploy from a path containing spaces. Static token assertions are not execution proof. | T4 Phase D: platform-realistic Windows/Linux path-with-spaces consumer/deploy case. |
| TG-016 | P2 package robustness | repeat deployment/idempotence | QPA deploy fixtures remove their binary dir before each case; release evidence uses a unique run/prefix. Clean deployment is proven, repeated install/deploy to the same destination is not. | T4 Phase D: run the same install/deploy twice and verify stable payload/no stale duplicate. |
| TG-017 | P2 package robustness | Generic capability-off contract | QML has missing/stale-payload negative cases and QPA has missing-package/metadata cases. The installed package publishes `HyRemote_GENERIC_AVAILABLE`, but no equivalent deterministic negative case was found proving an SDK built without Generic reports absence and `hyremote_deploy(... GENERIC)` fails for the intended reason. | T4 Phase D: add one Generic-off package/deploy negative case; do not build a combinatorial all-flags matrix. |
| TG-018 | P2 test intent | stale V1-only vocabulary | `tests/public-api-contract` and package/config comments/error text still describe durable package contracts as “V1” although the same surface is already used by progressive V0.x releases. Assertions are valuable; wording can mislead ownership/release relevance. | T3/T4 Phase B documentation/test-intent cleanup only; KEEP behavior. |

## Verified coverage — do not duplicate

| Area | Existing test code/evidence |
| --- | --- |
| occupied RFB port | `rfb_product_fit.py::verify_occupied_port_failure` — useful code, but TG-009 notes execution authority is missing |
| max-eight handshake timeout/recovery | `verify_handshake_slots_expire` |
| abrupt disconnect held-input cleanup | `verify_abrupt_disconnect_releases_input` |
| maintained-viewer Raw framebuffer/input/reconnect | `verify_standard_client` via `vncdotool` |
| VNC Auth positive/negative/no-downgrade/timeout | registered `hyremote-rfb-vnc-auth-handshake-test` when Security capability exists |
| concurrent viewer held state | registered `hyremote-rfb-multi-client-input-test` |
| saturated adapter disconnect cleanup | registered `hyremote-rfb-widget-disconnect-backpressure-test` |
| Widgets input pressure | pointer flood coalescing, protected releases under saturated mailbox, shutdown balancing/dropping pending input |
| Core deterministic lifecycle/concurrency | `hyremote-core-test-session-lifecycle` state/failure/race/exception cases |
| clean installed C++ primary paths | `installed-cpp-widgets` + `installed-cpp-quick` evidence cells |
| clean installed Generic primary paths | `installed-generic-widgets` + `installed-generic-quick` |
| minimal package/export/deploy closure | legacy-named `installed-sdk` has distinct minimal value; rename/clarify, do not delete |
| deploy helper proof layers | static contract scan, QML dispatch fixture, QPA source/installed negative matrix and real exact-SHA evidence protect different failure classes; no layer is approved as duplicate |

## Future-owned obligations — not current gaps

| Capability | Planned owner | Test direction |
| --- | --- | --- |
| TLS / VeNCrypt / secure viewer interop | #258 → #143 → #271 | positive/negative negotiation, no downgrade, certificate/credential failures, maintained viewer/customer-trial E2E |
| Runtime typed notifications | #259 | state/client/error event ordering and C++/QML observable parity |
| authenticated Session Registry | #170 / V0.2.1 | identity/admission/snapshot/events/terminate, peer correlation and held-input cleanup |
| accumulated damage / per-viewer delivery / ZRLE | #261 → #144/#175 | damage accumulation, encoding negotiation, resize invalidation, Raw/ZRLE equivalence, maintained-viewer interop |
| Qt 5.15 adaptation | #260/#265/#57 | exact toolchain/version seams after preflight |
| RK3588/EGLFS physical/native behavior | #236/#154 | real local display/input + remote coexistence; desktop offscreen is not a substitute |

## Observability / hardening candidates

| ID | Improvement | Rule |
| --- | --- | --- |
| TG-O01 | Linux ASan + UBSan lane | separate non-fast lane if clean enough; not a V0.1 admission requirement |
| TG-O02 | bounded repeated concurrency/stress | deterministic synchronization/seeds; never mask failures with retry |
| TG-O03 | coverage report | blind-spot observability, not a vanity percentage gate |
| TG-O04 | parser fuzz/property tests | only after a deterministic bounded parser seam exists; no architecture redesign merely for fuzzing |

## Release-admission handoff

- TG-001/TG-002 are real coverage defects, not yet proven product defects. Restore the intended tests first.
- TG-009 is an exact-candidate evidence-category issue already required by #229, so it was handed to #229 instead of deferred.
- TG-012 undermines #230 acquisition-isolation acceptance and therefore reopened #230.
- TG-013/TG-018 are #274 quality/robustness/test-intent work unless a restored test produces concrete release-blocking product evidence.

The #274 workstream does not convert every missing quality test into a new V0.1 feature.