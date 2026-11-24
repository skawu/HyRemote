---
name: CodeBuddy 项目级 Skill 发现入口实测
description: CodeBuddy CN 4.12.2 在本机只发现 .codebuddy/skills 下的项目技能，未发现 .agents/skills；Spec Kit 输出目录因此改为 .codebuddy/skills
type: project
---

2026-10-27 在 `F:\workspace\hyremote\HyRemote`、CodeBuddy CN 4.12.2（FileVersion 1.106.1.0）实测：

- 项目级 Skill 放在 `.agents/skills/<name>/SKILL.md` 时，客户端两次调用均返回 `Skill "<name>" not found`（技能文件确实在盘上、正文合法、无同名副本）。
- 同一份字节放到 `.codebuddy/skills/<name>/SKILL.md` 后，**无需窗口重载**即在同会话被动态发现并加载，返回 `Base directory for this skill: <repo>/.codebuddy/skills/<name>`。
- 因此结论只到"当前客户端配置下存在路径差异"：发现入口是 `.codebuddy/skills`，不是 `.agents/skills`。

**Why:** 仓库提交 547e2cd 曾按 spec-kit `--commands-dir .agents/skills --skills` 布局，导致 Skill 在仓库里存在但对客户端不可见，形成"有文件无能力"的假象。
**How to apply:** 升级/重装 Spec Kit 时用 `specify integration upgrade generic --script ps --integration-options "--commands-dir .codebuddy/skills --skills"`；`.codebuddy/skills` 需纳入 git 跟踪，`.codebuddy/memory` 等本地状态继续忽略。客户端 `$ARGUMENTS` 不做参数替换，调用时内容原样保留。
