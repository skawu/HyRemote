# #609 Qt LTS 底层合成与 HyRemote 最小性能消耗架构研究（2026-11-04）

**文档性质：方案论证 / 架构设计候选；不授权源码实施或发布。**  
**主产品版本范围：Qt 5.15 LTS、Qt 6.8 LTS（现有第一轮 GA 计划）；Qt 6.5 / 6.12 仍为 GA 后独立准入。**  
**管理原则：按 Qt 可移植的公开契约设计，按 Qt LTS 差异适配，不以 Windows/D3D11/GL/EGLFS/GPU 厂商作为产品架构路线。**  
**冻结约束：Stage 02 G7、一个 Qt-free Core + 一个共享 Qt Runtime、四接入路线、Widgets / Quick 目标适配器、RFB + Native 两协议，不修改任何一项。**

## 1. 产品目标、实际代价和反例

目标不是让 CPU 占用某个孤立基准值最低，而是 **在保持 Qt 本机显示/交互、完整可见目标、远端新鲜度及资源有界的前提下，让 HyRemote 的*附加*工作尽可能接近零**。同一个 Viewer 请求不应引起第二次无必要的 Qt 场景合成；同一帧数据不应在 Qt→Runtime→Core→transport 中被重复深拷贝；无像素变化时不应重复编码/网络发送；四接入路线不拥有四套实现。

