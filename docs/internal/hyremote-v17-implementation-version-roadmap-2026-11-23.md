# HyRemote V1.7｜开发落地版本路线图（候选）

> 2026-11-23；本文件是 **设计衔接发行路线图的修订候选**，不是第二份 HyRemote 权威版本表、不是激活 train 的决策、不是实际产品开发许可。完整受控产品路线仍以 `docs/internal/product-version-delivery-roadmap-2026-10-23.md` 的已合并 `develop` 版本为唯一事实源，机器配置以 `.github/release/release-trains.json` 为准。View 版本由 View 仓独立 Owner 决定。本稿用于 Human 一次集中路线评审，不自设 release/tag。

## 0. 资格判断与目标

**可进入版本设计、WBS、验收规划与设计交接；尚不能未经批准进入新增 Native/Viewer/Managed 产品代码实施。** Human 已批准 D-PROC=B（Private Native HMI 端与 Qt HMI 同进程）、D-REVOKE=A（T0/T1/T2 分阶段）；Public 的已发布维护可以依其原有发行权限继续，不被 View G3/Gate-D 全产品技术冻结一票阻止。View G3 仍 NOT_PASSED、总体 Gate-D NOT_FROZEN、Gate-I NOT_AUTHORIZED；Gateway #190 未激活。

### 用户成果优先顺序，而非技术抽象优先
1. 用户现有 Public RFB 版本的画面、输入、性能、隐私与升级可靠，V0.3.5.0 按保护门正常收尾。
2. Public 保持无 Private 即可用、稳定四 Qt 接入，提供仅被明确声明的新可选 HostPort 版本契约。
3. View 独立完成标准 RFB Direct 工作台（第一份真实可用客户端），并与 Native 设备端/客户端并行开发独立 Native Direct。
4. 在真正 Native Direct/设备授权/Qt Quick 画面及安全资格成立后集成设备工作区、权限和诊断；工厂 Edge 与可选 Gateway 属条件路径。
5. 首发支持档位限于实际取得证据的 Qt Kit/OS/ARM64/真实 HMI。旧客户现场缺陷不因新路线另起版本就延后。

## 1. HyRemote Public：**保留现有权威版次，不调整已排成果**

| 现有 Public 版次 | 正式权威路线归属 | 本次新增 V1.7 依赖关系 | 当前执行判断 / 出口 |
|---|---|---|---|
| **V0.3.4.0** | 已于 2026-10-29 作为 Windows Qt5.15.2/6.8.3 已声明单元发布 | 作为 V0.3.5 lineage、View 标准 RFB Direct 候选的公共基线；不是“所有 Qt 后端 Supported” | **已发布**；旧 release 和已声明支持矩阵不追改 |
| **V0.3.5.0** | #591 宿主路径 + #589 静态文字水印；现行 active Feature，受 #95、发布 PR #783 保护 | 只做既有范围的 release 修复和 Qt 性能/安全门，**不纳入 HostPort/Native/Viewer/Edge 新功能** | **HOLD / NO-GO**：#1048 定位 RealVNC 采样点遮挡造成的假阴性，但该 PR 仅修 runner；motion 1V 无改善证明，精确候选/水印 P2 与 #95 验收不可跳过 |
| **V0.3.6.0** | Tool S1 #579/#581–#586/#590；A 优先的设备工作台/安全控制/缩放/语言/基础诊断 | 保持 Tool 独立，既有 S1 不自动迁到 View；只复用 Public RFB 基础安全、无新增 Native Client | **PLANNED**，未因本稿激活 |
| **V0.3.7.0** | #592 实际 Qt 应用+SDK Docker 同部署 | 消费已合格 config/state/secret；Container 非 Win 首版 GA 强制门 | **CONDITIONAL PLANNED**，无真实 Docker E2E 不能宣称发布 |
| **V0.4.0.0** | #235/#602 **仅 RC 资格和冻结**，不能承接缺失新功能 | 不吸收全新 HostPort/Native/Viewer；对 Windows 已支持单元与 Public RFB 质量收口 | **CONDITIONAL active:false** |
| **V1.0.0.0** | #33/#95 Windows 现有 Qt5/6.8 合格单元 GA | 只提升真实 RC 已认证成果；Public 独立 SDK release 不依赖 View | **GA PLANNED、NOT RELEASED** |
| **V1.0.1.0 ~ V1.0.7.0** | #605 Qt6.5/6.12、Tool S2/S3、Logo、免安装 Viewer/授权剪贴板等 | 严格保留各自原定成果与 post-GA 条件，不能以 View 规划偷占版次 | **按已有权威 B 表执行** |
| **V1.1.0.0 / V1.1.1.0** | #190 可选安全 Managed 阶段；#170 Session Operations | #190 只有在独立授权及 Native E2E 后才进入 Managed；Public Session Ops 仍应按原有身份/授权政策讨论 | **战略/条件规划、未激活** |
| **V1.2.0.0 ~ V1.2.7.0** | ARM64 Quick、Wayland、触控、业务控制、安全文件评估、Viewer 服务端契约、空闲捕获、诊断 | 原有 issue/顺序与正式证据 Gate 保持，不将新架构宣称自动满足任何硬件或能力 | **现有各独立 PLANNED/CONDITIONAL/GO-NOGO** |

