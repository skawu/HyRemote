# #609 P3-S2 Qt6 native-frame integration — detailed design (design only)

```text
BASELINE=fc1bf73d53cee12194bb1cfae6bac673aceb4f63（S1R 代码基线；本文档提交在 573ca420 之后）
SCOPE=架构与接口设计；不新增/不修改/不执行测试，不实现任何原生帧发布
GATES=默认 OFF + 运行时显式 opt-in + 精确 Qt 6.8.3 + D3D11；原生像素不得进入 Core/RFB/Viewer
```

## 1. 现状事实（源码取证）

| 主题 | 事实 | 证据 |
| --- | --- | --- |
| 请求源 | 唯一生产调用方是 Core scheduler；`request.id = nextRequestId++`；composite target 会扇出到子采集源 | `src/core/src/session.cpp:224,516,532`；`src/runtime/src/automatic/composite_target.cpp:612` |
| 请求节奏 | Runtime 钉 60 fps；Core 另有 `maxInFlight=2`、拒绝退避 50 ms；RFB 客户端请求只触发 `delivery.request`，不产生采集请求 | `src/runtime/src/access_instance.cpp:1630`；`session.hpp:72-82`；`rfb_transport.cpp:1230-1245,1773-1776` |
| 现有采集 | 每次 `requestFrame` 都**强制新 grab**（Quick: `contentItem()->grabToImage(logicalSize)`；Widget: `QWidget::render`），因此“静态无新帧”在现有架构不存在 | `quick_target.cpp:388-398`；`widget_target.cpp:183-212` |
| 几何元数据 | `size = image 像素尺寸`（设备像素）；`devicePixelRatio = image 自报`；`alphaMode = Premultiplied`；无任何代码校验 DPR 与窗口尺寸的一致性 | `quick_target.cpp:272-274,341-347`；`src/core/src/frame.cpp:7-41` |
| 水印 | 装饰器要求 `Rgba8888` + 可读单平面；`devicePixelRatio` 用于字号；装饰失败即断开全部 viewer（fail-closed） | `watermark_frame_decorator.cpp:17-61`；`rfb_transport.cpp:621-660` |
| 输入 | RFB 输入视口 DPR **硬编码 1.0**，坐标以 `geometry.size` 设备像素为准 | `rfb_transport.cpp:1596` |
| 首帧/重连 | ServerInit 用 `recordInitialFramebuffer(logical, dpr)` 预置尺寸；无帧时回退该尺寸；静态探针 100 ms | `access_instance.cpp:1393-1419`；`rfb_transport.cpp:1294-1305`；`viewer_capture_demand.hpp:17-18,95-104` |

## 2. 设计目标与非目标

**目标**：用 Qt 原生渲染帧（D3D11 swapchain 读回，已在 S1/S1R 证明可安全异步完成）补充现有 `grabToImage` 采集，降低每帧整帧拷贝；保持所有既有语义（几何、Alpha、DPR、水印、授权、Viewer 行为）不变。

**非目标**：不替代静态首帧/重连保障；不扩展 Popup 采集；不改变输入坐标模型；不在本阶段发布原生像素。

## 3. 核心契约：几何/Alpha/DPR（P3-R1）

### 3.1 差异清单与处置

| 维度 | `contentItem()->grabToImage` | 原生 swapchain | 设计处置 |
| --- | --- | --- | --- |
| 像素尺寸 | `logicalSize × DPR`，由 grab 自报 DPR | `swapchain->surfacePixelSize()` | 原生仅在“`surfacePixelSize == grab 尺寸` 且 `DPR == surface.w / window.width()`”时可用，否则回退 |
| DPR 元数据 | 来自 QImage | 需推导，无现成字段 | 由 `surfacePixelSize / window logical size` 推导；要求与 `window->devicePixelRatio()` 一致（容差 1/256） |
| Alpha | 强制 `RGBA8888_Premultiplied`；未覆盖区域 alpha=0 | 交换链通常不透明，未覆盖区域为 clear color，alpha≈1 | 仅当窗口格式 `alphaBufferSize()==0`（不透明窗口）时允许原生路径；否则回退 |
| 画面范围 | contentItem 子树；不含窗口级 clear | 整张窗口 surface | 不透明窗口下两者对“用户所见”一致；仍以像素等价测试为准（T1） |
| Popup | 不含（独立 QQuickWindow） | 不含（同） | **等价**：不新增也不删减 Popup 支持；显式记录 |
| 缩放/系统 DPR 变化 | 随 grab 结果 | surface 变化，需代际 | 任何尺寸或 DPR 变化 → 代际自增，旧帧作废 |

