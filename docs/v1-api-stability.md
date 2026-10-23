# HyRemote V1 公共 API 稳定性契约

> 语言 / Language：**中文** ｜ [English](en/v1-api-stability.md)

本文定义 **HyRemote V1 GA 面向应用的预期稳定性契约**。

这是产品契约，不是发布检查清单。`V1.0.0.0` 之前，某项能力仍可在产品文档中描述为未完成工作。只有发布的兼容矩阵和发布包声明对应界面稳定时，V1 GA 才开始。

核心规则：

> HyRemote 可以演进 Core、Runtime、采集、传输、输入、平台和加速内部实现，不强迫普通应用采用新的接入模型。

## 1. 稳定产品模型

V1 保持一个共享 Runtime 和四个并列接入前端：

```text
C++ API ---------\
QML API ----------\
Generic Plugin ----> Shared Runtime -> Core
QPA --------------/
```

Qt Widgets 和 Qt Quick 是 Runtime 目标类型，不是独立产品版本。

版本号不编码前端、Qt 版本、UI 技术、操作系统或传输后端。

## 2. C++ API 稳定性

普通 C++ 应用契约：

```cmake
find_package(HyRemote CONFIG REQUIRED)
target_link_libraries(MyApp PRIVATE HyRemote::RemoteAccess)
```

公共头文件：

```cpp
#include <HyRemote/RemoteAccess.h>
```

典型用法：

```cpp
HyRemote::RemoteAccess remote(&window);
remote.start();
```

V1 中，`HyRemote::RemoteAccess` 是合格 Qt Widgets 和 Qt Quick 目标的稳定可编程入口。

V1 公共 C++ 面包括 V1 GA 时明确安装为公共 API 的类型和方法。当前候选类型：

- `HyRemote::RemoteAccess`；
- `HyRemote::RemoteAccessState`；
- `HyRemote::RemoteAccessErrorCode`；
- `HyRemote::RemoteAccessError`。

计划在 V1 稳定的行为不变量：

- 构造无副作用，不打开 listener；
- `start()`/`stop()` 是显式生命周期操作；
- 配置在停止状态完成；
- 默认 listener 地址和远程输入策略以当前产品文档和兼容矩阵为准；远程输入默认关闭；
- 远程查看和远程控制策略保持独立；
- `Running` 表示 Runtime/listener 已启动，不表示 viewer 已认证或已连接；
- 应用可见错误保持 HyRemote 产品类型，不暴露后端/协议类型；
- 不可恢复 Runtime 失败在 owner 调用 stop() 前保持 Faulted 可观察；
- 确认错误不会复活产生该错误的会话：无关 viewer、采集或传输活动不得让同一已确认失败重新变为当前；
- 同一条件在确认后再次发生时，后续发生必须再次可见；
- 传输、采集、输入和加速实现可以变化，不要求普通应用源码改变。

`RemoteAccess` 保持不可复制、可移动。私有实现边界防止后端/会话/平台类型泄漏到安装头文件。

> **V1 GA 前 TODO：** 从已接受的 V1 头文件冻结最终公共方法/属性集和 ABI 策略。

## 3. QML API 稳定性

声明式产品入口：

```qml
import HyRemote

RemoteAccess {
    target: mainWindow
    enabled: true
}
```

QML 前端是 C++ API 使用的同一共享 Runtime 上的薄层。

预期 V1 稳定 QML 契约：

- URI `HyRemote`；
- 公共 `RemoteAccess` 类型；
- V1 包文档化的生命周期、目标、listener、输入策略、状态、诊断和错误属性；
- 行为与 C++ Runtime 对齐，不建立第二套 QML 专用 Session/transport/input 实现。

QML 类型背后的 C++ 实现类不是消费者 API。

当前产品状态：并列路线，使用同一共享 Runtime。

> **当前产品线未完成：** 在声明 QML 契约稳定前，完成最终 QML API 面、安装 SDK 示例和兼容资格。

## 4. Generic Plugin 稳定性

应用能保留正常 Qt 平台接入时，Generic Plugin 是首选零代码路线。

应用源码保持纯 Qt。部署和激活：

```cmake
hyremote_deploy(TARGET MyApp GENERIC)
```

```text
MyApp -plugin hyremote
```

V1 高层契约预期保持：

- 不依赖 HyRemote 应用链接；
- 使用 Qt 公共 generic-plugin 机制；
- 保留应用原生平台身份；
- 插件背后仍是一个共享 Runtime；
- 通过文档化插件配置字段完成产品配置；
- 不因为使用 Generic 就依赖 QPA private ABI。

精确内部插件类、自动 surface 控制器和组合实现不是稳定应用 API。

## 5. QPA 稳定性类别

QPA 是专用零代码接入路径，不是通用 Qt private-ABI 稳定承诺。

面向应用的工作流：

```cmake
hyremote_deploy(TARGET MyApp QPA)
```

```text
MyApp -platform hyremote
```

预期 V1 高层承诺：

