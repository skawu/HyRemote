# Qt Quick 采集路线：独立变化信号与产品性能验收判定（设计研究，2026-11-04）

**性质：** 产品驱动的技术可行性研究；不构成代码改动、性能达标声明、G7 架构变更或新的发布门禁。  
**负责 Issue：** [#609](https://github.com/skawu/HyRemote/issues/609)，现场结果 [#789](https://github.com/skawu/HyRemote/issues/789)，性能记录 [#374](https://github.com/skawu/HyRemote/issues/374)，独立局部优化 PR [#823](https://github.com/skawu/HyRemote/pull/823)。  
**基线：** V14 产品设计、V7 正式架构及 G7；代码观察 `develop@fda998bd94a8e677635b1f8b76aaac0ae467e032`。本设计不能替代精确 SHA 的目标设备验证。

## 1. 先定义产品成功，而非优先保护旧实现

对一台运行 Qt Quick / Widgets 业务应用的现场设备，远程查看应做到：

1. 无画面变化时不持续占满一个 CPU 核；不影响宿主的正常现场操作与渲染。**当前严重未解决案例：** RK3588/Buildroot/Qt 6.8.3/EGLFS，旧候选 `6a4b7b37` 的静态单 Viewer CPU 由 15.6% 上升至 117.2%（单核等效，+101.6pp），见 #789。不能用 Windows x86 的百分比推断 ARM 修复。
2. 局部更新、60Hz 源动画及真正持续全屏变化时，客户端呈现**最新有效状态**，没有长时间冻结、逐块刷入、过时帧积压；不能靠限制画面刷新率“优化”CPU。
3. 本地输入、DPI、隐私/水印、重连、双 Viewer 与四种接入方式继续满足产品契约。
4. 以准确来源和设备范围报告实际 CPU、帧新鲜度、现场响应及错误；无目标板复测即 `TARGET_NOT_RUN`。候选定量数字仍为假设而非 V14/V7 签收 SLO。

这些目标可支持保留、改造或替换当前采集技术路线，现有实现不能倒过来降低验收目标。

## 2. 已证明事实、重要限制与否定性证据

| 项目 | 可用证据 | 不可作出的推断 |
| --- | --- | --- |
| Generic 静态小改动 | #374 / #609：`fda998bd`→`6669bef7` 单次/有限配对，CPU/捕获 12.56ms→8.39–8.61ms（约 -31%～33%） | 总进程 CPU、内存或动画流畅度已经改善 |
| 旧动态矩阵 | 16 格仅 1 格完成，且另一个观测客户端改变了 RFB 编码负载 | 这是有效的单 Viewer 对照 |
| 当前调度 | `src/runtime/src/detail/viewer_capture_demand.hpp`：三次相同后按不超过约 100ms 的探测间隔继续捕获；新 Viewer、远程输入及像素变化恢复活跃路径 | 静止连接能够完全取消 Qt Quick `grabToImage` 调用 |
| Qt Quick 采集 | `src/runtime/src/quick/quick_target.cpp`：`QQuickItem::grabToImage`，转换到 RGBA 时可能分配新缓冲并复制，形成 `Damage::fullFrame()` | 80.6% 未归因的 Windows CPU 残差全部来自 GPU/驱动 |
| RFB 发帧 | `rfb_delivery_state.hpp::changedRfbTiles()` 对已捕获像素执行差异检测；Windows 静态单 Viewer 可不编码/发送 | 所有静态 CPU 都在网络编码；旧 RK3588 帧条带现象的准确成因已经证实 |
| Qt 官方 Qt VNC Server | 提供 dirty-map；持续大面积变化时关闭脏检测有时有利（官方文档） | 商业模块已在我们的设备上跑赢 HyRemote |
| `uwerat/vnc-eglfs` | 公开 GPU 抓帧路线；`src/RfbEncoder.cpp` 当前是 Qt `QImageWriter` JPEG | 已实现 H.264/VA-API 硬编，或有可比的 1080p 测量结论 |

注意：只允许将不同硬件、不同 SHA 和不同采集/交互场景的结果并列展示，不得归一后拼成伪同场排名。

## 3. 核心判断：Qt Quick 渲染信号是否独立于远程抓图？

**技术命题 H1：** 宿主的正常屏幕渲染发生了变化时，能否从真实 `QQuickWindow` 得到**不由 HyRemote 自身 offscreen grab 引起**且不漏掉用户可见变化的通知？若不能证实，不能把“没有该通知”当成“画面未变化”。

Qt 6.8 的公开 API 有 `afterAnimating()`（GUI 线程、动画阶段）、`afterFrameEnd()`（场景图帧提交完成、渲染线程）、`frameSwapped()` 及 `beforeRendering()` 等。文档明确渲染信号跨线程，且某些信号反映**一帧正在/已经渲染**，不是“像素内容已变化”的保证；`grabToImage` 可能触发额外渲染。因此只把这些 API 作为**候选观察点**，而不是默认有效的脏区 oracle。

- 若采集调用自身导致 `afterFrameEnd/frameSwapped/beforeRendering` 增长，使用其作为变化请求会形成“抓图触发信号→信号触发抓图”的闭环。
- 若某类局部变更、动画、窗口遮挡/恢复、EGLFS 变更并未触发所选信号，则“无信号时跳过抓图”会冻结远端画面。
- `QQuickItem::update()` / item 几何变化不能保证覆盖整个 QML/原生 scenegraph 与外部 OpenGL 绘制；只关注此类事件无法支持完整产品承诺。
- 多渲染线程/渲染后端下不得从 render-signal 回调直接访问不具备同步保证的 GUI/捕获状态。仅记录线程安全的计数/单调时间，向 GUI 线程安全转发；不得阻塞渲染线程等待网络/锁。

**阶段性结论：** 公共 Qt 信号可用于小型可行性实验，**目前没有证据证明其完备与独立性**。本轮不授权仅凭它们关闭静态后备探测、直接引用渲染纹理、替换跨平台采集实现或修改公共 SDK API。

## 4. 最小可证伪实验，而非新测试平台

### 4.1 当前唯一需本地实际运行的优先实验：#823 Generic 60Hz A/B

沿用已存在的 1920×1080 Widgets Generic harness 的 `anim`（60Hz 是**源刺激目标**，不是 Viewer 实际 FPS），以同一 Windows/Qt6.8.3/MSVC2022 Release、同一 Kit、同配置探针 ON：

- **before:** `develop@fda998bd94a8e677635b1f8b76aaac0ae467e032`；
- **after:** `feature/609-generic-single-fullscreen-fastpath@6669bef7e8dda27a787620d995fd2f36f48e28e8`。
- 每个 SHA **独立一次运行**（10 秒暖机+40 秒样本），确保结果已落盘/进程确实退出再执行另一个。只连 **1 个** RealVNC 7.15.1；采样期间不能有另一个持续请求全帧的 Observer。
- 记录相同 wall-time 下的 Host 进程 CPU 秒/单核百分点、CPU/实际捕获、实际捕获次数/秒、private/WS、源动画逻辑状态及 Viewer 变化是否连续可见；缺可信 Viewer FPS/帧龄仪器就明确 `NOT_MEASURED`，不可用 10–15Hz 轮询冒称 60Hz 真实帧率。
- 出现 Viewer 冻结、现场操作退化、任何正确性回归或 Host CPU 没有足够证据改善时，PR #823 继续 `UNMERGED`，优先转向根因，不再反复补满 16 格。对低置信的小数值差异可做**一组**重复，禁止无限循环验证。
- 既有单元测试/CI 结果可以复用；**不因这项试验重复执行完整 G6/L2、例程、第三方开源软件构建**。

状态：`DYNAMIC_GENERIC_AB=NOT_RUN`。本设计文件不是给已离线本地电脑的执行凭证；GitHub Issue 评论不能冒充自动发单。

### 4.2 随后最小的 Qt Quick H1 判别实验（只在 Generic 实验处理后）

直接复用 Qt Quick 1080p harness；只在**独立诊断构建**记录计数和时间，产品源码暂不改策略。最多三个顺序运行的短场景，每次单独启动、及时输出、可中止：

| 场景 | 目的 | 必须同时记录 |
| --- | --- | --- |
| Q0：静态、无 Viewer、不调用主动 grab | 观察正常 scenegraph 无变化时各候选通知是否自发出现 | `afterAnimating`/`afterFrameEnd`/`frameSwapped` 次数、真实本地变化次数 |
| Q1：静态、1 Viewer、原 `grabToImage` 路线 | 识别 grab 自激信号和静态多余帧成本 | 相同通知计数、抓图次数、Host CPU、Viewer 是否收到实际变化 |
| Q2：真正持续的 60Hz Qt Quick 动画、1 Viewer | 判断源变更能否独立触发且无漏检 | 实际源帧/变化计数、通知与抓图计数、Host CPU、Viewer 可见变化 |

**H1 可以继续探索的必要条件：** Q1 的通知能够与主动 grab 因果区分；Q2 的可见源变化未发生持续漏报；窗口重建/resize/visibility/远程输入时能强制失效缓存；并且所需同步在 Windows/Qt6 及未来 EGLFS 上有可验证路径。未满足**任一项**，不要使用“无信号→无需捕获”的优化。Q0–Q2 没跑过则 `H1=UNVERIFIED`，不称 PASS。

此实验不增加新的公开参数、不跑多 Kit 组合、不创立永久 CI 框架。后续若证据证明需要跨平台后端，再单独设计。

## 5. 备选技术路线与选择条件

| 路线 | 产品价值 | 风险/缺口 | 当前判定 |
| --- | --- | --- | --- |
| A：现有 CPU `grabToImage` + RFB 差异反馈（现状） | 兼容范围已部分建立，最小迁移风险；恢复动态反馈不依赖新增 Qt API | 100ms 静态周期仍有完整 Quick 抓图/读回成本，RK3588 用户问题未解决 | **暂保留维护路线，但不接受其已达标** |
| B：可信宿主 scenegraph 变化事件 + 受控缓存/后备探测 | 可能在静态或低变化 UI 去除多余 GPU→CPU 读回 | H1 尚未成立；抓图自激、漏变化、跨线程及 EGLFS 语义风险 | **优先做最小 H1 可行性验证；不编码成正式策略** |
| C：面向目标图形平台的帧来源/读回后端（Qt 官方 VNC Server/vnc-eglfs 技术机制作参考） | 有机会绕开频繁 `grabToImage` 路径并改善连续运动成本 | Qt5/Qt6、RHI、DPR、EGLFS/Windows、服务集成与许可证有较大差异，不能照搬实现 | **开放候选；只有 B 不能满足产品目标或目标设备仍严重退化时进入详细设计** |
| D：压缩、RFB 比较策略 | 在大面积/高频动态时可能改善 Viewer 刷新与带宽 | 静态单 Viewer Windows 编码为 0，不能优先解释空载采集 CPU | **需动态 wire/Viewer 证据才能变更** |

任何技术路线必须保留 V7 规定的 Core/Shared Runtime/四接入、RFB/Native 互不依赖的抽象、Target 与 Session 授权/隐私、现场优先和资源上界。若路线 C 需要突破冻结边界，先出具相应产品收益、迁移/回退与风险对照，提交 Human 架构变更裁决；纯内部可替换采集实现不自动等同于 G7 变更。

## 6. 运行与交付状态、责任边界

- **云端 Owner（现在可执行）：** 完成源码与 Qt API 研究，审查方案，维护 #609/#374 证据状态，形成可执行的单个最小实验；不因旧 CI 全绿宣称产品性能 PASS。
- **本地 Owner（需实际活动 Windows 工作者）：** 按 4.1 运行两个独立短例，核对实际 Viewer，分别记录结果。任何实测记录需准确报 SHA、Kit、Viewer、观测端数和失败原因。未得到明确 Worker ACK 不能宣称已下发或已经执行。
- **工程实现：** 仅在 4.1 通过现有审查决策后考虑合并 #823；H1 未验证前不编写更大 Quick 优化；若 H1 不成立，直接比较新 FrameSource 架构选项，不靠多轮微调掩盖产品失效。
- **目标设备：** RK3588/EGLFS 是独立的必须证据层。现无项目自有、同源对照结果；#789 / #567 不关闭，也不要求最初的反馈用户代替项目测试。
- **Human：** 只有新技术方案要求改变 G7 已冻结的实质架构/安全边界时才需要裁决；日常任务传递、代码调查、短实验组织由 Agent 负责。

### 阶段输出判据

`DESIGN_RESEARCH=DONE`；`GENERIC_DYNAMIC_AB=NOT_RUN`；`QT_QUICK_SIGNAL_INDEPENDENCE=UNVERIFIED`；`WINDOWS_USER_PERF_CLAIM=NOT_QUALIFIED`；`RK3588_USER_OUTCOME=OPEN`；`RELEASE_V035=UNCHANGED`。

## 7. 事实来源

- 现有源码：`src/runtime/src/detail/viewer_capture_demand.hpp`；`src/runtime/src/quick/quick_target.cpp`；`src/runtime/src/transport/rfb_delivery_state.hpp`（`develop@fda998bd`）。
- [#609 既有真实 A/B 与实验纠偏](https://github.com/skawu/HyRemote/issues/609)；[#374 数据](https://github.com/skawu/HyRemote/issues/374)；[#789 RK3588 实测问题](https://github.com/skawu/HyRemote/issues/789)。
- [Qt 6.8 QQuickWindow 线程、动画与渲染信号文档](https://doc.qt.io/qt-6.8/qquickwindow.html)；[Qt Quick Scenegraph 渲染流程](https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph.html)。
- [Qt VNC Server 文档](https://doc.qt.io/QtVNCServer/index.html)；[vnc-eglfs 源码与许可](https://github.com/uwerat/vnc-eglfs)。
- [测试经济性规范](../internal/test-strategy.md)，G0/G1/G2 精简及发布前真实环境 G6 按风险运行，不重建例程测试体系。
