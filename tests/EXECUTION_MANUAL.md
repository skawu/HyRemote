# Test execution manual (G0–G6)

Authority: #393 records the gates, #394 the inventory work, #274 the catalog. This file states **when** a test is run,
not what it asserts (that stays in `TEST_CATALOG.md`).

Measurements behind it: [`test-inventory-ts1-2026-10-30.md`](../docs/internal/test-inventory-ts1-2026-10-30.md).

## Gates

| Gate | Moment | What to run | What is forbidden |
| --- | --- | --- | --- |
| **G0** | while editing | compile the affected target, then run the single affected test binary by hand | any full suite; repeated `build.cmd test` per edit |
| **G1** | one logical slice complete | one affected label at a time, e.g. `ctest --test-dir build -L runtime` or `ctest --test-dir build -L qml`; narrow further with `-R <name-regex>`. CTest's `-L` matches **any** label against one regex, so `"component;runtime;fast"` is not an intersection and selects nothing | treating a labelled run as a whole-suite signoff |
| **G2** | pull request ready | exactly one canonical `build.cmd test` on the affected configuration, then the hosted L1 lanes on that head | a second full run "to be sure"; merging on a pending/skipped gate |
| **G3** | after merge | the fast/affected subset only | re-running the full gate on `develop` |
| **G4** | nightly | breadth: wire robustness, example smoke, every adapter family | blocking a pull request |
| **G5** | weekly | stress/fuzz/soak/performance profiles (`hyremote-rfb-wire-robustness-test`, performance SLO runs) | blocking a pull request |
| **G6** | frozen candidate SHA | the exact-SHA full matrix on both Qt lanes plus real-viewer product fit | re-running on a later SHA and presenting it as the same evidence |

## Non-negotiable rules

1. **No full suite while the code is still being edited.** A green run against a half-finished tree is not evidence.
2. **No `build.cmd test` per small edit.** Run the one affected binary; a full run is a G2/G6 decision, not a habit.
3. **No product acceptance before the feature is complete.**
4. **Identical SHA + identical tests + identical environment is not new evidence.** Repeating a PASS adds nothing.
5. **CI polling is not development.** Inspect checks at milestones (push of a ready PR, review fix, candidate freeze);
   spend the interval on code, evidence or another work item.
6. **Zero discovered/executed tests is a failure**, not a pass.

## Which change needs which run

| Change touches | Minimum before PR ready |
| --- | --- |
| `src/core` | `ctest -L core` plus the runtime identities that consume the changed contract |
| `src/runtime` | `ctest -L runtime` plus `ctest -L contract` for the touched surface |
| `src/integrations/{cpp,qml}` | `cpp` / `qml` labels + the QML/installed consumer when its surface changed |
| `src/integrations/{generic,qpa}` | `generic` / `qpa` labels + the deploy-helper group when packaging changed |
| `cmake/`, `build.cmd`, `build.yml` | `repository` + `release` labels, then one canonical run |
| `docs/` only | `repository` label only (documentation layout, links, catalog) |
| `examples/`, `tools/` | the example/tool smoke identities for the built examples |
| anything touching deployment/packaging | the installed-consumer identities for the affected route |

## When a full run is mandatory

- G2 on the exact PR head, once;
- G6 on the frozen candidate SHA;
- a change to a shared contract that every route consumes (Core/Shared Runtime public behaviour, build system,
  packaging);
- after an environment change (new Qt kit, new compiler, new runner image).

## Scheduled breadth

[`.github/workflows/scheduled-breadth.yml`](../.github/workflows/scheduled-breadth.yml) owns both gates now:

| Gate | Schedule | Lane |
| --- | --- | --- |
| G4 | nightly 18:00 UTC | Windows Qt 6.8.3, `cpp,qml,generic,qpa`, full suite on `develop` |
| G5 | weekly Saturday 03:00 UTC | Windows Qt 5.15.2 C++/Widgets full suite on `develop` — the lane PR gating now skips for frontend-only diffs |

Both are `workflow_dispatch`-able (`gate=G4|G5|BOTH`). A scheduled run **never** gates a pull request and never counts as
PR evidence; it is recorded against the `develop` SHA it actually built.
