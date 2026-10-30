# TS-1 test inventory audit (read-only) — 2026-10-30

Audit baseline: `develop@21c508c`. Counts below were measured on `9f0c1d3` (the tree after the in-flight #589 work).
Authority kept: #393 / #394 / #274, `docs/internal/test-strategy.md`, `tests/TEST_CATALOG.md`.
This document records **facts and proposals only**; it deletes nothing.

## 1. Measured facts

Discovery (not execution) via `ctest --test-dir build --show-only=json-v1` on the canonical Qt 6.8.3 four-frontend
Release configuration (`build/local-qt683.yml`):

| Fact | Value | How measured |
| --- | --- | --- |
| Discovered CTest identities (Qt 6.8.3, cpp+qml+generic+qpa, VNC on, transport-security off, Windows) | **94** | `--show-only=json-v1` |
| Test executables built in that configuration | **43** | `build/{core,runtime,integrations,tests,tool}` `*.exe` |
| Full local suite wall clock | **390.9 s** | `build/test.log` total |
| Slowest identities | `hyremote-v01-example-smoke` 81.3 s, `hyremote-generic-installed-consumers` 55.7 s, `hyremote-cpp-installed-consumers` 51.7 s, `hyremote-qml-installed-consumer` 44.3 s, `hyremote-qpa-installed-product-fit` 40.4 s, `hyremote-rfb-wire-robustness-test` 30.2 s | `build/test.log` |
| Discovered identities, hosted Windows Qt 5.15.2 C++/Widgets lane | **50** | lane log, run 40976296339 / 40992593846 |
| Catalog rows in `tests/TEST_CATALOG.md` | **95** | regex over `| \`name\` |` rows |

Conditional registration explains 94 (Qt 6.8.3) vs 50 (Qt 5.15.2) vs 95 (catalog):

- Qt 5.15.2 builds only the C++/Widgets surface; QML, Generic, QPA, Quick and installed consumers do not exist there.
- `hyremote-vnc-auth-test` and `hyremote-rfb-vnc-auth-handshake-test` require
  `HYREMOTE_WITH_VNC AND HYREMOTE_TRANSPORT_SECURITY_AVAILABLE`; the default matrix is security-off, so they are absent.
- `hyremote-linux-qt-runtime-conflict` is Linux-only.
- `hyremote-qpa-deploy-helper-positive` / `-negative` are registered **and execute**, so the historical "83" and
  "113/112" prose is stale and must not be quoted as current.

### 1.1 Catalog defects found

| Defect | Detail |
| --- | --- |
| Missing identities | `hyremote-qpa-deploy-helper-positive`, `hyremote-qpa-deploy-helper-negative` execute but have no catalog row. |
| Rows without a condition | `hyremote-vnc-auth-test`, `hyremote-rfb-vnc-auth-handshake-test`, `hyremote-linux-qt-runtime-conflict` are listed as if universally present. |
| Unregistered assets (already declared debt) | `tests/product-e2e/example_product_fit.py`, `tests/product-e2e/qml_product_fit.py` have no execution owner (TG-010/TG-011, deferred P2); they are pinned only as *text* by `tests/release-readiness/check_v1_runtime_contract.cmake`, which is not execution. |
| Not an orphan | `hyremote-runtime-diagnostics-test` is registered and executes (0.08 s); `test_runtime_diagnostics.cpp` is not orphaned. |

## 2. Necessity classification (94 discovered identities)

Legend: **KEEP_BLOCKING** = blocks on the affected path; **KEEP_CONDITIONAL** = only when its capability is affected;
**MOVE_TO_G4** = nightly breadth; **MOVE_TO_G5** = stress/fuzz/soak/perf; **RELEASE_ONLY** = exact candidate;
**MERGE_CASES** = keep coverage in fewer processes; **DELETE_ORPHAN**; **INVESTIGATE**.
Cost is measured wall clock in the Qt 6.8.3 configuration.

### 2.1 Core (12 identities, ~4.5 s)

| Identity | Protected behaviour | PAC | Decision |
| --- | --- | --- | --- |
| frame-lifetime | frame/storage lifetime, immutability | PAC-6 | KEEP_BLOCKING (0.02 s) |
| damage | tri-state damage semantics | PAC-3 | KEEP_BLOCKING (0.02 s) |
| timing | PTS source rules | PAC-8 | KEEP_BLOCKING (0.02 s) |
| mailbox | bounded input mailbox | PAC-6 | KEEP_BLOCKING (0.03 s) |
| transport-handoff | enqueue/backpressure contract | PAC-6 | KEEP_BLOCKING (0.14 s) |
| session-lifecycle | start/stop/teardown | PAC-6 | KEEP_BLOCKING (3.4 s) |
| session-defaults | product defaults | PAC-4 | KEEP_BLOCKING (0.02 s) |
| input-routing, input-normalization | remote input mapping | PAC-3 | KEEP_BLOCKING (0.02 s each) |
| callback-lifetime, callback-exception-boundary | callback safety at boundaries | PAC-6 | KEEP_BLOCKING (0.04 s each) |
| dependency-boundary | no Qt in Core | PAC-4 | KEEP_BLOCKING (0.09 s) |

### 2.2 Runtime (11 identities)

| Identity | Protected behaviour | PAC | Decision |
| --- | --- | --- | --- |
| runtime-diagnostics-test | bounded, secret-safe support report | PAC-10 | KEEP_BLOCKING (0.08 s) |
| runtime-listener-binding-test | #174 binding resolution | PAC-3 | KEEP_BLOCKING (0.05 s) |
| runtime-notifications-test | event-driven state seam | PAC-6 | KEEP_BLOCKING (2.3 s) |
| host-paths-test | #591 precedence, scoping, UTF-8/space | PAC-5, PAC-10 | KEEP_BLOCKING (0.04 s) |
| watermark-policy-test | #589 defaults/bounds/fail-closed | PAC-3, PAC-7 | KEEP_BLOCKING (0.05 s) |
| watermark-compositor-test | pixel compositing, DPR, capture never mutated | PAC-3 | KEEP_BLOCKING (0.39 s) |
| watermark-frame-decorator-test | transport hook identity and refusal | PAC-3 | KEEP_BLOCKING (0.34 s) |
| security-descriptor-test | descriptor parsing/redaction | PAC-7 | KEEP_BLOCKING (0.10 s) |
| target-component-provider-test | adapter selection | PAC-1 | KEEP_BLOCKING (0.04 s) |
| input-mailbox-admission-test | admission policy | PAC-6 | KEEP_BLOCKING (0.03 s) |
| automatic-surface-model + composite-capture + composite-input | zero-code surface composition | PAC-2, PAC-4 | **MERGE_CASES** → one process, three phases (−2 processes) |

### 2.3 Transport / RFB (7 identities, ~71 s)

| Identity | Protected behaviour | PAC | Decision |
| --- | --- | --- | --- |
| rfb-multi-client-input-test | multi-viewer input fan-out | PAC-3, PAC-6 | KEEP_BLOCKING (0.56 s) |
| rfb-wire-robustness-test | fragmented/oversized wire input | PAC-6 | **MOVE_TO_G5** (30.2 s fuzz class; still triggered by wire/security changes) |
| vnc-auth-test, rfb-vnc-auth-handshake-test | VNC authentication wire contract | PAC-7 | KEEP_CONDITIONAL (already capability-gated) |
| rfb-widget-disconnect-backpressure-test | teardown with a slow viewer | PAC-6 | KEEP_CONDITIONAL (0.52 s) |
| rfb-input-preflight-test | production input path | PAC-3 | KEEP_BLOCKING (2.0 s) |
| rfb-listener-reachability-test | real listener/reconcile | PAC-3, PAC-6 | KEEP_CONDITIONAL (9.1 s; listener/RFB changes only) |

### 2.4 Widgets / Quick adapters (13 identities, ~10 s)

| Identity | Protected behaviour | PAC | Decision |
| --- | --- | --- | --- |
| widgets-capture + forced-dpr + dpr2 | Widgets capture across DPR | PAC-2, PAC-8 | **KEEP per-process** (0.14/0.05/0.04 s): `QT_SCALE_FACTOR` is fixed when the Qt application starts, so the three scale variants cannot share one process; merging them would delete the DPR coverage rather than save it. They are cheap already. |
| widgets-input-routing + backpressure | Widgets input correctness | PAC-3 | **MERGE_CASES** (−1 process) |
| widgets-input-preflight-test | shared input ingress | PAC-3 | KEEP_BLOCKING (2.3 s) |
| widgets-native-popup-capture-test | real native popup capture | PAC-2 | KEEP_CONDITIONAL (0.42 s) |
| quick-capture + forced-dpr + dpr2 | Quick capture across DPR | PAC-2, PAC-8 | **KEEP per-process** for the same `QT_SCALE_FACTOR` reason (0.68/0.65/0.65 s) |
| quick-input-routing + backpressure | Quick input correctness | PAC-3 | **MERGE_CASES** (−1 process) |
| quick-input-preflight-test | shared input ingress | PAC-3 | KEEP_BLOCKING (0.94 s) |
| quick-controls-preflight-test | QtQuick.Controls matrix | PAC-3 | KEEP_CONDITIONAL (0.21 s) |

### 2.5 Frontends (C++ / QML / Generic / QPA, 12 identities)

| Identity | Protected behaviour | PAC | Decision |
| --- | --- | --- | --- |
| remoteaccess-test + error-ack + target-loss | product façade lifecycle and errors | PAC-1, PAC-10 | **MERGE_CASES** (−2 processes) |
| listener-address-matrix-test | address truth table | PAC-3 | KEEP_BLOCKING (0.10 s) |
| qml-module-test | QML property surface (incl. #589) | PAC-4 | KEEP_BLOCKING (0.35 s) |
| qml-notifications-test | QML event parity | PAC-4, PAC-6 | KEEP_BLOCKING (0.67 s) |
| qml-deploy-helper-non-qml + -qml | deploy helper QML gate | PAC-5 | **MERGE_CASES** (−1 process) |
| generic-plugin-smoke | zero-code Generic end-to-end | PAC-4, PAC-5 | KEEP_BLOCKING (0.26 s) |
| generic-config-test | launch syntax mapping | PAC-4 | KEEP_BLOCKING (0.05 s) |
| qpa-deploy-helper-positive + -negative | QPA deploy helper accept/reject | PAC-5 | **MERGE_CASES** (−1 process, shared fixture setup ≈ −8 s) |
| qpa-runtime-basic + -surfaces + -recovery | QPA surfaces and recovery | PAC-2, PAC-4 | **MERGE_CASES** → one grouped runner, three phases (−2 processes) |
| qpa-quick-multi-window-connection-smoke | multi-window Quick under QPA | PAC-2 | KEEP_CONDITIONAL (2.2 s) |

### 2.6 Installed consumers and product fit (6 identities, ~190 s)

| Identity | Protected behaviour | PAC | Decision |
| --- | --- | --- | --- |
| cpp-installed-consumers | installed C++ SDK consumers | PAC-5 | KEEP_CONDITIONAL — SDK/installed/packaging changes only (51.7 s) |
| qml-installed-consumer | installed QML consumer | PAC-5 | KEEP_CONDITIONAL (44.3 s) |
| generic-installed-consumers | installed Generic consumers | PAC-5 | KEEP_CONDITIONAL (55.7 s) |
| qpa-installed-product-fit | installed QPA product fit | PAC-5 | KEEP_CONDITIONAL (40.4 s) |
| v01-example-smoke | learning examples still run | PAC-5, PAC-9 | **RELEASE_ONLY** (81.3 s) |
| v01-rfb-product-fit | exact-candidate RFB product fit (Pillow/vncdotool) | PAC-3, PAC-9 | **RELEASE_ONLY** (28.8 s; already scope-gated) |

### 2.7 Release readiness and governance (24 identities, ~45 s)

| Identity | Protected behaviour | PAC | Decision |
| --- | --- | --- | --- |
| release-readiness-metadata, -compatibility-cells, -ga-scope-reconciliation, -release-authority-policy, -repository-layout, -release-documentation-layout, -documentation-paths, -licensing-boundary, -ci-environment-baseline, -source-qpa-authority | release truth and docs/layout policy | PAC-9, PAC-10 | **MERGE_CASES**: split into two runners (facts/layout vs product claims); 18 tiny processes become 2 |
| release-readiness-runtime-contract, -security-runtime-deploy, -build-install-contract, -deployment-relocation, -deploy-helper-contract, -consumer-simplicity, -package-acquisition-isolation | product/packaging truth | PAC-5, PAC-9 | keep `-deploy-helper-contract` (10.2 s fixture) separate; **MERGE_CASES** the rest into the product-claims runner |
| release-preparation-correction | release-branch correction policy | PAC-9 | **RELEASE_ONLY** (5.2 s) |
| release-authority-v02-user-first | user-first release-authority contract (unconditional, top-level test block) | PAC-9 | KEEP_BLOCKING in the governance lane (0.07 s) |
| build-authority-selftest, ci-scope-self-test, mainline-audit-self-test, branch-name-gate-self-test, release-scope-self-test, acquisition-audit-self-test | the CI/build gates themselves | PAC-10 | KEEP_BLOCKING in the governance (no-Qt) lane, not in the product suite |
| release-profiles | profile resolution | PAC-5 | KEEP_CONDITIONAL (0.3 s) |
| tool-rfb-client, tool-client-ui | Tool client behaviour and brand presence | PAC-1, PAC-10 | KEEP_CONDITIONAL (0.6 s / 0.06 s) |

### 2.8 Aggregate effect of the proposals

| Metric | Today | Proposal |
| --- | --- | --- |
| Discovered identities, Qt 6.8.3 default configuration | 94 | ~68 (−26 by merging; no coverage removed, no DPR variant merged) |
| Default developer/mainline suite wall clock | 390.9 s | ~90–120 s (installed consumers, example smoke, product fit and wire fuzz leave the default set) |
| Full gate wall clock | 390.9 s | ~330 s (merged fixtures share setup) |
| Catalog rows | 95 | 94 identities + an explicit condition column |

Merge arithmetic (excluding the DPR variants, which stay per-process): zero-code surface 3→1 (−2), Widgets input 2→1
(−1), Quick input 2→1 (−1), product façade 3→1 (−2), QML deploy helper 2→1 (−1), QPA deploy helper 2→1 (−1), QPA
runtime 3→1 (−2), release-readiness 18→2 (−16): **−26**.

## 3. CI over-execution (measured)

- `windows-qt5` runs whenever `needs.classify.outputs.product == 'true'` (`.github/workflows/ci.yml:174-179`), i.e. for
  **every** ready product PR. A QML-only or QPA-only change still pays the Qt 5 download, a C++/Widgets build and 50
  tests (**2m48s–3m12s** per run), although Qt 5 cannot contain QML/Generic/QPA payloads.
- No scheduled G4/G5 workflow exists today, so nightly/weekly breadth has no owner. Proposal: one scheduled workflow
  with two purposes (`nightly` → G4 set, `weekly` → G5 set) reusing the candidate admission instead of new lanes.
- `resolve-ci-scope.py` exposes no Qt 5 capability output; the fix is a new `qt5_scope` output derived from the
  Shared-Runtime / C++ / Widgets path sets (never "everything except QML", which would silently widen).

## 4. G0–G6 execution rule (proposal, to be reviewed with #393)

| Gate | When | What runs | Forbidden |
| --- | --- | --- | --- |
| G0 | while editing | compile only, plus the one affected unit binary by hand | any full suite, repeated `build.cmd test` |
| G1 | one logical slice complete | affected labels only (`component;runtime;fast`, `contract;qml;fast`, …) | whole-suite signoff |
| G2 | PR ready | one canonical `build.cmd test` on the affected configuration plus hosted L1 | a second full run "to be sure" |
| G3 | after merge | fast/affected subset | re-running the full gate on `develop` |
| G4 | nightly | breadth set (wire robustness, example smoke, all adapters) | blocking a PR |
| G5 | weekly | stress/fuzz/soak/perf (`hyremote-rfb-wire-robustness-test`, performance profiles) | blocking a PR |
| G6 | frozen candidate SHA | full matrix, both Qt lanes, exact SHA, real-viewer product fit | re-running on a later SHA and calling it the same evidence |

Hard rules: (1) no full suite while code is still being edited; (2) no `build.cmd test` per small edit — run the one
affected binary; (3) no product acceptance before the feature is complete; (4) identical SHA + tests + environment is
**not** new evidence; (5) CI polling is not development time.

## 5. GUI / logo inventory (read-only; PR #730 carries the code changes)

| TARGET | CATEGORY | VISIBLE_LOGO | WINDOW_ICON | WINDOWS_EXE_ICON | RESOURCE_VALIDATED | EXCEPTION |
| --- | --- | --- | --- | --- | --- | --- |
| `hyremote-learning-01-widgets` | example, Widgets | PR #730 | PR #730 | **missing** | pending | — |
| `hyremote-learning-02-quick` | example, Quick | PR #730 | PR #730 | **missing** | pending | — |
| `hyremote-learning-03-widgets` | example, Widgets zero-code | PR #730 | PR #730 | **missing** | pending | must not inject brand into the hosted third-party-style app |
| `hyremote-learning-03-quick` | example, Quick zero-code | PR #730 | PR #730 | **missing** | pending | same |
| `hyremote-qml-basic` | example, QML | present | present | **missing** | pending | — |
| `hyremote-qpa-proxy-existing-app` | example, QPA | PR #730 | PR #730 | **missing** | pending | keeps native platform identity |
| `hyremote-remote-support-showcase` (HyRemoteTool) | product tool | PR #730 (`--verify-ui` asserts) | PR #730 | **missing** | pending | — |
| `hyremote-installed-qpa-consumer` | installed consumer GUI | — | — | **missing** | pending | brand only if HyRemote-owned |
| `qpa_widget_opengl_capture_smoke` and other QPA GUI smokes | internal test GUI | none (pixel fixtures) | one fixture only | **missing** | n/a | pixel-accuracy fixtures must not gain decorative pixels |
| headless/unit tests | library tests | n/a | n/a | n/a | n/a | must not link QtGui/Widgets for branding |

Facts behind the matrix: the only canonical assets are `logo/huayan-logo-single.png`,
`logo/huayan-software-horizontal.png`, `logo/hyremote-branding.qrc`; `git ls-files` contains **no `.rc`/`.ico`** and no
`WIN32_EXECUTABLE` property anywhere. Therefore **no Windows native file icon exists today**, and
`QGuiApplication::setWindowIcon()` cannot substitute for it (it changes the running window/taskbar icon only).

## 6. Proposed minimal remediation (Phase 3, separate PRs)

1. **Catalog truth PR**: add the two QPA deploy-helper rows, add an explicit "condition" column (security-on, Linux,
   capability) to the conditional rows, and replace the stale numbers in prose with the measured ones.
2. **Merge PR**: apply the `MERGE_CASES` rows above (no behavioural assertion removed), keeping one canonical entry
   point per merged family. Expected: 94 → ~78 processes, full gate 390.9 s → ~330 s.
3. **Trigger PR**: add `qt5_scope` to `resolve-ci-scope.py` and gate `windows-qt5` on it; move
   `v01-example-smoke`/`v01-rfb-product-fit` to release/candidate scope; add the scheduled G4/G5 workflow.
4. **Rules PR**: land section 4 as the user-facing execution manual and link it from `CONTRIBUTING.md`.
5. **Branding PR**: finish #730, then add the Windows native icon (.rc → ico generated from the canonical PNG, no new
   binary brand asset) and extend the existing `hyremote-tool-client-ui` brand check to the remaining GUI targets
   without creating one CTest per program.

## 7. Reproduction (read-only)

```text
ctest --test-dir build --show-only=json-v1                       # 94 identities, no execution
Select-String build/test.log 'Test\s+#\d+: ...'                  # per-identity wall clock
Get-ChildItem build/{core,runtime,integrations,tests,tool} -Recurse -Filter *.exe   # 43 executables
gh run view <qt5-lane-run> --log | Select-String DISCOVERED_TEST_COUNT              # 50 in the Qt 5 lane
git ls-files | Select-String '\.(rc|ico)$'                       # empty: no native Windows file icon
```

No test is deleted by this audit, and no full suite was executed to produce it.
