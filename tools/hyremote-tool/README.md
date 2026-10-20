# HyRemoteTool

HyRemoteTool 是 HyRemote 的**进程外配套客户端**。它运行在业务应用之外，通过显式 IP 和端口连接远程 HyRemote 应用，提供画面、输入和遥测。它不承载远程访问，不共享自己的窗口，也不链接 HyRemote Runtime。

源码目录是 `tools/hyremote-tool`，安装后的可执行文件是 `hyremote-tool`。

界面支持**中文和 English**，默认中文。可在窗口右上角切换，也可用 `--language en` 从英文启动。

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
hyremote-tool --verify-ui
```

`--verify-ui` 在 offscreen 下构造窗口，不连接、不监听。`--test-seconds N` 为自动化限制运行时间。

## 安全边界

V0.3.0.0 客户端只接受未认证的 `SecurityType None` 端点。仅在可信局域网或明确受保护的路径使用；它不是 Internet 安全方案，不能替代 TLS、认证或访问控制。

## English

HyRemoteTool is HyRemote's **out-of-process companion client**. It connects to a remote HyRemote-enabled application by explicit IP and port and provides viewing, input and telemetry. It does not host remote access, share its own window, or link the HyRemote Runtime.

The UI is bilingual (Chinese and English) and starts in Chinese. Switch it in the top-right selector or start with `--language en`.

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
