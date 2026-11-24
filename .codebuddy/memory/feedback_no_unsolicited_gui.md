---
name: 不要擅自启动 GUI 窗口或反复弹窗
description: 用户明确反感未经请求的桌面 GUI 启动/反复弹出；所有可见窗口操作必须先征得同意，优先离屏或无头验证
type: feedback
---

**规则：不要在用户的桌面上擅自启动可见 GUI 应用，更不要反复重启同一窗口。**

**Why:** 2026-10-19 我为“实机验收”连续启动/重启安装版 HyRemoteTool（含 WMI 脱离会话重启），用户看到的是“频繁弹出 HyRemoteTool”，并明确表达反感。这些弹窗对用户没有任何价值，只是打断工作；而且当时 Tool 的设计本身还未被接受，验收前提不成立。

**How to apply:** 需要 GUI 验证时，先问用户是否现在可以打开窗口、需要他做什么；优先用 `QT_QPA_PLATFORM=offscreen`、`--test-seconds`、CLI 诊断导出或 CI 证据完成可自动化的部分。绝不使用 SetForegroundWindow/AttachThreadInput 等抢焦点手段；一次只保留一个实例；用完主动关闭并告知。用户没有明确说“打开/运行”时，只做只读检查与代码/文档工作。
