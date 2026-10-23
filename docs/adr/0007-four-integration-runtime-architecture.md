# ADR-0007：Core + 共享 Runtime 上的四个接入前端

> 语言 / Language：**中文** ｜ [English](../en/adr/0007-four-integration-runtime-architecture.md)

状态：**已接受，V0.1 基础 / V1 架构不变量（#219，由 #209 校准）**

## 背景

HyRemote 最初把面向应用的技术放在 `src/cpp`、`src/qml`、`src/qpa` 等并列目录。设计基于 `QGenericPlugin` 的第四个零代码接入时，暴露出两个结构问题：

1. `src/cpp` 看起来像通用 C++ 实现目录，实际只是 Embedded C++ 的接入 facade。Embedded C++ 本身不专属 Widgets：`RemoteAccess` 的 target 可以是 Widgets 或 Qt Quick，由 Runtime 选择适配器。
2. `src/qpa` 持有应用级自动 surface/composition 逻辑。这些逻辑并非 QPA 专属，Generic Plugin 也需要共享。

`src/core` 仍是 UI、协议、平台无关的公共 core 实现。它的边界有意排除 QWidget、Qt Quick/QML、具体传输实现和 Qt private/QPA 类型。这些限制不能为了共享 Qt 侧 runtime 代码而放宽。

Qt 兼容性分析还确认两点：

- QPA/private API 不提供产品级公共层所需的兼容保证，QPA 相关代码必须隔离在一个兼容前端内；
- `QGenericPlugin` 可以在不替换原生 QPA 的前提下提供零代码接入，因此应是并列前端，而不是 C++ 或 QPA 的实现细节。

## 决策

HyRemote 使用 **Core + 共享 Runtime + 四个并列接入前端**，四个前端统一放在 `src/integrations/` 下：

```text
integrations/cpp --------\
integrations/qml ---------> runtime -> core
integrations/generic ----/
integrations/qpa --------/
```

规范源码角色：

```text
src/core                  共享的 UI/协议/平台无关 core 实现
src/runtime               共享 Qt/产品 runtime 实现
src/integrations/cpp      Embedded C++ 接入前端
src/integrations/qml      QML 接入前端
src/integrations/generic  QGenericPlugin 零代码接入前端
src/integrations/qpa      QPA 零代码接入前端与兼容 shim
```

四个前端是并列关系，任何前端都不是其他前端的架构父级。

## `src/core` 职责

`src/core` 拥有与 Qt UI 技术、具体传输和平台实现无关的稳定产品语义：

- Session 生命周期、状态和错误语义；
- `RemoteFrame` 元数据和存储生命周期；
- 有界调度、mailbox 和 backpressure 策略；
- 规范化的远程输入模型和分发边界；
- 传输无关、target 无关的接口；
- Core 抽象层的能力协商。

Core 不得要求 QWidget、QQuickWindow/QML、QPA/private API、具体 RFB 实现类型、图形平台 API 或 SoC/vendor API。

## `src/runtime` 职责

`src/runtime` 拥有多个或全部接入前端共用的产品实现，这些实现不能放进 UI 无关的 Core。包括：

- Qt Widgets target 适配器；
- Qt Quick target 适配器；
- 具体 RFB 传输与传输安全实现；
- 组装 Core 接口与 Qt/产品实现的 component factory；
- 共享 Qt 应用 surface 发现；
- 应用 surface 模型；
- composite capture target 和输入路由；
- Generic Plugin 与 QPA 零代码模式共用的应用级自动接入控制器；
- 前端共享的内部 runtime 配置/所有权服务。

`src/runtime` 可以依赖 Qt public API 和 `HyRemote::Core`，不得依赖 `src/integrations/` 下的任何目录。

## 接入前端职责

### `src/integrations/cpp`：Embedded C++ 接入

拥有公共 `HyRemote::RemoteAccess` C++ facade，以及 Embedded C++ 使用者需要的接入专用 facade/ABI。产品工作委托给共享 Runtime。

这个前端**不是 Widgets 前端**。C++ 使用者可以面向 Widgets 或 Qt Quick；target family 适配属于 Runtime。

### `src/integrations/qml`：QML 接入

拥有 `import HyRemote` 和 QML 侧 wrapper/属性/生命周期语义。产品工作委托给共享 Runtime，不实现第二套 Session/capture/transport 架构。

### `src/integrations/generic`：QGenericPlugin 接入

拥有通过 `-plugin hyremote` 或 `QT_QPA_GENERIC_PLUGINS` 加载的 Qt Generic Plugin payload。保留应用原生 QPA 和平台身份，启动共享自动 runtime。必须使用 Qt public Generic Plugin API，不得依赖 Qt QPA/private API。

### `src/integrations/qpa`：QPA 接入

拥有 Qt private/QPA 兼容边界。长期设计是 **Factory Trampoline**：解析 HyRemote 平台参数，解析或创建已验证的原生 Qt 平台集成，启动并 arm 同一自动 runtime，然后把原生 `QPlatformIntegration*` 返回 Qt。QPA 不拥有产品 capture/input/transport/session 语义。

四个接入前端中，只有 `src/integrations/qpa` 可以依赖 Qt private/QPA ABI。

