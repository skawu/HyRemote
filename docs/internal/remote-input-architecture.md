# 远程输入架构

> 语言 / Language：**中文** ｜ [English](../en/internal/remote-input-architecture.md)

状态：**#400 输入重构的已接受方向**。

本文取代 #401 选择的输入入口层。#401 的产品约束不变：HyRemote 是应用作用域、低侵入、跨平台 Qt 远程访问框架；C++、QML、Generic、QPA 是并列接入前端。改变的只是指针输入进入 Qt 的层。

## 决策

```text
Viewer / 协议
    -> 传输无关的原始 InputEvent
    -> Runtime 准入 / 生命周期 / 隔离
    -> Qt window-system ingress
    -> Qt 输入处理
    -> QWidgetWindow / QQuickWindow
    -> Widgets / Qt Quick
```

指针输入通过 `QWindowSystemInterface` 进入 Qt。HyRemote 不为控件自行合成应用级 `QMouseEvent`/`QWheelEvent` 语义。

Qt private 依赖有意隔离在 `src/runtime/src/detail/qt_window_system_input.*`。前端和 target 适配器不包含 QPA/private 头文件。

## HyRemote 负责

- 协议到传输无关原始输入事实的转换；
- 源 framebuffer 坐标及其到所选远程 surface 的映射；
- 有界输入准入和显式背压；
- 受保护的释放准入，以及断开/停止时的平衡；
- 传输/runtime 层的多 viewer 贡献与仲裁；
- target/session/应用隔离；
- 很薄的 Qt 版本相关 window-system ingress shim；
- 作为独立 Runtime 事项的 surface 发现/选择。

## Qt 负责

原始指针事实跨过 window-system 边界后，Qt 负责：

- 子控件/item 命中测试；
- 鼠标 grab 和 release 路由；
- 单击/双击分类；
- 移动距离与点击策略；
- 滚轮传播/路由；
- enter/leave/hover；
- 上下文菜单合成和平台触发约定；
- Widgets 与 Qt Quick 指针交付语义；
- 控件特有行为。

HyRemote 不得为 `QPushButton`、`QComboBox`、`QListView`、`QMenu`、`MouseArea`、`PointerHandler` 等控件增加兼容规则。

## 指针队列规则

指针历史是 Qt 的语义输入。Runtime 不得合并或丢弃已接受的 pointer move，再用 HyRemote 状态机重建其含义。

普通通道保持有界。队列满时显式拒绝准入并施加背压。受保护释放保留专用通道，使已接受的按住状态总能被平衡。

## 时间戳规则

Runtime 在接受原始远程事实时记录单调时间戳，早于 GUI 线程排队延迟，并传给 `QWindowSystemInterface`。该时间戳是传输/runtime 事实，不是 HyRemote 点击分类输入。远程输入有意使用一致的单调时间域，不依赖未安装的 Qt private global timer。

## 本次改动的非目标

输入入口改动不解决其他所有权层：

- #404 临时顶层捕获/组合与 surface 选择；
- #362 操作系统活动窗口激活；
- RFB 客户端 framebuffer 代际/resize 坐标同步；
- 桌面级原生输入注入；
- 触摸/数位板协议扩展。

这些关注点保持分离，使指针入口保持小。

## Private API 维护

`QWindowSystemInterface` 是 QPA/private API。维护规则很简单：用应用使用的同一 Qt private 开发包编译 shim，把 private include 限制在一个 translation unit 内，并用行为测试认证支持的 Qt 版本。不要在 HyRemote 中镜像 Qt private 实现细节。

## 架构不变量

以后出现输入 bug 时，先问 Qt 是否已经在 window-system 边界之下拥有该语义。如果是，修 ingress/surface/raw-fact 的正确性；不要新增另一个 HyRemote 语义模拟器。
