# HyRemote SDK 使用契约

> 语言 / Language：**中文** ｜ [English](en/sdk-consumption.md)

HyRemote 是面向 Qt 应用的独立 SDK/框架，通过正常 CMake/Qt 机制使用，不要求修改用户 Qt 安装。

产品契约保持精简：**一个共享 Runtime**、四个并列接入前端、两种获取方式、一个部署 helper。

## 1. 产品心智模型

应用开发者先选择接入前端：

| 前端 | 应用代码改动 | 产品角色 |
| --- | --- | --- |
| **C++ API** | 链接 `HyRemote::RemoteAccess` | 显式生命周期/策略控制 |
| **Generic Plugin** | 不链接 HyRemote 应用库 | 保留原生 Qt 平台的首选零代码路径 |
| **QML API** | `import HyRemote` | Qt Quick 的声明式前端 |
| **QPA** | 不链接 HyRemote 应用库；以 `-platform hyremote` 启动 | 精确 private-ABI 的专用零代码路径 |

Qt Widgets 和 Qt Quick 是共享 Runtime 处理的目标类型，不是独立 SDK 产品。

应用不需要实例化或理解：

- Core；
- Session 内部；
- `RemoteFrame`；
- 采集后端类；
- 传输/RFB 实现对象；
- 输入后端对象；
- 图形/SoC 加速后端。

这些由 HyRemote 负责。

## 2. 获取方式

HyRemote 支持两种一等获取路径。

### 安装 SDK

安装 SDK 通过 CMake 包元数据暴露：

```cmake
find_package(HyRemote CONFIG REQUIRED)
```

概念安装 prefix：

```text
hyremote-sdk/
├── include/HyRemote/
├── lib/
│   ├── cmake/HyRemote/
│   └── HyRemote 产品 Runtime
├── qml/HyRemote/                 # 包含 QML payload 时
├── lib/HyRemote/plugins/generic/ # 包含 Generic payload 时
├── plugins/platforms/            # 部署 QPA payload 时位于应用树
└── licenses/
```

SDK 与 Qt SDK 分离。用户不把 HyRemote 文件复制进 Qt 安装。

### 源码使用

HyRemote 也可以放进应用构建：

```cmake
add_subdirectory(third_party/HyRemote EXCLUDE_FROM_ALL)
```

源码使用继承调用方编译器、工具链、sysroot 和 Qt SDK，使用与安装 SDK 相同的产品 API 和部署模型。

不要在同一个 CMake configure 中混用安装包和源码获取同一个 HyRemote 实例。

## 3. 共享 Runtime 契约

`HyRemote::RemoteAccess` / `HyRemoteRemoteAccess` 是正常共享 Runtime 制品。

Core 保持内部，不是第二个部署 Runtime。QML、Generic 和 QPA 是同一 Runtime 上的前端 payload，不是独立产品栈。

即使物理构建未来为嵌入式/Quick-only 产品切片可选 Qt 依赖，这条规则仍然成立。

## 4. C++ API 使用

Qt Widgets：

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)
find_package(HyRemote CONFIG REQUIRED)

target_link_libraries(MyApp PRIVATE
    Qt6::Widgets
    HyRemote::RemoteAccess
)
```

Qt Quick：

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Quick)
find_package(HyRemote CONFIG REQUIRED)

target_link_libraries(MyApp PRIVATE
    Qt6::Quick
    HyRemote::RemoteAccess
)
```

两者使用同一 API：

```cpp
#include <HyRemote/RemoteAccess.h>

HyRemote::RemoteAccess remote(&window);
remote.start();
```

产品不变量：

- 仅构造不会打开 listener；
- `start()`/`stop()` 显式；
- 绑定地址和端口是产品配置，不是后端专用类型；
- 远程查看和远程输入是独立策略；
- 错误/诊断使用 HyRemote 产品类型；
- 切换内部传输/采集后端不要求普通应用源码改变。

## 5. Generic Plugin 使用

应用能保留正常 Qt 平台时，Generic Plugin 是首选零代码路线。

应用保持纯 Qt：

```cmake
target_link_libraries(MyExistingApp PRIVATE Qt6::Widgets)

find_package(HyRemote CONFIG REQUIRED)
install(TARGETS MyExistingApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyExistingApp GENERIC)
```

通过 Qt generic-plugin 机制激活：

```text
MyExistingApp -plugin hyremote
```

或在合适场景：

```text
QT_QPA_GENERIC_PLUGINS=hyremote
```

