# HyRemote 贡献指南

> **中文优先。** 面向用户和贡献者的说明默认使用简体中文；API、代码标识符、命令、协议字段和门禁 token 保留正式英文拼写。英文镜像只在确有协作需要时同步维护。

HyRemote 欢迎符合项目边界的贡献。所有变更必须保持跨平台设计、四种并列接入方式和模块职责分离。

## 协作速查

- 先使用 Issue 描述用户问题、范围边界和可观察验收标准；一个 PR 只关联一个同仓库主责 Issue。
- 普通开发分支使用 `feature/<lowercase-kebab-case>`，缺陷修复使用 `hotfix/<lowercase-kebab-case>`；发行和回合分支使用 `release/vX.Y.Z.W`、`backmerge/vX.Y.Z.W`。分支名保持 ASCII，遵守 Git Flow；不要使用中文分支名或自行发明分支族。
- PR 正文必须包含且只包含一行 `Issue: #N` 作为主责关联；Issue/PR 标题、说明、验收与回报优先中文。
- 提交主题使用 `<type>: <中文摘要>`，正文优先中文；保留 `feat`、`fix`、`perf`、`refactor`、`test`、`docs`、`build`、`ci`、`chore` 等机器可识别类型前缀。
- `README.md` 是中文主入口；英文说明放在 `README.en.md`。代码、API、命令和门禁 token 不翻译、不重命名。
- 不批量改写历史 Issue/PR、已发布发行说明或源码注释；只在新建或实质修改时执行中文优先规则。

---

## Before coding

For non-trivial changes, start with a GitHub Issue describing:

- the problem or capability;
- affected application types (Widgets, Quick, mixed UI);
- affected layer (target, capture, transport, input, encoder, integration);
- public API impact;
- Qt/private API impact;
- platform-specific impact;
- validation plan;
- affected product milestone/version when applicable.

Architecture-affecting changes should be discussed and recorded before implementation.

Before editing, read the current `develop`, the target Issue and its latest authoritative comments, and search open Issues/PRs for overlapping ownership. Current repository/machine facts beat old chat, branch and proposal prose.

## Repository and branch ownership

Repository paths are architecture boundaries, not arbitrary folders. Follow [`docs/internal/repository-layout.md`](docs/internal/repository-layout.md):

- `src/` is the shipping tree: one Core, one Shared Runtime, then the four peer integration frontends under `src/integrations/{cpp,qml,generic,qpa}`; every frontend reuses the same Shared Runtime;
- `tests/` holds what a unit test is not: cross-module integration, clean consumers, product E2E, the public-surface
  contract, the third-party matrix and the release gates;
- module-private tests stay with their module (`src/*/tests/`) and are never moved here;
- there is no separate research tree: an experiment's conclusion is recorded under `docs/internal/**`, and a runnable
  experiment either becomes product code in `src/` or leaves only its conclusion;
- branding is a root directory of its own and nothing else: the product mark lives at `logo/`, examples reference it
  instead of copying it, and no module may depend on it.

Do not recreate the historical root `core/`, `remoteaccess/`, `qml/`, `qpa/`, `integrations/`, `verification/`, `assets/`, `research/` or `spikes/` directories as compatibility copies, and do not bring back the pre-ruling source names `src/embedded/`, `src/declarative/`, `src/transparent/` or the documentation-owned `docs/assets/` copy of the mark.

Branches are temporary work cursors. Follow [`docs/internal/branch-lifecycle.md`](docs/internal/branch-lifecycle.md) and [`docs/internal/git-flow-release.md`](docs/internal/git-flow-release.md): normally only `main`, `develop` and current open-PR heads remain visible. Historical auditability belongs to Issues, PRs, commits and release tags rather than stale branch refs.

## Product milestones and technical WBS

HyRemote 产品版本由用户可见结果和明确支持范围决定，不由内部实现阶段决定。版本语义见 [`docs/versioning.md`](docs/versioning.md)，当前计划与激活状态以唯一[产品路线图](docs/internal/product-version-delivery-roadmap-2026-10-23.md)和 `.github/release/release-trains.json` 为准。

