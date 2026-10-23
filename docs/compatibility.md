# HyRemote 兼容性矩阵

> 语言 / Language：**中文** ｜ [English](en/compatibility.md)

HyRemote 只对具有明确产品状态的环境声明兼容性。Qt API 相似、补丁版本接近、操作系统相近或图形后端类似，都不会自动构成支持声明。

## 状态定义

- **Supported**：已对当前产品线完成资格认证。C++ API、QML API、Generic Plugin、QPA 是并列接入技术，没有主次或预览层级。
- **Limited**：只在声明的窄条件下完成资格认证，例如 QPA 的精确 Qt private ABI。
- **TODO**：计划中的产品覆盖，尚未可用或尚未认证。
- **Unsupported**：超出声明产品契约，或已知在该组合下不可用。
- **User-validation pending**：Qt 跨平台代码/构建路径存在，但用户目标系统尚未完成独立证明；不阻塞 Windows 版本，不等于 Supported。

## V0.3.0.0 已发布时的历史参考矩阵（保持原状）

**新版本资格策略（Owner #563）仅要求 Windows x86_64 / Qt 6.8.3 真实验证。** 下表 Linux 的 Supported/Limited 是 V0.3.0.0 **发布当时的历史声明**，并非新版本的实际资格状态；新版本 Linux 状态为 User-validation pending、非发布阻塞。历史 tag/包/Release Notes 不改写，未来支持声明必须以独立目标机证据为准。

| Qt | 平台 | 接入方式 | UI 目标 | 状态 | 说明 |
| --- | --- | --- | --- | --- | --- |
| 6.8.3 | Windows x86_64 | C++ API | Widgets | **Supported** | 公共 Qt API，显式 Runtime 生命周期（explicit Runtime lifecycle） |
| 6.8.3 | Windows x86_64 | C++ API | Qt Quick | **Supported** | 同一个 `HyRemote::RemoteAccess` 入口 |
| 6.8.3 | Linux x86_64 | C++ API | Widgets | **Supported** | 公共 Qt API，显式 Runtime 生命周期（explicit Runtime lifecycle） |
| 6.8.3 | Linux x86_64 | C++ API | Qt Quick | **Supported** | 同一个 `HyRemote::RemoteAccess` 入口 |
| 6.8.3 | Windows x86_64 | Generic Plugin | Widgets / Quick | **Supported** | 零代码路径，保留原生 Qt 平台 |
| 6.8.3 | Linux x86_64 | Generic Plugin | Widgets / Quick | **Supported** | 零代码路径，保留原生 Qt 平台 |
| 6.8.3 | Windows x86_64 | QML API | Qt Quick | **Supported** | 共享 Runtime 上的薄声明式前端 |
| 6.8.3 | Linux x86_64 | QML API | Qt Quick | **Supported** | 共享 Runtime 上的薄声明式前端 |
| 6.8.3 exact | Windows x86_64 | QPA | Widgets / Quick | **Limited** | Private ABI；原生 delegate `qwindows` |
| 6.8.3 exact | Linux x86_64 | QPA | Widgets / Quick | **Limited** | Private ABI；原生 delegate `qxcb` |
| 5.15.2 | Windows x86_64 | C++ API | Widgets | **Limited** | Qt5 遗留路径：Runtime、Tool、示例和安装消费者可构建/部署，62/62 CTest；Quick 为实验性 build-only，QML/QPA/Linux 未认证 |

这张表只描述当前产品线，不表示 Widgets/Quick 应用里的每一种图形配置都已经认证。

## Qt LTS 政策

HyRemote 支持选定的 Qt LTS 家族，不自动承诺每一个 Qt 版本。

当前状态：

- **Qt 6.8 LTS**：当前参考家族；Qt 6.8.3 是 V0.3.0.0 使用的精确参考 SDK。
- **Qt 5.15 LTS**：Windows x86_64 精确 cell 为 **Limited** 遗留路径。C++/Widgets Runtime、Tool、示例和安装消费者已构建/安装/测试通过；Qt5 Quick 仍为实验性 build-only；QML API、QPA、Linux 和 ARM64 Qt5 仍为 TODO，不得由 Windows Widgets cell 外推。
- 其他 Qt LTS 或非 LTS 家族：没有明确兼容行时，不做支持声明。