- `-platform hyremote` 保持产品启动概念；
- HyRemote 自有 QPA 参数保持文档化含义；
- 前端把正常平台行为委托给合格原生 Qt 平台实现；
- 普通现有 Qt 应用不链接 HyRemote QPA 目标；
- QPA 前端和其他前端使用同一共享 Runtime。

QPA 本身使用 Qt private ABI，因此二进制兼容按**精确 Qt 补丁/平台组合**认证。当前参考是 Qt 6.8.3，Windows x86_64 和 Linux x86_64。

私有 QPA 类、原生句柄、控制器/组合/provider 类型和 Qt private 头文件不属于 HyRemote 稳定应用 API。

当前产品状态：并列路线，使用同一共享 Runtime。

> **当前产品线未完成：** 在把 QPA 行加入最终 V1 支持矩阵前，完成 QPA 产品化和精确版本资格。

## 6. 安装 CMake/包稳定性

面向应用的包契约以以下内容为中心：

```cmake
find_package(HyRemote CONFIG REQUIRED)
HyRemote::RemoteAccess
hyremote_deploy(...)
```

文档化部署形式：

```cmake
hyremote_deploy(TARGET MyCppApp)
hyremote_deploy(TARGET MyQmlApp QML)
hyremote_deploy(TARGET ExistingQtApp GENERIC)
hyremote_deploy(TARGET ExistingQtApp QPA)
hyremote_deploy(TARGET ExistingQmlApp QML QPA)
```

部署 helper 负责 HyRemote Runtime/前端放置。普通应用不应按文件名查找或复制 `HyRemoteRemoteAccess`、QML 插件、`qmldir`、Core、传输库、Generic 插件二进制或 `qhyremote`。

Generic 和 QPA 是并列零代码策略，不合并成同一产品模式。

安装包元数据可以增量增长，但文档化含义不能在 V1 线内静默不兼容地重定义。

## 7. Runtime 与制品稳定性

正常 V1 C++ Runtime 制品仍是一个共享产品库：

```text
HyRemote::RemoteAccess
HyRemoteRemoteAccess
```

Core 是内部组合，不是第二个普通 SDK Runtime 目标。

QML、Generic 和 QPA payload 是这个 Runtime 上的前端，不是并行 Runtime 变体。

未来实现可以为了部署大小或平台原因物理切片私有可选组件，前提是公共产品仍表现为一个逻辑 Runtime，且不要求普通应用选择内部采集/传输/硬件栈。

## 8. 明确不稳定的实现面

以下不是普通应用兼容面：

- Core `Session` 和组合内部；
- `RemoteFrame` 存储实现；
- `CaptureSource`、`InputSink` 和具体 `Transport` 实现；
- RFB 协议实现类；
- Widgets/Quick 目标适配器类；
- 自动接入组合/控制器实现；
- QPA 私有类和原生句柄；
- 测试客户端、product-fit fixture 和验收 harness；
- 实验性图形/嵌入式/加速后端；
- DMA-BUF/GBM/RKMPP/VAAPI/D3D 实现细节。

只要文档化应用契约和兼容承诺不变，这些可以演进。

## 9. V1.x 内演进

V1 代际内，稳定应用面的变化应保持源码兼容和行为兼容，除非新的主产品代际明确引入破坏性契约。

正常可接受的 V1.x 变化：

- 调用方可以安全容忍扩展时增加枚举值；
- 增加只读诊断；
- 可选能力和重载；
- 支持更多合格 Qt/平台行；
- 私有性能或传输改进；
- 保持文档化消费者用法的打包改进。

正常 V1.x 不做：

- 删除或改变既有公共 API 用途；
- 把零代码路径变成必需应用链接依赖；
- 把后端/协议对象暴露为必需业务概念；
- 用前端专用公共 Runtime 变体替代一个共享 Runtime；
- 没有精确资格就扩大 QPA private-ABI 兼容。

## 10. 兼容性声明

本文定义**什么会稳定**，不定义**在哪里支持**。

实际支持由 [`compatibility.md`](compatibility.md) 定义。公共 API 可以稳定，而某个 Qt/OS/图形组合仍是 Preview、Unverified 或 Unsupported。

Windows 证据不表示 Linux 支持；Qt 公共 API 兼容不表示 QPA private-ABI 兼容；桌面资格不表示嵌入式支持。

## 11. V1 GA 完成项

在本文可作为最终 V1 GA 冻结前，仍有产品工作：

- **TODO：** 冻结精确安装 C++ API/ABI 面；
- **TODO：** 资格完成后把最终 QML 契约从 Preview 提升；
- **TODO：** 冻结计划长期支持的 Generic Plugin 配置字段；
- **TODO：** 为每个受支持精确 Qt 行冻结文档化 QPA 启动/配置面；
- **TODO：** 冻结 V1 `hyremote_deploy()` 参数契约和安装包元数据；
- **TODO：** 发布最终 V1 兼容/支持矩阵。

在此之前，当前产品状态由 V0.x 文档和兼容矩阵定义，不假设每个未来 V1 承诺已经 GA 稳定。
