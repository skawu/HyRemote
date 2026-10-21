# HyRemote — Local Owner execution brief (2026-10-21)

> 状态：经 #549 提交的 **工作指导 / rebaseline proposal**。本文件指导本地 Owner 和执行 Agent 排序、验证及提出决策，不自行修改 #1/#24/#33/#95/#343 的产品承诺，亦不替代 `.github/release/release-trains.json` 的机器发布权威。需要变更 GA 支持矩阵或发布门禁时，先取得显式 Owner 决策，再做受审的原子权威同步 PR。

## 0. 基线与不可变条件

- 取本文件准备时的 `develop@5a07184dd400d8d1ed2e9f694fcb88ddc95eb8fd` 为 **文档制作基线**，不是后续任务永远锁定的运行基线。每次执行须重新确认 `develop`、Issue 最新权威评论、当前 PR、实际代码与 CI SHA。
- **V0.3.0.0 Product Preview 已于 2026-10-19 发布**，见 [tag/release](https://github.com/skawu/HyRemote/releases/tag/v0.3.0.0)。历史 tag、main、release 分支、已接受 artifact 不能被本指导改写。 `develop` 根版本继续为 `0.0.0`。
- `V0.3.3.0` 是机器发布矩阵中已激活的下一项 product-breadth Feature，必需子项 **#144 + #175**，前提 #261；`0.3.1.0 / 0.3.2.0` 是退役的技术预留号，不得复活。**V0.4.0.0 仍为条件激活、资格认证-only**；任何将其写成已激活的表述均错误。
- 架构冻结：一个 Core、一个 Shared Runtime；C++ / QML / Generic / QPA 是对等技术（适用性按精确 cell 判定）；Widgets / Quick 是 UI 目标；RFB 是首个 transport。
- 默认安全现实：监听 `0.0.0.0:5921`，默认不认证、不加密、远程输入关闭；条件 VNC 口令认证也**不加密**；`AuthenticatedEncrypted` 不可用且失败关闭。不得声称当前为互联网直连安全产品。
- 本执行次序不会自行把 TLS、云中继、Session Registry、第一方浏览器 Viewer、GPU/硬件编码纳入 GA。 #123 只在测量证明产品阻塞后条件激活。

## 1. Owner 与 Agent 分工

**模式：AGENT_PRIMARY_HUMAN_ASSISTED。** Cloud/Local Agent 负责最新基线核对、Issue/PR 和可重复测试、最小实现、审核线程处置、证据打包、PASS/FAIL/BLOCKED 判定及状态报告；Local Owner 决定产品范围、GA 支持变更、重大风险接受、实体设备与需要身份权限的操作。不要把安装/运行/截图归档等可自动执行事项转交 Human。

每个工作包都先核查是否已有合并修复/并行 PR；复用归属 Issue，避免重复开缺陷。使用 `feature/<issue>-<topic> -> develop`；不直推受保护分支，不提前合并或打 tag。若不具备物理设备，完成云端可验部分并标 `PHYSICAL_BLOCKED`，不得把 hosted CI 伪装为物理资格。

## 2. 当前优先级与工作包

### P0 — 先清除已发现的输入正确性/审查质量风险

Owner 指派一个 bounded defect triage，不要求立即将四项都强行并入同一 PR。

| 证据/根因 | 最小复核/修复任务 | 自动化验收 |
| --- | --- | --- |
| [PR #533](https://github.com/skawu/HyRemote/pull/533) P1：Widgets 激活后首键焦点 | 验证 `keyboardReceiver(root)` 在 `activateWindow()` 前缓存导致 receiver 落到 root 的路径；保留应用作用域与 OS 前台锁限制 | 不活动窗口切换、输入框首键/连续按键、KeyUp 和焦点被拒绝情况 |
| [PR #544](https://github.com/skawu/HyRemote/pull/544) P1：RFB 指针到 HiDPI Qt 逻辑坐标 | 统一光标 hit-test 与输入映射的坐标变换；不得简单假定 DPR=1 | Windows/Linux 可执行 DPR 1/1.5/2 Widgets + Quick hit-test 及 resize/reconnect |
| [PR #543](https://github.com/skawu/HyRemote/pull/543) P2：Cursor poll 时序 | `refreshCursor()` 不得为未真正发送的 cursor 提前计入 250ms poll；验证 viewer 先 `SetEncodings` 后 `SetPixelFormat` | 8/16→32bpp、首个 FramebufferUpdateRequest、静态画面首 cursor 与持续连接 |
| [PR #546](https://github.com/skawu/HyRemote/pull/546) P2：AArch64 profile 引导 | 从原 `build.yml` 复制时显式设置 `build.examples=false` 和所需 `integrations`；避免重复 `CMAKE_BUILD_TYPE`/ `build.type` | profile 解析断言与 docs path/self-service 检查 |

上述问题属于**风险/待复核**，并非本 brief 声称所有平台都已复现。已合并 PR 上的独立审查线程在 2026-10-21 可见为 unresolved；Agent 需核对是否已被后继提交修复。结案规则：提供 exact SHA 测试事实 + 对照代码 + 评论回复与显式 resolved/Owner-accepted risk 记录；不可只因 CI 绿而消掉 P1。

### P1 — 关闭 V0.3.3.0 的“实现→验收”缺口（当前 Feature，不增加新产品方向）

权威：[机器 release train](../../.github/release/release-trains.json)、[#144](https://github.com/skawu/HyRemote/issues/144)、[#175](https://github.com/skawu/HyRemote/issues/175)、[#370](https://github.com/skawu/HyRemote/issues/370)。 

- 先创建 `implemented / automated-tested / physical-qualified / accepted` 四列能力差异表，代码、CI 与物理证据必须各绑精确 SHA。
- #144：Raw deterministic fallback、实际压缩路径、增量请求/无损 damage、static 无冗余 payload、slow viewer/backpressure 有界、resize/full refresh、每 viewer state 隔离；与 #371 Continuous/Fence、#372 demand-driven capture 组合验证，不把普通用户变成编码/队列参数调优者。
- #175：维护中可获取的 GUI Viewer，记录**产品/版本、OS、Qt、编码与安全握手、pointer/key/text/wheel、resize、连接/重连/停止**。内部脚本只能验证协议，不能代替 maintained GUI Viewer。
- 性能：采用 #370 的 W1 静态、W2 局部、W3 交互、W4 高动态场景，记录 `P50/P95`、帧年龄、CPU/RSS、bytes、慢客户端行为和本地 UI 影响。参考条件 `1080p / wired RTT<=20ms / >=100Mbps / 1 qualified Viewer`、交互 `P50<=200ms`、`P95<=500ms` 是**限定包络**，不能宣传为任意网络保证。已知 #410 曾有单次多秒 stall，需追踪，不可拿一次达标掩盖尾部缺陷。
- 仅当 #144/#175 的验收事实上完成且现有机器 Feature gate 通过，才建议冻结 V0.3.3.0 候选。Qt5/ARM64/TLS/云平台不是此 exact Feature 的隐含 blocker，也不能偷渡为其随带大项。

### P2 — 并行技术预检，不抢占当前 Feature 的完成

**Qt-LTS 更新：** 官方 [Qt 6.12 LTS 发布（2026-09-30）](https://www.qt.io/blog/qt-6.12-released)；[Qt 官方支持版本表](https://doc.qt.io/qt-6/qt-releases.html)。按以下顺序产出一个 no-code/probe-first 预检：

1. 保持现已承诺的 Qt 5.15、Qt 6.5、Qt 6.8 GA baseline（#343/#57）不静默删减；Qt 6.8.3 仍是当前精确发布参考。
2. 为 Qt 6.12.0 做公共 C++、QML、Generic 的编译/依赖/API 差异检查，QPA 单列 `EXACT PRIVATE ABI` 风险，不作跨补丁推断。
3. 记录安装来源/许可证/工具链、Qt5 qmake 消费、测试成本、真实应用覆盖和预计维护代价；给 Owner 提出 **GO（纳入 GA 明确支持单元） / DEFER（留在后续 Feature） / NO-GO（有证据的限制）**。
4. **未有 Owner 决策前，Qt 6.12 仅为预检，不修改 #343/#33 / 发布矩阵 / 当前 Supported 声明。**

**嵌入式：** #203 真实 Quick-only appliance `libQt6Widgets.so` 意外依赖优先于加速。以 RK3588 + Buildroot + Qt 6.8.3 + EGLFS/OpenGL ES 为参考，验收干净 rootfs 且无意外 QtWidgets 依赖、Qt Quick 本地画面/输入、远程 view/control/reconnect、runtime/manifest loader 溯源；辅以 Wayland 具体 reference cell，不由一个 board 外推整个 ARM64/SoC。任何 DMA-BUF/RKMPP/H.264 提案先按 #123 交出 #370 失败指标与根因分解，未证明失败则 DEFER。

**HyRemoteTool / 产品说明：** 同步 [#378](https://github.com/skawu/HyRemote/issues/378) 与当前正式 Release 的实际 shipped role，明确 SDK/Runtime（核心产品）、Tool 内 Host 工作面/Viewer 工作面（已实现的具体能力）、诊断/发布工具；不能把宿主诊断工具、RFB Viewer 和受认证管理平面混写。README 仍出现旧 V0.2 badge；更新时需与真实发布身份核对。公开说明 LAN-only / default insecure 与安全登录的区别。

### P3 — 进入 V0.4 / V1.0 的严格决策

- **V0.3-family 完成：** #343/ #57 已声明的 Qt5.15、6.5、6.8，Windows/Linux、x86_64/ARM64、qwindows/qxcb/Wayland/EGLFS 的**适用参考 cell**；#144/#175/#370；#203 嵌入式依赖；不要求虚构全笛卡尔积支持。
- **V0.4 条件激活：** 只有当 Product Owner 明确确认上述 GA 基线的功能完备、兼容矩阵和证据 plumbing（#393 的资格证据架构）就绪时，才按照 #235 和机器权威激活。V0.4 不承接尚未实现的 Qt 兼容/传输/设备功能，只做资格、缺陷修复和冻结。
- **V1.0：** #33/#101/#107/#109 的公开 API/package/support 及物理本地+远程共存均在同一候选 SHA 闭环。GA 范围变更需明确 Decision Record，不能借修改一份说明文档自动升级/豁免原支持承诺。

## 3. 证据与 Review Gate（Owner 的统一验收格式）

每个 PR/Issue 的证据至少包含：

```text
ISSUE/PR=                   BASE_SHA=       HEAD_SHA=
UI_FAMILY=                 ROUTE=          Qt/OS/CPU/QPA=
BUILD/INSTALL=PASS|FAIL|NOT_RUN (executed job link)
TARGET/VIEWER=product+exact version
ENV=network/graphics/DPR/device identifiers as applicable
TESTS=executed cases and counts, with logs/artifacts
PERF=workload, N, valid/miss, p50/p95, CPU/RSS, frame age, wire bytes
PHYSICAL=PASS|FAIL|BLOCKED|NOT_APPLICABLE (Human only where necessary)
CODE_REVIEW=SHA, findings, threaded disposition, unresolved_count
RESULT=PASS|FAIL|BLOCKED|INSUFFICIENT_EVIDENCE
OWNER_DECISION=required?|link; next owner + smallest next action
```

- **G0 local**：自测 + 静态规则 + 聚焦测试；**G1 PR**：受影响的真实 Windows/Linux lanes、真实 steps、独立 Review Gate，0 unresolved major threads 或有显式、可追溯的 Owner 处置；**G2 Feature**：同一 SHA clean build/install/deploy、SDK consumer、RFB maintained Viewer 与 W1–W4 合同；**G3 RC/GA**：完整资格/长期稳定/真实应用/必要物理单元，exact tag 与 artifact/checksum 一致。
- `workflow success` 必须区分 `job/step executed`、`skipped` 和 `runner unavailable`；`passed earlier SHA` 不等于 `passed current SHA`。真实 GUI/板级/本地输入证据不能用 hosted/offscreen 替代。
- 线上还未获得实机环境时，不拖住云端可完成的核查与协议自动化，明确遗留的 `PHYSICAL_BLOCKED` 与取证配方；需要人工时请求**最少**的一次性设备/Viewer 交互。

## 4. Local Owner 的明确决策清单（不要让 Agent 猜）

| 决策 | 当前默认 | 什么时候提交给 Owner |
| --- | --- | --- |
| Qt 6.12 进入首个 GA? | **未批准，仅预检** | #57/#343 兼容结果、资源/支持成本齐备 |
| #343 GA 参考矩阵是否收缩? | **不收缩** | 必需 cell 有可复现实证阻塞和可接受的替代范围时 |
| 首个 GA 是否承诺公网安全远控? | **不承诺** | 有加密、身份授权、威胁模型和可验证部署合同时 |
| HyRemoteTool 是否升级为独立完整管理平台? | **不升级** | 明确用户场景、访问安全与独立产品收益时 |
| 新的 RFB 传输/硬件加速? | **不激活** | #370 SLO/资源瓶颈经精确环境和 #123 证明时 |
| V0.3.3.0 候选是否可冻结? | **按 #144/#175 事实门禁** | 精确 SHA + CI + maintained Viewer + 审查闭环就绪时 |

## 5. 给本地 Owner / 执行 Agent 的可复制任务指令

> 接管 `skawu/HyRemote` 的当前 `develop`；先读 `AGENTS.md`、#549 与本 brief、#1/#24/#33/#95/#343、`.github/release/release-trains.json` 及待处理 Issue 最新评论。使用 **AGENT_PRIMARY_HUMAN_ASSISTED**。先清查 #533/#544/#543/#546 已合并审查发现是否在新 HEAD 仍存在，按现有 ownership 用小 PR 修复并补确定性回归；然后给出 V0.3.3.0 的 #144/#175 精确 SHA 能力-证据差异表，完成最小实现与 Viewer/性能准入。平行执行 Qt 6.12 LTS **只读预检**及 #203 Quick-only ARM64 部署前置审查，但不未经授权改变 GA 强制矩阵。遵守 G0→G1→G2→G3、独立 Review Gate、无私有设备时如实标注 BLOCKED。每轮输出：`HEAD`、`PASS/FAIL/BLOCKED`、实际执行的证据链接、剩余 blocker、下一步最小任务。不得跳过 Owner 产品范围决策、不得直接修改 main/release/tag。

## 6. 本 brief 完成的边界

此指导可以在 **Review Gate 通过后**合入 `develop`，帮助 Agent 正确排列工作；它**不是** #343 / #33 / #235 / release-trains 任何一方的新替代权威。提及 Qt 6.12 的日期来自 2026-09-30 的 Qt 官方公告。本 brief 不声称本次已编译/运行 Qt 6.12 或 ARM64，也不声称 P0 缺陷已修复。
