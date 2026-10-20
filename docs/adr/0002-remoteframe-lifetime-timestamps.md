# ADR-0002：RemoteFrame 所有权、Damage 与时间戳契约

> 语言 / Language：**中文** ｜ [English](../en/adr/0002-remoteframe-lifetime-timestamps.md)

状态：**ARCH-01（#5）提议**

## 证据

#3/PR #15 和 #16/PR #19 确立了 frame 模型必须显式编码的四项要求：

1. 把 borrowed pointer 发布到生产者会复用的存储不安全；
2. `QImage`/snapshot 语义可以安全，但可能触发 copy-on-write 或分配成本；
3. damage 可能已知、整帧或不可用，取决于采集 family；
4. **采集请求时间不是内容 PTS**：流水线化的 `grabToImage()` 请求可能由更晚的渲染完成，携带更新的场景状态。

## 决策

`RemoteFrame` 是**不可变元数据值 + 对 frame 不可变存储的共享所有权**。

概念上：

```text
RemoteFrame
 ├─ FrameId                   单调完成 id
 ├─ FrameGeometry             width/height/format/planes
 ├─ shared_ptr<FrameStorage>  生命周期锚点
 ├─ FrameTiming               内容 PTS + 诊断
 ├─ Damage                    Unknown | Full | Regions
 └─ metadata/capabilities     仅后端无关标志
```

`RemoteFrame` 可以在队列/消费者之间复制；底层存储在最后一个所有权引用释放前保持有效。

## 存储契约

`FrameStorage` 是 Core 生命周期锚点。

必需语义：

- 已发布 frame 引用的存储必须在 frame 生命周期内可读/可用；
- 生产者自有可复用内存不能在没有生命周期/回收对象防止覆盖时发布；
- 存储实现拥有回收回调和释放 buffer 所需的线程亲和；
- Core 可以在任意 Core/传输线程销毁最后一个 `shared_ptr`；资源必须在其他线程归还的存储实现要自己 marshal；
- 消费者不得在 `RemoteFrame`/`FrameStorage` 生命周期外保留裸指针/handle。

### 初始存储形式

1. **CPU 可读存储**：存储对象存活时暴露一个或多个只读映射 plane。
2. **外部/平台存储**：对通用 Core 不透明；暴露通用扩展边界，可选提供 CPU 映射。

精确 DMA-BUF/GBM/native-handle descriptor 有意推迟到 #17。#5 只冻结“外部存储是一等存储类型”，不冻结 Linux 专用形状。

## 不可变规则

一旦提交给 Core，`RemoteFrame` 表示的像素对所有消费者不可变。

生产者可以通过以下方式实现：

- 转移所有权；
- 发布深度不可变 snapshot；
- 从池中取 buffer，并在最后一个引用释放后回收；
- 发布外部存储，其复用由存储/fence/回收契约保护。

## 几何与像素描述

frame 独立描述逻辑图像尺寸和存储布局：

- width/height；
- pixel format 标识；
- plane 数；
- CPU 映射存在时的每 plane offset/stride/size；
- 采集后端无法低成本规范化时的 orientation/transform。

v0.1 CPU baseline 应规范化到少量 packed RGB(A) 格式。YUV/外部格式仍可作为优化路径扩展。

## Damage 模型

Damage 是三态：

```text
Unknown     后端无法提供可信区域几何
FullFrame   后端声明整帧变化
Regions     零个或多个 frame/root 坐标矩形
```

规则：

- `Unknown` 不等于空区域；
- 传输必须对 `Unknown` 回退到整帧，除非有独立 damage 检测；
- Widgets paint region 只有在映射/裁剪到共享 root 坐标系后才可使用；
- 观察到的 QWidget paint region 是输入信号，不自动成为协议级保证；
- 公共 Qt Quick async capture 可以合法报告 `Unknown`。

## 时间戳契约

### 一个内容 PTS

每个 frame 在 HyRemote 单调时间域携带一个 **内容 PTS**，以及描述来源的 `PtsSource`。

允许的来源质量，从低到高：

1. `Completion`：采集完成/`ready()` 时间戳；
2. `Render`：后端观察到的渲染时间戳；
3. `Presentation`：后端观察到的呈现/scanout 时间戳。

后端以后可以增加更精确的来源，但消费者比较规范化 PTS，而不是后端原生时钟。

### 请求时间仅用于诊断

采集请求时间永远不会自动提升为内容 PTS。

它可以单独保留为可选诊断，用于计算 request-to-completion 延迟。

对于公共 `QQuickItem::grabToImage()` v0.1 候选，completion/`ready()` 时间是安全 fallback PTS，因为请求可能由更晚的渲染完成，携带更新的场景状态。

### 完成时间

可用时也记录 `completionTime`。当 `PtsSource::Completion` 时，`pts == completionTime`。更低层后端可以提供更早/更准确的 render/presentation PTS，同时记录 completion time 用于延迟分析。

## Frame 身份

`FrameId` 按 Core 接受/完成顺序分配，独立于采集请求序号。

请求序号可以保留在诊断中，但不得作为视觉年龄的代理。

## 消费者新鲜度

传输/丢弃决策应使用：

- 内容 PTS / wall-clock 年龄；
- 队列深度；
- 传输状态；
- 后端提供的可选重复/内容提示。

不得从请求索引算术推断视觉新鲜度。

## 多客户端后果

传输可以把一个 `RemoteFrame` 扇出到多个客户端。共享存储所有权因此是有意的：只有所有下游用户释放后，buffer 才回收。

传输实现仍必须限制每客户端队列；Core 的 capture-to-transport mailbox 只保护应用/采集侧不受慢传输 ingestion 影响。

## 失败行为

不产生 frame 的采集请求（例如隐藏窗口的 `grabToImage()` 返回空）不会创建占位 `RemoteFrame`。它产生采集事件/错误，允许采集策略重试或选择 fallback。

## 结果

- CPU 和未来 DMA-BUF 路径共享一个生命周期模型；
- 慢客户端可以安全丢弃 frame 对象，不需要生产者专用清理知识；
- 传输/encoder 模块可以异步保留 frame；
- 内容 PTS 在公共和未来优化采集后端之间保持有意义。
