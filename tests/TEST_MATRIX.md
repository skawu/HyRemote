# HyRemote Test Capability / Execution Matrix

> Phase A companion for #274, refreshed against `develop@e3fcf2fd06f8feca00b83b6c49c48588264cfd81`.
>
> This records **when a test family is meaningful**. Phase A does not change the remaining guards or CI selection.

## Proposed Phase-C semantics

Types: `unit`, `component`, `integration`, `contract`, `consumer`, `e2e`, `release`; owners: `core`, `runtime`, `rfb`, `widgets`, `quick`, `cpp`, `qml`, `generic`, `qpa`, `repository`; cost: `fast`, `installed`, `e2e`, `qualification`.

Labels must never replace capability guards or make zero selected tests acceptable.

## T1 Core

| Family | Required capability | Cost | Decision |
| --- | --- | --- | --- |
| frame/damage/timing/mailbox | Core + tests | fast | KEEP |
| transport handoff/input normalization/routing | Core + tests | fast | KEEP |
| Session lifecycle/concurrency | Core + tests | integration | KEEP |
| callback lifetime/exception | Core + tests | fast/integration | KEEP |
| dependency boundary | source tree | contract | KEEP |

Core needs no GUI frontend, RFB or Qt Widgets/Quick.

## Runtime / RFB / adapters

| Test/family | Current registration requirement | Semantic requirement / decision |
| --- | --- | --- |
| automatic surface/composite tests | Runtime + tests | KEEP Runtime |
| security descriptor | Runtime + tests after B1 | Runtime/security |
| `hyremote-vnc-auth-test` | Runtime + VNC + Security after B1 | Runtime/RFB |
| `hyremote-rfb-vnc-auth-handshake-test` | Runtime + VNC + Security after B1 | Runtime/RFB |
| RFB multi-client input | Runtime + VNC after B1 | Runtime/RFB |
| `hyremote-v01-rfb-product-fit` | CPP + VNC; Python fail-closed; `candidate-evidence` | T5 candidate maintained-viewer evidence; TG-009 CLOSED |
| `hyremote-runtime-listener-binding-test` | Runtime + tests after #338 | deterministic interface resolution, listener-mode and IPv4-locality unit contract; no real socket/adapter dependence |
| `hyremote-listener-address-matrix-test` | CPP + Widgets | T2/C++ facade only after B4: public lifecycle/error/rejection mapping |
| `hyremote-rfb-listener-reachability-test` | Runtime + Widgets + VNC after B4 | Runtime/RFB real wildcard/explicit/interface listener reachability; independent of CPP frontend |
| input mailbox admission | Runtime + tests after B1 | Runtime |
| target component provider | Runtime + tests after B1 | Runtime/adapters |
| Widgets capture/routing/backpressure | Runtime + Widgets after #327/#328 | Runtime/Widgets; TG-001 remains on capture DPR |
| **RFB+Widgets disconnect/backpressure** | **Runtime + Widgets + VNC after #327/#328** | **TG-020 CLOSED**; absent when VNC is unavailable |
| Quick capture/routing/backpressure | Runtime + Quick after #327/#328 | Runtime/Quick; TG-002 remains on capture DPR |

Historical CPP guards describe former physical registration, not desired ownership. B4 adds exactly one all-capability CTest identity because one previously mixed CTest becomes one facade identity plus one Runtime/RFB integration identity; #338's binding unit is a separate current-develop addition, not a duplicated B4 row.

## Peer frontends

### C++

- `hyremote-remoteaccess-test` — CPP, fast, KEEP public facade.
- `hyremote-remoteaccess-error-ack-test` — CPP, fast, KEEP diagnostics.
- `hyremote-remoteaccess-target-loss-test` — CPP, integration, KEEP embedding safety.
- `hyremote-listener-address-matrix-test` — CPP + Widgets, integration, KEEP public listener facade rows after B4.

### QML

- `hyremote-qml-module-test` — QML, integration, KEEP T2.
- `hyremote-qml-deploy-helper-non-qml` / `...-qml` — QML, contract, MOVE T4.
- `qml_product_fit.py` — no execution owner today, e2e, KEEP asset/TG-011.

### Generic

- `hyremote-generic-plugin-smoke` — Generic + Widgets, integration, KEEP.
- installed Generic Widgets/Quick — Generic package capability, installed, KEEP T4.

### QPA

QPA itself requires exact Qt 6.8.3 private ABI. Unique behavior tests are KEEP:

- proxy/native semantics/config — QPA;
- auto Runtime/native-survival/multi-surface/popup — QPA + Widgets;
- OpenGL capture — QPA + OpenGLWidgets when available;
- Quick multi-window — QPA + Quick/software backend.

