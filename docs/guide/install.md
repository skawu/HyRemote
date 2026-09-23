# 安装、构建与接入准备

> 语言 / Language：**中文** ｜ [English](../en/guide/install.md)

本文说明如何构建 HyRemote、生成可安装 SDK，以及如何从应用工程中消费 HyRemote。内容按当前产品形态编写，不包含内部验收过程或开发历史。

## 当前产品环境

V0.1 Developer Preview 当前参考环境：

| 维度 | 当前产品状态 |
| --- | --- |
| 操作系统 | Windows x86_64、Linux x86_64 |
| Qt | Qt 6.8.3 reference |
| C++ API | **V0.1 主路径** |
| Generic Plugin | **V0.1 主路径** |
| QML API | **Preview** |
| QPA | **Preview，Qt 6.8.3 exact private ABI** |
| Embedded Linux / ARM64 | **TODO V1.1** |
| Qt 5.15 LTS | **TODO V0.4 qualification** |

精确状态见 [`../compatibility.md`](../compatibility.md)。

## 前置工具

推荐准备：

- CMake 3.21+；
- Ninja；
- Qt 6.8.3 开发套件；
- Windows：MSVC x64；
- Linux：GCC/Clang x86_64。

QPA 额外需要与 Qt 6.8.3 **完全匹配**的 Qt private Gui 开发目标。

## 从源码构建 HyRemote

仓库统一构建入口是 `build.cmd`。它在 Windows 和 POSIX shell 上都可运行，并从 `build.yml` 读取配置。`build.cmd build` 配置并编译，`build.cmd install` 把产品/SDK 树生成到 `build/install/`，`build.cmd test` 构建并运行测试，`build.cmd clean` 删除构建树，`build.cmd rebuild` 等价于 clean 加 build。`compile.cmd` 仅作为兼容转发保留。`build/` 下的内容是开发中间产物，`build/install/` 下的内容才是用户与 SDK 消费者真正使用的东西。

先查看最终配置：

```text
build.cmd build --show-config
```

V0.1 主路径构建：

```text
build.cmd build --integrations=cpp,generic --qt-prefix=/path/to/Qt/6.8.3/<kit>
```

开发时需要完整四 frontend：

```text
build.cmd install --integrations=cpp,qml,generic,qpa --qt-prefix=/path/to/Qt/6.8.3/<kit>
```

需要示例或测试时显式加入：

```text
build.cmd test --integrations=cpp,generic --examples
```

命令行参数覆盖 `build.yml` 中的对应配置。四个 integration 是独立选择项；选择 Generic、QML 或 QPA 不会把 C++ frontend 当作父实现隐式打开。

## 直接使用 CMake

如果你把 HyRemote 当作普通 CMake 工程构建，也可以直接配置：

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/<kit> \
  -DHYREMOTE_BUILD_CPP_API=ON \
  -DHYREMOTE_WITH_GENERIC_PLUGIN=ON
cmake --build build --parallel
```

可选 frontend：

```text
-DHYREMOTE_BUILD_QML_API=ON
-DHYREMOTE_WITH_GENERIC_PLUGIN=ON
-DHYREMOTE_WITH_QPA_PROXY=ON
```

QPA 需要 Qt 6.8.3 exact + `Qt6::GuiPrivate`。

## 生成 installed SDK

设置安装前缀并执行标准 CMake install：

```bash
cmake -S . -B build-sdk -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/<kit> \
  -DCMAKE_INSTALL_PREFIX=/path/to/hyremote-sdk \
  -DHYREMOTE_BUILD_CPP_API=ON \
  -DHYREMOTE_WITH_GENERIC_PLUGIN=ON

