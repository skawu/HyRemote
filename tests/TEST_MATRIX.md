# HyRemote Test Capability / Execution Matrix

> Phase A companion to `TEST_CATALOG.md` / `TEST_SCENARIOS.md` for #274. Facts refreshed against `develop` at `6dc244a8717bdc520544c71d9756499033857c73`.
>
> This matrix describes **when** each family is meaningful. It is audit metadata only; Phase A does not change CMake guards or CI selection.

## Legend / proposed Phase-C cost tiers

- **CPP / QML / Generic / QPA**: frontend capability exists.
- **Widgets / Quick**: corresponding Runtime target adapter + Qt module exists.
- **Security**: current VNC Auth/OpenSSL transport-security capability exists.
- `fast`: deterministic in-process/configure-only.
- `integration`: Qt event loop/socket/thread interaction.
- `installed`: clean install/configure/build/deploy/run.
- `e2e`: real viewer/product-process flow.
- `qualification`: exact candidate/platform/manual/physical evidence.

## T1 Core

| Family | Guard | Platform | Cost | Decision |
| --- | --- | --- | --- | --- |
| frame lifetime / damage / timing / mailbox | Core + tests | host-neutral | fast | KEEP |
| transport handoff / input routing / normalization | Core + tests | host-neutral | fast | KEEP |
| Session lifecycle/concurrency | Core + tests | host-neutral | integration | KEEP |
| callback lifetime/exception boundary | Core + tests | host-neutral | fast/integration | KEEP |
| Core dependency boundary | Core + tests | repository source | contract | KEEP |

Core requires no Widgets/Quick/RFB/QML/Generic/QPA frontend.

## T1 Shared Runtime / RFB / target adapters

| CTest/family | Current guard | Platform/env | Cost | Final decision |
| --- | --- | --- | --- | --- |
| automatic surface model | Runtime + tests | Qt Core | fast | KEEP Runtime |
| automatic composite capture/input | Runtime + tests | Qt Core/Gui | integration | KEEP Runtime |
| security descriptor | currently CPP + Runtime | Qt Core/Network | fast | MOVE Runtime/security |
| VNC auth primitive | CPP + VNC + Security | OpenSSL/Qt Network | fast | MOVE Runtime/RFB |
| RFB VNC-auth handshake | CPP + VNC + Security | loopback | integration | MOVE Runtime/RFB |
| RFB multi-client input | CPP + VNC | loopback | integration | MOVE Runtime/RFB |
| listener address matrix | CPP + Widgets | offscreen/network | integration | SPLIT C++ facade rows vs Runtime bind/address-family rows |
| Runtime input mailbox admission | CPP | Qt Core | fast/integration | MOVE Runtime |
| target component provider | CPP | available adapters | fast | MOVE Runtime/adapters |
| Widgets capture | CPP + Widgets | offscreen | integration | MOVE Runtime/Widgets; TG-001 forced-DPR GAP |
| Widgets input routing/backpressure | CPP + Widgets | offscreen | integration | MOVE Runtime/Widgets |
| RFB+Widgets disconnect/backpressure | CPP + VNC + Widgets | offscreen + loopback | integration | MOVE Runtime/RFB+Widgets |
| Quick capture | CPP + Quick | offscreen + software | integration | MOVE Runtime/Quick; TG-002 forced-DPR GAP |
| Quick input routing/backpressure | CPP + Quick | offscreen + software | integration | MOVE Runtime/Quick |
| `rfb_product_fit.py` | **unregistered** | Python + Pillow + `vncdotool` | e2e | MOVE to T5 RFB harness; TG-009 execution GAP |

Historical CPP guards above often come from physical registration, not semantic ownership. Phase B must remove unnecessary frontend ownership while preserving real capability requirements.

## T2 C++ frontend

