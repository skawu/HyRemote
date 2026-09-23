# HyRemote Test Capability / Execution Matrix

> Final #274 capability and semantic-selection authority, reconciled after Phase D against `develop@bee774e76a48c7a23020b5386b42ac7a2bbb56f8`.

CTest registration is controlled by capability/platform guards. Phase-C labels are additive semantic metadata and never replace those guards. Zero selected tests remains non-evidence.

## Stable semantic labels

Type labels: `unit`, `component`, `integration`, `contract`, `consumer`, `e2e`, `release`.

Owner labels: `core`, `runtime`, `rfb`, `widgets`, `quick`, `cpp`, `qml`, `generic`, `qpa`, `repository`.

Cost/tier labels: `fast`, `installed`, `e2e`, `qualification`.

Existing special labels such as `candidate-evidence` remain additive and are not replaced.

## Capability matrix

| Family | Required capability / platform | Semantic owner / tier |
| --- | --- | --- |
| Core mechanics | Core + tests | Core; unit/component/integration; fast |
| Runtime binding/security/provider/mailbox | Runtime + tests | Runtime; component/contract; fast |
| automatic surface/composite/notification tests | `HYREMOTE_AUTOMATIC_RUNTIME_AVAILABLE` | Runtime; component/integration; fast |
| RFB multi-client + wire robustness | VNC | RFB; integration; fast |
| VNC auth primitive + handshake | VNC + transport security | RFB; integration; fast |
| Widgets capture/routing/backpressure + forced DPR | Widgets adapter | Widgets; integration; fast |
| RFB+Widgets disconnect/backpressure | Widgets + VNC | RFB; integration; fast; absent when VNC is off |
| Quick capture/routing/backpressure + forced DPR | Quick adapter | Quick; integration; fast |
| RFB listener reachability | Widgets + VNC | RFB; integration; fast |
| C++ facade tests | C++ API; listener matrix additionally Widgets | C++; integration/contract; fast |
| maintained-viewer product fit | C++ API + VNC + Python/viewer tooling | C++/RFB E2E; `candidate-evidence` |
| QML module + notifications | QML API | QML; integration; fast |
| QML deploy-helper fixtures | QML API | QML/consumer; contract; installed |
| Generic config | Generic build path | Generic; contract; fast |
| Generic plugin smoke | Generic + Widgets | Generic; integration; fast |
| QPA behavior | exact Qt 6.8.3 private ABI; per-test Widgets/Quick/OpenGL guards | QPA; integration/contract; fast |
| QPA deploy matrix | QPA | QPA/consumer; contract; installed |
| QPA source payload relocation | QPA + Linux | QPA/consumer; contract; installed |
| build/CI/repository contracts | top-level tests; some tool availability guards | repository; contract; fast |
| installed C++ consumers | C++ API + Runtime | consumer/C++; installed |
| installed Generic consumers | Generic | consumer/Generic; installed |
| V0.1 example adoption | C++ API + Runtime + Python | C++ E2E; e2e |
| release profile/scope/authority | top-level tests | repository/release; qualification |

## Registration counts

Reference configuration is top-level tests with `cpp,qml,generic,qpa`, VNC enabled and Qt 6.8.3.

| Configuration | Linux | Windows | Explanation |
| --- | ---: | ---: | --- |
| final Phase-D security enabled | 108 | 107 | includes the two conditional VNC-auth tests |
| final Phase-D security off | 106 | 105 | same graph minus auth primitive/handshake |

The single Linux-only name is `hyremote-qpa-source-payload-relocation`. #274 final authority reconciliation adds no CTest identity, so these counts remain stable.

The historical 95/94 numbers are Phase-A/#300 before-refactor baseline only; see `EXECUTION_BASELINE.md`.

## Ownership results from Phase B

- Runtime/RFB/security/network/Widgets/Quick tests are Runtime-owned; C++ keeps only public facade behavior plus the maintained-viewer harness registration.
- `hyremote-build-authority-selftest` is repository/T3-owned and keeps its historical automatic-runtime capability guard.
- Listener proof is split into deterministic Runtime binding, public C++ facade configuration/error semantics, and real Runtime/RFB socket reachability with no duplicated rows.
- `hyremote-rfb-widget-disconnect-backpressure-test` requires both Widgets and VNC and is absent rather than skipped when VNC is unavailable.
- permanent T3/T4/T6 meaning is carried by semantic labels even where legacy physical `release-readiness` paths remain; bulk physical relocation was deliberately not required for #274 closure.

## Phase D result

TG-001/TG-002 are closed by the forced-DPR Widgets/Quick executions. TG-013/TG-014 are closed by the real-socket RFB wire-robustness test. There is no remaining confirmed P0/P1 gap.

TG-010/TG-011/TG-015/TG-016/TG-017/TG-018 remain explicit deferred P2 debt in `COVERAGE_GAPS.md`; this matrix does not promote them into ordinary fast CI.

## Selection rules

1. Capability guards decide whether a test can meaningfully exist.
2. Semantic labels describe why it exists and enable stable inspection/selection.
3. Ordinary PR selection may remain evidence-driven and path/classifier-aware; labels do not authorize zero-test lanes.
4. Exact CTest names remain compatibility identities where external selectors consume them.
5. New/renamed CTests must update `TEST_CATALOG.md` in the same change; configure-time catalog reconciliation enforces this.
