# QPA Proxy 接入

> 语言 / Language：**中文** ｜ [English](../en/getting-started/qpa-proxy.md)

本文面向**已存在的 Qt 应用**：希望以**零改动或最小改动**获得 HyRemote 远程访问能力的场景。

QPA 是 HyRemote V1 三种必备接入方式中的第三种。它**不是**"只做替换/无头"的 VNC 平台：HyRemote 把正常的
平台行为继续交给原生 Qt 平台集成，并**在其旁**加上同一份共享远程访问运行时。

一个完整的普通应用示例见 [`examples/qpa-proxy-existing-app`](../../examples/qpa-proxy-existing-app)。

## V1 限定参考线

QPA 是**有意**与 Qt 私有 API 版本耦合的：

- Qt：**精确 6.8.3**；
- Windows x86_64 委托平台：`qwindows`；
- Linux x86_64 委托平台：`qxcb`。

采集族的分类维护在 [`internal/qpa-capture-classification-qt-6.8.3.md`](../internal/qpa-capture-classification-qt-6.8.3.md)；
总体证据矩阵见 [`compatibility.md`](../compatibility.md)。

**不要**由此推断支持其它 Qt 补丁/次版本、Wayland、EGLFS、OpenHarmony 或任意外来/原生窗口。

## 安全默认值

- 监听地址：回环；
- 端口：5921；
- 远程输入：关闭；
- 本机显示/输入：原生平台委托仍为权威；
- 有界 RFB 正确性基线：未配置认证档时为 `SecurityType None` 且未认证；配置认证档后启用 RFB VNC 认证；两种情况下数据流都不加密。

- 监听地址：回环；
- 端口：5921；
- 远程输入：关闭；
- 本机显示/输入：原生平台委托保持权威；
- 当前有界 RFB 正确性基线：`SecurityType None`，因此未认证、未加密。

## 1. 带 QPA 构建并安装 HyRemote

使用**同一套**包含私有 Gui 开发目标的 Qt 6.8.3 SDK。正常的 HyRemote 产品选项已经会构建共享 C++ 运行时及其
Widgets/Quick 适配器；启用 QPA 只需再加**一个**产品选项：

```sh
cmake -S . -B build-qpa \
  -DCMAKE_PREFIX_PATH=<qt-6.8.3-prefix> \
  -DCMAKE_INSTALL_PREFIX=<hyremote-prefix> \
  -DHYREMOTE_WITH_QPA_PROXY=ON
cmake --build build-qpa --config Release
cmake --install build-qpa --config Release
```

得到的 V1 包在该方式下只有**一种固定的 HyRemote 运行时形态**：

- `qhyremote` —— Qt platform MODULE；
- `HyRemoteRemoteAccess` —— 该模块内部使用的共享 HyRemote 运行时。

Core、Session、采集、输入与传输对象**不是**可单独选择的 QPA 部署项。

## 2. 让应用保持普通 Qt 应用

应用**不**包含、也**不**链接 HyRemote。例如：

```cmake
find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Widgets)

add_executable(MyExistingApp main.cpp)
target_link_libraries(MyExistingApp PRIVATE Qt6::Widgets)
```

在启用代理之前，先用 `-platform windows` 或 `-platform xcb`（按平台）确认应用的原生行为正常。

## 3. 通过唯一那个 HyRemote 助手部署

```cmake
find_package(HyRemote CONFIG REQUIRED)

install(TARGETS MyExistingApp
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .
)

hyremote_deploy(TARGET MyExistingApp QPA)
```

这里的 `find_package(HyRemote)` 只提供打包元数据；它**不会**给 `MyExistingApp` 增加 HyRemote 链接依赖。

部署职责依然简单：

1. Qt 负责部署应用运行时与原生 `qwindows` / `qxcb` 委托；
2. HyRemote 负责加上 `qhyremote` 以及它所依赖的共享 `HyRemoteRemoteAccess` 运行时。

助手使用 Qt 的部署支持来解析运行时依赖：它**不**修改用户的 Qt SDK ✓、**不**硬编码其安装路径 ✓、也**不**要求应用知道
HyRemote 的运行时文件名 ✓。

QML 应用可以使用同一个助手：

```cmake
hyremote_deploy(TARGET MyQmlApp QML QPA)
```

## 4. 以安全的"只看"默认值启动

Windows：

```powershell
.\bin\MyExistingApp.exe -platform hyremote
```

Linux：

```sh
./bin/MyExistingApp -platform hyremote
```

一个正确部署的应用**不需要**任何 HyRemote 专有环境变量。特别是：正常部署路径**不得**要求 `QT_PLUGIN_PATH`、
`QT_QPA_PLATFORM_PLUGIN_PATH` 或某个 SDK 专有的运行时库路径。

预期行为：

- 原生委托继续拥有本机显示/输入；
- 受支持的应用 surface 组成**一个**远程会话；
- 默认监听仍是 `loopback:5921`；
- 远程输入保持关闭；
- 受支持的次级窗口/对话框可以进入与离开远程画布，而**无需重启**监听器。

