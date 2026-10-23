# ADR-0001：HyRemote Core 边界

> 语言 / Language：**中文** ｜ [English](../en/adr/0001-core-boundaries.md)

状态：**ARCH-01（#5）提议**

> 历史说明：本 ADR 记录 ARCH-01 早期设计。部分模块名和三种接入模式已被 ADR-0007 的四前端架构取代；当前架构以 [ADR-0007](0007-four-integration-runtime-architecture.md) 为准。

## 背景

HyRemote 需要通过一套 session 模型支持 Qt Widgets 和 Qt Quick，同时把协议、Qt private/QPA、GPU/BSP 和 SoC 细节留在稳定 core 之外。#3/#16 的宿主采集证据表明，不同 target family 的采集 API 差异很大，因此 Core 必须协调它们，但不能假设它们共享同一种渲染机制。

## 决策

`hyremote-core` 拥有 **session 语义**，不拥有 UI 框架或传输实现细节。

依赖方向：

```text
                 面向应用的模块
        ┌─────────────────┬─────────────────┐
        │ hyremote-widgets│ hyremote-quick  │
        └────────┬────────┴────────┬────────┘
                 │ capture/input adapters
                 ▼
             hyremote-core
       Session / RemoteFrame / queue
                 │
        ┌────────┼─────────┐
        ▼        ▼         ▼
 hyremote-vnc  encoder*  other transport*
     NeatVNC    adapters
```

可选的平台/QPA 兼容代码位于面向应用模块旁边，可以依赖 Qt private API，但 **Core 不得依赖它**。

## Core 职责

Core 拥有：

- `Session` 生命周期和状态；
- 采集调度策略和 in-flight 上界；
- 传输无关的 `RemoteFrame` 元数据和存储生命周期；
- 有界 completed-frame 队列和丢弃策略；
- 从采集完成到传输分发的交接；
- 规范化错误/状态报告；
- 把远程输入路由到 input sink；
- 选择兼容适配器所需的能力协商。

Core 不拥有：

- QWidget、QQuickWindow、QQuickItem 或 QML 对象语义；
- OpenGL/RHI/EGLFS 调用；
- NeatVNC/AML 调用；
- RFB 消息/编解码器；
- GBM/DRM/DMA-BUF 具体类型；
- RKMPP/V4L2/VA-API 具体类型；
- QPA private API；
- viewer 专用策略。

## 类型边界规则

Core 公共契约使用普通 C++17 值类型/接口类型。Qt GUI 类型、NeatVNC 类型和平台原生类型必须先适配，才能跨过 Core 边界。

这**不禁止** Core 实现在内部使用 QtCore（如果以后证明有用）；它禁止 Qt GUI/private/协议/平台类型成为 Core 公共语义的必需部分。

## 模块

### `hyremote-core`

协议无关的 session、frame、queue、状态/错误和扩展接口。

### `hyremote-widgets`

- QWidget/QPainter target 适配器；
- Widgets 采集源；
- QWidget 家族 input sink；
- 必要时处理 QOpenGLWidget/QQuickWidget 特例。

### `hyremote-quick`

- QQuickWindow target 适配器；
- 原生 Quick public async capture 候选（`contentItem()->grabToImage()`）；
- 同步 correctness fallback；
- Quick input sink；
- QML `RemoteAccess` wrapper，建立在同一 Core session 之上。

### `hyremote-vnc`

- NeatVNC 传输适配器；
- AML runtime 隔离；
- RFB 输入翻译；
- 每客户端传输队列/编解码器/认证。

### `hyremote-qpa`

可选零代码接入。Qt 版本敏感/private 代码隔离在这里，永远不定义 Core 语义。

### encoder/平台模块

可选的 RKMPP/V4L2/VA-API 等集成。encoder 可以消费 Core frame 或平台存储扩展，但 Core 不要求 encoder 才能运行。

## v0.1：一个 Session，一个共享 target

v0.1 中一个 `Session` 拥有一个逻辑共享 target/采集源和一个传输实例。传输可以在内部扇出到多个远程客户端。多 target/多显示 session 暂不冻结。

这保持 MVP 语义小，同时兼容后续多显示扩展。

## 接入模式

三种项目接入模式复用同一 Core 契约：

1. **Embedded C++ API**：应用代码通过 Widgets/Quick facade 构造/配置 Session。
2. **QML API**：Quick facade/Core Session 之上的声明式 wrapper。
3. **QPA proxy**：从隔离平台适配器创建/配置同一 Core 组件。

任何模式都不得创建第二套 capture/transport 架构。

## Encoder 位置

编码**不是** Core pipeline 的强制阶段。标准 RFB 可以直接消费 CPU frame；支持 H.264 的传输可以在下游插入 encoder。Core 因此冻结异步 encoder 需要的 frame/storage 生命周期契约，但不在 #5 冻结 codec 专用 API。

## 结果

- Widgets 和 Quick 可以使用不同采集源，而不需要 Session 特判；
- 未来 WebRTC/其他传输可以复用同一 frame/session 模型；
- #17 可以增加具体 external-buffer descriptor，而不改变 Session/backpressure 语义；
- #18 可以改变 EGLFS 上首选的采集后端，而不改变 Core 契约；
- Qt private/QPA 破坏不会使 `hyremote-core` ABI/源码语义失效。

## 暂缓

- external-buffer descriptor/handle/fence 的精确形状：#17；
- 最终 EGLFS 后端偏好/性能默认值：#18；
- 完整规范化键盘 schema：后续输入专项可以细化，不改变分发边界；
- 二进制 ABI 稳定性：pre-1.0 仍以源码兼容优先。
