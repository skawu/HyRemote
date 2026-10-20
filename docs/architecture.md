# HyRemote 产品架构

> 语言 / Language：**中文** ｜ [English](en/architecture.md)

HyRemote 由一个 Core、一个共享 Runtime 和四个并列接入前端组成。应用可以选择接入方式，但不会因此产生独立的 Session、采集、传输、输入、安全或性能实现。

## 1. 产品拓扑

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

依赖方向：

```text
integrations/cpp --------\
integrations/qml ---------\
integrations/generic ------> runtime -> core
integrations/qpa ---------/
```

四个前端并列：

- **C++ API**：通过 `HyRemote::RemoteAccess` 提供显式应用 API；
- **QML API**：通过 `import HyRemote` 提供声明式前端；
- **Generic Plugin**：使用 Qt 公共插件机制的零代码前端，保留原生 Qt 平台；
- **QPA**：使用 private ABI 的零代码前端，通过 Factory Trampoline 进入并委托给原生 Qt 平台实现。

Widgets 和 Qt Quick 是 Runtime 的目标适配器维度，不是接入模式。

## 2. Core

Core 负责不依赖 Qt UI 技术、Qt private API、具体传输后端或特定平台图形栈的产品语义。

Core 的职责：

- Session 生命周期和状态语义；
- 帧生命周期和与传输无关的帧元数据；
- 采集调度和有界帧交接；
- 队列、丢弃和背压策略；
- 规范化产品错误和状态；
- 规范化输入路由抽象；
- 与传输无关的能力描述。

Core 不依赖：

- Qt Widgets、Qt Quick、QML；
- Qt private/QPA API；
- 具体 RFB 实现类型；
- OpenGL/RHI/EGLFS/DRM/GBM/DMA-BUF API；
- 硬件编码 SDK；
- 接入前端策略。

这条边界让产品模型在目标、传输、平台和加速实现变化时保持稳定。

## 3. 共享 Runtime

共享 Runtime 是所有前端共用的唯一 Qt 感知产品实现。

Runtime 的职责：

- Widgets 和 Qt Quick 目标适配器；
- 具体 RFB 传输接入；
- 安全配置实现与校验；
- 应用 surface 自动发现和合成；
- 应用目标与输入路由；
- Runtime 组件工厂和服务；
- 跨模式诊断和运行行为；
- 自动性能策略，包括 viewer 感知采集需求、输入触发新鲜度和自适应节奏。

正常共享产物：

```text
HyRemote::RemoteAccess
HyRemoteRemoteAccess
```

不存在独立的 Widgets Runtime、Quick Runtime、Generic Runtime、QPA Runtime、Embedded Runtime 或 Accelerated Runtime 产品变体。

## 4. C++ API 前端

C++ API 是显式可编程入口：

```cmake
find_package(HyRemote CONFIG REQUIRED)
target_link_libraries(MyApp PRIVATE HyRemote::RemoteAccess)
```

```cpp
#include <HyRemote/RemoteAccess.h>

HyRemote::RemoteAccess remote(&window);
remote.start();
```

C++ 前端是共享 Runtime 的一层封装入口，不拥有其他前端使用的 Runtime 实现。

构造本身不产生副作用。应用在停止状态完成配置，然后显式调用 `start()`。

当前产品状态：并列路线。

## 5. QML API 前端

QML 前端是同一共享 Runtime 上的薄声明层：

```qml
import HyRemote

RemoteAccess {
    target: mainWindow
    enabled: true
}
```

它不会创建第二套 Session、传输、采集、状态、错误或性能模型。

当前产品状态：并列路线，使用同一共享 Runtime。

## 6. Generic Plugin 前端

Generic Plugin 使用 Qt 公共插件 API（`QGenericPlugin`）。它是零代码接入路线，应用原本的原生 Qt 平台继续负责窗口和输入。

```text
Qt application
    |
    +--> native Qt platform (qwindows/qxcb/...)
    |
    +--> HyRemote Generic Plugin -> Shared Runtime
```

