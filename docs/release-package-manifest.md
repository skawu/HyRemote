# HyRemote 包清单

> 语言 / Language：**中文** ｜ [English](en/release-package-manifest.md)

本文定义面向应用的 SDK 和部署 payload 模型，描述产品制品，不描述仓库验收流程。

HyRemote 有意保持安装产品精简：**一个共享 Runtime**、可选前端 payload、CMake 包元数据和产品文档/许可证。

## 包元数据

安装后的 HyRemote SDK 提供：

- `HyRemoteConfig.cmake` 和版本/包元数据；
- 导出应用目标的 `HyRemoteTargets.cmake`；
- `HyRemoteDeploy.cmake` / `hyremote_deploy()`；
- 导出 API 的公共头文件；
- 选定的前端 payload；
- Apache-2.0 `LICENSE`；
- `NOTICE.md`；
- 适用的产品文档。

安装 SDK 与用户 Qt 安装分离。HyRemote 不要求把文件复制到 Qt SDK 树。

## 共享 Runtime

正常 Runtime 制品：

```text
HyRemote::RemoteAccess
HyRemoteRemoteAccess
```

它是所有前端共用的单一共享 C++ Runtime。

Core 是内部组合，不是第二个应用 Runtime 或普通导出 SDK 目标。

`BUILD_SHARED_LIBS` 不会创建独立公共 HyRemote 产品变体。

## C++ API payload

安装 SDK 导出：

- `HyRemote::RemoteAccess`；
- `HyRemote/RemoteAccess.h` 和其他明确公共应用类型；
- `HyRemoteRemoteAccess` runtime payload。

典型应用使用：

```cmake
find_package(HyRemote CONFIG REQUIRED)
target_link_libraries(MyApp PRIVATE HyRemote::RemoteAccess)
```

部署：

```cmake
hyremote_deploy(TARGET MyApp)
```

应用不需要 Core、Session、采集、输入、传输或目标适配器文件名。

## Generic Plugin payload

包含 Generic Plugin 时，SDK 含有 HyRemote Qt Generic Plugin 以及以下命令需要的包元数据：

```cmake
hyremote_deploy(TARGET ExistingQtApp GENERIC)
```

目标应用保持纯 Qt，不链接 HyRemote 应用目标。

部署应用保留原生 Qt 平台插件（`qwindows`、`qxcb` 或其他合格原生平台），通过 Qt 通用插件机制加载 HyRemote。

Generic Plugin 基于公共 Qt API，不继承 QPA private-ABI 兼容约束。

## QML payload：并列接入路线

包含 QML API 时，SDK 含有同一共享 Runtime 上的 `HyRemote` QML 模块。

部署：

```cmake
hyremote_deploy(TARGET MyQmlApp QML)
```

QML backing payload 不是第二个公共 C++ Runtime 目标。

> 安装 QML-only consumer 和部署诊断路径已有 L1 回归证据。正式 V0.3.0.0 发布资格仍需要精确 frozen-candidate L2 Windows/Linux 四路线门禁；并列状态本身不构成未认证 Qt/平台组合的声明。

## QPA payload：并列接入路线（精确 private ABI）

包含 QPA 时，SDK 含有 `qhyremote` 平台插件和以下命令需要的包元数据：

```cmake
hyremote_deploy(TARGET ExistingQtApp QPA)
```

或者，应用同时使用 HyRemote QML API 时：

```cmake
hyremote_deploy(TARGET ExistingQmlApp QML QPA)
```

应用在链接层保持纯 Qt。安装 SDK 不暴露 HyRemote::QpaPlatform 作为应用目标。

QPA 使用 Qt private ABI，因此有精确版本兼容边界。当前参考是 Qt 6.8.3，Windows x86_64 和 Linux x86_64。

> QPA installed-consumer/product-fit 测试存在于 Qt 6.8.3 Windows/Linux 参考单元。V0.3.0.0 验收仍需要精确 frozen-candidate L2 门禁；不要推断其他 Qt 补丁、QPA delegate 或图形栈兼容。

## 部署形式

产品部署 helper 支持以下应用形式：

```cmake
hyremote_deploy(TARGET MyCppApp)
hyremote_deploy(TARGET MyQmlApp QML)
hyremote_deploy(TARGET ExistingQtApp GENERIC)
hyremote_deploy(TARGET ExistingQtApp QPA)
hyremote_deploy(TARGET ExistingQmlApp QML QPA)
```

Generic 和 QPA 是并列零代码平台策略，不合并为同一产品模式。

Helper 负责 HyRemote Runtime/前端 payload 放置，并与 Qt 部署支持协作处理 Qt runtime 依赖。

## 部署树预期

精确文件名因平台而异，概念部署树：

```text
MyApplication/
├── bin/ 或应用可执行文件
├── HyRemote Shared Runtime
├── Qt runtime 库
├── plugins/
│   ├── platforms/       # 原生 Qt 平台；选择 QPA 时还有 qhyremote
│   └── generic/         # 选择 GENERIC 时包含 HyRemote Generic Plugin
└── qml/
    └── HyRemote/        # 选择 QML 时
```

干净部署不能依赖原 HyRemote 源码/构建树。

不要使用永久 SDK 路径 workaround：

- `QT_PLUGIN_PATH` 指回开发者 SDK；
- `QT_QPA_PLATFORM_PLUGIN_PATH` 指回构建树；
- `LD_LIBRARY_PATH` 指回 HyRemote 构建输出；
- 手工复制内部 Core/transport/capture 文件。

## 不属于公共 SDK 的内容

仓库实现/测试材料不会因为存在于源码控制中就成为应用 API。

以下为内部或开发资产，除非未来产品明确提升：

- Core 实现类；
- Session/transport/capture/input 内部；
- 私有 Widgets/Quick 目标适配器；
- QPA 私有 controller/composition 类；
- 测试客户端/harness；
- CI/workflow 脚本；
- 研究/基准工具；
- 硬件加速实现细节。

它们的内部形态可以演进，不产生额外应用接入模式。

## 许可证与通知

HyRemote 包制品携带项目 Apache-2.0 `LICENSE` 和 `NOTICE.md`。

Qt 和其他第三方 runtime 组件仍受其自身许可证约束。分发者负责满足随应用发布的精确第三方二进制义务。

任何新捆绑第三方组件都必须有意识地更新包许可证/通知集。

## 当前产品包状态

当前包重点：

| Payload | 状态 |
| --- | --- |
| 共享 Runtime / C++ API | 并列路线 |
| Generic Plugin | 并列路线 |
| QML 模块 | 并列路线 |
| QPA 插件 | 并列路线（精确 Qt/private ABI） |
| Qt 6.8.3 Windows/Linux 包线 | 当前参考 |
| Qt 5.15 包线 | #343 下 V0.3 家族 GA 前强制工作；不属于 V0.3.0.0 参考发布 |
| ARM64/嵌入式 Linux 包 | TODO，V0.3 家族 GA 前强制兼容基线；不预留精确 Feature 版本 |

即使后续发布增加平台/依赖切片或硬件加速，包契约仍以一个 Runtime 为中心。
