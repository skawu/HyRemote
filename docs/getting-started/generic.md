# Generic Plugin 接入（零业务代码改动）

> 语言 / Language：**中文** ｜ [English](../en/getting-started/generic.md)

Generic Plugin 适合已经存在的 Qt Widgets / Qt Quick 应用：应用本身保持普通 Qt 程序，不链接 HyRemote API，通过 Qt 的 **generic plugin** 机制在运行时启用远程访问。

它的关键特点是：**不替换应用的 Qt platform integration**。Windows 仍使用 `qwindows`，Linux/X11 仍使用 `qxcb`；HyRemote 只在应用旁路接入同一个 Shared Runtime。

## 适用场景

优先考虑 Generic Plugin，当你希望：

- 不改业务代码；
- 不引入 `HyRemote::RemoteAccess` 链接依赖；
- 保持原生 Qt platform、窗口、本地输入和 GPU 路径；
- 使用统一的 `hyremote_deploy()` 完成打包。

如果应用需要在运行时显式控制启动/停止、目标切换或策略，优先使用 [`cpp.md`](cpp.md)。

## 构建应用

应用仍然只链接 Qt：

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)

target_link_libraries(MyApp PRIVATE Qt6::Widgets)
```

Qt Quick 应用对应链接 `Qt6::Quick`。

## 部署

部署阶段引入 HyRemote SDK：

```cmake
find_package(HyRemote CONFIG REQUIRED)

install(TARGETS MyApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyApp GENERIC)
```

部署结果会包含：

- HyRemote Generic Plugin；
- Shared Runtime；
- 应用正常启动所需的原生 Qt platform plugin；
- 相关 Qt 运行时依赖。

应用源码仍然不链接 HyRemote target。

## 启动

使用 Qt generic-plugin 参数：

```text
MyApp -plugin hyremote
```

也可以使用环境变量：

```text
QT_QPA_GENERIC_PLUGINS=hyremote
```

默认行为与 C++ API 一致：

- 地址：`127.0.0.1`；
- 端口：`5921`；
- 远程输入：关闭；
- 未认证的非回环监听：拒绝。

## 配置格式

Generic Plugin 的配置采用 `key=value` 字段，字段之间使用分号：

```text
address=127.0.0.1;port=5901;input=true;security=insecure
```

当前可识别字段：

| 字段 | 说明 |
| --- | --- |
| `address` | 数字 IP 地址 |
| `port` | `1..65535` |
| `input` | `true/false`、`on/off`、`yes/no`、`1/0` |
| `security` | `insecure`、`authenticated`、`authenticated-encrypted` |
| `security-config` | 安全配置文件路径 |

`authenticated-encrypted` 在 V0.1 中会**失败关闭**，不会降级成未加密连接。

> **TODO：** V0.2 提供完整 VeNCrypt/TLS、证书策略和会话安全能力后，再将 `authenticated-encrypted` 作为可用安全配置公开给生产场景。

## Native platform 保持不变

Generic Plugin 与 QPA 是两种不同产品入口：

```text
Generic Plugin:
Qt application -> native qwindows/qxcb/... + HyRemote generic plugin

QPA:
Qt application -> qhyremote Factory Trampoline -> native qwindows/qxcb
```

Generic Plugin 不应该让 `QGuiApplication::platformName()` 变成 `hyremote`。

## Widgets 与 Qt Quick

Generic Plugin 的 automatic-access Runtime 会发现和组合受支持的应用顶层窗口。Widgets 与 Qt Quick 使用同一个 Runtime，不要求应用切换 UI 技术。

当前参考环境：

- Windows x86_64 + Qt 6.8.3；
- Linux x86_64 + Qt 6.8.3。

其它环境请查看 [`../compatibility.md`](../compatibility.md)。

## 安全边界

V0.1 以 Developer Preview 为定位：

- 默认只监听回环地址；
- 远程控制默认关闭；
- 不允许未认证的非回环直接监听；
- `AuthenticatedEncrypted` 在 TLS 后端未实现时失败关闭；
- 不要将当前产品直接暴露到公网。

详见 [`../security.md`](../security.md)。

## 部署排错

如果应用在脱离 Qt SDK 的机器上启动失败，首先确认部署树同时包含：

- `plugins/generic/` 下的 HyRemote Generic Plugin；
- `plugins/platforms/` 下应用原本使用的 Qt platform plugin；
- Shared Runtime 与 Qt runtime dependencies。

不要通过手工设置 SDK 的 `QT_PLUGIN_PATH`、`LD_LIBRARY_PATH` 或复制构建树文件来掩盖部署缺失。正式部署应由 `hyremote_deploy(TARGET ... GENERIC)` 自包含完成。

## 后续

- 部署：[`../guide/deployment.md`](../guide/deployment.md)
- 查看器：[`../guide/viewer-connection.md`](../guide/viewer-connection.md)
- 安全：[`../security.md`](../security.md)
- 兼容性：[`../compatibility.md`](../compatibility.md)
- 已知限制：[`../known-limitations.md`](../known-limitations.md)
