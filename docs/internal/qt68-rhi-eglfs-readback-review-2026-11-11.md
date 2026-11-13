# Qt 6.8 RHI/EGLFS 既有渲染资源异步读回：兼容性与维护风险评审

日期：2026-11-11。性质：重大技术决策前的独立研究，不是正式产品开发或验收。
固定研究基线：develop@add37e3dffd186567f73c3978f2cb3a5662f8288。
研究代码：research/qt68-rhi-readback/（独立可执行程序，不接入 Runtime、Generic、QPA 或用户 HMI）。
**当前裁决：API 与源码审查完成；真实目标设备资源资格、无阻塞和性能尚未验证。禁止将私有 QRhi 接入正式产品。**

## 1. 源码与现场事实

- #789 的旧候选 6a4b7b37 在 RK3588 / Qt 6.8.3 / EGLFS / 1920×1080 的静态 1 Viewer 场景：Host CPU 由 15.6% 升至 117.2%（单核等效），同时出现画面逐块刷入。该现场记录不能外推到当前 develop。
- 当前 src/runtime/src/quick/quick_target.cpp 调用 QQuickWindow::contentItem()->grabToImage(logicalSize)。Qt 将内容额外渲染到离屏目标并复制到 CPU；现有 QuickCaptureSource 已有单飞、请求身份管理、代际终止、FrameStorage 对 QImage 的直接持有优化。无需重复开发已存在的调度器。
- 低 GPU load 不足以证明 CPU 主要耗于 GPU readback；仍需拆分 Qt 离屏重新渲染、图像转换、CPU 逐块比对、编码、Viewer/网络与实际本地 UI 影响。

## 2. Qt 6.8 官方合约核对

| 命题 | 已证实的接口事实 | 实施约束 |
| --- | --- | --- |
| 现有 Scene Graph 资源 | QSGRendererInterface::RhiResource 和 RhiSwapchainResource 可查询，后者在 QQuickRenderControl 可能为空 | 可查询不等于可以修改 Qt 所有的资源 |
| 现有显示 backbuffer 读回 | QRhiResourceUpdateBatch::readBackTexture 接受空 texture 来源，但对应交换链必须已经使用 QRhiSwapChain::UsedAsTransferSource 创建 | 必须检查 swapchain->flags()；不能擅自调用 setFlags/createOrResize 改 Qt 拥有的交换链 |
| 单独 QRhiTexture 读回 | 纹理需要 QRhiTexture::UsedAsTransferSource，且多采样纹理不支持 | Qt 没有保证暴露最终全画面的合格 QRhiTexture |
| 多采样交换链 | QRhiReadbackDescription 明确支持 multisample swapchain buffer readback | 不得把 sampleCount 大于 1 当成拒绝依据 |
| 帧阶段 | QQuickWindow::afterRendering 在场景图 render pass 已记录、队列尚未提交时触发；QRhiCommandBuffer::resourceUpdate 不能在 pass 内调用 | 只在渲染线程、当前合法帧中短暂使用 command buffer 和 swapchain |
| 完成通知 | QRhiReadbackResult::completed 在 QRhi 操作所在的线程调用，QByteArray 给出 CPU 数据 | 回调不得同步执行编码、网络、GUI 等待或无界锁持有 |
| 完成上界 | Qt 的保证依赖 finish() 或后续至少 N 个提交帧且开始下一帧，其中 N=MaxAsyncReadbackFrames | 异步 API 不保证静态场景永远及时完成，绝不能偷偷使用 finish() |
| 重定向离屏渲染 | QQuickRenderControl 的 beginFrame/endFrame 使用 QRhi beginOffscreenFrame/endOffscreenFrame | endOffscreenFrame 会等待 GPU 工作，不能冒充无阻塞 |
| EGLFS | Qt 6.8 通过 EGL/OpenGL ES 管理窗口，通常每屏一个全屏原生 GL 窗口 | 不能凭 EGLFS 名字推断零拷贝、转移标志或 Mali 特定驱动行为 |

上述 QRhi 类型需要 Qt6::GuiPrivate；Qt 文档明确其源码和二进制兼容保证有限。虽然 RHI 抽象跨图形 API，私有接口并非跨 Qt 版本稳定承诺。Qt5.15 与 Qt6.8 必须分别处理；不能将 Qt6 私有实现偷渡为通用后端。

## 3. 三条候选技术路径裁决

| 路线 | 结论 |
| --- | --- |
| A：现有公共 grabToImage + 变化来源/差分/背压优化 | 保持正式产品基线；已有 #922/#609/#567 研发路线继续，不能宣布 #789 已解决 |
| B：在 Qt 现有 swapchain 合法帧上提交一次 readback | 有条件研究；仅当 UsedAsTransferSource 已存在、资源真实有效且回调正确才能进入性能证明；默认 inspection-only |
| C：强制改 Qt swapchain 或接管 QQuickWindow 的渲染、另开 FBO/PBO/GBM/RGA 路径 | 本阶段 NO-GO；侵入 Qt 资源所有权且驱动和多端维护成本高，需要定量优势与 Human 重大架构审批 |

## 4. 已完成的独立探针设计

