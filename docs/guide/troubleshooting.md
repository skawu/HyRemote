# 排错

> 语言 / Language：**中文** ｜ [English](../en/guide/troubleshooting.md)

本文覆盖 C++ API、Generic Plugin、QML API 与 QPA 四条产品路径。优先检查产品级错误、部署结果和兼容矩阵，不需要先进入 HyRemote 内部实现。

## `find_package(HyRemote)` 找不到包

`find_package(HyRemote CONFIG REQUIRED)` 面向 installed SDK。

确认：

- HyRemote 安装前缀已加入 `CMAKE_PREFIX_PATH`；
- 当前应用没有同时混用 installed SDK 与 `add_subdirectory(HyRemote)`；
- 使用的是与当前 Qt/toolchain 匹配的 HyRemote SDK。

源码接入见 [`install.md`](install.md)。

## `RemoteAccess::start()` 返回 false

检查 `lastError()`，并确认：

- target 是存活且受支持的 `QWidget` 或 `QQuickWindow`；
- 配置在 Runtime 停止状态完成；
- 端口合法且未被占用；
- 监听地址是有效数字 IP；
- 当前安全 profile 的配置完整；
- `AuthenticatedEncrypted` 没有被误当成当前已实现能力。

如果选择 `AuthenticatedEncrypted`，V0.1 会按设计返回安全能力不可用并且不打开监听器。

## QML `enabled: true` 又变回 false

QML 的启用是显式请求。若 Runtime 启动失败，`enabled` 会回到 false。

检查：

- `target` 是否是有效的 Quick 顶层窗口；
- 端口/地址是否可用；
- 安全配置是否允许启动；
- `errorCode` / `errorString` 的产品级错误。

正常使用不需要通过 `Component.onCompleted` 手工补启动逻辑。

## Generic Plugin 没有生效

先确认部署使用了：

```cmake
hyremote_deploy(TARGET MyApp GENERIC)
```

然后确认运行时通过 Qt generic-plugin 机制激活，例如：

```text
MyApp -plugin hyremote
```

如果插件没有加载，检查应用部署树中的 Generic Plugin 目录和 Qt plugin search path。

Generic 的关键特征是**保持原生 Qt platform identity**。如果应用从 `windows`/`xcb` 等正常 platform 变成 `hyremote`，那不是正确的 Generic 路径。

## 查看端连不上

先确认对应 frontend 已经真正启动：

- C++：`start()` 成功；
- Generic：Generic Plugin 已成功加载；
- QML：`enabled` 保持 true 且状态到达 `Running`；
- QPA：应用以 `-platform hyremote` 启动。

同时确认：

- 查看端使用正确地址/端口；
- 默认地址是 `0.0.0.0:5921`；
- 端口没有被其它进程占用；
- 应用进程仍在运行；
- 防火墙/安全软件没有拦截预期连接。

## 查看端能看到画面，但输入没有反应

远程输入默认关闭。

- C++：`setRemoteInputEnabled(true)`；
- QML：`remoteInputEnabled: true`；
- Generic：Generic specification 中设置 `input=true`；
- QPA：`-platform "hyremote:hyremote-input=true"`。

如果只有特定按键、输入法或组合键异常，查看 [`../input-model.md`](../input-model.md) 和 [`../known-limitations.md`](../known-limitations.md)。

## 查看端连接后画面为空/不完整

确认应用目标属于当前兼容范围：

- QWidget top-level；
- QQuickWindow；
- 或兼容矩阵中明确声明的复杂组合。

QOpenGLWidget、QQuickWidget、Quick3D、自定义 FBO、foreign/native windows 不应从基础 Widgets/Quick 支持自动推导。

详见 [`../compatibility.md`](../compatibility.md)。

## 指针坐标不正确

记录以下信息：

- 目标逻辑尺寸；
- frame 像素尺寸；
- device-pixel ratio；
- 当前缩放；
- resize 前后的窗口几何。

