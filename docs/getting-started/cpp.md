# C++ API 接入（Qt Widgets / Qt Quick）

> 语言 / Language：**中文** ｜ [English](../en/getting-started/cpp.md)

HyRemote 的参考接入方式是一个很小的 C++ 门面，以**一个共享库**交付。既有的 Qt Widgets 与 Qt Quick 应用只链接
`HyRemote::RemoteAccess`；它们**不**组装 Core 会话、采集源、传输、输入汇或 RFB 对象。

## 前置条件

V1 参考线是 **Qt 6.8.x**；自动化产品工作在 Windows x86_64 与 Linux x86_64 上针对 **Qt 6.8.3**。其它 Qt 版本不会因此
被暗示为受支持，除非记录在 [`compatibility.md`](../compatibility.md) 中。

二选一获取方式：

- 已安装 SDK：`find_package(HyRemote CONFIG REQUIRED)`；
- 源码 / vendored：`add_subdirectory(path/to/HyRemote hyremote)`。

两者暴露**同一个**应用目标：`HyRemote::RemoteAccess`。

## Widgets 最小用法

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)
find_package(HyRemote CONFIG REQUIRED)

target_link_libraries(MyApp PRIVATE
    Qt6::Widgets
    HyRemote::RemoteAccess
)
```

```cpp
#include <HyRemote/RemoteAccess.h>

MainWindow window;
window.show();

HyRemote::RemoteAccess remote(&window);
remote.start();
```

这就是正常基线：**构造是惰性的**，`start()` 才打开服务。默认地址是回环，默认端口是 **5921**，远程输入**默认关闭**。
该默认端口可在配置期由集成方通过 `-DHYREMOTE_DEFAULT_PORT=<port>` 选择（见安装指南），并且仍可用 `setPort()` 按进程覆盖。

只有在需要时才启用远程控制：

```cpp
HyRemote::RemoteAccess remote(&window);
remote.setRemoteInputEnabled(true);
remote.start();
```

## Qt Quick 最小用法

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Quick)
find_package(HyRemote CONFIG REQUIRED)

target_link_libraries(MyApp PRIVATE
    Qt6::Quick
    HyRemote::RemoteAccess
)
```

```cpp
QQuickWindow *window = /* top-level Quick window */;
HyRemote::RemoteAccess remote(window);
remote.start();
```

Widgets 与 Quick 共用**同一个**公开门面；采集/输入的实现选择属于内部细节。

## 可选配置

`setListenAddress()` **只接受数字地址**。默认是回环 `127.0.0.1`；不属于任何接口的地址、或已被占用的端口，会在到达
`Running` **之前**失败，并且不会留下任何监听；IPv6 通配地址 `::` 在本平台上是**仅 IPv6** 监听，而非双栈。逐地址的
实测表见 [`known-limitations.md`](../known-limitations.md#listener-address-family-and-reachability)。

配置在**停止状态**下进行：

- `setTarget(QObject *)`；
- `setListenAddress(const QHostAddress &)`；
- `setPort(quint16)`；
- `setRemoteInputEnabled(bool)`；
- `start()` / `stop()`；
- `state()` / `connectedClientCount()` / `lastError()` / `clearError()`。

一个非默认示例：

```cpp
HyRemote::RemoteAccess remote(&window);
remote.setPort(5901);
remote.setRemoteInputEnabled(true);

if (!remote.start()) {
    const auto error = remote.lastError();
    // 报告产品级错误。
}
```

正常应用**不会**去选择传输/后端/采集类。

## 部署

V1 的 C++ 运行期产物是共享库 `HyRemoteRemoteAccess`。Core 静态组合在它之后，所以用户**不需要**再部署第二个 HyRemote Core 运行时。

使用唯一那个包助手：

```cmake
install(TARGETS MyApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyApp)
```

该助手与 Qt 支持的部署工具链配合，并自动带上 HyRemote 的共享门面。应用**不应**按文件名拷贝 HyRemote 的库。

详见 [`guide/deployment.md`](../guide/deployment.md)。

## 安全基线

当前 RFB SecurityType None 正确性传输**未认证、未加密**，且在回环之外会被直接拒绝；配置了认证档时，查看端会先经 RFB VNC
认证，但数据流**仍未加密**。不要把它直接暴露到不可信网络。默认绑定回环，远程输入默认关闭。
见 [`security.md`](../security.md) 与 [`known-limitations.md`](../known-limitations.md)。

## 示例

- [`examples/widgets-basic`](../../examples/widgets-basic)
- [`examples/quick-basic`](../../examples/quick-basic)
- [`examples/remote-support-showcase`](../../examples/remote-support-showcase)

无头/offscreen 的 E2E 验证的是"协议到应用"的正确性；它**不能替代**物理本机显示/本地输入的并存证据。

## 下一步

- 平台准备：[`guide/install.md`](../guide/install.md)（中文）或 [`en/guide/install.md`](../en/guide/install.md)（English）；
- 查看端流程：[`guide/viewer-connection.md`](../guide/viewer-connection.md)；
- 部署：[`guide/deployment.md`](../guide/deployment.md)；
- 排错：[`guide/troubleshooting.md`](../guide/troubleshooting.md)；
- 精确支持状态：[`compatibility.md`](../compatibility.md) 与 [`known-limitations.md`](../known-limitations.md)。
