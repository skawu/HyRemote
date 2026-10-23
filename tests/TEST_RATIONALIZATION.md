# Test rationalization inventory

Status: engineering-health tracking after V0.3.0.0. Authority: #393 / #394 (`docs/internal/test-strategy.md`).

This is an inventory and removal plan, not a new release contract. Counts are from the V0.3.0.0 Windows build and vary with examples/integration switches.

## Baseline

- 111 CTest processes; `TEST_CATALOG.md` had 102 rows, including a duplicate Tool row.
- 28 `release-*`, 25 `qpa-*`, 12 core, and the rest split across widgets/quick/runtime/rfb/qml/consumers.
- Several assets had no registration: `input_probe.cpp`, `run_diagnostic_export_test.cmake`, `showcase_product_fit.py`.

## Keep

- core/runtime/rfb/widgets/quick/qml protocol, input, capture and lifecycle correctness;
- one installed consumer per integration route;
- release authority, security, packaging/deployment, licensing and documentation contracts;
- the two HyRemoteTool client tests.

## Retire or merge

| Group | Current | Target | Reason |
|---|---|---|---|
| QPA deploy-helper matrix | 15 processes | 2 (positive + rejection table) (done) | Same script, different inputs |
| QPA runtime smokes | 9 | 3 groups + 1 isolated Quick case (done) | Repeated startup/connection; Quick startup is grouping-sensitive |
| Release profiles | 10 | 1 table-driven run (done) | Same scope logic over versions |
| Example/product-fit E2E | 3 harnesses | 1 registered harness + 1 retained unregistered QML asset | Duplicate setup and viewer logic; `qml_product_fit.py` remains pinned by the E3 runtime contract but has no CTest owner |
| Dead/unregistered assets | 3 files | 3 removed; `qml_product_fit.py` retained and path refreshed | No execution authority; the QML asset is deferred P2 until a task-level owner exists |

## Rules for new tests

1. Every retained or new test names the user task or product/release contract it protects.
2. Do not add a test per example, UI widget or historical incident; add it to an existing task-level scenario.
3. A test that cannot state its failure meaning is deleted or folded into its owner.
4. CTest names and `TEST_CATALOG.md` must stay in sync; no unregistered assets.

## Current state

- 83 CTest processes: 7 QPA, 18 release, the rest product correctness/consumers/tool.
- QPA deploy-helper is 2 table-driven runs and release-profile is 1 table-driven run.
- QPA runtime smokes are 3 groups plus 1 isolated Quick case.
- `qml_product_fit.py` remains an unregistered E3 contract asset; do not count it as current evidence.

## Sequence

1. **Inventory/cleanup (done):** remove dead assets, dedupe catalog, record this map.
2. **QPA/release consolidation (done):** table-driven deploy-helper and release-profile runs; consolidate runtime smokes.
3. **Examples/product-fit (partial):** the learning path is stable; one task-level E2E per route and the QML asset ownership decision remain.
3. **Examples v2:** task → example → doc → test matrix; one learning-path E2E.
4. **Metrics:** CTest ~111 → ~80; QPA 25 → ~10; CI wall time −30–40%; no loss of product-contract meaning.