| CTest | Guard | Cost | Decision |
| --- | --- | --- | --- |
| `hyremote-remoteaccess-test` | CPP | fast | KEEP facade |
| `hyremote-remoteaccess-error-ack-test` | CPP | fast | KEEP facade diagnostics |
| `hyremote-remoteaccess-target-loss-test` | CPP | fast/integration | KEEP embedding safety |

## T2 QML frontend

| Test | Guard | Cost | Final decision |
| --- | --- | --- | --- |
| `hyremote-qml-module-test` | QML | integration | KEEP T2 |
| `hyremote-qml-deploy-helper-non-qml` | QML | contract | MOVE T4, keep ordinary-dispatch/import-path semantics |
| `hyremote-qml-deploy-helper-qml` | QML | contract | MOVE T4, keep QML-aware dispatch/import-root semantics |
| `qml_product_fit.py` | **unregistered** | e2e | KEEP T5 preview; TG-011 explicit execution ownership needed |

## T2 Generic frontend

| CTest | Guard | Cost | Decision |
| --- | --- | --- | --- |
| `hyremote-generic-plugin-smoke` | Generic + Widgets | integration | KEEP zero-code activation/native-QPA contract |

Installed Generic paths are T4 and independently necessary.

## T2 QPA frontend

QPA requires exact Qt 6.8.3 private ABI before the frontend is configured.

| CTest | Additional guard/env | Cost | Decision |
| --- | --- | --- | --- |
| `hyremote-qpa-proxy-smoke` | `QT_QPA_PLATFORM=hyremote` | integration | KEEP |
| `hyremote-qpa-native-semantics` | GuiPrivate | integration | KEEP |
| `hyremote-qpa-remote-config-test` | Core/Network | fast | KEEP |
| `hyremote-qpa-auto-remoteaccess-smoke` | Widgets | integration | KEEP |
| `hyremote-qpa-remote-failure-native-survival-smoke` | Widgets; ws2_32 Windows | integration | KEEP |
| `hyremote-qpa-multi-surface-connection-smoke` | Widgets | integration / 25s bound | KEEP |
| `hyremote-qpa-widget-popup-connection-smoke` | Widgets | integration / 25s | KEEP |
| `hyremote-qpa-widget-opengl-capture-smoke` | conditional OpenGLWidgets | integration / 25s | KEEP conditional |
| `hyremote-qpa-quick-multi-window-connection-smoke` | Quick + software | integration / 30s | KEEP |

`hyremote-qpa-existing-app-build-check` is a build target, not a CTest; it is build evidence that the example source remains Qt-only.

## T3 repository/product contracts

| Test | Current location | Cost | Decision |
| --- | --- | --- | --- |
| build-authority selftest | Runtime registration | fast | MOVE T3 |
| CI classifier selftest | root | fast | KEEP |
| mainline audit selftest | root | fast | KEEP |
| branch-name selftest | root | fast | KEEP |
| public API contract | standalone fixture | contract | KEEP; reword stale V1-only vocabulary (TG-018) |
| repository layout | release-readiness | fast | MOVE T3 |
| Runtime architecture contract | release-readiness | fast | SPLIT cross-release T3 vs true T6 residue |
| documentation paths | release-readiness | fast | MOVE T3 |
| CI environment baseline | release-readiness | fast | MOVE T3 |
| licensing boundary | release-readiness | fast | MOVE T3 |

## T4 consumer / package / deploy

| Test/cell | Capability | Platform | Cost | Decision/claim |
| --- | --- | --- | --- | --- |
| installed C++ Widgets | CPP + Widgets | Win/Linux | installed | KEEP clean SDK Widgets lifecycle |
| installed C++ Quick | CPP + Quick | Win/Linux | installed | KEEP clean SDK Quick lifecycle |
| installed Generic Widgets | Generic + Widgets | Win/Linux | installed | KEEP zero-code deployment/native platform |
| installed Generic Quick | Generic + Quick | Win/Linux | installed | KEEP same for Quick |
| minimal `installed-sdk` | shared Runtime/C++ package | Win/Linux | installed | KEEP distinct minimal package/export/deploy closure; rename later |
| source consumer | source acquisition | cross-platform | installed/source | KEEP |
| installed QML | QML | Win/Linux | installed | KEEP preview |
| installed QPA | exact QPA | Win/Linux | installed | KEEP preview |
| relocation/package isolation | relevant payload | defined platform | installed/contract | MOVE/KEEP T4 |
| source-QPA authority negative | QPA | configure-only | contract | MOVE T4 |