### 新的 Public HostPort 插入规则

**成果名：P-EXT（规划占位符，非版本号、非激活 train）。** 在获批范围内将 V1.7 CT-01~09/11 的公共 HostPort、Policy/Frame/Qt 原子合同实现为最小、可选、协议中立 SDK Extension Host，守住 Public RFB 独立性和 Qt5/6 精确支持真相。

此成果**不能**自动贴到 V0.3.5.0、V0.4.0.0 或未启动的 V1.2.5.0；在 #594 通过 **现有成果冲突审查＋Owner 批准＋受审路线图 PR＋需要时机器 train 登记** 后分配一个合适的 Public Feature 版本号。已经发布的旧版本号、已排 Kit/Tool 成果不复用；不得因为 View 的计划版本而强制 Public 同号发布。若 Public 当前版本不足以支持 Native，View Native Direct 等待已发布/已合格 P-EXT，而 Public 旧 RFB Direct 可先行。

## 2. HyRemoteView：独立客户端/Private Host 的**候选版本梯队**

View 尚无经批准的完整产品版次表。因此下列仅是 **View Owner 待批准的候选号**，不是既存正式 train，绝不以 Public 版本号锁定相同的 View 版本；未经过 G3/Gate-D/Gate-I 的阶段只能做技术验证/设计交接，不得以计划号执行正式代码。