对于 QPA/自动 surface 组合，还要确认当前远程画布与顶层 surface 几何。

## 断开后按键/按钮像是还按着

HyRemote 会在远端异常断开和 Runtime stop 时清理已识别的 held key/button state。

如果仍可稳定复现，请记录：

- viewer；
- 按下/释放顺序；
- 是否异常断开；
- 是否同时存在第二个 viewer；
- 应用目标类型。

这类行为应视为产品缺陷，而不是让应用自行补发按键释放。

## QPA 部署说不可用

确认生成 HyRemote SDK 时启用了：

```text
-DHYREMOTE_WITH_QPA_PROXY=ON
```

同时确认消费者使用**精确 Qt 6.8.3** 和匹配的 private Gui 开发组件。

QPA 不导出一个供应用链接的 `HyRemote::QpaPlatform` target；应用本身仍是 Qt-only。

## QPA 拒绝 Qt 版本

这是预期的 fail-closed 行为。

QPA 使用 Qt private ABI，当前参考为 **Qt 6.8.3 exact**。不要通过手工复制 `qhyremote` 或修改 package metadata 绕过版本约束。

如果需要另一条 Qt private-ABI 线，应先完成对应资格化。

## 部署后的应用找不到 HyRemote Plugin / Runtime

正常部署应该从应用自己的部署树运行，而不是依赖原始 SDK。

不要长期通过这些方式“修好”部署：

- `QT_PLUGIN_PATH` 指回 Qt/HyRemote SDK；
- `QT_QPA_PLATFORM_PLUGIN_PATH` 指回构建目录；
- `LD_LIBRARY_PATH` 指回 HyRemote build tree；
- 手工复制内部 Core/transport/capture 文件。

重新检查 [`deployment.md`](deployment.md) 中对应的 `hyremote_deploy()` 调用。

## Windows 运行时找不到 Qt/HyRemote DLL

确认应用已经执行产品部署，而不是只完成编译。

installed/deployed 应用应从自己的目录解析：

- Qt runtime；
- `HyRemoteRemoteAccess`；
- 对应 Generic/QPA/QML payload。

不要把开发机 Qt `bin` 永久加入产品环境来掩盖部署缺失。

## Linux 只有设置 `LD_LIBRARY_PATH` 才能运行

构建树调试时临时设置 `LD_LIBRARY_PATH` 可以帮助定位问题，但正式部署不应该依赖原 HyRemote/Qt SDK 路径。

如果 deployed app 离开 build tree 就失败，应按部署问题处理。

## Headless/offscreen 能跑，但本机显示行为未知

Headless/offscreen 只说明对应代码路径能运行。

它不自动证明：

- 物理显示器正常；
- 本地键鼠正常；
- QPA native delegate 的全部行为正常；
- 特殊 GPU/rendering path 已经兼容。

> **TODO V0.4：** 完成最终物理 Windows/Linux local + remote coexistence qualification。

## 慢客户端导致资源增长

HyRemote 的 frame handoff、transport 和输入路径设计为有界。

如果一个慢客户端能够让内存、队列或工作量持续无界增长，应视为产品缺陷。不要简单通过扩大队列上限掩盖 backpressure/ownership 问题。

## 安全问题

V0.1 不提供加密 RFB stream。

如果你遇到：

- 非回环启动被拒绝；
- `AuthenticatedEncrypted` 无法启动；
- viewer password 与预期不一致；

先阅读 [`../security.md`](../security.md)。

不要为了“先连通”而把当前产品直接暴露到公网。

## 仍然无法定位

请记录至少：

- HyRemote 版本/commit；
- Qt 精确版本；
- OS/architecture；
- integration frontend；
- Widgets / Quick 目标类型；
- viewer 与版本；
- 启动参数；
- `lastError()` / 日志；
- 是否为 source build 或 installed SDK；
- 是否使用 `hyremote_deploy()`。

这些信息足以让问题首先按产品边界定位，而不是从内部模块猜测。