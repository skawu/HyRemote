# ADR-0003：线程、调度与背压

> 语言 / Language：**中文** ｜ [English](../en/adr/0003-threading-backpressure.md)

状态：**ARCH-01（#5）提议**

## 背景

HyRemote 不能让慢远程客户端造成本地 UI/渲染卡顿。宿主证据还表明，采集 API 有线程亲和，公共 Quick async capture 如果调用方不设上界，可能累积 in-flight 工作。

## 决策

HyRemote 分离 **采集调度**、**完成帧缓冲**、**传输分发**和**传输 runtime**。

```text
Qt GUI / target 线程
      │ request / completion adapter
      ▼
CaptureSource
      │ RemoteFrame completion
      ▼
Core completed-frame mailbox（有界）
      │
      ▼
Core dispatch worker
      │ enqueue / post
      ▼
Transport runtime 线程/事件循环
      │
      ├─ client A queues/codecs
      ├─ client B queues/codecs
      └─ input callbacks
                │
                ▼
        Input adapter -> Qt GUI 线程
```

## 线程归属

### 应用 / Qt GUI 线程

拥有 UI 对象生命周期，以及 Qt 要求在该线程运行的采集操作。

Widgets/Quick 适配器负责把一次性采集请求 marshal 到正确 Qt 线程。

GUI 线程不得在 Core 交接过程中执行网络 I/O、客户端扇出、压缩或硬件编码。

### Qt scene graph / render 线程

由 Qt 拥有。HyRemote 公共 baseline 不假设直接控制它。

未来优化后端只有在相关 Qt 契约允许时才可在该线程执行回调。此类后端必须快速交出稳定存储对象，不得等待网络消费者。

### 采集回调线程

`CaptureSource` 可以在后端定义的线程完成。Core 的 frame-ready 入口线程安全，只做有界簿记/入队。

任何采集后端都不得在帧生产过程中同步调用传输。

### Core dispatch worker

消费 completed-frame mailbox，把帧传给传输适配器。传输交接本身是**有界/非阻塞 enqueue 契约**：dispatch worker 不得在 `Transport::enqueueFrame()` 内执行网络 I/O、压缩、客户端扇出或无界等待。

这是确定性关闭和响应性的要求。Core mailbox 保护采集侧不受慢传输 runtime 影响；传输自己的有界 runtime/client 队列保护 dispatch worker 不受网络 peer 影响。

### 传输 runtime

由传输实现拥有。对 NeatVNC 而言是一个隔离的 AML/NeatVNC runtime；Core 和采集模块从不直接调用 AML。

传输实现**必须**通过向自己的 runtime post/enqueue 来实现 Core 交接，且有界执行时间。内部队列满时，必须应用显式本地丢弃/拒绝策略，不能无限阻塞 Core dispatch worker。

### 输入交付

远程输入源自传输 runtime 线程。传输规范化协议事件，然后 input adapter 将其 marshal 到目标要求的线程（通常是 Qt GUI 线程）。`InputSink::post()` 遵循同一规则：调度/marshal 工作，不得让传输 runtime 等待同步 GUI 执行。

## 采集调度

Core 把采集视为异步一次性工作：

```text
requestFrame(requestDiagnostic)
        ... backend work ...
frameReady(RemoteFrame)
```

Session scheduler 拥有：

- 目标采集节奏；
- 最大 in-flight 采集请求；
- 被拒绝请求的重试/fallback 行为；
- 是否按完成/渲染帧调节采集节奏。

scheduler **默认独立于传输积压**。慢传输不得停止本地采集/渲染，除非用户显式选择 producer-throttle 行为。

### 初始默认值

宿主证据建议公共 Quick 的保守起点：

- max capture in flight：**2**；
- completed mailbox capacity：**2**；
- completed-frame policy：**DropOldest / LatestFrameWins**。

这些是 v0.1 默认值/候选，不是普适性能声明。#18 可以为 RK3588/EGLFS 调整目标默认值，而不改变策略 API。

## Completed-frame mailbox

Core mailbox 容量计算**等待分发**的帧。dispatch worker 当前持有的帧额外计数，因此 Core 侧最大所有权是 `capacity + 1`，加上传输/客户端下游保留的帧。

### `DropOldest`：默认

mailbox 满且新帧到达时：

1. 移除最旧排队帧；
2. 释放所有权；
3. 入队新帧。

性质：

- 采集与传输速率解耦；
- 队列深度保持有界；
- 过载时 viewer 收到最新可用内容；
- 丢帧是正常流控，不是错误。

### `ProducerThrottle`：可选

有界 mailbox 满时减少/停止采集准入。

性质：

- Core mailbox 不主动丢帧；
- 采集与传输速率耦合；
- 只在“每帧都重要”优先于近实时行为时有用。

该策略必须显式选择，绝不能静默替代 LatestFrameWins。

### `BlockProducer`

不作为 v0.1 常规策略支持。阻塞 GUI/渲染/采集生产者等待传输消费，违反 HyRemote 的本地响应性目标。

## 重复/合并规则

采集后端可能产生多个完成请求，携带等价视觉内容。Core 以后可以增加重复检测/合并，但 v0.1 不得仅从请求序列推断重复身份。

传输始终可以按 PTS/队列策略丢弃被取代的帧。

## 停止语义

`Session::stop()` 是协调的，但不允许因远程客户端死锁：

1. 停止调度新采集请求；
2. 在支持时请求采集源停止/取消；
3. 关闭 completed-frame mailbox；
4. 停止/join dispatch worker（安全，因为 `Transport::enqueueFrame()` 契约有界/非阻塞）；
5. 请求传输停止 runtime/客户端；
6. 释放适配器/存储。

超时/错误处理可以把组件升级为 `Faulted`，但本地 Qt 对象销毁不得无限等待远程 peer。

## 错误类别

### 可恢复采集事件

示例：

- 目标暂时隐藏/不可采集；
- 单个采集请求被拒绝；
- 临时传输断开；
- 客户端认证失败。

Session 可以保持 `Running` 并报告事件。

### 组件失败

示例：

- 采集后端初始化失败；
- 传输 runtime 无法启动；
- 适配器永久丢失 target。

Session 转入 `Faulted` 或尝试配置的 fallback。

### 程序员/配置错误

示例：

- 启动时 target 为空/已过期；
- 采集/传输格式不兼容且无转换器；
- 无效队列容量/策略。

`start()` 在进入 `Running` 前失败。

## 状态模型

```text
Stopped -> Starting -> Running -> Stopping -> Stopped
              │          │
              └--------> Faulted
```

始终支持 `Faulted -> Stopping -> Stopped`，以便确定性回收资源。

## 传输义务

传输适配器：

- 必须安全接受/保留 `RemoteFrame` 所有权；
- 不得假设生产者内存在其 owning frame 释放后仍有效；
- 拥有 Core mailbox 之外的客户端队列/背压；
- 不得把协议原生输入/帧类型暴露进 Core；
- **必须把 `enqueueFrame()` 实现为有界/非阻塞 post/enqueue**；Core dispatch 调用中不允许网络 I/O、encode wait、客户端扇出或无界队列等待；
- 自己的 runtime/client 队列满时，必须选择显式内部溢出策略。

## 结果

- 阻塞/慢 viewer 不能直接阻塞 Qt 渲染；
- 阻塞/慢 peer 也不能无限阻塞 Core 关闭；
- 传输实现可以使用自然事件循环/线程；
- 采集和传输可以独立演进；
- 队列所有权上限显式且可测试；
- 同一策略适用于 Widgets 和 Quick 采集源。