应用本身保持纯 Qt。部署使用：

```cmake
hyremote_deploy(TARGET MyApp GENERIC)
```

通过 Qt 通用插件机制启动：

```text
MyApp -plugin hyremote
```

Generic 不继承 QPA 的 private ABI 要求。它的核心约束是应用的原生平台身份保持正常。

当前产品状态：并列路线。

## 7. QPA 前端

QPA 是专用零代码接入路径，用于需要进程级平台拦截的场景：

```text
MyApp -platform hyremote
```

QPA 使用 Factory Trampoline：

```text
qhyremote plugin
    -> QPlatformIntegrationFactory::create(native delegate)
    -> actual native QPlatformIntegration
    -> Shared Runtime attached alongside native behavior
```

它不重新实现完整平台，也不会静默替换成 offscreen、minimal 或 qvnc 风格后端。

当前参考 delegate：

- Windows：`qwindows`；
- Linux/X11：`qxcb`。

Qt private ABI 依赖只属于 QPA。资格认证按精确 Qt 补丁版本进行，当前参考是 Qt 6.8.3。

当前产品状态：并列路线，使用同一共享 Runtime。

## 8. Widgets 与 Qt Quick 目标适配器

Widgets 和 Qt Quick 都由共享 Runtime 处理。

### Widgets

受支持的 QWidget 目标使用 Widgets 适配器和 CPU 可读的正确性采集路径。

### Qt Quick

受支持的 `QQuickWindow` 目标使用 Quick 适配器和公共异步 Quick 采集路径。

应用无论使用哪种目标类型，都使用相同的产品入口，不直接选择采集后端类。

QOpenGLWidget、QQuickWidget、Quick3D、自定义 FBO 或特殊原生窗口归属等场景需要单独兼容性资格认证，不能从基础 Widgets/Quick 行为推断。

性能策略遵循同一规则：适配器暴露采集和 damage 能力，共享 Runtime 与传输决定如何使用这些能力。应用不选择内部采集或性能后端。

## 9. 应用 surface 自动模型

Generic 和 QPA 都需要自动访问应用 surface。这部分逻辑只放在 Runtime 中，不在两个前端各复制一份。

Runtime 自动访问负责：

- 发现受支持的应用顶层 surface；
- 在需要时把它们合成为远端应用画面；
- 把规范化远程输入路由到合适目标；
- 保持本地原生平台行为优先。

前端不得实现自己的竞争性合成或控制器栈。

## 10. 帧、新鲜度与背压模型

`RemoteFrame` 与传输无关，并拥有自己的存储生命周期。

帧契约携带：

- 尺寸和像素/存储格式；
- 自有存储生命周期；
- 内容时间；
- damage 信息；
- 后端无关能力。

帧在进入传输分发前经过有界 Core 交接。慢客户端不能造成无界帧保留，也不能阻塞 Qt GUI/渲染路径。

近实时策略只保留最新有效帧，不构建持续增长的队列。性能工作遵循 **新鲜度优先**：存在更新有效状态时，过期中间工作可以丢弃或合并。

Core 负责有界性和与传输无关的新鲜度语义。共享 Runtime 负责自动采集需求和节奏。RFB 传输负责压缩、增量更新状态、Continuous Updates/Fence 和每 viewer 网络交付行为。这些协议或平台机制不作为普通应用调优参数暴露。

详细模型见 [`performance-optimization.md`](performance-optimization.md)。

## 11. 输入模型

远程输入到达 Qt 目标适配器前会先规范化。

产品区分：

- 指针移动；
- 指针按键；
- 滚轮/滚动；
- 按键和修饰键；
- 协议信息足够时提交的文本。

输入作用域是应用，不是整个桌面。HyRemote 不把操作系统级虚拟 HID/uinput 注入作为正常路径。

断开连接或显式停止 Runtime 时会清理按住的键/按钮状态，避免远端把本地应用留在卡住状态。

