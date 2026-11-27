---
name: 工程健康里程碑进度（指向权威，不缓存台账）
description: 工程健康/发布进度的唯一权威入口与三层教训；逐日 PR/SHA 明细已归档，禁止从记忆推断当前状态
type: project
---

**不要从本文件推断当前开发状态。** 权威顺序：root `AGENTS.md` 的产品阶段快照 →
[单条现行路线图](docs/internal/product-version-delivery-roadmap-2026-10-23.md) / #594 →
`.github/release/release-trains.json` → #95 与最新 Issue 权威评论 → exact-SHA 证据。
逐日 PR/SHA 明细保存在本地 `archive/engineering-health-chronicle.md`（不入 git，供溯源），
需要时按 `git log` / `gh issue view` 核对，不引用记忆里的历史数字。

**Why:** 2026-11-10 发现本文件曾膨胀到 30 KB / 72 行，含 29 处逐日追加、132 个 Issue 引用、
96 个 SHA，开头仍写「当前开发目标 = V0.3.4.0」，而 `AGENTS.md` 已记载 V0.3.4.0 为最新已发布、
V0.3.5.0 RC2 冻结未发布。把 git/PR 台账塞进耐久记忆会同时造成：读得慢、重点被稀释、
以及最危险的——用过期版本事实指导新任务。

**How to apply:** 开工只读 `AGENTS.md` 的产品阶段段和当前 Issue 最新评论；里程碑数字一律查
git/GitHub。新增长期记忆时若发现自己在粘贴 PR 号、CI run id 或 SHA 清单，立即停手：那是
证据或 chronicle，应落在 `tmp/` 或归档，不是记忆。

三条已定案、不会随时间失效的教训（从归档蒸馏，保留原因）：

- **G6 假红根因（#800，已由 PR #801 修复）**：`build.cmd test` 会把构建树里所有
  `*HyRemoteRemoteAccess.dll` 目录前插测试 `PATH`，`build/install/plugins/platforms` 缺
  `qoffscreen` 导致 offscreen 用例在 `QGuiApplication` 构造时崩溃。与登录会话无关。
  **不要再**用「重新登录桌面」「先测后装」「移出 install」「加 `QT_PLUGIN_PATH`」解释或绕行。
- **账目型文档必须逐行从 git 求证**：凭记忆写合并 SHA 曾经写反（#727/#728），教训是
  `git log --all --grep='#<n>)'` 全表复核，而不是复述上一轮对话。
- **Human 协助时优先复用其现有工具**：曾为「减少操作者负担」自建 noVNC+websockify 桥接，
  反而引入第二个端口与失败模式；操作者本来就有 RealVNC。判断系统档位是否可用，先读原生
  UI 的选项集合，不要反复试 API 猜（175% DPI 封顶即由此定案）。

研发效率债务仍是最高优先级流程问题，见 `feedback_engineering_health.md`；canonical 验证
与测试治理见 `.codebuddy/rules/`。