| 候选 View 版本 | 独立用户可见成果 | 实施依赖/边界 | 建议实际验收 |
|---|---|---|---|
| **View V0.1.0.0 — RFB Direct 产品预览** | 无云、单设备标准 RFB3.8 可靠查看/可授权输入、状态/错误可见；具备最小 DeviceProfile 与 SecretStore，不只是屏幕窗口 demo | 复用 Public 已发布 RFB（例如合格 V0.3.4+），独立 RFB bounded parser/Qt Quick RemoteSurface，G3 RFB/Quick 真实证据；不依赖 P-EXT、Native/Gateway | 真实设备 Host/Viewer 画面、键鼠 held/断线、resize、失权遮帧、Windows Linux 的**适用已通过单元** |
| **View V0.2.0.0 — 可信 Native Direct 技术预览** | 在 V0.1 标准 RFB 之外提供完全独立的 Native DeviceTLS/身份、Target/View/ControlGrant、画面、输入、分阶段撤权 | Public 已合格 P-EXT；Private **同进程 HMI Native 模块** + View Native Client 以 21 type/P-A/A 精确相容；安全 PoC/G3 必须有证据；禁止双端假 Mock E2E | 两仓 exact SHA/HostPort ABI/设备身份/TLS、21 type 正负字节、Host View/ControlGrant/QtFinalGate、2 Peer/撤权/无 Private RFB 回归 |
| **View V0.3.0.0 — 工业设备工作区预览** | 多 Target/多设备工作区、焦点/坐标/旧帧提示、DeviceCatalog/Trust 生命周期、运维诊断与 OEM 最小交付配置 | 消费 V0.2 已获资格 Native 或 V0.1 合格 RFB，**用户当前连接能力不能被错误升级为 Native Grant**；合法离线 Direct | 实际多设备/多屏/证书变化/重连/操作者隔离；异常状态无假 ONLINE、业务结果 UNKNOWN |
| **View V0.4.0.0 — 工厂内部 Edge 预览** | **同一个 View 安装包**内可选启用工厂登录期 Edge，精确共享 HMI、站点策略、退出/注销停止，不另装软件 | Edge 独立于 Native HMI 模块的 OS 部署概念；SitePolicy/RunLease/Pair/可信路由，安全 Gate；可先做内网 Edge 管理体验，但没有 Gateway 不宣称公网 Managed 成立 | 默认 OFF、站点白名单、管理员批准、注销/崩溃/升级后不复活 Route，已授权 Direct 仍独立 |
| **View V1.0.0.0 — 独立稳定交付** | 把已真实合格的 RFB Direct、Native Direct、Workspace 和已获支持的本地 Edge 能力形成一个可信正式安装与维护基线 | 以 View 独立安全/许可/安装/支持 Matrix/双仓版本配对真实合格结果为准，不要求 Public 与 View 1.0 同时发布；Gateway 未激活不应阻塞符合范围的 Direct View | 只声明真实 PASS 的 Qt/OS/Viewer/设备组合、升级与回退、安全/资源/性能、用户完整操作流程 |
| **View Managed 后续版本：编号待定** | 可选企业 Gateway 自托管/远程路由、账号/企业身份/审计；RoutePermit 与 HMI Grant 双授权 | 须 #190 独立授权、Stage05 受控 IDL、Private Native Direct E2E 与 Factory Edge 安全能力已真实合格；不能硬套“View V1.1.0.0”假审批 | DeviceTLS 端到端、一次性配对/ACL/租户隔离/失效重连/无任意网段代理，云端不决定设备 Grant |

**版本是否需要分成多条正式发布**取决于每阶段真实用户价值和资格；若某阶段无法独立满足客户结果，应按 View Owner 正式决议与相邻阶段合并，而非空发版本。特别是 V0.4 Edge 的“Preview”不意味着 Gateway 已经可用，若最终产品不需要独立本地 Edge 预览，则应同 View 1.0 一并审定，不强制出一个孤立 Feature。

## 3. 交付窗口、并行性与最短依赖路径

```text
现有 Public 发布维护 (V0.3.5 HOLD → #95 真资格) ──┬─> V0.3.6 Tool S1 / V0.3.7 Docker 条件
                                                       └─> Public V0.4 RC → V1.0 GA（无 View 强制依赖）

全产品 Gate-D（V1.7 原子合同/上位约束）── Gate-I 批准范围
               ├─> View G3 RFB/Quick ──> View V0.1 RFB Direct
               ├─> P-EXT Public HostPort ─┐
               │                         ├─> Private Native HMI F14 + View Native Client ──> View V0.2 Native Direct
               └─> View G3 DeviceTLS/安全 ┘
View V0.1/V0.2 的合格能力 ──> V0.3 Workspace + Trust
View V0.3 ──> (可选) V0.4 同安装 Edge
View 独立真实 E2E/版本配对与支持资格 ──> View 1.0
#190 独立 Managed 安全/产品激活 + Native Direct + Edge ──> View Managed 后续独立版本
```