当前最新正式发行是 **V0.3.5.1**（源码发行）。V0.3.5.2 是尚未激活的客户维护计划；V0.3.6.0 是已登记的开发诊断工作台 Feature，仍须完成 #586 新范围验收和发行门禁。`develop` 上存在实现不代表已经发布；不得把 PLANNED 版次写成当前正式版本。

Core、采集、传输、RemoteFrame、DMA-BUF、硬件编码、CI 和安全工作都应归属其实际支持的产品版本/WBS，不得脱离用户结果单独创建平行产品路线。

## Core boundaries

Do not introduce dependencies that make the core permanently depend on:

- a specific transport protocol such as VNC/RFB;
- a specific QPA platform such as EGLFS;
- a specific SoC such as RK3588;
- a specific hardware encoder such as RKMPP;
- Qt private APIs.

Such dependencies belong behind adapters/backends.

## Integration technologies

HyRemote intentionally supports four **peer product integration technologies**. A contribution must not remove, rank or reimplement another route merely to simplify one implementation:

- C++ (`HyRemote::RemoteAccess`);
- QML (`import HyRemote`);
- Generic Plugin (public-Qt zero-code integration that preserves the application's native platform identity);
- QPA (exact-compatible private-ABI platform-entry/native-delegate integration).

All four consume one Shared Runtime. Peer product status does not imply every route is Supported on every Qt/OS/QPA cell: per-cell `Supported`, `Qualified`, `Preview`, `Experimental`, `Unsupported` and `Not Applicable` outcomes remain truthful. Generic must not depend on Qt private/QPA interfaces; QPA is the only frontend that may own the exact private-ABI/native-delegate boundary.

Widgets and Qt Quick are target/UI families, not additional frontends or Runtime personalities.

## Qt application types

Qt Widgets and Qt Quick are first-class peers. Changes should state whether they apply to:

- QWidget/raster UI;
- QOpenGLWidget;
- QQuickWindow;
- QQuickWidget;
- Quick3D;
- mixed UI.

Do not claim compatibility without repeatable validation.

## Public APIs and Qt private APIs

Prefer Qt public APIs for stable functionality. If Qt private/QPA APIs are unavoidable:

1. isolate the QPA platform-entry ABI in the QPA compatibility seam, or the Human-approved `AR-04=A` Qt window-system input/high-DPI exception in its dedicated shared Runtime implementation unit; do not expand that exception to Core, public SDK types, or unrelated Runtime/frontend code;
2. document the exact Qt versions tested;
3. keep private types out of HyRemote public headers, and truthfully declare the shared Runtime's qualified Qt Private development/ABI requirements;
4. add compatibility tests before enabling support for a new Qt minor/patch cell.

Do not create a second Qt5, embedded or accelerated Runtime to absorb version/platform differences.

## Dependencies

New third-party dependencies require an Issue that records:

- upstream repository and release/commit;
- license;
- maintenance/community status;
- security/update strategy;
- why existing dependencies are insufficient;
- whether the dependency is required or optional.

Avoid vendoring a fork when a maintained upstream library can be used or extended upstream.

## Pull requests

A pull request should be focused and independently reviewable. It should include:

- linked Issue;
- scope and non-scope;
- architecture/API impact;
- tests performed;
- platforms/Qt versions tested;
- known limitations.

Do not combine architecture changes, unrelated refactors, and feature work in one PR.

After a PR is merged, superseded or closed and no other open PR uses its head, delete the short-lived branch. Do not keep task branches as permanent documentation.

## Compatibility claims

A feature or matrix cell is considered **supported** only when the current product authority and exact evidence justify that status. At minimum the claim needs:

- a reproducible build/install/deploy path;
- applicable functional/product evidence;
- an explicit compatibility entry;
- documented known limitations.

Do not infer a Qt/OS/CPU/QPA/graphics cell from another platform's result. QPA private-ABI compatibility is exact-cell truth, separate from public-Qt family claims.

## 文档、注释与提交语言

**中文优先，机器接口保持稳定。**

- 根目录 `README.md`、Issue/PR 模板、贡献说明和用户指南以中文为主；英文镜像使用 `README.en.md` 或 `docs/en/` 对应路径，并与中文主文档在同一变更中同步。
- 新建或实质修改的产品参考文档、内部说明、ADR 和研发记录优先使用中文解释。历史英文文档不要求批量翻译；需要修改时只翻译本次涉及的说明，不改变既有结论。
- 发行门禁、脚本或自动化读取的固定字段、状态值、路径、文件名和 token 保留英文原文。若确需调整，必须在同一受审变更中同步修改门禁并验证，不得只翻译文档造成门禁失效。
- 新增或实质修改的源码注释默认使用中文；已有注释不做批量迁移。公开 API 文档或跨语言约定确有必要时可保留英文，避免无意义的中英重复。
- C++ 类型、函数、命名空间、QML 属性、CMake target、日志 token、协议字段和命令行参数沿用正式标识符；语言偏好不构成重命名理由。

## Git 提交主题

提交主题采用 `<type>: <中文摘要>`，必要时正文补充中文背景、影响和验证结果。类型前缀保持稳定，便于工具检索：

- `feat: 增加 QWidget 目标适配`
- `fix: 修复断连后的旧帧残留`
- `perf: 降低空闲采集开销`
- `refactor: 收敛会话生命周期`
- `test: 补充 QPA 负例`
- `docs: 更新中文接入说明`
- `build: 修正 Qt6 QML 部署`
- `ci: 调整 Git Flow 门禁`

禁止用纯英文长句替代中文摘要；不得为了中文化修改机器识别的类型前缀、源码标识符或门禁 token。

## Issue、PR 与分支约定

- 新 Issue/PR 标题和正文优先中文，结构化模板负责收集版本、SHA、范围、验收与验证信息。
- 分支名遵守 Git Flow，使用 ASCII 小写 kebab-case：`feature/<topic>`、`hotfix/<topic>`、`release/vX.Y.Z.W`、`backmerge/vX.Y.Z.W`。分支名属于机器标识，不使用中文、空格或随意增加前缀。
- 普通 feature/hotfix PR 正文必须有且仅有一行 `Issue: #N`，链接一个同仓库主责 Issue；历史编号分支和受保护发行准备例外按现有门禁兼容，不批量重命名。
- 历史 Issue/PR 不批量翻译或改名；从新建内容开始执行中文优先规则。

## License and contributions

HyRemote is licensed under the **Apache License 2.0**. Unless a contribution is explicitly marked otherwise and accepted under a compatible license, contributions intentionally submitted for inclusion in HyRemote are provided under the Apache License 2.0, consistent with Section 5 of that license.

Do not add source or assets copied from external projects unless their license compatibility, attribution, and redistribution requirements have been reviewed first.

The Apache License 2.0 does not grant trademark rights. Project names, logos, trademarks, and brand assets may have separate usage terms.

## Hosted CI cadence

Hosted runners exist to produce decisions, not to act as a remote compiler. The classifier in
`.github/scripts/resolve-ci-scope.py` selects one of four lanes, and its self test is the authority for what each
lane may start:

| Lane | Trigger | What runs |
| --- | --- | --- |
| `PR_DRAFT` | draft pull request | Git Flow/topology, release-authority governance when release authority changed. No Qt runner. |
| `PR_FAST` | ready pull request | Only what the real `base...head` diff can affect: affected capabilities, the deploy evidence for contracts it touches, readiness when it touches a readiness-consumed surface. |
| `DEVELOP_SENTINEL` | push to `develop` | Repository policy only. The merged pull request already passed its own hosted acceptance, so the dual-platform matrix is not repeated on every merge. |
| `FULL_GATE` | admitted `workflow_dispatch` (`purpose` + `expected_sha` == the dispatched commit) | Both platforms, all four frontends, all readiness gates, all applicable clean-deploy evidence. This is the lane for an exact candidate, a release-branch readiness run, main/GA validation or a deliberate diagnostic. |

### The three decision layers

Every CI run reports one **decision** — what the run may be waited on for. The decisions map to three layers of
development cadence:

| Decision | Layer | Meaning | Who waits |
| --- | --- | --- | --- |
| `OBSERVE` | L0 — Local development feedback | Fire-and-observe: no merge or release decision is waiting on this lane (draft PRs, the develop sentinel, no-product reporting, maintenance and audit-only runs). | Nobody. **OBSERVE workflows do not block sequencing of independent work** — a later red result opens a defect owner, it is not a reason to idle. |
| `MERGE` | L1 — Ready PR merge decision | Hosted evidence for a ready pull request's merge decision. Decided by the lane, not by matrix size: a ready docs-only PR is still a MERGE decision. | The author of that pull request, before merging it. |
| `CANDIDATE` | L2 — Frozen exact-candidate qualification | Full qualification of one frozen, exact SHA for candidate/release purposes. | Whoever will act on the candidate evidence. |

### Candidate standard flow

For candidate-producing work, run the layers in order and never start the expensive one while edits are still
pending:

```text
local canonical checks
→ user-path preflight
→ freeze exact SHA
→ admitted FULL_GATE
→ physical/user validation only if required
```

A `FULL_GATE` is admitted, not generic: it requires `purpose` (`candidate`, `release-readiness` or `diagnostic`) and
an `expected_sha` that must equal the dispatched commit. The gate fails closed before any Qt runner starts when the
SHA is malformed or does not match, so a branch-name dispatch can never be mistaken for candidate evidence.

**A candidate SHA, once changed, invalidates the previous candidate evidence automatically.** Re-running for the new
SHA is legitimate and must be recorded as such; reusing evidence from a superseded SHA never is.

### Specialized historical evidence

`tls-preflight.yml` is historical/specialized evidence for the closed #258 preflight and is explicit-dispatch only.
It is not a development or release wait gate and must never be consumed as one unless a future activated security
capability explicitly re-adopts it. Nothing in this cadence re-activates a TLS product scope.

Working rules that keep that cadence meaningful:

- **Push once per stable change.** During unfinished work, compile and use focused positive/negative tests at meaningful checkpoints; do **not** repeatedly run the full suite. Before opening a Ready PR, validate the affected contract; run one local canonical full `build.cmd test` only when the risk matrix in [G0–G6](tests/EXECUTION_MANUAL.md) actually requires it. Every ready-PR head still requires its own fresh hosted affected L1 `ci-merge-verdict`; an older head's MERGE decision cannot be reused. A second push is for a genuine fix/review change, not to use CI as a trial-and-error loop.
- **Drafts are cheap on purpose.** A draft runs governance only, so it is safe to open early for collaboration; the
  product gate starts when the pull request becomes ready for review.
- **Merge only from a fresh base.** A merge commit is built from the pull request head against the base's current tip,
  so a hosted run against an older base no longer describes what merges. The Git Flow policy checks this at merge
  time: if the base moved, merge the base into the branch, re-run the gate, and merge only afterwards. That is what
  makes it safe not to repeat the full matrix after every merge.
- **Wait only where a decision is pending**: an `OBSERVE` run is never one of them. Wait on the `MERGE` decision of
  your own ready pull request and on an admitted `CANDIDATE` gate. Do not wait on ordinary commits, drafts,
  post-merge duplicates or maintenance runs; poll by run id and stop as soon as the run reports completion, because
  an empty answer is "unknown", never "finished".
- **Artifacts are evidence, not routine.** Build evidence is uploaded for a failure, for the clean SDK/deploy
  acceptance contracts a run was asked to prove, and for `FULL_GATE`. A documentation or governance change does not
  need a build-evidence bundle.
- **Release-authority work needs no Qt.** Changes to `.github/release/**`,
  `tests/release-readiness/release_scope.cmake` or `check_release_authority_policy.cmake` run the lightweight
  governance lane, which executes those policy scripts directly. `governance=true` deliberately does not imply
  `product=true`.

Reducing frequency must never reduce coverage of the contracts that actually depend on cross-platform behaviour:
core, runtime, integrations, public headers, the deploy/install authority, package metadata, clean-consumer fixtures,
the RFB transport and QPA private ABI still require both platforms on the final ready pull request.
