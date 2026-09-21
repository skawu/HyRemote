# HyRemote Test Capability / Execution Matrix

> Phase A companion to `TEST_CATALOG.md` and `TEST_SCENARIOS.md` for #274.
>
> This matrix describes *when* a test family is meaningful. It is audit metadata only in Phase A; it does not replace existing CMake capability guards or change CI selection.

## Legend

- **Always**: whenever the owning product module and `HYREMOTE_BUILD_TESTS` are built.
- **CPP / QML / Generic / QPA**: frontend capability must exist.
- **Widgets / Quick**: corresponding Runtime target adapter + Qt module must exist.
- **Security**: current VNC Auth/OpenSSL transport-security build capability must exist.
- **Win/Linux**: reference desktop OS semantics are part of the contract.
- **Conditional**: registered only when a discovered tool/module exists.

Cost tiers proposed for Phase C:

- `fast`: deterministic in-process/configure-only test, normal PR candidate.
- `integration`: real Qt event loop/socket/thread interaction, still normal PR when affected.
- `installed`: clean install/configure/build/deploy/run; relatively expensive and path-sensitive.
- `e2e`: real viewer/product process flow; not required on every unrelated PR.
- `qualification`: exact candidate/platform/manual/physical evidence.

## T1 — Core

| Family | Existing guard | Platform | Cost | Release relevance | Phase-A decision |
| --- | --- | --- | --- | --- | --- |
| frame lifetime / damage / timing / mailbox | Core + tests | host-neutral | fast | cross-release | KEEP |
| transport handoff / input routing / normalization | Core + tests | host-neutral | fast | cross-release | KEEP |
| Session lifecycle/concurrency | Core + tests | host-neutral | integration | cross-release; critical | KEEP |
| callback lifetime/exception boundary | Core + tests | host-neutral | fast/integration | cross-release | KEEP |
| Core dependency boundary | Core + tests | repository source | fast/contract | every release | KEEP |

Core does not require Widgets, Quick, RFB, QML, Generic or QPA.

## T1 — Shared Runtime / RFB / target adapters

| CTest/family | Existing registration guard | Platform/env | Cost | Intended owner | Decision |
| --- | --- | --- | --- | --- | --- |
| automatic surface model | Runtime + tests | Qt Core | fast | Runtime | KEEP |
| automatic composite capture | Runtime + tests | Qt Core/Gui + Shared Runtime | fast/integration | Runtime | KEEP |
| automatic composite input | Runtime + tests | Qt Core/Gui + Shared Runtime | fast/integration | Runtime | KEEP |
| security descriptor | currently CPP + Runtime | Qt Core/Network | fast | Runtime/security | MOVE |
| VNC auth primitive | CPP + VNC + Security | OpenSSL/Qt Network | fast | Runtime/RFB security | MOVE |
| RFB VNC-auth handshake | CPP + VNC + Security | loopback socket | integration, 15s bound | Runtime/RFB | MOVE |
| RFB multi-client input | CPP + VNC | loopback socket | integration, 15s bound | Runtime/RFB | MOVE |
| listener address matrix | CPP + Widgets adapter | offscreen + Qt Network | integration | Runtime/network | MOVE; remove accidental Widgets ownership if implementation allows |
| Runtime input mailbox admission | CPP | Qt Core | fast/integration | Runtime | MOVE |
| target component provider | CPP | Qt Core + available adapters | fast | Runtime/adapters | MOVE |
| Widgets capture | CPP + Widgets adapter | `QT_QPA_PLATFORM=offscreen` | integration | Runtime/Widgets | MOVE; add forced-DPR variant later |
| Widgets input routing/backpressure | CPP + Widgets adapter | offscreen | integration | Runtime/Widgets | MOVE |
| RFB + Widgets disconnect/backpressure | CPP + VNC + Widgets | offscreen + loopback socket | integration | Runtime/RFB+Widgets | MOVE |
| Quick capture | CPP + Quick adapter | offscreen + software Quick | integration | Runtime/Quick | MOVE; add forced-DPR variant later |
| Quick input routing/backpressure | CPP + Quick adapter | offscreen + software Quick | integration | Runtime/Quick | MOVE |
| `rfb_product_fit.py` | **not currently registered** | Python + Pillow + `vncdotool`, loopback | e2e | RFB product-fit | KEEP code; execution authority gap TG-009 |

The existing `CPP` dependency for many rows is historical registration placement, not the desired semantic guard. Phase B must preserve the actual Runtime/adapters required by the test while removing unnecessary C++-frontend ownership.

## T2 — C++ frontend

| CTest | Guard | Platform | Cost | Release relevance |
| --- | --- | --- | --- | --- |
| `hyremote-remoteaccess-test` | CPP | Qt Core/Network + Runtime fakes | fast | V0.1+ public facade |
| `hyremote-remoteaccess-error-ack-test` | CPP | Qt Core | fast | public diagnostic contract |
| `hyremote-remoteaccess-target-loss-test` | CPP | Qt Core | fast/integration | embedding safety |