**并行工作原则**：独立审查已发现“共享 View Surface 无条件依赖 RFB G3”会让 Native-only 假性阻塞；现已修正 WBS V02：RFB 显示资格取 A03R，Native-only 共享 Qt Quick 显示资格取 A03N 中的专门正负证据，而不要求 RFB parser。**此仅为技术分流设计**，View #5 当前 G3 总门仍要求 A–H 证据整体 PASS；本路线将 RFB 与 Native 分路径，仅是向 View Owner 提交的**门禁拆分建议**，不是已生效的 G3 资格规则。若 #5 未正式修改，任意单路径 G3 证据不能自动解锁其正式 Gate-I。Gate-D 审的是设计完整性，G3 可进行已单独获准的有界 PoC；Gate-I 不能以本文件自动代替 Human 激活。RFB Direct 的真实实现、Private Native HMI/Client 的真实实现，均在对应 Gate-I 范围内；可在不同产品仓库按接口冻结并行，但真实 Host↔View 对拍必须锁两仓 SHA。Public 日常维护和已批准功能 train 不被全产品 Gate-D 反向阻断。**不做“每有一个中间合同就再开一轮架构研究”**。

## 4. 明确的非目标和质量下限

- 不重建第二套 Runtime、第二个 Native HMI OS 进程、RFB 私有协议、通用插件总线、独立工厂 Edge 软件或强制云端。
- 不通过 View Gate 强迫 Public 延期独立升级；不由 Public HostPort 决定商业许可/收费功能；不以商业许可禁用已发布标准 RFB 权限和最低安全。
- 不以 V0.3.5 runner 采样遮挡假阴性证明 Native 或 viewer 性能已优化，亦不能掩盖 motion 1V 的未改善。客户现场 ARM64 结果按现场可用性独立收集，不假设迁移 Windows 数据就是 ARM PASS。
- 不擅自改 Native 1.0 21 type、TLS-first 两档，也不把延期的文件业务回执/离线签收通道塞回当期必需；H-O01/H-O02=A 保持。
- 首轮 Public GA 的 Windows Qt5.15/6.8 事实资格与后续 Qt6.5/6.12、ARM64/Wayland 分期不混淆；任何支持等级必须有真实可复现 evidence，不能从设计列表推出全平台 high-FPS。

## 5. 路线评审的唯一重大决定与实施准入

1. Human 现有已批准架构决定不重开。**尚需集中确认的是 View 候选版本梯队和 P-EXT 的 Public 正式成果版次分配**；Private View 不拥有 Public 版次批准权。
2. 正式 Gate-D：V1.7 + D1–D5 真实实施责任、线程/授权/资源、适用负例、V15/V8 原件差异与既有 Human 分期全部审签，无本期不可实施关键矛盾；不要求正式产品运行结果或 G3 已完成才能审技术设计。
3. Gate-G3：按 View 实际选定平台/Native/RFB 路径获得 PoC 性能、安全、许可与 E2E 证据；不得以协议纸面向量代替 Parser/DeviceTLS。
4. Gate-I：Human **按工作包和仓库范围**授权新增代码；Public 既有维护/已授权 v035 修复沿原有 Gate，不因本计划失权。
5. Gate-R：每一个发布的独立产品/声明组合检查 #95 对应治理、两仓双 SHA、真实 Viewer/Qt Kit/内存/安装/回退证据，Managed 自有 #190 安全准入；任何一项 FAIL/HOLD 不伪造已发布。

状态：`ROADMAP=CANDIDATE_FOR_OWNER_REVIEW`；`HYREMOTE_CANONICAL=UNCHANGED`；`MACHINE_TRAINS=UNCHANGED`；`VIEW_VERSION_NUMBERS=PROPOSED_NOT_APPROVED`；`GATE_D=NOT_FROZEN`；`GATE_I=NOT_AUTHORIZED`。衔接 [V1.7 三项收口](hyremote-v17-three-closures-design-2026-11-23.md) 和同分支 WBS 文件。
