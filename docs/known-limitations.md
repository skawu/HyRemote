# 已知限制

> 语言 / Language：**中文** ｜ [English](en/known-limitations.md)

本文列出会影响 HyRemote 用户的产品边界，不记录开发历史、CI 状态或验收流程。

精确兼容矩阵见 [`compatibility.md`](compatibility.md)。

## 当前产品线

当前产品线是 **V0.3.0.0 Product Preview**，参考环境：

- Windows x86_64；
- Linux x86_64；
- Qt 6.8.3；
- C++ API、QML API、Generic Plugin、QPA 是并列接入技术，没有主次或预览层级。

Qt 5.15 LTS、嵌入式 Linux、ARM64 和其他 QPA private-ABI 组合不在 V0.3.0.0 支持声明中。**新版本 Windows-first Owner 决策 #563：Linux/嵌入式/Wayland 为用户验证中，不阻塞开发或 V0.4/V1；不得从 Qt 跨平台性推断其已通过资格。**

## 安全

V0.3.0.0 不是面向 Internet 的远程访问服务器。

当前限制：

- 默认 listener 是 `0.0.0.0:5921`，在主机 IPv4 接口可达；
- 远程输入默认关闭；
- `Insecure` 未认证、未加密，只面向可信 LAN，不适合 Internet；
- `Authenticated` 是条件能力，需要启用传输安全的构建和有效安全描述文件；
- `Authenticated` 可用时使用 RFB VNC 认证，但传输仍未加密；
- 默认构建不表示认证传输已经编译进来；
- 请求 `Authenticated` 但构建能力不足时，在创建 target/transport/listener 前以 `SecurityUnavailable` 失败；
- `AuthenticatedEncrypted` 未实现，始终在创建 listener 前以 `SecurityUnavailable` 失败；
- `AuthenticatedEncrypted` 不会回退到 `Authenticated` 或 `Insecure`，即使存在证书/私钥描述文件；
- 当前没有生产 TLS/证书策略；
- 当前没有认证 Session Registry 或每会话角色模型。

停止 Runtime 是显式且完整的：`stop()` 会释放 Runtime 为已连接 viewer 保持的输入状态，并把 listener 和目标适配器恢复到 `Stopped`，应用不需要自己撤销合成的远程按住状态。

> **后续：** 加密传输、证书策略、认证会话、准入/终止控制和生产网络策略。

见 [`security.md`](security.md) 和 [`security-model.md`](security-model.md)。

## Listener 地址

HyRemote 当前接受数字 IP 地址，不接受主机名/DNS 名称。

重要行为：

- 默认 `0.0.0.0`，即所有 IPv4 接口；也可以是一个精确本地 IPv4 或命名接口；
- 无效/未分配的绑定地址会失败，不会静默扩大到 wildcard；
- 端口已被占用时启动失败；
- `0.0.0.0` 是默认值，会扩大 IPv4 暴露到主机接口；它配合 `Insecure` 使用，因此只面向可信 LAN，不适合 Internet；
- IPv6 wildcard 行为因平台而异，不能假设双栈。

在 Windows/Qt 6.8.3 参考路径上，`::` 是 IPv6-only listener，不会通过同一 socket 接受 IPv4。

> **TODO V0.4：** 在最终 Windows/Linux 兼容矩阵上完成显式地址族资格。

修改绑定地址会改变安全边界，见 [`security.md`](security.md)。

## 采集范围

可移植正确性基线支持经过认证的 Qt 应用 surface，不覆盖所有图形配置。

当前基线：

- QWidget 顶层目标通过 Widgets 适配器；
- QQuickWindow 目标通过 Qt Quick 适配器；
- Generic/QPA 路径对受支持的应用自有顶层 surface 做自动组合。

不要自动推断支持：

- 每一种 `QOpenGLWidget` 配置；
- 每一种 `QQuickWidget` 组合；
- Quick3D；
- 自定义 FBO/渲染节点路径；
- 任意 `QOpenGLWindow`/原生 OS 窗口；
- 不属于 Qt 应用的外部窗口。

> **TODO V0.4：** 认证有代表性的真实复杂渲染路径，并写成显式兼容行。

## 输入范围

远程输入当前覆盖规范化、应用作用域的指针、按钮、滚轮、按键/修饰键，以及协议信息足够时的提交文本。

限制：

- HyRemote 不把桌面级虚拟 HID/uinput 控制作为正常产品路径；
- 不是每个平台/viewer 组合都保证完整 IME 组合、死键和国际布局一致；
- 在合格路径上，维护中 VNC viewer 可以输入可打印文本：真实 LAN 试验输入过拉丁字符、数字和中文；
- 远程输入是**应用作用域**。如果宿主本地激活了另一个应用或桌面，远程指针不能可靠地把 OS 激活抢回目标应用，远程输入可能停止，直到目标应用在本地重新激活；
- 不支持的输入语义不通过猜测重建；
- 多个 viewer 共享一个逻辑应用输入设备，而不是各自独立的 cursor/focus 域。

