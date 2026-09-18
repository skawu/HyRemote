# HyRemote 文档

> 语言 / Language：**中文** ｜ [English](en/README.md)

HyRemote 面向应用的模型刻意保持很小：**一个共享 C++ 库**，加上两个可选集成载荷。本文档索引按"读者意图"组织，
而不是按开发历史组织。

## 1. 从这里开始

| 我要做什么 | 读这篇 |
| --- | --- |
| 安装、构建、把 HyRemote 接进应用 | [`guide/install.md`](guide/install.md) ｜ [English](en/guide/install.md) |
| 用 Embedded C++ API 集成 | [`getting-started/cpp.md`](getting-started/cpp.md) |
| 用 Declarative QML 集成 | [`getting-started/qml.md`](getting-started/qml.md) |
| 不改应用源码，走 Transparent QPA | [`getting-started/qpa-proxy.md`](getting-started/qpa-proxy.md) |
| 部署与打包 | [`deployment.md`](deployment.md) |
| 运行与查看器 | [`viewer-connection.md`](viewer-connection.md) |
| 出问题时的诊断 | [`troubleshooting.md`](troubleshooting.md) |

## 2. 三区结构

| 区 | 面向 | 语言 | 内容要求 |
| --- | --- | --- | --- |
| `docs/guide/**` | 最终用户 | 中文为主 + `docs/en/guide/**` 英文镜像（**强制**） | 只讲最终形态：怎么装、怎么接、怎么部署、怎么排错。**不含**过程内容 |
| `docs/reference/**` | 产品最终态契约 | **英文为准**；`docs/en/reference/**` 中文镜像**可选** | 架构、采集/输入模型、API 稳定性、兼容矩阵、安全边界、版本策略 |
| `docs/acceptance/**` | 验收/评审证据 | 英文 | 每个候选一个目录的证据记录；**是文档，不是测试代码** |
| `docs/` 其余 + `adr/` `releases/` `proposals/` | 维护者/发布 | 英文（发布门禁冻结） | 验收 runbook、仓库管理、布局权威、里程碑记录 |

**"过程文档"的定义**（用户文档中禁止出现）：issue 编号与追踪、验收排期与状态看板、里程碑编年史、调查/实验过程记录、
一次性检查清单。这类内容属于内部区，或属于 `research/` 的研究证据。

## 3. 参考（产品最终态）

| 文档 | 内容 |
| --- | --- |
| [`architecture.md`](reference/architecture.md) | 分层模型、依赖规则、线程原则 |
| [`v1-api-stability.md`](reference/v1-api-stability.md) | V1 公开 API 稳定性契约（稳定面与非稳定面） |
| [`compatibility.md`](reference/compatibility.md) | 精确证据/状态矩阵 |
| [`known-limitations.md`](reference/known-limitations.md) | 明确的 V1 限制 |
| [`security.md`](reference/security.md) | 已实现的安全边界 |
| [`security-model.md`](reference/security-model.md) | 威胁模型与发布安全门槛 |
| [`versioning.md`](reference/versioning.md) | 版本与里程碑策略 |
| [`release-package-manifest.md`](reference/release-package-manifest.md) | V1 发布包清单（安装载荷契约） |
| [`dependency-policy.md`](reference/dependency-policy.md) | 依赖政策（含可选的构建期依赖） |
| [`widgets-capture.md`](reference/widgets-capture.md) ｜ [`quick-capture.md`](reference/quick-capture.md) ｜ [`input-model.md`](reference/input-model.md) | 采集与输入模型 |

## 4. 内部 / 发布文档（非用户文档）

[`repository-layout.md`](repository-layout.md)（仓库布局权威）、[`branch-lifecycle.md`](branch-lifecycle.md)、
[`git-flow-release.md`](git-flow-release.md)、[`v1-ga-acceptance.md`](v1-ga-acceptance.md)、
[`v1-physical-acceptance.md`](v1-physical-acceptance.md)、[`v1-repository-admin.md`](v1-repository-admin.md)、
[`release-candidate-checklist.md`](release-candidate-checklist.md)、[`adr/`](adr/)、[`releases/`](releases/)、
[`development-roadmap.md`](development-roadmap.md)、研究/评估记录（`capture-spike.md`、`async-capture-spike.md`、
`neatvnc-evaluation.md`、`x86-vnc-transport-evaluation.md`、`qpa-*-qt-6.8.3.md`、[`proposals/`](proposals/)）。

普通用户集成 HyRemote **不需要**读这一区。

## 5. 导航根与编写约定

本文件是**双语文档导航根**，英文镜像为 [`en/README.md`](en/README.md)。双语镜像区是 `docs/guide/**` 与
`docs/reference/**`；其余文档属于内部/发布区，保持英文。

- **双语镜像**：中文为主文档位于 `docs/<路径>`，英文镜像位于 `docs/en/<路径>`。**进入 `guide/` 或 `reference/`
  的文件**必须一一对应（同一相对路径），每篇顶部一行语言切换，且两种语言在同一次改动里同步；尚未迁入这两个区的
  既有文档在迁移完成前不要求镜像。
- **一对一份额**：用户文档按"读者意图"合并，不按开发阶段拆分；宁可一篇详尽，不要五篇各说一半。
- **源码注释**：沿用各文件既有风格；新写或修改的注释以清晰、与相邻代码一致为先，中英双语在确实有助维护时允许使用，
  但**不是强制要求**，也不做批量注释迁移。
- **链接**：文档移动时同步更新全仓引用；只有发布门禁仍列出旧路径时，才保留**不承载内容**的迁移指针，并在门禁改为
  跟踪新路径后于同一次改动中删除。
- **过程内容**：issue 编号与跟踪、验收排期与状态、里程碑编年史、调查过程、一次性清单不进入用户/参考区。

完整规则见 [`CONTRIBUTING.md`](../CONTRIBUTING.md)。
