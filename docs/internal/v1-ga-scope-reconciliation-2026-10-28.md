# V1.0.0.0 GA 机器范围对账（#602 / WBS E02）

Status: **审核中的执行台账 + 机器变更提案**。本文不是第二份机器权威，也不改变任何发布资格。
机器登记仍在 `.github/release/release-trains.json`；产品排序仍在
[唯一产品路线图](product-version-delivery-roadmap-2026-10-23.md)。本文只回答一个问题：
`trains["1.0.0.0"].mandatory_children` 里继承的历史 26 条，每一条在今天的 Windows GA 合同下是什么。

- Owner 责任：#602（范围对账），#33（GA 合同）、#95（发布放行）、#57（Windows Qt 资格）。
- 基线：`develop@eb20194`（2026-10-28）。
- 机器变更**单独一个 PR**，必须同步更新
  `tests/release-readiness/check_release_authority_policy.cmake` 的固定期望，并跑 exact-head
  release-readiness 与 Windows 产品 CI。本文合并**不等于**机器已改。

## 1. 分类词表（#602 规定，不得混用）

| 标签 | 含义 |
| --- | --- |
| `KEEP_CLOSEABLE` | 仍是真实的 Windows GA 可关闭门禁，保留在 `mandatory_children`。 |
| `HISTORICAL_COMPLETE` | Issue 已关闭且有已接受证据；保留审计价值，不再作为未来工作，不因"还会再开"留在门禁里。 |
| `REPLACE` | 已被新的权威取代（本文写明由谁取代）。 |
| `SPLIT` | umbrella 无法整体关闭，拆成已存在的可关闭门禁后退出 mandatory。 |
| `DEFER_NONBLOCKING` | 属于 GA 之后或非支持声明的能力，不是首轮 Windows GA 阻塞项，写明归属版本。 |
| `OWNER_RISK_ACCEPTED` | 明确由 Owner 承担的风险；本次对账**未使用**这一类。 |

## 2. 26 条逐条对账

状态取自 2026-10-28 的 GitHub 实际状态；分类只看"它今天是否是 Windows GA 的可关闭门禁"，
不按 Issue 标题里的版本前缀。

| Issue | 标题（截断） | 状态 | 分类 | 依据 / 可关闭条件 |
| --- | --- | --- | --- | --- |
| #9 | PERF: qualify production interaction latency/resources | open | KEEP_CLOSEABLE | 需要 exact-SHA 的延迟/资源证据；低层后端只在测出阻塞后才准入 |
| #30 | CPP-ACCEPTANCE: Windows x86_64 | open | KEEP_CLOSEABLE | C++ 路线 Windows 资格逐单元证据 |
| #31 | QML-ACCEPTANCE: Windows x86_64 | open | KEEP_CLOSEABLE | QML 路线按 Qt 6.8 单元；Qt5 QML 依 D2 为 Unsupported，不参与 |
| #32 | QPA-ACCEPTANCE: Windows/qwindows exact ABI | open | KEEP_CLOSEABLE | 仅精确 Qt 6.8.3 私有 ABI |
| #33 | GA: freeze the qualified Qt-native baseline | open | KEEP_CLOSEABLE | GA 合同本身，同时是 authority_parent |
| #39 | SDK: freeze consumer/public facade contract | closed | HISTORICAL_COMPLETE | 已随 V0.0.1.0 交付并被后续 SDK 消费者持续回归 |
| #41 | EXAMPLES: task-oriented learning system | open | DEFER_NONBLOCKING | 学习/文档广度不是支持声明门禁；示例已按版本增量交付，不阻塞 GA |
| #57 | QT-COMPAT: Windows Qt 5.15 / Qt 6.8 exact cells | open | KEEP_CLOSEABLE | 首轮 GA 唯一的 Qt 家族资格门禁，必须保留 |
| #101 | API-FREEZE: public C++/QML/CMake surface | open | KEEP_CLOSEABLE | 1.x 兼容契约需在 GA 前冻结 |
| #104 | GA-AUTOMATION: four-route Windows matrix | closed | HISTORICAL_COMPLETE | 四路线 Windows 矩阵已建立，当前由每条 PR 的 L1 继续执行 |
| #107 | RELEASE-READINESS: notes/manifest/exact metadata | open | KEEP_CLOSEABLE | 精确候选的发行元数据与二语说明 |
| #109 | PHYSICAL-ACCEPTANCE: Windows local/remote coexistence | open | KEEP_CLOSEABLE | Windows x86_64 实机共存证据（#563） |
| #143 | SECURITY: truthful optional access control | closed | HISTORICAL_COMPLETE | 能力与文档真实性已交付；后续安全责任在 #611/#158 |
| #144 | RFB-DELIVERY: compressed + incremental with Raw fallback | closed | HISTORICAL_COMPLETE | B1/B2 x86 范围已交付（V0.3.3.0）；嵌入式 B3/B4 属独立带 |
| #157 | SUPERSEDED: old x86-first V1 scope ruling | closed | REPLACE | 由 #594（版本决策台账）+ 本路线图取代；机器 `scope_authority` 提案改为 #594 |
| #158 | RFB: exact 3.8 and bounded Normal-phase occupancy | open | KEEP_CLOSEABLE | 协议/资源正确性门禁；代码已实现（#694/#696），等 acceptance 关单 |
| #159 | CORE: start/stop lifetime race | closed | HISTORICAL_COMPLETE | 已修复并有回归 |
| #162 | INPUT: Widgets/Quick routing, DPR, IME | closed | HISTORICAL_COMPLETE | 已修复并有回归 |
| #163 | QPA: exact runtime Qt identity / delegate failure | closed | HISTORICAL_COMPLETE | 已修复并有回归 |
| #164 | RUNTIME: callback exceptions / lifecycle hazards | closed | HISTORICAL_COMPLETE | 已修复并有回归 |
| #165 | RELEASE: freeze exact GA candidate | open | KEEP_CLOSEABLE | 冻结与冻结后证据影响控制，同时是 candidate_freeze_authority |
| #170 | SESSION-OPERATIONS: authenticated session registry | open | DEFER_NONBLOCKING | 路线图已排到 V1.1.1.0（STRATEGIC_CONDITIONAL），不是首轮 GA 阻塞项 |
| #174 | NETWORK: IPv4 listener modes | closed | HISTORICAL_COMPLETE | 已交付并有监听矩阵回归 |
| #175 | VIEWER-INTEROP: maintained viewers | closed | HISTORICAL_COMPLETE | V0.3.3.0 已交付；V0.3.4/V0.4 的 RealVNC 7.15.1 由 #603/#57 承担（D7） |
| #176 | SDK-RELEASE: clean SDK/deployment umbrella | open | SPLIT | 拆入 #107（发行元数据/清单）与 #57/#101（安装消费者/公共面）；umbrella 本身不可关闭 |
| #209 | GA-FOUNDATION: one Runtime + converged foundations | open | SPLIT | 拆入 #101（公共面冻结）与 #107（发行基础）；架构护栏由 ADR/CI 持续执行，不占 GA mandatory |

