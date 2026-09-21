# HyRemote Test Coverage Gap Register

> Phase A audit for #274. Baseline: `develop` at `97083f874491c8c4987856340c83c4157c7d3d6f`.
>
> A `GAP` here means the repository lacks trustworthy evidence for an already-promised invariant, or the existing test cannot actually exercise the scenario it claims. `FUTURE-OWNED` items are **not current defects**; they are recorded only so future features have an explicit test owner before implementation.

## Status / severity

- `CONFIRMED`: code/test registration proves the gap now.
- `REVIEW`: likely gap, but Phase A has not yet proved absence/equivalence strongly enough to authorize implementation.
- `FUTURE-OWNED`: planned capability; must be tested in the owning future release, not pulled into the current release.
- `OBSERVABILITY`: quality improvement that helps find defects but is not itself a product contract.

Severity:
- `P0`: current release evidence can falsely pass or a current security/correctness contract is untrustworthy.
- `P1`: important current invariant has an untested path / misleading test claim.
- `P2`: useful robustness/maintainability gap, no evidence of current release unsafety.
- `P3`: optional quality/observability improvement.

## Confirmed gaps

| ID | Severity | Status | Area | Evidence / why it matters | Owner / action |
| --- | --- | --- | --- | --- | --- |
| TG-001 | P1 | CONFIRMED | Widgets HiDPI capture | `test_widgets_capture.cpp` says it is registered a second time with `QT_SCALE_FACTOR=1.5` and `HYREMOTE_EXPECT_DPR`, specifically to prevent DPR=1 from making the assertions vacuous. Current `src/integrations/cpp/tests/CMakeLists.txt` registers only `hyremote-widgets-capture-test` with `QT_QPA_PLATFORM=offscreen`; no forced-DPR registration exists. | Runtime/Widgets adapter. Add a distinct deterministic HiDPI CTest (or parameterized equivalent) in Phase D; do not change product code unless the restored test finds a defect. |
| TG-002 | P1 | CONFIRMED | Quick HiDPI capture | `test_quick_capture.cpp` makes the same explicit second-registration claim (`QT_SCALE_FACTOR=1.5` + `HYREMOTE_EXPECT_DPR=1.5`), while current CMake registers only the normal software/offscreen Quick test. | Runtime/Quick adapter. Same action as TG-001. |
| TG-003 | P1 quality | CONFIRMED | test ownership | Runtime/RFB/network/Widgets/Quick adapter tests are registered from `src/integrations/cpp/tests` and include/compile `src/runtime/src` internals. Necessary tests therefore encode the wrong architecture ownership. | #274 Phase B: move without behavior changes; C++ keeps facade-only tests. |
| TG-004 | P2 quality | CONFIRMED | repository test ownership | `hyremote-build-authority-selftest` is repository/build governance but is registered by `src/runtime/tests/CMakeLists.txt`. | #274 Phase B: register from repository-level test authority. |
| TG-005 | P2 quality | CONFIRMED | test documentation | `tests/README.md` omits active `build-authority`, `consumer-installed-cpp`, `consumer-installed-generic`, `v01-examples`, and still lists historical/non-current `third_party`. | #274 Phase A: update README to factual taxonomy. |
| TG-006 | P1 quality | CONFIRMED | execution semantics | Repository-wide CTest registrations have no authoritative semantic labels for owner/type/cost/release relevance; CI/release selection therefore relies substantially on names/path classifiers. | #274 Phase C after inventory. Add labels/equivalent semantic metadata; retain capability guards and #250 fail-closed execution. |
| TG-007 | P1 quality | CONFIRMED | release-readiness ownership | `tests/release-readiness` currently mixes true release authority with persistent repository/package contracts (layout, CI environment, licensing, package acquisition, relocation, consumer simplicity). This makes the release bucket a catch-all and obscures when checks should execute. | #274 Phase B/C: split semantic ownership; only release-specific candidate truth stays T6. |
| TG-008 | P2 quality | CONFIRMED | test asset catalog | No current authoritative map explains every registered test's necessity, failure meaning, capability guard, overlap and release relevance. #250 showed count alone is not evidence. | This Phase A catalog closes the documentation half; Phase C may make the metadata machine-checkable if useful. |

## Existing coverage verified during the audit — do not duplicate

These are explicitly listed to prevent future contributors from creating redundant tests based on an inaccurate gap assumption.

| Area | Existing evidence |
| --- | --- |
| occupied RFB port failure | `rfb_product_fit.py::verify_occupied_port_failure` proves bounded start failure |
| handshake timeout / max-eight slot recovery | `rfb_product_fit.py::verify_handshake_slots_expire` opens eight incomplete handshakes, waits for expiry, then proves a legitimate viewer can connect |
| abrupt disconnect held-input cleanup | `rfb_product_fit.py::verify_abrupt_disconnect_releases_input` checks modifier/key/button release ordering, exactly-once cleanup and next-viewer clean state |
| real maintained-tool Raw RFB smoke | `rfb_product_fit.py::verify_standard_client` uses `vncdotool` for framebuffer pixel, pointer, button, wheel, keyboard/text, reconnect and listener stop |
| VNC Auth positive/negative/downgrade/timeout | `test_rfb_vnc_auth_handshake.cpp` covers correct password, wrong password, forced-None refusal, stalled-auth timeout and explicit insecure None mode |
| concurrent multi-viewer held state | `test_rfb_multi_client_input.cpp` covers reference-counted key/button holds, malformed/out-of-order release, one-viewer disconnect, repeat input, final release modifier/position semantics |
| Core deterministic lifecycle/concurrency | `test_session_lifecycle.cpp` already covers repeated start/stop, partial-start cleanup, recoverable/fatal events, invalid config, bounded in-flight requests, backend exceptions, concurrent stop ownership and stop-during-start races |
| clean installed C++ primary paths | release evidence `installed-cpp-widgets` + `installed-cpp-quick` |
| clean installed Generic primary paths | release evidence `installed-generic-widgets` + `installed-generic-quick` |
| minimal package/export/deploy closure | legacy-named `installed-sdk` still has distinct minimal `Core+Network+Gui + HyRemote::RemoteAccess + hyremote_deploy()` value; rename/clarify rather than delete blindly |

