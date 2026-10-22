# HyRemote Agent Execution Entry

This file is the repository-visible starting point for ChatGPT, CodeBuddy and local/AI agents working on HyRemote.

## Authority order

When sources disagree, use this order:

1. current `develop`, target Issue/latest authoritative comments, exact code and accepted exact-SHA evidence;
2. product authority: #1 roadmap, #24 version semantics, #343 mandatory pre-GA baseline, the active release-scope Issue and #95 release mechanics;
3. architecture authority: one Core + one Shared Runtime + `src/integrations/{cpp,qml,generic,qpa}`;
4. execution authority: this file, `CONTRIBUTING.md`, canonical build/test/release/Git-flow documents;
5. historical chats, branches, superseded PR prose and proposal-era documents are context only and never override current repository truth.

Do not infer project status from chat memory alone.

## Before starting work

- Read current `develop` unless the target Issue explicitly owns another accepted base.
- Read the target Issue and its latest authoritative comments.
- Search open Issues/PRs for overlapping ownership before creating parallel work.
- Verify the task has not been superseded or reclassified.
- If Issue prose conflicts with current machine/code/product facts, resolve the governance truth before implementation.
- Keep one focused Issue/PR/root-cause owner; do not duplicate another Agent's active fix.

## Current product phase

**V0.3.0.0 Product Preview was released on 2026-10-19** (annotated `v0.3.0.0`). The current development priority is **V0.3-family product-breadth convergence**, with **V0.3.3.0** activated for practical RFB delivery (#144) and maintained GUI-viewer interoperability (#175), as recorded in `.github/release/release-trains.json`. V0.3.1.0 and V0.3.2.0 remain retired planning labels; V0.4.0.0 remains conditionally inactive and qualification-only until its entry gate passes.

**Local Owner / Agent execution handoff:** [docs/internal/owner-execution-brief-2026-10-21.md](docs/internal/owner-execution-brief-2026-10-21.md) (governance work #549). It records merged correctness fixes for regression watching, prioritizes current #144/#175/#370 acceptance evidence, Qt 6.12 LTS *preflight only*, embedded Quick-only loader correctness and the V0.4/GA decision boundary. This brief does not override #1/#24/#33/#95/#343 or machine release authority.

Current execution order:

```text
verify latest develop + Issue ownership + exact-SHA evidence
 -> reuse merged #553 cursor/input/build-doc regressions; triage only new failures or open review debt
 -> close V0.3.3.0 product outcome (#144 + #175, supported by #370)
 -> finish remaining #343 mandatory Qt/platform/ARM64/performance breadth
 -> enter V0.4 qualification-only RC after explicit activation
 -> V1.0 GA on one accepted, qualified RC lineage
```

**Version-scope Owner ruling #558 (2026-10-22):** V0.3.3.0 #144/#175 closure applies to desktop Qt 6.8.3 Windows/Linux x86_64 delivery/viewer evidence. ARM64/EGLFS is mandatory later in the V0.3 family under #236/#154/#203/#343 **before V0.4 activation**, not a hidden desktop Feature freeze condition. Do not preassign V0.3.4.0/V0.3.5.0 to technical packages; activate coherent user outcomes via #24/#95 and the reviewed machine release train. The #343/#33 GA scope is unchanged.

Qt 6.12 LTS was released in 2026-09; it is a compatibility **preflight**, not a silently added GA support promise. Changing GA scope, security promises or the machine release map requires an explicit Product Owner decision and reviewed authority updates.

Cross-cutting test-system work under #393/#394 may progress independently when it does not overlap the active product-path owner; it must not block unrelated V0.3 implementation work merely because its PR is open.

## Requirement classes

### V0.3.0 accepted self-service baseline (historical)

Owned by #234/#333; already released, not the active V0.3.3 task list:

- #264: clean self-service SDK/install/deploy for C++ / QML / Generic / QPA;
- #240: task-oriented docs and runnable learning paths;
- #335: minimum self-service diagnostics;
- exact release-truth obligations and admitted usability defects that block self-service.

### Mandatory pre-GA baseline

Owned by #343 and completed across the V0.3 family before V0.4 entry:

- Qt 5.15 / 6.5 / 6.8 product families;
- Windows + Linux;
- x86_64 + ARM64/AArch64;
- required qwindows / qxcb / Wayland / EGLFS reference cells;
- Widgets + Qt Quick/QML;
- C++ / QML / Generic / QPA peer integration technologies with truthful per-cell status;
- practical compressed + incremental delivery with Raw fallback;
- maintained-viewer interoperability;
- practical rendering/performance baseline.

No exact V0.3.x number being preassigned makes any of these optional.

### Evidence-driven optional capabilities

Do not pull these into active scope without their owning evidence/Product Owner decision:

- Session Operations / Session Registry;
- native TLS/VeNCrypt/certificate lifecycle;
- network convenience beyond the basic listener contract;
- cloud/relay/NAT traversal;
- first-party viewer/browser client;
- vendor-specific acceleration;
- Application Control Plane.

## Architecture invariants

```text
src/
├── core/
├── runtime/
└── integrations/
    ├── cpp/
    ├── qml/
    ├── generic/
    └── qpa/
```

- One Core and one Shared Runtime.
- C++ / QML / Generic / QPA are peer product integration technologies.
- Widgets / Quick are UI families/adapters, not separate runtimes/frontends.
- Generic uses public Qt and preserves the application's native platform identity.
- QPA owns exact-private-ABI/platform-entry/native-delegate adaptation.
- Runtime may depend on Core; integration frontends may depend on Runtime; Runtime/Core never depend on a frontend.
- No Qt5 tree, embedded Runtime, accelerated Runtime or frontend-to-frontend implementation dependency.
- RFB/VNC is the first transport/backend, not HyRemote's product identity.

## Build and test authority

Canonical repository path:

```text
build.cmd + build.yml
  -> CMake configure/build/test/install
  -> product install root
```

Do not create a parallel acceptance build path. `compile.cmd`, if present, is compatibility-only.

CI is decision-driven:

```text
L0 local implementation feedback
L1 exact Ready-PR merge decision
L2 exact frozen-candidate qualification
```

Rules:

- Iterate locally with focused tests and the canonical build/install path.
- Hosted CI is required where it owns the L1/L2 decision, not after every local iteration.
- `OBSERVE`, post-merge, maintenance and specialized workflows are fire-and-observe for independent sequencing.
- Zero discovered/executed tests are invalid evidence.
- Exact-head evidence/artifacts must support the claim being made.
- Installed/deployed consumer evidence must not rely on source/build-tree/Qt-SDK PATH repair.
- Hosted/headless evidence does not replace real physical/native/viewer evidence when the claim depends on those facts.
- Review Gate occurs before merge.

Candidate flow:

```text
local canonical checks
 -> user-path preflight
 -> freeze exact SHA
 -> admitted FULL_GATE
 -> physical/user handoff only where required
 -> Review Gate
 -> merge/release action
```

Changing the candidate SHA invalidates affected exact-candidate evidence.

## Physical validation model

When a real viewer/device/environment is required, use **Agent-primary / Human-assisted** validation:

- Agent owns orchestration, machine observation, evidence capture and PASS/FAIL/BLOCKED classification.
- Human performs only physical actions and visual observations that automation cannot supply.
- Do not turn Human into the test runner or evidence organizer.
- Do not rerun already-proved cells merely to relabel an unrelated harness result.

## Git and parallel-agent discipline

- Work from a fresh accepted base.
- Use focused temporary branches and PRs; delete stale/superseded heads after closure.
- Do not force-push/rewrite accepted shared history unless repository authority explicitly requires it.
- Do not merge a stale-head PR whose accepted evidence no longer matches the base/head that will merge.
- Salvage useful work narrowly from superseded branches; do not merge stale branches wholesale.

## Scope discipline

Prefer the smallest architecture-compatible solution. A bug in one Qt control must not create control-specific product semantics. A platform or performance opportunity must not create a second Runtime. Optional future capabilities must not leak into current release acceptance.

If a change would alter product scope, GA support claims, the one-Runtime architecture, major frontend hierarchy, permissions/system-daemon behavior or desktop-wide/native input semantics, stop at the Product Owner decision boundary instead of silently expanding scope.