## 3. 机器变更提案（独立 PR 才生效）

`trains["1.0.0.0"]`：

| 字段 | 现在 | 提案 | 理由 |
| --- | --- | --- | --- |
| `mandatory_children` | 26 条 | **11 条**：9, 30, 31, 32, 33, 57, 101, 107, 109, 158, 165 | 只保留可关闭、且属于首轮 Windows GA 的门禁 |
| `scope_authority` | 157（已 closed/superseded） | **594** | 157 自己声明被 progressive release scopes 取代 |
| `candidate_freeze_authority` | 165 | 165（不变） | 仍是冻结权威 |
| `authority_parent` | 33 | 33（不变） | GA 合同本身 |
| `notes` | 保留并补 | 对每个新 mandatory 写一句可关闭条件；被移除的 15 条写去留 | 让机器文本能自解释 |
| `rule` | 26-child legacy 描述 | 改为"仅保留可关闭 Windows GA 门禁；#170 属 V1.1.1，#605 的 Qt6.5/6.12 永不在 V1 前" | 与路线图一致 |

**退出 mandatory 的 15 条**：39, 41, 104, 143, 144, 157, 159, 162, 163, 164, 170, 174, 175, 176, 209
（其中 10 条 `HISTORICAL_COMPLETE`、1 条 `REPLACE`、2 条 `SPLIT`、2 条 `DEFER_NONBLOCKING`）。

**不改变**：V0.3.3.0 已发布事实、V0.3.4.0 当前开发目标与 machine train、V0.4 `active:false`、
Qt 6.5/6.12 的 post-GA 归属（#605）、Linux/ARM64/Wayland 非阻塞（#563）。

## 4. 生效前必须满足

1. 独立机器 PR 更新 JSON 与 `check_release_authority_policy.cmake` 的固定期望，并新增负例：
   移除项不得回归、#57 必须保留、#605 的 Qt 家族不得进入 V1 mandatory。
2. 同一 PR 的最新 HEAD 跑 release-readiness policy 与 Windows 产品 CI。
3. 文档合并本身**不**让 GA 变成 READY：`V0.4.0.0` 仍 `active:false`，V1 仍 `NOT_READY`。
4. #33/#95 的正文引用在机器 PR 合并后同步（不在本 PR 改 Issue 正文）。
