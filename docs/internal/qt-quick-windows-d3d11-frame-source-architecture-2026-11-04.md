# #609 Qt Quick / Windows D3D11 被动帧源架构决策草案（2026-11-04）

**状态：DESIGN_CANDIDATE / NOT_IMPLEMENTATION_AUTHORIZATION**  
**范围：** Windows Qt 6.8.3、默认 D3D11，复用 HyRemote 的 Core + Shared Runtime + Generic/QML/Cpp/QPA 接入和现有 RFB。此文件是架构设计门槛，不是新的功能验收或发布承诺。  
**负责：** Cloud Owner 设计/评审；Local Owner 仅负责后续批准的 Windows 最小可行性探针；RK3588/EGLFS 真机由用户验证。

## 1. 产品问题与已有证据

- 用户要的是 **不改变 Qt 应用正常本机显示、可完整远控 QML/弹窗/多窗口、无需增加外部部署软件** 的远端画面，而不是某一种 OpenGL/D3D 实现。
- [#609 Windows OpenGL ON/OFF 同源对照](https://github.com/skawu/HyRemote/issues/609#issuecomment-6631533345)：Qt6.8.3/MSVC Release/Intel UHD770/OpenGL、1080p Quick、1 Viewer，Host 稳态 CPU **37.5% OFF -> 27.8% ON**（单核等效，少 9.7pp / 约 25.9% 相对值），实际 Qt render **24.0/s -> 25.0/s**，动态 Viewer 在三个取样时刻确有变化。这是 **单机 OpenGL 的实验收益**，不是 Windows 默认 D3D11 的证明；Viewer 真正 FPS/端到端帧龄未测。
- [Draft #829](https://github.com/skawu/HyRemote/pull/829) 的 `HYREMOTE_EXPERIMENTAL_PASSIVE_QUICK_GL` **默认 OFF**，GL `afterRendering` + `glReadPixels` 不可直接移植到 D3D11；`quick_target.cpp` 仍保留 `grabToImage` 正确性回退。该 Draft 不晋升、也不修改已冻结 release。
- [#789](https://github.com/skawu/HyRemote/issues/789) RK3588 / Buildroot / EGLFS 静态单 Viewer 增量约 +101.6pp 单核及 [#567](https://github.com/skawu/HyRemote/issues/567) 大面积条带刷新仍 OPEN；**开发环境无 RK3588 真机**，用户现场验证另行交付，不得将 Windows 数据外推到 ARM。

## 2. 必须满足的产品不变量（FrameSource 合同）

1. 不改应用指定的 RHI（Windows 默认 D3D11 不能强制 `QSG_RHI_BACKEND=opengl`），不要求用户部署独立 Capture Agent，只有原有 HyRemote 组件及 HyRemoteView；不强制用户在应用代码中调用截图 API。
2. 维持 Core / Shared Runtime 一次共享捕获、每 Viewer 有界消费；现有 RFB、输入路由、认证、水印和会话策略不因 FrameSource 后端不同而漂移。
3. 静态场景不因 Viewer 的周期采集需求反复触发全屏离屏重绘；动态场景从**真实已产出**的画面更新，不以拉长轮询间隔牺牲新鲜度。
4. 获取到的帧必须为正确目标（非桌面/其他应用）与可见内容，清晰界定窗口/客户区、DPR、旋转、行序、RGBA/BGRA、premultiplied alpha、HDR/SDR、弹窗/多窗口/遮挡/最小化与隐藏；仅当**完整且已准备好**时发布。不得把缩放错误、旧帧、半帧、黑帧当成功。
5. 帧源记录原始生成/捕获完成时间，缓存复用保留原始内容时间戳，采集请求时间与 Viewer 交付时间另记，避免每次缓存命中伪装“新帧”。有界至多 1 个正在转换/回读及 1 个待交付最新帧（具体队列约束不得弱于既有 Core）；旧帧合并但不能饿死真实变化。
6. 无 Viewer 需求时停止重型捕获；退出/异常、窗口销毁、D3D11 device loss、分辨率/DPR 变化、显示迁移、重连时释放/重建资源，失败后安全回退原 `grabToImage`，回退次数可观测。不能因读取造成 UI/渲染线程明显阻塞。
7. 外部依赖/系统授权/可见捕获边框必须明示；不允许悄悄引入用户必须点选、额外安装或不符合既定产品体验的条件。

## 3. Qt6.8/D3D11 公开边界：可以获取设备，但不能推定可读 backbuffer

- `QSGRendererInterface::graphicsApi()` 判断真实 D3D11；在 **render thread** 可由 `getResource` 查询 `DeviceResource` (`ID3D11Device*`)、`DeviceContextResource` (`ID3D11DeviceContext*`) 和 `RhiSwapchainResource` (`QRhiSwapChain*`)，资源由 Qt 持有、不转移所有权。**`RhiSwapchainResource` 不是 `IDXGISwapChain*`，不应直接 reinterpret_cast。**
- `QQuickWindow::afterRendering` 表示场景图命令已记录但 **尚未提交图形队列**，在这里 `CopyResource` 或读取当前绑定 RenderTarget 并不自动证明像素来自本次完成帧；`afterFrameEnd` 是提交之后的信号，但不意味着可任意安全读取已呈现的 swapchain 或脱离帧生命周期访问 Qt 资源。
- `QRhiResourceUpdateBatch::readBackTexture` 的空纹理 swapchain readback **要求 swapchain 创建时含 `QRhiSwapChain::UsedAsTransferSource`**；Qt Quick 自有 swapchain 是否设置该标志必须在实机查询，不得从头文件存在推导。不能在运行中擅自 `setFlags/createOrResize` Qt 持有的交换链，更不能依赖不受控 Qt private ABI。
- 因此：**直接从 Qt D3D11 backbuffer 抽帧 = 尚未证明可行，现阶段 NO-GO 直接落地。** 有针对性的只读能力预检可以解除部分疑点，但不是证明帧正确/性能合格。

## 4. 候选路线与产品取舍

| 路线 | 实现思路 | 优点 | 不确定性/否决边界 | 本阶段判断 |
| --- | --- | --- | --- | --- |
| A. Qt RHI/原生 D3D11 backbuffer | 利用 render-thread 可获取的设备/上下文，按 Qt 官方允许的实际 swapchain readback 或安全纹理资源完成 GPU→CPU 回读 | 贴近原始 Qt 帧；可能跨 Qt 窗口一致复用 Core | Qt 不向我们公开稳定 `IDXGISwapChain`；transfer-source 标志、提交/读回同步、resize 和 Qt private ABI 限制。不得钩取未受保证的 OM RenderTarget | **条件可行性待验证；禁止直接开工“CopyResource 修复”** |
| B. Windows Graphics Capture（WGC，OS 自带） | 使用官方 `IGraphicsCaptureItemInterop::CreateForWindow(HWND)` 创建指定窗口采集，`Direct3D11CaptureFramePool::CreateFreeThreaded` 接收 `FrameArrived`，只消费真实到达的完成纹理，按需求更新共享帧 | 不要求改变 Qt RHI，系统负责窗口图像采集，不依赖 Qt 内部交换链；不新增独立进程/安装包 | Win10 1903+、会话/策略/边框/系统权限；标题栏/客户区/颜色、resize、延迟，及 **弹窗跨 HWND** 的完整性必须验证。系统 API 可用不代表满足产品 | **优先开展最小能力探针，不直接指定为正式方案** |
| C. 保留当前抓图 | 所有无法证明 A/B 正确的场景使用原 `grabToImage` | 行为兼容性兜底 | 静态 CPU 原始痛点仍在；不能作为“方案 B 已实现” | **长期兜底，不作为性能问题收口** |
| D. 全局桌面复制 / 私有 Qt / 商业模块 | Desktop Duplication、钩 Qt 私有纹理、第三方采集 SDK | 可能绕开某些 API 缺口 | 目标窗口隔离、额外部署/许可、Qt ABI、窗口叠加/安全以及既定产品约束越界 | **本轮不授权** |

**WGC 重大产品门槛：** `GraphicsCaptureSession::IncludeSecondaryWindows` 直到 **Windows 11 24H2 (build 26100)** 才提供，且默认 false；较早 Windows 的单 HWND 采集不能自动宣称包含 Qt 的独立 popup/tool window。无边框采集涉及 OS 授权与相应条件，不应默默禁用捕获边框。故不能把 WGC 视为“全部 Windows 版本完整弹窗方案”；如果不能在约定支持的 Windows 版本内维护完整内容，则 B 作为通用产品默认路由 **NO-GO**，必须再评估与 Shared Runtime 的多窗口合成协作或其他帧源。

## 5. 推荐路线及最小验证顺序

### 设计裁定（待工程事实闭环）

- 以 **`IQuickNativeFrameSource`（概念接口，不是本轮立即新增公共 C++ API）** 把 Backend 能力、需求生命周期、`RemoteFrame`、异常/失效和指标封装在 Runtime 私有适配层；既有捕获源选择遵循 `qualified native source -> correct legacy fallback`。不把 Windows 依赖扩散进 Core/RFB/四个接入入口。
- **第一验证对象：Windows 自带 WGC 的独立 Qt Quick 目标窗口采集能力**；原因是它更接近“采集已经存在的窗口画面”且不强迫改变用户的 D3D11 后端；但弹窗与捕获边框是先决产品门槛。同步仅做只读 Qt RHI swapchain flags/资源的能力检查，避免绕过公开 API 猜实现。
- 实际工程进入顺序为：P0 Windows OS/API/Qt 能力核验（无产品代码）→ P1 单窗口静态图像/生命周期 → P2 动态新鲜度 + 同后端 CPU/Qt render/Viewer 对照 → P3 弹窗/多窗口、尺寸/DPR、权限边框/安全、显示与失效恢复 → 产品准入。**P0 不通过直接 NO-GO/改方案，不得通过反复试错或扩大矩阵掩盖产品限制。**
- 即使 WGC 有系统 `FrameArrived` 也不等于一定无旧帧；候选必须保存源内容时间戳并验证静态不重复重绘、动态画面更新速度/时序。捕获帧池、COM、设备和回调只在有需要时活跃，禁止静默持续采集桌面。

### 下一项 Local Owner 可执行的单一 P0 任务（设计先于实现）

在现有 Windows Qt6.8.3/MSVC 机器 **只读核验/最小独立诊断**：
1. 真实 Windows 版本/build、SDK 与 WGC `GraphicsCaptureSession::IsSupported()`、`IGraphicsCaptureItemInterop`、`CreateFreeThreaded` 的可用性；说明是否需要用户选择 UI、捕获边框可能造成的产品影响（不能尝试无授权规避）。
2. 在真实 **默认 D3D11** Qt Quick 窗口的 render-thread 仅查询 `graphicsApi`、`DeviceResource`、`DeviceContextResource`、`RhiSwapchainResource` 和（如现有 Qt Kit 允许只读访问）`QRhiSwapChain::flags()`，不要调用 `setFlags/createOrResize` 或 `CopyResource`。
3. 记录是否拥有独立 Qt popup HWND、当前 Qt Kit 所需 private headers；P0 **不录制用户内容、不读像素、不接 Viewer、不跑性能、不改产品源码**。若独立诊断需要编译，仅编译诊断程序。
4. 以 `WGC_SUPPORTED/NOT_SUPPORTED/UNKNOWN`、`SWAPCHAIN_TRANSFER_SOURCE_YES/NO/UNKNOWN`、`POPUP_WINDOW_MODEL`、`BORDER_POLICY_UNKNOWN/ACCEPTABLE/CONFLICT` 回报 #609；有任何无法判定的项目记 UNKNOWN，不得编造通过。
5. Cloud Owner 根据 P0 结果先确认产品方案，再授权最小隔离 FrameSource 实现；PR #829 保持 Draft，默认 OFF，发布分支不动。

## 6. 证据与源文档

- [Qt 6.8 QSGRendererInterface（资源枚举、线程/所有权）](https://doc.qt.io/qt-6.8/qsgrendererinterface.html)
- [Qt 6.8 QQuickWindow（afterRendering / afterFrameEnd 的语义）](https://doc.qt.io/qt-6.8/qquickwindow.html)
- [Qt 6.8 QRhiResourceUpdateBatch（swapchain readback 的前置条件）](https://doc.qt.io/qt-6.8/qrhiresourceupdatebatch.html)
- [Qt 6.8 QRhiSwapChain（UsedAsTransferSource 标志）](https://doc.qt.io/qt-6.8/qrhiswapchain.html)
- [Microsoft IGraphicsCaptureItemInterop::CreateForWindow（Win10 1903+）](https://learn.microsoft.com/en-us/windows/win32/api/windows.graphics.capture.interop/nf-windows-graphics-capture-interop-igraphicscaptureiteminterop-createforwindow)
- [Microsoft Direct3D11CaptureFramePool::CreateFreeThreaded](https://learn.microsoft.com/en-us/uwp/api/windows.graphics.capture.direct3d11captureframepool.createfreethreaded)
- [Microsoft GraphicsCaptureSession::IncludeSecondaryWindows（Win11 24H2+）](https://learn.microsoft.com/en-us/uwp/api/windows.graphics.capture.graphicscapturesession.includesecondarywindows)
- [Microsoft GraphicsCaptureSession::IsBorderRequired](https://learn.microsoft.com/en-us/uwp/api/windows.graphics.capture.graphicscapturesession.isborderrequired)

## 7. P0 真实环境结果与设计复核（2026-11-04）

来源：[本地 Owner #609 P0](https://github.com/skawu/HyRemote/issues/609#issuecomment-6632184423)。这是一份**接口存在性/窗口模型证据**，未创建实际 WGC 捕获会话、未读取测试像素、未连接 Viewer，因此不能记为 `D3D11_NATIVE_CAPTURE_PASS`。

| 项目 | 观察 | 工程解释 |
| --- | --- | --- |
| Windows / Qt | Windows 11 10.0.26200 (UBR 9711)，Qt 6.8.3/MSVC Release；主窗口与独立 Popup 运行时确认为 D3D11 | 已证明 Qt 默认 D3D11，符合后续 WGC 目标环境 |
| WinRT 类 | `RoGetActivationFactory` 激活 `GraphicsCaptureSession / Item / Picker / Access` 均 `S_OK` | **只证明类存在/激活工厂可取得**；`GraphicsCaptureSession::IsSupported()` 尚未实际调用，`CreateForWindow` 和 `CreateFreeThreaded` 没有创建成功证据 |
| 默认视频设备 | WMI 显示 OrayIddDriver 间接显示设备，另有 Intel UHD770 | WMI 设备名称**不是** Qt D3D11 Device 所在 adapter LUID；若要跨设备共享或拷贝，须用设备实际 `IDXGIDevice::GetAdapter()->GetDesc()->AdapterLuid` 核对 |
| Qt swapchain | `UsedAsTransferSource=UNKNOWN` | `QRhiSwapChain::flags()`（Qt GuiPrivate 类型）是 Qt 读回许可标志；不能仅从 DXGI `GetDesc1` 或 D3D11 texture `BindFlags` 推导 Qt 自身已创建的 QRhi 标志，更不能为实验修改 Qt-owned swapchain |
| Qt Popup | item 型 Popup 无独立 HWND；`Popup.Window` 拥有可见独立 HWND | Qt Window Popup 实测 `exStyle=0x00080088`，准确解码为 `WS_EX_LAYERED(0x80000) + WS_EX_TOOLWINDOW(0x80) + WS_EX_TOPMOST(0x8)`，**不包含** `WS_EX_NOACTIVATE(0x08000000)` 或 `WS_EX_WINDOWEDGE(0x100)`。可激活性必须另测，不可从该掩码错误推断 |
| WGC secondary | OS build 26200 超过 `IncludeSecondaryWindows` 的 build 26100 门槛 | 仍需逐项验证 `ApiInformation` / 会话实际支持，设置 true 才可能包含 `WS_POPUP` / `WS_EX_TOOLWINDOW` 的相交子窗口；**超出主窗口矩形的部分按微软合同仍会裁切**，不满足无条件完整 Popup |
| 边框与授权 | `GraphicsCaptureAccess` 可激活，但未请求授权 | `IsBorderRequired=false` 并非零成本操作；微软要求声明 `graphicsCaptureWithoutBorder` 并经 `RequestAccessAsync(Borderless)` 用户同意，拒绝后边框仍显示。项目不能以 API 存在宣称无需授权或捕获边框不可见 |

资料：[Microsoft Win32 扩展样式](https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles)、[WGC IsSupported 检查](https://learn.microsoft.com/en-us/windows/apps/develop/media-authoring-processing/screen-capture)、[WGC secondary 窗口边界](https://learn.microsoft.com/en-us/uwp/api/windows.graphics.capture.graphicscapturesession.includesecondarywindows)、[WGC 边框权限](https://learn.microsoft.com/en-us/uwp/api/windows.graphics.capture.graphicscapturesession.isborderrequired)。

### P0 审查裁定与下一验证

**P0=PASS_FOR_ENV_DISCOVERY_ONLY / WGC_SESSION=NOT_RUN / PRODUCT_GATE=OPEN。** 继续 WGC 但不将其定为正式后端；若独立系统窗口方案无法覆盖全部 Qt Popup/权限要求，保留 Legacy fallback，转回 Qt 原生帧/同进程多窗口 FrameSource 设计选择。不得在未验证前为 WGC 业务行为编码、绕过捕获授权，或修改产品既有多窗口/截图语义。

**仅授权一项新的 P0.5：隔离系统级单窗口采集可行性试验，不是 HyRemote 功能实现。**

- 在现有 Windows 11/Qt6.8.3 环境单次启动测试用、无隐私内容的 1080p Quick 窗口，显示静态独特标记、一个 item Popup 和一个跨主窗口边缘的 `Popup.Window`，沿用默认 D3D11。不要连接 Viewer、不要修改 Runtime/Generic/发布代码。
- 独立诊断进程先调用并记录 `GraphicsCaptureSession::IsSupported()`；如果 false，立即 `NO_GO` 并停止。对**已知测试目标 HWND** 调用 `IGraphicsCaptureItemInterop::CreateForWindow`（记录 HRESULT）；使用独立测试 D3D11 设备建立 `Direct3D11CaptureFramePool::CreateFreeThreaded`、`CreateCaptureSession`，在可用且获授权的情况下短时捕获并**只读取这一测试窗口的几帧**，记录尺寸、像素方向、item Popup 与窗口 Popup 的包含/缺失/裁切、边框/权限结果、HRESULT 和系统对话框。检查 `IncludeSecondaryWindows` 的真实支持与 true/false 效果即可，不进行性能矩阵、额外安装或补丁实现。输出图像仅为测试场景，不截取用户桌面。
- 目标上设备使用 `IDXGIDevice::GetAdapter` 记录 LUID，若 Qt 对应设备 LUID 未拿到则 `QT_LUID=UNKNOWN`；不要为取得它引入 Qt private hook，优先判断 WGC 自身是否可行。实验遇到权限提示必须依赖合法用户同意；无法同意/策略拒绝即如实记录并停止，不自动绕过。
- 自行限时结束并释放 WinRT/D3D/窗口资源。结果仅写入 #609：`IsSupported`、`CreateForWindow HRESULT`、`FRAME_ARRIVED`、`MAIN_PIXELS`、`POPUP_ITEM`、`POPUP_WINDOW_INSIDE/OUTSIDE`、`SECONDARY_API/SETTING`、`BORDER/CONSENT`、`WGC_ADAPTER_LUID`。完成即停，由云端 Owner 判断是否进入产品路线设计终审。

## 8. 当前结论

```text
DESIGN_DIRECTION=APPROVED_B_PASSIVE_EXISTING_FRAME
D3D11_NATIVE_QT_DIRECT_READBACK=NO_GO_IMPLEMENTATION_UNTIL_PROOF
D3D11_WGC=CONDITIONAL_CANDIDATE_P0_ENV_PASS_P0_5_SESSION_NOT_RUN
WINDOWS_OPENGL_ON_OFF=PASS_SINGLE_PAIR_NOT_D3D11
RK3588_EGLFS=USER_SITE_VALIDATION_PENDING
ISSUE609=P1_OPEN
PR829=DRAFT_UNMERGED_DEFAULT_OFF
RELEASE=UNCHANGED
```
