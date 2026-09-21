# 查看端连接与远程控制

> 语言 / Language：**中文** ｜ [English](../en/guide/viewer-connection.md)

HyRemote 使用同一个 Shared Runtime 和 RFB 传输为四种 integration frontend 提供远程查看/输入能力。查看器不需要知道应用选择了 C++、QML、Generic 还是 QPA。

## 启动目标应用

### C++ API

应用显式调用：

```cpp
remote.start();
```

### Generic Plugin

保持应用 Qt-only，通过 Qt generic-plugin 机制启动 HyRemote：

```text
MyApp -plugin hyremote
```

### QML API（Preview）

```qml
RemoteAccess {
    target: mainWindow
    enabled: true
}
```

### QPA（Preview）

```text
MyApp -platform hyremote
```

四种入口最终进入同一个 Shared Runtime，因此默认网络与输入策略一致。

## 默认连接地址

V0.1 默认监听：

```text
127.0.0.1:5921
```

将标准 VNC/RFB 查看端连接到该地址即可。

HyRemote 当前使用数字 IP 地址。改变为非回环地址会扩大网络信任边界；在修改前请先阅读 [`../security.md`](../security.md)。

地址族与平台差异见 [`../known-limitations.md`](../known-limitations.md)。

## 只看与远程控制

远程查看和远程输入是两项独立能力。默认是**只看**。

### C++ API

```cpp
remote.setRemoteInputEnabled(true);
remote.start();
```

### QML API

```qml
RemoteAccess {
    target: mainWindow
    remoteInputEnabled: true
    enabled: true
}
```

### Generic Plugin

通过 Generic Plugin specification 启用输入，例如：

```text
MyApp -plugin "hyremote:input=true"
```

多项配置写在冒号后的 specification 中，并使用分号分隔，详见 [`../getting-started/generic.md`](../getting-started/generic.md)。

### QPA

```text
MyApp -platform "hyremote:hyremote-input=true"
```

如果不需要远程控制，请保持默认只看模式。

## 断开与重连

正常查看端断开不需要重启 Qt 应用。

典型生命周期：

```text
application running
    ↓
viewer connects
    ↓
viewer disconnects
    ↓
application keeps running
    ↓
viewer reconnects
```

Shared Runtime 负责远端客户端生命周期；Qt 应用和本机 UI 继续独立运行。

对于 C++/QML，显式 `stop()`/disable 会关闭 Runtime 与监听器。Generic/QPA 则遵循其插件生命周期。

## “Running” 不等于“已连接”

`RemoteAccessState::Running` 表示 Runtime/监听器已经运行，不代表查看端已连接或已认证。

C++/QML 可通过：

```text
connectedClientCount()
```

观察当前连接数量。

该值只代表运行状态，不代表用户身份、角色或授权。

Generic/QPA 作为零代码路径不会为了暴露该值而给原应用增加一套新的业务 API。

## 多窗口

支持的应用顶层 surface 可以在一个逻辑远程会话中出现和消失，而不是为每个窗口建立一个独立监听器。

这不代表任意 native/foreign OS window 都自动受支持。复杂多窗口和特殊图形场景的准确范围见 [`../compatibility.md`](../compatibility.md)。

## 查看器兼容性

HyRemote 使用标准 RFB/VNC 协议作为当前 transport baseline。

不同查看器在快捷键、剪贴板、缩放和输入法行为上可能有差异。只有明确进入兼容矩阵的查看器/版本才构成正式互操作声明。

> **TODO V0.3/V0.4：** 完成正式 Viewer interoperability matrix，并覆盖至少两个维护中的真实 VNC Viewer。

## 本机与远端并存

HyRemote 的产品目标是：远程访问不破坏本机 Qt 显示与本地输入。

Generic 明确保留原生 Qt platform；QPA 通过 Factory Trampoline 委托给原生 platform integration。

Headless/offscreen 运行不能单独证明所有物理显示/键鼠组合都已经资格化。

> **TODO V0.4：** 完成最终 Windows/Linux 物理本机显示 + 本地输入 + 远程访问并存资格。

## 安全边界

V0.1：

- 默认回环；
- 远程输入默认关闭；
- 未认证非回环监听被拒绝；
- `Authenticated` 可使用 RFB VNC authentication；
- 当前数据流不加密；
- `AuthenticatedEncrypted` 在加密 backend 不可用时失败关闭。

不要把当前产品直接暴露到公网。详见 [`../security.md`](../security.md)。

## 相关文档

- C++ API：[`../getting-started/cpp.md`](../getting-started/cpp.md)
- Generic Plugin：[`../getting-started/generic.md`](../getting-started/generic.md)
- QML API：[`../getting-started/qml.md`](../getting-started/qml.md)
- QPA：[`../getting-started/qpa-proxy.md`](../getting-started/qpa-proxy.md)
- 部署：[`deployment.md`](deployment.md)
- 安全：[`../security.md`](../security.md)
- 兼容性：[`../compatibility.md`](../compatibility.md)
- 已知限制：[`../known-limitations.md`](../known-limitations.md)