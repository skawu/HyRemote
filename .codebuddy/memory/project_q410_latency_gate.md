---
name: #410 V0.3.0.0 实机交互延迟门禁状态
description: HyRemote #410 物理延迟验收的当前结论（PASS）、根因阶段与所用环境（2026-10-10 有线 LAN 运行）
type: project
---

HyRemote #410（V0.3.0.0 发布前"真实 Viewer 交互延迟"门禁）已在锁定候选 `3368a17a113318b9d805cd7bc42a4263970c1c86` 上、于**有线参考级 LAN** 完成验收，结论 **PASS**（P50 75.5 ms ≤ 200 ms、P95 206.1 ms ≤ 500 ms、n=40 有效 0 未命中，均为同一客户端单调钟）。此前的 `INSUFFICIENT_EVIDENCE` 结论已被本轮取代。证据：Issue #410 评论 6096073840、#234 评论 6096074109，本地 `tmp/physical-evidence-q410/{ENVIRONMENT.md,RESULT.md,SAMPLES.csv,SAMPLES_loopback_reference.csv,target_trace_qslider_lan.txt,stage_probe.log}`。

**Why:** 上一轮失败是因为环境不合格（WiFi USB 网卡 115 Mbps 链路、RTT 67 ms）且采样器不可靠；本轮改用 **Intel I219-LM 有线 1 Gbps 双机参考 LAN**（设备地址按脱敏要求不记录，链路指标见下），实测 TCP echo RTT P50 0.52 ms、双向 912/933 Mbps，并修好了采样器的两处真实缺陷：① 消息解析按偏移原子化（旧版在超时中丢弃半个 payload 导致 "unknown server message type 243" 失同步）；② 坐标必须先用产品自身的 `SHOWCASE_INPUT` 轨迹标定到具名控件（旧坐标打在 QProgressBar 上 ⇒ 无 damage ⇒ 100% 未命中）。回环对照 P50 69.3 ms vs 有线 75.5 ms ⇒ 链路只贡献 ≈6 ms，**根因阶段为 `CAPTURE_PACING`**（input_to_qt 12.2 ms、capture_to_viewer 15.5 ms，其余约 45–50 ms 落在捕获/损伤检测环节，与"本机拖动经 Viewer 观看无延迟、Viewer 内拖动有延迟"的人工观察一致）。

**How to apply:** 不要再用上一轮的 INSUFFICIENT_EVIDENCE 描述 #410；也不要把这个 PASS 当成发布授权（Ready/tag 仍由维护者决定，V0.3 家族性能基线 #370/#371/#372 不因此取消）。若需复测：`hyremote-tool --auto-start --remote-input --port 5921` 分享的是**它自己的窗口**（客户端 framebuffer = 该窗口客户区，例如 780x1208），且窗口客户区高度会变（1193→1208）⇒ **每次测量前必须重新用 `tmp/q410/calibrate.py` 右键标定控件坐标**。

**2026-10-19 复测（新候选 `bc9de14`，#469 紧凑布局）：PASS，#410 已 closed。** P50 74.92 ms / P95 95.60 ms（n=39 有效，1 次超时），有线 LAN、RTT p95 1.49 ms、1920×1080；`release→首帧` 产品+网络腿 p50 14.2 / p95 34.9 ms（其中约 60 ms 为采样装置注入的合成 hold）。延迟**不再是 V0.3.0.0 发布阻塞项**。保留跟踪、不作门禁：①约 2.5% 偶发多秒停顿（一次约 5 s 自愈，主机侧未定位）；②采样期间无维护版 Viewer 并发连接（`SHOWCASE_CLIENTS` 峰值 1），故无 Viewer 显示时间主张；③结论只覆盖 `bc9de14e`，Draft PR #471 的 Tool UX 需另行验收。与上一轮的关键差别是窗口从 780×1193（约 3.7 MB 整帧）变成 780×700 紧凑客户区，因此 `CAPTURE_PACING` 不再复现。
