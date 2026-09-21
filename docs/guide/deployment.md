# 部署

> 语言 / Language：**中文** ｜ [English](../en/guide/deployment.md)

HyRemote 使用统一的 `hyremote_deploy()` 负责产品 Runtime 与集成载荷的部署。应用不需要按文件名手工复制 HyRemote 内部库、插件或 Qt 依赖，也不应依赖源码树/构建树才能运行。

## 一个部署入口

已安装 SDK 与源码方式使用同一组接口：

```cmake
hyremote_deploy(TARGET MyCppApp)
hyremote_deploy(TARGET MyQmlApp QML)
hyremote_deploy(TARGET ExistingQtApp GENERIC)
hyremote_deploy(TARGET ExistingQtApp QPA)
hyremote_deploy(TARGET ExistingQmlApp QML QPA)
```

四个 integration frontend 共用同一个 Shared Runtime；这些参数只选择需要部署的前端载荷，不会创建第二套 Runtime。

## 产品载荷模型

| 载荷 | 作用 |
| --- | --- |
| `HyRemote::RemoteAccess` / `HyRemoteRemoteAccess` | 一个共享 Runtime |
| QML `HyRemote` module | QML API 的声明式载荷 |
| Generic Plugin | Qt generic-plugin 载荷，保持原生 Qt platform |
| `qhyremote` | QPA frontend 的 platform plugin 载荷 |
| Core | 内部静态组成，不作为独立应用 Runtime 部署 |

## C++ API 部署

```cmake
find_package(HyRemote CONFIG REQUIRED)

install(TARGETS MyApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyApp)
```

应用链接 `HyRemote::RemoteAccess`，部署助手负责 Shared Runtime 与其 Qt 运行时依赖。

应用不需要自己定位 `HyRemoteRemoteAccess.dll` 或 `libHyRemoteRemoteAccess.so`。

## Generic Plugin 部署

Generic Plugin 是 V0.1 的主零代码接入路径。应用本身仍然只链接 Qt：

```cmake
target_link_libraries(MyExistingApp PRIVATE Qt6::Widgets)

find_package(HyRemote CONFIG REQUIRED)
install(TARGETS MyExistingApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyExistingApp GENERIC)
```

部署树会包含：

- HyRemote Generic Plugin；
- Shared Runtime；
- 应用正常使用的 native Qt platform plugin；
- 所需 Qt runtime dependencies。

Generic Plugin 不替换 Qt platform。部署后应用仍应使用原来的 `qwindows`、`qxcb` 等 platform identity。

启动示例：

```text
MyExistingApp -plugin hyremote
```

或：

```text
QT_QPA_GENERIC_PLUGINS=hyremote
```

详见 [`../getting-started/generic.md`](../getting-started/generic.md)。

## QML API 部署（Preview）

```cmake
find_package(HyRemote CONFIG REQUIRED)

install(TARGETS MyQmlApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyQmlApp QML)
```

QML 部署会加入 `HyRemote` import module，并复用同一个 Shared Runtime。

如果 SDK 中不存在 QML 载荷，部署会在配置阶段失败，而不是生成一个运行时才发现缺少 `import HyRemote` 的不完整包。

> **TODO：** 完成最终 installed-SDK 示例与完整产品化资格后，将 QML API 从 Preview 提升为正式产品路径。

## QPA 部署（Preview）

应用源码仍保持 Qt-only：

```cmake
target_link_libraries(MyApp PRIVATE Qt6::Widgets)

find_package(HyRemote CONFIG REQUIRED)
install(TARGETS MyApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyApp QPA)
```

启动：

```text
MyApp -platform hyremote
```

部署助手加入：

- `qhyremote`；
- Shared Runtime；
- 被 Factory Trampoline 委托的 native Qt platform plugin；
- 所需 Qt runtime dependencies。

QPA 使用 Qt private ABI，因此必须按精确 Qt patch 资格化。当前参考是 Qt 6.8.3。

如果消费者 Qt 与该 QPA 载荷不匹配，部署应失败关闭，而不是尝试猜测兼容性。

> **TODO：** 在更多 Qt patch / OS 组合上完成精确兼容性与物理本地+远程并存资格后，再扩大 QPA 支持声明。

## QML + QPA

QML 应用可以同时使用 QML payload 和 QPA frontend：

```cmake
hyremote_deploy(TARGET MyQmlApp QML QPA)
```

两者仍然复用同一个 Shared Runtime。

## Generic 与 QPA 不组合

Generic 和 QPA 是两种不同的零代码平台策略：

```text
Generic: native Qt platform + HyRemote generic plugin
QPA:     qhyremote Factory Trampoline -> native Qt platform
```

它们不是一套部署中需要同时开启的两个补充插件。选择其中一种零代码入口即可。

## 自包含部署

正式部署应能脱离原始 HyRemote SDK、Qt SDK 和构建目录运行。

不要用下面这些方式掩盖缺失载荷：

- 把 `QT_PLUGIN_PATH` 指回 Qt SDK；
- 把 `LD_LIBRARY_PATH` 指回构建树；
- 手工从 build 目录拷贝内部 HyRemote 文件；
- 同时混用源码 acquisition 与 installed package metadata。

`hyremote_deploy()` 的目标就是让部署树本身包含应用需要的产品载荷。

## Windows / Linux

当前 V0.1 参考环境：

- Windows x86_64 + Qt 6.8.3；
- Linux x86_64 + Qt 6.8.3。

Linux 部署中，HyRemote 会处理产品自有 Runtime / plugin 的重定位，使正常运行不需要回到原 SDK 路径。

精确状态见 [`../compatibility.md`](../compatibility.md)。

## 安全与部署

部署不会改变安全默认值：

- 默认绑定 `127.0.0.1`；
- 远程输入默认关闭；
- 未认证非回环监听被拒绝；
- authenticated profile 可使用 RFB VNC authentication，但当前数据流不加密；
- `AuthenticatedEncrypted` 在 TLS/VeNCrypt backend 不可用时失败关闭，并且不会打开监听器。

> **TODO V0.2：** VeNCrypt/TLS、证书策略、authenticated sessions 与生产网络策略。

不要把 V0.1 直接暴露到公网。详见 [`../security.md`](../security.md)。

## 部署检查

部署完成后建议确认：

- 应用在没有原始 HyRemote/Qt SDK 的环境中能够启动；
- C++ API 应用能加载 Shared Runtime；
- Generic 应用同时存在 `plugins/generic/` 中的 HyRemote plugin 和 `plugins/platforms/` 中的 native Qt platform plugin；
- QPA 应用包含 `qhyremote` 与匹配的 native delegate；
- QML 应用可以解析 `import HyRemote`；
- 运行时没有依赖 source/build-tree search path。