### 3.2 冻结的帧契约（原生路径必须完全等于既有 RemoteFrame 语义）

```text
pixelFormat = Rgba8888
alphaMode   = Premultiplied（不透明窗口下 alpha 全 1，与 grab 的预乘结果一致）
geometry.size = surfacePixelSize（设备像素，uint32）
geometry.devicePixelRatio = surfacePixelSize.width / window.width()  （校验一致）
geometry.planeCount = 1；stride = width*4（读回数据本身保证）
damage = fullFrame（与现有 Quick 采集一致，逐帧 diff 仍由 transport 承担）
```

任一不满足 → **不发布、计数、回退到 grab 路径**。

## 4. 请求模型衔接（P3-R2）

### 4.1 状态机（每个 QuickCaptureSource 一份）

```text
pendingRequest: { id, requestTime } | none        // 仅保留最新请求
surfaceEpoch:   { generation, w, h, dpr }         // resize/DPR/失效即自增
nativeEligible: bool                              // 编译选项 && 运行时 env && D3D11 && 不透明窗口 && 精确 Kit
lastNativeFrame: { epoch, capturedAt, storage }   // 有界缓存（最多 1 帧，可选）
```

规则：

1. `requestFrame()`（scheduler 线程）→ 投递到 GUI 线程，**覆盖** pending，旧请求计 `superseded`。
2. 原生完成回调（渲染线程）→ 投递到 GUI 线程决策：请求存在、epoch 等于当前、`capturedAt ≥ requestTime − freshness` 才可发布；否则丢弃并计 `stale`。
3. 发布时 `frame.requestId = pending.id`，`timing.ptsSource=Completion`，进入既有 `onFrame`；随后清空 pending。
4. `requestFrame` 后超过 `nativeFallbackDeadline`（建议 120 ms）仍无可用原生帧 → **直接走现有 grab 路径**，不做强制 render。
5. 无 pending 的原生帧一律丢弃（不缓存发布），避免“无请求也产帧”。

### 4.2 为什么不做“按请求强制 render”

- `window->update()` / `contentItem->update()` 会产生额外渲染，与“被动采集”原则冲突，且改变 VSync 节奏；
- 静态场景下 grab 已经确定性地满足请求（首帧/重连/静态探针），无需再引入第三条路径；
- 强制 render 与 Qt 渲染线程/动画器交互复杂，收益未证明。

## 5. 静态首帧、重连与无自然渲染（P3-R5）

- **首帧与重连始终走 grab**：不依赖原生帧；`recordInitialFramebuffer`、`AwaitInitialFrame`、静态探针语义完全不变。
- 原生帧只在“客户端已连接且确实有渲染活动”时补充；连接后的第一次成功原生帧可替换第二次及以后的 grab（第一次仍由 grab 完成）。
- 若 viewer 静默、窗口最小化/隐藏、无动画：无原生帧 → 每个请求由 grab 兜底（现有 60 fps/探针节奏不变）。
- 结论：原生路径是**加速器**，不是唯一来源；任何不确定状态都可退回，不引入“无帧可发”。

## 6. generation / requestId / 异步 / stop / resize / 过期帧

