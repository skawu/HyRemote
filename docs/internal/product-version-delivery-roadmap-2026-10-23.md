# HyRemote 开源 SDK/Runtime 产品版本路线图：Qt 5.15 + Qt 6.8 首轮 GA，Qt 6.5 + Qt 6.12 后置

> **仓库范围与当前状态（2026-11-28）：** 本路线图只管理 HyRemote 独立开源 SDK/Runtime 的公开 API、四种接入方式、Qt 目标适配、RFB 传输、构建/安装/部署、安全、性能、资源生命周期、开发诊断和兼容性。它不列示其它产品范围。最新正式发行是 V0.3.5.1；V0.3.5.2 为尚未激活的客户维护计划；V0.3.6.0 已登记为 SDK 开发与诊断工作台，#581 实施完成，#586 新范围验收仍开放。旧候选保持 Draft/HOLD，不构成发行授权。


> **2026-11-27 当前执行状态（优先于所有历史排期）：** Human [H-T01=B 正式裁决](https://github.com/skawu/HyRemote/issues/594#issuecomment-7214790385) 将 Tool 转为 SDK 研发与诊断工具。V0.3.5.1 已正式发布，客户现场静态单 Viewer CPU 改善，#567/#790 未解决；独立维护 V0.3.5.2 为计划目标、未激活。V0.3.6.0 保持版号，重新定义为开发诊断工作台首期；旧冻结发行候选 HOLD，受审候选更新前不得发行。原已发布 RFB 访问仅作兼容维护，不新增 Viewer 功能，暂不设退役日期；不扩建设备/远程会话工作台，不重写 Viewer。其余 Qt/SDK、Docker、RC、GA、既定维护与发布保护不变。

> **仓库职责：** HyRemote 是独立开源 SDK/Runtime；本文件只决定本仓 API、版本、兼容性和发行计划。外部使用者可固定消费已发布 SHA，但不改变本仓产品所有权、研发或发布权。


> **2026-11-02 线下用户资源负担反馈 — 即时优先级调整（#609/#370/#374）：** 用户已反馈启用/使用 HyRemote 给原 Qt 应用带来明显资源负担，尚无足够可比的 CPU 增量、内存增量、GPU 或性能退化量化证据。**将 #609 提升为 P1 产品质量执行优先级，立即开展实测与有界优化**；它不是仅供 GA 后讨论的技术储备。该变更是跨版本维护任务优先级调整，**不新增 Feature 编号、不调整 V0.3.6.0 Tool S1 已规划目标、不修改 machine release train / mandatory / V0.4 RC 激活状态**。V0.3.5.0 已发现的 P2 发布阻断修复须经新的精确候选资格验证；性能专项可在隔离工作树中并行推进，须在大范围 V0.3.6 Tool S1 新功能开发前形成第一轮可审核的用户资源基线、瓶颈归因与最小改进或 NO-GO 结论。
>
> **首轮用户可见验收：** 同一 Qt 应用/同一 SHA/相同分辨率和工作负载，比较未启用 HyRemote、启用但 0 Viewer、1 Viewer 静态/局部交互/高变化、2 Viewer 时的 Host 进程 **CPU 增量、内存（RSS/Private Bytes）增量、峰值、帧龄与本地 UI 交互/流畅性**；Viewer CPU/RSS 单独记录，不混入 Host 开销。优先 Windows Qt 6.8.3 1080p Widgets 与 Qt Quick，真实 RK3588/EGLFS 仅在设备可用时另记证据，不冒充 Windows 首 GA 门禁；#374 保存可复现 exact-SHA 记录。定位抓帧、GPU 读回、全帧扫描、拷贝、Raw/TRLE 编码的实际热点后，以最小 PR 优化并验证 before/after。本地 UI 不应为了降低 Host CPU 而明显退化；**没有测量基线不得编造“CPU 增加 x%”“内存增加 y MB”或已支持 GPU 加速**。
>
> **发布与加速边界：** RC3 既有证据仅覆盖已测范围；新增 P2 阻止发行，后续精确候选按 #95 独立重新验收。性能证据若发现影响已承诺支持范围的严重资源无界、死锁或确切已成立 SLO 失败，交 #95 明确裁定是否阻断候选；其余优化遵循 #24/#95 的单独维护/后续切片准入，不擅自扩展 V0.3.5.0 已批准发行范围。GPU/零拷贝/硬件视频编码只有在可移植 CPU 路径无法满足实测预算时，才按 #123 独立 GO/NO-GO，并保持兼容回退。


> **Owner Decision 2026-10-27:** D1 popup visual is NOT_CONFIRMED and deferred to V0.4 qualification, not a V0.3.4 hard gate; D2 accepts bounded Qt5 C++/Widgets while Qt5 Quick/QML/Generic/QPA stay EXPERIMENTAL/UNSUPPORTED/REJECTED; D3 requires legal custom build.directory end-to-end; D4 makes --no-config internal diagnostic; D5 keeps --qt-prefix advanced; D6 makes existing build identity win over environment discovery and requires explicit switching; D7 makes RealVNC 7.15.1 the required maintained Viewer. Details and evidence links are recorded in #594/#603.

> **本仓版本事实源：** 本文件决定 HyRemote SDK/Runtime 与开发工具的用户成果、计划版本、顺序、依赖和条件状态；`docs/versioning.md` / `docs/en/versioning.md` 解释版本语义，`.github/release/release-trains.json` 决定机器登记的 train 与 mandatory children，`docs/compatibility.md` / `docs/en/compatibility.md` 记录实际资格单元。已发布发行说明和标签属于历史事实。

> **按问题读取事实源：** 计划路线读本文件（`develop` 最新已合并版本）；当前 train/mandatory/RC 激活状态读 machine manifest 与 release gate；详细验收读责任 Issue 当前正文及其 PR/CI；已发布与 Supported 声明读 Release/tag、精确 SHA、兼容矩阵和实证。旧 Issue 段落、旧评论、旧 PR 分支不能单独推翻当前事实。

- 产品范围决定：2026-10-23。**最新 Owner 决定覆盖较早“Qt 5.15/6.5/6.8 在首轮 GA 前全部完成”的规划**：V1.0.0.0 前只要求 Qt **5.15 LTS** 与 **6.8 LTS** 的准确 Windows 合格单元；Qt **6.5 LTS** 和 **6.12 LTS** 在 **V1.0.0.0 正式 GA 后**各自推进（#605）。
- **Qt 5.15/6.8 适配优先于 HyRemoteTool 工作台完整化**。HyRemoteTool 是 D 综合工作台，A 优先的远程连接/基础诊断按小范围 PR 随多个版本逐步完善。
- **2026-10-29 当前明确开发版本：V0.3.5.0（#591 → #589）**，即宿主配置/持久化路径先于服务端静态文字水印；对应 machine train 需由**独立 machine PR** 登记，登记前不构成可开发或可发布证明。前一开发版本 **V0.3.4.0（#603）已于 2026-10-29 发布**（tag `v0.3.4.0` → `main` `27bf97d`，Windows Qt 5.15.2 + Qt 6.8.3 精确单元；证据见 [V0.3.4.0 发布说明](../releases/v0.3.4.0.md) 与 [D05 验收记录](v0.3.4.0-d05-acceptance-2026-10-29.md)）。**有编号的 PLANNED 目标不等于 Release 已就绪**；每个 Feature 仍按 #24/#95 对可交付用户成果逐个准入，不能把 WBS 技术包当新 Feature。
- **2026-10-29 V0.3.4.0 已发布**：tag `v0.3.4.0` 指向 `main` `27bf97d`，候选与验收 SHA `c57dae0`，同 SHA L2（run 40812083626）与 RealVNC 7.15.1 D05 实机均 PASS；其 machine train 由 **PR #655 已合并到 develop** 登记。发布后补入被引用的 D05 记录并在公告前把 tag 重指到修正后的 main HEAD，Git Flow tag 审计通过。细节见 [V0.3.4.0 发布说明](../releases/v0.3.4.0.md)。
- 历史有效发行 v0.1.0.0、v0.2.0.0、v0.2.0.1、v0.3.0.0 不追改；0.3.1/0.3.2 retired 不复活。**2026-10-23 首次错误发布的 `v0.3.3.0` 曾被撤回；2026-10-24 已通过正确的 `release/*→main` 流程合法重新发布 `v0.3.3.0`**（tag `a352a3e`，release code head `d6cb758`；Windows Qt6.8.3 L2 **85/85**），详见 [正式 Release](https://github.com/skawu/HyRemote/releases/tag/v0.3.3.0)、[勘误](../releases/v0.3.3.0-errata.md) 和历史 [整改审计](v0.3.3.0-release-remediation-2026-10-23.md)。**证据限制**：部分 Viewer/认证/性能数据来自先前候选 `de94a0a`，并非全部在 `d6cb758` 重跑；发布页面无二进制附件。不得抹去早期错误发布，也不得声称当前仍无合法发布；#370 仍是跨版本性能治理。当前开发 Feature 已明确为 **V0.3.4.0**，且 machine train 已由 PR #655 登记；但在 mandatory 验收和 #95 正式放行前不能创建其 release/tag。**V0.4.0.0 RC 是 machine `conditional_trains` 明确 `active:false` 的未激活候选；V1.0.0.0 GA 则已作为 machine `trains` 中可选的规则配置存在，但 GA 尚未通过 #33/#95/RC 门槛，未被接受、更未发布**。`machine profile selectable` 绝不等于已获准发布或已取得 Supported 资格。
- 仅 Windows x86_64 是首轮强制真实构建/设备测试环境（#563）。Linux/Wayland/ARM64/EGLFS 为用户验证/资格未定，不能无证据宣称 Supported，也不阻断独立 Windows 成果。

## 2026-10-26 — Cloud Owner 对本地开发的明确版本交接

**即时开发版本 = V0.3.4.0 / Windows Qt5.15.2 + Qt6.8.3 产品化（#603）**，不是 V0.4.0.0。此决定消除了“版本未指定导致本地 Owner 直接规划 V0.4 功能开发”的风险；[V0.3.4.0 可执行任务书](v0.3.4.0-windows-qt-lts-development-charter.md) 按 PR 顺序、WBS、Kit、CI 与真实 Viewer 逐项交付。

**V0.4.0.0 继续保持 `conditional_trains.active=false`**；#235 只在 V0.3 家族 GA 必备范围已实现、#602 release authority 可关闭、真实 Qt5/Qt6.8 Windows 产品证据齐备时负责启动 RC。任何将“Qt5/Qt6 正式适配、SDK 安装消费者、QML/Generic/QPA 补实现、Tool 新功能”安排在 V0.4 的开发计划，全部退回 V0.3 家族安排。

**后续产品排期**：V0.3.5.0 宿主配置/静态文字水印已发布，V0.3.5.1 独立维护已发布；V0.3.6.0 为开发与诊断工作台首期，V0.3.7.0 Docker #592 条件性交付；Qt6.5/6.12 依计划 V1.0.1.0 / V1.0.2.0 且严格 V1 GA 后 #605。真实用户场景/环境及现行保护合格后才可发布。

**不改变已发布历史**：V0.3.3.0 是合法发布基线。V0.3.4.0 开发 train 已由版本准入 PR #655 合并到 develop；它使当前开发目标具备机器登记，但不等于 Release 已就绪，后续仍按 #24/#95 通过 PR/CI/Review 和发布闭环。

## 事实源与冲突处理（本地 Owner 必须遵守）

本节是防止错误取用事实源的强制规则。不要将“最新更新时间”误认为“最高权威”。

| 要回答的问题 | 权威事实源 | 不可替代它的内容 |
| --- | --- | --- |
| **哪些用户成果排在何时、计划分配哪个版本？** | 本文件在 `develop` 上的最新已合并版本；本文件是 HyRemote 本仓完整的跨版本成果/版本排序表；View 自己的版本由 View 仓库负责 | Issue #594 的旧正文/评论、#605 的局部 Qt 决策、任何未合并 PR 或本地文件副本都不能形成另一张版本表 |
| **当前 train 是否登记、mandatory children 是什么、RC 能否激活/候选能否选择？** | `.github/release/release-trains.json` 与 release-scope gate；依 #24/#95 的机器准入和发布治理 | 本路线图里的 `PLANNED`、Issue 编号、PR 已合并或版本号本身都不构成激活/发布授权 |
| **单个成果的需求、验收条件和实际进度是什么？** | 该成果的责任 Issue 当前正文/验收清单，加上关联 PR 的最新 HEAD、CI、独立 Review 和测试证据 | Issue 标题、历史评论、关闭状态本身或路线图摘要不能代替具体验收与实际状态 |
| **功能是否已交付、某个 Qt/平台组合是否 Supported？** | 正式 Release/tag、不可变 release SHA、兼容矩阵及对应 exact-SHA 证据 | 规划文档、machine profile 存在、另一个 SHA 的测试通过都不能证明已发布或 Supported |
| **版本号的含义是什么？** | `docs/versioning.md` / `docs/en/versioning.md` | 这两份文档是语义说明/排期摘要，不是独立的完整路线图 |
| **旧文档/旧决定/旧分支是什么地位？** | 仅作为历史记录，除非当前路线图明确引用并说明仍有效 | 旧 `development-roadmap.md`、旧 Owner handoff、旧 Issue 段落/评论、closed 或 unmerged PR branch 不可直接作为当前执行指令 |

### 冲突处理协议

1. 本地 Owner 每次开始工作都从 `develop` 的当前 HEAD 读取本文件和 `.github/release/release-trains.json`，再读取本次责任 Issue；不得从旧分支或记忆中的路线图取版本。
2. `PLANNED` 不等于 machine 登记或开发/发行资格。V0.3.4.0、V0.3.5.0、V0.3.5.1 已发布；V0.3.6.0 的新产品范围须经本次受审路线图与 machine 记录同步；V0.3.7.0 及其他计划维持原条件；V0.4.0.0 仍为 `active:false`，V1.0.0.0 未完成 GA 资格。
3. 若 Issue #594/#605 的新决定与本文件不同，或本文件与 machine manifest 冲突，本地 Owner 不自行选一边、不开始依赖该冲突的 Feature，也不创建 release/tag/Supported 声明。将状态标为 `PLANNING_RECONCILIATION_REQUIRED`，由 Owner 在 #594 记录日期、旧→新版本、原因、依赖/用户/CI/支持范围影响，并提交路线图同步 PR；涉及 machine activation/mandatory 的再单独提交 machine PR。**只有相应审查 PR 合并后，新规划/机器状态才成为当前事实。**
4. 新功能的 Issue 应链接到本路线图并记录其用户成果、验收、依赖和真实证据；Issue 不另行维护全局版本表。路线图每次改动须同步中英文版本摘要与本文件的回归检查。
5. 任何证据缺失都保留 `NOT_RUN` / `UNQUALIFIED`；不能从“有计划”“有 train”“CI 通过其他单元”推断产品支持。

## 产品目标：提供可验证、可维护的 Qt 远程访问 SDK/Runtime

HyRemote 是独立开源 SDK/Runtime。本路线图仅覆盖本仓公开 API、四种并列接入方式、Widgets/Qt Quick 目标适配、RFB 传输、构建与部署、安全边界、性能与资源生命周期、开发诊断和平台兼容性。

### 主要使用者与用户结果

| 使用者 | 主要任务 | 验收证据 |
| --- | --- | --- |
| Qt 应用开发者 | 选择正确 Qt Kit，接入 C++/QML/Generic/QPA，完成构建、安装、部署与连接 | 真实应用构建、安装树消费、RFB 连接和输入结果 |
| 设备与产品制造商 | 在目标 Widgets/Qt Quick 应用中复用同一 Runtime，确认显示/采集与本地交互共存 | 精确 OS/架构/Qt Kit/接入方式的实机资格记录 |
| SDK 集成与维护人员 | 识别 Kit/ABI/部署问题，定位 Runtime 资源与性能故障 | 可复现诊断、脱敏报告、同 SHA CI 与维护记录 |

### 产品交付原则

- 四种接入方式共享一套 Core/Runtime；不得因前端不同重复实现 Session、采集、传输或输入。
- 每个 Feature 必须对应可观察的 SDK/Runtime 用户结果、明确范围、责任 Issue 和真实验收证据。
- 平台支持按精确 Qt 补丁、编译器/ABI、操作系统/架构、接入方式和 UI 目标逐单元声明；可移植或交叉编译不等于 Supported。
- 工程任务归属其支持的产品版本，不单独获得 Feature 号；未批准的研究项不预留发行槽位。

## 零、全量编号版本路线（Owner 2026-10-26，当前产品排程）

> **本节是本文件最优先的「已知用户成果 → 确切版本 → 本地 Owner 任务」分配。** 老 Issue 中的「no exact version」仅保留历史审计作用，遇到冲突以本节为准。以后版本允许调整，但必须由 #594 的新决议与受审 PR 明确写出“原目标→新目标、原因、依赖/里程碑影响和生效 SHA”，不能靠本地负责人猜，也不能默默把功能搬到 V0.4。**规划编号不等于激活 release train / 可发布 tag**；机器 selector 只负责已激活的确切版本。已合并的机读准入 PR #655 已登记 V0.3.4.0；以下更远的编号均为「计划目标，未激活」，不得直接执行发布选择器。
>
> **排程优先级：** 已发缺陷/安全修复可独立维护 → V0.3.4 Qt 主线 → V0.3.5 宿主配置/文字水印 → V0.3.6 Tool S1 → V0.3.7 Docker 条件性交付 → V0.4 RC → V1 GA → GA 后独立 Feature。若条件性产品功能没有真实环境可验收，必须在 RC 冻结前通过审查将其保留为未发行计划或重新安排具体 GA 后版本，不能伪造通过，也不能以它阻塞已承诺的 Windows GA 支持范围。

### A. 版本总表：从已发布版本到 GA

| 版本 | 产品目标与实际用户价值 | Owner / Issues / 完成与验收条件 | 规划状态与门禁 |
| --- | --- | --- | --- |
| **V0.1.0.0 / V0.2.0.0 / V0.2.0.1 / V0.3.0.0** | Developer Preview → 用户 LAN 首试 → 维护修复 → 四前端 SDK 自助 Product Preview | #229/#233/#386/#234；保留既有已发布 tag、证据、接受限制，不进行版本号/功能回填 | **RELEASED / 历史**；0.3.1.0 与 0.3.2.0 已退休且不复活 |
| **V0.3.3.0** | Raw/TRLE + Damage 增量、维护版真实 GUI Viewer 互操作 | #144/#175；Windows Qt6.8.3 L2 85/85；真实 Viewer 等部分证明不在同一 release HEAD，受勘误限制；#370 持续性能责任 | **RELEASED 2026-10-24**；不再开发该 Feature |
| **V0.3.4.0** | **跨 Qt 主版本 SDK 用户交付**：Windows Qt5.15.2 / Qt6.8.3，正确 Kit/ABI、可安装/部署的四条适用前端、真实应用 + Viewer | **#603 产品 Owner；#265 适配；#566 Kit UX；#57 逐单元资格；#209 架构**。A01–D05/E01：真实 Qt5/6 targets、Qt API/Private ingress、source/installed/qmake consumers、negative tests、Viewer、精确 SHA CI；不把 Qt5 Quick/QML/Generic/QPA 未验收称 PASS | **RELEASED 2026-10-29**；已登记并正式发布，不重启已交付 Feature |
| **V0.3.5.0** | **可运维宿主配置 + 服务端静态文字水印**：客户可决定 config/state/secret 路径；标准 RFB Viewer 看得到文字，不污染本地 Qt 窗口 | **#591 → #589**；四接入前端共用 Runtime 路径与远端出帧处理；中文/英文文本、合法配置、Raw/TRLE/Damage/Resize/多 Viewer、路径权限/重启、真实 Viewer，独立性能与安全验收；Logo 不在本版 | **RELEASED 2026-11-24**；#95 发行及回合已完成，现场反馈独立维护 |
| **V0.3.6.0** | **HyRemoteTool SDK 开发与诊断工作台 S1**：本地 SDK/Qt Kit 静态核对、四接入准确最小指导、显式协议/帧验证、基础诊断/脱敏报告、本机语言；存量 RFB 兼容入口不扩展 | **#579 B 产品设计、#581 研发导航及静态核对/指导、#582/#583/#584 存量兼容安全回归、#585 基础诊断、#590 语言、#586 同 SHA 实装研发任务验收**；既有兼容、安全、部署、候选门禁继续 | **已登记 train 的范围修订；旧候选 HOLD**；#587/#588 仍为后续深化计划 |
| **V0.3.7.0** | **用户 Qt 应用与 HyRemote 的真实 Docker 联合部署**：Qt 应用、插件、RFB、挂载 config/state/secret、非 root、重建保持配置 | **#592，依赖 #591/#589**；必须真实 Dockerfile/Compose、Qt App container、RFB Viewer、restart/recreate 与权限测试；没有 Linux/Docker 实验事实只能 NOT_RUN | **PLANNED / CONDITIONAL**；非 Windows 首 GA mandatory；若缺真实环境，RC 冻结前需显式作出保留/移版决定，**不得因其未实现直接进入 V0.4 开发 Docker 功能** |
| **V0.4.0.0** | **Release Candidate 资格验证，不是功能开发** | **#235/#343/#57/#370/#602/#269**；只冻结并证明已在 V0.3 家族实现的 Windows Qt5.15/6.8 真实适用单元和已交付 Tool/Host 功能，真实 Viewer、安装/安全/性能/回滚、CI、P1/P2；不承诺未实测 Docker/Linux | **CONDITIONAL_RC / NOT_ACTIVE**，`active:false` 保持；缺实现即退回相应 V0.3.x，不准由本地 Owner 以 V0.4 做功能开发 |
| **V1.0.0.0** | **Windows 第一版 GA / 长期 SDK 和支持合同** | **#33/#95/#57**；只提升已认证的 RC 支持矩阵；公开 API、二语安装/安全限制、Qt Kit/ABI/维护窗口与制品 SHA；#602 已完成 V1 继承 26 项 machine mandatory 对账，V1 只保留 11 条可关闭 Windows GA 门禁（台账见 [GA 范围对账](v1-ga-scope-reconciliation-2026-10-28.md)） | **GA_PLANNED / NOT_RELEASED**；Qt6.5/6.12、Linux/ARM64 非首版 GA 门禁 |

### B. GA 后具体版本目标（规划号可调整，默认均未激活）

| 计划版本 | 已知独立用户成果 | 责任 / 前置依赖 | 当前规划态 |
| --- | --- | --- | --- |
| **V1.0.1.0** | **Qt 6.5 LTS** Windows 实际适配与完整 exact-Kit 资格，保留现有 SDK 一致性 | #605（分项目）、#57 的认证方法；**只在 V1 GA 后**，先考虑该 Qt 版本实际维护生命周期和用户价值，再批准 exact patch/ABI | **PLANNED / POST_GA** |
| **V1.0.2.0** | **Qt 6.12 LTS** Windows 实际适配与 exact Private ABI/Viewer/SDK 验收 | #605；沿用单 Core/Runtime/Kit adapter 与 V1.0.1 的流程，但 **证据完全独立** | **PLANNED / POST_GA** |
| **V1.0.3.0** | **HyRemoteTool S2**：导入宿主诊断报告、来源区分、脱敏与对照分析 | #587，依赖 V0.3.6.0 基础诊断；不增加远程管理接口 | **PLANNED / POST_GA** |
| **V1.0.4.0** | **HyRemoteTool S3**：深化 SDK/Qt Kit 核对、真实 CMake/安装消费者及四接入集成验证 | #588，依赖 V0.3.4.0 SDK Kit 契约与 V0.3.6.0 S1；复用首期静态核对及指导，不重复建设 | **PLANNED / POST_GA** |
| **V1.0.5.0** | **Logo/图片水印**：与已合格文字水印共用服务器端远端帧输出；尺寸、透明、资产安全处理 | #593，必须 **在 V1 GA 与 V0.3.5 文字水印完成之后**；具体格式/组合方式需 Owner 确认 | **PLANNED / POST_GA** |
| **V1.2.0.0** | **真实工业 Qt Quick/ARM64 低侵入交付**：将现有 HMI 应用低成本启用远控，Quick-only 不额外加载 Qt Widgets，保留本地触控/显示 | **#149/#203/#7/#236/#18/#154**；准确 RK3588/Buildroot/EGLFS 参考 Kit 与性能/依赖正反例；QtPrivate/QPA-EGLFS #148 单元只能依 GO/NO-GO 判定真实支持范围 | **CONDITIONAL / POST_GA**；有硬件和可复现实机证据后才准入，不重启 Windows GA 物理门禁 |
| **V1.2.1.0** | **Linux Wayland 原生平台可用性**：原生 Qt 程序在明确的 Wayland Kit 上看/控/部署，保持桌面输入与正确平台身份 | **#346/#148/#563**；先 Linux x86_64 Qt6.8 真实 Wayland 用户场景，按 C++/QML/Generic/QPA 逐单元验收，XWayland 不能冒充 native | **CONDITIONAL / POST_GA**；真实环境和平台兼容证据门槛 |
| **V1.2.2.0** | **工业 HMI 真实单点/多点触控**：区别于鼠标模拟，触点 ID、按下/移动/释放/取消语义对 Qt Quick 正确 | **#172**；基于 V1.2.0 适用 HMI 参考板，协议/Viewer 触控能力先选型；远端切断释放全部触点，本机触控持续可用 | **CONDITIONAL / POST_GA**；未有真实触控通道只能提供 Mouse Emulation，不能宣称多点触控 |
| **未分配版本的条件候选** | **认证 Session Operations**：共享 Runtime 的认证会话身份、准入、生命周期事件与单会话终止；C++/QML 暴露同一语义，Generic/QPA 复用 Runtime 执行 | **#170**；依赖已合格的认证/授权前置与真实用户需要；不是 Tool 的会话管理 UI | **DISCOVERY_DEPENDENT / NOT_ACTIVATED**；未分配版本，不阻塞当前交付 |
| **未分配版本的条件候选** | **业务应用控制平面**：C++/QML 应用可声明准入、只读/控制、撤权与敏感区域隐私策略；Generic/QPA 不推定所有业务能力都适用 | **#225**；依赖明确用户故事、可用的身份/Session 原语、权限撤销与无一帧泄露负例；不得提前预留版本 | **DISCOVERY_DEPENDENT / NOT_ACTIVATED**；无版本槽位，不阻塞当前交付 |
| **V1.2.4.0** | **安全文件交付 SDK/Runtime GO/NO-GO 评估**：判断跨前端有界文件提供能力是否属于通用 SDK/Runtime 责任，或应采用外部安全维护通道 | **#173**；先确认互操作机制、认证/加密前置、应用授权与文件系统范围、配额/取消/路径安全/回滚；只有 GO 才建立实现范围，NO-GO 则明确外部安全通道替代方案 | **DISCOVERY_DEPENDENT / NOT_ACTIVE**；未作 GO 决定前不实现，不构成当前 V0.3.6.0 或 V1 GA 阻塞 |
| **V1.2.6.0** | **工业设备低负载待机和按需远程捕获**：无人观看时不过度占用 CPU/GPU，有人接入时画面/输入仍有响应 | **#151**，需在实际设备上给出 no-viewer/one-viewer/idle/reconnect 的资源与可用性测量；复用 #370/#372 既有机制，不重新发明 Core Scheduler | **CONDITIONAL / POST_GA**；只有真实设备资源预算不能满足时才独立激活 |
| **V1.2.7.0** | **面向现场运维的高级性能诊断**：解释为什么远端变慢/帧丢弃/输入失败，明确数据来源与不可用项 | **#180**，依赖 S2 基础诊断 #587 / 已有 Core SessionStats 正确性，按受支持 SDK C++/QML 公开有界只读指标 | **CONDITIONAL / POST_GA**；需证明基础诊断不足以定位真实客户故障，否则合并需求/不制造独立发行 |


**关于路线范围的边界：** 本路线图只记录 HyRemote 开源 SDK/Runtime 的用户成果与工程义务。未批准或不属于本仓 SDK/Runtime 的内容不在此处描述，也不预留版本号。#123 GPU/低拷贝、#371/#372 极端弱网仍是待证 SDK 技术研究项，未获激活。

### C. 横跨版本的正确性/安全/技术债归属（持续实施，不占用 Feature 号）

| 任务 | 范围与影响 | 负责 Issue / 验收 |
| --- | --- | --- |
| 已报告现场功能/体验缺陷 | **#565 数字键盘 Keysym 未覆盖**、**#567 大画面更新从上到下刷入**、**#362 目标不在 OS 焦点时无法激活**：按已承诺行为、风险与用户交互语义 triage，当下维护修复或可审查限制；不推迟至 V1.2 平台/触控版本；#567/#565 RK3588/EGLFS 证据不能反推 Windows 必然有同样故障 | #565/#567/#362、#95/#370；正常/异常输入、增量刷新、真实 Viewer 行为 + 受影响 exact-Kit 回归 |
| 现行代码的 RFB/Qt 安全和资源上限 | **已修** Tool #641/#642、CPU frame #643、Alpha #644：保持已合并修复回归；**未结** Host RFB #158 读缓冲、Normal 占槽/RFB3.8 要按实际风险于 RC 前收口 | #158、#393、#611；真实非法输入负例/asan（可用时）/合法流程回归 |
| CPU/帧新鲜度和慢 Viewer 公平性 | 0/1/2/8 Viewer CPU/RSS，W1–W4、Raw/TRLE、负载、帧龄、输入时延；有数据再优化，不默认 GPU | #370/#374/#609/#205；每个敏感 Feature 的跨版本回归基线，N-viewer 成本模型仅在真实产品阻塞时启动 |
| Qt LTS 长期架构护栏 | **22 项 WBS A01–E02** 随 V0.3.4 实施、V0.4 资格治理收口；**F01–F03** 归 GA 后新增/退出 LTS | #209/#265/#57/#603/#602/#605；[22 项 WBS](qt-lts-architecture-wbs-2026-10-24.md)，未实现项目不算 PASS |
| 安全依赖/公开声明/审查门禁 | 真实 LAN 监听、VNC 密码不加密，准确 QtPrivate 依赖、真实 CI Review Gate、单一 machine train 和发行审计；Direct 访问独立于云端，未来 Relay 不进入 Core | #611/#269/#393/#95/#602/#189；只发布真实 Supported / NOT_RUN 状态 |
| SDK 运行诊断/采集加速/极端弱网 | 仅在用户场景或量测支持下做小规模研究；成功试验不自动激活 Feature | #180（计划 V1.2.7）/#123/#371/#372：下一次路线 Owner 复审时做 GO/NO-GO，必要时调整后续版本规划 |
| 官方客户端的 SDK 服务端能力契约 | 按需提供版本化能力描述、可信只读诊断元数据和安全能力协商/拒绝；标准 RFB 兼容保持独立 | #246；仅记录 SDK server-side 设计依赖，须另行授权并提供服务端真实证据，不以客户端 mock 代替 | DESIGN_DEPENDENCY / NOT_ACTIVATED |

### D. 本地 Owner 顺序、依赖和可修订版本的规则

1. **下一开发目标 V0.3.6.0**：新 #579 B 设计实施 #581 研发工作台、SDK/Kit 静态核对与四路线指导、#585/#590 诊断/语言；#582/#583/#584 仅保留兼容与安全回归；#586 新 exact SHA 实装研发任务验收。先完成本次受审范围同步，再开展实施；不重做完整 Viewer。
2. V0.3.5.0 已发布；用户现场 #567/#789/#790 待反馈，维护独立排期；Docker V0.3.7.0 仍依赖真实容器环境，未实测不得宣称交付。
3. **RC 准入检查**：V0.3.4 兼容和所有已承诺 Windows GA 范围内的宿主/Tool 功能必须已在对应 V0.3 Feature 实现；#602 机器 GA 可关闭门禁（对账后 11 条）已收敛、#235 冻结支持矩阵与精确 SHA。**V0.3.7 的未验证 Docker 非 GA 阻塞，应经明示延后/限制决定后进入 RC**；不要因此凭空宣布容器交付。
4. **版本目标变更**：新的 Owner 决定先在 #594 提交“旧版号→新版号、为什么、谁受影响、依赖、机器是否激活、CI/用户影响”，再走独立路线图 PR 同步本表、中英文 `versioning`、对应 Issue/WBS；只有 **激活/发布** 才修改 `release-trains.json`；退休或重排的 *仅规划* 编号保留历史记录，不复用已发布 tag。
5. **用户体验和产品闭环优先**：按上述五类角色/任务收集真实问题；开发从用户旅程的失败点设计可交付结果，分配版本号是为了排程，不是按版本号凑功能。公开版本完成后持续观察接入困难、远程交互成功率、实际画面刷新、控制安全、诊断可理解性和跨 Kit 回归；基线/量化指标由真实测量冻结，不造承诺。自 V1.0 往后继续覆盖嵌入式/Wayland/Touch/业务控制/文件/增强客户端，不在 V1.1 后停止规划。
6. **每个已规划版本至少具备**：一个确切版本号、一个用户可验收结果、Owner Issues、前置依赖、明确不包含项、目标 Windows/Linux/Kit 范围、验收证据、状态。Issue 关闭 != Feature 已发布；FEATURE_PLANNED → ACTIVATED_DEV → IMPLEMENTED → QUALIFIED → RELEASED，候选和 RC 分开。
7. **本地与云端责任**：HyRemote 云端 Owner 维护本仓权威版本表、版本冲突诊断与本仓 release train 分类，Agent 执行代码/CI/PR/review，Human 只负责需真实 Windows/Viewer/设备或 GitHub 人工授权的节点。本地 Owner 只能使用本表的**当前明确开发版本**，不得自建平行 V0.4 功能计划或从“计划号”生成 release tag。

## 一、交付对象与版本定义

**四种对等接入方式：** Embedded C++ API、Declarative QML API、Generic Plugin、Transparent QPA Proxy。它们共享一套 Core/Shared Runtime；Widgets 与 Qt Quick 是 UI 目标，不是新增 Runtime。Qt5.15/Qt6.8 的实际合格单元逐个按 Windows Qt Kit/CPU/编译器/Qt patch、C++/QML/Generic/QPA 路线、Widgets/Quick、安装消费者、RFB Viewer 和 QPA exact Private ABI 判断，不承诺未经实测的所有组合。

**GA 前产品交付主线**：V0.3.4.0、V0.3.5.0、V0.3.5.1 已发布 → V0.3.5.2 维护计划（未激活）→ V0.3.6.0 Tool S1（已登记）→ V0.3.7.0 条件 Docker → V0.4 RC 资格冻结 → V1 GA；GA 后既定 Feature 不变。现场维护根据真实反馈另行分流。

## 二、版本与功能完整分配

| 阶段 | 用户可感知产品成果 | 必交范围与验收 | 关系/明确排除 |
| --- | --- | --- | --- |
| **V0.3.3.0 已合法发布（2026-10-24）** | 已交付的 RFB Raw/TRLE、Damage 与维护版 Windows GUI Viewer 互操作；遵守 [已公布证据限制](../releases/v0.3.3.0-errata.md) | **#144/#175** 实现归档；release code head `d6cb758` 的 Windows L2 **85/85 PASS**；真实 Viewer/认证/性能部分证据来自 `de94a0a`，非全部同 SHA，#370 持续维护 | **不倒灌** Qt LTS 多版本、Tool UI、水印、Docker 或新 mandatory；未附二进制 release assets |
| **V0.3.4.0 — 已发布 Feature（#603，PR #655 已合并登记）** | **主流 Qt 5.15 LTS + Qt 6.8 LTS** 的 Windows SDK/Runtime 实用兼容 | **#603、#57、#265、#566、#604**：Qt5.15.2 Windows C++/Widgets + Tool 已有**有限资格**，剩余 Qt5 Qt Quick/QML/Generic/QPA 按实际可适配+可验收单元决定；Qt6.8.3 作为 Windows 参考及回归。跨 C++/QML/Generic/QPA 的一个 Shared Runtime、SDK 安装、CMake/qmake 消费者、Qt Kit/version+compiler+arch 显式选择、QPA exact Qt Private ABI 及真实 Viewer 验收；未测 Unsupported/NOT_RUN 不冒称 PASS | **Qt 6.5、Qt 6.12 明确不属于本 Feature 或首轮 GA**，跟踪 #605；Tool 整套 S1 完成不阻塞 Qt 5.15/6.8 |
| **V0.3.5.0（RELEASED）** | **宿主配置/持久化路径 + 服务端文字水印** | **#591 + #589**：四条入口共用 config_file、可写 state_dir、未来可选 DB 路径及受保护 secret 引用；可配 UTF-8 中文/英文静态文字、透明度/字号/颜色/位置/平铺；原应用本地像素不变，标准 Viewer 看得见；Raw/TRLE/Damage/Resize/多 Viewer 性能真实验证 | 不提前实现 SQLite 引擎或 TLS/账号平台；Logo #593 在 GA 后 |
| **V0.3.7.0（CONDITIONAL）** | **用户自己的 Qt App 与 HyRemote Docker 联合部署** | **#592**：依赖可运行 #591 配置/数据 API，真实 Dockerfile/Compose + Qt 插件/显示后端、RFB 连通、非 root、只读配置/密钥、可写 volume、restart/recreate 后持久化 | 没跑真实 Qt 应用容器不能宣称 Docker Supported；不因无法取得 Linux 设备阻断已合格的 Windows Features |
| **V0.3.6.0 Tool S1 / V1.0.3.0 S2 / V1.0.4.0 S3** | 开发诊断工作台首期 → 深入宿主诊断 → 深入 SDK 消费验证 | #581/#585/#590 研发任务，#582/#583/#584 兼容维护，#586 实装总体验收；#587/#588 后续深化 | 存量 RFB 安全与可用性不回归；不扩建 Viewer；现有入口仅作兼容维护，退役日期未设定 |
| **V0.4.0.0 RC（未激活）** | 冻结**Qt 5.15 与 6.8** 的真实 Windows 支持矩阵，全面验证已交付产品 | #235/#343/#602：Qt5.15 与 Qt6.8 的适用 Windows exact Kit/Widgets/Quick/四路线/Private ABI、外部 GUI Viewer、性能/安全/SDK 安装/升级回滚、Tool 已发货能力与文字水印/路径等声明，同 SHA 发布闭环 | **不要求 Qt 6.5/6.12 的任何适配、兼容性或真实 Qt Kit PASS**；RC 不开发新 Feature。Docker 如未实测只能标 Unqualified |
| **V1.0.0.0 GA（机器规则已列出；NOT_READY／未发布）** | 仅提升合格 V0.4 RC 的 Windows 产品资格为首个稳定支持承诺 | #33/#95：**正式 Qt 家族只覆盖已实际资格化的 Qt 5.15/Qt 6.8 单元**，C++/QML/Generic/QPA 支持单元清单、API/包、二语说明、tag/checksums/维护声明；Tool 功能按实际验收声明 | 不暗含 Qt 6.5/6.12 支持；不发布未经实际验证的 Docker/Linux cell |
| **V1.0.1.0 Qt6.5 / V1.0.2.0 Qt6.12（PLANNED，POST_GA）** | **Qt 6.5 LTS 及 Qt 6.12 LTS 扩展支持** | **#605**：各自选择确切 patch+compiler Kit，Windows SDK/Widgets/Quick/四路线中可支持组合与 QPA exact ABI，实际安装、连接/输入与性能回归，按真实证据声明 Supported/Limited/Unsupported | V1.0.1.0 与 V1.0.2.0 是已规划的 GA 后独立成果；如需调整必须按版本变更治理记录旧→新、原因与影响。在 V1 GA 前不要求真实运行/消费者/外部 Viewer 支持认证 |

## 二.1、2026-10-24 审查发现：执行 WBS 与独立缺陷闭环（不新增版本编号）

**本节是本路线图的执行索引，不是第二套 Issue / machine release authority，也不把 PLANNED 变成 PASS。** 完整的 **22 项 WBS、每项验收、责任 Issue、PR 切片、Gate G0–G4 与证据格式** 位于 [Qt LTS 适配架构 WBS 执行台账](qt-lts-architecture-wbs-2026-10-24.md)。当前全部新工作包初始为 `PLANNED`；只在实际合并代码和 exact-HEAD 验证后才能转状态。

| 产品阶段 | WBS ID / 执行包 | 责任与验收焦点 |
| --- | --- | --- |
| **V0.3.4.0 · Qt5.15/Qt6.8** | **A01–A05**：真实 Kit/target、统一 eligibility、源码/安装消费者；**B01–B03**：Qt API 差异、GuiPrivate 输入边界、Widgets/Quick；**C01–C04**：四前端按适用 Kit 验证；**D01–D04**：部署/qmake、受影响双 Kit CI、跨 ABI 负例 | [#603](https://github.com/skawu/HyRemote/issues/603) 总交付，[#604](https://github.com/skawu/HyRemote/issues/604) Kit，[#265](https://github.com/skawu/HyRemote/issues/265) 适配，[#209](https://github.com/skawu/HyRemote/issues/209) 架构，[#393](https://github.com/skawu/HyRemote/issues/393) 回归 |
| **V0.3.x 实测 → V0.4 RC 资格** | **D05**：exact SHA/Kit/原生 GUI/Viewer；**E01**：中英文支持/依赖事实 | [#57](https://github.com/skawu/HyRemote/issues/57)/[#603](https://github.com/skawu/HyRemote/issues/603)，未运行物理单元只能 NOT_RUN |
| **V0.4 RC → V1 GA 门禁** | **E02**：26 条继承 machine mandatory 整理、真实 review 门禁、精确发行 SHA | [#602](https://github.com/skawu/HyRemote/issues/602)、[#269](https://github.com/skawu/HyRemote/issues/269)、[#95](https://github.com/skawu/HyRemote/issues/95)；机器更改走独立受审 PR |
| **V1 GA 之后** | **F01–F03**：新 Kit 探针 fail-closed、Qt6.5 与 Qt6.12 分别认证、旧 LTS 退出演练 | [#605](https://github.com/skawu/HyRemote/issues/605)、[#611](https://github.com/skawu/HyRemote/issues/611)；Qt6.5/6.12 已按本路线图规划为 V1.0.1.0 / V1.0.2.0；其余未形成用户成果的研究项仍不预分配版本号 |

**今天发现的具体缺陷不应被大 Qt LTS Feature、Tool S1 或泛化 umbrella 遮蔽：**

| Issue | 处理路径 | 拟关闭条件与准入边界 |
| --- | --- | --- |
| **[#641](https://github.com/skawu/HyRemote/issues/641) P1 Tool 输入错误** | 立即进入现有维护/Hotfix 评估，具体 Maintenance 编号由 #95 准入；不等待整个 S1 | RFB wheel 0x08/0x10、按键/按钮释放先于取消控制、失焦/reconnect 安全状态的 wire 测试及适用双 Kit CI |
| **[#642](https://github.com/skawu/HyRemote/issues/642) P1 Tool 资源安全** | 独立维护/安全正确性修复，不能与 #158 Host 问题相互代替 | 长度、图像尺寸、累计 Raw、读缓冲和 pixel format 的 fake-peer 负例、有限内存与合法重连 |
| **[#643](https://github.com/skawu/HyRemote/issues/643) P2 Frame 溢出** | 优先 GA 前正确性风险判定与受限修复；实际维护时机由风险验证及 #95 决定 | `SIZE_MAX` stride、plane layout、Raw/TRLE/tile 无越界访问，ASan/UBSan（如可用） |
| **[#644](https://github.com/skawu/HyRemote/issues/644) P2 Alpha/RFB 语义** | 先确认透明/复合 UI 是否属于已承诺支持面；若属于则 GA 前修复与验证，否则明示限制并按后续功能准入 | Raw/TRLE opaque 呈现政策、alpha=0/128/255 fixtures、适用真实 Viewer；不能凭静态推断视作已复现 |
| **[#158](https://github.com/skawu/HyRemote/issues/158) Host RFB 内存与协议** | 现有 V1 正确性范围，RC 前按 Host 真实负例验收 | 256 KiB 输入上限的先读后检查、RFB3.8、Normal 阶段占槽/超时与合法静态增量请求 |
| **[#609](https://github.com/skawu/HyRemote/issues/609)/[#370](https://github.com/skawu/HyRemote/issues/370)/[#374](https://github.com/skawu/HyRemote/issues/374) CPU/公平性** | 持续资源/SLO 合同；量测后再局部优化，禁止无数据的独立 codec/GPU 扩张 | 0/1/2/8 Viewer Host/Tool CPU、RSS、帧龄/输入和本地活性；exact SHA/同基线对照 |
| **[#611](https://github.com/skawu/HyRemote/issues/611)/[PR 612](https://github.com/skawu/HyRemote/pull/612)、[#269](https://github.com/skawu/HyRemote/issues/269)/[#602](https://github.com/skawu/HyRemote/issues/602)** | 当前安全事实/依赖与 release-review 真实性，分别使用现有独立 PR/门禁路径 | 保留已发布 V0.3.3.0+勘误的事实；真实 branch/check protection，GA train 不按本规划私改 |

**执行纪律**：缺陷 `TRIAGED → FIXED_IN_PR → ACCEPTED`，WBS `PLANNED → IN_PR → CODE_DONE → QUALIFIED`；以上只是待实施台账，独立 Reviewer 要求最新 PR HEAD 的真实 Windows CI、受影响 Qt Kit 正反例、证据清单和适用物理验收。Qt6.5/6.12 仅 GA 后准入，Linux/ARM64 实机验证不作为 Windows GA 阻塞；机器 mandatory #602 单独修订，不在路线文档中暗加。

## 三、Qt 家族资格矩阵：首轮强制 vs 后续

| Qt 家族 | V1 GA 目标范围 | 目前真实证据与约束 | 版本管理 |
| --- | --- | --- | --- |
| **Qt 5.15 LTS** | **必须在 V0.4 RC 前按适用声明真实合格** | Qt5.15.2 Windows C++/Widgets + Tool 有有限证据（#569/#576）；其余 route/Quick/private QPA 要逐 cell 证明，不因 Qt 大版本符合就全盘通过 | 下一个 V0.3.x 客户成果的重点 |
| **Qt 6.8 LTS** | **必须在 V0.4 RC 前按适用声明真实合格** | Qt6.8.3 Windows 参考路径，必须保持 SDK/Viewer/Raw/TRLE、各适用 route 回归与 exact Qt Private ABI | 当前参考/下一个成果回归 |
| **Qt 6.5 LTS** | **POST_GA_ONLY** | 以前 preflight/Issue 声称“必需”已被 Owner 更新的版本范围取代；不能由历史 preflight 记录推断已验证 | #605，GA 后独立资格 |
| **Qt 6.12 LTS** | **POST_GA_ONLY** | 可以记录探索性兼容性调查，但不加入首轮 GA 必需支持矩阵，也不要求当前 Windows 真实 Kit | #605，GA 后独立资格 |

**特别强调 build / Kit UX（#566/#604）：** 已合并的 PR 568 在 Qt Kit 自动发现中有过宽的 `Qt6>=6.8` 条件，错误地屏蔽已经部分资格化的 Qt5.15；下一个 Feature 必须使 **实际 Qt5 可构建的 Kit/组件**被准确提供、选择和验证。**不要求首轮让 Qt6.5 成为可选合格 Kit**；Qt6.5/6.12 检出时可注明「未来计划/尚未资格化」，不可误标 Supported。必须同时保持 Qt6.8.3 和 QPA exact ABI 的严格路径约束。Kit 发现仅表示 *candidate*，不是发布支持证据。

## 四、GA 前 SDK/Runtime 功能与依赖

- **RFB 核心不可回退**：#144/#175/#370 的 Raw/TRLE/Damage、输入键鼠、重连/光标、维护版 Windows GUI Viewer、慢客户端与性能、安全状态、Qt 本地/远程共存 #109。
- **四种 SDK 接入方式共用 Shared Runtime**；Windows GA 以 Qt5.15 和 Qt6.8 真正可支持的路由单元验收，QPA exact patch + private ABI，不把 Qt5 实现难度虚构为全通过。
- **配置/水印为 GA 前独立产品目标**：#591 + #589，首批**静态文字水印**；宿主与 Tool 本机设置分离；数据库仅预留可配置路径、不因为未来认证而强制引入数据库或明文密码。
- **Docker #592**：产品要有真实用户 Qt 应用容器示范；是否可以在首轮 GA 对外标 Docker Supported 由其真实容器证据决定。缺验证时不能偷给 PASS，也不阻断其它已完成 Windows 能力。
- **HyRemoteTool 持续演进**：定位为 SDK 开发与诊断工作台，优先开发环境、Kit 核对、四种接入指导和只读协议/帧诊断；已发布访问入口仅保留兼容，不扩展其访问功能。#586 新研发任务总体验收不是 Qt5/Qt6.8 SDK 资格的前置。
- **机器规则的第一步已经通过 [PR 606](https://github.com/skawu/HyRemote/pull/606) 合并到 develop**：`.github/release/release-trains.json` 的 #57/V1 规则现仅要求 Windows Qt5.15/Qt6.8，明确 Qt6.5/Qt6.12 为 GA 后，且 active V0.3.3 mandatory [144,175] 不变。此项是已完成的**即时 Qt 家族范围修正**，不是后续才由 #602 接手的工作。
- **RC/GA 其余机器范围整理仍由 #602 负责**：盘点历史 V1 machine 26 条 mandatory/退休 umbrella、拆成可关闭的 Windows GA 验收，再单独评审；不能拿本路线图代替机器范围权威，#57/#343/#33/#235 的 Owner 最新 Qt 家族限定已经生效。

## 五、跨版本长期责任与尚未单独定版的候选（持续维护）

**长期工作不等于“下一版本新功能”，更不能挤占当前 Windows Qt5.15/6.8 首轮 GA 已核准的发布范围。** 本章是长期技术债、稳定支持、用户反馈以及尚未形成独立交付承诺的候选索引，**不是第二套机器 mandatory_children**。已明确编号的产品成果只以本文件“全量编号版本路线”中的目标为准；其他候选在完成用户价值/验收定义前不分配版本。实际发布资格仍由 #24/#95、machine manifest 和证据决定。

| 类别 | 跨版本长期责任或未来用户成果 | 事实源 / 准入原则 |
| --- | --- | --- |
| **CPU / 资源效率与长期性能** | 主动构建真实 Qt Host 与 Viewer 分离的 CPU/RSS/功耗（有证据时）及 W1–W4 交互/Frame Age 基线，修正性能计时、解决测得的采集/编码/重复拷贝热点；后续每次性能敏感变更防回退 | **#370/#374/#609/#9**：CPU 是已观察问题，**现在主动测量与可移植优化**，不是等待客户投诉；GPU/低拷贝仍需 #123 有证据 GO |
| **弱网退化与长时间可靠性** | 保持 Raw/压缩、准确累积 damage、latest-frame/backpressure、慢 viewer 隔离、本地 Qt 活性、超时/重连输入清理；执行有界运行、关闭重启和异常路径回归 | **#370/#371/#372/#144/#158/#164/#109**：**基础正确性现在维护**；极端弱网 FPS/SLA/特殊 codec 按用户反馈专项，不设置新的首轮 GA 弱网门槛 |
| **安全报告、供应链与支持生命周期** | 实际版本/支持单元声明，公开安全边界的真实性，私密漏洞报告与补丁/披露流程、Qt private ABI/依赖/许可证及安装包清单维护；视具体需要决定是否加 SBOM/溯源制品 | **#611（新增持久责任设置）**，现有 **#12/#143/#158/#95/#101/#107/#176**。安全缺陷必须及时修复；任何新增传输安全能力须有明确威胁模型和资格证据；现有 `SECURITY.md`/依赖文档的陈旧版本文字纳入 #611 修正 |
| **1.x API/Kit/升级维护** | 公共 C++/QML/CMake/Generic 稳定性、Windows Qt 补丁/编译器及 QPA 精确 private ABI，独立安装消费者、版本升级/回滚和 release/hotfix 证据 | **#101/#57/#603/#604/#209/#176/#95**；Qt6.5/Qt6.12 **严格 GA 后 #605**；每个 qualified cell 才可声明 Supported |
| **用户问题闭环与可观测性** | 错误分类、脱敏诊断、可复现步骤、精确 SHA/Qt Kit/Viewer/OS 归档、历史性能对比；避免默认收集用户屏幕/输入/秘密或打造第二套远程遥测平台 | 当前基础 **#335/#585**，进阶 **#180/#587**；真实运维阻塞或可复现缺陷驱动最小能力，持续回归 **#374** |
| **SDK 生态与真实应用覆盖** | 四条对等接入路线、文档示例、复杂第三方 Qt 应用 Widgets/Quick/弹窗/输入、clean-rootfs/安装包及真实容器的事实性兼容声明 | **#41/#134/#343/#176/#592**；Windows GA 对既定矩阵负责；Linux/Wayland/ARM64/EGLFS **#236/#346** 用户验证，不隐含预先 Supported |
| **工程可持续性** | Git Flow、可重复 CI/真实 L2、独立评审、性能回归、依赖更新、旧 Issue/历史 docs 清理、适用时的 ADS 通用化而不改变产品架构 | **#95/#209/#374/#393/#602/#14**；治理工作贯穿各版本，不按技术工作项机械占用 Feature 号 |

**长期候选激活判断：** 有明确的用户场景或可复现的产品缺陷 → 确定当前 SDK/Runtime 能力是否足够 → 精确设备、Qt Kit、Viewer、SHA、网络/性能/安全证据 → 独立评估用户价值、互操作、安全与维护成本 → #24/#95 决定 GO/NO-GO 和准确 Feature 号；没有足够证据则保留条件候选而非变成 GA 阻塞。**协议正确性、内存/CPU 无界及安全缺陷是当前已有质量责任，不得借此规则放到用户投诉后再修复。**

## 六、执行次序、审查与风险约束

1. **V0.3.3.0 正式重发已完成；持续维护证据真实性**。2026-10-24 合法 Release/tag 已存在，Windows release-head Qt6.8.3 L2 85/85；不同 SHA 的 Viewer/认证/性能限制按 [勘误](../releases/v0.3.3.0-errata.md) 保留，不能谎称全部同 SHA，也不得重写历史标签。#370/#374 继续负责性能趋势和随后的可复现回归；当前优先处理已发现的产品正确性问题和**已明确编号的 V0.3.4.0 Qt LTS Feature**。
2. **V0.3.4.0 已发布**：维持已验证 Qt5.15/Qt6.8 适用单元，不扩展未验证支持范围。
3. **V0.3.5.0 已发布**；用户现场反馈作为跨版本维护输入。V0.3.7.0 条件计划仍要求 #592 真实容器证据。
4. **V0.3.6.0 Tool S1**：#581/#585/#590 开发诊断工作台，#582/#583/#584 兼容安全回归，#586 实装验收；**V1.0.3.0 S2 #587、V1.0.4.0 S3 #588** 深化诊断和实际 SDK 消费者验证，保留 GA 后计划。
5. **V0.4 RC → V1 GA**：只对 frozen Windows **Qt5.15 / Qt6.8** 和已实际完成的其它产品能力资格化，不开发新功能；真实包/发行说明/标签/回滚/支持矩阵。
6. **V1.0 GA 完成后**：按规划 V1.0.1.0 Qt6.5、V1.0.2.0 Qt6.12（#605）顺序独立准入；后续仅保留本仓 SDK/Runtime 已列明的平台、图像水印、输入、资源与运行诊断目标，并逐项等待 Owner 激活和精确资格证据。

所有代码 PR 必须在 Ready PR 最新 head 的适用 Windows CI、Git Flow 和独立 P1/P2 Review **PASS 后**才能合并。没有物理 Windows/Container 实证的支持 claim 标记 NOT_RUN/UNQUALIFIED；无 Linux/ARM64 硬阻塞 #563。发布版本只能按 #24/#95 的精确 SHA、release/v、L2、tag/checksum、backmerge 过程完成。

**关联**：#1 #24 #33 #57 #95 #144 #175 #235 #260 #265 #343 #370 #563 #566 #579 #581–#594 #602 #603 #604 #605。此文件是产品规划与未来准入依据，不是功能已交付、GA 发布或新的精确 Feature 激活记录。
