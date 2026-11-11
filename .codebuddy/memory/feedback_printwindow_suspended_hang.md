---
name: 禁止对挂起进程调用 PrintWindow
description: slow-viewer 卡死根因：NtSuspendProcess 后 PrintWindow 会无界阻塞；必须 try/finally 恢复并设置超时
type: feedback
---

禁止对已挂起（`NtSuspendProcess`）的进程调用 `PrintWindow` 或任何需要其 UI 线程响应的捕获 API；捕获只能在挂起前或恢复后进行。

**Why:** 2026-10-23 的 slow-viewer 测试中，脚本先挂起慢 Viewer，随后 `after-final` 阶段调用 `Pixel(slow)` -> `PrintWindow(slowWindow)`。窗口线程已冻结，`PrintWindow` 无限阻塞，导致阶段 3 卡死；这不是 `Start-Sleep` 的问题。

**How to apply:** 捕获函数必须先检查 `Responding`，挂起期间只记录时间戳、不截图；挂起/恢复必须放在 `try/finally` 中，异常也恢复；每个阶段加 watchdog/超时（例如 15 秒）并输出心跳；不要把挂起状态跨 PowerShell 进程/脚本阶段传递。
