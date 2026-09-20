# 查看端连接与远程控制

> 语言 / Language：**中文** ｜ [English](../en/guide/viewer-connection.md)

HyRemote V1 在三种集成方式背后使用**同一个**有界内部 RFB 正确性传输。协议后端不属于面向应用的 API。

## 启动目标应用

### Embedded C++

应用显式调用 `RemoteAccess::start()`。

### Declarative QML

应用通过 `enabled: true` 显式请求启动；封装层会在 QML 组件构造完成之后应用该请求。

### Transparent QPA

应用被有意地通过 `hyremote` 平台插件启动，例如：

```text
MyApp -platform hyremote
```

每种方式下的默认产品配置都监听回环端口 5921，并保持远程输入关闭。

## 连接

监听器究竟落在哪个接口上，是连接答案的一部分，所以连接之前先把这件事说清楚：默认是回环 `127.0.0.1`；V1 只接受**数字**地址（不接受主机名或 DNS 名）；逐地址的实测行为——包括 IPv6 通配地址 `::` 在这里是**仅 IPv6** 监听而非双栈——列在
[`known-limitations.md`](../known-limitations.md#listener-address-family-and-reachability)。非回环地址意味着**显式扩大**信任边界，选择之前请先看 [`security.md`](../security.md)。

在默认配置下，把标准 VNC/RFB 客户端指向：

```text
127.0.0.1:5921
```

查看端的写法各有不同。自动化的产品适配套件使用受维护的 `vncdotool` 作为互操作客户端；任何兼容性条目的结论都必须记录其所用查看端与版本的**精确值**。

## 只看 与 可控制

远程查看与远程输入是**两项独立策略**。安全的产品默认是只看。

Embedded C++ 在启动前显式开启控制：

```cpp
remote.setRemoteInputEnabled(true);
remote.start();
```

QML 在停止状态下暴露等价的 `remoteInputEnabled` 策略。

Transparent QPA 把"零源码改动"的策略放在启动配置里：

```text
-platform hyremote                         # 只看
-platform hyremote:hyremote-input=true     # 远程控制
```

不支持的按键/输入法行为**保持为已文档化的限制**，不会被静默近似。见 [`input-model.md`](../input-model.md) 与 [`known-limitations.md`](../known-limitations.md)。

## 断开与重连

查看端可以在**不重建**目标应用的前提下断开并重连。远端客户端生命周期由共享传输拥有；Qt 目标与本机应用各自独立继续运行。

`RemoteAccess::stop()` 拆除 Embedded C++/QML 的会话与监听器，并把门面回到 `Stopped`。Transparent QPA 则在受支持的 surface 变动与正常的查看端重连之间，保留它的**同一个应用会话**，直到插件/控制器生命周期结束。

查看端异常消失时，已识别到的远端按下状态会被平衡（不残留按住）。

## "在监听" 不等于 "已连接"

`RemoteAccessState::Running` 表示远端运行时/监听器正在运行，**不**等价于"有查看端连着"。

当前产品门面/QML 封装暴露后端中立的 `connectedClientCount()` 诊断值。E1/E2/E3/E5 的产品适配使用期望的 `0 → 1 → 0 → 1 → 0` 生命周期覆盖连接、断开与重连。

Transparent QPA **不会**为了让一个本来未改动的应用镜像这个值，而额外暴露第二套应用诊断 API。

## 本机与远端并存

无头/offscreen/Xvfb 的查看端测试只证明它实际执行到的那条路径。V1 还要求：在集成方式声明了并存能力的场合，远端查看端工作期间，**物理本机的显示与输入保持可用**。

跨模式的物理证据与标准查看端互操作是**两件不同的事**，前者不由无头测试证明。

## 安全边界

当前 RFB 正确性基线使用 **SecurityType None**：既无传输认证，也无传输加密。它适用于回环/受信测试，不适用于直接暴露在不可信网络。

在把绑定地址从回环改走之前，请先读 [`security.md`](../security.md) 了解 V1 已实现的安全边界。[`security-model.md`](../security-model.md) 是更宏观的未来威胁模型背景，**不代表**认证/加密已经存在。
