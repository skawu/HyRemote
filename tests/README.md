# `tests/` — cross-module and delivered-product verification

HyRemote test ownership follows the product architecture. Tests that need private implementation detail live with the module they qualify (`src/<owner>/tests/`). The repository-level `tests/` tree is for contracts that cannot be proved from one product module alone: clean consumers, package/deploy behavior, repository/public contracts, adoption flows and release qualification.

## Phase-A audit authority

The #274 Phase-A audit is split into five focused documents:

- [`TEST_CATALOG.md`](TEST_CATALOG.md) — every registered test's owner, necessity and final KEEP/MOVE/SPLIT/MERGE/RETIRE/GAP disposition;
- [`TEST_SCENARIOS.md`](TEST_SCENARIOS.md) — meaningful internal scenarios and overlap decisions for multi-case executables/scripts;
- [`TEST_MATRIX.md`](TEST_MATRIX.md) — capability/platform/cost/release-relevance matrix and proposed Phase-C semantic tiers;
- [`EXECUTION_BASELINE.md`](EXECUTION_BASELINE.md) — hosted all-frontends 93-Linux / 92-Windows executed CTest inventory and platform difference;
- [`COVERAGE_GAPS.md`](COVERAGE_GAPS.md) — confirmed missing coverage, severity and owning phase/release.

These files describe test intent and ownership. They do not replace CTest/CMake as execution authority.

## Semantic layers

| Layer | Meaning | Normal location |
| --- | --- | --- |
| T1 | deterministic module/component behavior, including private seams | `src/core/tests`, `src/runtime/tests` |
| T2 | behavior unique to one peer integration frontend | `src/integrations/<frontend>/tests` |
| T3 | repository/product contracts | repository-level `tests/` / dedicated scripts |
| T4 | clean external consumer/package/deploy contracts | `tests/consumer-*`, deploy/relocation fixtures |
| T5 | user/adoption/product end-to-end flows | `tests/product-e2e`, `tests/v01-examples`, shared RFB product-fit harness |
| T6 | release/candidate-specific authority and readiness | `tests/release-readiness` after Phase-B ownership cleanup |

Physical locations are still legacy in places until Phase B. Do not create a duplicate test merely to obtain the preferred directory.

## Current repository-level directories

| Directory | Purpose |
| --- | --- |
| `build-authority` | executable checks for canonical build/bootstrap authority |
| `consumer-installed-cpp` | clean installed C++ consumers using real Widgets and Quick targets |
| `consumer-installed-generic` | clean installed zero-code Generic consumers for Widgets and Quick |
| `consumer-installed-sdk` | minimal installed public-package/export/deploy smoke with no Widgets/Quick adapter dependency; historical name, retained because its contract is distinct |
| `consumer-installed-qml` | clean installed declarative/QML payload consumer |
| `consumer-installed-qpa` | clean installed QPA payload consumer/product-fit fixture |
| `consumer-source` | external `add_subdirectory`/source-acquisition consumer |
| `product-e2e` | black-box/semi-black-box product/user-flow probes; several are intentionally dormant until explicit non-fast execution ownership is established |
| `public-api-contract` | installed public C++ API/target/dependency-surface contract |
| `release-readiness` | current legacy home for true release gates plus persistent T3/T4 contracts that Phase B will relocate/split |
| `v01-examples` | V0.1 adoption smoke: install SDK, independently configure/build/run minimum examples |

There is no current `tests/third_party` directory. Real-world maintained-viewer/application qualification belongs to the release/issue that promises it rather than to a stale directory name.

## Ownership rules

- `src/core/tests` protects Core-only lifetime, queue/backpressure, timing, input, callback and dependency-neutrality invariants.
- `src/runtime/tests` is the intended owner for Shared Runtime, RFB backend/private security, automatic composition and Widgets/Quick Runtime-adapter behavior. Some currently live under C++ and are marked MOVE/SPLIT in `TEST_CATALOG.md`.
- `src/integrations/cpp/tests` should protect only behavior unique to the public `HyRemote::RemoteAccess` facade.
- QML, Generic and QPA module tests protect only behavior unique to those peer frontends; they do not duplicate Shared Runtime state-machine/security behavior merely through another wrapper.
- Persistent deployment/package contracts are T4 even when their current fixture physically lives below QML/QPA or `release-readiness`.
- Clean installed/source consumers stay outside the product target graph because their purpose is to prove acquisition, export, relocation and deployment from an external application's perspective.
- `release-readiness` is not a dumping ground for ordinary regressions. A check belongs in T6 only when its failure specifically invalidates release/candidate truth.

## Test necessity rule

Every durable test must answer:

1. who owns it;
2. what observable behavior/invariant it protects;
3. why a lower or higher layer cannot replace it;
4. what a failure means to product/architecture;
5. what capabilities/platforms it requires;
6. whether it overlaps another test and, if so, why both remain necessary.

A historical test is not kept merely because it exists. Tests are also not deleted merely to reduce CI runtime. Phase A records the semantic decision first; Phase B changes physical ownership; Phase C changes selection metadata; Phase D closes confirmed gaps.

## Execution and evidence

The repository's canonical build authority remains the only normal way to configure/build/run the suite. Capability guards decide whether a test is meaningful for a configuration; Phase-C labels/tiering must not replace those guards.

A test file existing in the repository is **not evidence** unless a defined execution authority runs it. `EXECUTION_BASELINE.md` explicitly separates registered/executed CTests from dormant product-fit scripts.

A selected test set that executes zero tests is non-evidence. CI/release evidence must remain fail-closed under #250 semantics. Recorded acceptance evidence is documentation/artifact output, not product test code, and belongs under `docs/acceptance/` or the release's designated evidence artifacts.

No target under `tests/` may become a product dependency, and no product module may include/link a repository-level test target. Overall repository layout authority remains `docs/internal/repository-layout.md`.