Generic 兼容性基于公共 Qt 插件 API，必须保留应用原生 Qt 平台身份。

## 6. QML API 使用

```qml
import HyRemote

RemoteAccess {
    target: mainWindow
    enabled: true
}
```

QML API 是 C++ API 使用的同一共享 Runtime 上的声明式前端。

部署：

```cmake
hyremote_deploy(TARGET MyQmlApp QML)
```

当前状态：并列路线；安装 SDK 的 QML 示例和部署资格仍在持续完善。

## 7. QPA 使用

QPA 让应用保持纯 Qt：

```cmake
target_link_libraries(MyExistingApp PRIVATE Qt6::Widgets)

find_package(HyRemote CONFIG REQUIRED)
install(TARGETS MyExistingApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyExistingApp QPA)
```

运行：

```text
MyExistingApp -platform hyremote
```

QPA 使用 Qt private ABI，因此有精确版本兼容边界。当前参考是 Qt 6.8.3，Windows x86_64 和 Linux x86_64。

Generic 满足需求时优先 Generic。确实需要平台入口行为时再用 QPA。

## 8. 部署契约

HyRemote 拥有一组部署 helper：

```cmake
hyremote_deploy(TARGET MyCppApp)
hyremote_deploy(TARGET MyQmlApp QML)
hyremote_deploy(TARGET ExistingQtApp GENERIC)
hyremote_deploy(TARGET ExistingQtApp QPA)
hyremote_deploy(TARGET ExistingQmlApp QML QPA)
```

Helper 放置共享 Runtime 和选定前端 payload，并与 Qt 部署支持协作处理 runtime 依赖。

普通应用不应手工查找/复制 HyRemote 内部 DLL/SO/插件，也不应在部署环境中保留 SDK 专用 runtime 路径。

Generic 和 QPA 是并列零代码平台策略，不合并为同一接入模式。

见 [`guide/deployment.md`](guide/deployment.md)。

## 9. 产品包预期

干净部署应用应能在没有原 HyRemote 构建/源码树的情况下运行。

应用部署不得依赖：

- HyRemote 源码/构建目录；
- 不相关的 HyRemote SDK prefix；
- 手工复制 Core/transport/capture 文件；
- 指回开发者 SDK 的永久 `QT_PLUGIN_PATH` / `LD_LIBRARY_PATH` workaround。

安装 SDK 和源码使用路径应导向相同的应用产品模型。

## 10. 后端/工具链隔离

普通应用代码必须与传输/采集/加速实现选择隔离。

后端专用编译器、runtime 或 SDK 只有在产品收益足以抵消分发成本时才可在内部引入。后端专用类型不得泄漏到普通 C++/QML 应用契约。

同样规则适用于未来 DMA-BUF、RKMPP、VAAPI、D3D 或其他传输实现。

## 11. 当前 SDK 兼容性

V0.3.0.0 参考：

- Windows x86_64；
- Linux x86_64；
- Qt 6.8.3；
- C++ API、QML API、Generic Plugin、QPA 是并列路线；
- QPA 仍需精确 Qt private ABI。

> **TODO V0.4：** 在加入支持矩阵前认证计划中的 Qt 5.15 LTS 兼容线。
>
> **TODO：** 增加嵌入式/ARM64 SDK 和交叉编译包/部署契约。嵌入式 Linux/ARM64/EGLFS 参考单元是 #343 下 V0.3 家族 GA 前强制兼容基线；额外 BSP/SoC 扩展单独准入，不预分配版本号（#333）。

见 [`compatibility.md`](compatibility.md)。

## 12. 示例/学习契约

示例是开发者产品入口，不是架构演示。

当前可运行路线：

```text
01-widgets-cpp        C++ Widgets
02-quick-cpp          C++ Quick
03-generic            Generic 零代码
04-qml                QML API
05-qpa                QPA proxy
```

每个 HyRemote 自编 GUI 示例使用统一项目品牌，并在适用处提供英文和简体中文 UI/文档。

C++ 和 Generic 的端到端接入见 [`guide/integrate-your-project.md`](guide/integrate-your-project.md)。

## 13. 成功定义

Qt 开发者能够：

1. 按应用所有权模型选择接入前端；
2. 通过安装 SDK 或源码获取 HyRemote；
3. 在不理解内部 Core/transport/capture 架构的情况下构建应用；
4. 用 `hyremote_deploy()` 部署；
5. 从应用自己的部署树运行；
6. 连接标准 viewer，获得文档化查看/控制行为；
7. 从产品文档理解精确安全、兼容和已知限制边界。
