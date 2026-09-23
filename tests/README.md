# `tests/` — cross-module and delivered-product verification

HyRemote test ownership follows the product architecture. Module-private behavior stays with its module; repository-level `tests/` owns contracts that cannot be proved from one product target alone: clean consumers, package/deploy behavior, repository/public contracts, adoption flows, bounded preflight evidence and release qualification.

## Final #274 authority

The completed test-architecture workstream is represented by:

- [`TEST_CATALOG.md`](TEST_CATALOG.md) — exact registered CTest identities, semantic owner/layer and necessity;
- [`TEST_SCENARIOS.md`](TEST_SCENARIOS.md) — meaningful scenario groups inside multi-case executables/scripts;
- [`TEST_MATRIX.md`](TEST_MATRIX.md) — capability/platform requirements plus stable semantic labels;
- [`EXECUTION_BASELINE.md`](EXECUTION_BASELINE.md) — historical before-refactor counts and final hosted execution reconciliation;
- [`COVERAGE_GAPS.md`](COVERAGE_GAPS.md) — closed gaps plus explicitly deferred lower-severity debt.

Final implementation authority before this reconciliation is `develop@bee774e76a48c7a23020b5386b42ac7a2bbb56f8`.

Reference all-frontends/VNC Qt 6.8.3 registration after Phase D is:

- security enabled: Linux **108**, Windows **107**;
- security off: Linux **106**, Windows **105**.

The single Linux-only identity is `hyremote-qpa-source-payload-relocation`. The historical 95/94 Phase-A inventory is no longer current authority.

## Semantic layers

| Layer | Meaning | Normal owner/location |
| --- | --- | --- |
| T1 | deterministic module/component/private seams | `src/core/tests`, `src/runtime/tests` |
| T2 | behavior unique to one integration frontend | `src/integrations/<frontend>/tests` |
| T3 | repository/product contracts | repository-level checks/scripts |
| T4 | external consumer/package/deploy/acquisition | clean consumers and deploy fixtures |
| T5 | user/adoption/product E2E | examples and maintained-viewer product-fit harnesses |
| T6 | release/candidate-specific truth | release scope/profile/readiness authority |
| PRE | bounded technical-decision preflight | `tests/preflight` + dedicated workflow |

Physical directory names are not semantic authority. Some durable T3/T4/T6 checks still live under the legacy `tests/release-readiness` path; Phase C labels carry their real meaning without forcing broad physical churn.

## Current repository-level directories

| Directory | Purpose |
| --- | --- |
| `build-authority` | canonical build/bootstrap authority checks |
| `consumer-installed-cpp` | clean installed C++ Widgets/Quick consumers |
| `consumer-installed-generic` | clean installed zero-code Generic Widgets/Quick consumers |
| `consumer-installed-qml` | clean installed declarative/QML consumer |
| `consumer-installed-qpa` | clean installed exact-private-ABI QPA consumer |
| `consumer-installed-sdk` | smallest public package/export/deploy release-evidence cell; deliberately distinct from adapter apps |
| `consumer-source` | external source/add_subdirectory acquisition |
| `preflight` | standalone bounded technical-risk probes; not part of normal product CTest graph |
| `product-e2e` | black-/semi-black-box user-flow assets; some remain deferred without execution authority |
| `public-api-contract` | installed public API/target/dependency contract fixture |
| `release-readiness` | legacy physical home containing semantically classified T3/T4/T6 checks |
| `v01-examples` | V0.1 SDK adoption smoke for canonical learning examples |

There is no `tests/third_party` directory.

## Ownership rules

- Core-only lifetime, timing, queues/backpressure, input, callback and dependency-neutrality invariants stay in Core.
- Shared Runtime, RFB backend/private security and Widgets/Quick adapter behavior belong to Runtime/RFB/adapters.
- C++ tests protect the public `HyRemote::RemoteAccess` facade, not private transport mechanics.
- QML, Generic and QPA tests protect behavior unique to those peer frontends and reuse the same Shared Runtime.
- Persistent package/deploy/acquisition tests are T4 regardless of legacy physical path.
- Clean external consumers stay outside product targets because they prove acquisition/export/relocation/deployment from an application's perspective.
- T6 is reserved for release/candidate truth, not ordinary regressions.
- PRE tests are bounded technical-decision evidence and deliberately do not form a second normal product test system.

## Stable CTest semantics

Phase C established additive labels:

- types: `unit`, `component`, `integration`, `contract`, `consumer`, `e2e`, `release`;
- owners: `core`, `runtime`, `rfb`, `widgets`, `quick`, `cpp`, `qml`, `generic`, `qpa`, `repository`;
- costs/tiers: `fast`, `installed`, `e2e`, `qualification`.

Existing special labels such as `candidate-evidence` are retained. Labels do not replace capability guards and do not make zero selected tests acceptable.

## Catalog drift is fail-closed

At top-level configure completion, the Phase-C semantic helper recursively enumerates configured CTest identities from CMake directory `TESTS` properties and verifies that every identity appears exactly in `TEST_CATALOG.md`.

A change that adds or renames a CTest without updating the catalog therefore fails configuration. This guard adds no CTest identity and does not alter current counts, selectors or runtime behavior.

## Important completed findings

- Runtime/RFB/security/network/Widgets/Quick ownership debt is closed by Phase B.
- `hyremote-build-authority-selftest` is repository/T3-owned.
- listener behavior is split into deterministic Runtime binding, public C++ facade semantics and real RFB reachability without duplicated rows.
- TG-020 is closed: RFB+Widgets disconnect/backpressure requires both Widgets and VNC and is absent when VNC is off.
- Phase-C semantic labels are stable and additive.
- TG-001/TG-002 forced-DPR coverage and TG-013/TG-014 RFB fragmentation/malformed/oversized coverage are closed by Phase D.
- there is no remaining confirmed P0/P1 gap in `COVERAGE_GAPS.md`.

Deferred P2 findings TG-010/TG-011/TG-015/TG-016/TG-017/TG-018 remain intentionally outside #274 implementation scope with explicit owner/rationale.

## Evidence rules for contributors

1. Use the repository's canonical build authority.
2. Capability guards decide registration; semantic labels describe meaning.
3. A source file is not evidence unless an execution authority runs it.
4. Discovered count is not executed count.
5. A selected suite that executes zero tests is non-evidence.
6. Cross-platform changes need exact-head Windows/Linux evidence where the touched contract is cross-platform.
7. New/renamed CTests must update `TEST_CATALOG.md` in the same change.
8. Do not keep duplicate tests merely because they are historical; document the distinct contract or consolidate them.
9. Do not add retries/skips to manufacture green correctness evidence.

Overall repository layout authority remains `docs/internal/repository-layout.md`.