## 为什么显式使用 `src/integrations/`

把 `cpp`、`qml`、`generic`、`qpa` 直接放在 `src/` 下，会让这些技术名看起来像共享实现层。对 `cpp` 尤其误导，因为 HyRemote 绝大部分本来就是 C++。

统一放到 `src/integrations/` 后，所有权模型可以直接从目录读出：

```text
src/core           共享产品语义
src/runtime        共享产品实现
src/integrations/* 应用进入 runtime 的方式
```

Widgets 和 Quick 是 Runtime 适配维度，不是接入目录维度。

## 自动接入归属

以下代码属于应用级自动接入，不属于 QPA 实现，因此从 QPA 前端移入共享 Runtime：

- 应用 surface 模型；
- composite target 和路由；
- interactive composite target；
- 应用级 automatic access controller。

Generic Plugin 和 QPA 必须使用同一个自动控制器和 surface/composition 语义。

## QPA 组合边界

`qpa_composition_seam.*` 曾经存在，是因为 QPA 拥有的 composite 代码需要 Core 的 frame/input 操作，而 QPA 不能直接链接 Core。composite/automatic 代码完全归共享 Runtime 后，这层间接不再必要，已删除而不是改名为另一个前端专用 seam。runtime 侧组合源码现在直接包含 `detail/composition_support.hpp`。

## QPA 平台策略

HyRemote 不创建 Rockchip/NXP/TI 专用 QPA 实现。平台链：

```text
HyRemote QPA Factory Trampoline
        -> 原生 Qt QPA (windows/xcb/wayland/eglfs/...)
        -> vendor Qt/BSP
        -> OS/GPU/display/input 硬件
```

Vendor BSP 与 Qt 负责原生显示/输入/GPU 适配。平台专用 HyRemote 代码只允许出现在 QPA 兼容 shim，或在 portable 路径测量证明必要后单独准入的加速后端。

## 兼容模型

- `core`：普通 C++ 兼容边界，不涉及 Qt private ABI；
- `runtime`：Qt public API/产品实现边界；
- `integrations/cpp`：公共 C++ 接入 ABI/API；
- `integrations/qml`：公共 Qt/QML 接入边界；
- `integrations/generic`：公共 Qt Generic Plugin 边界；
- `integrations/qpa`：Qt private QPA 边界，按支持的 Qt/QPA 兼容单元分别重建和认证。

共享源码架构不意味着跨 Qt minor、CPU 架构或 vendor-patched Qt 分发共用同一个 QPA 二进制。

## 目标产品行为

四个模式共享同一产品语义：

1. Embedded C++ API；
2. QML API；
3. Generic Plugin 零代码接入；
4. QPA Factory-Trampoline 零代码接入。

差异只在 bootstrap、配置和 target 所有权。Session、frame、capture、input、transport、security、cleanup 和错误语义都是共享 Runtime/Core 行为。

## 目标仓库布局

迁移后的规范发布树：

```text
src/
├── core/
├── runtime/
└── integrations/
    ├── cpp/
    ├── qml/
    ├── generic/
    └── qpa/
```

`src/` 仍是发布树。`runtime` 是内部共享实现模块，不是第五种应用接入模式。

## 迁移期间的构建/制品兼容

源码所有权和制品路径是两份契约。迁移可以在改变源码目录的同时保留现有 target 名、输出名和构建/部署位置。

特别是 `HyRemoteRemoteAccess` / `HyRemote::RemoteAccess` 可以继续使用既有的构建/输出身份，即使 target 所有权移到 `src/runtime`，C++ facade 位于 `src/integrations/cpp`。

任何有意的制品/API 变化都需要单独的兼容决策，不能由仓库搬迁隐含。

## 迁移约束

- 不得为了吸收这次迁移而改变已冻结的发布候选或证据；
- 在专门的 #219 架构分支/PR 上执行迁移；
- 全程保持一套 Core/session/capture/input/transport 架构；
- 不得通过让 Generic 或 QPA 依赖 Embedded C++ 实现细节来临时解决所有权问题；
- QPA/private 头文件不得进入 Core、Runtime、C++、QML 和 Generic 模块；
- 删除旧的物理根目录 `src/cpp`、`src/qml`、`src/generic`、`src/qpa`，不保留转发目录或兼容副本；
- 完成迁移前更新仓库布局门禁、package/deploy 契约、示例和跨模式测试。

## 结果

正面：

- 源码树本身区分共享实现与应用接入；
- `src/integrations/cpp` 明确表示 Embedded C++ API，而不是所有 C++ 实现；
- Widgets 和 Quick 保持平等的 Runtime target family；
- Generic 和 QPA 成为真正的并列零代码前端；
- 应用级自动接入只实现一次；
- QPA private 兼容风险被显著隔离；
- vendor/SoC 平台扩展不会制造新的产品人格；
- QPA 专用组合桥可以消失。

成本：

- build/install/release-readiness 路径必须有计划地迁移；
- 编码旧物理路径的历史文档和 CI 检查必须更新；
- 移动 automatic/composite 实现时，需要通过跨模式测试保持现有 QPA 行为。