cmake --build build-sdk --parallel
cmake --install build-sdk
```

如果需要 QML/QPA payload，在生成 SDK 时按需开启对应 frontend。

安装后的应用通过：

```cmake
find_package(HyRemote CONFIG REQUIRED)
```

消费产品，不需要知道 HyRemote 内部目录结构。

## 已安装 SDK：C++ API

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

Widgets 与 Quick 使用同一个 `HyRemote::RemoteAccess`。

详见 [`../getting-started/cpp.md`](../getting-started/cpp.md)。

## 已安装 SDK：Generic Plugin

Generic 应用保持 Qt-only，不链接任何 HyRemote target：

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)
find_package(HyRemote CONFIG REQUIRED)

target_link_libraries(MyExistingApp PRIVATE Qt6::Widgets)

install(TARGETS MyExistingApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyExistingApp GENERIC)
```

运行时通过 Qt generic-plugin 机制激活：

```text
MyExistingApp -plugin hyremote
```

详见 [`../getting-started/generic.md`](../getting-started/generic.md)。

## 已安装 SDK：QML API（Preview）

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick)
find_package(HyRemote CONFIG REQUIRED)
```

```qml
import HyRemote

RemoteAccess {
    target: mainWindow
    enabled: true
}
```

部署：

```cmake
hyremote_deploy(TARGET MyQmlApp QML)
```

> **TODO V0.3：** 完成正式产品化后再将 QML API 从 Preview 提升为正式路径。

## 已安装 SDK：QPA（Preview）

应用保持 Qt-only：

```cmake
find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Widgets)
find_package(HyRemote CONFIG REQUIRED)

target_link_libraries(MyExistingApp PRIVATE Qt6::Widgets)

install(TARGETS MyExistingApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyExistingApp QPA)
```

启动：

```text
MyExistingApp -platform hyremote
```

QPA 只对明确声明的 exact Qt/private-ABI 组合成立。当前参考是 Qt 6.8.3。

## 源码接入

也可以把 HyRemote 作为源码子项目：

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)

set(HYREMOTE_BUILD_CPP_API ON CACHE BOOL "" FORCE)
set(HYREMOTE_WITH_GENERIC_PLUGIN ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/HyRemote EXCLUDE_FROM_ALL)

add_executable(MyApp main.cpp)
target_link_libraries(MyApp PRIVATE Qt6::Widgets HyRemote::RemoteAccess)
```

源码接入与 installed SDK 使用同一个产品 API 和部署模型。

同一次 CMake configure 中不要混用：

```text
find_package(HyRemote)
+
add_subdirectory(HyRemote)
```

选择一种 acquisition source 即可。

## 部署

HyRemote 的统一部署入口是：

```cmake
hyremote_deploy(TARGET MyCppApp)
hyremote_deploy(TARGET MyQmlApp QML)
hyremote_deploy(TARGET ExistingQtApp GENERIC)
hyremote_deploy(TARGET ExistingQtApp QPA)
```

正常部署应从应用自己的部署目录运行，不需要把 `QT_PLUGIN_PATH`、`LD_LIBRARY_PATH` 等变量指回 HyRemote/Qt SDK 或构建树。

详见 [`deployment.md`](deployment.md)。

## 安全默认值

V0.1 默认：

- `0.0.0.0:5921`；
- 远程输入关闭；
- `Insecure` 仅允许回环监听；
- `Authenticated` 只有在 HyRemote 构建包含 transport-security capability 且配置了有效 security descriptor 时才可用；当前提供 VNC authentication，但流量不加密；
- 默认 V0.1 build/profile 不代表 authenticated transport 已编译进产品；
- `AuthenticatedEncrypted` 尚未实现，始终在监听器创建前 fail-closed，且不会降级到较弱 profile。

不要把当前产品直接暴露到公网。详见 [`../security.md`](../security.md)。

## 下一步

- C++ API：[`../getting-started/cpp.md`](../getting-started/cpp.md)
- Generic Plugin：[`../getting-started/generic.md`](../getting-started/generic.md)
- QML API（Preview）：[`../getting-started/qml.md`](../getting-started/qml.md)
- QPA（Preview）：[`../getting-started/qpa-proxy.md`](../getting-started/qpa-proxy.md)
- 部署：[`deployment.md`](deployment.md)
- 兼容性：[`../compatibility.md`](../compatibility.md)
- 已知限制：[`../known-limitations.md`](../known-limitations.md)