- [#609 Q0/Q1](https://github.com/skawu/HyRemote/issues/609#issuecomment-6628044624)：Qt6.8.3 / Quick 1080p 静止无 Viewer 时约 0.14% 单核、0 grab、约 1 次 Qt render；有 1 Viewer 时在 33.3s 做了 247 次 `grabToImage`（约 7.42/s）、269 次 render signal、Host 约 10.99% 单核。**这是固定内容被捕获重复渲染的强证据。**
- [#829 隔离 OpenGL ON/OFF](https://github.com/skawu/HyRemote/issues/609#issuecomment-6631533345)：同一 Windows OpenGL Kit，一次同源 1080p 动画对照 Host 稳态 CPU 37.5%→27.8%（约 −25.9% 相对），Qt render 24.0/s→25.0/s；证明减少离屏 grab 的价值，**不能**当作跨 LTS、跨 Qt 后端和实际 Viewer FPS 的性能承诺。该专项原型已关闭、未合并。
- [#789](https://github.com/skawu/HyRemote/issues/789) 的 RK3588/EGLFS 静态 1 Viewer +101.6 单核百分点及 [#567](https://github.com/skawu/HyRemote/issues/567) 大幅刷新条带是**用户现场真实待解决产品问题**；开发环境无真机，验证由用户负责，不能用桌面数字替代。
- [#609 Human 纠偏](https://github.com/skawu/HyRemote/issues/609#issuecomment-6632387364)：Windows WGC/D3D11 逐后端路线已撤销；早期 GitHub #829/#832 作为实验与历史证据归档。

## 2. 渲染/合成架构差异：Qt5.15 vs Qt6.8

| 维度 | Qt 5.15 LTS | Qt 6.8 LTS | 统一产品设计结论 |
| --- | --- | --- | --- |
| Qt Quick 视觉树 | QML/QQuickItem→QSGNode，场景图复用图形节点和批次 | 同样的 QSG 场景图 | 不需要 HyRemote 再拥有/重建一个 QML 场景图 |
| 默认绘制/合成 | 标准适配通常**直接 OpenGL / GLES**；Qt5.14 起有**可选** RHI 实验路径，另有 software 适配 | 默认 **QRhi** 记录并提交图形命令，实际 API 可为 D3D/Metal/GL/Vulkan；也可 software | **关键 LTS 实现差异**。不能假定 5.15 必为 GL、6.8 必有可读/可转移 texture |
| basic / threaded | Qt 选择渲染循环，可能不同线程；`afterRendering` 在标准 GL 场景完成绘制之后，Qt5 文档允许 GL screen scraping | Qt 选择渲染循环；`afterRendering` 在 RHI 命令录制之后、图形队列提交**之前** | 即使两版信号名字相同，**时序与可读像素语义不同** |
| `frameSwapped` | 代表帧已排队呈现（非内容脏标志） | 同为渲染/呈现通知，不能从此假定 CPU 像素可读 | 可只读记录 native render cadence，不可据此触发重复 grab 成环 |
| `afterFrameEnd` | 无 Qt6 的此信号 | Qt6 提供更晚帧生命周期通知 | Qt6-only，不是 Qt5/Qt6 统一的 CPU readback API |
| `QQuickItem::grabToImage` | 异步返回 QImage 的公共正确性基线，但单次调用可额外 offscreen render/readback | 同样明确会 offscreen render + GPU→CPU copy，昂贵 | **PUBLIC_LTS_SHARED，但不能称为被动复用屏幕帧** |
| `QQuickWindow::grabWindow` | 公共 GUI-thread API；Qt 官方警告性能代价 | 公共 GUI-thread API；同类性能警告 | 不能把同步 grab 替换异步 grab 冒充优化 |
| `QQuickRenderControl` | 允许**应用拥有**离屏 scenegraph / render target | 同样允许，Qt6 有 begin/endFrame、QQuickRenderTarget、RHI 适配差异 | **仅对应用主动拥有 renderer 的场景适用**，不能对既存可见窗口无痛接管，不能作为 Generic/QPA 通用零代码方案 |
| 普通 Widgets | QWidget / QPainter / backing store 路径 | QWidget / QPainter / backing store 路径 | GUI 采集的主要负担是重复 `QWidget::render`；不能用 Quick 的 GPU 方案代替 |
| Qt Quick 与 Widgets 混用 | QQuickWidget / QOpenGLWidget 有额外 offscreen/layer 结构 | Qt6 同类结构随 RHI 改变 | 复杂窗体必须保留实际可见内容和 alpha/DPR/父子 surface 的正确性 |

**无法省去的物理成本：** 标准 RFB/Native 的当前 CPU-readable frame 消费需要 GPU→CPU 的某种 readback 或 CPU 已拥有的像素。即使把它“放到同一次本机 render”，GPU readback/同步/颜色格式化仍不是免费的；应用持续动画时无法保证 CPU 额外开销为零。所谓最大程度降低资源消耗，是**消除重复合成和重复深拷贝**，再明确必须发生的采样成本。

**关键 NO-GO：** 在已审核的 Qt5.15/Qt6.8 公开 API 合同中，尚未证明存在一个可用于**任意已经在屏幕上显示的 Qt Quick 窗口**、可在两代 LTS 下稳定返回“最新完整已合成 CPU 像素”的非阻塞公共 API。不能用 Qt5 GL `afterRendering` 的文档行为套到 Qt6 RHI，也不能从 `QSGRendererInterface` 得到 device 就当已取得可读 render target。用户批准的“复用已完成帧”是产品优化目标，**不是**这些不成立的 API 前提。

## 3. 现有 HyRemote 链路的确切消耗位置（develop 源码审查）

```text
Qt own render/composition                  [Qt app, baseline; MUST NOT interfere]
         │
         ├─ Quick Runtime : contentItem()->grabToImage(logicalSize)
         │                  → asynchronous ready QImage
         │                  → convertToFormat(RGBA8888_Premultiplied)
         │                  → allocate owned CpuFrameStorage
         │                  → memcpy every row / Damage::fullFrame()
         │
         ├─ Widgets Runtime: allocate CpuFrameStorage → QImage view
         │                  → QWidget::render(&image) / Damage::fullFrame()
         │
         └─ Automatic multi-surface: CompositeCaptureSource
                            ├─ exact single surface fast path: reuse immutable pixels (ALREADY IMPLEMENTED)
                            └─ multi-surface QPainter full-canvas composition
                  → Core bounded latest-frame semantics
                  → RFB changedRfbTiles / Raw/TRLE / per-viewer delivery
```

代码验证：`src/runtime/src/quick/quick_target.cpp` 的 `requestFrame()` 直接排队 `attemptGrabOnGuiThread()`，每次 grab `ready` 后执行格式变换、另分配 `CpuFrameStorage` 并逐行复制；`src/runtime/src/widgets/widget_target.cpp` 每次重执行 `target->render(&image)`；`src/runtime/src/automatic/composite_target.cpp` **已有**符合严格前提的单 surface 零额外合成快路径，不能重复算成新待开发优化；RFB worker 里**已有** `changedRfbTiles` 变化检查，不能再引入一份完整像素比对虚报收益。

`src/runtime/src/detail/viewer_capture_demand.hpp` **已存在**统一 Viewer demand gate：0 Viewer 完成 ServerInit bootstrap 后休眠；连续 3 次未变则静态探针间隔 **100ms**，远端输入后重置。静态仍约每秒最多触发 10 次昂贵 grab，这解释了剩余 CPU 成本。调整探针间隔会改变本地独立变化的发现延迟，属于新鲜度取舍，**不能仅凭 CPU 变低就将 100ms 改成 500ms/1s**。`frameSwapped/afterRendering` 曾被 grab 激发，单靠它唤醒会形成新的反馈环，已在 #609 实测否定。

还有一个时间语义风险：`composite_target.cpp` 在返回 `lastFrame` 时重写 `completionTime`；Core `normalizeFrameTiming` 对 `PtsSource::Completion` 会用它填充内容 PTS。复用未更新像素时如果覆盖 completion/PTS，就会**把陈旧内容伪装为新鲜**。任何缓存优化必须先澄清原始内容 PTS、响应请求时间、提交/交付时间三种语义，按 ADR-0002 的数据合同保留来源。

## 4. 优化候选：从确定可移植到技术未证实

| 顺序 | 机制 | 理论可消除 | Qt5/Qt6 适用 | 决策/门槛 |
| --- | --- | --- | --- | --- |
| **A0** | 保留已经实现的 Viewer 需求门控、Core newest-frame、RFB changedRfbTiles、单 surface composite fast path；不要再复制一套 | 已实现的重复请求/编码/合成 | 两代可用 | **BASELINE，禁止重复造轮子** |
| **A1 优先** | **QImage-backed immutable FrameStorage**：Qt Quick 的 `ready()->image()` 经必要颜色转换后，由 Runtime 私有只读 `FrameStorage` 持有其 `QImage`；`mapRead` 使用 `constBits/bytesPerLine/sizeInBytes`，Core 无 Qt 类型；同一像素块在 Runtime/transport 共享生命周期 | 第二个全帧 CPU 分配 + 逐行 memcpy；如果颜色格式一致则可能还可避免深转换 | Qt5/Qt6 均可设计；仅 Runtime 私有 | **最高确定性可验证候选**。先证明 QImage const 只读映射/stride/padding/alpha/跨线程销毁/多 Viewer 正确，再做小切片；未测 CPU 收益不得预报 |
| **A2** | 在现有 `ViewerCaptureDemand` 上引入可信的 Qt 变化通知作为**提前唤醒**、不改 100ms 正确性探针；记录 captured-induced vs native render 的独立性 | 交互时无谓等待，可能改善新鲜度 | Qt5/Qt6 事件语义需分别确认 | **候选**。只允许被证明可靠的 hint；不能由 hint=无推断页面绝对不变 |
| **A3** | 目标和 composite 的**版本化结果缓存**：只有可独立证明所有 child 像素版本、大小、DPR、排序/遮挡、mask/watermark/privacy 均不变时才复用 CPU compose 结果；单 surface 已优化 | 多窗口 CPU 全画布再合成 | 两代相同产品语义 | **条件方案**；不能以 `frameSwapped` 为 version；隐私/水印变更强制失效 |
| **B 研究** | 真正复用 **Qt 自己已合成的** native frame，但由 Qt LTS 可验证的受控能力提供（而不是 OS/GPU 名称分支） | 第二次 Qt scenegraph render，潜在最大收益 | Qt5.15 标准 GL 后渲染屏幕抓取能力 ≠ Qt6 RHI 安全 readback；两代都有非默认 render backend 例外 | **长期研究 GO/NO-GO**：需要证明已完整提交/读回不阻塞/像素新鲜/内容含 popup / 资源生命周期，先排除 private/默认后端不可用；不能默默迫使应用切图形 API |
| **C 禁止** | Windows WGC、D3D11 sampler、硬编码 GL/PBO、强制 offscreen/QPA 私有帧或不受控 QRhi SDK 作为普适产品架构 | 局部场景可能很快 | 不满足 Qt LTS-first | **REJECTED_AS_DEFAULT_PRODUCT_DIRECTION**；历史实验可保留，不能自动转正 |

### A1 的实现契约（供后续独立代码 PR 使用；本文件不实施）

1. `QImage` 引用由 Qt-aware Runtime 私有只读 `FrameStorage` 子类独占生命周期；发布后不使用任何可能 detach 的 `bits()` / 非 const 操作。允许 `constBits` 提供只读字节，缓存原始 `bytesPerLine` 并验证 `bytesPerLine >= width*4`、plane bytes、溢出和行填充；非 `RGBA8888_Premultiplied` 的颜色语义需在 GUI 线程先做必要转换；决不把错误 alpha 直接按 opaque RFB 处理。
2. `FrameStorage` 的 `mapRead` 只能通过 `shared_ptr` 生命周期访问；跨 Qt 线程**不同实例/共享数据只读**，不得同时修改同一个 `QImage` 实例；销毁/释放回调遵守现有 Storage/Qt 线程规则。窗口销毁与 stop 不得使已发布 CPU 像素悬空；不允许 `QPixmap` 跨线程消费。
3. 如果数据不可安全共享、转换增加拷贝、Quick 5.15 不兼容或缓冲滞留造成 RSS 超预算，**自动回退现有已拥有 `CpuFrameStorage` 的复制路径**；默认产品行为不变。不得改 Core API / Runtime 公开 SDK / 4 ingress / 双协议。
4. 独立 unit fixture 验证 stride/padding、多 viewer lifetime、复制后源 QImage 释放、切换 DPR、颜色含 alpha、窗口关闭、stop/restart 与恶意 metadata；实际 Kit 下比较 `QuickConvert/QuickBuffer/QuickCopy/QuickGrabReadyWall/Host CPU/RSS/Viewer pixels`，不把节省一次 copy 推断成减少 grab/rerender。

### 为什么 A1 不是最终目标

A1 只能除掉 GPU readback **之后**的重复分配/复制。#609 的静态最大成本可能是 `grabToImage()` 自己导致的 offscreen scene render/readback。故 A1 是低风险跨 LTS 的明确第一切片，B 仍然保留为产品研究方向；**只在 B 的 Qt 公共/可接受依赖合同证明成立时批准 B 实现**。不能以 A1 冒充 RK3588 +1 核和条带刷新已经解决。

## 5. 最小验证：先因果归因，再代码

**M0（云端本轮完成）**：Qt5.15 vs Qt6.8 场景图/合成/信号/readback 的文档矩阵；`develop` 的 Qt Quick、Widgets、composite、Viewer gate、Core timing 全链审查；A0 已实现、A1 优先、B 缺口、冻结边界结论。无源码改动。

**M1（唯一下一项本地 Owner，可转发）**：在**已经存在的** Qt5.15.2 与 Qt6.8.3 本地 Kit 中，做一个**独立且只读的 API/开销诊断**，不是新 Runtime 实现。用同一个极小 1080p 静态 Qt Quick scene 分别采样：`QQuickItem::grabToImage` 之前/之后 Qt `afterRendering/frameSwapped` 计数（证明 offscreen 自激是否在 Qt5/6 各自发生）、固定少量 grab 的 `ready` 耗时、`QImage::format/bytesPerLine/sizeInBytes/constBits` 读访问是否发生 detach（仅检查共享/地址，不写入）、`QQuickRenderControl` 是否仅针对自己拥有的 offscreen Window。只观察 Qt，不连接 Viewer、不修改 HyRemote 产品源码、不改变 Qt backend、不做性能大矩阵。Qt5 API 或现有 Kit 缺失则记录 `UNAVAILABLE/UNKNOWN`，当轮完成即停。**无需把已存在的 Qt6 用户 Viewer CPU 对照再跑一次。**

**M2（云端在 M1 后）**：校验 A1 是否满足跨 LTS 的 Qt/FrameStorage/PTS 语义和负例，先确定完整代码方案、回退与验收，才允许 Local Owner 做一个独立的 Runtime 私有实现 PR；若 M1 证据不足则保持设计而非盲码。

**M3（A1 单点验证后）**：进行小型正确性/性能对照，仍需 Viewer 真正画面、原 Qt render 次数、Qt5/Qt6 适用分工与 LTS 差异；不以静态 CPU 单指标换取新鲜度或页面完整性。更广义 B 如需 QtPrivate 或明显触及 G7 已冻结架构，依法提交 Human 决策卡，不能暗中进行 OS adapter 研发。

## 6. 验收合同与止损

- **强制正确性**：内容像素、alpha、DPR、旋转、Popup/多 top-level、遮挡/水印/privacy、尺寸/重连/关闭，当前 Core 有界 Backpressure，与本机 UI 并行无回归；frame 源时间不得由缓存命中更新。
- **性能必须单独量化**：0/1 Viewer 静态、局部交互与动态（必要时 W1–W4）；`Qt native render/s`、额外 offscreen grabs/s、`Qt grab ready P50/P95`、`QuickConvert/Buffer/Copy`、`GenericCompose`、`RfbDiff/Encode`、Host CPU 单核百分比、RSS、高水位/提交计数，Viewer 实际 FPS/帧龄只有在真实可测才填数。统计用同 SHA/KIT、控制变量、相邻 CPU time 与 wall time 区间。
- **拒绝伪收益**：让 QML 16ms timer 变成 ~24fps 仍不能当“60fps 达标”；删掉抓图却漏 popup、缓慢静态探针换低 CPU、Qt 抓帧通知反馈环、重复创建 full-canvas、改 OS backend 后比较、GPU 同步阻塞 GUI、单机 Intel→RK3588 外推，都不准过门槛。
- **产品支持状态**：首轮 GA Qt5.15/Qt6.8 是计划，不表示 Qt5 Quick 现已 Supported；当前 Qt5 Quick 仍 experimental、Qt5 QML/Generic/QPA 未按该家族授权，真实支持声明须独立验证。Qt6.5/6.12 不提前推进。
- **G7**：本文件只澄清内部优化候选，不变更统一 Core/Shared Runtime、四入口、RFB+Native、无额外 Agent 部署要求；如需改变上述合同，先 Human 正式变更审批。

## 7. 参考证据

- [Qt5.15 QQuickWindow：afterRendering/GL 语义与可 screen scrape 条件](https://doc.qt.io/archives/qt-5.15/qquickwindow.html)
- [Qt6.8 QQuickWindow：afterRendering 在 GPU 提交之前](https://doc.qt.io/qt-6.8/qquickwindow.html)
- [Qt6.8 默认 RHI renderer 与合成批处理](https://doc.qt.io/qt-6.8/qtquick-visualcanvas-scenegraph-renderer.html)
- [Qt6.8 scenegraph adaptations（Qt5 直接 GL / Qt6 RHI）](https://doc.qt.io/qt-6.8/qtquick-visualcanvas-adaptations.html)
- [Qt6.8 QQuickItem::grabToImage 离屏绘制/GPU→CPU 成本](https://doc.qt.io/qt-6.8/qquickitem.html)
- [Qt6.8 QQuickRenderControl：只有由应用控制的 offscreen scene](https://doc.qt.io/qt-6.8/qquickrendercontrol.html)
- [Qt6.8 implicit sharing（copy on write）](https://doc.qt.io/qt-6.8/implicit-sharing.html) 和 [QImage constBits](https://doc.qt.io/qt-6.8/qimage.html)
- [ADR-0002 RemoteFrame 源时间与存储生命周期](../adr/0002-remoteframe-lifetime-timestamps.md)
- [HyRemote 产品架构](../architecture.md)、[兼容性 LTS 计划](../compatibility.md)、[Stage02 G7](architecture-stage02-g7-freeze-record-2026-11-04.md)

```text
RESEARCH_STAGE=QT5_15_QT6_8_COMPOSITION_REVIEW_COMPLETE
DESIGN_STATUS=CANDIDATE_NOT_IMPLEMENTATION
PROVEN_PORTABLE_COPY_SAVINGS=A1_CANDIDATE_NEEDS_M1
PASSIVE_ONSCREEN_CPU_READBACK_CROSS_LTS=NOT_PROVEN
NEXT_LOCAL_TASK=M1_READ_ONLY_QT_API_COST_PROBE
WINDOWS_WGC_D3D11_PRODUCT_BRANCH=REJECTED
QT5_QUICK=EXPERIMENTAL_NOT_SUPPORTED
G7_ARCHITECTURE=UNCHANGED
```
