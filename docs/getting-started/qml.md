# QML API 接入（Preview）

> 语言 / Language：**中文** ｜ [English](../en/getting-started/qml.md)

QML API 是 HyRemote 面向 Qt Quick 应用的声明式接入方式。它是 Shared Runtime 之上的薄前端，不会创建第二套 Session、采集、传输、输入或安全实现。

当前产品状态：**Preview**。

## 适合谁

选择 QML API，如果你的应用：

- 主要使用 Qt Quick / QML；
- 希望通过声明式属性控制远程访问；
- 不希望为基础生命周期再写额外 C++ glue code。

如果你的 Quick 应用更希望通过 C++ 显式控制 HyRemote，也可以直接使用 [`cpp.md`](cpp.md) 中的 C++ API。

## 当前参考环境

当前参考环境：

- Windows x86_64；
- Linux x86_64；
- Qt 6.8.3。

QML API 目前按 Preview 提供；精确支持状态见 [`../compatibility.md`](../compatibility.md)。

> **TODO V0.3：** 完成 installed-SDK、部署、双语示例和完整产品资格后，将 QML API 提升为正式产品路径。

## 最小接入

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick)
find_package(HyRemote CONFIG REQUIRED)
```

```qml
import QtQuick
import QtQuick.Controls
import HyRemote

ApplicationWindow {
    id: window
    visible: true

    RemoteAccess {
        id: remote
        target: window
        enabled: true
    }
}
```

`enabled: true` 是显式启动请求。HyRemote 在 QML 组件完成构造后启动 Shared Runtime，因此不需要为了基础启动额外编写 `Component.onCompleted` glue code。

默认行为与 C++ API 一致：

- 默认监听 `0.0.0.0:5921`；
- 远程输入默认关闭；
- 未显式启用时不会启动 Runtime；
- 安全行为由 Shared Runtime 统一实现。

## 常用配置

```qml
RemoteAccess {
    id: remote
    target: window
    port: 5901
    remoteInputEnabled: true
    enabled: true
}
```

运行时配置遵循显式生命周期：

```text
disable -> 修改配置 -> enable
```

QML 前端不会偷偷建立第二个 Runtime，也不会暴露 RFB/socket 专有对象。

## 状态与连接数

`Running` 表示 Runtime/监听器已经运行，不代表已有查看端连接或已经认证。

```qml
Label {
    text: remote.state === RemoteAccess.Running
          ? "Listening · clients " + remote.connectedClientCount
          : "Stopped"
}
```

`connectedClientCount` 是运行状态信息，不是身份或授权数据。

## 只看与远程控制

默认是只看。需要时显式开启远程输入：

```qml
RemoteAccess {
    target: window
    remoteInputEnabled: true
    enabled: true
}
```

远端指针、按键和文本输入使用与其它 frontend 相同的归一化输入语义；Qt 自己的焦点与控件状态仍然是应用权威。

## 部署

```cmake
install(TARGETS MyQmlApp
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

hyremote_deploy(TARGET MyQmlApp QML)
```

QML payload 与 C++ API 共用一个 Shared Runtime。应用不需要手工复制 `qmldir`、内部插件或 Runtime 库。

如果所选 HyRemote SDK 不包含 QML payload，配置阶段应直接失败，而不是生成不完整部署。

详见 [`../guide/deployment.md`](../guide/deployment.md)。

## 与 QPA 组合

在确实需要 QPA platform-entry 行为时，可以同时部署 QML 与 QPA payload：

```cmake
hyremote_deploy(TARGET MyQmlApp QML QPA)
```

这只是两个 frontend payload 共用同一个 Shared Runtime，不会创建第四套 Runtime 架构。

## 安全边界

V0.1 的 Shared Runtime 采用回环优先策略：

- 默认绑定 `0.0.0.0`（本机全部 IPv4 接口），也可指定一个精确的本机 IPv4，或指定网卡；
- 远程输入默认关闭；
- 未认证非回环监听被拒绝；
- `Authenticated` 可使用 RFB VNC authentication，但数据流不加密；
- `AuthenticatedEncrypted` 在加密后端不可用时失败关闭。

不要把当前产品直接暴露到公网。详见 [`../security.md`](../security.md)。

## 示例

新的 Example 体系会提供独立的 Quick + QML 学习路径，并和 Quick + C++ 示例明确区分。

> **TODO V0.3：** 完成 `examples/learning/04-quick-qml` 的双语、品牌化正式示例。

## 下一步

- Quick + C++：[`cpp.md`](cpp.md)；
- Generic 零代码接入：[`generic.md`](generic.md)；
- QPA（Preview）：[`qpa-proxy.md`](qpa-proxy.md)；
- 部署：[`../guide/deployment.md`](../guide/deployment.md)；
- 查看器连接：[`../guide/viewer-connection.md`](../guide/viewer-connection.md)；
- 安全：[`../security.md`](../security.md)；
- 兼容矩阵：[`../compatibility.md`](../compatibility.md)；
- 已知限制：[`../known-limitations.md`](../known-limitations.md)。