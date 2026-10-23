# HyRemote Agent Execution Entry

This file is the repository-visible starting point for ChatGPT, CodeBuddy and local/AI agents working on HyRemote.

## Authority order

When sources disagree, use this order:

1. current `develop`, target Issue/latest authoritative comments, exact code and accepted exact-SHA evidence;
2. product authority: **[single current product outcome roadmap](docs/internal/product-version-delivery-roadmap-2026-10-23.md)** / #594 (including latest Owner Qt scope #605), #1, #24 version semantics, #343 pre-GA applicable baseline, active release-scope Issue and #95 release mechanics; exact activated machine scope is `.github/release/release-trains.json`;
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
 -> prioritize exact Windows **Qt5.15 + Qt6.8** LTS adaptation #603/#604 before completing HyRemoteTool GUI; other independent outcomes #589/#591/#592 and Tool increments follow
 -> finish only the committed Windows #343 baseline; Qt6.5/Qt6.12 post-GA #605, Linux/embedded user Issues nonblocking
 -> enter V0.4 qualification-only RC after explicit activation
 -> V1.0 GA on one accepted, qualified RC lineage
```

**Latest Owner decision #563 (supersedes #558's platform-validation gate):** Windows x86_64 is the *only mandatory physical/runtime qualification platform* for V0.3.3/V0.4/V1. Linux desktop, Wayland, ARM64/EGLFS and embedded are Qt-based user/community-validation paths; no absent Linux runner, maintained GUI viewer, cross-toolchain or board may block Windows development, candidate freeze or release. Never label an unverified cell Supported. Handle reproducible user-reported port defects through Issue -> bounded fix/maintenance/hotfix with truthful exact-cell evidence and notes. Keep Windows #144/#175/#370 Viewer/SLO and same-SHA L2 hard gates; **Qt5.15 and Qt6.8** exact applicable cells are the only first-GA LTS families. Qt6.5/Qt6.12 must wait until **after V1 GA #605**.  Do not preassign V0.3.4.0/V0.3.5.0. See [platform validation policy](docs/internal/windows-first-platform-validation-policy-2026-10-22.md).

Qt 6.5 LTS and Qt 6.12 LTS are both **post-V1.0.0.0 GA** support projects #605 (not pre-GA qualification gates); optional historical preflight is not a support claim. Changing GA scope, security promises or the machine release map requires an explicit Product Owner decision and reviewed authority updates.

Cross-cutting test-system work under #393/#394 may progress independently when it does not overlap the active product-path owner; it must not block unrelated V0.3 implementation work merely because its PR is open.

## Requirement classes

### V0.3.0 accepted self-service baseline (historical)

Owned by #234/#333; already released, not the active V0.3.3 task list:

- #264: clean self-service SDK/install/deploy for C++ / QML / Generic / QPA;
- #240: task-oriented docs and runnable learning paths;
- #335: minimum self-service diagnostics;
- exact release-truth obligations and admitted usability defects that block self-service.

### Mandatory pre-GA baseline

Windows-qualified #343 capability baseline before V0.4 entry; **Linux/embedded evidence listed below is aspirational/user-validation only**, not a release gate (Owner #563):

- **Qt 5.15 + Qt 6.8** product families only for the FIRST GA. Qt 6.5 + Qt 6.12 strictly after V1 GA (#605);
- **Windows x86_64 / qwindows** as the mandatory actual test/deployment environment;
- Linux x86_64, ARM64/AArch64, qxcb/Wayland/EGLFS as community/user-validation platform paths, with truthful per-cell states;
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

CI uses **PR-triggered, protected pre-merge product validation** (latest Owner #573 supersedes #571):

```text
L0 ordinary feature-branch commit -> no hosted Qt CI
L1 ready PR -> actual affected Windows Qt compile/CTest/install + Git Flow/review checks; all required statuses PASS before merge
L2 explicit frozen-candidate workflow_dispatch -> exact-SHA FULL_GATE, CANDIDATE
```

Rules:

- Iterate locally with focused tests and the canonical build/install path.
- A bare feature-branch push does **not** invoke hosted Qt CI. Opening/readying a PR does; subsequent updates to a ready PR **must revalidate the newest head**. Do **not** merge on a failed, skipped, cancelled or pending `ci-merge-verdict`; `develop` ruleset must require it and `p1-p2-disposition`. No repeat Qt build after merging. See [CI policy](docs/internal/ci-purpose-and-trigger-policy-2026-10-22.md).
- The PR `MERGE` decision is binding only after Windows selected tests and review gates PASS; `OBSERVE` and specialized diagnostic runs never authorize a merge.
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