## Behavioral gaps under review

These are not implementation tickets yet. Phase A must prove that an equivalent scenario does not already exist before promotion to `CONFIRMED`.

| ID | Status | Candidate gap | Why review is needed | Promotion criterion |
| --- | --- | --- | --- | --- |
| TG-R01 | REVIEW | fragmented RFB client messages across many socket reads | RFB worker accumulates `readAll()` into a bounded input buffer and parses by phase/message size, but inspected RFB tests mostly write complete protocol messages. TCP fragmentation is a real transport property. | no existing test deliberately fragments version/security/client-message bytes at multiple boundaries |
| TG-R02 | REVIEW | malformed/oversized RFB SetEncodings / ClientCutText / unsupported message input | Runtime has explicit `kMaxClientInputBytes`, `kMaxEncodings`, `kMaxCutTextBytes`, so those bounds are security/robustness contracts. Need prove tests exercise rejection and bounded cleanup, not merely define limits. | no existing deterministic negative case reaches each relevant bound/reject branch |
| TG-R03 | REVIEW | package/deploy paths containing spaces | evidence code is deliberately careful about spaces/semicolons/backslashes in `CMakeCache.txt`, especially Windows, but no explicit path-with-spaces execution scenario has yet been found. | no existing fixture places source/build/install/deploy path under a directory containing spaces |
| TG-R04 | REVIEW | repeated install/deploy to same clean destination / idempotence | current evidence proves clean install/deploy but not obviously a second identical deployment. Repetition may matter for SDK/customer workflows. | no existing cell intentionally repeats deployment and verifies stable payload/no stale duplicate |
| TG-R05 | REVIEW | capability-off negative configuration matrix | C++ installed consumer registration has an explicit `HYREMOTE_BUILD_CPP_API` negative check in #230 evidence, and release-profile tests exercise version/capability combinations. Need determine whether QML/Generic/QPA OFF combinations fail/omit contracts correctly at normal configure/package level. | missing negative checks for one or more frontend-off/package-acquisition contracts |
| TG-R06 | REVIEW | public API contract vocabulary still says `V1` while current pre-GA releases are V0.x | Test itself is valuable, but comments/error text may cause test intent to drift from current release semantics. | inspect whether assertions are genuinely GA-only or are already pre-GA package contracts; retitle wording without weakening contract |
| TG-R07 | REVIEW | duplicate deploy-helper matrices across root, QML and QPA | All may protect unique paths, but current physical ownership obscures overlap. | scenario matrix proves identical input/output contract in two places with no distinct acquisition/frontend value |

## Future-owned test obligations — not current gaps

| Capability | Planned owner | Required test direction |
| --- | --- | --- |
| TLS / VeNCrypt / maintained-viewer secure interop | #258 → #143 → #271 | preflight interoperability first; then positive/negative TLS negotiation, no downgrade, certificate/credential failures, exact Windows/Ubuntu customer-trial E2E |
| Runtime typed notifications | #259 | state/client/error event ordering, no product-significant polling dependence, C++/QML observable parity |
| authenticated Session Registry | #170 / V0.2.1 | authenticated identity/admission/snapshot/events/terminate, peer correlation and held-input cleanup |
| accumulated damage / per-viewer delivery / ZRLE | #261 → #144/#175 | accumulated damage across slow viewer, encoding negotiation, resize invalidation, Raw/ZRLE equivalence, maintained viewer interop |
| Qt 5.15 adaptation | #260/#265/#57 | only after exact Qt patch/toolchains and compatibility seams are frozen |
| RK3588/EGLFS physical/native behavior | #236/#154 | physical display/input/local+remote coexistence; must not be simulated as desktop headless qualification |

## Observability / hardening improvements

| ID | Status | Improvement | Rule |
| --- | --- | --- | --- |
| TG-O01 | OBSERVABILITY | Linux ASan + UBSan lane | add as a separate non-fast lane if clean enough; do not make V0.1 wait for introducing it |
| TG-O02 | OBSERVABILITY | bounded repeated concurrency/stress lane | run Core lifecycle and RFB multi-client races repeatedly with deterministic seeds/bounds; never hide flakes behind automatic retry |
| TG-O03 | OBSERVABILITY | code coverage report | use to expose blind spots, not as a vanity percentage gate |
| TG-O04 | OBSERVABILITY | parser fuzz/property tests | only after a deterministic bounded parser seam exists; do not redesign product architecture solely for fuzzing |

## Release-admission note

At the time of this audit, TG-001/TG-002 are real test-coverage defects but **not yet proven product defects**. #274 must restore the intended HiDPI tests first. If the restored tests fail on the current V0.1 candidate, that failure is then reported to the owning current-release issue under the normal release-admission rule. The test workstream must not silently turn every missing quality test into a new V0.1 feature.
