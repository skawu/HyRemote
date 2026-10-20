# 声明式 QML 使用与部署

> 语言 / Language：**中文** ｜ [English](en/qml-consumption.md)

HyRemote QML API 是 C++ API 使用的同一共享 Runtime 上的薄声明式前端。它不引入第二套 Session、采集、输入或传输栈，背后实现也不是第二个公共 C++ SDK 目标。

当前产品状态：并列路线，与 C++ API 使用同一共享 Runtime。

QML 路由和其他三条并列，没有主次。端到端接入、部署和 viewer 连接流程与其他路线相同，见 [`guide/integrate-your-project.md`](guide/integrate-your-project.md)。选择时需要注意：QML 模块必须通过 `QML` 关键字部署，QML 暴露的是同一 Runtime。

## 1. 安装 SDK

使用所选 HyRemote 包支持的 Qt 版本，并正常定位独立 HyRemote prefix：

```cmake
find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Core Gui Qml Quick)
find_package(HyRemote CONFIG REQUIRED)
```

安装包包含 QML payload 时，HyRemote 发布部署 helper 需要的 QML import root。应用开发者不按文件名复制 HyRemote QML 插件或内部库。

应用边界：

```qml
import HyRemote

RemoteAccess {
    target: mainWindow
    enabled: true
}
```

`enabled: true` 是显式声明式启动请求。Runtime 在 QML 组件构造完成后才启动，让初始目标和策略绑定稳定。

安全默认与 C++ API 一致：

- listener `0.0.0.0:5921`；
- 端口 5921；
- 远程输入关闭；
- 仅导入模块不会打开 listener。

## 2. 正常定义应用

普通 Qt Quick 应用保留自己的 QML 模块和正常 Qt 依赖：

```cmake
qt_add_executable(MyQmlApp
    main.cpp
)

qt_add_qml_module(MyQmlApp
    URI MyApplication
    VERSION 1.0
    QML_FILES Main.qml
)

target_link_libraries(MyQmlApp PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
)
```

应用不链接 HyRemote Core、Session、采集、输入、传输或 QML 实现目标。

## 3. 通过 `hyremote_deploy()` 部署

先安装应用，再使用 HyRemote 部署 helper：

```cmake
install(TARGETS MyQmlApp
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

hyremote_deploy(TARGET MyQmlApp QML)
```

`QML` 选择已经存在的 HyRemote QML payload。它不是构建开关，不能在消费者配置时把 C++-only SDK 变成 QML SDK。

所选 HyRemote 包不包含 QML payload 时，配置失败，不产生部分可部署应用。

部署 helper 把 HyRemote QML import root 与 Qt 正常 QML 部署支持结合，并携带其他前端使用的同一共享 Runtime。

应用不要：

- 手工复制 `qmldir`；
- 按文件名复制 HyRemote QML 插件二进制；
- 把 `QML_IMPORT_PATH` 指回 SDK 作为永久部署 workaround；
- 查找传输/采集实现文件。

## 4. 源码使用

源码获取使用相同的应用 QML 和部署模型。

先启用 QML 前端，再加入 HyRemote：

```cmake
set(HYREMOTE_BUILD_QML_API ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/HyRemote EXCLUDE_FROM_ALL)
```

之后像安装 SDK 一样部署：

```cmake
hyremote_deploy(TARGET MyQmlApp QML)
```

源码树可以创建生成 QML payload 所需的私有构建目标，但这些目标不加入应用公共链接契约。

## 5. QML + QPA

QML 应用可以有意把声明式 HyRemote API 与 QPA 部署前端组合：

```cmake
hyremote_deploy(TARGET MyQmlApp QML QPA)
```

这会打包两个前端 payload，同时仍使用一个共享 Runtime，不创建新 Runtime 架构。

QPA 仍按精确 Qt private ABI 认证；见 [`getting-started/qpa-proxy.md`](getting-started/qpa-proxy.md) 和 [`compatibility.md`](compatibility.md)。

## 6. 生命周期语义

QML `RemoteAccess` 面遵循与 C++ 入口相同的产品生命周期：

- 对象构造/导入不会打开 listener；
- `enabled: true` 在组件完成后请求启动；
- `enabled: false` 停止自有 Runtime；
- 配置修改属于停止状态；
- 启动失败会把 `enabled` 恢复为 false，并暴露产品级错误状态；
- `Running` 表示 Runtime/listener 已启动，不表示 viewer 已连接；
- `connectedClientCount` 是运行状态，不是认证或授权；
- 销毁 QML 对象会停止其自有 Runtime。

## 7. 错误处理

QML API 暴露 HyRemote 产品级状态和诊断，不暴露 RFB/socket/后端对象。

应用应对文档化公共状态/错误属性作出反应，不要依赖内部实现消息。

不可恢复 Runtime 故障保持可见，直到应用显式停止/禁用 Runtime。清除诊断不会静默恢复或替换失败的 Runtime 实例。

## 8. 安全边界

QML 前端不会削弱或替换共享 Runtime 安全/访问默认值。

当前发布行为：

- listener 默认 `0.0.0.0:5921`，监听本地 IPv4 接口；这是 wildcard 绑定地址，不是 viewer 主机地址；
- 远程输入默认关闭，必须显式启用；
- `Insecure` 未认证、未加密，只面向可信 LAN/显式保护网络，不适合 Internet；
- 配置后 `Authenticated` 可以使用 RFB VNC 认证；
- 当前流未加密；
- 加密传输不可用时 `AuthenticatedEncrypted` 失败关闭；
- `diagnosticReport()` 从 C++/Generic/QPA 路由使用的同一 Runtime 报告配置/有效 listener、安全、输入、客户端和有界错误事实。

不要把当前产品直接暴露到公网。见 [`security.md`](security.md)。

## 9. 兼容性

当前参考环境：

- Windows x86_64 + Qt 6.8.3；
- Linux x86_64 + Qt 6.8.3。

QML API 是并列路线。基础 Qt Quick 运行不会自动认证 Quick3D、自定义 FBO、混合 `QQuickWidget`、特殊图形后端或原生窗口配置。

当前产品边界见 [`compatibility.md`](compatibility.md) 和 [`known-limitations.md`](known-limitations.md)。

## 10. 推荐阅读

- [`getting-started/qml.md`](getting-started/qml.md)：简明首次接入；
- [`guide/deployment.md`](guide/deployment.md)：部署模型；
- [`guide/viewer-connection.md`](guide/viewer-connection.md)：viewer 和远程控制流程；
- [`security.md`](security.md)：安全边界；
- [`compatibility.md`](compatibility.md)：当前支持矩阵。
