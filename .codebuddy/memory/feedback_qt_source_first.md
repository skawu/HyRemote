---
name: Qt 技术判断以精确版本 Qt 源码为最高依据
description: 涉及 Qt 渲染循环/场景图/DPR/RHI/抓帧/信号时序的结论必须查 qtbase 源码，不依赖搜索文章或论坛
type: feedback
---

在本仓库讨论任何 Qt 内部行为（`QQuickItem::grabToImage` 的 DPR 语义、`QQuickWindow` 渲染循环、`afterRendering`/`afterFrameEnd`/`frameSwapped` 时序、QRhi readback/fence、`QQuickItem` damage/update、Qt5 与 Qt6 差异）时，**最高依据是与实测 Kit 精确对应的 Qt 源码，不是搜索到的文章、论坛、博客或通用知识**：

- 本机已有 Qt 6.8.3 源码树 `C:\Qt\6.8.3\Src\qtbase`（直接只读检索）；Qt 5.15.2 本机只有头文件，源码需只读参考 `github.com/qt/qtbase` 的 `v5.15.2` 标签/对应维护分支，不能用 Qt6 实现推断 Qt5 行为。
- 优先顺序：精确版本 qtbase 源码 → HyRemote 自身代码中的调用点 → 本机实测证据 → 明确标注的推断。**不引文章/论坛结论作为依据**。
- 每条涉及 Qt 机制的断言要区分三类：`源码证实`（给文件与行号）、`HyRemote 侧调用可证`、`待验证推断`。没有源码证据就直说，不用记忆中的 Qt 常识填空。
- 提出技术方案前先核对所依赖的 Qt API/信号在该版本是否真实存在、发出时机与线程上下文，避免把「听起来合理」的机制写成建议。

**Why:** 2026-11-11 Human 在追问技术建议后补充「我认为你应该参考的最多的就是 Qt 的源码，不应该过度依赖你搜索到的文章或者论坛」。当时关于高 DPI grab 图像网格、`QRhiReadbackResult` 完成回调、`afterRendering`/`afterFrameEnd` 时序的论断只有 HyRemote 侧调用点与实测现象支撑，未经 qtbase 源码核实，属于依据层级不够。
**How to apply:** 今后做 Qt 相关技术评审、根因分析、优化建议或回答「为什么」时，先检索本机精确版本 qtbase 源码定位实现，再下结论；跨 Qt5/Qt6 的差异分别查对应版本源码；最终答复里显式标注证据类型，不把推断包装成事实。
