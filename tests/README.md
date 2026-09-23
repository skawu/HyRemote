# `tests/` — cross-module and delivered-product verification

HyRemote test ownership follows the product architecture. Private module behavior belongs with the module it qualifies; the repository-level `tests/` tree owns contracts that cannot be proved from one product target alone: clean consumers, package/deploy behavior, repository/public contracts, adoption flows, preflight evidence and release qualification.

## Phase-A audit authority

The #274 Phase-A audit is represented by:

- [`TEST_CATALOG.md`](TEST_CATALOG.md) — necessity, owner and KEEP/MOVE/SPLIT/MERGE/RETIRE/GAP decision;
- [`TEST_SCENARIOS.md`](TEST_SCENARIOS.md) — meaningful scenarios inside multi-case executables/scripts;
- [`TEST_MATRIX.md`](TEST_MATRIX.md) — capability/platform/cost matrix and proposed semantic tiers;
- [`EXECUTION_BASELINE.md`](EXECUTION_BASELINE.md) — current registration/execution evidence and platform reconciliation;
- [`COVERAGE_GAPS.md`](COVERAGE_GAPS.md) — confirmed gaps and findings closed during Phase A.

Current audit baseline: `develop@e3fcf2fd06f8feca00b83b6c49c48588264cfd81`.

Current default all-frontends/security-off registration is **Linux 95 / Windows 94**. This is a registration fact, not a claim that one ordinary PR lane executes all 95/94 cases. See `EXECUTION_BASELINE.md`.

## Semantic layers

| Layer | Meaning | Normal owner/location |
| --- | --- | --- |
| T1 | deterministic module/component/private seams | `src/core/tests`, `src/runtime/tests` |
| T2 | behavior unique to one integration frontend | `src/integrations/<frontend>/tests` |
| T3 | repository/product contracts | repository-level checks/scripts |
| T4 | external consumer/package/deploy/acquisition | `tests/consumer-*` and deploy fixtures |
| T5 | user/adoption/product E2E | examples and maintained-viewer product-fit harnesses |
| T6 | release/candidate-specific truth | release/readiness/candidate authority |
| PRE | bounded technical-decision preflight | `tests/preflight` + dedicated workflow |

Physical locations are still legacy in places until Phase B. Do not duplicate a test merely to obtain the desired directory.

## Current repository-level directories

| Directory | Purpose |
| --- | --- |
| `build-authority` | canonical build/bootstrap authority checks |
| `consumer-installed-cpp` | clean installed C++ Widgets/Quick consumers |
| `consumer-installed-generic` | clean installed zero-code Generic Widgets/Quick consumers |
| `consumer-installed-sdk` | minimal public package/export/deploy closure; retained because it is distinct |
| `consumer-installed-qml` | clean installed declarative/QML payload consumer |
| `consumer-installed-qpa` | clean installed exact-private-ABI QPA payload consumer |
| `consumer-source` | external source/add_subdirectory acquisition |
| `preflight` | standalone bounded technical-risk probes, currently V0.2 TLS/VeNCrypt feasibility; not part of normal product CTest graph |
| `product-e2e` | black-/semi-black-box product/user-flow harnesses; some currently lack execution authority |
| `public-api-contract` | installed public API/target/dependency contract |
| `release-readiness` | legacy mixed home for T3/T4 persistent contracts and true T6 gates; Phase B/C will split ownership |
| `v01-examples` | V0.1 SDK adoption smoke for canonical learning examples |

There is no current `tests/third_party` directory.

## Ownership rules

- Core-only lifetime, timing, queue/backpressure, input, callback and dependency-neutrality invariants stay in Core.
- Shared Runtime, RFB backend/private security and Widgets/Quick adapter behavior belong to Runtime/RFB/adapters even though several currently live under C++ tests.
- C++ tests should protect the public `HyRemote::RemoteAccess` facade, not private transport mechanics.
- QML, Generic and QPA tests protect behavior unique to those peer frontends and reuse the same Runtime.
- Persistent package/deploy/acquisition tests are T4 regardless of where their fixture currently lives.
- Clean external consumers stay outside product targets because their purpose is to prove acquisition/export/relocation/deployment from an application's perspective.
- T6 is not a dumping ground: a test belongs there only when its failure specifically invalidates release/candidate truth.
- PRE tests are bounded decision evidence. They deliberately do not become a second normal build/test system.

## Test necessity rule

Every durable test must explain:

1. semantic owner;
2. protected behavior/invariant;
3. why another layer cannot replace it;
4. what failure means;
5. required capabilities/platforms;
6. overlap and why both tests remain necessary.

Historical age is not justification. CI time reduction alone is not justification for deletion.

## Important current execution facts

- #281 added `hyremote-v01-rfb-product-fit` as fail-closed `candidate-evidence`; TG-009 is closed.
- #280 added `hyremote-acquisition-audit-self-test`; TG-012 and #230 are closed.
- #296 replaced fixed QPA popup sleeps with bounded condition waits; TG-019 is closed.
- #300 guards `hyremote-v01-example-smoke` by C++ API + Runtime target + Python. #296's first fresh qpa-only run proves it is absent when CPP=OFF; TG-021 is closed.
- `hyremote-rfb-widget-disconnect-backpressure-test` still has a weaker Widgets-only registration guard although it requires VNC/RFB; TG-020 remains open.
- Widgets/Quick forced-DPR second runs remain absent (TG-001/TG-002).

## Preflight boundary

`tests/preflight` currently contains the V0.2 TLS transition and VeNCrypt feasibility work. `hyremote-tls-transition-preflight` is a standalone CMake/CTest project driven by `.github/workflows/tls-preflight.yml` on Windows/Linux Qt 6.8.3 + OpenSSL.

This proves a technical transition is feasible before product implementation. It does **not** qualify the product security profile, replace `ci.yml`, or count in the normal 95/94 product CTest inventory.

## Execution and evidence rules

Use the repository's canonical build authority. Capability guards determine whether a test is meaningful; future labels/tiering may select by semantics but must not replace guards.

A source file is not evidence unless a defined authority executes it. A discovered count is not the same as executed count. A selected suite that runs zero tests is non-evidence under #250 fail-closed semantics.

Current evidence is intentionally split:
- current registration discovery: Linux 95 / Windows 94 from #300 exact-head full configure;
- latest prior full execution of the then-94/93 set: #280 run `35591928798`;
- new candidate product-fit: explicit candidate evidence after #281;
- reduced qpa-only guard/stability proof: #296 run `35676277901`;
- technical TLS preflight: dedicated `tls-preflight.yml` evidence.

No target under `tests/` may become a product dependency. Overall repository layout authority remains `docs/internal/repository-layout.md`.
