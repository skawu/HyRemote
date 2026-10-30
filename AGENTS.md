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

## Stop / continuation contract (hard)

Ending a work turn is an explicit state transition, not a normal way to report progress. An Agent may
end a turn only when one of these is true:

```text
1. the requested outcome is complete;
2. a Human must make a product/version/security decision;
3. a Human must perform a GitHub administrator action;
4. a required credential or environment is unavailable and no local substitute exists;
5. an external dependency is genuinely unavailable after the Agent has exhausted safe alternatives.
```

Before ending any turn, the Agent must record and satisfy this self-check:

```text
BLOCKER_TYPE=None | HumanDecision | HumanAdmin | Credential | Environment | External
WHAT_EXACTLY_IS_NEEDED=
WHY_AGENT_CANNOT_DO_IT=
ALTERNATIVES_ATTEMPTED=
NEXT_NONBLOCKED_TASK=
```

Rules:

- If `BLOCKER_TYPE=None`, the Agent must not end while any nonblocked task remains. It must continue with
  implementation, evidence collection, a different WBS item, or the next queued action in the same turn.
- `NOT_RUN`, `NOT_CONFIRMED`, a failed experiment, a red test, a failing review gate, or a pending CI job
  is an evidence/action gap, not a legal stop condition. Record it and continue with an alternative, a fix,
  or another nonblocked task.
- A physical Human action is a legal blocker only after the Agent has exhausted automatable alternatives;
  state the exact checklist instead of treating a failed automation attempt as Human-required.
- A status summary or "next steps" paragraph is not a terminal action while a nonblocked task exists.
- CI polling is not work: inspect checks only at meaningful milestones and use the interval for code,
  failure remediation, evidence, or another WBS item.

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

A planned or registered V0.3.x version does not weaken any accepted product qualification obligation.

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

**Hard boundaries for all local Agents and Owner workflows:**

- **One canonical command:** start in the repository workspace root selected by the project runner, then run `build.cmd test` on Windows (or `sh ./build.cmd test` on POSIX) for the complete suite. Do **not** invent a PowerShell command chain with `--qt-prefix`, `--build-dir`, manual `PATH`, `cd`, `cmake --build`, an external `ctest` acceptance gate, or post-processing pipelines. Build-system options are legitimate API surface, **not** an invitation for an Agent to reconstruct the environment per invocation.
- **Single configuration authority:** `build.yml` (or a repository-authorized named profile) and the canonical Kit resolver determine the Qt/toolchain/build directory, without an Agent guessing absolute installation paths or creating ad-hoc workspace/build directories. Treat the configured workspace root and the configured build tree as hard safety boundaries; an Agent must not substitute its preferred `build-qt683` directory, change a current build root, or use `clean/rebuild` to delete unverified paths. When the needed profile is missing/ambiguous, report the missing build configuration; do not write a speculative local override.
- **Local Agent visibility rule — 60 seconds, NOT a build-system timeout:** while the local Agent is carrying out work, **no more than 60 seconds may pass without a user-observable, useful work-process update**. Report the current operation, actual command/phase status, evidence gathered, and any detected blocker or next action. If a command is silent or taking time, the Agent must actively inspect its process/log when able and report the true state; do not hang silently or use repeated empty "still running" messages as fake progress. **This rule governs the Agent's work reporting, not CMake/CTest:** a valid long-running build/test must not be automatically killed just because its subprocess has produced no output for 60 seconds. No build-level idle-output watchdog or 60-second kill-on-silence feature is requested.
- **Evidence:** trust the canonical build's own process result, `test.log`, discovered/executed CTest counts and CI. Text filters (`Select-String`, `Out-String`, `Select-Object`) are for reading retained logs, not deciding PASS. A direct CTest invocation is permitted only as an explicitly bounded, non-authoritative test-debugging exception after verifying the build identity, never for an all-suite signoff.
- **No redundant shell wrapper:** the canonical build is responsible for process exit codes, environment handling, log retention and workspace protection. Do not append a second PowerShell `if ($LASTEXITCODE ...)` wrapper as the routine development interface. If build status propagation or directory safety is deficient, fix and test the canonical build authority, not a local one-liner. The Agent itself remains responsible for the separate 60-second work-visibility rule.

For **deliberate full verification only** (stable acceptance milestone/frozen candidate, not each edit), the user-facing command is:

```text
build.cmd test
```

The canonical runner must provide a concise resolved-configuration identity (Qt Kit, compiler, workspace/build root) before execution, preserve the native exit status, and reject ambiguous/unsafe environment. No physical-device or Viewer claim follows from CTest PASS. **Build-system safety work: #678 / PR #679** protects workspace/build-tree deletion. **The 60-second update rule is solely the local Agent's observable work-process discipline**, not a `build.cmd` watchdog requirement; do not create a parallel toolchain or introduce silent-output termination based on this rule.

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

- **Do NOT repeatedly run full tests while a feature is unfinished.** A local Agent may compile/inspect on demand, then run only relevant positive/negative component tests at a coherent behavior checkpoint. No ritual `build.cmd test` after every file edit, small W1/W2 slice, draft push or documentation change. A large count of passing irrelevant tests is not product progress.
- **Cadence is L0 focused → one stable Ready PR L1 affected lane → one frozen exact-SHA L2 qualification.** The existing [test necessity/cadence catalog](tests/TEST_CATALOG.md) classifies each test family. Do not interpret an entry marked KEEP as a command to execute it every time. Special high-risk code/ABI/security/safety changes require *their own affected negatives* before merge.
- For narrow local debugging on an already built, identity-checked tree, a direct focused CTest is diagnostic-only until the canonical build gains an approved positive test selector; do not script a second PowerShell acceptance path. Do not claim unrun tests PASS.
- Iterate locally with focused tests and the canonical build/install path when its corresponding acceptance decision is due.
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
