# Qt Quick 采集与输入基线

> 语言 / Language：**中文** ｜ [English](en/quick-capture.md)

本文描述 HyRemote 对合格 Qt Quick 目标的当前产品行为。

应用通过所选 HyRemote 前端附着 `QQuickWindow`，不直接构造采集源或输入 sink。

## 产品边界

可编程 C++ API：

```cpp
HyRemote::RemoteAccess remote(quickWindow);
remote.start();
```

QML API 在同一 Runtime 上提供声明式前端；Generic Plugin 和 QPA 可以自动发现受支持的应用自有 Quick surface。

Quick 目标适配器是 Runtime 内部实现细节。

## 采集基线

可移植正确性路径：

```text
Core 调度器
    -> Quick 目标适配器
    -> Qt GUI 线程排队请求
    -> QQuickWindow::contentItem()->grabToImage(...)
    -> 异步 ready 回调
    -> 自有 CPU 可读帧存储
    -> RemoteFrame
    -> 有界 Core 交接
    -> 传输
```

生产基线避免在正常路径同步调用 `QQuickWindow::grabWindow()`。

当前属性：

- QQuickWindow/QQuickItem 访问发生在 Qt GUI 线程；
- 采集完成是异步的；
- 帧存储独立于临时 Qt grab 结果；
- 像素尺寸由当前目标尺寸和 DPR 计算一次；
- 完成时间作为安全基线内容时间戳；
- damage 可以保守表示为整帧；
- 已完成帧交接保持有界；
- 目标销毁以产品级 target loss 呈现，不留下陈旧采集工作。

窗口暂时不可采集时，Runtime 避免忙循环，也不同步阻塞渲染/UI 路径等待帧。

## 远程输入基线

规范化远程输入路径：

```text
传输规范化
    -> Core InputEvent 路由
    -> Quick InputSink
    -> GUI 线程排队交付
    -> QQuickWindow
    -> 正常 Qt Quick 焦点/事件路由
```

基线支持 [`input-model.md`](input-model.md) 中的输入类别：指针移动/按钮、滚轮步长、逻辑按键转换、修饰键，以及传输提供时的提交文本。

指针坐标从远程帧 viewport 映射到当前逻辑 QQuickWindow 几何，不重复应用 DPR。

输入交付异步且有界；传输回调不等待 Qt GUI 线程处理事件。

## 可见性与生命周期

隐藏、最小化、已销毁或快速变化的 Quick 窗口不按普通静态 framebuffer 处理。

Runtime 保持采集工作有界，目标不再有效后不发布帧。正常 viewer 断开/重连不要求重建应用或目标窗口。

## 兼容范围

基线覆盖合格 Qt Quick 2D 应用内容，包括普通控件、文本焦点和同场景 overlay。

基础 Quick 支持**不会**自动覆盖：

- Quick3D；
- 自定义 framebuffer/渲染节点管线；
- 特殊图形后端；
- `QQuickWidget` 混合 Widgets/Quick 组合；
- 原生/窗口化 popup 变体；
- 正常应用自有 Qt surface 模型之外的外部/原生窗口。

这些场景需要显式兼容资格。

## 性能方向

当前公共 Qt Quick 回读路径是正确性基线，不是零拷贝性能声明。

未来实现可以增加：

- 渲染线程/RHI 感知采集；
- 外部/GPU 后备帧存储；
- 更高效回读/转换；
- damage 感知更新；
- 嵌入式低拷贝路径；
- 硬件辅助编码。

这些优化留在共享 Runtime 和 Core 契约之后。普通应用不需要替换 C++/QML/Generic/QPA 接入模型来采用更快的后端。

## 当前状态

当前参考环境中，C++ API 和 Generic Plugin 的 Qt Quick 路线是并列路线。

QML API 和 QPA 也是并列路线；QPA 还需要精确 Qt private-ABI 资格。

精确产品状态见 [`compatibility.md`](compatibility.md) 和 [`known-limitations.md`](known-limitations.md)。