These remain C++-owned even when lower Runtime tests move away.

## T2 — QML frontend

| CTest | Guard | Platform | Cost | Unique contract |
| --- | --- | --- | --- | --- |
| `hyremote-qml-module-test` | QML | QML module runtime path | integration | import/type registration and declarative facade semantics |
| `hyremote-qml-deploy-helper-non-qml` | QML | configure-only | fast/contract | ordinary helper dispatch leaves application QML import path untouched |
| `hyremote-qml-deploy-helper-qml` | QML | configure-only | fast/contract | QML helper dispatch + existing import path + installed HyRemote import root preserved |
| `qml_product_fit.py` | **not currently registered** | Python/Pillow/vncdotool + offscreen/software Quick | e2e | real viewer, QML client-count observation, view-only isolation, stop/configure/start, input, reconnect |

Deploy-helper rows are semantically T4 despite current QML physical ownership; keep their unique dispatch/import-path contract when consolidating fixtures.

## T2 — Generic frontend

| CTest | Guard | Platform | Cost | Unique contract |
| --- | --- | --- | --- | --- |
| `hyremote-generic-plugin-smoke` | Generic + Qt Widgets | offscreen; plugin path + `QT_QPA_GENERIC_PLUGINS` | integration | zero-code generic activation through Qt plugin mechanism |

Installed Generic Widgets/Quick tests are T4 and independently necessary.

## T2 — QPA frontend

All QPA tests require QPA capability and therefore exact Qt private-ABI qualification before this directory is reached.

| CTest | Additional guard/env | Cost | Unique contract |
| --- | --- | --- | --- |
| qpa proxy smoke | `QT_QPA_PLATFORM=hyremote` | integration | proxy/platform plugin loads |
| native semantics | Qt GuiPrivate | integration | native delegate/private-ABI semantics |
| remote config | Qt Core/Network | fast | QPA-only launch/config vocabulary |
| auto remote access | Widgets | integration | QPA starts one Shared Runtime |
| remote-failure native survival | Widgets; ws2_32 on Windows | integration | remote failure does not destroy local/native app |
| multi-surface connection | Widgets | integration, timeout 25s | multiple Widgets surfaces through proxy |
| popup connection | Widgets | integration, timeout 25s | popup/transient behavior through proxy |
| OpenGL widget capture | Widgets + conditional `Qt6::OpenGLWidgets` | integration, timeout 25s | real QOpenGLWidget capture classification |
| Quick multi-window | Quick + software backend | integration, timeout 30s | multiple Quick windows through proxy |

`hyremote-qpa-existing-app-build-check` is a build target, not a CTest; it proves the example source remains ordinary Qt-only by compilation dependency shape and should be catalogued as build evidence, not counted as an executed test.

## T3 — repository/product contracts

| Test | Current location | Guard/platform | Cost | Decision |
| --- | --- | --- | --- | --- |
| build-authority selftest | registered from Runtime tests | tests + repository | fast | MOVE to repository owner |
| CI scope classifier selftest | root | Python/script | fast | KEEP |
| mainline audit selftest | root | Bash available | fast | KEEP |
| branch-name gate selftest | root | Bash available | fast | KEEP |
| public API contract | standalone consumer fixture | Qt + installed/source contract as configured | fast/contract | KEEP |
| repository layout | release-readiness | repository files | fast | MOVE from T6 to T3 |
| Runtime architecture contract | release-readiness | repository files | fast | SPLIT/MOVE cross-release pieces |
| CI environment baseline | release-readiness | repository/workflow files | fast | MOVE to T3 |
| licensing boundary | release-readiness | repository metadata | fast | MOVE to T3 |

## T4 — clean consumer / package / deploy

| Test/cell | Capability | Platform | Cost | Unique delivery claim |
| --- | --- | --- | --- | --- |
| installed C++ Widgets | CPP + Widgets | Win/Linux reference | installed | clean SDK C++ Widgets lifecycle, deployment, source/build isolation |
| installed C++ Quick | CPP + Quick | Win/Linux reference | installed | same for Quick |
| installed Generic Widgets | Generic + Widgets | Win/Linux reference | installed | zero-code clean deployment + native platform identity |
| installed Generic Quick | Generic + Quick | Win/Linux reference | installed | same for Quick |
| minimal `installed-sdk` | CPP/shared Runtime package | Win/Linux reference | installed | minimal export/package/deploy closure without adapter-specific application target |
| source consumer | Runtime/CPP as fixture requires | cross-platform | installed/source | add_subdirectory/source acquisition does not inherit dev-only assumptions |
| installed QML | QML | Win/Linux reference | installed | declarative payload package/deploy preview |
| installed QPA | QPA exact 6.8.3 | Win/Linux reference | installed | private-ABI package/deploy preview |
| relocation/package isolation | relevant payload | Win/Linux as defined | installed/contract | installed tree is relocatable and source/build acquisition does not leak |

