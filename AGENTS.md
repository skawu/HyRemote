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

**V0.3.0.0 Product Preview** was released on 2026-10-19. **V0.3.3.0 was validly reissued on 2026-10-24** following the documented retraction/remediation incident; the earlier retracted candidate remains historical audit evidence, not the current release. #144/#175 are historical delivered work; #370 remains long-lived performance governance. **Current development Feature = V0.3.4.0**, Windows Qt5.15.2/Qt6.8.3 exact qualified SDK/Runtime product cells (#603/#265/#566), with machine train registered by merged PR #655; a registered train is not release approval. See the [single current roadmap](docs/internal/product-version-delivery-roadmap-2026-10-23.md), [local Owner task charter](docs/internal/v0.3.4.0-windows-qt-lts-development-charter.md), and `.github/release/release-trains.json` for separate planning and activation authority. V0.3.1.0/V0.3.2.0 stay retired; V0.4 RC is inactive and qualification-only; V1 GA is not released.

**Local Owner / Agent execution handoff:** use the [current V0.3.4.0 product charter](docs/internal/v0.3.4.0-windows-qt-lts-development-charter.md) and [merged canonical roadmap](docs/internal/product-version-delivery-roadmap-2026-10-23.md) with #603; `docs/internal/owner-execution-brief-2026-10-21.md` and the earlier V0.3.3 retraction/remediation notes are retained as history, not current work queues. Do not let historical text override the actual reissued V0.3.3 Release, product roadmap, machine manifest, Issue, or exact-SHA verification.

Current execution order:

```text
verify latest develop + merged roadmap + machine train + Issue ownership + exact-SHA evidence
 -> implement the current **V0.3.4.0** Windows Qt5.15.2/Qt6.8.3 SDK/Runtime product breadth (#603/#265/#566), with one Core and one Shared Runtime
 -> use focused diagnostics for iteration, then canonical build.cmd build/test/install and current-head PR CI/review; do not assemble a parallel acceptance route in shell snippets
 -> preserve truthful per-cell Qt/Widgets/Quick/C++/QML/Generic/QPA/deploy/Viewer claims; Linux/embedded evidence remains nonblocking for first Windows GA
 -> plan V0.3.5.0 Host config/text watermark, V0.3.6.0 Tool S1 and conditional V0.3.7.0 Docker per merged roadmap, not as active V0.4 work
 -> enter V0.4 qualification-only RC only after explicit Owner/machine activation and accepted pre-GA Windows product scope
 -> promote a qualified V0.4 RC to V1.0 GA through the release authority; post-GA Qt6.5/Qt6.12 are planned under #605
```

**Latest Owner decision #563 (supersedes #558's platform-validation gate):** Windows x86_64 is the *only mandatory physical/runtime qualification platform* for V0.3.3/V0.4/V1. Linux desktop, Wayland, ARM64/EGLFS and embedded are Qt-based user/community-validation paths; no absent Linux runner, maintained GUI viewer, cross-toolchain or board may block Windows development, candidate freeze or release. Never label an unverified cell Supported. Handle reproducible user-reported port defects through Issue -> bounded fix/maintenance/hotfix with truthful exact-cell evidence and notes. Keep Windows #144/#175/#370 Viewer/SLO and same-SHA L2 hard gates; **Qt5.15 and Qt6.8** exact applicable cells are the only first-GA LTS families. Qt6.5/Qt6.12 must wait until **after V1 GA #605**.  Current exact Feature = V0.3.4.0; subsequent known user outcomes have **PLANNED** numbers in the [single canonical roadmap](docs/internal/product-version-delivery-roadmap-2026-10-23.md), but are not automatically activated or releasable. See [platform validation policy](docs/internal/windows-first-platform-validation-policy-2026-10-22.md).

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

**Everyday development and final local acceptance MUST use the canonical `build.cmd` + `build.yml` entry.** Avoid building a second implicit build contract with shell snippets that manually prepend Qt/DLL paths, run stale build trees, or filter CTest success text. The canonical runner already selects an exact Qt Kit, configures/builds when needed, adds current Qt/runtime DLL locations for Windows tests, writes `<build-dir>/test.log`, checks nonzero discovered/executed tests, and fails on CTest errors. A green-looking `Select-String` output is not an exit-code oracle.

Recommended Windows PowerShell invocation for the **full, applicable Qt6.8.3 local suite** (run in an appropriate MSVC developer environment; the explicit Qt path is an example, not a claim about the current machine):

```powershell
.\build.cmd test --mode=all --qt-prefix=C:/Qt/6.8.3/msvc2022_64 --build-dir=build-qt683 --no-prompt
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
```

For **Qt5.15.2**, choose a *separate* build dir and only the qualified C++/Widgets frontend cell, e.g. `--mode=cpp --qt-prefix=C:/Qt/5.15.2/msvc2019_64 --build-dir=build-qt515`. Build trees must not be mixed across Qt major/Kit/compiler identities. Use `--show-config` to diagnose selection, `--test-exclude=REGEX` for documented scope exclusions, and `--env=KEY=VALUE` only for explicit test-environment overrides.

A direct `ctest --test-dir <existing-build-dir> -R <test-name> --output-on-failure` is allowed **only for a fast focused diagnostic on an already configured/built tree**. It is not authoritative all-suite evidence or a substitute for the canonical test gate, and must explicitly preserve the native process exit code. PowerShell text pipelines through `Select-String`, `Select-Object` or `Out-String` are for viewing an already retained log, not for deciding PASS. No made-up physical/Viewer claim follows from a CTest PASS.

**Workflow usability gap:** the current wrapper has no dedicated `--test-only` / CTest `-R` positive selector; the full `build.cmd test` intentionally configures/builds/tests. Improving targeted re-runs belongs to test/tooling work under #393, without spawning a second build authority. Do not work around missing convenience flags by permanently replacing the canonical workflow with shell compositions.

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
