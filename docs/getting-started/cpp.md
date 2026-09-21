# C++ API 接入（Qt Widgets / Qt Quick）

> 语言 / Language：**中文** ｜ [English](../en/getting-started/cpp.md)

C++ API 是 HyRemote V0.1 的主接入方式之一。应用链接一个公开目标 `HyRemote::RemoteAccess`，即可为 Qt Widgets 或 Qt Quick 顶层窗口增加远程查看和可选远程控制能力。

应用不需要了解 Core、Session、RFB、采集后端或输入后端。

## 适合谁

选择 C++ API，如果你的应用：

- 可以修改少量 C++ 代码；
- 希望显式控制 start/stop、目标窗口、监听地址或输入策略；
- 使用 Qt Widgets、Qt Quick，或两者混合；
- 希望以后接入更丰富的可编程策略。

如果你希望**不修改应用业务代码**，优先查看 [`generic.md`](generic.md)。

## 当前参考环境

V0.1 当前参考环境：

- Windows x86_64；
- Linux x86_64；
- Qt 6.8.3。

其它 Qt 版本的准确状态见 [`../compatibility.md`](../compatibility.md)。

> **TODO V0.4：** 完成 Qt 5.15 LTS 等计划兼容线的正式资格化后再扩大支持声明。

## Qt Widgets 最小接入

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

默认行为：

- 构造 `RemoteAccess` 不会自动打开监听器；
- `start()` 显式启动 Runtime；
- 默认监听 `127.0.0.1:5921`；
- 远程输入默认关闭。

## Qt Quick 最小接入

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

Widgets 与 Qt Quick 使用同一个公开 API 和同一个 Shared Runtime。应用不需要选择不同的 Runtime 产品。

## 启用远程控制

默认是只看。只有在业务确实需要时才启用远程输入：

```cpp
HyRemote::RemoteAccess remote(&window);
remote.setRemoteInputEnabled(true);
remote.start();
```

远程输入只作用于绑定的 Qt 应用目标，不是操作系统级桌面输入注入。

## 常用配置

配置应在 Runtime 停止状态下修改：

- `setTarget(QObject *)`；
- `setListenAddress(const QHostAddress &)`；
- `setPort(quint16)`；
- `setRemoteInputEnabled(bool)`；
- `start()` / `stop()`；
- `state()`；
- `connectedClientCount()`；
- `lastError()` / `clearError()`。

示例：

```cpp
HyRemote::RemoteAccess remote(&window);
remote.setPort(5901);
remote.setRemoteInputEnabled(true);

if (!remote.start()) {
    const auto error = remote.lastError();
    // 在应用自己的日志/UI 中处理产品级错误。
}
```

监听地址使用数字 IP。更改为非回环地址会扩大网络信任边界；请同时阅读 [`../security.md`](../security.md)。

## Runtime 状态

典型状态：

```text
Stopped -> Starting -> Running
                    \-> Faulted
Running/Faulted -> stop -> Stopped
```

`Running` 表示 Runtime/监听器已经运行，不代表已有查看端连接或已经完成身份认证。

`connectedClientCount()` 用于观察当前连接数量，不代表用户身份或授权角色。

## 部署

使用统一部署助手：

```cmake
install(TARGETS MyApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyApp)
```

`hyremote_deploy()` 负责 Shared Runtime 及其运行时依赖。应用不应按文件名手工复制 HyRemote 内部库。

详见 [`../guide/deployment.md`](../guide/deployment.md)。

## 安全边界

V0.1 采用回环优先策略：

- 默认绑定 `127.0.0.1`；
- 远程输入默认关闭；
- 未认证非回环监听被拒绝；
- `Authenticated` 可使用 RFB VNC authentication，但数据流当前不加密；
- `AuthenticatedEncrypted` 在加密后端不可用时失败关闭，不会打开监听器。

不要把 V0.1 直接暴露到公网。详见 [`../security.md`](../security.md)。

## 示例

新的 Example 体系按“从 0 到真正用起来”的用户旅程提供 C++ Widgets 和 C++ Quick 两条入门路径。

> **TODO V0.1：** 完成 `examples/learning/01-widgets-cpp` 与 `examples/learning/02-quick-cpp` 的双语、品牌化正式示例。

## 下一步

- 零代码接入：[`generic.md`](generic.md)；
- QML API（Preview）：[`qml.md`](qml.md)；
- 部署：[`../guide/deployment.md`](../guide/deployment.md)；
- 查看器连接：[`../guide/viewer-connection.md`](../guide/viewer-connection.md)；
- 排错：[`../guide/troubleshooting.md`](../guide/troubleshooting.md)；
- 兼容矩阵：[`../compatibility.md`](../compatibility.md)；
- 已知限制：[`../known-limitations.md`](../known-limitations.md)。