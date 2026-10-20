# Qt Widgets 采集与输入基线

> 语言 / Language：**中文** ｜ [English](en/widgets-capture.md)

本文描述 HyRemote 对合格 Qt Widgets 目标的当前产品行为。

应用通过所选 HyRemote 前端附着 `QWidget`，不直接构造采集源或输入 sink。

## 产品边界

可编程 C++ API 的正常用法：

```cpp
HyRemote::RemoteAccess remote(&widget);
remote.start();
```

Generic Plugin 和 QPA 可以通过共享 Runtime 自动发现受支持的应用自有 Widget surface。

Widgets 适配器是 Runtime 内部实现细节，应用不显式选择。

## 采集路径

可移植正确性路径：

```text
Core 调度器
    -> Widgets 目标适配器
    -> Qt GUI 线程排队工作
    -> QWidget::render()
    -> 自有 CPU 可读帧存储
    -> RemoteFrame
    -> 有界 Core 交接
    -> 传输
```

当前基线属性：

- QWidget 访问发生在 Qt GUI 线程；
- 每个发布帧在所需生命周期内拥有自己的像素存储；
- 不把借用/复用的 `QImage` 指针暴露为远程帧；
- device pixel ratio 只应用一次到采集尺寸；
- 完成时间作为安全基线内容时间戳；
- damage 可以保守表示为整帧；
- 采集到传输的交接保持有界；
- stop/目标 teardown 阻止 Runtime 不再活跃后发布陈旧排队采集。

这是正确性基线，不是最终性能上限。

## 远程输入路径

启用远程输入时，规范化输入路径：

```text
传输规范化
    -> Core InputEvent 路由
    -> Widgets InputSink
    -> GUI 线程排队交付
    -> QWidget / 聚焦子控件
```

基线支持 [`input-model.md`](input-model.md) 中的规范化输入类别：指针移动/按钮、滚轮步长、逻辑按键转换、修饰键，以及传输支持时的提交文本。

指针坐标从远程帧 viewport 映射到当前 QWidget 目标几何，不重复应用 DPR。

输入交付异步且有界。传输回调不同步等待 QWidget 事件处理。

## 窗口与 surface 范围

单个显式 QWidget 目标不表示“采集整个桌面”。HyRemote 面向应用。

取决于所选前端，受支持的顶层对话框、菜单、popup 或其他应用窗口可以通过 Runtime 的自动/组合 surface 模型表示。

任意外部/原生 OS 窗口不会自动包含。

当前合格 surface 范围见 [`compatibility.md`](compatibility.md)。

## 图形特例

基础 QWidget 资格不会自动覆盖：

- `QOpenGLWidget`；
- `QQuickWidget` 混合内容；
- 原生子窗口；
- 自定义渲染引擎；
- 绕过正常 QWidget 渲染假设的第三方组件。

这些场景需要显式兼容行或真实应用资格，不能从 raster Widgets 支持推断。

## 性能方向

当前 Widgets 路径优先可移植正确性和自有帧生命周期。

未来改进可以包括：

- 可复用帧池；
- damage 感知采集；
- 更高效的图形回读；
- 平台专用低拷贝路径；
- 硬件辅助转换/编码。

这些变化留在 Runtime 边界之后，不要求普通应用替换 `HyRemote::RemoteAccess` 或选择内部采集类。

## 当前状态

当前参考环境中，C++ API 和 Generic Plugin 的 Widgets 路线是并列路线。

QPA Widgets 支持仍受其精确 Qt private-ABI 兼容边界约束。

精确产品状态见 [`compatibility.md`](compatibility.md) 和 [`known-limitations.md`](known-limitations.md)。