公共 Qt 前端和 QPA 的兼容规则不同：

- C++、QML、Generic 主要依赖 Qt 公共 API，可以按 Qt 家族/版本范围认证；
- QPA 依赖 Qt private ABI，必须按精确 Qt 补丁和平台组合认证。

一个 Qt 补丁在公共 API 上兼容，不会自动让 QPA 二进制兼容。

## 操作系统

强制真实验证平台：

- **Windows x86_64**（实际工作/GA 资格环境）；
- Linux x86_64（Qt 跨平台路径；**用户验证中，不阻塞开发、V0.4 或 V1**）。

一个操作系统通过，不表示另一个操作系统也会通过。

> **Owner #563：** V0.4 只对已承诺 Windows 单元执行硬资格；Linux 可收集用户报告或已有 CI 诊断，但不得作为发布门槛。

## 产品 payload 兼容性

| 产品 payload | 兼容性预期 |
| --- | --- |
| `HyRemote::RemoteAccess` | C++ API 和其他接入技术内部共用的 Shared Runtime |
| QML `HyRemote` 模块 | 同一 Shared Runtime 上的声明式 payload |
| Generic Plugin | Qt 公共 generic-plugin payload，必须保留应用原生 Qt 平台身份 |
| `qhyremote` QPA 插件 | 精确 Qt private-ABI payload，委托给经过认证的原生平台实现 |
| Core | 内部实现组件，不是独立 SDK/Runtime 兼容面 |

应用不要把不相关 HyRemote SDK 构建的 payload 混用，也不要用构建树副本修补不完整的部署包。

## Widgets 范围

可移植正确性基线支持经过认证的 QWidget 顶层目标，通过 Widgets 适配器路径。

基础 Widgets 支持不会自动认证以下配置：

- `QOpenGLWidget`；
- 嵌入/原生子窗口；
- 特殊平台原生归属；
- 绕过正常 QWidget 渲染假设的第三方渲染引擎。

这些场景只有在进入产品矩阵时才单独认证。

## Qt Quick 范围

可移植正确性基线支持经过认证的 `QQuickWindow` 目标，通过公共异步 Quick 采集路径。

### Quick-only 部署

Quick-only 应用可以关闭 Widgets 适配器，避免共享 Runtime 因另一个 UI family 而加载 `Qt6Widgets`：

```text
-DHYREMOTE_BUILD_WIDGETS_ADAPTER=OFF
-DHYREMOTE_BUILD_QUICK_ADAPTER=ON
```

在 `build.yml` 的 `cmake` 段设置这两个值后，先清理再构建。该组合仍保留同一个 Core 和一个 Shared Runtime；Quick 目标的采集与输入走 Quick 适配器。

Windows x86_64 上已用 `dumpbin /dependents` 验证 `HyRemoteRemoteAccess.dll` 只依赖 `Qt6Quick/Qt6Gui/Qt6Core/Qt6Network`，不依赖 `Qt6Widgets`。ARM64/EGLFS 的干净 rootfs 运行验证仍需要目标板。

基础 Qt Quick 支持不会自动认证以下配置：

- Quick3D；
- 自定义 FBO/渲染节点管线；
- 特殊图形后端；
- `QQuickWidget` 混合 Widgets/Quick 组合；
- 应用正常 Qt surface 模型之外的外部/原生窗口。

> **TODO V0.4：** 认证有代表性的真实渲染/应用组合，不用推断扩大声明。

## Generic Plugin 兼容性

当应用可以保留正常 Qt 平台时，Generic Plugin 是首选的零代码路径。

兼容的 Generic 部署应满足：

- 应用源码保持纯 Qt；
- 应用不链接 `HyRemote::RemoteAccess`；
- 通过 Qt 通用插件机制激活 HyRemote；
- 原生 Qt 平台身份保持 `windows`、`xcb` 或应用原本使用的平台；
- 部署树同时包含 Generic Plugin 和正常原生 Qt 平台插件；
- 运行时不依赖原 HyRemote/Qt 构建树。

Generic 兼容性是公共 Qt 声明，不与 QPA private ABI 绑定。

## QPA 兼容性

