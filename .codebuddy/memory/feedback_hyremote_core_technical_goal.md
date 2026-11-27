---
name: HyRemote 任务的技术底层总目标
description: 回答 #949 及相关任务「目的是什么」时必须以「从技术底层实现 HyRemote 支持 Qt 5.15 以上 LTS 的高性能低延迟远程访问」为锚，不要泛化到治理/协作层
type: feedback
---

讨论 HyRemote 的 #949 / #947 / #945 / #806 / #605 等采集、DPR、退休等待、后端路线任务时，**最终目的必须落到技术底层命题**：从 Runtime/Core 的采集管线层面，让同一套 HyRemote 在 Qt 5.15 及以上 LTS 家族上都实现高性能、低延迟的远程访问。先讲这个，再讲证据、准入或分工；不要把答案抽象成「发布治理」「证据链」「Cloud↔Local↔Human 协作」等上层表述。

- 「一条 Runtime、多个 Qt LTS」：架构不变量是 one Core + one Shared Runtime，不能按 Qt 版本各写一套；Qt5.15.2 没有 Generic 插件（硬依赖 Qt6::Gui），直接 Quick 只能走 C++ API；Qt6.8.3 有 RHI（d3d11/opengl）；Qt6 用 `effectiveDevicePixelRatio()`、Qt5 用 `devicePixelRatio()`。当前 GA 前资格只认 Qt5.15 与 Qt6.8（#563/#603），Qt6.5/6.12 是 post-V1 GA（#605），不要擅自扩大「5.15 以上」的范围。
- 「高性能低延迟」是双目标：CPU 占用与端到端 Viewer 延迟必须同时改善；CPU 降 26–30pp 但延迟涨 35–100ms 是取舍失败，不是优化。Viewer 屏幕扫描可见时延至今 `NOT_MEASURED`，目标判据本身尚未闭环。
- 「底层」指采集管线：Qt 渲染 → 抓帧（公共 `grabToImage` 或原生 GL PBO / D3D11 staging）→ GPU→CPU 回读与 fence 退休 → Composite 合成（#806 逻辑 1x 画布）→ RFB 编码 → Viewer。跨版本差异（高 DPI grab 返回物理网格、RHI 后端、场景图重建）都要在这层消化，不改线协议、不缩放合成画布。
- #949 的 A1/B/A2、DPR=1.5、Qt5.15 真实像素、断线重连，是在两条 LTS 线上对同一条管线做「正确性 + 双目标」的现场度量；实验开关默认 OFF、原生晋升仍 NO，说明这条默认路径尚未收敛。

**Why:** 2026-11-11 Human 连续追问三轮「目的是干嘛 / 解决什么问题 / 最终目的是干嘛」后明确纠正「唉，不对，Human 要解决的是从技术底层实现 HyRemote 支持 Qt 5.15 以上 LTS 版本的高性能低延迟的远程访问」。此前回答虽覆盖工程根因与发布治理，却把核心技术命题说丢了，被 Human 判定为高度不对。
**How to apply:** 今后被问及本仓库任何性能/采集/Qt 版本任务的意图、价值或优先级时，第一句就锚定上述技术命题；随后用具体采集阶段、Qt 版本差异、CPU/延迟实测取舍展开「所以然」；治理/准入/协作只作为该命题的约束条件补充，不能当作答案主体。