| 事件 | 处理 |
| --- | --- |
| resize / DPR 变化 | `surfaceEpoch.generation++`；未完成读回标记过期；后续请求先 grab 一帧确认新几何 |
| stop | 清 pending、断开 `afterRendering`、停止采样；在途读回由 S1R 证明的 RHI 销毁路径完成并丢弃；不再投递任何帧 |
| 窗口销毁 / 失效 | 同 stop；`QQuickWindow` 生命周期守卫已有（shadow 用 context 连接） |
| 过期帧 | `epoch != current` 或 `capturedAt < request.requestTime` 或 `requestId` 非最新 → 丢弃并计数，不发布 |
| 连续请求 | 只保留最新；原生帧按完成顺序最多 1 个待决策，不排队累积 |
| 请求超时 | 120 ms 后 grab 兜底；不重复请求原生 |
| 时间戳 | `capturedAt` 用渲染完成时刻（steady clock），仅用于新鲜度；`ptsSource=Completion` 与现有语义一致 |

## 7. 水印、隐私、授权与回退（P3-R6）

- 原生帧构造成与 grab **完全相同**的 `RemoteFrame`（storage + geometry + alphaMode），进入既有 `enqueueFrame → watermark decorator → RFB` 链路，**不新增旁路**。
- 水印要求 `Rgba8888`/可读平面/stride 覆盖，原生路径已满足；装饰失败仍按现有 `failDecoration()` 断开全部 viewer，绝不发未装饰帧。
- 授权与传输阶段门控不变；Shadow/原生路径的编译选项默认 OFF，运行期需显式 env；`nativeFrameProductionAllowed` 在获得单独批准前恒为 false。
- 回退顺序固定：`原生帧（同 epoch、新鲜）` → `grab` → `TemporarilyUnavailable`（保持现有重试与 2 s 看门狗）。

## 8. 代码落点（未来实现，不在本轮提交）

```text
quick_target.cpp
  QuickCaptureSource::requestFrame()  ── 记录 pending / 超时兜底
  publishNativeFrame()                ── 校验 epoch/geometry/alpha → RemoteFrame → onFrame
  grab 路径                            ── 保持原样，作为唯一回退
quick_native_frame_shadow.cpp          ── 读回计数器扩展为“可交付候选帧”（仍在 gate 内）
新增 detail/native_frame_epoch.hpp     ── epoch 与几何契约校验（纯值类型，可单测）
```

## 9. 架构分歧（交云端裁决）

1. **原生帧定位**：仅作加速器（推荐）vs 逐步替代 grab。替代会牵动首帧/重连/Popup/Alpha 全部语义，不建议在 S2 处理。
2. **请求绑定**：latest-frame + 超时兜底（推荐）vs 按请求强制 render。后者引入额外渲染，需独立授权。
3. **Alpha 策略**：仅不透明窗口启用（推荐）vs 允许透明窗口并要求 clear-color 补偿（高风险，需专门证据）。
4. **DPR 来源**：由 surface/窗口尺寸推导并校验（推荐）vs 直接信任 `window->devicePixelRatio()`。
5. **Popup**：维持“仅目标窗口、不含 Popup”的既有语义（推荐）vs 扩展为合成采集（超出 S2）。
6. **集成位置**：QuickCaptureSource 内部（推荐，生命周期单一）vs 独立 CaptureSource。

## 10. 剩余风险

- P3-R1 等价性未实测：DPR 1.25/1.5、透明窗口、clear-color 区域、Popup 覆盖差异均需证据。
- `geometry.devicePixelRatio` 变化会影响水印字号；推导值若与 grab 不一致会造成可见差异。
- 输入视口 DPR 硬编码 1.0，几何必须与 grab 完全一致，否则远端坐标漂移。
- Qt 私有 ABI/后端限制：仅 D3D11、精确 6.8.3；设备丢失未覆盖。
- 性能收益未证明；S2 不承诺任何 CPU 降幅。
- 首帧/重连仍依赖 grab，因此“完全去掉拷贝”不可达；优化上限受静态与重连路径限制。

## 11. 候选测试计划（**未执行，待 Human 事前确认**）

