---
name: 工作空间边界：内部全允许，外部仅只读
description: 红线：唯一工作空间是 F:\workspace\hyremote\HyRemote；内部任意子目录无需询问；外部只读允许，任何写/改/删/建一律拒绝（由 PreToolUse hook 强制）
type: feedback
---

唯一工作空间是 `F:\workspace\hyremote\HyRemote`。

- **内部**（含所有子目录：`build/`、`tmp/`、`.codebuddy/` 等）：所有操作合法，不需要逐次询问。
- **外部**：只读允许（`Get-Content`、`Get-ChildItem`、`Test-Path`、`Select-String`、`git` 查询等）；**写入、修改、删除、创建目录一律拒绝**。
- 例外只有一种：用户对某个**具体外部路径**明确授权。即使授权，也需用户临时停用 hook 才能执行，因为客户端层无法按次放行。

**Why:** 用户 2026-11-11 明确要求「在当前工作空间内的所有操作都合法，包括子目录，不需要询问；所有对当前空间目录以外的所有除只读操作之外的全部拒绝」。此前那条「外部一律不得访问（含读取）」已被这次决定取代——只读外部现在是允许的。越界写入曾真实发生（旧的 `F:\workspace\hyremote\build`）。

**How to apply:** 所有构建、测试、安装、证据与临时产物都落在仓库内唯一 `build` 目录或 `tmp/`。命令先 `cd` 到仓库根。需要读外部文件时直接读；需要**写**外部时先停下来说明原因并等用户决定，不要尝试变通绕过——`tools/local-owner/guard_workspace_boundary.py` 会在工具执行前直接拒绝，绕过尝试同样会被记录为违规。
