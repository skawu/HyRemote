# `tests/` — cross-module and delivered-product verification

HyRemote test ownership follows the product architecture. Tests that need private implementation detail live with the
module they qualify (`src/<owner>/tests/`). The repository-level `tests/` tree is for contracts that cannot be proved
from one product module alone: clean consumers, package/deploy behavior, repository/public contracts, adoption flows
and release qualification.

The Phase-A audit is split into four complementary views:

- [`TEST_CATALOG.md`](TEST_CATALOG.md) — what each durable test protects and whether it should KEEP/MOVE/SPLIT/MERGE/RETIRE;
- [`TEST_SCENARIOS.md`](TEST_SCENARIOS.md) — meaningful scenarios inside multi-case executables/scripts;
- [`TEST_MATRIX.md`](TEST_MATRIX.md) — capability/platform/cost/execution conditions;
- [`COVERAGE_GAPS.md`](COVERAGE_GAPS.md) — confirmed gaps, review candidates and future-owned obligations.

These files describe test intent and ownership. They do not replace CTest/CMake as execution authority.

## Semantic layers

| Layer | Meaning | Normal location |
| --- | --- | --- |
| T1 | deterministic module/component behavior, including private seams | `src/core/tests`, `src/runtime/tests` |
| T2 | behavior unique to one peer integration frontend | `src/integrations/<frontend>/tests` |
| T3 | repository/product contracts | repository-level `tests/` / dedicated scripts |
| T4 | clean external consumer/package/deploy contracts | `tests/consumer-*`, deploy/relocation fixtures |
| T5 | user/adoption/product end-to-end flows | `tests/product-e2e`, `tests/v01-examples` |
| T6 | release/candidate-specific authority and readiness | `tests/release-readiness` |

Physical locations are being audited in #274. Until Phase B is accepted, an existing test may still live in a legacy
location even when the catalog marks it `MOVE`. Do not create another copy merely to obtain the preferred directory.

## Current repository-level directories

| Directory | Purpose |
| --- | --- |
| `build-authority` | executable checks for the repository's canonical build/bootstrap authority |
| `consumer-installed-cpp` | clean installed C++ consumers using real Widgets and Quick targets |
| `consumer-installed-generic` | clean installed zero-code Generic consumers for Widgets and Quick |
| `consumer-installed-sdk` | minimal installed public-package/export/deploy smoke with no Widgets/Quick adapter dependency; historical name, retained while #274 clarifies it |
| `consumer-installed-qml` | clean installed declarative/QML payload consumer |
| `consumer-installed-qpa` | clean installed QPA payload consumer/product-fit fixture |
| `consumer-source` | external `add_subdirectory`/source-acquisition consumer; must not inherit developer-only assumptions |
| `product-e2e` | black-box/semi-black-box product/user-flow probes; some historical scripts currently lack execution authority and are classified in #274 |
| `public-api-contract` | installed public C++ API/target/dependency-surface contract |
| `release-readiness` | release/candidate authority plus some historical cross-release checks being reclassified by #274 |
| `v01-examples` | V0.1 adoption smoke: install SDK, independently configure/build/run the minimum examples |

There is no current `tests/third_party` directory. Real-world maintained-viewer/application qualification belongs to
the release/issue that promises it rather than to a stale directory name.

## Ownership rules

- `src/core/tests` protects Core-only lifetime, queue/backpressure, timing, input, callback and dependency-neutrality
  invariants.
- `src/runtime/tests` is the intended owner for Shared Runtime, RFB backend/private security, automatic composition and
  Widgets/Quick Runtime-adapter behavior. Some of these tests currently remain under the C++ frontend and are marked
  `MOVE` in `TEST_CATALOG.md`; do not add new Runtime/backend tests there.
- `src/integrations/cpp/tests` should protect only behavior unique to the public `HyRemote::RemoteAccess` facade.
- QML, Generic and QPA module tests should protect only behavior unique to that peer frontend; they should not duplicate
  Shared Runtime state-machine/security tests through another wrapper.
- Clean installed/source consumers stay outside the product target graph because their purpose is to prove acquisition,
  export, relocation and deployment from the perspective of an external application.
- `release-readiness` is not a dumping ground for ordinary regressions. A check belongs there only when its failure
  specifically invalidates release/candidate truth. Persistent repository/package contracts may move to T3/T4 as #274
  proceeds.

## Test necessity rule

Every durable test must be able to answer:

1. who owns it;
2. what observable behavior/invariant it protects;
3. why a lower or higher layer cannot replace it;
4. what a failure means to the product/architecture;
5. what capabilities/platforms are required;
6. whether it overlaps another test and, if so, why both remain necessary.

A historical test is not kept merely because it already exists. Conversely, tests are not deleted just to reduce CI
runtime. `KEEP`, `MOVE`, `SPLIT`, `MERGE`, `RETIRE` and `REVIEW` decisions are recorded before structural changes.

## Execution and evidence

The repository's canonical build authority remains the only normal way to configure/build/run the suite. Capability
guards decide whether a test is meaningful for a configuration; semantic labels/tiering are planned by #274 but must
not replace those guards.

Having test code in the repository is not evidence that the test executes. A durable automated contract must have an
explicit execution authority (CTest/semantic e2e/candidate lane as appropriate); otherwise it is catalogued as a
manual harness or a coverage gap. Likewise, a selected test set that executes zero tests is non-evidence.
CI/release evidence must remain fail-closed under #250 semantics.

Recorded review/acceptance evidence is documentation rather than product test code and belongs under
`docs/acceptance/` or the release's designated evidence artifacts.

No target under `tests/` may become a product dependency, and no product module may include or link a repository-level
test target. Overall repository layout authority remains `docs/internal/repository-layout.md`.