QPA 是专用零代码路径，兼容边界更窄。

当前参考组合：

| Qt | OS | 原生 delegate | 状态 |
| --- | --- | --- | --- |
| 6.8.3 exact | Windows x86_64 | `qwindows` | 并列路线 |
| 6.8.3 exact | Linux x86_64 | `qxcb` | 并列路线 |

不要从这些行推断其他 Qt 补丁、Wayland、EGLFS、macOS 或其他平台插件受支持。

> **TODO：** 只有精确 Qt/private-ABI/平台组合完成认证后，才增加新的 QPA 行。

## 传输与 Viewer 边界

当前传输基线是有界 RFB 3.8。

当前产品行为：

- 默认提供 LAN 可达的 listener；
- 标准 RFB 远程查看；
- 可选远程输入；
- 重连不要求重建应用 Runtime；
- `Insecure` 未认证、未加密，只面向可信 LAN；
- 只有构建包含传输安全能力并配置有效安全描述文件时，才提供条件 RFB VNC 认证；
- 当前产品线没有流加密；
- `AuthenticatedEncrypted` 不可用，在创建 listener 前失败关闭。

默认构建不能因为公共 API 暴露 `Authenticated` 就视为提供认证传输。

Viewer 互操作声明只应针对实际测试过的 Viewer 和版本。某个 Viewer 能连一次，不足以扩大整个产品兼容矩阵。

## 输入兼容性

远程输入兼容性包括生命周期行为，不只是普通指针/按键交付。

产品必须在断开和 Runtime 停止时保持受支持的按住键/按钮状态平衡。不支持的输入法/组合或按键场景会被记录，不做猜测。

## 部署兼容性

受支持的部署路径应能从应用部署树运行，不依赖：

- 原 HyRemote SDK 路径；
- HyRemote 构建树；
- 作为运行时拐杖的 Qt SDK 插件路径；
- 手工复制的 HyRemote 内部实现文件。

产品部署入口是 `hyremote_deploy()`；见 [`guide/deployment.md`](guide/deployment.md)。

## 嵌入式/平台扩展

当前强制验证是 Windows x86_64。ARM64、嵌入式 Linux、EGLFS/Wayland 依托 Qt 跨平台能力供用户验证，未获得实机证据前不能称为 Supported；**不要求在 V0.4 RC 或 V1 前完成 Linux/嵌入式物理认证**（#563）。声明 GA 参考单元之外的 BSP/SoC/平台扩展按后续连贯结果单独准入。硬件/厂商加速不预分配版本或 GA 前后位置，只有性能专项的测量证据证明可移植基线存在 SLO/资源阻塞时，才通过记录的激活权威准入。

嵌入式部署是后续产品线，不属于当前桌面声明。

计划方向：

| 方向 | 当前状态 |
| --- | --- |
| ARM64 嵌入式 Linux | User-validation pending / 非发布阻塞；不预留精确 Feature 版本 |
| RK3588 / EGLFS 或 Wayland | User-validation pending / 非发布阻塞；不预留精确 Feature 版本 |
| NXP i.MX 类 | TODO，声明 GA 单元之外的平台扩展，单独准入 |
| DMA-BUF / GBM / 外部缓冲路径 | TODO，性能专项测量证明阻塞后，通过记录权威（#123）条件实现；不预留版本 |
| RKMPP / VAAPI / 平台硬件编码 | TODO，性能专项测量证明阻塞后，通过记录权威（#123）条件实现；不预留版本 |
| OpenHarmony | 长期方向，当前没有支持声明 |

桌面证据不表示嵌入式支持，一个 BSP 也不表示整个 SoC/平台家族。

## 兼容性规则

1. Supported、Limited、User-validation pending、TODO、Unsupported 不能互换。
2. Windows 结果不能替代 Linux 结果，反之亦然。
3. 公共 Qt API 兼容不表示 QPA private ABI 兼容。
4. 基础 Widgets/Quick 支持不会自动认证每一种图形/渲染配置。
5. 桌面 x86 结果不表示嵌入式 Linux 支持。
6. 硬件加速支持独立于稳定的应用接入契约。
7. 只有产品能说明精确 Qt、OS、接入路径、应用范围和已知限制时，才增加新的支持行。