### T1 — 几何/Alpha/DPR 等价矩阵

1. 名称/目标：`native-vs-grab-geometry-equivalence`；同窗口状态下比较原生帧与 grab 帧的尺寸、DPR、Alpha、采样像素。
2. 真实风险：几何/DPR/Alpha 任一不一致会导致水印字号、输入坐标或可见内容偏差。
3. 现有证据不足：S1/S1R 只验证读回生命周期与字节自洽，从未与 grab 帧逐像素对照；F-G1/G2 场景均为不透明全铺满窗口。
4. 时机/环境/范围/上限：设计评审通过、实现落地后；Qt6.8.3 ON + 离屏/本地窗口；覆盖 DPR 1.0 / 1.25 / 1.5、不透明/透明窗口、一次 resize；**最多 4 次运行**。
5. 判据/后果/成本：PASS = 尺寸与 DPR 精确相等、Alpha 语义一致、抽样像素差 ≤1 LSB；FAIL = 任一项不一致（则原生路径不得启用）。不执行则 P3-R1 保持 UNKNOWN。成本 ≈ 5 分钟。

### T2 — 请求绑定与静态/重连兜底

1. 名称/目标：`native-request-binding-static-fallback`；验证无 pending 不产帧、静态无原生帧时 grab 兜底、重连首帧正确。
2. 真实风险：无请求发布或静态无帧会造成黑屏/旧帧；requestId 错配会污染时序统计。
3. 现有证据不足：现有抓帧每次强制 grab，不存在“原生帧可用性”分支。
4. 时机/环境/范围/上限：实现后；单 Viewer + 静态场景 + 一次断线重连；**最多 3 次运行**。
5. 判据/后果/成本：PASS = 首帧/重连均成功、无 pending 时零发布、超时后 grab 生效；FAIL = 黑屏/旧帧/无兜底。不执行则该分支不得启用。成本 ≈ 3 分钟。

### T3 — 水印/授权 fail-closed

1. 名称/目标：`native-frame-watermark-failclosed`；原生帧必须经过水印；装饰失败仍断开 viewer。
2. 真实风险：原生路径可能绕过装饰，导致未加水印画面外泄。
3. 现有证据不足：现有 fail-closed 证据只针对 grab 帧链路。
4. 时机/环境/范围/上限：实现后；开启水印策略 + 人为装饰失败注入（仅测试夹具）；**最多 2 次运行**。
5. 判据/后果/成本：PASS = 原生帧带水印、失败即断连且无未装饰帧发出；FAIL = 任一未装饰帧到达 Viewer（立即 STOP）。不执行则禁止进入发布评估。成本 ≈ 2 分钟。

### T4 — 请求/epoch/stop/resize 生命周期

1. 名称/目标：`native-request-epoch-lifecycle`；pending 情况下的 stop、resize、过期帧丢弃。
2. 真实风险：过期帧以新 requestId 发布会造成画面跳变/错帧。
3. 现有证据不足：S1R 只覆盖读回资源生命周期，不覆盖请求与 epoch 映射。
4. 时机/环境/范围/上限：实现后；与 S1R 相同的 stop/resize 夹具；**最多 3 次运行**。
5. 判据/后果/成本：PASS = 过期帧 100 % 丢弃且计数正确、stop 后零发布；FAIL = 任何过期帧发布。成本 ≈ 3 分钟。

> 以上四项均未执行；在 Human 逐项确认前不提交任何自动触发测试的代码或 CI 变更。

## 12. 交回云端 Owner 的问题

1. 是否接受“原生帧仅作加速器、grab 永远保留”的总体定位？
2. 透明窗口是否直接禁用原生路径（推荐），还是要求 clear-color 补偿设计？
3. Popup 维持既有“不含”语义是否可接受（推荐），还是必须在本阶段解决？
4. 请求超时 120 ms 与 60 fps pacing 的组合是否符合产品时序预期？
5. T1–T4 是否作为 S2 的准入证据集；是否允许缩减（例如仅 T1+T3）？
