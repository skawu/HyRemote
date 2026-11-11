---
name: 本机实机任务的脚本与长驻进程约束
description: 在本工作区做实机验收时，命令外壳、脚本编码与"跨轮存活进程"的可靠做法
type: feedback
---

在这台 Windows 机器上做实机验收（启动 GUI/监听器、抓窗口、长时间采样）时：

1. 把逻辑写进 `.ps1` 文件并用 `powershell -NoProfile -ExecutionPolicy Bypass -File <path>` 调用，**脚本内容只用 ASCII**；不要在 `-Command` 里内联 `$变量`，也不要依赖 `Select-Object`（工具外壳在 PowerShell 与 cmd 之间会漂移，`cd /d`、内联变量、中文注释都会失败）。
2. 需要**跨工具调用存活**的进程（产品 GUI、TCP sink、HTTP 分发、后台采样）必须用 `Invoke-CimMethod -ClassName Win32_Process -MethodName Create` 创建（`cmd.exe /c "... > log 2>&1"` 转日志），因为 `Start-Process` 的子进程会随宿主命令结束被回收。
3. 想抓某个 Qt 窗口的完整位图用 `PrintWindow(hwnd, hdc, 2)` + `System.Drawing`（需 `Add-Type -AssemblyName System.Drawing`，且该引用不能写进 `Add-Type @"..."@` 的内联 C#）；前台锁需用 ALT 键 + `AttachThreadInput` 才能 `SetForegroundWindow`。

**Why:** 这些都是这一轮 #400/#410 实机验收里实际踩过的失败（脚本解析错误、进程被回收、`Add-Type` 编译失败、置前失败），每次都要多花数轮才定位；而产品按"活动窗口"取景/注入这一事实此前也未被记录。

**How to apply:** 下一次做任何需要真实桌面、真实窗口、跨轮长驻进程的任务时直接按上述方式起手，不要重新试错；若被控端是 HyRemote 的 `hyremote-tool`，注意它分享的是**它自己的窗口**（客户端 framebuffer 等于该窗口客户区，例如 780x1193），远程输入也注入到该窗口内。