断开连接或 Runtime 停止时，受支持的按住键/按钮状态会被清理，远端不应把目标留在按下状态。

## 会话模型

V0.3.0.0 暴露运行连接数，不提供完整认证会话管理 API。

`connectedClientCount()` 告诉应用当前连接了多少客户端，但不提供：

- 认证用户身份；
- 角色；
- 每会话准入策略；
- 每会话终止/封禁控制；
- 独立的每 viewer focus/cursor 策略。

> **后续：** 认证 Session Registry 和有界准入/终止 API。

## QML API

QML API 是 V0.3.0.0 的并列接入技术，和 C++ API 使用同一共享 Runtime。

它不创建第二套 Runtime。安装后 SDK 的学习/部署体验和更广资格认证仍在完善。

> **后续：** 进一步 QML 产品化和示例/部署资格。

## Generic Plugin

Generic Plugin 是 V0.3.0.0 的并列零代码接入技术。

它的契约是应用原生 Qt 平台保持权威。部署后如果应用平台身份意外变成 `hyremote`，就不是有效 Generic 配置。

Generic 不继承 QPA private-ABI 兼容声明。

## QPA

QPA 是 V0.3.0.0 的并列接入技术，比 Generic 更窄，因为它依赖 Qt 精确 private ABI。

限制：

- 涉及 Qt private ABI；
- 当前参考是 **Qt 6.8.3 exact**；
- Windows 原生 delegate：`qwindows`；
- Linux/X11 原生 delegate：`qxcb`；
- 另一个 Qt 补丁/次版本不自动兼容；
- Wayland/EGLFS/macOS 和其他 delegate 不由参考组合推断；
- 任意外部/原生窗口不在当前 QPA 声明内。

满足应用需求时优先使用 Generic。

> **当前产品线未完成：** 正式 QPA 产品化；只有合格 Qt/OS 组合才增加精确兼容行。

## 多窗口行为

HyRemote 面向一个 Qt 应用，不面向完整操作系统桌面。

受支持的应用自有顶层 surface 可以表示在一个逻辑远程应用会话中。打开或关闭受支持对话框/工具窗口不应要求新建 listener。

这不构成对任意原生/外部窗口的 blanket 声明。

## 性能

V0.3.0.0 是正确性优先、CPU 可读的基线。

它不承诺：

- 通用零拷贝采集；
- DMA-BUF/GBM 生产路径；
- 硬件 H.264/H.265 编码；
- RKMPP/VAAPI/D3D 加速传输；
- 每种 Qt 应用类型的最优 per-surface damage；
- 与媒体流协议等价的高动态视频/Quick3D 性能。

远程交互可能明显慢于本地输入，但仍可用：首次真实 LAN 试验报告宿主立即响应，远程画面有明显延迟。性能工作由测量到的产品阻塞驱动，不由平台 API 可用性驱动。

> **后续加速线：** 只有测量证明必要时才加入低拷贝/硬件路径。

## 部署

有效产品部署应从自己的应用树运行。

它不应依赖：

- 原 HyRemote 构建目录；
- 原 HyRemote SDK 路径；
- 作为运行时 workaround 的 Qt SDK 插件路径；
- 手工复制的内部 Core/transport/capture 文件。

使用 `hyremote_deploy()` 放置产品 payload。见 [`guide/deployment.md`](guide/deployment.md)。

## 物理本地+远程共存

Headless/offscreen 执行可以证明远程协议到应用的行为，但不能单独证明每种物理显示/输入配置在远程访问期间都正确。

产品目标是在加入远程访问的同时保持本地原生渲染/输入。

> **TODO V0.4：** 完成最终兼容矩阵的 Windows/Linux 物理资格，尤其是 QPA 和复杂多窗口路径。

## Qt 版本

当前参考：**Qt 6.8.3 / Qt 6.8 LTS 家族**。

Qt 5.15 LTS 是计划中的兼容线，但不是当前产品声明，因为当前 Runtime 需要 Qt 6.8+。

> **Owner #563：** Qt 5.15 如需加入 GA Windows 支持，必须先在 Windows 完成构建/部署/运行验证；Linux 验证不再是 V0.4 准入条件。

## 嵌入式平台

桌面 x86 结果不表示嵌入式 Linux 支持。

以下是后续产品方向：

- ARM64 嵌入式 Linux；
- RK3588；
- EGLFS/Wayland；
- NXP i.MX 类；
- DMA-BUF/GBM 外部缓冲路径；
- RKMPP 和其他硬件编码器。

> **Owner #563（取代原硬门槛）：** 嵌入式 Linux/ARM64/EGLFS 属于 Qt 跨平台用户验证路径；无目标机/完整测试不阻塞开发、V0.4 或 V1。发现缺陷请提交附有 Qt/BSP/架构/日志/复现步骤的 Issue，按可行的维护/hotfix 流程处理；不预留版本或伪造 PASS。
>
> **后续加速线：** 在嵌入式正确性建立后再做硬件加速采集/编码。
