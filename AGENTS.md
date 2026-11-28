# HyRemote Agent Execution Entry

This file is the repository-visible starting point for ChatGPT, CodeBuddy and local/AI agents working on HyRemote.

**Operational role/handoff authority:** [Cloud Owner ↔ Local Owner ↔ Human contract](docs/internal/cloud-local-human-operating-contract.md) (#768).
Cloud creates and verifies approved machine work orders; an actually running Local Owner Worker
pulls/ACKs and performs qualified Windows tasks; Human only grants exceptional permissions,
answers exceptional product decisions or performs unavoidable physical operations. Human must **not**
be asked to relay each task or ask for instructions. Issue comments are not deliveries, and
neither a Cloud nor Local ChatGPT conversation automatically runs in the background.
The [Local Owner inbox setup](tools/local-owner/README.md) is opt-in once per authorized Windows
environment; until the first real ACK, report `RECORDED_UNDELIVERED` rather than "已下发".

## Human-triggered continuation (mandatory)

On “继续工作” or an otherwise unspecified continuation request, read the current upstream [#768 Owner dispatch entry](https://github.com/skawu/HyRemote/issues/768) and its latest applicable dispatch comment, plus [#594 version authority](https://github.com/skawu/HyRemote/issues/594). Read the latest results in the selected owning Issue/PR. Do not execute a historical snapshot or rely on chat memory. Check existing work, claim the highest-priority authorized unclaimed task, and post a short receipt to its owning Issue (task/dispatch comment ID, target version, exact starting SHA, Owner); then implement and report there. If the task is already claimed or finished, continue the next independent authorized task. If GitHub cannot be read, report the actual access blocker rather than guessing. This is a Human-started interactive Agent path, not an unattended executable order; the reviewed machine inbox and its security requirements remain unchanged. Human need not forward task text. A recorded dispatch is not a verified receipt.


## Design and verification contract (mandatory)

Before assigning a nontrivial implementation slice, the Cloud Owner must define its approved behavior, implementation boundary, and executable verification plan together, in the existing owning Issue/design. The plan identifies acceptance criteria, existing registered test identities or tracked drivers, canonical invocation/profile and environment, when to run each (iteration, integration, candidate), expected oracle, reusable evidence and unqualified cells. Specify new tests or scripts only for a demonstrated coverage gap, with their intended tracked path and oracle before implementation. Do not create another test framework or document for each task. Local Owner confirms actual environment and executes the plan; it does not independently expand product design, compose alternative build/test pipelines, copy probes or add acceptance criteria during implementation. Ordinary diagnostic commands may inspect a real failure without becoming a new acceptance path. If a newly discovered risk invalidates the plan, record the reason, affected contract and minimal proposed adjustment in the owning Issue for Cloud Owner disposition before expanding the test/implementation scope; continue independent approved work. Do not require Human approval for routine work or waive mandatory security/ABI/CI/release gates. Cloud Owner reviews the plan and resulting diff/evidence, including unnecessary implementation and repeat runs. A missing plan is a Cloud Owner responsibility, not permission to improvise.

## Implementation economy (mandatory for agents)

Read `.codebuddy/rules/lean-engineering.md` before designing or reviewing a code change. For nontrivial engineering use the existing `engineering-task-execution` Skill via actual client discovery; for scripts also use `script-planning`. If discovery fails, read the tracked Skill directly and report `NOT_LOADED`, not an invented invocation. Favor the smallest correct implementation, existing Qt/runtime mechanisms, and reused risk-selected tests. No extra abstraction/test executable/CI workflow without a concrete uncovered product risk; do not invent a Human approval step for routine testing. This does **not** reduce current protected security/ABI/release gates. PRs must explain any new test identity and show actual product evidence, not test counts. This section is an execution constraint, not authorization to change approved V15/V8 architecture.

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

## Product-phase historical snapshot (2026-10-26; not the current work queue)

**当前产品与发行状态（2026-11-28；优先于下方历史记录）：**

- 最新正式发行：`V0.3.5.1`，正式标签 `v0.3.5.1` 指向 `main@3cbc1cdcf6b6d2f9156ab139d7f88ed0225529de`，发行形式为源码发行。
- `V0.3.5.2` 是计划中的客户维护目标，尚未登记为可选 machine train、未激活；#567/#790 仍开放，不得宣称客户现场问题已解决。
- 当前 `V0.3.6.0` train 已登记，按 Human H-T01=B 定义为 SDK 研发与诊断工作台。PR #1101 已合并至 `develop@2a40b48c920a7ddfb391b22628d63b4e708e4af2`；#581 实现任务已关闭，#586 新范围安装验收仍开放。
- 旧 V0.3.6.0 冻结候选 PR #1076 仍为 Draft/HOLD，源码不包含 PR #1101 新工作台；未经新候选 lineage、同 SHA 验收和完整发行门禁，不得合并 main、打 tag 或发布。
- HyRemote 是独立开源 SDK/Runtime 产品；HyRemoteView 是独立客户端仓库。HyRemote 仓库不得加入高级功能或商业功能描述/实现；View 的功能、G3/G4/Gate-D/Gate-I 状态不得混作 HyRemote 发行事实。
- Windows x86_64 是当前必须完成真实资格验证的平台；Qt 6.8.3 是四种接入方式的参考资格单元，Qt 5.15.2 仅保留 C++/Widgets Limited 路径。Linux、ARM64、嵌入式 Linux、Wayland/EGLFS 仍为用户验证待完成，不得推导为 Supported。Qt 6.5/6.12 在 V1.0 GA 后另行准入。
- 当前任务、版本激活、mandatory children 与候选选择以本文件引用的[唯一产品路线图](docs/internal/product-version-delivery-roadmap-2026-10-23.md)、#594 和 `.github/release/release-trains.json` 为准；历史 charter、旧 Issue 正文和旧 PR 描述不能覆盖当前事实。

**下方 2026-10-26 的 V0.3.4.0 charter 与执行顺序仅保留为历史审计记录，不是当前任务队列。**
2026-10-26 V0.3.4.0 execution order (historical; **do not execute without a new reviewed order**):

```text
verify latest develop + merged roadmap + machine train + Issue ownership + exact-SHA evidence
 -> implement the current **V0.3.4.0** Windows Qt5.15.2/Qt6.8.3 SDK/Runtime product breadth (#603/#265/#566), with one Core and one Shared Runtime
 -> use focused diagnostics for iteration, then canonical build.cmd build/test/install and current-head PR CI/review; do not assemble a parallel acceptance route in shell snippets
 -> preserve truthful per-cell Qt/Widgets/Quick/C++/QML/Generic/QPA/deploy/Viewer claims; Linux/embedded evidence remains nonblocking for first Windows GA
 -> plan V0.3.5.0 Host config/text watermark, V0.3.6.0 Tool S1 and conditional V0.3.7.0 Docker per merged roadmap, not as active V0.4 work
 -> enter V0.4 qualification-only RC only after explicit Owner/machine activation and accepted pre-GA Windows product scope
 -> promote a qualified V0.4 RC to V1.0 GA through the release authority; post-GA Qt6.5/Qt6.12 are planned under #605
```

**Latest Owner decision #563 (supersedes #558's platform-validation gate):** Windows x86_64 is the *only mandatory physical/runtime qualification platform* for V0.3.3/V0.4/V1. Linux desktop, Wayland, ARM64/EGLFS and embedded are Qt-based user/community-validation paths; no absent Linux runner, maintained GUI viewer, cross-toolchain or board may block Windows development, candidate freeze or release. Never label an unverified cell Supported. Handle reproducible user-reported port defects through Issue -> bounded fix/maintenance/hotfix with truthful exact-cell evidence and notes. Keep Windows #144/#175/#370 Viewer/SLO and same-SHA L2 hard gates; **Qt5.15 and Qt6.8** exact applicable cells are the only first-GA LTS families. Qt6.5/Qt6.12 must wait until **after V1 GA #605**.  The exact active Feature, planned maintenance targets and activation status must always be read from the canonical roadmap and `.github/release/release-trains.json`; never hardcode an old current-version claim in this execution entry. See [platform validation policy](docs/internal/windows-first-platform-validation-policy-2026-10-22.md).

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

For an explicitly required **milestone/candidate full verification** (not routine edits or every Ready PR), the canonical command is:

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

- During unfinished development, compile and use focused positive/negative tests at meaningful checkpoints; never run the full suite per edit, slice, draft or docs-only change. Use the canonical full build/test/install path only for the risk-authorized G2 or frozen-candidate G6 decision. See [test execution manual](tests/EXECUTION_MANUAL.md).
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

## Shared engineering conventions

Cloud Owner maintains the task/roadmap and execution rules; Local Owner implements the authorized product scope. Reuse mature, suitable Skills before adapting or creating one; verify actual availability, license and safety. New project-owned source files and technical documents use a hy prefix, except established fixed filenames and compatibility-required paths. Use Chinese logical commits. New ordinary feature/hotfix branches use semantic lowercase kebab-case topics without Issue/PR numbers; put exactly one `Issue: #N` in the PR body. Existing numbered branches remain compatible; protected release-preparation and older protected base policies keep their required format until migrated. Preserve existing protection and avoid bulk renaming. Use the always-applied lean-engineering rule for minimum implementation, affected-test reuse and short actual efficiency review; customer maintenance is prioritized and receives its own incremental Maintenance version under the existing release authority.

## Scope discipline

Prefer the smallest architecture-compatible solution. A bug in one Qt control must not create control-specific product semantics. A platform or performance opportunity must not create a second Runtime. Optional future capabilities must not leak into current release acceptance.

If a change would alter product scope, GA support claims, the one-Runtime architecture, major frontend hierarchy, permissions/system-daemon behavior or desktop-wide/native input semantics, stop at the Product Owner decision boundary instead of silently expanding scope.

## 中文优先与 Git 元数据规范

- 面向 Human、用户和贡献者的 README、Issue/PR 标题与正文、模板、贡献说明和新建技术文档默认中文优先；确有国际协作需要时提供同步英文镜像。
- Git 提交主题使用 `<type>: <中文摘要>`，提交正文优先中文；例如 `feat: 增加 Qt Quick 目标适配`、`fix: 修复断连后的旧帧残留`、`docs: 更新中文接入说明`。合并标题也使用中文。
- 分支名是 Git/CI 机器标识，按 Git Flow 使用 ASCII 小写 kebab-case：`feature/<topic>`、`hotfix/<topic>`、`release/vX.Y.Z.W`、`backmerge/vX.Y.Z.W`；不得使用中文、空格或随意增加分支族。语义分支的唯一主责 Issue 按门禁写入 PR 正文的 `Issue: #N` 行。
- API、C++/QML/CMake 标识符、命令、路径、协议字段、CI 状态值和门禁解析 token 保留正式英文拼写；中文优先不意味着重命名机器接口。
- 仅适用于新建和实质修改内容；不批量改写历史 Issue/PR、源码注释、已发布发行说明或受门禁保护的历史 token。
