# 排错

> 语言 / Language：**中文** ｜ [English](../en/guide/troubleshooting.md)

本文覆盖 V1 的 C++ API、QML API 与 QPA 三条路径。请先看产品层面的错误、包元数据与已文档化的策略，再去追内部实现层。

## `find_package(HyRemote)` 找不到包

`find_package(HyRemote CONFIG REQUIRED)` 针对的是**已安装**的 HyRemote 前缀。把该前缀加入 `CMAKE_PREFIX_PATH`，或改用 [`guide/install.md`](install.md) 里的源码获取方式。

不要把 `add_subdirectory(HyRemote)` 与"已生成安装版 `HyRemoteConfig.cmake`"的假设混用。

已安装的 V1 包只暴露**一个**普通 C++ 目标：`HyRemote::RemoteAccess`。Core、QML 后端库与 `qhyremote` 都**不是**可供应用链接的替代目标。

## `RemoteAccess::start()` 返回 false

检查 `lastError()` 并确认：

- 目标是一个**存活的、受支持的** `QWidget` 或 `QQuickWindow`；
- 配置是在**停止状态**下完成的；
- 所选端口合法且未被占用；
- 所构建产品所需的 target adapter 存在；
- 内部 VNC 正确性传输没有在自定义源码构建里被显式关掉。

公开的错误面**有意不暴露** RFB 后端类型。

## QML 的 `enabled: true` 又变回 false

声明式启用是**事务性**的。QML 初次构造期间，`enabled: true` 只是一个请求；真正的共享运行时启动会推迟到组件构造完成，好让目标/策略绑定先稳定下来。

若启动随后失败，`enabled` 会变回 false。此时检查 `errorCode` / `errorString`，并确认绑定的目标是受支持的存活 Quick 目标、且配置的端点可用。

**不要**为了绕开属性顺序而加 `Component.onCompleted: remote.enabled = true`；正常的声明式用法不应需要这类生命周期胶水。

## 查看端连不上

确认该集成方式**确实启动了服务**：

- C++：`start()` 成功；
- QML：`enabled` 保持 true 且状态到达 Running；
- QPA：部署后的应用是以 `-platform hyremote` 启动的，而不是直接用原生平台。

同时确认查看端用的是配置中的回环地址/端口、端口没有被别的进程占用、进程仍在运行、且没有防火墙/安全软件干扰预期的本机/受信连接。

**仅仅构造**永远不会打开 C++ API 的监听器；仅仅 `import` QML 模块也不会启动它。

## 查看端能看到画面，但输入没有反应

远程输入默认关闭。

- C++：在停止状态下调用 `setRemoteInputEnabled(true)`，再启动；
- QML：先设 `remoteInputEnabled: true`，再启用；
- QPA：以 `-platform hyremote:hyremote-input=true` 重新启动。

如果只是特定按键/组合失效，请查 [`input-model.md`](../input-model.md) 与 [`known-limitations.md`](../known-limitations.md)；V1 **不声称**完整的输入法/死键/各国键盘布局一致性。

## 指针坐标不对

记录：目标逻辑尺寸、采集到的帧缓冲尺寸、device-pixel ratio 与缩放状态。坐标映射基于远端帧/目标的几何；过期或不受支持的几何变化**不得**被掩盖成"查看端行为"。

对 QPA，还要记录当时活跃的顶层 surface 与合成画布几何。

## Quick 采集是空白的，或隐藏后停止

公开的异步 Quick 正确性路径依赖一个**可被采集**的 Qt Quick 场景/窗口。隐藏/最小化行为有明确的处理与限制，它**不等价于**合成器层面的桌面采集服务。

先查 [`compatibility.md`](../compatibility.md)、[`known-limitations.md`](../known-limitations.md) 与 QPA 采集分类，再把问题扩大成泛泛的"图形支持"结论。

## QPA 部署说包不可用

已安装的 SDK 必须以如下方式构建：

```text
-DHYREMOTE_WITH_QPA_PROXY=ON
```

`hyremote_deploy(TARGET ... QPA)` 消费的是包自带的可用性/版本/插件元数据。已安装 SDK **有意不导出** `HyRemote::QpaPlatform` 供应用链接。

## QPA 部署拒绝 Qt 版本

V1 中，QPA 与**精确的 Qt 6.8.3** 私有 ABI 耦合。应用/部署的配置必须解析出与所限定 QPA 载荷**完全相同**的 Qt 版本。

**不要**靠改包内文件或手工拷贝 `qhyremote` 绕过这道闸；正确做法是显式去限定另一条 Qt 线。

## 部署后的 QPA 应用找不到 `hyremote`

正常部署后的应用应当在自己的 Qt `plugins/platforms` 树下含有 `qhyremote`，并且**不需要**在 `QT_PLUGIN_PATH` 或 `QT_QPA_PLATFORM_PLUGIN_PATH` 上指向原始 SDK 路径。

如果插件只存在于 SDK 的暂存前缀里，请把它当作**部署缺陷**处理，而不是长期把 SDK 目录加进环境变量。

## Windows 上 Qt 相关测试起不来

运行**构建树内**的测试时，确保匹配的 Qt `bin` 与构建树的 `remoteaccess` 目录在 `PATH` 上可被发现。

对已安装/已部署的验收夹具，规则正好相反：**移除**原始 SDK/构建路径，验证应用能从自己的部署树运行。

## Linux 上构建树测试只有设了 `LD_LIBRARY_PATH` 才通过

在开发者直接跑构建树测试时，把 Qt/构建输出目录放进 `LD_LIBRARY_PATH` 可以是合理的；但它**不是**已安装部署的可接受证据。

`hyremote_deploy()` 必须产出一个**不依赖**原始 HyRemote SDK/构建目录即可解析共享门面与插件/QML 载荷的部署。

## Linux 上 offscreen/Xvfb 通过，但本机桌面行为未知

无头 CI 只证明它实际执行到的行为，**不会**把本机可见的并存或某个桌面 QPA/图形组合升级为"已支持"。

物理本机显示/本地输入与远端并存的验收另行处理。

## 查看端断开后输入看起来还按着

查看端异常断开后，已识别到的按下键/按钮状态会被平衡。如果当前候选仍能在异常断开后复现按住残留，请记录**精确的事件序列**，并按回归处理。

## 慢客户端或恶意客户端

Core 的帧邮箱、RFB 帧交接、GUI 输入投递与未完成握手的存活期都在设计上有界。如果内存/工作量仍然无界增长，请当作**产品缺陷**处理，记录确切的客户端流量与复现步骤；**不要**用调大队列容量替代修复所有权/背压问题。

## 安全提醒

当前 RFB SecurityType None 基线是未认证、未加密的。**不要**仅仅为了验证连通性就把监听器直接暴露到不可信/公网。见 [`security.md`](../security.md)。