### Deploy-helper proof layers — no duplicate layer approved for deletion

| Layer | Current owner | Unique proof | Decision |
| --- | --- | --- | --- |
| root static deploy/package scan | release-readiness | required/forbidden implementation/package semantics | MOVE T4 |
| QML dispatch fixture | QML tests | ordinary vs QML-aware Qt deploy API + import-path preservation | MOVE T4 |
| QPA source/installed matrix | QPA | source/installed combinations, negative metadata, Qt mismatch, generator shape, Linux relocation | MOVE T4/QPA |
| exact-SHA evidence cell | release evidence | actual clean install/deploy output | KEEP evidence production |

Confirmed T4 gaps: TG-015 paths with spaces, TG-016 same-destination repeatability, TG-017 Generic capability-off negative contract.

## T5 adoption / product E2E

| Asset | Current authority | Cost | Final decision |
| --- | --- | --- | --- |
| `hyremote-v01-example-smoke` | registered when Python/examples are available | installed/e2e-lite | KEEP canonical 01/02/03 adoption proof |
| `rfb_product_fit.py` | unregistered | e2e | KEEP contract/MOVE owner; TG-009 exact-candidate execution GAP |
| `example_product_fit.py` | unregistered; historical scenario labels | e2e | KEEP unique app-level viewer/control contract; retarget to canonical 01/02, do not resurrect old examples |
| `qml_product_fit.py` | unregistered | e2e | KEEP preview/non-fast |
| `showcase_product_fit.py` | unregistered | e2e | KEEP maintained showcase regression, non-fast and non-V0.1-primary |

## T6 release authority/readiness

| Family | Guard | Cost | Decision |
| --- | --- | --- | --- |
| release metadata/security/version truth | top-level | contract | SPLIT: true candidate truth T6, permanent contract pieces T3/T4 |
| release authority policy | top-level | contract | KEEP T6 |
| release documentation layout | top-level | contract | KEEP T6 |
| release scope self-test | top-level | contract | KEEP T6 |
| release-profile acceptance/rejection matrix | top-level/profile configure | contract | KEEP T6 |
| source-QPA acquisition negative | QPA/root conditional | contract | MOVE T4 |

#277 expanded Feature-release scenario coverage inside existing T6 registrations; it did not add CTest names.

## Hosted platform reconciliation

Run `35577711774` all-frontends Release:

- Linux Qt6.8.3: 93 discovered / 93 executed / PASS;
- Windows Qt6.8.3: 92 / 92 / PASS;
- only difference: Linux-only `hyremote-qpa-source-payload-relocation`, matching explicit platform guard;
- no product-e2e Python harness above and no forced-DPR second capture run appears in the executed CTest set.

Exact names are in [`EXECUTION_BASELINE.md`](EXECUTION_BASELINE.md).

## Phase-C proposed labels

No labels are applied in Phase A.

- type: `unit`, `component`, `integration`, `contract`, `consumer`, `e2e`, `release`;
- owner: `core`, `runtime`, `rfb`, `widgets`, `quick`, `cpp`, `qml`, `generic`, `qpa`, `repository`;
- cost: `fast`, `installed`, `e2e`, `qualification`;
- optional matrix: `security`, `windows`, `linux`, `physical` only where semantically needed.

Labels must not encode release versions into every test name, replace capability guards, create a second runner, or allow zero selected tests to count as evidence.