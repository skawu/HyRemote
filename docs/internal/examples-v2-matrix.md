# Examples v2 matrix

Status: post-V0.3.0.0 engineering-health plan. Authority: #41 / #240. This document maps the **current** example tree to the user journey and defines the next consolidation. It is not a new release contract and makes no new compatibility claim.

## Current inventory

| Actual path | Route / UI | Runnable | Current role | v2 decision |
| --- | --- | --- | --- | --- |
| `learning/01-widgets-cpp` | C++ API / Widgets | yes | canonical first C++ example | keep as `01-cpp-widgets` |
| `learning/02-quick-cpp` | C++ API / Qt Quick | yes | shows Quick UI does not require the QML API | keep as `02-cpp-quick` |
| `learning/03-zero-code-generic` | Generic plugin / Widgets + Quick | yes | untouched-app route | keep as `03-generic` |
| `learning/04-qml` | QML `import HyRemote` | yes | declarative QML-API example | promoted (done) |
| `learning/05-qpa` | QPA proxy / Widgets | yes | E4 ordinary-Qt fixture | promoted (done) |
| `real-world` | Generic / three frozen upstream apps | studies | third-party verification, never a support claim | keep separate |
| `remote-support-showcase` | HyRemoteTool client | yes | companion tool, not a learning example | move out of `examples/` later |

The five routes are not missing runnable examples. The gap is **information architecture and task mapping**: `examples/README.md` links the QML and QPA routes to guides rather than to their runnable directories, and the tree does not present one ordered journey.

## Task coverage

Legend: `Y` = covered by a runnable example; `D` = covered by docs only; `-` = not covered.

| User task | 01 C++ Widgets | 02 C++ Quick | 03 QML | 04 Generic | 05 QPA | Docs |
| --- | --- | --- | --- | --- | --- | --- |
| understand what HyRemote is / is not | Y | Y | Y | Y | Y | `examples/README` |
| choose a route | Y | Y | Y | Y | Y | route table (needs one canonical chooser) |
| install / find the SDK | D | D | D | D | D | `docs/guide/install.md` |
| integrate the route | Y | Y | Y | Y | Y | `docs/guide/integrate-your-project.md` |
| build / deploy | Y | Y | Y | Y | Y | `docs/guide/deployment.md` |
| connect / view / control | D | D | D | D | D | `docs/guide/viewer-connection.md` |
| lifecycle / reconnect / input policy | Y | Y | Y | Y | Y | per-example README |
| security / access boundary | D | D | D | D | D | `docs/security-model.md` |
| troubleshoot / diagnostics | D | D | D | D | D | `docs/guide/troubleshooting.md` |
| disable / update / roll back | - | - | - | - | - | no dedicated walkthrough |
| platform-specific guidance | D | D | D | D | D | `docs/guide/cross-compilation.md` |

The two real gaps are **one canonical route chooser** and a **disable/update/rollback walkthrough**; the rest is already covered but scattered.

## Target v2 layout

```text
examples/
  learning/
    01-cpp-widgets/
    02-cpp-quick/
    03-generic/
    04-qml/
    05-qpa/
  real-world/
  README.md          # one task journey, five runnable route entries
tools/
  hyremote-tool/     # companion client, moved out of examples later
```

Rules:

- one directory per logical route; no Qt-version duplicate trees;
- every route entry in `examples/README.md` points to a runnable directory, not only a guide;
- `03 QML` and `02 C++ Quick` stay distinct and explicitly labelled: Quick UI is a UI family, QML API is a frontend;
- `04 Generic` keeps the two ordinary Qt fixtures (Widgets + Quick) without HyRemote headers;
- `05 QPA` keeps the exact Qt 6.8.3/private-ABI boundary and never becomes a generic compatibility promise;
- real-world studies remain third-party verification, not product examples;
- the tool is not a tutorial and must not be counted as one.

## Test mapping

The target is **one task-level E2E per route**, not one per directory:

| Route | Installed consumer | E2E / product fit |
| --- | --- | --- |
| C++ | `hyremote-cpp-installed-consumers` | example smoke + one task-level path |
| QML | `hyremote-qml-installed-consumer` | one QML connect/view/control path |
| Generic | `hyremote-generic-installed-consumers` | `hyremote-generic-plugin-smoke` |
| QPA | `hyremote-qpa-installed-product-fit` | one QPA scenario group |

After v2, `example_product_fit.py` and `qml_product_fit.py` should either become one task-level harness or be explicitly retired; unregistered assets must not stay as de facto evidence. This is the same rule as `tests/TEST_RATIONALIZATION.md`.

## Sequence

1. **Inventory/sign-off (2 days):** confirm the five-route target and the Tool move; no code moves yet.
2. **Tree + README (3 days):** move the two existing runnable examples into `learning/`, update CMake/test references in one PR, rewrite the route chooser. **The QML/QPA promotion and route chooser are done; renaming `03-generic` remains optional.**
3. **Task docs (3 days):** add the disable/update/rollback walkthrough and point troubleshooting/security tasks at the right guide.
4. **Test mapping (3 days):** consolidate the example E2E harnesses; remove obsolete per-example UI assertions.
5. **Validation (2 days):** installed example smoke + one E2E per route on Windows/Linux Qt 6.8.3.

Estimated **2 weeks** of focused work after V0.3.0.0, before V0.3.x feature work. Moving directories is a one-time history-preserving change, not a permanent dual taxonomy.

## Non-goals

- no new compatibility claim (Qt 5.15/6.5, ARM64, Wayland/EGLFS remain outside the current V0.3.0.0 statement);
- no rewrite of real-world upstream projects;
- no Qt-version-specific example trees;
- no pixel-level UI gate;
- no new example solely to fill a row in this table.
