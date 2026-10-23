# HyRemote 规范化输入模型

> 语言 / Language：**中文** ｜ [English](en/input-model.md)

本文定义 HyRemote 与传输无关的远程输入契约，描述 C++、QML、Generic 和 QPA 前端共享的产品行为。

目标是让协议值、Qt 事件细节和平台输入机制留在 Core 之外。

## 输入边界

```text
viewer/协议事件
  -> 传输专用规范化
  -> hyremote::InputEvent
  -> Core 路由
  -> 目标专用 InputSink
  -> 在目标 UI 线程排队交付
```

Core 不把 VNC keysym 翻译成 Qt 按键，也不直接注入 GUI 事件。传输适配器规范化协议数据；目标适配器把规范化事件翻译成目标 UI 框架事件。

## 事件类别

`InputEventKind` 覆盖：

- 指针移动；
- 指针按钮转换；
- 指针滚动；
- 按键按下/释放；
- 已提交文本。

`None` 是空/默认值，合规传输不会把它作为用户输入发出。

基线指针按钮包括左、中、右键。滚动值使用规范化逻辑滚轮步长，传输在提交给 Core 前转换协议原生单位。

## 键盘与已提交文本

`KeyCode` 是后端无关的逻辑键集合，数值上不等同于 Qt 按键、VNC keysym、Windows 虚拟键、Linux evdev 码或其他协议/平台按键表示。

修饰键和锁定键有两种互补表示：

- `KeyCode` 标识正在交付转换的按键；
- `InputEvent::modifiers` 携带该事件适用的修饰状态。

已提交 UTF-8 文本作为独立文本事件携带。目标适配器不从 `KeyCode` 推断文本，避免把键盘布局假设带进 Core。

当前修饰状态包括 Shift、Control、Alt、Meta、CapsLock 和 NumLock。

完整 IME 预编辑/组合不在当前基线内。

## 指针坐标契约

指针 `x`/`y` 使用 `InputViewport.width` 和 `InputViewport.height` 描述的精确像素网格。viewport 表示 viewer 交互的远程 surface。

`devicePixelRatio` 是诊断/上下文信息。源宽高仍是 Core 映射的权威，避免 DPR 被应用两次。

目标适配器把当前逻辑目标尺寸提供给映射。Core 把源边缘映射到目标边缘，并 clamp 越界位置。

无效源几何、无效目标几何、非有限坐标或非指针事件会显式映射失败，不做猜测。

## 线程

远程输入交付不得同步阻塞 Qt GUI 处理上的传输路径。

`InputSink::post()` 把目标工作排队或编组到合适 UI 线程。Widgets 和 Qt Quick 继续负责在 Qt GUI 线程完成实际框架事件交付。

远程查看和远程输入策略相互独立。远程输入关闭或 Runtime 不处于接受输入状态时，事件被丢弃，不会在策略变化后延迟交付。

默认不把输入文本写入普通诊断日志。

## 断开清理

viewer 可能在按住键或指针按钮时消失。HyRemote 不得让本地 Qt 应用停留在合成的远程按住状态。

清理发生在两个边界：

1. **Viewer 断开**：传输会在丢弃该 viewer 的协议/客户端状态前，平衡它贡献的已识别按住状态。
2. **Runtime/目标 teardown**：传输/Core 回调静止后，目标适配器丢弃未交付的排队输入，并平衡已经到达 Qt 目标的受支持按住键/按钮状态。

无论 Runtime 通过 C++、QML、Generic 还是 QPA 进入，这些规则都适用。

## 多个 viewer

多个远程 viewer 共享一个逻辑应用输入目标，除非未来产品能力显式引入独立控制所有权。

每个 viewer 的协议状态保持独立，但同一规范化键/按钮的重叠按住会安全合并：

- 第一个持有者产生目标按下；
- 另一个 viewer 按住同一逻辑输入不会产生第二次物理按住转换；
- 一个 viewer 释放或断开只移除自己的贡献；
- 目标释放发生在最后一个持有者释放或断开时。

当前 HyRemote 不暴露每 viewer cursor、独立焦点上下文或公共每客户端控制所有者 API。

> **后续 V0.2/V1.x：** 更丰富的认证会话和控制策略 API 可以增加显式所有权/准入语义，同时不改变规范化输入词汇。

## 有界输入交付

远程输入有界。慢或卡住的 GUI 不得创建无界队列。

Widgets 和 Qt Quick 适配器区分普通有界输入和释放交付，后者用于防止此前已接受的按住状态卡住。

只有当对应的按下被接受时，释放才受保护。如果按下因背压被拒绝，后续不匹配的释放会被丢弃，不会为 Qt 从未接受的按下合成释放。

这保持过载行为有界，同时保持输入中立。

## 终止 teardown 行为

目标终止关闭时：

- 尚未交付给 Qt 的排队输入被丢弃；
- 已接受/已交付的按住键/按钮状态在目标 UI 线程上平衡；
- 重复 teardown 是幂等的，不会合成重复释放；
- QWidget 交付可以保留正确平衡先前按下所需的具体 receiver；
- 复合 QPA/自动目标在子输入适配器终止清理后才退役它们。

这是内部产品保证，不是单独的应用重置 API。

## 交给适配器的部分

规范化 Core 模型不定义：

- VNC keysym 到 `KeyCode` 的映射；
- Qt 按键/事件构造；
- QWidget 与 QQuickWindow 焦点策略细节；
- Linux `uinput` 或 Windows 虚拟 HID 等 OS 级虚拟输入；
- 完整 IME/死键组合；
- 后端专用协议记录。

这些留在传输和目标适配器之后，同时保持相同的公共远程输入行为。

## 当前产品限制

当前产品不承诺：

- 完整 IME/预编辑一致；
- 每种国际键盘布局边界；
- OS 桌面级输入注入；
- 独立每 viewer 输入设备；
- 任意原生/外部窗口输入定位。

不支持的输入场景会被记录，不会静默近似。见 [`known-limitations.md`](known-limitations.md) 和 [`compatibility.md`](compatibility.md)。
