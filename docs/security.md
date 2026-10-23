# HyRemote 安全边界

> 语言 / Language：**中文** ｜ [English](en/security.md)

HyRemote 是远程访问基础设施。viewer 能连上，不等于部署本身已经安全。

本文描述当前产品的实际安全行为。尚未实现的能力会直接标出，不写成已经可用。

## 1. 当前安全边界

共享 Runtime 对 C++ API、QML API、Generic Plugin 和 QPA 使用同一套安全模型。

默认行为：

- 绑定地址：`0.0.0.0`，即主机所有 IPv4 接口，不只是 loopback；
- 端口：`5921`；
- 远程输入：关闭；
- 构造 C++ `HyRemote::RemoteAccess` 不会打开 listener；
- QML 只有请求 `enabled: true` 后才启动；
- Generic/QPA 只有通过各自的 Qt 启动机制显式激活后才启动；
- `Insecure` 未认证、未加密，只面向可信 LAN，不适合暴露到 Internet；
- 畸形或不完整的客户端仍受传输层有界限制；
- 密钥不得写入普通诊断。

默认构建里有安全配置枚举，不代表认证或加密传输已经可用。

不要把产品直接暴露到公网。

## 2. 安全配置

### Insecure

`Insecure` 是默认开发配置。

- 不做 viewer 认证；
- 不做传输加密；
- 只应部署在可信 LAN；
- 远程输入仍是单独的策略。

`Insecure` listener 在绑定范围内可达。HyRemote 直接说明这一点，不通过缩小绑定地址来制造安全假象。

### Authenticated

`Authenticated` 是条件能力，只有以下条件同时成立才可用：

1. 构建包含传输安全能力；
2. 配置了安全描述文件路径；
3. 描述文件对所选配置有效。

条件满足时，当前传输使用 RFB VNC 认证完成 viewer 认证。

传输仍然**不加密**。

构建能力缺失时，`start()` 会在创建 target、transport 或 listener 之前以 `SecurityUnavailable` 失败。描述文件缺失或无效时，启动失败，不会降级到更弱的配置。

因此，默认开发构建应视为未认证、未加密。

### AuthenticatedEncrypted

`AuthenticatedEncrypted` 已在产品模型中声明，但**当前产品线没有实现**。

选择它会失败关闭，即使构建可以提供 `Authenticated` VNC 认证：

```text
AuthenticatedEncrypted
  -> SecurityUnavailable
  -> no target/transport/listener composed
  -> no fallback to Authenticated
  -> no fallback to Insecure
```

证书或私钥描述文件本身不会让加密传输变得可用。

> **待办：** 加密传输配置、证书/私钥策略、协议与密码套件策略，以及认证加密会话，属于后续产品工作。

## 3. 查看与控制是两件事

查看和控制是两个独立策略。

C++ 示例：

```cpp
HyRemote::RemoteAccess remote(&window);
remote.setRemoteInputEnabled(true);
remote.start();
```

QML 示例：

```qml
RemoteAccess {
    target: mainWindow
    remoteInputEnabled: true
    enabled: true
}
```

Generic 和 QPA 通过各自的插件启动配置提供对应选项。

远程输入关闭时，会话是只读的。只读既不等于认证，也不等于加密。

## 4. 监听范围

默认 listener 是：

```text
0.0.0.0:5921
```

修改绑定地址会改变网络信任边界，不会自动启用认证或加密。

`Insecure` 在绑定范围内可达。`Authenticated` 只有在认证构建能力和描述文件有效时才允许非 loopback listener。

地址族行为见 [`known-limitations.md`](known-limitations.md)。

## 5. 连接状态不等于授权

`RemoteAccessState::Running` 表示 Runtime/listener 已启动，不表示 viewer 已认证或已连接。

`connectedClientCount()` 是运行信息，不是用户身份、角色或授权结果。

> **待办：** 认证 Session Registry、有界准入策略、会话事件和显式会话终止 API 属于后续产品工作。

## 6. 输入安全

远程输入会规范化，并且只路由到所附着的 Qt 应用目标。HyRemote 不把操作系统级虚拟 HID/uinput 注入作为正常产品路径。

受支持的按住键/按钮状态会在 viewer 断开和 Runtime 停止时清理，避免远端把本地应用留在按下状态。

不支持的按键、输入法和组合场景会明确记录，不做猜测。

## 7. 资源边界

RFB 基线对并发客户端、协议缓冲、握手时限、帧交接和 GUI 输入交付等使用有界行为。

这些限制降低意外或恶意资源增长的风险，不代表当前传输适合直接面对公网。

## 8. 密钥与诊断

密码、token、私钥和同类密钥不得写入普通日志、错误字符串、UI 诊断或命令示例。

安全配置应使用外部描述文件或配置机制，不要把密钥直接放在进程命令行上。

描述文件只证明操作者配置了什么，不证明所选安全机制已经编进构建。

## 9. 推荐部署方式

### 本机开发

使用 `Insecure` 的 `0.0.0.0:5921` 默认值，并保持在可信 LAN。只有需要时才启用远程输入。

### 可信实验室或维护网络

只有在软件包显式构建了所需安全能力、并配置了有效描述文件时才使用 `Authenticated`。传输仍未加密，网络可达范围仍需要合适的信任边界。

### 公网

**不作为当前版本的直接部署方式。**

不要把当前 HyRemote listener 直接暴露到公网。

## 10. 当前能力矩阵

| 能力 | 当前状态 |
| --- | --- |
| 默认 LAN listener（`0.0.0.0:5921`） | 可用 |
| 远程输入默认关闭 | 可用 |
| 未认证/未加密状态如实说明 | 可用 |
| `Authenticated` API 和配置面 | 可用 |
| RFB VNC 认证 | **条件能力：需要启用传输安全的构建和有效描述文件** |
| 默认构建包含认证传输 | **否** |
| `AuthenticatedEncrypted` | **不可用，失败关闭** |
| 流加密 | **未实现** |
| 证书/私钥生产策略 | **未实现** |
| 认证会话身份/Registry | **未实现** |
| 每会话准入/终止 | **未实现** |
| VPN、隧道、防火墙配置 | 不属于 HyRemote 产品范围 |

当前版本的简明结论：

> HyRemote 当前版本默认在 `0.0.0.0:5921` 提供未认证、未加密 listener，远程输入关闭。`Authenticated` 只存在于启用传输安全的构建中，并且需要有效安全描述文件；它提供 VNC 认证，不提供加密。`AuthenticatedEncrypted` 未实现，始终失败关闭。不要把产品直接暴露到公网。
