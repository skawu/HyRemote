# QPA 接入

> 语言 / Language：**中文** ｜ [English](../en/getting-started/qpa-proxy.md)

QPA 是 HyRemote 的专用零代码接入路径。它适合确实需要从 Qt platform entry 进入的应用；如果 Generic Plugin 已经能满足需求，应优先使用 [`generic.md`](generic.md)，因为 Generic 使用 Qt public plugin API，不引入 QPA private ABI 约束。

当前产品状态：peer route。

## 产品模型

QPA 不把应用切换成一个 replacement-only 的 offscreen/qvnc 平台。`qhyremote` 使用 Factory Trampoline 创建并返回原生 Qt platform integration，同时把 HyRemote Shared Runtime 挂接到应用旁路：

```text
MyApp -platform hyremote
        |
        v
qhyremote Factory Trampoline
        |
        +--> native qwindows / qxcb platform integration
        |
        +--> HyRemote Shared Runtime
```

本机显示和本地输入仍由原生 Qt platform 负责。

## 当前参考环境

QPA 依赖 Qt private ABI，因此兼容性按**精确 Qt patch**声明。

当前参考：

- Windows x86_64 + Qt 6.8.3 exact + `qwindows`；
- Linux x86_64 + Qt 6.8.3 exact + `qxcb`。

不要从这些组合推断其它 Qt patch、Wayland、EGLFS、macOS 或其它 native platform 已经受支持。

> **TODO V0.3/V0.4：** 完成正式产品化、更多精确 Qt/OS 组合和物理本地+远程并存资格后，再扩大 QPA 支持声明。

精确状态见 [`../compatibility.md`](../compatibility.md)。

## 保持应用 Qt-only

应用本身不需要包含或链接 HyRemote：

```cmake
find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Widgets)

add_executable(MyExistingApp main.cpp)
target_link_libraries(MyExistingApp PRIVATE Qt6::Widgets)
```

HyRemote 只通过部署和启动参数进入。

## 部署

```cmake
find_package(HyRemote CONFIG REQUIRED)

install(TARGETS MyExistingApp
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .
)

hyremote_deploy(TARGET MyExistingApp QPA)
```

部署助手加入：

- `qhyremote` platform plugin；
- Shared Runtime；
- 匹配的原生 Qt platform plugin；
- 所需 Qt runtime dependencies。

应用可执行文件仍然不链接 `HyRemote::RemoteAccess`。

正常部署不应依赖 SDK 专有的 `QT_PLUGIN_PATH`、`QT_QPA_PLATFORM_PLUGIN_PATH` 或构建树库路径。

详见 [`../guide/deployment.md`](../guide/deployment.md)。

## 启动

Windows：

```powershell
.\bin\MyExistingApp.exe -platform hyremote
```

Linux：

```sh
./bin/MyExistingApp -platform hyremote
```

默认行为：

- 原生 platform 继续负责本机显示和输入；
- 默认监听 `0.0.0.0:5921`；
- 远程输入默认关闭；
- 支持的应用顶层 surface 进入一个逻辑远程会话；
- 正常查看端断开/重连不需要重启 Qt 应用。

## 启用远程控制

QPA 的远程输入属于启动策略：

```text
-platform "hyremote:hyremote-input=true"
```

省略该参数时保持只看。

如果应用需要在运行时动态调整复杂远程访问策略，应优先选择 C++ API 或 QML API，而不是依赖 QPA 私有控制对象。

## 地址与端口

QPA 使用与 Shared Runtime 相同的监听语义：

```text
hyremote-address=<numeric-ip-address>
hyremote-port=<1..65535>
hyremote-input=<0|1|false|true|off|on|no|yes>
```

示例：

```text
-platform "hyremote:hyremote-address=<host-lan-ip>:hyremote-port=5921:hyremote-input=false"
```

非法参数失败关闭。

地址族和平台限制见 [`../known-limitations.md`](../known-limitations.md)。

## 多窗口行为

QPA 把一个 Qt 应用表示成一个逻辑远程会话，而不是每个窗口建立一个监听器。

受支持的应用自有顶层 QWidget / QQuickWindow surface 可以进入或离开远程画布；打开或关闭支持的对话框、工具窗口或第二个顶层窗口不应要求重启监听器。

任意 foreign/native OS window 不自动属于 QPA 支持范围。

## Widgets / Qt Quick

QPA 与其它 frontend 共用 Runtime target adapter：

- QWidget 使用 Widgets adapter；
- QQuickWindow 使用 Quick adapter；
- QOpenGLWidget、QQuickWidget、Quick3D、自定义 FBO 等复杂组合只按明确兼容矩阵声明，不从基础 Widgets/Quick 支持推断。

详见 [`../compatibility.md`](../compatibility.md)。

## 安全边界

QPA 复用 Shared Runtime 的安全策略：

- 默认 `0.0.0.0:5921`；
- 远程输入默认关闭；
- `Insecure` 未认证、未加密：监听器面向**可信 LAN**，**不适合暴露到 Internet**；
- `Authenticated` 可使用 RFB VNC authentication，但当前数据流不加密；
- `AuthenticatedEncrypted` 在加密后端不可用时失败关闭。

不要把当前产品直接暴露到公网。详见 [`../security.md`](../security.md)。

## 排错

### 找不到 `qhyremote`

确认应用使用了：

```cmake
hyremote_deploy(TARGET MyExistingApp QPA)
```

正常部署树应包含平台插件目录中的 `qhyremote` 与匹配的 native platform plugin。

### Qt 版本被拒绝

QPA 是 exact-private-ABI 路径。当前参考为 Qt 6.8.3；不同 patch 不应被静默当成兼容。

### 查看端能看但不能控制

这是默认行为。只有确实需要远程控制时才启用 `hyremote-input=true`。

### Generic 和 QPA 怎么选

优先 Generic；只有当应用需要 QPA/platform-entry 行为时再选 QPA。

## 示例

新的 Example 体系会单独教学 Generic 与 QPA，避免把两种零代码方案混为一谈。

> **TODO V0.3：** 完成 `examples/learning/07-zero-code-qpa/{widgets-app,quick-app}` 的双语、品牌化正式示例。