远程输入也是性能信号：应用处理远程操作并有机会更新/渲染后，Runtime 可以优先安排一次新的采集/更新，使交互响应不只依赖周期采集定时器。

## 12. Runtime 状态

公开 Runtime 状态模型：

```text
Stopped -> Starting -> Running
                    \-> Faulted
Running  -> Faulted
Running/Faulted -> stop -> Stopped
```

`Running` 表示 Runtime/listener 已启动，不表示 viewer 已认证或已连接。

`connectedClientCount()` 等运行信息是诊断状态，不是授权身份。

采集活动不等于连接存活：静态连接会话可以在内容变化前不发送帧数据，同时 TCP/RFB 会话保持正常。

## 13. 传输边界

当前正确性传输是位于 Runtime/Core 私有边界后的有界 RFB 3.8。

RFB 不属于普通应用 API。后续传输可以复用相同的接入前端、Runtime 目标适配器、Core 帧模型、输入模型和新鲜度契约。

RFB 专用性能机制留在传输内部，包括：

- 编码协商和 Raw fallback；
- 实用压缩；
- 增量/非增量更新状态；
- 可互操作时的 Continuous Updates/Fence；
- 有界的每 viewer 交付流控；
- 有用时的 cursor pseudo-encoding。

当前安全行为：

- 默认监听 `0.0.0.0:5921`；
- 远程输入默认关闭；
- `Insecure` 未认证、未加密，仅限可信 LAN，不适合互联网；
- 认证配置可以使用 RFB VNC 认证；
- 当前基线没有实现流加密；
- 加密后端不可用时，`AuthenticatedEncrypted` 在打开 listener 前失败关闭。

## 14. 打包与部署

`hyremote_deploy()` 是唯一产品部署入口：

```cmake
hyremote_deploy(TARGET MyCppApp)
hyremote_deploy(TARGET MyQmlApp QML)
hyremote_deploy(TARGET ExistingQtApp GENERIC)
hyremote_deploy(TARGET ExistingQtApp QPA)
```

该 helper 负责 HyRemote payload 放置和 Runtime 闭包。应用不要手工复制内部库/插件，也不要把运行时搜索路径指回 SDK 或构建树。

Generic 部署同时携带 HyRemote Generic payload 和应用正常原生 Qt 平台插件。QPA 部署携带 `qhyremote` 及其精确原生 delegate 链。

## 15. 兼容性边界

当前桌面参考环境：

- Windows x86_64；
- Linux x86_64；
- Qt 6.8.3 参考 SDK。

公共 Qt 接入路径和 private QPA 兼容性是两类声明：

- C++、QML、Generic 主要依赖 Qt 公共 API；
- QPA 按精确 private ABI 做资格认证。

在某个操作系统、Qt 补丁、图形后端、应用类型或性能配置上可用，不自动等于另一个环境也已认证。

见 [`compatibility.md`](compatibility.md)。

## 16. 性能扩展点

架构把性能相关实现选择留在内部扩展点之后：

- 共享 Runtime 中的按需/自适应采集策略；
- 更高效的图形采集适配器；
- 外部或 GPU 后备帧；
- DMA-BUF/GBM 和嵌入式图形路径；
- RKMPP/VAAPI/D3D 硬件编码；
- 面向高动态负载的额外传输。

强制 GA 前性能基线由 [`performance-optimization.md`](performance-optimization.md) / #370 定义，包括实用压缩/增量交付和低延迟 Runtime/RFB 行为。硬件或厂商加速不自动成为强制项，只有在已接受测量显示可移植基线未达到声明的延迟/资源预算时才启用。

H.264/H.265/AV1 风格媒体交付是独立的端到端传输/客户端产品决策，不自动替代普通 RFB GUI 路径。

这些扩展必须保持普通产品原则：应用选择接入前端，不选择内部采集/传输/硬件实现。

## 17. 其他未来扩展点

更丰富的会话和应用控制 API 等能力同样留在内部扩展点之后。

这些能力必须保持一个 Core 和一个共享 Runtime，不能为不同接入前端制造并行的性能语义。
