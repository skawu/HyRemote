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
| QPA deploy-helper matrix | 15 processes | 2 (positive + rejection table) | Same script, different inputs |
| QPA runtime smokes | 9 | 3 scenarios (native capture/input, multi-surface routing, failure/reconnect) | Repeated startup/connection/capture |
| Release profiles | 10 | 1 table-driven run (done) | Same scope logic over versions |
| Example/product-fit E2E | 3 harnesses | 1 task-level learning path | Duplicate setup and viewer logic |
| Dead/unregistered assets | 3 files | 0 | No execution authority |

## Rules for new tests

1. Every retained or new test names the user task or product/release contract it protects.
2. Do not add a test per example, UI widget or historical incident; add it to an existing task-level scenario.
3. A test that cannot state its failure meaning is deleted or folded into its owner.
4. CTest names and `TEST_CATALOG.md` must stay in sync; no unregistered assets.

## Sequence

1. **Inventory/cleanup (this slice):** remove dead assets, dedupe catalog, record this map.
2. **QPA/release consolidation:** table-driven deploy-helper and release-profile runs; consolidate runtime smokes.
3. **Examples v2:** task → example → doc → test matrix; one learning-path E2E.
4. **Metrics:** CTest ~111 → ~80; QPA 25 → ~10; CI wall time −30–40%; no loss of product-contract meaning.
