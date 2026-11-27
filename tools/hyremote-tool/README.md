# HyRemoteTool

HyRemoteTool 是 HyRemote 的**进程外配套客户端**。它运行在业务应用之外，通过显式 IP 和端口连接远程 HyRemote 应用，提供画面、输入和遥测。它不承载远程访问，不共享自己的窗口，也不链接 HyRemote Runtime。

源码目录是 `tools/hyremote-tool`，安装后的可执行文件是 `hyremote-tool`。

界面支持**简体中文和 English**。首次启动默认 **Auto（跟随运行 HyRemoteTool 的本机系统语言）**：本机系统区域为 `zh-*`（包括 `zh_TW`）时使用现有简体中文界面；其余语言回退至 English。可在窗口语言菜单选择 Auto、简体中文或 English；手动选择保存在**本机 HyRemoteTool 设置**中，重新启动后生效。`--language zh|en` 只覆盖**本次启动**，不会改写已保存的偏好。

## V0.3.6.0 S1 开发中：本机连接档案

本功能尚处于 V0.3.6.0 的开发与验收阶段，**不是已发布版本承诺**。新的“已保存”区域可以显式将当前主机/端口保存为本机书签，选择后回填连接地址（不会自动连接）；“连接档案”才会发起一次新会话，并始终从只读开始。支持本机编辑、删除确认、最近成功使用排序与重启后保留。只保存名称、主机、端口和最近使用时间；**不保存密码、凭据、权限或远端设备信任**。损坏档案默认不改写，恢复必须由用户确认，先隔离备份原文件。未知或过大的损坏档案可能需要人工恢复。

## V0.3.6.0 S1 开发中：基础诊断（会话接线与本机预览）

按 #585 加入**仅本机客户端来源**的报告模型候选，不是已发布诊断面板或 SDK 诊断。只记录本机观察到的连接阶段、错误类别、FPS/字节/帧/尺寸与最多 24 条状态事件；完整 JSON 限 8 KiB，绝不自动上传/复制。默认把设备端点遮盖为 `[redacted]`，操作者显式选择后也仅允许真正的 IPv4、IPv6（含合法作用域）或 DNS/本机主机名；包含路径、凭据、URL、无效地址的一律拒绝导出。远端 CPU、身份、Runtime 版本不可用；当前 Tool 已成功连接的 RFB None 明确标记未加密。现阶段已将真实 RFB 客户端状态、Qt 结构化 SocketError、连接统计和远程控制请求意图接到本机诊断会话，支持右上角“诊断”打开只读双语面板，用户主动刷新、预览、显式选择显示合法端点、复制本机报告或通过文件对话框保存。开启面板默认重新隐藏设备地址；不同连接尝试清除旧主机、指标和错误；报告不包括远端原始拒绝文本。复制不会操作远端 RFB 剪贴板，保存只在用户明确选择本机文件后执行。当前已有模型单测和离线窗口预览操作测试，仍需要真实假 RFB 拒绝/DNS/超时/畸形/断连、真实 Windows 窗口及完整 #586 现场验收；不能因这些 CTest 合并而称 S1 已发布。

## 会话安全（S1 #582 增量）

每次新连接、断开或故障都撤销本机控制请求；默认 **ViewOnly**，用户必须明确勾选“请求远程控制”才允许本机发送输入。该勾选只表达**本机请求意图**，不代表设备授权、接收或业务结果，最终仍由真实 HMI Host 的输入策略决定。RFB 3.8 ServerInit 名称是未经认证的显示元数据，不能凭 `view-only` / `control-enabled` 等名称判断设备能力；日志只保留固定 `TOOL_CLIENT_SERVER name=[redacted]`，不复制名称中的秘密、换行或 ANSI 控制序列。#582 完整状态机与 #586 真实 Windows 产品验收仍未完成。

## V0.3.0.0 范围

- 连接 `<remote-ip>:<port>`，端点由操作者输入，不做发现；
- RFB 3.8 / `SecurityType None`；
- 保持宽高比的 framebuffer 缩放显示；
- 默认只读；只有勾选**允许远程控制**后才发送指针、滚轮和键盘；
- FPS、吞吐（Mbit/s）、帧数、字节数和分辨率遥测；
- 明确的连接中 / 已连接 / 未连接 / 连接失败状态和最后一次错误；
- 断开和重连不需要重启工具。

## V0.3.0.0 不支持

- 管理面操作（远程启动/停止、策略或主机密码修改）；
- VNC-auth 连接密码；
- **远端剪贴板互通**、全屏、多显示器、TLS/VeNCrypt 和文件传输（本机诊断 JSON 的显式复制不属于远端剪贴板功能）。

界面不会为这些功能放置无效占位按钮。它们属于后续 companion-tool 阶段，见 [`docs/internal/hyremote-tool-plan.md`](../../docs/internal/hyremote-tool-plan.md)。

## 运行

启用 examples 后构建，然后：

```text
hyremote-tool
```

输入远程应用 IP 和端口，点击**连接**。可先用示例启动一个 HyRemote 主机：

```text
hyremote-learning-01-widgets --port 5931 --remote-input
```

可选参数：

```text
hyremote-tool --connect 192.168.1.6:5921
hyremote-tool --language en
hyremote-tool --language zh
hyremote-tool --verify-ui
```

`--verify-ui` 在 offscreen 下构造窗口，不连接、不监听。`--test-seconds N` 为自动化限制运行时间。

## 安全边界

V0.3.0.0 客户端只接受未认证的 `SecurityType None` 端点。仅在可信局域网或明确受保护的路径使用；它不是 Internet 安全方案，不能替代 TLS、认证或访问控制。

## English

HyRemoteTool is HyRemote's **out-of-process companion client**. It connects to a remote HyRemote-enabled application by explicit IP and port and provides viewing, input and telemetry. It does not host remote access, share its own window, or link the HyRemote Runtime.

The UI supports Simplified Chinese and English. The default is **Auto**, following the local OS language where HyRemoteTool runs (all `zh-*`, including `zh_TW`, use the available Simplified Chinese UI; other locales use English). The top-right selector offers Auto / Simplified Chinese / English. A manual selection persists locally across restarts; `--language zh|en` overrides **this launch only** and does not rewrite the saved preference. Invalid CLI language values fail instead of silently defaulting. No language is read from the remote RFB host.

Scope:

- connect to `<remote-ip>:<port>`; no discovery;
- RFB 3.8 / `SecurityType None`;
- aspect-preserving framebuffer scaling;
- view-only by default; pointer, wheel and keyboard are sent only after **Remote control** is checked;
- FPS, throughput, frame count, byte count and resolution telemetry;
- explicit Connecting / Connected / Disconnected / Failed state and last error;
- reconnect without restarting the tool.

Not supported: management-plane actions, VNC-auth passwords, **remote clipboard synchronization**, fullscreen, multi-monitor, TLS/VeNCrypt and file transfer. The local Diagnostics view can preview a redacted client-only JSON report; copying it is an explicit local clipboard action, never an RFB clipboard transfer.

Run:

```text
hyremote-tool --connect 192.168.1.6:5921 --language en
```

The client accepts only unauthenticated `SecurityType None` endpoints. Use it on a trusted LAN or another explicitly protected path; it is not Internet-safe.