## 5. 查看端连接/重连

用 RFB/VNC 查看端连接 `127.0.0.1:5921`，关闭后再连。正常的查看端断开/重连**不应要求**重启应用。

共享传输的正确性门禁还要求：查看端异常断开后，按住状态的远端按键/按钮必须被平衡。

## 6. 显式启用远程控制

在零改动的 QPA 方式下，远程输入属于**启动策略**：

```text
-platform "hyremote:hyremote-input=true"
```

本机输入仍走原生平台路径。省略该参数即回到"只看"。

这里**有意不提供**供应用代码使用的私有 QPA 控制对象。需要运行期策略控制的应用，应当使用公开的 C++ 或 QML 产品 API，
而不是平台插件内部的私有接口。

## 7. 可选的地址与端口参数

`hyremote-address` **只接受数字地址**；逐地址的实测行为——包括 IPv6 通配地址 `::` 在本平台是**仅 IPv6** 监听而非双栈、
以及未分配地址或端口被占用会在到达 `Running` 之前失败且不留监听——见
[`known-limitations.md`](../known-limitations.md#listener-address-family-and-reachability)。QPA 的参数路径与 C++/QML
接入解析到的是**同一套**监听语义。

```text
hyremote-address=<numeric-ip-address>
hyremote-port=<1..65535>
hyremote-input=<0|1|false|true|off|on|no|yes>
```

示例：

```text
-platform "hyremote:hyremote-address=127.0.0.1:hyremote-port=5921:hyremote-input=false"
```

非法的 HyRemote 取值一律**失败关闭**。

## 8. 安全边界

当前 RFB 基线在未配置认证档时通告 `SecurityType None`；配置认证档后选择 RFB VNC 认证。**不要**把当前 V1 正确性基线直接绑到
不可信网络或公网，也**不要**把它描述成已加密的远程支持 —— 认证不等于加密。

## 9. 多窗口行为

QPA 把一个 Qt 应用表示成**一个逻辑远程会话**，而不是"每窗口一个监听器"。受支持的应用自有顶层 surface 会被
组合进该会话；打开、关闭或移动受支持的对话框、工具窗口、`QWidget` 弹出菜单或第二个 `QQuickWindow`，**本身不得**
重启监听器。

同一个 `QQuickWindow` 内的 Qt Quick 内容仍属于该窗口的场景，不会变成重复的远程 surface。任意外来的原生 OS 窗口
**不在** V1 范围内。

## 10. 采集族限制

不要在 `QWidget`、`QOpenGLWidget`、`QQuickWindow`、Quick3D、自定义 FBO 与任意原生窗口之间泛化证据。权威的生产分类见
[`internal/qpa-capture-classification-qt-6.8.3.md`](../internal/qpa-capture-classification-qt-6.8.3.md)。

当前边界包括：

- QWidget 的正确性采集使用 `QWidget::render()`；
- QQuickWindow 使用公开的 `contentItem()->grabToImage()`；
- Quick3D/自定义 Quick FBO 的证据是**后端特定**的；
- 混合 `QQuickWidget` 组合仍与具体配置相关；
- 没有限定适配器的通用 `QWindow`/`QOpenGLWindow`/外来原生 surface **不会被静默声称**支持。

## 11. 仅限构建树内诊断

直接从开发构建树运行时，手工设置 `QT_PLUGIN_PATH=<hyremote-build-plugin-root>` 可能有用；它**不属于**已安装产品的契约。
如果**部署后**的应用还需要它才能找到 `qhyremote`，请按**部署缺陷**处理。

## 12. 排错

### 平台插件缺失

确认部署使用了 `hyremote_deploy(TARGET ... QPA)`，且已安装应用中包含：

```text
plugins/platforms/qhyremote.dll       # Windows
plugins/platforms/libqhyremote.so     # Linux
```

不要为了掩盖打包缺陷而长期把 `QT_PLUGIN_PATH` 指向 SDK。

### 部署助手拒绝 Qt

QPA 要求精确限定的 Qt 6.8.3 线。缺 QPA 包、或消费者 Qt 版本不同，都会**显式失败**。

### 插件存在但加载不了

确认部署树里含有 `HyRemoteRemoteAccess`，且应用/代理/原生 Qt 运行时来自**同一条**限定 Qt 线。用户**不应**需要拷贝 Core 或后端库。

### 查看端连上了但不能控制

这是默认行为。只有在确实需要远程控制时才加 `hyremote-input=true`。

### 受支持的窗口变化导致查看端断开

这违反了"一个会话内的 surface 连续性"要求，应按 **QPA 回归**处理。

## 13. 部署验证要求

干净的已安装 SDK QPA 消费者必须证明：一个普通的纯 Qt 可执行文件能用该助手部署、能从**自己的应用树**里找到 `qhyremote`、
能加载共享 HyRemote 运行时、能连接/重连 RFB 查看端，且**全程不需要** SDK 路径或插件路径的覆盖设置。

无头/宿主 CI **不能**证明物理原生的本机显示/本地输入与远端并存——那是另一件必须在真实本机环境验证的事。