研究目录是独立 CMake 项目，绑定 Qt6::GuiPrivate，不进入 HyRemote 顶层编译：
1. 默认只观察真实渲染线程、RHI、Qt 创建的 swapchain flags、sampleCount、最大异步帧数。
2. 无 RHI、无 swapchain、缺 UsedAsTransferSource 时 fail-closed，明确输出原因；不会猜测、不会直接 glReadPixels、不会修改 Qt flags。
3. 只有显式 --submit-readback 才在 afterRendering 的 command buffer 排队一次 QRhi readback；不调用 finish、不创建额外 offscreen frame。
4. 输出 JSON：graphics_api、rhi_backend、qpa、swapchain_flags、submissions、completions、data_bytes、submit_wall_ms、callback_after_submit_return 等。
5. 获得有效字节只证明某个独立测试窗口的读回回调出现，**绝非**完整像素、正式 HMI 兼容、无阻塞或产品性能 PASS。

## 5. 五道后续验证门

- R0 资格：固定源码与 Kit，目标运行确认为 Qt 6.8.3 / eglfs / OpenGL ES，Qt-owned swapchain 已有 UsedAsTransferSource。缺标志直接关闭该直接读回路线，不强改内部 swapchain。
- R1 正确性：重复回调、有效像素大小与格式、真实 Overlay/Quick3D、alpha、DPR、resize、窗口销毁/重建，以及逐像素完整场景对照。只有 data_bytes 大于零不得 PASS。
- R2 并发：渲染线程轻量提交；回调在 RHI 线程；无等待 GPU 完成、跨帧持有 Qt 内部对象、死锁、悬挂 callback；在停止、重建与资源失效时能 fail-closed。
- R3 非阻塞：在真实 EGLFS，A1(无读回)→B(读回)→A2(无读回) 比较本地渲染帧间隔 P50/P95/P99、render thread 回调耗时、GPU 同步等待、GUI 输入响应和画面完整性。回调是在提交后执行，**并不能独立证明**渲染线程没有隐式同步和卡顿。
- R4 产品：同源 1080p 真实 Viewer、CPU/GPU/内存/功耗/画面、0/1/2 Viewer/断连，验证公共回退、四种接入与 Qt 5.15 边界。Windows/Mesa 成功不代表 RK3588 实测成功。
- R5 治理：独立风险、定量收益、Qt Kit/版本矩阵、回滚、许可和长期成本完整；涉及私有产品路径时提交 Human 决策，未经同意不修改正式 Runtime。

## 6. 维护风险评审

| 风险 | 级别 | 控制 |
| --- | --- | --- |
| Qt6 私有 RHI API/ABI 变动 | 高 | 仅研究构建，锁定编译/运行版本，Qt5 保留公共回退，逐版本验收 |
| Qt-owned swapchain 缺转移标志 | 阻断 | 不 setFlags、不 createOrResize，直接输出 NOT_ELIGIBLE |
| GPU/driver 隐式 fence 等待、静态帧无回调 | 高 | R3 真实线程性能验证，不能用异步函数签名背书 |
| Qt Scene Graph 重建、隐藏、resize、资源回收 | 高 | 不跨帧缓存 Qt-owned 指针；代际/销毁安全另测 |
| 颜色、alpha、overlay、完整画面及 DPR | 高 | 逐像素对照，默认公共回退 |
| EGLFS 特性、Mali/GBM/跨设备差异 | 高 | 实际 backend + driver 能力判定，不以 SoC 白名单推断 |
| 四前端/Qt5/Qt6 多路径长期维护 | 高 | 统一 Shared Runtime 抽象，独立审批之前不增加第二个产品人格 |

**独立自审：** 研究已证明 Qt 6.8 存在有条件的 readback 合约；尚不能证明 HyRemote 当前真实 Qt-owned 最终画面资源满足这些条件。缺现场资格/线程数据时，不能声称完成性能修复或通过不阻塞验证。

## 7. 状态与产品决定

- QT68_API_REVIEW=PASS_DOCUMENTED
- HYREMOTE_SOURCE_AUDIT=PASS
- RESEARCH_PROBE_SOURCE=COMMITTED
- PRIVATE_INTERFACE_RISK_REVIEW=COMPLETE_HIGH_RISK
- PROBE_BUILD_AND_RUNTIME=NOT_YET_VERIFIED
- EGLFS_TRANSFER_FLAG=UNKNOWN
- EGLFS_READBACK_CALLBACK=NOT_RUN
- RENDER_THREAD_NONBLOCKING=NOT_PROVED
- PRODUCT_PRIVATE_RHI_GO=NO
- PRODUCT_DEFAULT_CHANGE=NONE
- RK3588_CUSTOMER_FIX=NOT_CLAIMED

## 8. 来源

仓库：src/runtime/src/quick/quick_target.cpp、quick_capture_backend.hpp、docs/internal/async-capture-spike.md，以及 issues #789、#609、#922、#123。

Qt 官方文档：
- https://doc.qt.io/qt-6.8/qsgrendererinterface.html
- https://doc.qt.io/qt-6.8/qquickwindow.html
- https://doc.qt.io/qt-6.8/qrhiswapchain.html
- https://doc.qt.io/qt-6.8/qrhireadbackdescription.html
- https://doc.qt.io/qt-6.8/qrhiresourceupdatebatch.html
- https://doc.qt.io/qt-6.8/qrhireadbackresult.html
- https://doc.qt.io/qt-6.8/qrhicommandbuffer.html
- https://doc.qt.io/qt-6.8/qrhi.html
- https://doc.qt.io/qt-6.8/qquickrendercontrol.html
- https://doc.qt.io/qt-6.8/embedded-linux.html