### Deploy-helper proof layers

These are overlapping by subject but not equivalent by proof type:

| Proof layer | Current owner | What it uniquely proves | Phase-A disposition |
| --- | --- | --- | --- |
| static deploy/package contract scan | `tests/release-readiness/check_deploy_helper_contract.cmake` | helper/package/install implementation retains required dispatch, metadata, fail-closed phrases, native-platform/QPA/Generic separation, and fixture capabilities | KEEP behavior; MOVE T4 and reduce brittle prose-token assertions later where semantic execution exists |
| QML dispatch fixture | `src/integrations/qml/tests` | exactly one Qt deploy API is selected; non-QML does not mutate app import path; QML preserves app import path and adds installed HyRemote root | KEEP unique contract; consolidate physical fixture in Phase B if safe |
| QPA/source-installed matrix | `src/integrations/qpa/CMakeLists.txt` | ordinary/QML/QPA source+installed combinations, missing/stale QML/QPA metadata, missing module/root, Qt mismatch, missing QPA package, generator shape, Linux source-payload relocation | KEEP scenarios; semantically T4/QPA deploy-contract |
| release evidence `deploy-helper` | evidence runner | real clean install/deploy output from exact SHA rather than configure-time doubles/static scanning | KEEP as evidence-production cell |

Phase B target is one shared deploy-contract fixture/runner where practical, **not** one giant test. Static structure, deterministic negative configure matrix and real exact-SHA deploy evidence protect different failure classes.

## T5 — adoption / product E2E

| Asset/test | Guard/current authority | Cost | Phase-A conclusion |
| --- | --- | --- | --- |
| `hyremote-v01-example-smoke` | Python found + examples/product capabilities | installed/e2e-lite | KEEP: proves SDK install + standalone build/deploy/run of canonical 01/02/03 and Generic native identity |
| old `example_product_fit.py` | no current registration; targets removed `widgets-basic/quick-basic` | e2e | RETIRE/MIGRATE unique standard-viewer contract; never resurrect removed teaching examples |
| `rfb_product_fit.py` | no current registration | e2e | KEEP code; exact-candidate execution gap reported to #229 |
| `qml_product_fit.py` | no current registration | e2e | KEEP as preview/non-fast candidate or explicit manual harness; unique QML observable lifecycle value |
| `showcase_product_fit.py` | no current registration | e2e | REVIEW/optional non-fast: showcase still exists but is not V0.1 primary authority; retain only if showcase remains a supported regression surface |

## T6 — release authority/readiness

| Family | Guard | Cost | Release relevance |
| --- | --- | --- | --- |
| release metadata/security/version truth | top-level tests | fast/contract | candidate-specific truth; KEEP/SPLIT |
| release authority policy | top-level tests | fast/contract | every selectable train |
| release documentation layout | top-level tests | fast/contract | candidate/release artifact layout |
| release scope self-test | top-level tests | fast/contract | exact train selection/fail-closed unknown |
| release-profile acceptance/rejection matrix | top-level tests; profile-specific configure | fast/contract | version semantics across trains |
| source-QPA authority negative checks | QPA/root conditional | contract | QPA acquisition truth; permanent owner under review T4 vs T6 |

## Platform / capability observations

1. Headless `offscreen`/software-Quick tests qualify deterministic Qt adapter behavior, **not** native/physical GPU/display qualification.
2. QPA carries an exact Qt 6.8.3 private-ABI boundary; Generic and ordinary C++ must not inherit it.
3. Security/VNC tests are conditionally registered according to actual compiled capability; a test that skips at runtime is not a substitute for correct non-registration.
4. Installed/evidence cells must bind to one SHA and prove clean acquisition/runtime isolation; a green in-tree unit test cannot replace them.
5. Real-viewer product-fit belongs in a semantic `e2e`/candidate lane rather than every unrelated fast PR.
6. Phase C labels must express these semantics without introducing a second test runner or bypassing canonical build authority.

## Phase-C proposed semantic labels

No labels are applied in Phase A. Proposed vocabulary:

- type: `unit`, `component`, `integration`, `contract`, `consumer`, `e2e`, `release`;
- owner: `core`, `runtime`, `rfb`, `widgets`, `quick`, `cpp`, `qml`, `generic`, `qpa`, `repository`;
- cost: `fast`, `installed`, `e2e`, `qualification`;
- optional matrix tags only where needed: `security`, `windows`, `linux`, `physical`.

Avoid encoding release versions into every test label. Release scope should select required semantic contracts rather than turning test names into a second WBS system.
