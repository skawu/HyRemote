# QML API 接入

> 语言 / Language：**中文** ｜ [English](../en/getting-started/qml.md)

HyRemote 的 QML API 方式，是 C++ 所使用的**同一份**共享 `HyRemote::RemoteAccess` 运行时之上的薄声明层。
它**不会**创建第二套 Session、采集、传输或输入栈。

## 前置条件

V1 参考线是 Windows x86_64 与 Linux x86_64 上的 **Qt 6.8.3**。

QML 应用先解析它原本的 Qt 模块，再解析 HyRemote 包：

```cmake
find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Core Gui Qml Quick)
find_package(HyRemote CONFIG REQUIRED)
```

`find_package(HyRemote)` 本身**不会**把无关的 Widgets/Quick/QML 开发组件强加给一个纯 C++ 消费者；应用自己选择它已经在用的 Qt UI 栈。

## QML 最小用法

正常的声明式启动刻意只涉及两个属性：

```qml
import QtQuick
import QtQuick.Controls
import HyRemote

ApplicationWindow {
    id: window
    visible: true

    RemoteAccess {
        target: window
        enabled: true
    }
}
```

`enabled: true` 是一个**请求**，不是构造副作用。HyRemote 会等到 QML 组件构造完成之后才启动共享运行时，因此初始属性
声明顺序**不需要** `Component.onCompleted` 之类的胶水代码。如果启动失败，`enabled` 会回滚为 `false`，并由产品级
错误属性描述失败原因。

安全默认值与 C++ 一致：

- 回环监听；
- 端口 5921；
- 远程输入关闭；
- 必须显式 `enabled: true`；
- 当前 RFB 正确性基线默认 SecurityType None（未认证、未加密，且回环之外被拒绝）；配置认证档后增加 RFB VNC 认证，但仍不加密。

## 可选配置

只设置你确实需要改变的值：

```qml
RemoteAccess {
    target: window
    port: 5901
    remoteInputEnabled: true
    enabled: true
}
```

配置属于**停止状态**。要在运行中改变监听/输入策略，请先禁用、更新属性、再重新启用。HyRemote **不会**静默创建第二套
运行时，也没有隐藏的重启路径。

## 连接状态

`Running` 表示监听器/运行时是活跃的，**不**表示已有查看端连接：

```qml
Label {
    text: remote.state === RemoteAccess.Running
          ? "Listening · clients " + remote.connectedClientCount
          : "Stopped"
}
```

`connectedClientCount` 是只读且后端中立的。E3 的产品适配要求真实查看端生命周期 `0 → 1 → 0 → 1 → 0`（连接、断开、
重连，且**不重建**应用）。

## 只看 与 远程控制

默认是**只看**。只有在你确实需要时才开启远程控制：

```qml
RemoteAccess {
    target: window
    remoteInputEnabled: true
    enabled: true
}
```

远端指针、按键与已提交文本事件走的是与 C++ **同一条**归一化输入路径。Qt 的正常焦点保持权威；QML 里**不会**出现
RFB 专有的按键/socket 对象。

## 部署

正常安装应用，并使用唯一那个 HyRemote 助手：

```cmake
install(TARGETS MyQmlApp
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

hyremote_deploy(TARGET MyQmlApp QML)
```

QML 模块是**导入载荷**，不是第二个 C++ SDK 目标。应用开发者不链接 `HyRemote::Qml` 目标，也不需要手工拷贝后端库、
插件、`qmldir`、共享门面或传输文件。

当应用有意把声明式 API 与 QPA 打包组合时，可用 `QML QPA`：

```cmake
hyremote_deploy(TARGET MyQmlApp QML QPA)
```

这仍然复用**同一份**共享运行时，不构成第四套集成架构。

## 查看端流程

在默认配置下，用标准 RFB/VNC 查看端连接 `127.0.0.1:5921`。关闭查看端并重连，**无需重启** Qt 应用：监听器保持活跃，
`connectedClientCount` 在两次客户端之间回到零。

查看端行为见 [`guide/viewer-connection.md`](../guide/viewer-connection.md)。

## 安全边界

当前正确性基线在未配置认证档时协商 RFB SecurityType None；配置了认证档则用 RFB VNC 认证对查看端做认证。不要把它直接暴露到
不可信/公网网络。"只看"是**输入策略**，不是认证；而且数据流**未加密**。

见 [`security.md`](../security.md)。

## 本机与远端并存

无头 offscreen/软件渲染的 E2E 证明了"查看端到 QML"的产品路径，但**不能**证明物理显示器与本地键鼠同时可用。
跨模式的物理并存证据与标准查看端互操作是两件不同的事。

## 相关文档

- 部署：[`guide/deployment.md`](../guide/deployment.md)；
- Windows / Linux 平台准备：[`guide/install.md`](../guide/install.md)；
- 查看端连接：[`guide/viewer-connection.md`](../guide/viewer-connection.md)；
- 安全：[`security.md`](../security.md)；
- 排错：[`guide/troubleshooting.md`](../guide/troubleshooting.md)；
- 兼容矩阵：[`compatibility.md`](../compatibility.md)；
- 已知限制：[`known-limitations.md`](../known-limitations.md)。
