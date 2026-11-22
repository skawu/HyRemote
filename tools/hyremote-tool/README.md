# HyRemoteTool

HyRemoteTool 是 HyRemote 的**进程外配套客户端**。它运行在业务应用之外，通过显式 IP 和端口连接远程 HyRemote 应用，提供画面、输入和遥测。它不承载远程访问，不共享自己的窗口，也不链接 HyRemote Runtime。

源码目录是 `tools/hyremote-tool`，安装后的可执行文件是 `hyremote-tool`。

界面支持**简体中文和 English**。首次启动默认 **Auto（跟随运行 HyRemoteTool 的本机系统语言）**：本机系统区域为 `zh-*`（包括 `zh_TW`）时使用现有简体中文界面；其余语言回退至 English。可在窗口语言菜单选择 Auto、简体中文或 English；手动选择保存在**本机 HyRemoteTool 设置**中，重新启动后生效。`--language zh|en` 只覆盖**本次启动**，不会改写已保存的偏好。

## V0.3.6.0 S1 开发中：本机连接档案

本功能尚处于 V0.3.6.0 的开发与验收阶段，**不是已发布版本承诺**。新的“已保存”区域可以显式将当前主机/端口保存为本机书签，选择后回填连接地址（不会自动连接）；“连接档案”才会发起一次新会话，并始终从只读开始。支持本机编辑、删除确认、最近成功使用排序与重启后保留。只保存名称、主机、端口和最近使用时间；**不保存密码、凭据、权限或远端设备信任**。损坏档案默认不改写，恢复必须由用户确认，先隔离备份原文件。未知或过大的损坏档案可能需要人工恢复。

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
- 剪贴板、全屏、多显示器、TLS/VeNCrypt 和文件传输。

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

Not supported: management-plane actions, VNC-auth passwords, clipboard, fullscreen, multi-monitor, TLS/VeNCrypt and file transfer.

Run:

```text
hyremote-tool --connect 192.168.1.6:5921 --language en
```

The client accepts only unauthenticated `SecurityType None` endpoints. Use it on a trusted LAN or another explicitly protected path; it is not Internet-safe.
