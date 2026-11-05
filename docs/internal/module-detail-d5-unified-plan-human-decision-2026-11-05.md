# D5｜HyRemote 全产品统一实施规划、设计冻结准入和 Human 决策卡（候选）

> **设计总控**：[HyRemote #843](https://github.com/skawu/HyRemote/issues/843)，覆盖 14 模块 F-01..F-06、V-01..V-03、E-01、G-01、T-01、X-01/X-02。  
> **设计前提**：Human 已批准产品设计 `HYR-PD-001/V15` 和架构 `HYR-ARCH-001/V8`；已合并 D1 Core/Runtime/四接入/设备授权、RFB/Native 独立协议、D1 横向一致性候选，以及 D2 客户端、D3 Managed、D4 Tool/交付设计候选。Stage04/05 已有详细 IDL/DAG 不重编。  
> **法律/工程效力**：`DESIGN_CANDIDATE / IMPLEMENTATION_WBS_ONLY / NO_CODING_ORDER / NO_PRODUCT_ACTIVATION / NO_RELEASE_AUTHORIZATION`。**设计包合并不是 Human 全产品详细设计冻结，也不是 G3/G4/G5、Gateway #190 或任何产品版本激活。**

## 1. 现在的真实阶段：设计接近全域覆盖，但不能宣布冻结

| 阶段 | 交付范围 | 当前设计状态 | 未能自动越过的门禁 |
| --- | --- | --- | --- |
| D0 基线/目录审查 | 14 模块与覆盖矩阵 #843；设计模板 | **覆盖完成，正式原文追踪待证** | V15/V8 DOCX 和 Native A/B 的真实原文逐条核对 |
| D1 Framework | F01–F06：Core、Runtime、四入口、RFB/Native、Host 安全 | **三份设计候选与横向审查** | 传统 RFB 身份/Scope、Dual 启动、撤权在途数据、真实 Annex A/B |
| D2 HyRemoteView | V01–V03：完整 Direct、QtQuick、Trust、设备/凭据/诊断/OEM | **设计候选** | G3 #5 **NOT PASSED**、真实 Native DeviceTLS/Frame/Grant、源条款对照 |
| D3 Edge/Gateway | E01/G01：同安装 Edge、受管控制/字节隧道、ACL/自托管 | **复用 Stage04/05 并形成跨模块候选** | #190 **NOT_ACTIVATED**、control IDL 版本、OS 后台安全、真实 Managed E2E |
| D4 Tool/交付/安全 | T01、X01、X02：Tool 角色、安装/Qt Kit、全系统验收 | **设计候选** | 与已批 Tool 综合工作台重叠的 Human 产品取舍；支持矩阵实证 |
| D5 实施规划/冻结提交 | 统一依赖、WBS、Owner、门禁、决策卡及文档编制顺序 | **本文件形成候选** | Human 审批正式设计与实施准入，未做则不能开始正式功能代码 |

**状态术语**：`DESIGN_CANDIDATE` 表示已有可审阅结构/合同，不等于逐行高保真原文追踪、版本固化和技术实测。缺正式 DOCX 和安全技术证据时必须显式保留 `CONDITIONAL`。

## 2. 产品实现工作包与依赖（只是计划）

| WBS | Owner 类型 / 产品仓库 | 必须已有的输入 | 设计完成后拟实现的客观交付物 | 出口证据（不能只看编译） |
| --- | --- | --- | --- | --- |
| W00 | Cloud Architecture / HyRemote | V15/V8 原文件 + D1-D4 评审 | 条款→模块→接口→状态→验收→冲突/变更 ID 一一追溯、接口/版本选择记录；全产品正式冻结签收包 | Human 设计审批、所有关键冲突有归属和裁决；非代码 |
| F10 | Framework Host/Security / HyRemote | W00、H-02a / D1 HMI Policy 合同 | `DeploymentAdmission` 和可信 Target/Scope/PolicyProvider，listener 前 fail closed、可信来源/版本 | 无有效部署不能 bind、Host 现场优先、Qt5/6 适用代码/负测 |
| F11 | Framework Session/Input / HyRemote | F10、D1 per-session Grant 语义 | AccessSession Registry、Scope Arbiter、Qt 投递前最终栅栏、held-state 不串 peer/协议 | 撤权/两人争 Scope/GUI 排队竞态及旧回调测试 |
| F12 | Framework Frame/Privacy / HyRemote | F10/F11、独立设备输出合同 | 共享基础 capture、per-session Filter/Watermark/Full/Delta、撤权清未发缓存 | 两 peer 不同权限遮罩、慢端、旧 Full/Delta 不泄漏 |
| F13 | Framework RFB / HyRemote | F10-F12、#158 真实修复与验收计划 | 现有标准 RFB3.8 的宿主授权接入、legacy 输入与 Scope 兼容、真实安全档位/资源 | 标准第三方 Viewer 与旧应用兼容；无 Native 私有消息、非公网明文资格 |
| F14 | Framework Native / HyRemote | W00 Native A/B 逐条签收、F10-F12、合格 TLS 身份材料 | 真正**独立完整 Native1.0** Listener/Parser、TLS 用户身份、Target/View/Control/Frame/Input/安全拒绝 | 正/异常消息矩阵、真 DeviceTLS 与 Host Grant/Qt 最终栅栏、RFB-only/Native-only/Dual |
| F15 | Framework four integrations / HyRemote | F10-F14、ADR-0007/0008 Kit 约束 | C++/QML/Generic/QPA 一套共享 Runtime 的可配置/可观察接入、打包插件兼容 | 同期 Qt5/6 Windows exact cell，公共 API/ABI 与已发布部署回归 |
| V10 | View Direct RFB / HyRemoteView | W00 与 View G3真实结论，F13 标准端点 | 产品级 bounded RFB client、独立 Direct、失败关闭与存储/恢复 | 真实维护中 Host/Viewer 像素、输入/持键/恶意消息/断网测试 |
| V11 | View Qt Quick UI / HyRemoteView | V10 的合法帧 / D2 稳定 Qt Quick 模型 | RemoteSurface/QSG、frame lifecycle/damage/resize/DPR、焦点、输入映射、可证明帧龄/反馈 | Qt6.8 Windows/Linux 按支持单元 FPS/P95/RSS/正确性；G3 PoC 不能直接冒充 |
| V12 | View Device Trust/Native / HyRemoteView | F14 真 HMI Native/TLS & Annex A/B、D2 TrustStore | 同一 View 中独立 Native client、安全验证、TargetList/View/ControlGrant，E2E 可信 Direct | 伪装设备/证书变化/撤权/重连负测；无旧 Grant 或业务重放 |
| V13 | View Identity/Workspace / HyRemoteView | V10–V12、D2 交互批准及安全资料 | DeviceCatalog/ProfileStore/SecretStore/Connection/Session UI、诊断/品牌/更新 | 离线首启、身份分离、旧画面遮蔽、OEM/凭据/升级独立真实测试 |
| E10 | Factory View Edge / HyRemoteView | W00、D3、View 同产品安装治理 | SiteRegistry/EndpointBinding/管理员共享、GatewayLink/RouteAdmission、FIFO relay、默认 OFF/撤权 | 工厂只装 View、被动局域网只拨白名单、站点 ACL/权限不串、Direct 不退化 |
| E11 | Factory Edge lifecycle / HyRemoteView | E10，OS 服务真实取舍 | 首批准入的用户登录态运行；获批后可增可验证的同产品 service 角色 | GUI 关闭/注销/重启/升级/卸载/IPC ACL/OS 凭据逐项合格，无假“无人值守” |
| G10 | Optional Gateway / 独立交付责任 | W00、D3 Control IDL 和**Human 对 #190 明确激活** | Identity/Site/Edge Registry、RoutePermit + one-shot Pair、Opaque Relay、tenant/audit | 自托管证书/网络隔离/攻击负测/配对重放/HA 目标按获批范围 |
| G11 | Gateway Ops / Gateway owner | G10 + 批准的运维范围 | 最小部署/升级/备份/密钥轮换/策略迁移/脱敏诊断 runbook | 客户真实自托管环境可复现恢复/回退且不复活撤权 |
| X10 | Whole-system E2E / Security + QA | F14 + V12 + E10 + G10，D3 E2E 目标 | 真 HMI↔View DeviceTLS + Edge/Gateway opaque route，12 Edge + 20 产品场景追溯 | 路由 READY≠Grant、无明文、现场优先、撤权/在途/慢端/多设备真实测试 |
| T10 | Tool maintainer / HyRemoteTool | **Human Tool vs View 职责决定**，现有 Tool S1 事实 | 当前已发布的 Tool 基础 RFB 回归/维护；S1 全量依 #586 仍待验收，SDK/Qt Kit Inspector/诊断/可复制四路线；是否复用 RFB client 单独资格 | 正式 Tool 版本/Qt5/6 Windows 工具兼容、脱敏报告、绝不虚构 Native |
| X11 | Product packaging / 各产品交付 Owner | F15、V13、E11、G11、T10 适用范围 | 安装/更新/卸载/密钥/迁移/产品级版本表/许可证/SBOM、精确平台适用矩阵 | 正/负安装/升级/回退、只装 View、SDK Kit 与 QtPrivate 精确隔离 |
| X12 | Release QA / Human 产品/发布治理 | X10/X11 + 已批准支持范围 | 独立产品发布申请、客户支持矩阵与风险接受 | 与 #95 版本治理衔接、真实所有 Gate PASS，Human 另行批准 |

**Owner 说明**：这些是未来产品**责任类型**，不是已经有实际 Local Worker ACK、不是自动派发、也没有预约版本号。Cloud Owner 当前仅有**设计审查和计划权限**，不因 WBS 排好队就打开 G10 产品公网服务。存在跨不同产品仓库的 PR/测试时按各仓真实保护规则独立签收。

## 3. 关键依赖有向图：先完整设计，再在获批范围中并行实施

~~~mermaid
flowchart TB
  B["W00 正式条款追踪 / Human Gate-D 设计冻结"] --> I["Human Gate-I 正式实施授权"]
  G3["HyRemoteView G3 独立 PoC 证据，不必等 Gate-D 才验证"] --> I
  I --> F10["F10 Host DeploymentAdmission / Policy"]
  F10 --> F11["F11 AccessSession / Input Gate"]
  F10 --> F12["F12 Per-session Output Privacy"]
  F11 --> F14["F14 Full Native 1.0 DeviceTLS"]
  F12 --> F14
  F11 --> F13["F13 RFB Host Policy"]
  F12 --> F13
  I --> V10["V10 Direct RFB Product Client"]
  V10 --> V11["V11 RemoteSurface / UI"]
  F14 --> V12["V12 Native Trust / Grants"]
  I --> V12
  V11 --> V13["V13 Device/Identity/Diagnostics"]
  V12 --> V13
  I --> IDL["Stage05 IDL/安全审查 + #190 独立激活"]
  IDL --> E10["E10 Factory View Edge"]
  IDL --> G10["G10 Optional Gateway"]
  E10 --> X10["X10 真 HMI Native E2E / Security"]
  G10 --> X10
  V12 --> X10
  F14 --> X10
  X10 --> X11["X11 View/HMI/Edge/Gateway 安装升级矩阵"]
  V13 --> X11
  F13 --> X11
  X11 --> X12["X12 对应产品发布准入 / Human"]
  HT["H-T01 Human Tool 产品分工"] --> TOOL["T10 Tool 产品主线，既有基础 RFB 维护，未完成 S1 按原授权治理"]
  TOOL --> TQ["Tool 独立安装/版本/质量 Gate"]
~~~


**说明**：箭头代表被批准的未来实施依赖，而非现在自动启动代码。**G3 PoC 可以独立于 Gate-D 设计冻结开展技术验证；Gate-I 实施授权必须同时审查 Gate-D 与适用的 G3/安全证据**。F13 与 F14 可在共同 HMI Policy/Frame 基础后并行；V10 可基于现有标准 RFB3.8 在获得 Gate-I 后与 F13 并行设计/研发；Factory Edge/Gateway 可在 Stage05 IDL 审核与 #190 独立产品激活后并行实施，不必等真 Native Viewer 完成才设计 FIFO relay，但 X10 的真实 E2E 必须同时具备 F14/V12/E10/G10。Tool 已发布的基础 RFB 回归维护可按独立产品路线继续；未完成 S1 遵循 #586 及 Human 未来产品分工，新产品重组需 Human 先选，**Tool 不成为 View/HMI/Gateway 的强制打包依赖**。Stage05 S05-A..I 细粒度计划继续有效，不重写控制 wire。

**不得用“Gateway 已启动”倒推 HMI Native 或 View 资格；不得以“PoC 编译通过”倒推正式客户端授权。**

## 4. 全产品四个不同准入门槛

### Gate-D：详细设计冻结申请（当前目标，不等于已通过）

- [ ] V15/V8 Human 批准的**原始** DOCX/等效可信完整文本可审计；Native A/B type/field/bounds/negative vector 一一比对并附 digest，权威清晰。
- [x] 已有六类 Framework 模块、三 View 模块、Edge/Gateway、Tool、部署/测试的设计候选和跨模块审查基础。
- [ ] 全部重大冲突（特别 Tool vs View、RFB Scope/个人身份、Dual 启动、撤权 in-flight）有 Human 决定或经批准的条件处置，错误/权限状态跨模块互相一致。
- [ ] 关键复杂图（组件、两协议、信任、撤权、状态机、数据流、安装）与消息/接口/字段矩阵内容审定，无平行正式规范。
- [ ] 真实验证前置的设计假设有**可证伪**实验/失败回退，未经验证的容量和 SLA 明确“候选”。
- [ ] Human 明确发出 `DETAIL_DESIGN_FROZEN` 或带条件设计批准；只有此后实施 Gate 另行判断。

### Gate-G3：正式研发之前的技术可行性证据

HyRemoteView #5 仍是 `NOT PASSED`。RFB Direct #9 Windows PoC 只有部分证据，Linux 同项目适用单元未取得；Native TLS Trust/Grant、安全凭据、Qt Quick 1920×1080 帧率/延迟/资源、许可证/可交付性均要按真实执行环境有 exact-SHA 证据。G3 **不得被 D1-D5 纸面设计或 PR CI 替代**。

### Gate-I：正式产品研发准入

仅当 Gate-D、适用 Gate-G3/安全/接口和独立 Human 研发激活全部满足，才能在已批准代码范围创建正式 View/Native/Gateway 产品树。**现阶段不构建/修改正式产品、不开生产公网监听、不新开发布 train。**

### Gate-R：版本与支持声明

每个产品及已批准平台 Kit 的真实 CI/物理 Windows 或适用 Linux/嵌入式、维护中 Viewer/安全 E2E/升级回退、许可证、发版权限都要单独证据。GA、Post-GA、Gateway #190 激活与发行均有独立审批；任何 `DESIGN_PASS`、`G3_PASS` 均不推出 `RELEASE_PASS`。

## 5. 设计→验收追踪矩阵（当前覆盖与缺口，非正式条款签收）

| 设计源域 | 设计包 / 合同 | 建议正/负证据 | 当前真相 |
| --- | --- | --- | --- |
| HMI Framework/四接入 | D1 Boundary + Consistency，XC01..14 | FW01..12 / XP01、XP04–06 | 候选已审，Native/Grant 实现未批准 |
| 标准 RFB | D1 Protocol，DP01..06 | RFB strict 3.8/Raw/TRLE 与传统 Viewer/Scope/场景 | 当前 RFB 代码存在，精确版本真实测试仍需专项 |
| Native A/B + DeviceTLS | HMI design05 + D1 Protocol | DP07..14、HMI01..08；原文 21 类和错误集 | **正式 A/B 源未核对，端到端未实现** |
| View Direct/RemoteSurface/Identity | D2 VIEW01..14 | XP02–03、XP07、XP13、XP16–17 | G3 未通过，无 G4 产品代码 |
| Factory Edge / Gateway | Stage04/05 + D3 MG01..16 | EDGE01..12 / XP08–13、XP18 | 设计候选；#190 未激活 |
| Tool SDK/诊断 | Tool PRD + D4 | XP14–15，真实 SDK/Qt Kit 与脱敏报告 | 已有基础 RFB 产品基线；完整 Tool S1 的 #586 仍 OPEN；新版产品边界需 Human |
| 安装/安全/质量 | D4 X01/X02 | XP01..20 全域 + 环境/证据表 | 设计矩阵已成；未运行全产品 E2E/产品资格 |

正式 V15/V8 的条款 ID 不是从 Issue 目录数量推导；以获批准的原 Word 真实章节/表格为准建立 `RequirementID→ModuleID→ContractID→State/Failure→TestID→Owner→EvidenceSHA`。对每一项需记录 `COVERED/CONFLICT/OPEN/NOT_APPLICABLE` 及理由，不能以“共有 20 个 XP”证明 100% 条款覆盖。

## 6. 高风险与回退：研发前必须决策的事实

| RiskID | 风险和可逆策略 | 决策/门禁 |
| --- | --- | --- |
| R-01 | 旧 RFB 匿名/共享口令不提供 Native 个人身份，却要求单 Scope 控制独占；以 Host 显式授权 + 默认只读 + RFB 标准兼容测试作为候选；不修改 wire | H-RFB（如影响已交付用户权限需 Human） |
| R-02 | Dual 任一 listener 启动失败可能出现用户误认“全可用”；优先原子绑定需全部成功，对运行中故障可只标部分不可用但绝不弱协议回退 | H-DUAL |
| R-03 | 撤权时已进入 TCP/TLS/内核/远端显示的数据不可撤回；不产生新的旧权限发送，清仍可控制的队列，必要时断链，View 遮蔽 | H-REVOKE，Human 对安全承诺范围接受 |
| R-04 | Tool 与 View 双官方访问 UI，长期双安全与客户支持成本；优先保留已发布 Tool 基础 RFB、未完 S1 保持真实规划，未来完整 View 归 View | H-T01（明确 Human） |
| R-05 | 证书/TrustBundle 来源不可审计、自托管/外网替代身份；安全失败关闭，DeviceId HMI 独立验证，不因 Gateway 成功登录解锁 | V8 原文核对 + 安全审查 |
| R-06 | Edge 用户注销后被误宣称无人值守在线；首个可验证级别只登录期，真正服务模式通过 Windows SCM/ACL/升级资格后另发布 | H-EDGE（产品目标级别） |
| R-07 | 现有 D1/D2/Stage05 只引用 Native V8 Annex 派生材料，不能形成 wire freeze | H-BASELINE（权威原始输入） |
| R-08 | 128MiB HMI 目标/8 route/256KiB 测试参数被当客户 SLA；任何门禁须真实 per-process/负载/平台证据，未测不支持 | G3 / 性能 / 实机 Gate |

## 7. 向 Human 提交的单次决策卡（只需要真正产品裁决）

下列是**尚未取得新批准的建议**，并非在文档中代替 Human 选定：

| 决策 | 推荐 A / 影响 | 替代 B / 成本 | Human 决定格式 |
| --- | --- | --- | --- |
| **H-T01 Tool / View 长期分工** | **A** 保持 Tool 已发布基础 RFB 访问的可用性与维护，S1 全量仍待 #586 验收并按新产品边界协调；未来 Native/Managed/官方完整工作区主线归 HyRemoteView；Tool 新投资 SDK/Kit/诊断，避免双研发 | **B** 规划 Tool Remote 逐步退役/迁移到 View，需要用户兼容与支持期限；或 C 永久双全功能，成本更高 | `H-T01=A/B/C` |
| **H-DUAL RFB-only/Native-only/Dual 启动失败语义** | **A** 要求的协议端点在新架构统一受信 Admission 后做受控原子启动：任一必须项失败不发布部分成功；运行期局部故障只关受影响协议连接并明示 | **B** 宿主显式声明可降级的 required/optional Profiles，仅在不削弱安全/授权情况下容许局部服务，需要更多 UI/运维测试 | `H-DUAL=A/B` |
| **H-REVOKE 设备撤权对在途内容的安全承诺** | **A** 自新 Policy/GrantEpoch 生效点之后不产生新的旧政策输出；清本进程可控制缓冲，必要时断链，View 立即遮蔽旧画面；明确已发送/已展示不能物理撤回 | **B** 要求更强在途可达性约束，需要额外主动终止/协议和硬件实验且仍无法撤回已显示内容；若产品文案暗示物理撤回须修正文案 | `H-REVOKE=A/B` |
| **H-EDGE 首批工厂后台可用级别** | **A** 首先认证用户已登录期间 Edge 在线，退出/注销真实离线；无人值守 Windows service 作为后续独立可验收能力，不额外安装软件 | **B** 首批必须注销后在线，要求更强 OS 账户/安全/更新实际证据并改变最短可交付路径 | `H-EDGE=A/B` |
| **H-RFB 传统 RFB 用户控制权的未来映射** | **A** 保留已交付的客户明确启用远程输入的实际可用行为；新架构要求 Host 显式批准 Scope/Controller，默认只读，不把共享密码当个人身份；动态细粒度身份用 Native | **B** 无可证明个人身份的 RFB 连接一律只能看不能控制，会影响既有使用权益，需要迁移/版本治理 | `H-RFB=A/B` |

**正式 DOCX 输入**：请在冻结前提供可核对的正式 `HYR-PD-001/V15` 与 `HYR-ARCH-001/V8`（特别原始附录 A/B），或其当前可访问的可信位置；要求与已归档 digest 对齐。单纯截图/历史转述/Issue 中引用的十几条字节结构不足以宣称完整 wire 追踪。

**审批流程**：若 Human 同意上述方向，也不自动等于 `G3_PASS`、`GATE-D_FINAL_APPROVED` 或正式研发激活。先对原文逐条追溯与最终接口、测试风险差距签收，再申请**独立** `DETAIL_DESIGN_FREEZE`；之后才按真实资格和已批准版本范围启动实现。

## 8. 正式《模块详细设计与实施规划》Word 的先行结构设计（暂不制成 Word）

用户明确要求**先目录、内容、图表、表格、版式规划及审查后，再落地编制正式 Word**。推荐一个主文档 + 少量**受控技术附录引用**（Native wire 仍以原 V8 A/B 为唯一规范），不复制数份同一协议。

| 章节建议 | 内容范围 | 主要图/表（先审内容，后编排） |
| --- | --- | --- |
| 封面、文档控制、版本与批准 | 正式编号、来源 digest、适用产品、修订历史与审批 | 文档信息页、自动目录 |
| 1 目标与上位权威 | V15/V8、ADR、版本/Source SHA、设计与实现边界 | 条款交叉索引表 |
| 2 产品模块与依赖 | 14 模块、四产品责任、依赖方向 | 总体模块架构图、职责 RACI |
| 3 Framework Core/Runtime | 会话、采集、四入口、QtPrivate 例外、资源限额 | Core/Runtime 结构图、生命周期表 |
| 4 RFB 与 Native | 两独立设备协议、不重复 wire、互操作与安全 | 协议对照表、状态机、Capability 表 |
| 5 设备端可信准入与 Grant | DeploymentAdmission、Target/Scope、控制独占、隐私、撤权/最终输入 | 安全信任图、Grant/撤权时序 |
| 6 HyRemoteView 客户端 | DeviceCatalog、ByteStream、RFB/Native、QtQuick、渲染/输入、身份凭据 | UI/Session/Render pipeline、状态表 |
| 7 Factory Edge / Gateway | 内嵌安装、站点管理、Route Permit/Pair、FIFO、自托管与生命周期 | End-to-end sequence、Trust boundary、ACL matrix |
| 8 HyRemoteTool / 安装交付 | Tool 角色、SDK/Qt Kit、报告、各产品安装/更新/回滚 | 产品职责/兼容矩阵 |
| 9 全产品非功能与资源 | 性能预算、CPU/帧龄/内存、可观测、隐私/安全分层 | per-process memory budget、风险表 |
| 10 一体化验证与验收 | XC/FW/DP/VIEW/MG/XP ID 映射、环境、正负例、状态 | Trace matrix / Gate evidence |
| 11 实施 WBS/Owner/DAG | 本文 W00..X12、Owner、并行/依赖、独立 Gate | 工作包表、关键路径图 |
| 12 未决决策与变更 | Human 卡、取舍、已接受风险、排除项、后续修订 | Decision register |
| 附录 A 规范引用与术语 | **引用** V8 Native 原有 A/B，不复制/改号；其他受控设计 URL 与 digest | Provenance, glossary |

**版式建议，仅供审批**：A4 纵向、中文正文宋体小四、章节标题样式、自动目录/图表目录与交叉引用、图不跨页断裂、表头跨页重复/主要合同表尽量一页，页眉页脚和页码一致；正文图表优先 Mermaid/矢量源核对、再转换为 Word 嵌入图；先校对所有标题/数据/决策后再处理封面和分页。沿用 V15/V8 文档实际获批准的视觉体系，不单独创造新模板/版本。**此阶段不生成 Word、PDF或发布文件。**

## 9. 最终自检与下一动作

- [x] 14 模块有一份可追溯的总体目录、分模块设计候选和实施依赖计划，不重复生成 Gateway/Native 平行规范。
- [x] 四产品职责、两设备协议、Direct 独立、设备端最后授权、现场优先、Factory View 一个安装已贯穿。
- [x] Tool 旧审批与新 View 产品重叠、RFB legacy identity、Dual 启动、撤权在途、Edge 登录/无人值守被标为**Human 产品问题**，未自行宣布裁决。
- [x] 明确各 WBS 的依赖/Owner 类型/交付物/验证出口，避免“任务清单=已下发”。
- [x] 保留 G3、Native E2E、Edge12、真实安装/支持资格和 Gateway #190 版本激活为独立 Gate，均未被设计 PR CI 取代。
- [ ] V15/V8 原文 digest、全部要求逐条、Native A/B 正文与负向向量的复核和最终评审。
- [ ] Human 选择/批准真正的产品范围与安全承诺、正式模块设计冻结及实施准入。
- [ ] 一切产品代码、实机/资格、全产品发布尚未由本计划启动。

**状态结果**：`D5_PLAN_DRAFT=COMPLETE_FOR_REVIEW`，`DESIGN_COVERAGE=CANDIDATE_FULL_DOMAIN`，`NORMATIVE_TRACE=PENDING`，`HUMAN_DECISION_REQUIRED=YES`，`DESIGN_FREEZE=NO`，`IMPLEMENTATION_AUTHORIZED=NO`。

**下一动作**：提交唯一 Human 选择包和正式 Word 源材料核对需求；Human 尚未参与前，可完成审查/CI/合并候选，但**不得越过 Gate-D 和 Gate-I**。取得 Human 实质决策后再按真实原文作最终一致性修订与目录/图表/版式签收，之后才申请冻结。