`hyremote-qpa-widget-popup-connection-smoke` no longer has an open timing gap: TG-019 CLOSED by #296 bounded waits and fresh first-attempt Win/Linux qpa-only evidence.

## T3 repository contracts

| Family | Cost | Decision |
| --- | --- | --- |
| build-authority selftest | fast | MOVE from Runtime to T3 |
| CI classifier / mainline audit / branch-name selftests | fast | KEEP T3 |
| public API contract | contract | KEEP; TG-018 wording cleanup |
| repository layout / documentation paths / CI environment / licensing | contract | MOVE from release-readiness to T3 |
| Runtime architecture contract | contract | SPLIT permanent T3 vs candidate T6 |

## T4 package / consumer / deploy

| Family | Capability | Cost | Decision |
| --- | --- | --- | --- |
| acquisition-audit self-test | top-level | fast | KEEP; TG-012 CLOSED |
| installed C++ Widgets/Quick | CPP + target adapter | installed | KEEP |
| installed Generic Widgets/Quick | Generic | installed | KEEP |
| minimal installed-sdk | shared package | installed | KEEP distinct minimal closure |
| source consumer | source acquisition | source/installed | KEEP |
| installed QML | QML | installed | KEEP preview |
| installed QPA | QPA exact ABI | installed | KEEP preview |
| relocation / package isolation / consumer simplicity | relevant payload | contract/installed | MOVE T4 |
| source-QPA authority negative | QPA | contract | MOVE T4 |
| QML deploy dispatch fixtures | QML | contract | MOVE T4 |
| QPA deploy source/installed negative matrix | QPA | contract/installed | MOVE T4 |

Open T4 robustness gaps remain TG-015 path-with-spaces, TG-016 repeated destination and TG-017 Generic-off negative contract.

## T5 adoption / E2E

| Asset | Current guard/authority | Decision |
| --- | --- | --- |
| `hyremote-v01-example-smoke` | **CPP API + Runtime target + Python** after #300 | KEEP; TG-021 CLOSED. Combined 01/02/03 split may be considered later |
| `hyremote-v01-rfb-product-fit` | CPP + VNC, fail-closed Python/tooling, candidate label; maintained-viewer/mainline authority retained | KEEP contract/MOVE semantic RFB E2E owner; TG-009 CLOSED |
| `example_product_fit.py` | no active owner | KEEP, retarget historical fixture naming before activation |
| `qml_product_fit.py` | no active owner | KEEP preview/TG-011 |
| `showcase_product_fit.py` | no active owner | KEEP non-fast/TG-011 |

## T6 release authority

Release metadata (candidate portion), authority policy, release documentation layout, release scope self-test and release-profile acceptance/rejection matrix remain T6. #277 expanded Feature-release scenarios inside existing registrations; #281 added the separate candidate-evidence RFB product-fit.

## PRE — technical preflight

`tests/preflight` is intentionally standalone:

| Asset | Requirement | Authority | Meaning |
| --- | --- | --- | --- |
| `hyremote-tls-transition-preflight` | Qt 6.8.3 + OpenSSL SSL/Crypto | `tls-preflight.yml` Win/Linux | prove same-socket plaintext→TLS, TLS>=1.2, cert prevalidation, timeout/reconnect/shutdown/no fallback |
| VeNCrypt probe scripts | Python/protocol/viewer prerequisites | #258 bounded preflight | viewer/protocol feasibility before implementation |

PRE evidence is not an ordinary product CTest and is not a customer/release acceptance substitute.

## Hosted reconciliation

The Phase-A baseline was Linux 95 / Windows 94 discovered with the Linux-only QPA source relocation as the one platform-name difference. Later Phase-B/current-develop slices legitimately changed that inventory: #338 added deterministic listener coverage, B2 moved adapter tests without changing their identities, and B4 is expected to add exactly one further all-capability identity by splitting the mixed listener matrix into two semantic owners. Exact final counts must therefore be taken from the final B4 hosted Review Gate rather than inferred from the historical Phase-A baseline.

Ordinary PR fast lane excluded candidate/release-expensive tests and executed Linux 62 / Windows 61 at the Phase-A baseline. Latest prior full all-executed baseline is #280 Linux 94/94 and Windows 93/93; #281's new candidate test has its own candidate/full authority.

Reduced qpa-only run #296 (`35676277901`) proved capability guards:
- Linux 72 discovered / 59 executed / PASS;
- Windows 71 / 58 / PASS;
- popup PASS both;
- V0.1 combined example smoke absent with CPP=OFF.

Exact names and interpretation are in `EXECUTION_BASELINE.md`.
