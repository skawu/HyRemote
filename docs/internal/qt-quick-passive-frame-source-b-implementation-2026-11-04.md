# #609 方案 B 已获 Human 批准：Qt Quick 被动原生帧来源 — 第一阶段实验设计

> **批准：** 2026-11-04 Human 明确“批准B”。这批准的是以已存在的真实渲染帧为输入、保留完整产品行为与回退的技术方向，**不是**未验证的特定 OpenGL、D3D、Qt private 或 Qt 商业模块可直接进入发布。
> **本分支性质：** Draft / opt-in 原型；默认 OFF、不影响正常 SDK 或冻结 RC；现阶段没有 CPU/FPS 实测收益声明。

## 背景事实与目标

[#609 Q0/Q1 实测](https://github.com/skawu/HyRemote/issues/609#issuecomment-6628044624)：
Qt Quick 1080p 无 Viewer 静态画面，0 次 `grabToImage()`、仅 1 次真实渲染、Host CPU 0.14% 单核。
静态连接 1 Viewer 后，33.3s 记录 247 次 `grabToImage()`（约 7.42/s）、269 次帧通知、Host CPU 10.99%。
Qt 的帧生命周期通知可以被 HyRemote 自己的 offscreen grab 激发，故**不得**使用“afterRendering→再 grabToImage”作为变化驱动路径。

真正的 B：在 Qt **本来就准备显示**的一次帧提交过程中，取已经绘制的原生帧；不制造另一张离屏抓图；在后续 Core 采集请求中重用不可变帧，保留既有 RFB、Target、会话和编码流程。静态时屏幕不渲染，缓存不被更新；动画时真实渲染自动更新缓存。无有效原生帧时回退旧抓图，不得回传未经证明的黑屏/旧尺寸/错像素。

## 第一阶段为何只对 OpenGL opt-in

Qt6 Quick 使用 RHI，Windows 常用 D3D11，而 RK3588/EGLFS 通常是 OpenGL ES。Qt 6.8 公开 `QSGRendererInterface` 可辨别实际 graphics API；`QQuickWindow::afterRendering` 是渲染线程直接信号。它**不**保证所有 RHI 后端的 CPU 可读帧数据可用。Qt6 QRhi 的 `readBackTexture()` 需要可传输的源纹理或 `UsedAsTransferSource` 的 swapchain，而 Qt Quick 原生交换链的这一能力**不能在无验证的情况下假定存在**。直接调用 `QQuickWindow::grabWindow()` 也被官方标注有性能问题且只能从 GUI 线程调用，不适合作为本轮“避免反复离屏渲染”的替代证明。

这一最窄源码原型：
- 仅在显式 `HYREMOTE_EXPERIMENTAL_PASSIVE_QUICK_GL=ON` 且 **实际 Qt scenegraph 使用 OpenGL** 时接管；
- `QQuickWindow::afterRendering` 仅在渲染线程内观察**当前 native default framebuffer**，通过公开 `QOpenGLContext` / `QOpenGLFunctions::glReadPixels` 读出一次，校验 FBO、viewport、期望物理尺寸、GL 错误；
- 只有 Core 最近 350ms 有采集需求时才读回，避免断开 Viewer 后持续消耗 CPU/GPU；零 Viewer 的初始 bootstrap 最多触发短窗口；
- 用不可变 `CpuFrameStorage` 缓存整帧，后续 Core 请求复用相同 frame storage（不重新分配、渲染、复制），保留 per-request 元数据；
- 无 native 帧时**只触发一次** `QQuickWindow::update()` 帮助启动真实屏幕渲染，短暂等待后回退原来的 `QQuickItem::grabToImage()`。不让 afterRendering 再调用 grabToImage，防止通知-抓图反馈环；
- resize、DPR、scenegraph 失效、target 丢失或非 OpenGL 下缓存作废/旧路径回退；
- Qt 常规构建仍走原有逻辑，默认保持完整兼容与资源策略。

**必要警告：** Qt6 OpenGL RHI 在 `afterRendering` 时可能仅记录了渲染命令、尚未提交；当前 GL framebuffer 可能不代表最新完整可视像素。默认 FBO/viewport 匹配只是**必要、非充分**条件。若出现上一帧、黑帧、部分帧或不可读，判实验失败并禁用该路径，不要以继续加定时器/猜测 Qt 内部行为补丁掩盖。不能从本实验自动推导 Windows D3D11 或 Mali EGLFS 已支持。

## 性能与正确性验收（只做一个问题一个实验，不跑矩阵）

**阶段 P0：** 在实际 Qt6.8.3/MSVC Qt Kit 上，先只编译 `HYREMOTE_EXPERIMENTAL_PASSIVE_QUICK_GL=ON` 的 Runtime/Generic 受影响目标。Default OFF 的正常产品目标无需重新进行全量例程/测试。编译是否通过、Qt 实际图形后端、source SHA 必须真实记录；若默认 Windows D3D11，不允许悄悄当成 OpenGL PASS。需要按明确、仅实验的 Qt OpenGL backend 运行对照时，`QSG_RHI_BACKEND=opengl` 只用于实验，不能写入用户默认配置。

**阶段 P1：** *仅一条*真实 1080p Qt Quick 静态、1 Viewer：检查绘制结果是否为正确图像（而非老图像、黑帧、翻转/颜色错误）；检查 Host `quick_grab_submit` 不再 ~7Hz；CPU 是否从同硬件/同 graphics API 的 baseline 实质下降。仅用 PrintWindow/人的真实 Viewer 观察，不添加第二 Viewer。进程自己退出、每 5～10 秒报告实际 PID/进度，异常停止并保留结果。

**阶段 P2：** P1 确实正确且减负后，再做*一条*持续动画/局部变化（目标60Hz源动画）实测：Host CPU + 原生帧获取与实际 Viewer 可见变化、局部窗口变化、输入响应。不能只看 Host FPS；若 Viewer FPS/延迟无法可靠测量，标注 NOT_MEASURED、不得承诺无掉帧。

**阶段 P3：** 验证实际 EGLFS/OpenGL ES 和 Windows默认 D3D11 的能力差异，以及 popups/多窗口/水印/resize/回退前提。若原型只支持 EGLFS，不得将 Windows 默认 D3D11 声称已解决；如果默认 Windows 后端不能支持，另用 Qt6 对应原生帧来源（D3D11 resource/staging copy或经过证明的RHI能力），仍在同一 Shared Runtime 的适配层隔离。产品最终性能承诺以前后同机实测（包括原 RK3588 用户结果）为准。

**失败即退出路线的条件：** OpenGL context/FBO 非屏幕帧、Qt6 `afterRendering` 读到过期像素、重新产生 render loop、自身读回 CPU 很高、动画新鲜度恶化、扰乱本地渲染、Qt6 private/G7 边界无法保障。不得通过延长 100ms 探针或牺牲动画帧率伪装优化。

## 文件和控制面

- `src/runtime/src/quick/passive_gl_frame_sampler.{hpp,cpp}`：仅实验路径的原生 OpenGL 已绘帧读取，公开 Qt GL 接口。
- `src/runtime/src/quick/quick_target.cpp`：opt-in 挂钩 / immutable frame cache / 失败时旧采集路径。
- `src/runtime/CMakeLists.txt`：实验开关默认 OFF，运行时警告**不可作为发布构建**。
- Core/Shared Runtime 公共 API、RFB/输入协议、教学 examples、CI 工作流和冻结 `release/v0.3.5.0@b0680d5d` 不变。

Human 已批准 B 技术路线，**不需要再次批准第一阶段可回退实验**；如果实测证明多平台需求触及新的 Qt private ABI/正式 G7 越界，云端 Owner 应说明额外边界及收益后再提交变更。未测试/无目标设备时只能声称 `PROTOTYPE_NOT_QUALIFIED`。
