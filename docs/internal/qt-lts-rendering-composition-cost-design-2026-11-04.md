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


## 8. M1 本地回报复核与方法纠正（2026-11-04）

证据：[本地 Owner M1，#609 comment 6633575189](https://github.com/skawu/HyRemote/issues/609#issuecomment-6633575189)。**仅验收已执行的测试，不补造原计划未跑的结果。**

### 与原 M1 验证合同的偏差

- 原批准的是 **`QQuickItem::grabToImage` 异步 `ready()` 路径**，与 `develop/src/runtime/src/quick/quick_target.cpp` 一致；本地实际使用了 **`QQuickWindow::grabWindow()` 同步路径**。它在 Qt5.15.2 和 Qt6.8.3 上各 5 次调用分别带来了 `afterRendering +5` / `frameSwapped +0`；该观察**仅对同步 grabWindow 成立**，**不得**归因到 `grabToImage` 的实测 CPU/延时/信号因果。
- 本次 Qt5 `grabWindow` 计时约 240.7ms 首次、随后 7–9ms；Qt6 D3D11 前两次约 8ms、后三次约 1011–1023ms。后者是**单次测试中的异常**，既没有重复、也未在 HyRemote 的异步 `grabToImage` 路径复现。不得把 `~1s` 当成标准 1080p GPU readback 代价、Qt6 家族回归或平台路线依据；目前 `CAUSE=UNKNOWN`。
- 图像实际客户区是 `1920x1061`，而非精确 1920x1080；这不会取消格式观察，但不可当作严谨 1080p 性能基准。

### QImage 结果：成功证明隐式共享，但格式枚举文字需更正

| Kit | 实际枚举值（来自 grabWindow） | **官方 Qt 格式名称** | 字节/Alpha 约束 |
| --- | --- | --- | --- |
| Qt5.15.2 | `4` | `QImage::Format_RGB32` | 值为 `0xffRRGGBB`；在已观察的小端 Windows 内存是 BGRA，**不能**伪装 `RGBA8888` |
| Qt6.8.3 | `18` | **`QImage::Format_RGBA8888_Premultiplied`** | 不是报告所写的非预乘 `Format_RGBA8888`；后者枚举值 **17** |
| Both | `bytesPerLine=7680`, copy 后 `constBits` 相同；可变 `bits()` 导致 detach | 符合 `QImage` 隐式共享/const 读取合同 | 这次数据支持**持有本轮同步 grabWindow QImage 而不额外复制**，不等于已证明 HyRemote 实际异步结果相同 |

官方值与语义：[QImage Qt 官方格式枚举](https://doc.qt.io/qt-6.8/qimage.html)、[Qt 6.8 constBits](https://doc.qt.io/qt-6.8/qimage.html)。具体零拷贝候选 A1 仅对已满足 **`RGBA8888_Premultiplied` + Core/RFB 只读寿命合同** 的帧执行，不得用 endian-dependent RGB32 或直通未预乘 alpha 冒充；格式不匹配时维持原有 `convertToFormat` 并验证必要的转换，哪怕目标只是取消后续多余 `memcpy`。

### 验收状态与唯一纠正任务

```text
M1_QIMAGE_IMPLICIT_SHARING=PASS_FOR_GRABWINDOW_IMAGES
M1_QT5_15_VS_QT6_8_GRABWINDOW_EXTRA_RENDER=OBSERVED
M1_QT5_15_VS_QT6_8_GRABTOIMAGE=NOT_RUN
M1_QT6_SYNCHRONOUS_GRABWINDOW_1S_ANOMALY=OBSERVED_UNATTRIBUTED
M1_QIMAGE_FORMAT18=RGBA8888_PREMULTIPLIED_CORRECTED
A1_RUNTIME_FRAMESTORAGE_IMPLEMENTATION=NOT_AUTHORIZED
```

**M1b 限定补证**：仍用现有两套 Qt Kit、自建 Qt-only `QQuickView` 场景、无 HyRemote Viewer 或产品代码变化，**只**更换为实际 `window->contentItem()->grabToImage(QSize)`；预先建立 `QQuickItemGrabResult::ready` callback，**异步顺序**发 5 次（前次完成才发后次，不允许阻塞事件循环），分别记录 `submit->ready` wall、`afterRendering` / `frameSwapped` 增量、从 `result->image()` 得到的真实 `QImage::format()` 的**数值和正确枚举名称**、`bytesPerLine`、`constBits` 共享，及至少一张具有 RGB 非对称色块和透明/预乘差异的测试图的像素语义。如果格式不满足现有 Core 的 RGBA8888 premultiplied，记录**必须转换**而非强求零拷贝。若 Qt5 Kit 该路径未构建/程序错误，记录第一条错误、停止，不扩大范围。有限时长、自行退出、不重新跑 `grabWindow`、不跑性能矩阵。向 #609 报告完成即停。

M1b 通过前不批准 A1 产品实现；M1b 也不改变 Stage02 G7 冻结、Qt LTS 范围和 #789 用户现场资格状态。


## 9. M1b 已实测：Qt5/Qt6 真正异步抓图与 A1 内部设计裁定

来源：[本地 Owner #609 comment 6633738535](https://github.com/skawu/HyRemote/issues/609#issuecomment-6633738535)。实际 API 是 `QQuickView::contentItem()->grabToImage(QSize)`，每次异步 `QQuickItemGrabResult::ready()` 后才发下一次，每 Qt Kit 五次；无 Viewer、无产品源码修改、无 forced RHI。客户区实际是 **1920×1061**，不是完整 1920×1080。

| 事实 | Qt5.15.2 默认 GL | Qt6.8.3 默认 D3D11 | 判定 |
| --- | --- | --- | --- |
| 5 次异步 grab 完成 | 290.2/49.9/43.7/58.5/38.3 ms | 23.1/18.4/20.2/22.5/17.2 ms | **两代 API 真实可用**；单次、不同默认图形栈，数值不可推为 Qt 家族普适倍数 |
| `afterRendering` 增量 | +5 | +5 | 结果符合 grab 引起额外 render work；不能用此信号独立判断内容变脏 |
| `frameSwapped` 增量 | +4 | +5 | 记录为渲染/交换关联通知；单次计数**不能证明每个 grab 必然完成一次实际扫描呈现** |
| `QImage::format()` | `6=Format_ARGB32_Premultiplied`，本机小端 BGRA | `18=Format_RGBA8888_Premultiplied`，RGBA | 必须按实际格式变换。预乘语义已有，不代表字节顺序相同 |
| stride / read-only | 7680，`constBits` copy 后同指针，可变 `bits()` detach | 同左 | 支持 Qt image-backed readonly buffer，不要从本次紧排推所有图像紧排 |
| 旧 QImage 在 resize / target destroy 后存活 | NOT_RUN | NOT_RUN | A1 代码必须依照 QImage ownership/implicit sharing 契约，后续加单元与集成负例 |
| Viewer FPS/latency | NOT_RUN | NOT_RUN | 不得宣称产品性能改善 |

### A1 设计裁定（不改变 G7 架构）

**已确定的私有改动范围**：`src/runtime/src/quick/` 中新增仅 Runtime 可见的 `QImage` 包裹 `FrameStorage`（仍报告 `StorageKind::Cpu`、一个 plane、只读 `mapRead()`、以自身 `QImage` 的隐式共享引用维持底层数据寿命）；`publishImage()` 保留 `FrameGeometry=RGBA8888/Premultiplied` 与 Qt-independent Core/Transport 合同。Qt5 `ARGB32_Premultiplied` 必须转换到 `RGBA8888_Premultiplied`，Qt6 已是目标格式时**不再转换**。不暴露另一个用户配置开关，不修改 Core storage API，也不增加 OS/GPU 分支。

**输入校验与安全回退**：只接受目标 `RGBA8888_Premultiplied` 的非空已拥有 QImage；验证 width/height、`bytesPerLine >= width * 4`、`sizeInBytes >= stride * height`、安全乘法与 `constBits()!=nullptr`；若不能映射，维持当前 `CpuFrameStorage + memcpy` 路径（明确仅旧版后备，不能把不合法像素当成成功）。禁止在发布后调用 image 的可变 API，禁止传 `QPixmap` 跨线程。RFB、composite、watermark 均通过 `FrameStorage::mapRead` 读取，保留原始颜色和 format contract。

**退出/保留生命周期**：grab result 对象及 Qt 窗口销毁不得取消已发布 `QImage` 的像素持有；`shared_ptr<const FrameStorage>` 被最后消费者释放时由持有者析构 QImage；作为 CPU image 不依赖 render context，不保持 Qt GUI 对象/窗口指针；跨线程只读测试、目标停止/销毁后仍读旧已发布帧是代码 review / test 的必要门槛。

**效益边界**：一次帧少一次完整 CPU buffer allocation/memcpy，可能避免已经是目标格式时的深格式转换；**`grabToImage()` 额外离屏渲染 + readback 不变**，静态 100ms 采样策略不变，不能声称已解决 #789 ARM 多吃一核或 #567 的条带刷新。

**代码准入**：这是 G7 允许的 Shared Runtime 私有实现优化，不触及冻结产品/API/wire/security 合同。先以 **Draft 小型代码 PR** 编写、静态审查和独立确定性单元回归；Qt5.15 与 Qt6.8 两 Kit 编译/受影响测试通过、真实 Viewer 像素/内存/CPU 对照通过以后才能评价 Ready/合并。任何 ABI、时序、跨线程删除或实际功能不同立即 stop，不自动合并或推广。


## 10. H1 公共变化事件 NO-GO：统一 Qt LTS 下一阶段技术决策（2026-11-05）

**已完成证据**：[Local Owner #609 H1](https://github.com/skawu/HyRemote/issues/609#issuecomment-6645942372)，两代 Qt5.15.2/Qt6.8.3 独立 QQuickView，`QEvent::UpdateRequest`、`afterRendering`、`frameSwapped`、实际 `grabToImage` 的准确时间线。各 Kit 单次、小场景，只作机制证伪、不宣传普适 GPU 量化性能。

| 相位 | Qt 5.15.2 | Qt 6.8.3 | 判定 |
| --- | --- | --- | --- |
| A：静态、不抓图 | UR 0、帧 1 | UR 0、帧 1 | 本机静止不触发持续场景渲染 |
| B：静态、每 100ms 异步抓图 | 49 grabs → 49 UR / 49 frameSwapped | 54 grabs → 54 UR / 54 frameSwapped | **UR/帧信号被 grab 自激，不能作为独立 dirty oracle** |
| C：每 100ms 独立 UI 修改 + grab | 54 真实变化、55 grabs → 56 UR | 54 真实变化、55 grabs → 57 UR | **更新合并，无法由 UR 计数区分变化来源** |
| D：grab 在途发生 UI 修改 | 3/3 取回修改前的像素 | 3/3 取回修改前的像素 | **没有公开的 grab 像素快照修订边界**；ready 晚于 update event，且旧像素并未自动补获 |

**结论：NO-GO 是针对上述共同公共渲染通知/窗口事件的精确限定结论，不宣称已证明 Qt 的全部内部机制都不可能。** 禁止把 `afterRendering`、`frameSwapped`、`UpdateRequest`、单纯 `QQuickItem::update()` 或基于它们的「抓图来源扣除」实现为可靠的产品无变化闸门。尤其不得把「事件早于 ready」视为这一帧已经含有该次变化，不能因一次截图是旧画面就永久丢弃相应业务 revision。

### 10.1 剩余产品级性能根因：机制已证实，线程归因未完成

- [#609 Qt6 静态 Q0/Q1](https://github.com/skawu/HyRemote/issues/609#issuecomment-6628044624)：无 Viewer / 约 33s 是 `0 grabs / 1 render / 0.14% 单核`；一个 Viewer 时 `247 grabs / 269 render / 10.99% 单核`。**直接可控的根因是 Runtime 100ms 静态探针调用全帧 `grabToImage`，自激 Qt 渲染 + GPU→CPU readback**；RFB 静止帧可不发送数据。单机实验不能归因 Qt GUI、render thread、driver 的具体 CPU 栈份额，也不能代替 RK3588 源设备准入。
- [#842 A1 实测](https://github.com/skawu/HyRemote/issues/609#issuecomment-6644533595)：单次 Qt6 静态→动态 RealVNC A/B，正确性 PASS，Host CPU 48.5%→43.9% 单核、WS peak 252.2→239.6MB。A1 解决抓图**之后**的重复 CPU full-frame copy；对额外 scene render/readback 的触发频率零改善，故 #609 仍 OPEN。
- `QQuickItem::grabToImage` Qt 文档明确为 offscreen render + GPU→CPU copy；Qt5 默认直接 GL、Qt6.8 默认 QRhi，公共 `afterRendering` 时序不同。不要把 Qt5 framebuffer scraping 当成 Qt6 ABI 或产品接口的替代。

### 10.2 方案分层：适用于所有入口的核心合同与有边界的适配器

| 路线 | 适用范围 | 对「100ms 全帧探测」的实际价值 | 重要边界 | 裁定 |
| --- | --- | --- | --- | --- |
| **P0 当前公共 API 回退** | Qt5/Qt6、所有现有适用入口 | **不能根除**：只有像素比较后才知道没有变 | 可以保证本地独立变化有界发现；不得单纯把 100ms 改成 500ms/1s 来伪造低 CPU | 暂存，用户问题未解决 |
| **P1 Qt 应用协作源 Revision/Dirty 契约** | 应用显式集成的 C++/QML（经既有 Shared Runtime/目标适配，不增一个独立核心） | 在可靠修订标记、动画失效时可真正跳过不变帧抓图 | 无应用修改的 Generic/QPA **不适用**；不满足零代码完整产品目标，也不能把业务数据版本当成所有视觉变化版本；一次 grab 在途变化须累积 pending revision 并补获 | **可移植、可选补充；不作全产品根修复** |
| **P2 Qt LTS-specific scenegraph invalidation 适配** | Qt5.15/Qt6.8 版本能力适配层，在 **Qt Runtime 私有实现**；不按 OS/graphics backend 路线拆产品 | **理论上**可追踪 Qt 场景真实失效及提交修订，在静止时跳过重 render/readback | 公开窗口通知不足；Qt5/Qt6 内部 dirty 链表/renderer 生命周期与线程不同，可能需要 QtPrivate/匹配 **精确 Qt minor/patch Kit**；外部 OpenGL、独立 popup、QuickWindow 之外的 native UI、异步 revision↔snapshot 仍需 fail-closed；QtPrivate ABI、许可证/打包与零代码能力需 Human 批准 | **下一架构评审优先候选，尚未证明可行，未授权代码实施** |
| **P3 Qt 输出面复用 / 合成帧源** | 若 P2 无法达到产品约束或必须取消第二次 scene render 时 | **最大潜在收益**：只读回 native 已渲染结果，少一次场景合成 | Qt5 GL 与 Qt6 QRhi 具体 readback API、surface / Popup、异步时序、跨线程并发复杂；不能假定 QtPrivate 给了可靠 CPU readback，且不能强制某个 graphics backend | **后续高风险取舍，先架构设计/能力合同，不立即做 OS 专项 spike** |

### 10.3 LTS-first 架构评审必须回答的问题（不得靠另一个事件计数代替）

1. **真正 dirty 源在哪里？** Qt5.15/Qt6.8 的 `QQuickItemPrivate::dirty` / `QQuickWindowPrivate::dirtyItem` 及 render loop 消费路径有内部迹象，但私有符号是否可用、时序是否覆盖所有可见变化、能否获知**哪个 revision 被成功抓入帧**均 **NOT_PROVEN**。**不能**把读到 `dirtyItemList` 非空简单当作可用产品能力，更不能在公开 `UpdateRequest` 上贴私有 API 的名字。
2. **并发正确性：** 设单调 `requestedRevision`、`capturedRevision`、`inFlightRevision`，抓图发起时保留需求，`ready` 仅在证明快照确实包含该 revision 才前移已捕获版本。若未能标定快照边界，在途业务变化都必须保持 pending 并触发下一次；不得静默变成「无 dirty」。
3. **场景覆盖：** 普通 QML 属性/动画、ShaderEffect/视频/纹理提供器、QQuickWidget、Popup.Window/native surfaces、外部 OpenGL/3D、DPR/resize/隐藏/恢复/窗口销毁、水印/privacy、Viewer join/reconnect、RFB + Native 都须有保守回退。部分仅提供适用能力声明，不得宣称 Generic/QPA 零代码全面支持。
4. **可移植构建与法律边界：** 尤其不能假设某个 Qt5/6 Kit 自带全部 QtPrivate 开发头、链接符号；不能依赖 Windows WGC/D3D11/私有厂商驱动实现，也不能顺便改变首轮 GA Qt5.15/Qt6.8 支持声明。
5. **最小准入**：优先设计一个**版本化 CapturedContentRevision/dirty capability 内部接口**（非 Core 公共 API），将「来源可靠」、「覆盖范围」、「异步归属」与「什么时候必须完整回退」写清；只在 **Human 批准具体 QtPrivate 风险/产品覆盖取舍**后才允许做最小实验。禁止先写私有 hook 再寻找产品验收理由。

### 10.4 当前决策边界及行动

**Cloud Owner 立刻执行**：收口 #842 受控代码优化（已有 Qt6 8/8、Qt5 experiment storage 1/1、RealVNC 单次 A/B、独立 review 无阻塞），仅在新 HEAD 保护门禁通过时合并。**下一项不是让本地 Owner 再做 100ms/信号/Viewer 试验**，而是提交面向 Human 的 **Qt LTS-specific dirty contract vs 可选应用协作 vs 保守轮询**三路取舍卡。它是产品架构选择，不能由一次试验证据自动授权 QtPrivate 生产实现。

**建议**：以统一 Qt LTS-adapter 的私有场景失效契约作为主线研究（P2），可选 C++/QML 协作源版本辅助（P1），维持现有有界 fallback（P0）；**不把私有 Qt 源方案声明为已可实现**，Human 明确接受匹配 SDK/ABI 和 zero-code 功能覆盖边界前不写实现。若 QtPrivate 仍无法提供稳定内容边界，则正面交付「纯公共 API + 零代码 + 不遗漏任意本地变化 + 静态近零捕获成本」可能无法同时满足的产品取舍，不能无限绕圈调参数。

```text
H1_QEVENT_PUBLIC_RENDER_DIRTY=NO_GO_QT5_QT6
SNAPSHOT_CONTENT_REVISION_FROM_PUBLIC_SIGNALS=NOT_PROVEN
QT_LTS_PRIVATE_SCENE_REVISION=ARCHITECTURE_REVIEW_OPTION_ONLY
COOPERATIVE_DIRTY_REVISION=OPTIONAL_NOT_ZERO_CODE
A1_COPY_OPT=VALIDATED_SCOPE_PENDING_MERGE
#609_USER_OUTCOME=P1_OPEN
RK3588_TARGET_OUTCOME=NOT_RUN_ON_NEW_SHA
G7_FROZEN=UNCHANGED
```


## 11. QtPrivate 精确源版本技术可行性研究（2026-11-05；Human 仅批准研究，**实施未批准**）

**本节阶段：`SOURCE_FEASIBILITY_ONLY`，Qt 官方仓库的精确 tag `v5.15.2`、`v6.8.3` 已对照；没有使用本地 Qt Kit 验证 QtPrivate 头文件/动态符号，没有开发或改变任何 HyRemote 产品代码，也没有修改 Qt。此处发现不是产品实现的授权。**

### 11.1 精确源码核对结果（跨 LTS 共性与差异）

| 原理 | Qt 5.15.2 tag | Qt 6.8.3 tag | 结论 |
| --- | --- | --- | --- |
| item dirty 入口 | `src/quick/items/qquickitem.cpp:6155–6182`：`QQuickItemPrivate::dirty()` 更新 bitmask 并入 `dirtyItemList`，`dirtyItem()` 通知渲染 | 同源文件 `:6758–6784`：同构，但 transform 处理有变化 | 两代确实管理 scene item dirty；**不是持久 image revision** |
| dirty 列表消费 | `qquickwindow.cpp:3593–3611`：`updateDirtyNodes` 取链表并置 `dirtyItemList=nullptr` | `qquickwindow.cpp:1997–2015`：同样提取清空 | **轮询链表是否为空会漏掉已经同步消费的变化**；不能作为安全无变化判据 |
| grab 强制更新 | `qquickitemgrabresult.cpp:281–345`，create 内 `item->window()->update()`，抓图订阅 `beforeSynchronizing/afterRendering` | 同源文件 `:273–337`，同构 | **即使没有 dirty item，grab 仍可直接触发窗口渲染**；这解释 H1 自激，与 Runtime 100ms 探测一致 |
| window update 语义 | `qquickwindow.cpp:411–426` 明确即使场景图未改变仍发起 repaint | `qquickwindow.cpp:372–386` 同一语义 | “window render” 与 “QML 视觉像素发生了变化”不是同一个版本 |
| QtPrivate ItemChangeListener | `qquickitem_p.h:320ff` `ChangeType` + `qquickitemchangelistener_p.h` 只有几何、层叠、可见性、alpha、父子、启用等回调 | `qquickitem_p.h:300ff` 增加 Focus/AllChanges，`qquickitemchangelistener_p.h` 同类回调，**无 `Content` callback** | **注册已有私有 listener 并不能观察全部内容绘制/ShaderEffect/texture 更新**；根节点或每个子节点的 ChangeListener 不等于完整变更流 |
| `QQuickItemPrivate` DirtyType | 两代 `Content=0x40` 等定义存在，`dirty()` 非 virtual | 同构 | dirty flag 本身**不是可订阅、跨线程带 revision 的公开/受支持通知合同** |
| LTS Qt 后端 | 默认直连 OpenGL/GLES，使用 Qt 5 scenegraph/render loop | Qt 6 scenegraph 默认经 QRhi，多 GPU API | 按**Qt LTS**评价接口，避免生成 OS/GPU 分支；真实生产覆盖不能仅在 Windows 通过 |
| 私有头文件声明 | `qquickitem_p.h:43–50` 明确私人实现细节，版本间可能变化或移除 | `qquickitem_p.h:8–15` 同样警告 | 没有跨 Qt patch/minor ABI 或链接兼容承诺；需要精确 SDK/deploy/法律审查 |

**精确 Qt 源链接：**
- Qt5.15.2：[qquickitem.cpp](https://github.com/qt/qtdeclarative/blob/v5.15.2/src/quick/items/qquickitem.cpp)、[qquickwindow.cpp](https://github.com/qt/qtdeclarative/blob/v5.15.2/src/quick/items/qquickwindow.cpp)、[qquickitemgrabresult.cpp](https://github.com/qt/qtdeclarative/blob/v5.15.2/src/quick/items/qquickitemgrabresult.cpp)、[qquickitem_p.h](https://github.com/qt/qtdeclarative/blob/v5.15.2/src/quick/items/qquickitem_p.h)、[qquickitemchangelistener_p.h](https://github.com/qt/qtdeclarative/blob/v5.15.2/src/quick/items/qquickitemchangelistener_p.h)。
- Qt6.8.3：对应 [qquickitem.cpp](https://github.com/qt/qtdeclarative/blob/v6.8.3/src/quick/items/qquickitem.cpp)、[qquickwindow.cpp](https://github.com/qt/qtdeclarative/blob/v6.8.3/src/quick/items/qquickwindow.cpp)、[qquickitemgrabresult.cpp](https://github.com/qt/qtdeclarative/blob/v6.8.3/src/quick/items/qquickitemgrabresult.cpp)、[qquickitem_p.h](https://github.com/qt/qtdeclarative/blob/v6.8.3/src/quick/items/qquickitem_p.h)、[qquickitemchangelistener_p.h](https://github.com/qt/qtdeclarative/blob/v6.8.3/src/quick/items/qquickitemchangelistener_p.h)。

### 11.2 针对可落地性的否决与保留，不作乐观假定

| 可行性候选 | 当前证据等级 | 裁定 |
| --- | --- | --- |
| **F0 直接在 GUI 线程轮询 `dirtyItemList`** | 列表同步时被清空，版本会消失；grab 可无 dirty 强制 render | **NO-GO 作为唯一可靠变化判据**，不是线程安全产品接口 |
| **F1 注册私有 `QQuickItemChangeListener`** | 精确两代类型回调均缺 Content dirty，且根/所有子节点也不能覆盖外部纹理、QSG 直接变化 | **NO-GO 作为完整 visual dirty oracle**；作为局部失效辅助可研究，不能关掉 fallback |
| **F2 拦截 `QQuickItemPrivate::dirty()` 入队点** | 这是发生时间较早的真实 item dirty 来源，但函数非虚、没有可直接注册的 observer；如何无侵入监控未知 | **UNPROVEN/HIGH RISK**。不能从“源码存在 dirty()”推出独立可安装 hook；若要 Qt 源码补丁、编译器插桩、符号劫持或注入，应转入单独 Human 风险审批，现阶段不得实施 |
| **F3 对 Qt 控制的内容建立 revision↔grab snapshot 屏障** | 原始 H1 在途变更 6/6 返回旧帧；内部 dirty 列表消费并不告诉我们 `ready` 图像属于哪个 revision | **UNPROVEN，阻断任何无限期跳抓实现**。必须保证在途修改保留 pending，并在能证明图像包含该版本时才 acknowledge |
| **F4 Qt5/Qt6 render-output frame reuse** | Qt5 GL 与 Qt6 QRhi 资源/提交语义不同；公开 `grabToImage` 无法被当作被动 screen scrape | **RESEARCH_ONLY**。不是 F2 的自动回退解法，不能先做 Windows/D3D 或 GL 专项产品代码 |
| **F5 可选应用层 `visualRevision`** | 应用可在自己的全量视觉状态变化时提交 revision，能避开“抓图事件”和业务事件混淆 | **CONDITIONAL**。仅 C++/QML 配合场景；无业务代码的 Generic/QPA 仍需可靠的 fallback，且业务数据属性不一定覆盖视频/QSG/外部内容 |
| **F6 保守公共 Qt 抓图** | 当前统一 Runtime 路径能够按有界探针发现独立变化 | **KEEP**。不能等同于用户现场性能问题已解决 |

**本轮可明确排除的捷径：** `QtPrivate` *有一个 dirty 列表* `!=` *HyRemote 可以通过简单读取可靠跳过抓图*；`QQuickItemChangeListener` *有 AllChanges* `!=` *存在 ContentUpdate callback*；`ready()` *返回一张图片* `!=` *该图片包含 ready 前所有的业务变化*。

### 11.3 风险清单（实施前必须逐条交 Human）

| ID | 风险与未决项 | 重要性 | 研究证据/完成前提 |
| --- | --- | --- | --- |
| R1 | **信号覆盖缺失**：item-listener 与 dirty list 不是完整像素变更流，造成 Viewer 长时间显示旧内容 | **阻断** | 必须证明可独立观察内容失效，覆盖动画、ShaderEffect/texture、窗口内/跨窗口 UI，不得靠自激事件 |
| R2 | **in-flight 版本错误**：UI 修改后 grab 旧帧，错把 `ready` 当新鲜版本 | **阻断** | 需有单调修订号、提交/已捕获/待补获关系，异步重叠及 stop/reconnect 不丢修订 |
| R3 | **线程竞态与崩溃**：读取私有 GUI/render 链表，未保证并发/生命周期同步 | **阻断** | 不可直接在 render thread/网络 thread 遍历私有指针；需线程/锁/失效规则证据 |
| R4 | **QtPrivate 版本与 Kit 锁定**：Qt5.15 vs Qt6.8 私有布局/内部行为和发布安装差异 | **高** | 两版各自 exact Kit 编译/运行/升级兼容研究；未知或缺头必须安全回退 |
| R5 | **原生/零代码入口覆盖**：Generic/QPA、Widgets/Quick、多窗口、Popup/水印/privacy、视频/外部内容是否收到统一修订 | **阻断产品承诺** | 明确每个入口和 surface 的适用能力，不得用 C++/QML 业务协作冒充零代码解法 |
| R6 | **Qt 分发与许可**：如果需要修改/重建 Qt、分发 QtPrivate 相关二进制，许可证、SDK 提供方式、维护成本 | **高 / Human 决策** | 对应已安装 Qt Kit 授权与发布方式单独核对；需 Legal/Owner 批准适用条件 |
| R7 | **性能未达目标**：即使跳过静态重复 grab，动态图像 readback 仍可能吞掉 ARM 核；网络/条带问题独立存在 | **高** | 分开验证静态 + 动态真实 Viewer、RK3588 EGLFS，同源前后 CPU、流畅度及本地交互，不许以单机基准外推 |
| R8 | **回退与安全**：不能让缺少 QtPrivate、变化漏检、帧缓存失效绕过隐私/水印/安全或冻结远端 | **阻断** | 已有 100ms 公共 API 回退保留；丢能力、DPR/size/change、Viewer join、stop/restart 均回退并重新完整抓帧 |

### 11.4 后续研究门槛及必须 Human 再确认的审批卡

**当前已做**：两个精确 Qt LTS tag 源码/差异分析与可行性拆解。**未做**：真实 QtPrivate 头/链接可用性、可观察且不自激的 dirty 事件原型、准确 snapshot revision 屏障、Qt5/Qt6/ARM 跨设备验证。

**研究下一关 F-G0（仍属于可行性，不属于产品实施）**：仅在独立诊断程序里验证现有 Qt5.15.2/Qt6.8.3 **匹配安装的私有头/符号可用性**，识别能否在**不修改 Qt、自定义 interpose、不注入宿主进程**前提下观察每次原生内容失效。若不能，直接将该路线 `NO_GO_UNMODIFIED_QT`，**停止**，不得转为偷偷 patch Qt；不要求跑额外 16 格性能矩阵。

**研究下一关 F-G1（须 F-G0 先 PASS）**：在**独立 sandbox 非产品代码**中明确区分 grab-triggered update、真实业务 content dirty、在途 revision；单个失效能否合法从 GUI/render 更新边界上锁存为单调版本。发生任何漏报即按保守回退定义 `INCOMPLETE_COVERAGE`，不能自称成功。不会引入 Runtime 生产文件、SDK/public API 或 release 改动。

**产品实施前必须给 Human 展示并获得明确批准的选项**：
1. **是否允许 QtPrivate 生产依赖、锁定 exact Kit 与可能的 ABI/部署风险？** 不能只因为本次研究允许就默认批准。
2. **如果 F-G0/F-G1 不能证明安全完整失效流，是否接受（a）仅协作 C++/QML 的性能加速、Generic/QPA 保留较高 CPU；（b）受控修改 Qt build/patch 的部署和维护代价；或（c）继续公共 Qt 保守抓图及其已测 CPU 成本？** 不经 Human 审批不得下调四入口的产品能力承诺。
3. **性能与画面验收**：静态 CPU 降幅、持续 60Hz 源变化下 Viewer 真实新鲜度/延迟、RK3588 现场验收、Popup/水印/连接生命周期；没有采样就填 UNKNOWN，不许承诺具体 CPU SLO。
4. **回退/回滚**：不支持 Kit 或触发异常自动关闭私有加速，恢复安全完整帧并保留审计；明确关闭/升级/独立验证责任。

**明确停线：`HUMAN_FEASIBILITY_RESEARCH_APPROVED=YES`，`HUMAN_PRODUCT_IMPL_APPROVED=NO`。** Human 未读到上述技术问题/风险并明确选择前，云端及本地 Owner **不得**向 HyRemote 生产源码加入 QtPrivate、场景 dirty hooks、修改 Qt/构建部署、或默默削弱 zero-code 安全合同；即使 F-G0/F-G1 通过也只可提交研究结论、不能自动实施。


## 12. F-G0 深化：同步前只读 dirty 观察窗口与抓图反向污染（2026-11-05）

**阶段：源代码机制验证已完成；本地精确 Qt Kit 的私有头/链接 preflight 尚未运行；产品实施未获 Human 批准。**

### 12.1 两代 Qt 的同步窗口并非虚构，但也不是完整的变化事件

经直接核对 **qt/qtdeclarative `v5.15.2`、`v6.8.3`**：

- Qt5 `src/quick/items/qquickwindow.cpp:500–525`：`QQuickWindowPrivate::syncSceneGraph()` 先 `emit q->beforeSynchronizing()`，随后 `updateDirtyNodes()`；Qt6 对应 `:524–566` 同样如此。两代 `updateDirtyNodes` 均提取并清空 `dirtyItemList`。因此，**在 `beforeSynchronizing` 的 `Qt::DirectConnection` 里只读采样队列头**是一个*有时间窗口的研究候选*，比同步后随意轮询更有技术根据。
- 官方 Qt 6.8 [QQuickWindow::beforeSynchronizing](https://doc.qt.io/qt-6.8/qquickwindow.html) 保证 threaded render loop 中同步阶段 GUI 线程被阻塞；**但信号回调在渲染线程**，不能执行阻塞 I/O、访问未同步的外部 Runtime/Network 对象、遍历失效指针或持有任意跨线程锁。basic/threaded 生命周期不同，研究结果仍需按 LTS 精确评估。
- Qt5 `qquickwindow_p.h:109` 用 `Q_QUICK_PRIVATE_EXPORT` 声明私有类，Qt6 `:107` 用 `Q_QUICK_EXPORT`；两个版本 `dirtyItemList` 是内部可访问的数据成员（Qt5 `:232`、Qt6 `:188`），**不是 public ABI 约束，也不是可订阅内容回调**。Qt6 源 `src/quick/CMakeLists.txt` 存在 `Qt::QuickPrivate` 私有模块引用，但**不能由 Qt 源构建声明推出已安装 Kit 一定分发 `Qt6::QuickPrivate` 或 Qt5 相应 target**。本地 G0 要核实 target/headers/links。

### 12.2 一个此前未充分识别的污染源：抓图能制造 private dirty

两代 `QQuickItemGrabResultPrivate::create()` 不只调用 `window()->update()`，还在之前调用 **`QQuickItemPrivate::get(item)->refFromEffectItem(false)`**：
- Qt5：`qquickitemgrabresult.cpp:312–315`；Qt6：同文件 `:304–307`。
- Qt5 `qquickitem.cpp:6202–6212` 和 Qt6 `:6804–6814`：当 `effectRefCount` 首次从 0→1，`refFromEffectItem(false)` 调用 `dirty(EffectReference)`；若有父 item，还可能 `dirty(ChildrenStackingChanged)`。两代版本同构。
- **重要后果**：在 `beforeSynchronizing` 中发现 `dirtyItemList != nullptr`，仍**不能证明发生过业务视觉变化**。即便这次 capture 没有视觉变化，它自己的 effect-reference 构建也可能产生真实 internal dirty bit。简单筛选某一种 bitmask 也需要证明不会漏掉同时发生的内容变化、ShaderEffect、视频/外部纹理与渲染线程动画。

这比 H1 的公共 `UpdateRequest` 自激更深一层：**私有 dirty 本身也可能受 HyRemote 的抓图操作污染**。因此 F-G0 如能编译，应核查 exact dirty mask 来源及相位时序，绝不能因为看到“非空 dirty”就将 P2 判为 GO。**本源代码事实不足以推断每一次 grab 都产生 effect-dirty：该路径仅在 effectRefCount 的特定状态下设置。**

### 12.3 F-G0 最小独立研究 preflight（唯一下一项本地工作；不是产品实现）

在**现有** Qt5.15.2 / Qt6.8.3 Kit 上，使用各自**独立、限时、自退出的非仓库 `tmp/` Qt-only** 工程：

1. **先判断依赖**：CMake `Qt5::QuickPrivate`、`Qt6::QuickPrivate` 是否在已安装精确 Kit 实际提供；能否包含 `<QtQuick/private/qquickwindow_p.h>`、`<QtQuick/private/qquickitem_p.h>`，并以 Kit 原生目标成功编译、链接一个只读 `QQuickWindowPrivate::get(view)` 取指针程序。记录 Qt 版本、Kit、编译器、CMake target、头路径、第一条失败。**缺失即该 Kit `BLOCKED_KIT`，停，不下载/替换 Qt，不绕过现有 SDK。**
2. **只有两个 Kit 的实际 private preflight 可用才进入最小只读区分**：给现成 QQuickView **`beforeSynchronizing` Qt::DirectConnection**，仅取 `dirtyItemList` 是否非空、**已知 content item 的** `dirtyAttributes` bitmask、Qt 阶段/线程与捕获请求时刻；不遍历、变更/锁住 private 链表，不从渲染线程日志输出。先记录静态第一次同步；单独做固定五次 `grabToImage`；再单独修改少量 QML 颜色，检查 `Content` 与 `EffectReference` 区分/同时出现的情况。只有需要测试 `inFlightRevision` 时追加固定两次“提交抓图后改变视觉属性”的单独相位，仍只观察，不修改生产。
3. **严格止损**：只要抓图自身的 private dirty 无法与可见内容变化可靠分开，或 Kit 缺支持/同步数据存在未解释竞态，记录 `NO_GO_OR_UNKNOWN` 并立即停止。不要将测试扩展为 Window/D3D/EGLFS 产品后端、QtPrivate hook/符号劫持、Qt 重编译、常驻探针或 16 格矩阵。
4. 汇总至 #609，明确 `G0_KIT_LINK`、`G0_ITEM_DIRTY_PROVENANCE`、`SNAPSHOT_BARRIER` 三项的 PASS/FAIL/UNKNOWN。**即使观察成功也只表示一个狭义 Quick item 的研究假设成立；不代表完整 zero-code／Generic/QPA 覆盖，也不允许直接进入 Runtime 实施。**

### 12.4 下一次 Human 审查时的技术选择

若 G0 只能获得 private class 读取权限，但没有覆盖完整内容的非自激、版本化变化源，应将 **QtPrivate 只读方案判为对整产品 `NO_GO`**。不要以“能够 include QtPrivate”冒充“解决静态 CPU 问题”。随后应向 Human 提交三项真实产品取舍：

- 保持零代码与完整画面能力，接受现有有界轮询的静态 CPU 成本（可保留 A1 已完成的额外复制优化）。
- 对自有 C++ / QML 场景增加**明确可选**的真实 visual revision 通道，零代码入口不降低功能承诺但可能无法享受近零静态 CPU。
- 允许一个经过具体技术与许可评审的 **Qt LTS renderer integration / 受控 Qt 分发机制**，由 Qt 内部真实失效与捕获边界提供可验证版本。此选项可能显著增加应用集成/SDK/ABI/许可证/升级成本，实施前必须再次获得 Human 批准。

**仍然明确：`RESEARCH_APPROVED=YES`；`IMPLEMENTATION_APPROVED=NO`。不因独立诊断项目存在就视为允许注入或修改 HyRemote 产品代码。**


## 13. F-G0 实测验收与 F-G1 独立可靠性研究合同（2026-11-05）

**证据**：[本地 Owner #609 comment 6646475852](https://github.com/skawu/HyRemote/issues/609#issuecomment-6646475852)。与 §12 的源码假设相比，本阶段已经由实际安装的 Qt5.15.2/MSVC2019、Qt6.8.3/MSVC2022 双 Kit 在独立 Qt-only 程序中编译、链接和运行确认；保留 Qt5 Quick Experimental 产品状态。原始日志位于本地 `tmp/609-fg0`，此文件仅引用 GitHub 已回报的结构化数据，不宣称云端已读取本地每行日志。

### 13.1 F-G0 事实及边界

| 实验 | Qt5.15.2 | Qt6.8.3 | 结论 |
| --- | --- | --- | --- |
| 私有头/链接/只读访问 | `QQuickWindowPrivate::get`、`dirtyItemList`、`dirtyAttributes` 编译链接和运行 PASS | PASS | **F-G0 private-read 可行**，不等同于未来所有 patch Kit/ABI 被支持 |
| A：静态、无 grab | 1 次启动 dirty: `4/3/0/0` | 同 | 初始化项不代表每次都实际业务变化 |
| B：静态只抓图 | 41 grabs；首个 root `EffectReference`，其余 40 次空 dirty，`Content=0` | 45 grabs；首个 root `EffectReference`，其余 44 次空 dirty，`Content=0` | 抓图本身能污染 **private EffectReference**，但本场景未产生 `Content`；**不得假设所有 grab 都只有 EffectReference** |
| C：独立矩形颜色变化 | 45/45 `Content` | 45/45 `Content` | 在此 **Rectangle 颜色** 场景，区分业务更新与抓图自激 PASS |
| D：业务色块变化 + grab | 46/46 观察 `Content`；45 grabs | 45/45 观察 `Content`；45 grabs | 该同时发生场景无 dirty 漏报；**不是“最终抓回图像无陈旧帧”的证明** |

**F-G0=`CONDITIONAL_PASS_FOR_STANDALONE_QT_QUICK_RECTANGLE`**。由 Qt 源码可以校验：Qt5 `qquickitem.cpp:4386` / Qt6 `:4580` 的 `QQuickItem::update()` 都调用 `dirty(Content)`，所以该阳性对照在设计上合理。两代 `QQuickWindowPrivate::syncSceneGraph()` 的 `beforeSynchronizing` 早于 `updateDirtyNodes`，而后者消耗 private dirty 列表，解释为何同步前只读能观察到本次变化。

**重要限制**：不能根据 `dirtyItemList 非空且非纯 root EffectReference` 就宣布完整的可见场景变化 oracle。这个表达式只是一个待检验的候选启发式：现实 layer/effect 本身可能由 `EffectReference` 表示；原生 texture、shader uniforms、QSG render-thread animation、子窗口/Popup、外部 surface 未证实；真实 GUI 变化与抓图可在同一/跨相位出现，dirty 修订和 grab 返回 image 的同步边界尚未建立。

### 13.2 F-G1 首先验证语义与时间，而不是 CPU

**目标是证明/证伪一个内部版本化变更能力，在不改 Qt、不改 HyRemote 产品代码的条件下，避免永久漏掉可见业务变化。** 不为现有 Runtime 生产实现授权。只使用之前 F-G0 的两个精确 Qt Kit 与本地 `tmp/` standalone probe；完成一组固定、不循环扩展的短测试。每段输出计数、时间戳、dirty 类型、对应 item 身份、`beforeSynchronizing` 同步序号，以及实际图像颜色/空间变化。生产 CMake、Core/Runtime API、Viewer、传输均不得更改。

**F-G1a — 类型覆盖与自激负例**：
1. 复用原 B 阶段的少量静态 grab，检查不会把第一次 root `EffectReference` 当成**自动可以忽略的全部**变化，也不因为空 dirty 的同步产生虚假的业务修订。
2. 在独立 QML 测试对象上每次只触发一个**真正可见**的转换：位置/缩放等 Transform/Position/Size；opacity 或 visibility；叠放顺序/增删子项或 layer/effect 切换。每类做少量确定性更新，记录出现的**实际 DirtyType 组合**和是否有无法通过该观察点识别的真画面变化；**不预设“位置动画就是 Transform flag”**，须以最终源码/日志为准。至少一个简短持续动画周期，确认真实动态可见变化不会因 item 级 flag 去重而静默漏报。
3. 若实验时出现**可见像素已变化，但同步前 dirty 空或只有误排除的标志**，立即记录 **`COVERAGE_NO_GO_FOR_GENERIC`**，停止扩张；保留原公共 API fallback。ShaderEffect/video/external/native 未运行的场景明确 `UNVERIFIED`，不得判 `SUPPORTED`。

**F-G1b — 在途修订与旧图像屏障**：
1. 只做 **3 次可复现的抓图事务**，每次：发起异步 `grabToImage`；在既有同步观察点记录该帧的 `syncSerial`、相关 dirty bit 和测试 QML 逻辑 `sourceRevision`；发起后延时约 15ms 再做一次真实可见变化，等 `ready` 取图像像素与源修订信息，并在需要时发一次**明确的补抓**，验证新图是否包含变化。
2. 定义 `requestedRevision`（已知真实业务变化最新修订）、`observedSyncRevision`（在一次场景同步时看到的候选修订）、`confirmedImageRevision`（有**像素证据**证明确实进入该次图片的最新版本），对各次抓图按单调时间记录。**不得**因为 `ready` 晚于 dirty 通知就自动 `confirmedImageRevision=requestedRevision`；必须保留 pending，直至图像确实包含新 revision。将 `grabToImage` 内部的 `beforeSynchronizing` setup / `afterRendering` render 先后顺序作为源级技术依据，是否可以在生产界限无 race 仍 **NOT_PROVEN**。
3. 若无法将某次 ready 图片可信地归属于同步周期，或改变发生在 sync 后却返回旧像素，记录 `SNAPSHOT_BARRIER_UNPROVEN`；不能由一次“补抓正确”推导任意复杂场景不会漏帧。需要阻断或丢帧处理时，不允许锁住 GUI/render 线程来伪造同步；旧帧可合法上屏，但待变更修订必须继续 pending。

**所有结果均是**：`QT5_EXPERIMENTAL_QUICK` 和 `QT6_683_QUICK` **独立可行性证据**，不是 Generic/QPA/Widgets/Native、多窗口、ShaderEffect/视频/RK3588 的支持承诺，也不能自动转成产品功能。

### 13.3 研究停止条件与下一 Human 风险卡

1. 任一**可见更新漏报**、effect-only 真变化被跳过、无法区分 `ready` 的旧快照与后续 dirty、新增跨线程数据竞态或 toolkit 不兼容，判 `NO_GO_OR_SCOPE_LIMITED`，停止测试扩张。**不要求实际黑屏才判失败**；源码和观察证据也能判 NO-GO。
2. 即使以上通过，也只能形成 `FEASIBLE_FOR_COVERED_QT_QUICK_CASES`，**不能**直接将现有 100ms fallback 移除。下一决定应明示未覆盖 surface 的 fallback 成本、Qt5 private ABI 风险、Qt6 QRhi/effect 行为、pre-sync 读锁与 multi-window 竞态。
3. 下一份实施前 Human 审批材料需写明：① private exact-Kit 兼容与部署/许可；②真实内容 revision 如何产生/如何处理 render-thread animated/texture；③同步序号如何绑定抓图像素版本并保留 pending；④ 四种接入/Popup/跨窗口/安全水印与隐私能力哪些可加速哪些继续保守回退；⑤ static/dynamic Viewer FPS/延迟/CPU，RK3588 单独验证；⑥异常退回公共抓图及停用私有路径；⑦不采用方案时保留原产品约束的性能后果。
4. **明确审批状态**：`HUMAN_RESEARCH_APPROVED=YES`；`HUMAN_QT_PRIVATE_PRODUCTION_IMPL_APPROVED=NO`；`F-G1_TEST_APPROVED_WITHIN_RESEARCH_SCOPE=YES`；`F-G0_PASS_NOT_EQUAL_PRODUCT_GO`。所有生产提交必须在人审与风险裁定之后另行申请授权。


## 14. F-G1 实测验收、拟议采集状态机与 Human 实施前决策（2026-11-05）

**审批边界：** Human 已批准 **Qt5/Qt6 私有接口的技术可行性研究**，但**没有批准 HyRemote 产品实施**。本节不授权提交任何 Runtime/QtPrivate 生产源码、修改 Qt SDK 或延长/关闭静态检测时钟。本设计的建议和实施审批卡必须在 Human 明确决策后才可进入生产分支。

**实测来源**：[本地 Owner #609 F-G1 完成报告](https://github.com/skawu/HyRemote/issues/609#issuecomment-6646603431)。独立 Qt-only `tmp/609-fg1`，Qt5.15.2 / MSVC 和 Qt6.8.3 / MSVC 各运行一次 10 s，默认 Windows 图形 API；**云端已核对 GitHub 报告与精确 Qt tags 源码，未直接读取本地 tmp 的原始日志**。

### 14.1 已确证 / 仍未证实（不可混淆）

| 判断 | 实测 / 依据 | 可以用于设计的结论 | 不能作出的推断 |
| --- | --- | --- | --- |
| 7 类离散变化都在同步前产生 item dirty | 位置 `0x8`、width `0x10`、opacity `0x100`、visible `0x10000`、z + 父 stacking `0x20/0x400`、layer enable/disable `0x28000` + ShaderEffectSource；**双 Qt 7/7** | 在这些 QQuickItem 路径中，观察 `dirtyAttributes` 比公共 window render 事件更有判别力 | 不能扩大为所有 Qt Quick 绘制内容或所有接入模式无遗漏 |
| 动画同步观察 | Qt6 `72 tick→58 sync`（58/58 带 Transform）；Qt5 `73 tick→51 sync`（51/51 带 Transform）；无空 dirty sync | **已发生的每个 sync** 在这个动画中都携带场景变化；发生 coalescing | 不能把 58/58 说成 72/72 个逻辑状态均独立显示；不证明达到源 60 FPS 或 Viewer FPS |
| `ready()` 的图像版本 | 3 in-flight 操作×2 Qt = **6/6 取回提交时内容**；提交后 ~15ms 改色、ready 时源版本已前进；随后补抓 **6/6 取回新色** | `ready` 时间**不能**标成它之前全部业务更新对应的最新像素；观察到后续真实 dirty 则应让待补抓保持 pending | **“任意 grab 返回图像版本都精确等于调用时版本”未证实**；真实图像来源于后续 scenegraph sync/render，时间点受调度影响 |
| 原生 Qt 私有入口 | F-G0 已确认双已安装 Kit 的 `QQuickWindowPrivate` 只读可编译链接 | 可以开展严控的版本化设计 | private ABI 不保证 Qt patch 后稳定；未证明 QPA/Generic/独立 surface 适用 |
| 真正性能改善 | 本轮没有 Viewer、CPU/RSS、嵌入式设备测量 | **仅算法可行性** | 禁止宣称可将 #789 ARM 静态满核问题解决或产出具体 CPU 降幅 |

**额外时序结论（来自两代 Qt 源码）：** `QQuickItemGrabResultPrivate::create` 为捕获请求安排下一次 `QQuickWindow::update()`；抓图 `setup()` 在 `beforeSynchronizing`、`render()` 在 `afterRendering`；随后触发 GUI 线程 `ready`。因此图像的真实内容边界取决于**哪个 sync 被该 grab 使用**。在目前实验中“提交时版本”是避免把旧图误认成最新的**保守标记**，不能作为无例外的精确快照标识。对于 *submit 后、sync 前* 的更新，即使结果包含新内容，保守重复抓帧通常是多做工、不会漏画面；相反按 `ready` 当前状态确认则可能永久漏掉未抓入的更新。

### 14.2 F-G1 判定与真正未闭合的产品问题

**F-G1=`PASS_FOR_TESTED_STANDALONE_QT_QUICK_SCENES`**。研究进入“方案风险与决策”阶段，**不是** `PRODUCT_READY`。最重要的**阻断设计缺口**是**变化源覆盖资格**：任意 Qt Quick 内容可能有 ShaderEffect/time uniforms、视频纹理/texture provider、render-thread animator、窗口外独立 surface、popup/native window、非 QQuickItem 绘制、外部 Qt/OpenGL 更新。若其可见像素能变化而同步前没有相应 item dirty，停止 100ms fallback 会产生**静默冻结**。本轮离散变化 7/7 和当前测试动画并未证明这些情况安全；**不能靠“未观察 dirty”自动将任意窗口标记为可跳抓的 Eligible**。

其次存在 **snapshot↔dirty 的证明缺口**：只有“提交/ready 时间”而无稳定的 `capturedSyncSerial`，则不能把私有观察的业务版本严格归属到图像像素；当前最安全语义是**保留 pending + 必要时补抓**，而不是精确确认任意图像已包含该版本。要承诺所有 source/Viewer 不丢刷新，必须对多次积压、异步完成顺序、stop/reconnect/resize 给出单调版本规则并满足测试。

### 14.3 内部最小状态机提案（**仅设计，不是已批准实施**）

**架构原则**：保留 G7 Core / Shared Runtime 分界与 `CaptureSource` / `RemoteFrame` 合同，只对 **Qt Quick 目标**提出一个私有 `DirtyRevisionCapability`，通用 `ViewerCaptureDemand` 不得因 Quick 专项假设改变 Widgets/Generic/QPA 的现有行为。Qt5.15 / Qt6.8 各自 exact Kit adapter，只记录同步点 item bitmask/同步序号，绝不从 render thread 等待网络/UI、遍历失效对象、变更 Qt 私有列表或传 QtPrivate 类型入 Core。Core scheduler 的 pace/in-flight 上界照旧，RFB 像素差异/水印与安全链不被跳过。

拟议生命周期与状态：
1. **`UNQUALIFIED_FALLBACK`（所有目标默认）**：继续公共异步 `grabToImage` + 现有 100ms bounded static probe。只有经过**明确完整能力资格判定**的 Quick 目标才可在后续实施中申请 `ELIGIBLE`。没有可靠资格机制时，**不应交付 auto-on 优化**。
2. **`ELIGIBLE_IDLE`**：已持有有效的最近 `RemoteFrame`，无需业务 dirty 则不发新 capture；此状态需要证明所有可见内容源都在 QtPrivate dirty revision 管辖内，并能与自激性 `EffectReference`、窗口生命周期事件正确区分。异常、窗口变更、未知 dirty 类型、跨 surface 都转 `FALLBACK`。
3. **`DIRTY_PENDING`**：可信 item dirty 出现时推进内部单调 `observedRevision`（不是像素版本），从 render synchronization 回调通过**无阻塞、生命周期锚定的信号**安全唤醒捕获调度。远程输入、Viewer 加入、重新可见、DPR/size/popup/privacy/水印变更和超时恢复均强制 request full capture，不能等待“下一次脏项”。
4. **`CAPTURE_IN_FLIGHT`**：request 入队时记录 `submissionRevision`、`requestId`、目标代际 `generation`；观察到的后续 dirty 始终保留为 pending。记录真实与本次 grab 关联的 `syncSerial` 需要进一步证明其 capture attribution；不可假定 `ready` 时间就是 captured revision。
5. **`COMPLETED_CHECK`**：收到 `ready` 先按旧路径发布正确像素；若 `observedRevision > submissionRevision`，需明确补抓直到交付更新或转有界 fallback，避免只显示陈旧帧。若快照串联/队列溢出/后端能力未知，不得将 pending 清零；转 `UNQUALIFIED_FALLBACK`。确保不会因高速动画无限补抓淹没 Host，保持 pacing / backpressure / latest-only 语义。

**验证与受益边界**：这种设计有希望减少**静止且已具备可信能力资格**的 Qt Quick 目标的抓图次数；对仍使用 100ms fallback 的零代码通用窗口没有该收益；动画仍可能频繁 grab/readback。不能在完整资格与覆盖风险闭合前承诺“零代码 Quick 默认静态近零 CPU”。

### 14.4 Human 实施前必须裁定的风险与实际取舍

| 风险 | 等级 | 具体后果 | 需要 Human 明确知情/接受的条件 |
| --- | --- | --- | --- |
| **R1 变化源覆盖不完整** | **BLOCKER** | Shader/视频/纹理、render-thread 动画、Popup 或第三方自绘未触发 item dirty，远端永远停留旧画面 | 只有可被证明完整覆盖的 Quick 目标允许激活，其他继续保守 100ms，不能把 7/7 扩为所有入口 |
| **R2 快照版本与在途更新** | **BLOCKER** | `ready` 是旧像素却确认最新 revision，丢更新；竞态时可能补抓风暴 | 必须保留后续 dirty pending、强制补抓/失效回退；submitVersion 是安全保守参考，非普适精确标签 |
| **R3 render/GUI 与 Core 锁顺序** | **BLOCKER** | 死锁、取悬空 item、退出时触发无主回调 | sync 只读轻量、回调无阻塞；有 generation/unsubscribe/drain，受停止/销毁/重连约束 |
| **R4 精确 Qt Kit / 私有 ABI** | **HIGH** | Qt 更新/不同构建的 private 布局变更、Qt5 experimental Quick 质量未知 | 精确 Kit 编译+运行门禁，未知/替代 Kit 默认 fallback；不得冒称跨 Qt ABI 兼容 |
| **R5 四入口及双协议覆盖** | **BLOCKER product promise** | Quick 场景局部优化被误写为 Generic/QPA/Widgets 整体性能修复 | 单独声明适用的 Qt Quick surface；其他入口完全不变，保持零代码连接功能和隐私/授权 |
| **R6 发行与许可** | **HIGH** | QtPrivate/private SDK 的开发、分发、许可条款与维护升级成本 | Human 审核准确 Qt 许可/部署方式；本研究并未批准修改或分发 Qt |
| **R7 资源与实时性未验证** | **HIGH** | 静态降耗有限、动态滞后或多补抓、RK3588 +1 core/条带仍存在 | 以 Windows 同源实机 A/B 和 RK3588 EGLFS 后续目标验证分别裁定；真实 Viewer FPS/latency 不可由源动画 tick 代替 |
| **R8 故障回退** | **BLOCKER** | 误启 private path 或 dirty 漏报时远端永久冻结，水印/敏感画面失效 | 缺资格/异常/超时/切换强制完整抓图；保留 100ms 安全兜底；设计可撤销、可记录并快速停用 |

**三种产品决定方案（仍待 Human 选择）**：

- **选项 A — 推荐的受限研究延续**：允许仅 Qt-only/独立分支的**资格识别与失效覆盖可靠性**继续小范围技术验证，尤其 ShaderEffect 动态纹理、render-thread animator、独立 Popup/窗口内容，不改 Runtime/Qt/SDK；证明具备可安全开启的有限 Quick profile 后再请求实施批准。益处：优先防止静态 CPU 优化造成产品冻结；代价：性能根因修复尚未交付。
- **选项 B — 允许受限产品开发（需要新的 Human 明确批准）**：只为明确限定的 Quick 场景引入 Qt5/Qt6 exact-Kit private observer + revision/bounded fallback，**默认不开**、不改公共 API/零代码入口，先独立 PR + 真实 Viewer 回归/静态和动态 CPU 对照；任一 blocker 不满足即停。不应谎称对所有 Quick/Generic/QPA 自动起效。Human 必须接受 R1–R8 的残余风险，尤其只能对确定适用的 Quick 场景提速。
- **选项 C — 维持现有**：保留 #842 A1 的去重复制收益与 100ms 有界静态探测，不引入 QtPrivate 生产依赖；接受目前已证实的抓图渲染 CPU 成本，#609 用户问题仍 P1 OPEN；需要另行设计 Qt LTS render-output reuse/受控 Qt 分发技术路线并提 Human 决策。

**Cloud Owner 现阶段建议 A**。F-G1 的成功证明了 Qt LTS 观察方向具有可行性，但不足以给出可自动开启且满足零代码通用能力的生产安全合同。实际实施**仍须在 Human 了解当前信息与完成的风险结论后**再次明确批准。任何 Human 暂未选择，不允许以“已授权技术研究”推导 B 自动生效。

```text
F_G0=CONDITIONAL_PASS_QT5_QT6_INSTALLED_KITS
F_G1=PASS_TESTED_VISUAL_DIRTY_AND_REGRAB
F_G1_GLOBAL_QUICK_COVERAGE=UNVERIFIED
F_G1_IMAGE_SYNC_SERIAL_BARRIER=UNVERIFIED
F_G1_GUI_CORE_THREAD_SAFETY=NOT_PRODUCT_TESTED
RECOMMENDED_NEXT=RISK_CLOSURE_RESEARCH_OPTION_A
HUMAN_FEASIBILITY_RESEARCH_APPROVED=YES
HUMAN_PRODUCT_IMPLEMENTATION_APPROVED=NO
HUMAN_QTPRIVATE_DEPLOYMENT_ACCEPTANCE=NOT_YET
PRODUCT_CODE_CHANGED_BY_THIS_RESEARCH=NO
```


## 15. F-G2 反例优先：Render-thread Animator 可能绕过 item dirty（2026-11-05）

**范围和审批**：只做技术可行性研究。F-G0 / F-G1 在普通 `Rectangle` 修改及 16ms GUI 定时器动画上双 Qt PASS，结论有效，但不得据此给任意 Qt Quick 窗口授予“dirty=empty ⇒ 图像不变化”资格。**没有实施授权**，不修改 HyRemote、Qt SDK 或 release。

### 15.1 精确 Qt tags 源码发现（构成高优先级潜在反例）

通过 GitHub 连接核对 `qt/qtdeclarative@v5.15.2` 与 `@v6.8.3`：

1. 两代 `src/quick/util/qquickanimatorjob.cpp` 中的 **`QQuickOpacityAnimatorJob::updateCurrentTime` 直接调用 `m_opacityNode->setOpacity(m_value)`**（Qt5 行 610 起；Qt6 行 571 起）。这改变的是 **scenegraph 的 `QSGOpacityNode`**，没有逐帧调用 `QQuickItemPrivate::dirty(OpacityValue)`。渲染线程的 transform helper 也直接调用 `node->setMatrix(m)`（Qt5 行 413 起、Qt6 行 386 起）。
2. 两代 `src/quick/util/qquickanimatorcontroller.cpp::advance` 在运行时 `job->commit()`，并在仍有动画运行时执行 **`m_window->update()`**（Qt5 行 96 起、Qt6 行 59 起）；`beforeNodeSync()` 对待运行动画也能直接安排 `m_window->update()`。故**可以有真实 scenegraph 节点变化 + Window 更新，却不必每帧有 QQuickItemPrivate item dirty 入队**。
3. [Qt 6.8 OpacityAnimator 官方文档](https://doc.qt.io/qt-6.8/qml-qtquick-opacityanimator.html) 解释它可以在渲染线程执行，且 GUI `Item::opacity` 会在动画结束后才写回；[Qt 6.8 Scene Graph 文档](https://doc.qt.io/qt-6.8/qtquick-visualcanvas-scenegraph.html) 区分 GUI animation driver 与 render-thread Animator driver。**F-G1 采用 GUI 定时器改动属性的 51/51、58/58 sync dirty 不能代表 OpacityAnimator、XAnimator、ScaleAnimator 这一路径。**
4. 另外一个外部窗口状态路径：两代 `src/quick/items/qquickwindow.cpp::setColor` 都直接变更 `QQuickWindowPrivate::clearColor` 并调用 `update()`，在 scenegraph sync 后仅把 clearColor 传入 renderer，未按每次颜色变更向 item dirtyList 入队（Qt5 行 4951 起、Qt6 行 3257 起）。它证明**实际窗口背景视觉输出不是纯 item dirty 的完整子集**；不过 **HyRemote 当前 `contentItem()->grabToImage` 是否会包含 Window 背景须单独核实**，不能把背景清色当成未经验证的 Viewer 像素漏报。
5. 源码核查支持判断 **`ITEM_DIRTY_ONLY_GENERIC_QUICK=DESIGN_NO_GO_UNLESS_COUNTEREVIDENCE`**。这是机制级反例线索，**Qt5/Qt6 目标 Kit 的实际图像和 dirty 时序反例尚未测**，不能谎称已有某个 Viewer bug。

准确源码链接：Qt5 [animator job](https://github.com/qt/qtdeclarative/blob/v5.15.2/src/quick/util/qquickanimatorjob.cpp)、[animator controller](https://github.com/qt/qtdeclarative/blob/v5.15.2/src/quick/util/qquickanimatorcontroller.cpp)、[window](https://github.com/qt/qtdeclarative/blob/v5.15.2/src/quick/items/qquickwindow.cpp)；Qt6.8.3 [job](https://github.com/qt/qtdeclarative/blob/v6.8.3/src/quick/util/qquickanimatorjob.cpp)、[controller](https://github.com/qt/qtdeclarative/blob/v6.8.3/src/quick/util/qquickanimatorcontroller.cpp)、[window](https://github.com/qt/qtdeclarative/blob/v6.8.3/src/quick/items/qquickwindow.cpp)。

### 15.2 唯一下一项独立研究任务 F-G2：两 Kit 一个原生 Animator 反例测试

**目的不是继续“证明变化有 dirty”，而是以最小成本确定“可见图像改变而 item dirty 为空”是否真实出现。**

- 用 F-G1 现成独立 `tmp/` Qt5.15.2/Qt6.8.3 工程，各运行一个有限时长的 **`OpacityAnimator` QML**：固定背景、单个红/白不透明矩形，动画持续约 1.2s `opacity:0.2→0.8`（必须使用 **`OpacityAnimator`，不是 `NumberAnimation`/定时器改 `opacity`**）。记录 per-sync `beforeSynchronizing` 中 item dirty 位和 sync serial、真实原生 `frameSwapped`/render loop，以及**独立于 dirty 的 QImage 像素检查**；最多 2–3 次稀疏 `grabToImage` 作为读回验证，并明确标注其自激时段，避免反过来“用 grab 信号证明 native 内容 dirty”。只观察、不修改 private 结构/线程或 Qt SDK。
- 判断：在动画 **持续期间**，若确有相异的 Qt 图片像素或节点视觉状态，但存在一个或多个**无对应 item dirty** 的原生渲染帧，`F_G2_ITEM_DIRTY_ONLY=NO_GO`。一旦可信负例出现，就停，不追加更大覆盖矩阵；将 render-thread animator / native output 帧变化作为独立变更来源要求。若两 Qt Kit 未复现，也只能判 `NOT_REPRODUCED_FOR_KITS`，**不能**证明全平台 no-miss；仍需注明 Qt 源机制风险。
- 不进行 Windows GPU API 指定、真实 Viewer、CPU 性能 A/B、无上限循环、QtPrivate Hook 或 HyRemote 产品开发。两 Kit 各一次，程序自行退出并在 #609 写一次结论。

### 15.3 同时收敛“复用 Qt 已渲染帧”路线（纯研究，不扩大本地试验）

- [Qt 6.8 QQuickRenderControl](https://doc.qt.io/qt-6.8/qquickrendercontrol.html) **不是现有可见 QQuickWindow 的透明监听器**：它要求应用自己控制渲染循环，并把 Window 设为离屏；自有 Qt Quick 集成可以研究，但不能对 Generic/QPA 或现有零代码 Quick 自动替换，而不改变应用集成与视觉行为。
- [Qt 6.8 QQuickWindow 渲染信号](https://doc.qt.io/qt-6.8/qquickwindow.html) 可给出 render-thread 录制阶段入口；**并未直接提供一个跨 Qt5 OpenGL / Qt6 QRhi、所有 surface/多窗口都可消费且由 CPU 拥有的已合成帧**。即使复用真正 frame output，也仍可能要独立 GPU→CPU readback/同步/颜色转换，不能把它写成天然零拷贝。
- 因此 **`P3_REUSE_COMPOSITED_FRAME=TECHNICALLY_POSSIBLE_AS_BACKEND_DESIGN_HYPOTHESIS_ONLY`**，不等同于现有 Core/Runtime 中随时可得的帧；必须列明渲染线程 / render pass / 回读复制时序、帧版本、窗口外绘制、多后端、关闭与恢复、以及 Qt5/Qt6 ABI。
- 若 F-G2 击穿 item dirty-only，**不得**改用 `frameSwapped/afterRendering` 来声称独立内容变更：H1 已证实这些公共通知受 grab 自激。后续方案必须明确区分**来自真实渲染工作**与**由自身主动 grab 产生**的 frame，并解决“已渲染内容怎样成为 RemoteFrame”这一产品成本问题。

### 15.4 Human 实施前决策更新

旧 §14 **受限自动跳抓（B）**在 F-G2 的来源覆盖风险完成前仅是风险取舍候选，**绝不可**借 F-G1 PASS 把它当作可上线设计。建议继续选 **A — 风险封口研究**：优先 F-G2 一个真正的 render-thread 动画反例；若确认漏检，直接把 **QtPrivate dirty-only 通用方案判 NO-GO**，将完整 GUI+render 变化源及 P3 Qt 输出帧复用纳入新的可行性/实施风险卡，给 Human 决策。禁止默认关闭现有 100ms fallback，禁止隐性牺牲四接入零代码特性。

```text
F_G1_COVERED_ITEM_MUTATIONS=PASS
F_G2_RENDER_THREAD_ANIMATOR_DIRTY_COVERAGE=SOURCE_RISK_IDENTIFIED_LIVE_NOT_RUN
QT_PRIVATE_DIRTY_ONLY_GENERIC_QUALIFICATION=NOT_APPROVED
RESEARCH=AUTHORIZED
PRODUCT_QTPRIVATE_IMPLEMENTATION=NOT_AUTHORIZED
```


## 16. F-G2 原生渲染线程反例验收及 Qt LTS 被动帧复用研究路线（2026-11-05）

**状态与审批**：这是 Human 已允许的技术可行性研究，**绝非产品实施批准**；本节只修改设计文档，不改 HyRemote Runtime、Qt SDK、develop、发布分支或 G7。来源：[Local Owner #609 F-G2 报告](https://github.com/skawu/HyRemote/issues/609#issuecomment-6646844859)。Qt5.15.2 / Qt6.8.3 默认 Windows 图形栈、独立 Qt-only `OpacityAnimator` 程序（各 4s、到首个反例停止）；云端未直接读取本地 `tmp` 原始日志。

### 16.1 已确认反例：通用 Quick item dirty-only 方案 NO-GO

| 观测 | Qt5.15.2 | Qt6.8.3 |
| --- | --- | --- |
| 原生帧数推进 | 1→65，约 64 帧 | 1→77，约 76 帧 |
| 稀疏 grab 红像素 | 104→110→138 | 108→149→185 |
| 同步次数 / 空 `dirtyItemList` | 53 / **52** | 38 / **36** |
| 非空来源 | root `EffectReference 0x8000`（grab 相关） | 同左 + 动画结束 `OpacityValue 0x100` |

**终局裁定：`F_G2=NO_GO`；`PRIVATE_ITEM_DIRTY_ONLY_UNIVERSAL_QUICK_SUPPRESSION=REJECTED`。** 动画期间存在大量可见像素/原生渲染推进而 item dirty 为空的真实反例。因此 F-G0 私有访问和 F-G1 普通 GUI item 变化、GUI 定时器动画的限定 PASS 不能泛化到 render-thread `OpacityAnimator`。不得自动启用 §14 的 `ELIGIBLE_IDLE` dirty-only 状态机，不得将 `Content` 或“非空 item dirty”的存在作为任意零代码 Quick 的唯一抓图闸门。也不能退回公共 `UpdateRequest/frameSwapped/afterRendering` 当内容变化信号，H1 已证明它们被主动 grab 自激。

### 16.2 新目标 P3：从“主动抓图导致额外 render”到“已有原生帧被动 CPU readback”

本方向以**实际 Qt scenegraph 正常渲染帧**为来源，希望消除为检查变化而周期主动发起的 `QQuickItem::grabToImage()`；因此自然包含 Qt GUI item 更新、render-thread Animator 及其他确实进入目标原生帧的变化，静态帧不再因判断是否变化而被主动唤醒。但**从原生 GPU render target 读回 CPU 仍可能是昂贵的同步/像素转换，不等于零拷贝**，且目前只是技术候选。

精确对照 `qt/qtdeclarative@v5.15.2` 与 `@v6.8.3` 的渲染实现：

- **`QQuickWindow::grabWindow()` 不是被动替代**。两代 `src/quick/scenegraph/qsgthreadedrenderloop.cpp::grab()` 都先 polish/sync 并发 `WMGrabEvent`；Qt6 的 `WM_Grab` 路径直接执行 `beginFrame → syncSceneGraph → renderSceneGraph → grabAndBlockInCurrentFrame → endFrame(SkipPresent)`，所以还会主动 render+readback。即使像素完整，也不能宣称消除了额外 render。
- **Qt6 QRhi 具备在当前帧读回 backbuffer 的内部示例**：`src/quick/scenegraph/qsgrhisupport.cpp:1302+` 使用 `QRhiReadbackDescription(src=nullptr)`（swapchain）和 `readBackTexture`、`cb->resourceUpdate`，随后的 `rhi->finish()` **强制等待 GPU**。其存在说明理论上可在真实帧内安排 readback，但不能直接照搬阻塞实现；须研究异步 completion、资源保留和 command buffer 时序，不能从源码直接宣称产品安全。
- **Qt5 GL / Qt6 QRhi 是 Qt LTS 版本适配问题，不是 OS 适配问题**。Qt5 默认 OpenGL 需在真实渲染线程/合法目标 framebuffer 上读回（可用 `glReadPixels` 作受限同步基线，但可能停顿）；Qt6 `QSGRendererInterface` 能提供 `RhiResource`、`CommandListResource`、`RhiSwapchainResource` 查询入口，但访问私有 QRhi 类型仍需 exact Qt Kit/编译许可评审。Qt5 同一 `QSGRendererInterface` 根本没有 `RhiSwapchainResource`。
- **`QQuickRenderControl` 并非“对已显示可见窗口的透明录屏”**：两代源码 `src/quick/items/qquickrendercontrol.cpp` 定义为应用自己管理 offscreen target、context、render loop；Qt6 明确此 Window 不应 show。它可能适用于自有 C++/QML 重新集成，却不等同于对已有零代码 Quick/Generic/QPA 的产品直接优化。
- **帧边界有产品语义风险**：原来 `contentItem()->grabToImage` 的范围与 Native Window swapchain 背景、alpha、Popup 子窗口/独立 surface、overlay、隐私遮罩、水印、多个观众和 DPR/resize **不能预设一致**。凡不覆盖的场景须保持当前安全采集，不能因为 Native Tap 识别原生帧就跳过产品的隐私与渲染合成。

对应精确 Qt 源：
[Qt5 render loop](https://github.com/qt/qtdeclarative/blob/v5.15.2/src/quick/scenegraph/qsgthreadedrenderloop.cpp)；
[Qt6 render loop](https://github.com/qt/qtdeclarative/blob/v6.8.3/src/quick/scenegraph/qsgthreadedrenderloop.cpp)；
[Qt6 QRhi readback](https://github.com/qt/qtdeclarative/blob/v6.8.3/src/quick/scenegraph/qsgrhisupport.cpp)；
[Qt5 RenderControl](https://github.com/qt/qtdeclarative/blob/v5.15.2/src/quick/items/qquickrendercontrol.cpp)；
[Qt6 RenderControl](https://github.com/qt/qtdeclarative/blob/v6.8.3/src/quick/items/qquickrendercontrol.cpp)。

**P3 总结：`PASSIVE_NATIVE_FRAME_REUSE=POSSIBLE_SOURCE_MECHANISM_NOT_TESTED`。** 相比 H1/G0/G1 的“是否 dirty”，它直接关注真实产出的 native frame；但 GPU→CPU readback/线程/Kit 依赖与零代码 surface 覆盖可能使它无法成为跨 Qt LTS 产品方案。

### 16.3 下一关仅 F-G3：无产品改动的单次被动原生帧读回证伪

两套既有 Windows Qt Kit，`tmp/` 独立自动退出 Qt-only QQuickView，每 Kit 最多一次。**只研究正常可见 Window 的原生渲染帧**，不实现 HyRemote Runtime/QtPrivate 生产代码、不连接 Viewer、不改 Qt SDK、不跑 CPU/多平台矩阵。

1. **G3-A 无自激**：静态阶段 2s，确认 observer 不发 `window->update()`、`grabToImage`、`grabWindow`、离屏重新渲染；在自然原生帧阶段最多 3 次被动读回，不能让静态阶段意外增加原生 render。
2. **G3-B 画面正确**：真实 QML `OpacityAnimator`（0.2→0.8，约 1.2s），从实际 native render output 最多取 3 张 CPU image，确认不同时刻有效红像素推进、本地动画正常、图像尺寸/格式/方向与原始窗口吻合；记录 native frame serial 与读回时间。Qt5 原生 OpenGL readback 若同步，用于研究负例，明示潜在 render-thread stall；Qt6 若用 QRhi，就在合法 command buffer/frame 时刻安排异步 `readBackTexture`，**不得**为了“成功”调用 `QRhi::finish()` 或阻塞 GUI/render thread。
3. **G3-C 无破坏和准确失败**：没有合法 GL FBO / QRhi swapchain buffer、API 不可用、readback 没完成、改变正常显示/引发循环、显著同步阻塞或 GPU/resource 错误，立即停止，`NO_GO_OR_UNKNOWN`。不许强制切后端、重编 Qt 或改 HyRemote。详细报告第一条具体错误、Qt Kit、默认图形 API/目标输出、线程、渲染次数、每张 image bytes/格式/时间。
4. 若两代 PASS，只表示 **bounded native frame readback 可行性**，不表示无 CPU 成本、任意 Surface/Popup/Generic/QPA 均安全，也**不触发产品实现授权**。之后必须把 ABI/部署/许可、readback queue/fence、资源生命周期、样本性能、四入口与隐私合成差异提交 Human 再批准。

**唯一批准事项：研究/设计继续；`HUMAN_PRODUCT_IMPLEMENTATION_APPROVED=NO`；现有 100ms 安全回退保留。**

```text
FG2=NO_GO_DIRTY_ONLY
F_G3=PASSIVE_NATIVE_FRAME_READBACK_NOT_RUN
QQUICKWINDOW_GRABWINDOW=NOT_PASSIVE
QT6_RHI_READBACK_INTERNAL_SAMPLE=BLOCKING
QT5_GL_READBACK=UNVERIFIED_POTENTIAL_STALL
HUMAN_RESEARCH_APPROVED=YES
HUMAN_PRODUCT_IMPLEMENTATION_APPROVED=NO
```


## 17. F-G3 双 Qt 原生帧被动读回验收与 F-G4 全帧容量门禁（2026-11-05）

**审批状态不变**：Human 只批准**技术可行性研究**，没有授权修改 HyRemote 产品捕获代码、QtPrivate 生产依赖、Qt SDK、发布或 G7。证据：[Local Owner F-G3 #609](https://github.com/skawu/HyRemote/issues/609#issuecomment-6647041056)。Qt5.15.2 / MSVC2019、Qt6.8.3 / MSVC2022 各一套独立自动退出 Qt-only QQuickView，默认 Windows 图形后端；云端仅核对 GitHub 的试验汇报，未直接读取本地 `tmp/609-fg3` 原始日志。

### 17.1 准确验收：两代 PASS，但不是同等产能证明

| 检查 | Qt5.15.2 | Qt6.8.3 |
| --- | --- | --- |
| native 渲染来源 | 当前桌面 OpenGL 后缓冲，`afterRendering`、交换前 | 默认 D3D11 / QRhi 当前 swapchain，合法 CB `resourceUpdate(readBackTexture(null,...))` |
| 静态 2s | `frames=1; afterRendering=1; reads=0` | `frames=1; afterRendering=1; submits=0` |
| 自然原生动画 | `frames/afterRendering=76/76` | `78/78` |
| 有效像素 | 3次 4×4 `GL_RGBA/UNSIGNED_BYTE`，R=`51,74,100`，`glGetError=0` | 3次完整 **1920×1061** RGBA8（`8,148,480` bytes），R=`51,74,100`；3/3 async completed，`8/22/22 ms`，无 `rhi->finish()` |
| 无异常 | 本测试未观察到 QT WARN / GL error / crash | 未观察到警告、未完成回调或 crash |
| 尚未证明 | **全帧读取时延、render-thread stall、帧率** | 持续 readback 队列、in-flight 背压、提交/完成版本、surface 生命周期与资源失效 |

**F-G3=`PASS_FOR_PASSIVE_BYTE_ACCESS_ON_TESTED_KITS`**。静态无额外帧，动画内 native `afterRendering==frameSwapped` 且探针未触发采集专用 `update/grab`，可合理支持“该探针未额外驱动场景绘制”的有限判断；不能从 3 次读回推出任意驱动、原生绘制帧率或 full-frame 性能零损失。**Qt5 4×4 与 Qt6 全帧不是对等性能样本**。Qt5 GL 当前读回 alpha=255，不能自动代表 `contentItem()->grabToImage` 的预乘 alpha / 窗口背景语义。Qt6 1920×1061 是受窗口边框/显示区域限制的物理 surface，不是 1920×1080 逻辑合同。

### 17.2 后续架构候选（只形成设计，不修改代码）

- **P3-native-frame-capability**：Qt Quick 目标仅在 native frame 已实际生成时，Qt5/Qt6 LTS-version adapter 发起有边界的 readback，完成后发布 owned/immutable `Rgba8888` frame 到现有 Shared Runtime。`CaptureSource` / Core pacing、不透明权限校验、水印/隐私处理、RFB/Native 传输以及四入口的现有支持定义**不得更改**。这需要显式定义 native frame tap 与 Core request-driven 模型如何对齐，否则 `requestFrame` 可能等不到 native 帧，或在 Viewer 新连接时没有有效初始化帧。
- **Bootstrap / static / reconnect**：Qt Quick 在静态时可能只渲染过**1 帧**，而 Viewer 可能晚于它连接。不能简单“只听未来 native 帧”导致 Viewer 无 ServerInit。新 Viewer / 断线重连 / window 显示与隐藏 / DPR/resize / 失效 / watermark/privacy 状态变更，必须允许**一次可信的按需完整抓图或安全 fallback**；成功获取并确认为有效后再可能进入 native passive 状态。未知类型一律退回当前 bounded `grabToImage` 路径。
- **异步状态与生命周期**：native frame `frameSerial`、`readbackRequestId`、`windowGeneration`、pixel size/format/DPR 必须跨完成回调保持关联；Qt6 GPU 结果不能被 premature free，失效/stop/resize 后的异步 callback 不得进入 Core。Qt5 同步 GL 读回若 stall 严重，须先证明可用 PBO + fence（不强制同步），否则不能宣布符合实时性。
- **捕获边界与安全**：native window/swapchain 包含的颜色/清屏/overlay 等不同于旧 Quick contentItem 结果；独立 Popup/native surface、不透明背景与透明控件、Y 轴方向、premult RGBA/BGRA、DPR 都需要保持现有 API 的语义。任何水印、隐私、权限不得被 raw swapchain 绕过。被动帧未提供可验证的完整覆盖时，不应启用新快路。
- **非消失的成本**：GPU→CPU readback、内存带宽、潜在 GPU fence/submit latency、RFB 压缩与对端交付仍存在；Qt6 8–22ms 仅为单次低密度 readback completion，未验证 60Hz。不能以 F-G3 宣称 #789 或 #609 已解决。

### 17.3 唯一下一研究门槛 F-G4 — full-frame 容量与干扰止损

**目的**：填上对产品性能结论影响最大的差异——Qt5 只测 4×4，而 Qt6 只测稀疏的整帧。继续复用本地 Qt-only `tmp/609-fg3`，不修改 Qt/HyRemote 源码，不连 Viewer、不开展平台矩阵。

每 Kit **一次**自退出自然 `OpacityAnimator` 连续动画：先 **2s baseline**（不读回），随后 **2s capture phase**（最多 **12 次**，以约 6Hz 做完整 window surface 的 readback；不得另外调用 `update/grabWindow/grabToImage` 或强制 graphics backend），原生帧持续，尽量保持同一窗口尺寸和场景。记录每相位的 native frame 计数和帧时间（中位/P95）、host process CPU 窗口；读回的 dimensions/format/total bytes/实际像素校验、提交与完成时间、完成数和最大同时在途数。Qt5 按合法 GL 状态全帧读取，实测 `glReadPixels` 调用耗时中位/P95 和首个 GL 错误；Qt6 继续有效 QRhi asynchronous callback，**不准 `rhi->finish()` 或主线程等待**，最大同时在途 **1**（超出直接跳过一次读回，不能无限排队）。
- **硬停线**：整帧数据无效、异步资源跨阶段失效、native frame 被读回反向唤醒、常规动画明显卡死/同步卡顿、GPU/Qt 错误、Qt5 阻塞读回导致明显不可接受的 frame delay，就在第一条可信异常处停止该 Kit；立即报告 `G4_STALL_OR_CORRECTNESS_NO_GO`，不得改用 Qt/驱动补丁或另换 Windows API 绕过。
- **判定**：仅当两代完整像素有效、容量/线程安全可解释、没有明显交互破坏，输出 `F_G4_CAPACITY_RESEARCH_PASS`；性能指标同时原样报告，**不预设 gain**，即使 PASS 仍须 Human 审批 QtPrivate 依赖、安全语义和先期产品实验。无法测到的写 NOT_MEASURED。Qt5 若严重 stall，本版同步 GL 方案 NO-GO，保留“受控研究异步 GL PBO”选项供 Human 选择，而**不是自动进入产品开发**。

### 17.4 实施前 Human 风险核对卡（当前待签字）

1. **High/Blocker**：零代码 Quick 的 native swapchain vs 原 `contentItem` 区域/alpha、跨窗口 Popup/native/Shader/视频与隐私水印链一致性（未证明）。
2. **High/Blocker**：Qt5 full-frame GL stall / 异步 PBO 可用性；Qt6 QRhi async 读回/资源寿命/背压和高频延迟（F-G4 待测）。
3. **High**：QtPrivate exact-Kit ABI、Qt5 Quick experimental 的支持声明、Qt5 GL/Qt6 QRhi 各平台 SDK 和 Qt 授权/发行方式。
4. **Blocker**：CaptureSource request/ready 关联、静态首帧/新 Viewer ServerInit、DPR/resize、stop/reconnect/target-lost 故障回退；未知支持能力必须 fail-closed。
5. **High**：没有真实 Viewer/ARM/RK3588 的新 P3 A/B；CPU/RSS/FPS/延迟的可实现 SLO 尚不能承诺。

**Human 已批准本研究，不等于批准下一项代码实施。所有 P3 产品 Runtime 改动前，须提交 F-G4 结论+风险清单+相对保守 fallback 的能力取舍，由 Human 明确授权；研究过程中发现反例即提前 STOP。**

```text
F_G3=PASS_QT5_SMALL_READ_QT6_FULL_ASYNC
F_G4=NOT_RUN_FULL_FRAME_CAPACITY
QT5_REALTIME_COST=UNKNOWN
QT6_ASYNC_SUSTAINABILITY=UNKNOWN
P3_PRODUCT_GO=NO
HUMAN_FEASIBILITY_RESEARCH=APPROVED
HUMAN_PRODUCT_IMPLEMENTATION=NOT_APPROVED
```
