# Generic Plugin 接入（零业务代码改动）

> 语言 / Language：**中文** ｜ [English](../en/getting-started/generic.md)

Generic Plugin 是四个 peer 前端之一，并且与 Embedded C++ 一起构成 V0.1 的两个**主要（primary）**接入面。它是一个公开的
Qt **generic** 插件：你的应用保持为一个普通 Qt 应用，不链接任何 HyRemote 目标，运行期通过 Qt 自己的插件机制获得远程接入。

它的决定性属性是**不替换应用的 Qt 平台集成**：Windows 仍是 `qwindows`，Linux/X11 仍是 `xcb`，HyRemote 只是把同一个
Shared Runtime 挂在原生平台路径旁边。

## 这个前端给你什么、不给你什么

- 给你：安装 SDK 之后，一个**已经存在的** Qt Widgets 或 Qt Quick 应用可以在**不改业务源码**的情况下变得可远程查看；
- 不给你：应用代码里没有 HyRemote 头文件、没有 HyRemote API 调用、也没有 HyRemote 链接依赖；应用只是被部署成带有
  Generic 载荷的形态。

## 什么时候用它

当你希望：既有应用源码尽量不动；应用继续以自己的平台身份运行；集成在**部署/运行期**完成而不是编译期。适合普通 Qt
Widgets / Qt Quick 应用。

若你愿意改应用代码并使用 C++ 门面，请改用 [Embedded C++](cpp.md) —— 那是另一条 V0.1 主要接入面。

## 接入步骤

应用本身不需要任何 HyRemote 代码，只需要在**构建/打包**时使用安装好的包与部署助手：

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets)
find_package(HyRemote CONFIG REQUIRED)

add_executable(MyApp main.cpp)
target_link_libraries(MyApp PRIVATE Qt6::Widgets)   # 不链接 HyRemote 目标

install(TARGETS MyApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyApp GENERIC)
```

`find_package(HyRemote CONFIG REQUIRED)` 与 `hyremote_deploy(...)` 是**打包/部署**集成，不是应用链接依赖：应用目标的
`target_link_libraries` 里不应出现 `HyRemote::RemoteAccess`、HyRemote QML/QPA 目标或任何内部 runtime/core 目标。

## 运行

普通启动（没有任何 HyRemote 行为）：

```text
MyApp
```

Generic 激活（**同一个可执行文件**，无需重新编译）：

```text
MyApp -plugin hyremote
MyApp -plugin hyremote:port=5921
```

用任意 RFB 3.8 viewer 连接该端口即可看到这个应用窗口：

```sh
vncviewer 127.0.0.1:5921
```

## 部署产物

`hyremote_deploy(TARGET MyApp GENERIC)` 之后，部署树包含：

- 应用可执行文件；
- Generic 载荷，位于 Qt 的 generic plugin 目录（`plugins/generic/`）——**具体的载荷文件名由已安装包元数据决定，不要在脚本或文档里硬编码**；
- **native** Qt 平台插件，位于 `plugins/platforms/`；
- 唯一的共享 `RemoteAccess` 运行时（所有前端共用同一个）；
- 该应用实际需要的 Qt 运行期闭包。

Generic 部署**不会**安装任何 HyRemote 平台插件：如果部署树里出现 `plugins/platforms/*hyremote*`，那是缺陷——Generic 保持
原生平台集成，而 Transparent QPA 才是替换它的那个前端。

更完整的部署说明见 [`../guide/deployment.md`](../guide/deployment.md)。

## V0.1 边界

- 参考矩阵：**Windows x86_64** 与 **Linux x86_64**，**Qt 6.8.3**；
- V0.2 是 **LAN-capable Developer Preview**：监听默认在本机全部 IPv4 接口上可达，远程输入默认关闭，需要显式启用；
- **没有**可用的加密档：`AuthenticatedEncrypted` 在 V0.1 不可用，且在开始监听前就会以 SecurityUnavailable 失败，不做降级；
- VeNCrypt/TLS 属于 **V0.2**，由 #143 跟踪，**不是** V1.0 专属工作；
- 不要把 V0.1 部署描述成 production ready、authenticated、encrypted 或 GA。详见
  [`../known-limitations.md`](../known-limitations.md) 与 [`../security-model.md`](../security-model.md)。
