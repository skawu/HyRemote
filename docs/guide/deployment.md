# 部署

> 语言 / Language：**中文** ｜ [English](../en/guide/deployment.md)

HyRemote 自己负责其产品运行时与集成载荷的部署，不要求应用开发者手工去猜内部后端文件的落点。

## 一个部署入口

已安装 SDK 与源码（`add_subdirectory`）两种获取方式暴露**同一个**面向应用的助手：

```cmake
hyremote_deploy(TARGET MyCppApp)
hyremote_deploy(TARGET MyQmlApp QML)
hyremote_deploy(TARGET ExistingQtApp QPA)
hyremote_deploy(TARGET ExistingQmlApp QML QPA)
```

这些写法是**同一份部署契约**的扩展，不会产生第二套运行时架构。

`QML` 与 `QPA` 用于选择**可选载荷**：它们必须已经存在于所选的那份 HyRemote 构建/包中。它们不是构建开关——`hyremote_deploy()` 不会在应用配置阶段把一个只有 C++ 的 SDK 变成 QML/QPA SDK。请求了而实际不可用的可选载荷会在**配置阶段即失败**（fail-closed），而不是产出一个日后在运行期才失败的不完整部署。

## V1 固定产物模型

正常 V1 部署刻意保持简单：

- `HyRemote::RemoteAccess` 是**唯一的共享 C++ 产品库**；
- Core 静态组合在该门面之后，**不是**独立的运行期载荷；
- `qhyremote` 是用于 Transparent QPA 的**一个 Qt platform MODULE**；
- QML 模块是同一份共享 `RemoteAccess` 运行时之上的薄封装。

`BUILD_SHARED_LIBS` **不会**在静态与共享形态之间切换产品。

应用**不得**按文件名拷贝或挑选 Core、Session、RFB、采集、输入、适配器或后端实现文件。

## 普通 C++ 部署

对 C++ Widgets/Quick 应用：

```cmake
install(TARGETS MyApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyApp)
```

Qt 自身的部署脚本负责 Qt 运行时的放置。HyRemote 的补充脚本只负责安装那**一个** `HyRemoteRemoteAccess` 共享库，并请 Qt 的部署支持去解析该库的 Qt 依赖。

应用开发者**不需要**自己去找 `HyRemoteRemoteAccess.dll` / `libHyRemoteRemoteAccess.so`。

## QML 部署

以启用 QML API 的方式构建/安装 HyRemote，然后这样打包应用：

```cmake
install(TARGETS MyQmlApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyQmlApp QML)
```

已安装与源码两种方式都会发布一个绝对的 `HyRemote_QML_IMPORT_PATH`，指向当前 HyRemote 的 QML import 根。助手会把该根追加到应用已有的 QML import 路径上，并使用 Qt 支持的 QML 感知部署机制；同一步补充动作也会带上共享的 `RemoteAccess` 运行时。

若所选构建/包没有 QML 载荷，`hyremote_deploy(... QML)` 会在**配置阶段失败**，而不会静默产出一个没有 `import HyRemote` 的部署。

另见 [`qml-consumption.md`](../qml-consumption.md) 与 [`getting-started/qml.md`](../getting-started/qml.md)。

## Transparent QPA 部署

应用在源码/链接层面保持**纯 Qt**：

```cmake
target_link_libraries(MyApp PRIVATE Qt6::Widgets)
```

通过已安装的 HyRemote SDK 打包：

```cmake
find_package(HyRemote CONFIG REQUIRED)
install(TARGETS MyApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyApp QPA)
```

助手会加入：

- 由获取方式选定的那份 SDK/源码构建的 `qhyremote` platform module；
- 该模块内部使用的共享 `HyRemoteRemoteAccess` 运行时；
- 由 Qt 部署工具解析出的 Qt/原生平台依赖。

应用可执行文件本身**仍然不链接** `HyRemote::RemoteAccess`。

当前 QPA 包与**精确的 Qt 6.8.3** 版本耦合。所选 HyRemote 构建/包不含 QPA 支持、或消费者 Qt 版本与该限定私有 ABI 线不一致时，`hyremote_deploy(... QPA)` 会**失败关闭**。

在 Linux 上，qhyremote 携带一段有界的、以自身位置为基准的重定位锚点指向共享门面。当部署把插件放进应用通常的 `plugins/platforms` 目录时，HyRemote 会把这段由包自己拥有的 RUNPATH 段改写为应用的 Qt 部署库目录。正常部署后的应用**不应**仅仅为了找到 HyRemote 就需要 `LD_LIBRARY_PATH` 或 `QT_PLUGIN_PATH`。

对源码/`add_subdirectory` 获取方式，qhyremote 保持为内部构建目标，留在 HyRemote 的子构建内，而不会写进宿主应用的顶层插件构建目录；部署助手直接解析该目标文件，因此这种构建隔离**不改变**应用的使用方式。

另见 [`getting-started/qpa-proxy.md`](../getting-started/qpa-proxy.md) 与 [`guide/install.md`](install.md)。

## QML + QPA

当两个可选载荷都存在时，QML 应用可以有意同时使用两者：

```cmake
hyremote_deploy(TARGET MyQmlApp QML QPA)
```

`QML` 选择 Qt 的 QML 感知部署流程，`QPA` 追加代理模块。两者仍然使用**同一份**共享 `RemoteAccess` 运行时——这不是第四套运行时架构。两个被选载荷的可用性与版本检查同样保持失败关闭。

## 安全与网络配置

部署**不会**削弱产品默认值：

- Embedded C++ 的构造本身是惰性的；
- QML 在应用策略显式启用之前保持禁用；
- QPA 的监听器创建遵循已文档化的平台插件生命周期；
- 默认绑定回环地址；
- 远程输入默认关闭；
- 当前 RFB SecurityType None 基线既不提供查看端认证，也不提供传输加密。

如果应用有意更改绑定地址，其运维/部署文档必须描述由此产生的信任边界。见 [`security.md`](../security.md)。

## 部署验证边界

正常 V1 部署的要求是：干净安装的、以及声明为源码获取方式的消费者应用，都能**脱离原始 HyRemote SDK/构建路径**、直接从自己的部署树运行。四种部署调用形态各自独立验证；`QML` 单独一种不会从 `QML QPA` 组合推断出来，因为二者选择的是不同的补充部署路径。

参考环境为 Windows x86_64 与 Linux x86_64，以及里程碑所规定的精确 Qt 矩阵。
