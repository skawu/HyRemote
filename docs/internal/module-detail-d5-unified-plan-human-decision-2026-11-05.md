# D5｜HyRemote 全产品统一实施规划、设计冻结准入和 Human 决策卡（候选）

> **2026-11-05 Human 决策追加**：H-T01/H-DUAL/H-REVOKE/H-EDGE/H-RFB **全部 APPROVED_A**，以 [总控 #843 审批记录](https://github.com/skawu/HyRemote/issues/843#issuecomment-6646988698) 为权威；五项无需再反复讨论。仅产品设计取舍获批，V15/V8 原始全文核对、设计冻结、G3/安全验证及正式研发另设 Gate。

> **设计总控**：[HyRemote #843](https://github.com/skawu/HyRemote/issues/843)，覆盖 14 模块 F-01..F-06、V-01..V-03、E-01、G-01、T-01、X-01/X-02。  
> **设计前提**：Human 已批准产品设计 `HYR-PD-001/V15` 和架构 `HYR-ARCH-001/V8`；已合并 D1 Core/Runtime/四接入/设备授权、RFB/Native 独立协议、D1 横向一致性候选，以及 D2 客户端、D3 Managed、D4 Tool/交付设计候选。Stage04/05 已有详细 IDL/DAG 不重编。  
> **法律/工程效力**：`DESIGN_CANDIDATE / IMPLEMENTATION_WBS_ONLY / NO_CODING_ORDER / NO_PRODUCT_ACTIVATION / NO_RELEASE_AUTHORIZATION`。**设计包合并不是 Human 全产品详细设计冻结，也不是 G3/G4/G5、Gateway #190 或任何产品版本激活。**

### 上位文件来源核验（Human 已提供等效完整正文）

| 受控输入 | 现有审批记录提供的 SHA-256 | 当前全文读取 | 冻结前需核对 |
| --- | --- | --- | --- |
| `HYR-PD-001/V15` | 正式版 SHA `d0408f333d4b9aec186067cd159ff8a4a2206ef66d58243fd4c5cd10ffb82cf1`；实际读取送审稿 SHA `b753ebc69bbed47c2d8761f2eaf5f616001fb3e6ae32cff42f432b8930084b6b` | `REVIEW_DRAFT_HASH_MATCH / HUMAN_CONTENT_EQUIVALENCE` | V15 56 条与 V8 附录 D 56/56 匹配；行为追踪仍未冻结 |
| `HYR-ARCH-001/V8` | 正式版 SHA `1101d5546cfc0ab63e4cfde809ba4b0530e1048ed3760b56b8c9e7be3de6803a`；实际读取送审稿 SHA `3d89a90559ae62852538e919b7f0ad2188f54806ef13d806a095474dccce5824` | `REVIEW_DRAFT_HASH_MATCH / HUMAN_CONTENT_EQUIVALENCE` | V8 99 小节、A/B 21 type/fields/length 可读；Human P-A/A 已补准数值，真实负例仍 NOT_RUN |

**已取得正文等效依据**：Human 上传完整 V15/V8 送审稿并明确确认“内容一样”；真实 SHA-256 逐一与 #827 历史**送审稿**完全匹配，**不是正式版文件二进制摘要**。结构审计 V15 56、V8 99、附录 D 56/56、Native 21 已完成，详情见 [受控源审计](module-detail-source-audit-v15-v8-2026-11-05.md)。行为级追踪与实际测试仍不得标 `NORMATIVE_TRACE=PASS`（Native 数值 P-A/A 已获批）。**Tool 当前已发布的基础 RFB 访问不等于全量 S1 已通过验收；[#586](https://github.com/skawu/HyRemote/issues/586) 仍 OPEN**。


**W00 2026-11-05 追溯工作增量（非冻结）**：[V15 表6 **31/31** 源场景→[D4 XP01..28 设计用例](module-detail-w00-v15-table6-xp-coverage-2026-11-05.md) 的交叉追踪已补，补充 XP21..28 用于原 20 项场景未明确覆盖的真实用户结果；**只完成规格设计、全部 NOT_RUN**。

**W00 2026-11-05 上位要求索引**：已生成 [V15 56 条行为/失败/表6场景映射](module-detail-w00-v15-behavior-trace-2026-11-05.md)、[V8 99 个 Heading2 详细设计主责索引](module-detail-w00-v8-architecture-contract-index-2026-11-05.md)，以及 [Native 21 类消息正/负向向量与 WIRE-V01..V07 严格差距](module-detail-w00-native-wire-contract-gaps-2026-11-05.md)。56 条仍是 `MAPPED_NOT_TESTED`，99 节仍是 `INDEXED_CONTRACT_REVIEW_PENDING`，Native 测试仍是 `NOT_RUN`。原先 `SOURCE_TENSION_01` 的 V8 TLS-first/plain 歧义**已经 Human 选择 A**，P-A 具体数值亦已批准；此前 [待批原提案](module-detail-w00-native-wire-value-proposal-2026-11-05.md) 仅保留审签历史。当前唯一规范补充见 [Native V8 approved delta](module-detail-w00-native-v8-delta01-approved-2026-11-05.md)。无产品代码/服务激活。

**Native 规范数值及安全档位审批已落实（2026-11-05）**：Human 明确批准 `NATIVE-V8-WIRE-DELTA-01=P-A; SOURCE_TENSION_01=A`（[#843 决策](https://github.com/skawu/HyRemote/issues/843#issuecomment-6648340105)）。唯一生效的 [V8 A/B 数值增补](module-detail-w00-native-v8-delta01-approved-2026-11-05.md) 已将先前 WIRE-V01..V06 数值、plain/tls-password 两档统一 TLS-first 正式绑定。基于此完成 [Native 21 类／26 个正向黄金字节＋12 个负向规格](module-detail-w00-native-v8-golden-vectors-design-2026-11-05.md)；**全部仍为设计数据，未测试**。此前 [P-A 候选](module-detail-w00-native-wire-value-proposal-2026-11-05.md) 只作历史取舍记录，不再重复申请 Human 批准。Gate-D/Native E2E/Gate-I 仍待独立准入。

**W00 Native 会话/授权原子合同增量（2026-11-05）**：在 Human 已批准的 P-A/A 基础上，已将 [F-05 Native 的五条正交状态轴、消息准入、撤权竞态及 AFS-01..12](module-detail-d1-device-protocols-2026-11-05.md) 设计为明确的 Host/Viewer 共同验收合同，并在 [D2](module-detail-d2-hyremoteview-client-2026-11-05.md) 与 [D4](module-detail-d4-tool-delivery-quality-2026-11-05.md) 逐项标注消费与测试责任。该增量不添加 wire 字段/错误码/新产品能力；**12 项 AFS 全部 DESIGN_ONLY**，旧 XP 和 Native E2E 仍未测试。

## 1. 现在的真实阶段：设计接近全域覆盖，但不能宣布冻结

| 阶段 | 交付范围 | 当前设计状态 | 未能自动越过的门禁 |
| --- | --- | --- | --- |
| W00 源结构/行为初步追踪 | 14 模块 #843 + [来源审计](module-detail-source-audit-v15-v8-2026-11-05.md) + V15 56 行 + V8 99 行 + Native 21 类负向规格 | **送审稿 SHA 匹配；56 行行为/失败映射、99 节主责索引形成，未验证实现** | 细分原子合同与状态/错误/测试/Owner，Wire 数值已获 Human P-A/A 批准；剩余状态、异常测试与真实互通仍未完成 |
| D1 Framework | F01–F06：Core、Runtime、四入口、RFB/Native、Host 安全 | **既有 D1 候选 + F-02 RT-01..08 / F-06 POL-01..08 细化** | 需审签 Qt 最终投递线性化、逐 Session 过滤/缓存所有权及技术失败回退；真实产品验证不算 Gate-D 的前置 PASS |
| D2 HyRemoteView | V01–V03：完整 Direct、QtQuick、Trust、设备/凭据/诊断/OEM | **设计候选** | G3 #5 **NOT PASSED**、真实 Native DeviceTLS/Frame/Grant、源条款对照 |
| D3 Edge/Gateway | E01/G01：同安装 Edge、受管控制/字节隧道、ACL/自托管 | **复用 Stage04/05 并形成跨模块候选** | #190 **NOT_ACTIVATED**、control IDL 版本、OS 后台安全、真实 Managed E2E |
| D4 Tool/交付/安全 | T01、X01、X02：Tool 角色、安装/Qt Kit、全系统验收 | **设计候选** | **H-T01=A 已批准**，不再重开分工；Tool S1 #586 OPEN，支持矩阵实证尚缺 |
| D5 实施规划/冻结提交 | 统一依赖、WBS、Owner、门禁、决策卡及文档编制顺序 | **本文件形成候选** | Human 审批正式设计与实施准入，未做则不能开始正式功能代码 |

**状态术语**：`DESIGN_CANDIDATE` 表示可审阅结构/合同，不等于完整行为追踪或技术实测。送审稿历史 SHA 已核且 Human 确认正文等效；正式版二进制未逐字节对比，Native 数值绑定/负例未签收，仍为 `CONDITIONAL`。

## 2. 产品实现工作包与依赖（只是计划）

| WBS | Owner 类型 / 产品仓库 | 必须已有的输入 | 设计完成后拟实现的客观交付物 | 出口证据（不能只看编译） |
| --- | --- | --- | --- | --- |
| W00 | Cloud Architecture / HyRemote | V15/V8 原文件 + D1-D4 评审 | 条款→模块→接口→状态→验收→冲突/变更 ID 一一追溯、接口/版本选择记录；全产品正式冻结签收包 | Human 设计审批、所有关键冲突有归属和裁决；非代码 |
| F10 | Framework Host/Security / HyRemote | W00、H-02a / D1 HMI Policy 合同 | `DeploymentAdmission` 和可信 Target/Scope/PolicyProvider，listener 前 fail closed、可信来源/版本 | 无有效部署不能 bind、Host 现场优先、Qt5/6 适用代码/负测 |
| F11 | Framework Session/Input / HyRemote | F10、D1 [F-06 POL-01..08](module-detail-d1-framework-boundaries-2026-11-05.md) 设计合同 | AccessSession Registry、Scope Arbiter、Qt 投递前最终栅栏、held-state 按来源计数不串 peer/协议 | **实施后**验证撤权/两人争 Scope/GUI 排队竞态及旧回调；设计阶段审签 PolicyEpoch 与 Qt 投递线性化 |
| F12 | Framework Frame/Privacy / HyRemote | F10/F11、D1 [F-02 RT-01..08](module-detail-d1-framework-boundaries-2026-11-05.md) 设计合同 | 共享基础 capture、per-session Filter/Watermark/Full/Delta、撤权清未发缓存 | **实施后**实测两 peer 权限遮罩、慢端、旧 Full/Delta 不泄漏；设计阶段核准过滤顺序、缓冲所有权与 epoch 失效 |
| F13 | Framework RFB / HyRemote | F10-F12、#158 真实修复与验收计划 | 现有标准 RFB3.8 的宿主授权接入、legacy 输入与 Scope 兼容、真实安全档位/资源 | 标准第三方 Viewer 与旧应用兼容；无 Native 私有消息、非公网明文资格 |
| F14 | Framework Native / HyRemote | 已批准 P-A/A 唯一值源、W00 AFS-01..12 状态/错误合同、F10-F12、合格 TLS 身份材料 | 真正**独立完整 Native1.0** Listener/Parser、TLS 用户身份、Target/View/Control/Frame/Input/安全拒绝 | **Gate-I 后**按 26 字节黄金向量/12 异常变异与 AFS-01..12 逐项实际对拍、真 DeviceTLS、Host Grant/Qt 最终栅栏及 RFB-only/Native-only/Dual；当前这些真实 E2E 未运行 |
| F15 | Framework four integrations / HyRemote | F10-F14、ADR-0007/0008 Kit 约束 | C++/QML/Generic/QPA 一套共享 Runtime 的可配置/可观察接入、打包插件兼容 | 同期 Qt5/6 Windows exact cell，公共 API/ABI 与已发布部署回归 |
| V10 | View Direct RFB / HyRemoteView | W00 与 View G3真实结论，F13 标准端点 | 产品级 bounded RFB client、独立 Direct、失败关闭与存储/恢复 | 真实维护中 Host/Viewer 像素、输入/持键/恶意消息/断网测试 |
| V11 | View Qt Quick UI / HyRemoteView | V10 的合法帧、D2 `SURF-01..08` 已审候选合同、精确 Qt Quick 线程边界 | RemoteSurface/QSG、immutable FrameSnapshot / DisplayEpoch、frame lifecycle/damage/resize/DPR、Focus 与输入映射、帧龄/反馈 | **Gate-I 后**在 Qt6.8 Windows/Linux 实际支持单元验证 FPS/P95/RSS/正确性；G3 PoC 不能直接冒充正式产品 |
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
  HT["H-T01=A 已批准的 Tool / View 分工"] --> TOOL["T10 Tool 产品主线，既有基础 RFB 维护，未完成 S1 按原授权治理"]
  TOOL --> TQ["Tool 独立安装/版本/质量 Gate"]
~~~


**说明**：箭头代表被批准的未来实施依赖，而非现在自动启动代码。**G3 PoC 可以独立于 Gate-D 设计冻结开展技术验证；Gate-I 实施授权必须同时审查 Gate-D 与适用的 G3/安全证据**。F13 与 F14 可在共同 HMI Policy/Frame 基础后并行；V10 可基于现有标准 RFB3.8 在获得 Gate-I 后与 F13 并行设计/研发；Factory Edge/Gateway 可在 Stage05 IDL 审核与 #190 独立产品激活后并行实施，不必等真 Native Viewer 完成才设计 FIFO relay，但 X10 的真实 E2E 必须同时具备 F14/V12/E10/G10。Tool 已发布的基础 RFB 回归维护可按独立产品路线继续；未完成 S1 遵循 #586，**H-T01=A 已正式批准未来产品分工**。Tool 不成为 View/HMI/Gateway 的强制打包依赖。Stage05 S05-A..I 细粒度计划继续有效，不重写控制 wire。

**不得用“Gateway 已启动”倒推 HMI Native 或 View 资格；不得以“PoC 编译通过”倒推正式客户端授权。**

**本轮以既有文档为唯一交付载体**：D1 F-02 新增运行时生命周期、线程/Target/Frame/Session/QtTicket 所有权、per-session 输出和缓冲背压 `RT-01..08`；D1 F-06 新增 PolicySnapshot 生效、Qt 最终投递/现场撤权串行化、按 Session held ledger `POL-01..08`。二者是会影响研发实施的**实质模块技术合同**，不是新增 W00 追溯文档。与 D2 View 失权显示、D3 透明转发及 D4 XP 设计验收的消费关系继续独立审查。技术合同候选合并不自动冻结产品或授权代码。

**V-02 RemoteSurface 模块技术合同增量（2026-11-05）**：原 [D2 客户端详细设计](module-detail-d2-hyremoteview-client-2026-11-05.md) 已补充 `SURF-01..08`：Decoder/Render 不可变 FrameSnapshot 所有权、Qt Quick thread fence、per-Target FULL/DELTA、DisplayEpoch 失效、DPR/坐标逆变换、Focus/Grant、本地 held 和像素/资源负例；D4 质量合同已定位 `VIEW/XP/AFS` 证据出口。它们是 **Gate-D 可审设计合同**，G3/产品平台/渲染性能实测未通过；不创建新产品或额外 Native wire。

## 4. 全产品四个不同准入门槛

### Gate-D：详细设计冻结申请（当前目标，不等于已通过）

- [x] V15/V8 有经 #827 历史 SHA 验证的**等效完整正文**；56/99 标题、附录 D 56/56、Native 21 个 type/字段/长度已核，正式版**二进制**未对照。
- [x] V8 A/B 原文曾空缺的数值、键码与两种 Native TLS-first 模式已获 Human P-A/A 正式批准，统一受控值源已归档；21 type／26 个正向 golden vectors 与 12 个负向规格只完成计算设计。
- [x] Human 批准的 Native 21 type / P-A 数值 / TLS-first A 已形成同一受控设计来源，26 个黄金字节与 12 个异常变异规格供未来验证。
- [ ] Host/Viewer 的解析器消息顺序、状态转移、错误归因、TLS 身份、Qt 最终输入与隐私撤权**设计合同及负例规格**必须充分签收；**不要求正式产品代码已开发或 Host/Viewer 产品实测 PASS 才能审 Gate-D**。真实互通/设备/平台测试留给适用 Gate-G3/研发后验收 Gate，不能用它们反过来阻止研发授权形成。
- [x] 已有六类 Framework 模块、三 View 模块、Edge/Gateway、Tool、部署/测试的设计候选和跨模块审查基础。
- [x] H-T01/H-DUAL/H-REVOKE/H-EDGE/H-RFB 的产品/安全语义已由 Human 正式批准 A，纳入 D1–D5 合同（不是实现证明）。
- [ ] 与正式 V15/V8 原文和 Native A/B 逐条核对的最终权限、错误与协议细节仍需技术审查。
- [ ] 关键复杂图（组件、两协议、信任、撤权、状态机、数据流、安装）与消息/接口/字段矩阵内容审定，无平行正式规范。
- [ ] 真实验证前置的设计假设有**可证伪**实验/失败回退，未经验证的容量和 SLA 明确“候选”。
- [ ] Human 明确发出 `DETAIL_DESIGN_FROZEN` 或带条件设计批准；只有此后实施 Gate 另行判断。

**Gate-D 与产品验证的边界（纠正循环依赖）**：Gate-D 审的是**已经确定、无关键自相矛盾、可由后续实现和负例验证的完整详细设计**；源规范/人批准决定、14 模块关键内部合同/状态/错误/线程与资源预算、最终目录/图表/版式内容规划、可证伪验收规格及未验证假设/失败回退必须齐备。Gate-D 不宣称任何产品代码、Windows/Linux Kit、DeviceTLS 真机、性能或 Gateway E2E 已通过；这些只能在适用的技术 PoC 和**获批实施之后**形成真实证据。**Gate-D 不得因未获 Gate-I 而尚未进行的产品测试而陷入前置循环**。对确实影响设计可行性的高风险假设，列入有边界、独立授权的 Gate-G3 PoC 条件或 Human 的有条件冻结决策，不能偷跑正式产品代码，也不能用纸面设计消除风险。

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
| Native A/B + DeviceTLS | HMI design05 + D1 Protocol + [21 消息负向规格](module-detail-w00-native-wire-contract-gaps-2026-11-05.md) | DP07..14、HMI01..08；21 message 字段/长度、值域/负例及新 TENSION | **21 类源字段已核；WIRE-V01..V06 数值及 SOURCE_TENSION_01=A 已批准；26 正向字节样例＋12 异常规格设计完成但均 NOT_RUN** |
| View Direct/RemoteSurface/Identity | D2 VIEW01..14 | XP02–03、XP07、XP13、XP16–17 | G3 未通过，无 G4 产品代码 |
| Factory Edge / Gateway | Stage04/05 + D3 MG01..16 | EDGE01..12 / XP08–13、XP18 | 设计候选；#190 未激活 |
| Tool SDK/诊断 | Tool PRD + D4 | XP14–15，真实 SDK/Qt Kit 与脱敏报告 | 已有基础 RFB 产品基线；完整 Tool S1 #586 仍 OPEN；H-T01=A 已批准，技术交付待审 |
| 安装/安全/质量 | D4 X01/X02 + [V15 表6 31 场景映射](module-detail-w00-v15-table6-xp-coverage-2026-11-05.md) | XP01..28 全域 + 环境/证据表 | 31/31 有设计验收入口；新增 XP21..28 均 NOT_RUN，未运行全产品 E2E/产品资格 |

正式 V15/V8 的条款 ID 不是从 Issue 目录数量推导；以获批准的原 Word 真实章节/表格为准建立 `RequirementID→ModuleID→ContractID→State/Failure→TestID→Owner→EvidenceSHA`。对每一项需记录 `COVERED/CONFLICT/OPEN/NOT_APPLICABLE` 及理由，不能以“共有 20 个 XP”证明 100% 条款覆盖。

## 6. 高风险与回退：研发前必须决策的事实

| RiskID | 风险和可逆策略 | 决策/门禁 |
| --- | --- | --- |
| R-01 | 旧 RFB 匿名/共享口令不提供 Native 个人身份；按 H-RFB=A 保留 Host 显式允许的传统输入、默认只读及单冲突 Scope Controller，不改 wire | H-RFB=A 已批准；实现与兼容性证据待取得 |
| R-02 | Dual 任一必需端点失败不能露出半启动；运行期单协议故障可隔离但不能默默退为更弱协议 | H-DUAL=A 已批准；原子绑定/回滚/错误证据待取得 |
| R-03 | 撤权时已到达 TCP/TLS/内核/远端显示的数据无法收回；按新 epoch 停止新增旧权限输出与清可控队列，必要时断链，View 遮帧 | H-REVOKE=A 已批准；线性化/真实负测待取得 |
| R-04 | Tool 与 View 历史功能重叠；按批准分工保留 Tool 基础 RFB 与 #586 未完成 S1 治理，新完整 Native/Managed 客户端归 View | H-T01=A 已批准；迁移/复用代码技术方案待证 |
| R-05 | 证书/TrustBundle 来源不可审计、自托管/外网替代身份；安全失败关闭，DeviceId HMI 独立验证，不因 Gateway 成功登录解锁 | V8 原文核对 + 安全审查 |
| R-06 | 工厂 Edge 注销后误显示在线；首批仅支持登录态，后台 Service 为后续同产品独立资格 | H-EDGE=A 已批准；生命周期/状态真实性仍待验收 |
| R-07 | 现有 D1/D2/Stage05 只引用 Native V8 Annex 派生材料，不能形成 wire freeze | H-BASELINE（权威原始输入） |
| R-08 | 128MiB HMI 目标/8 route/256KiB 测试参数被当客户 SLA；任何门禁须真实 per-process/负载/平台证据，未测不支持 | G3 / 性能 / 实机 Gate |

## 7. Human 已批准的决策与执行追踪（2026-11-05）

> Human 正式在项目对话确认：**H-T01=A; H-DUAL=A; H-REVOKE=A; H-EDGE=A; H-RFB=A**，登记证据：[总控 #843 决策记录](https://github.com/skawu/HyRemote/issues/843#issuecomment-6646988698)。五项状态均为 `HUMAN_APPROVED_A`，不再是待审批方案。替代 B/C 仅作为历史评估保留，只有新的受控产品变更才需重开。

| 决策 ID | 已批准的唯一产品语义 | 模块/工作包 | 还需要证明的技术行为 | 当前 |
| --- | --- | --- | --- | --- |
| H-T01=A | 保留 Tool 已发布基础 RFB 访问并维护；完整 Tool S1 #586 仍 OPEN；未来完整 Native/Managed/Edge/官方客户端由 HyRemoteView 发展，Tool 新重点为 SDK/Kit/诊断 | T01/V01-V03；T10/V10-V13 | 已发布客户端回归、Tool S1 权益不被删除，产品包独立；RFB 代码复用许可证另审 | `APPROVED_DESIGN_NOT_IMPLEMENTED` |
| H-DUAL=A | 对部署声明为**必需**的所有 RFB/Native listeners 统一 Host Admission 和原子启动；初启失败整体拒绝，运行期局部故障可隔离但不自动弱协议回退；RFB-only/Native-only 仍可合法存在 | F03/F04/F05/F06；F10/F13/F14/F15 | 端点预绑定/回滚无可观察半成功，错误源真实、同 Kit/四入口/双协议负测 | `APPROVED_DESIGN_NOT_IMPLEMENTED` |
| H-REVOKE=A | 新权限 epoch 生效后不得**新生成/选择/排队**旧授权输出/输入，清所有可控队列，必要时断开，View 遮/清旧帧；不声称撤回进入 OS/TLS/已被用户看到的字节 | F02/F06/V01-V02；F11/F12/V11/V12 | 设备端线性化点、Qt GUI 二次输入栅栏、FrameQueue ownership、TLS/socket bytes-in-flight 时序、显示/held 清理 | `APPROVED_DESIGN_NOT_IMPLEMENTED` |
| H-EDGE=A | 首批工厂 Edge 只在用户登录及产品明确启用期间在线，用户退出/注销即离线；将来可研究统一 View 安装/升级中的无人值守 Service，**不算首批支持** | V03/E01/X01；E10/E11/X11 | Windows/Linux 精确平台运行与卸载、可信管理命令、真实状态；不因 Gateway online 假称 service 在线 | `APPROVED_DESIGN_NOT_IMPLEMENTED` |
| H-RFB=A | 保留 Host 明确启用的传统 RFB 控制能力，新设计默认只读且经 Target/Scope Host policy 允许，冲突 Scope 默认一个远程控制者；共享 VNCAuth 不代表个人身份，细粒度动态 Grant 由 Native | F04/F06/V01/Tool；F11/F13/V10/T10 | 标准第三方 Viewer 互通、Host 授权/单 Controller、旧 Tool 权益、legacy 受限 UI 的真实来源/错误状态 | `APPROVED_DESIGN_NOT_IMPLEMENTED` |

**剩余真正阻断详细设计正式冻结的工作，不再是这五个取舍：**

1. **规范正文现已取得，行为追踪仍需补齐**：Human 提供送审稿历史 SHA 完全匹配的完整原文并确认内容等效，V15 56 条与 V8 附录 D 56/56、架构 99 小节、Native A/B 21 type 已核。剩余工作是逐要求映射 `Contract/State/Failure/Test/Owner` 并完成已批准 P-A/A 值域的 Host/Viewer 双端状态/安全/兼容设计审签与向量实际资格；`NORMATIVE_TRACE=PARTIAL`，不能把本次送审稿 SHA 误写为正式版文件 SHA。
2. **无需重开产品审批的工程细化**：Admission 的实际预绑定/回滚原子性，RFB 固定 Target/匿名会话显示信息与第三方 Viewer 兼容，撤权线性化与 GUI/网络栅栏，登录态 Edge 的进程/凭据/退出处理，Server/Client 的真实 Qt5/Qt6 适配及资源预算，均须形成可测合同。
3. **Gate 的独立保持**：设计冻结由 Human 后续独立签收；G3 #5 技术资格尚未通过，Native TLS/E2E 未实现，Gateway #190 尚未激活。以上五项批准**没有**授权新产品代码、开公网路由或发布。

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
| 12 已批准决策与剩余风险 | Human A 决策卡、源条款追溯缺口、技术候选与后续受控变更 | Decision register |
| 附录 A 规范引用与术语 | **引用** V8 Native 原有 A/B，不复制/改号；其他受控设计 URL 与 digest | Provenance, glossary |

**版式建议，仅供审批**：A4 纵向、中文正文宋体小四、章节标题样式、自动目录/图表目录与交叉引用、图不跨页断裂、表头跨页重复/主要合同表尽量一页，页眉页脚和页码一致；正文图表优先 Mermaid/矢量源核对、再转换为 Word 嵌入图；先校对所有标题/数据/决策后再处理封面和分页。沿用 V15/V8 文档实际获批准的视觉体系，不单独创造新模板/版本。**此阶段不生成 Word、PDF或发布文件。**

## 9. 最终自检与下一动作

- [x] 14 模块有一份可追溯的总体目录、分模块设计候选和实施依赖计划，不重复生成 Gateway/Native 平行规范。
- [x] 四产品职责、两设备协议、Direct 独立、设备端最后授权、现场优先、Factory View 一个安装已贯穿。
- [x] H-T01/H-DUAL/H-REVOKE/H-EDGE/H-RFB 已由 Human 全部选择 A 并在本章登记到对应模块与技术验证合同，未经批准不重开 A/B。
- [x] 明确各 WBS 的依赖/Owner 类型/交付物/验证出口，避免“任务清单=已下发”。
- [x] 保留 G3、Native E2E、Edge12、真实安装/支持资格和 Gateway #190 版本激活为独立 Gate，均未被设计 PR CI 取代。
- [x] V15/V8 **送审稿**的实际 SHA 与历史归档匹配，56/99 章节、附录 D 56/56、Native A/B 21 type 字段及定长算术已核。
- [x] 56 项 V15 逐项映射主责模块/候选行为/失败边界及原文表6序号；V8 99 个 Heading2 逐项登记到 D1–D5 Owner（**并非已审通过**）。
- [x] Native 21 类消息各有待执行的合法字段及至少一类拒绝情形；NV-01..NV-12 确定性负向设计已列出（**NOT_RUN**）。
- [x] V15 表6 **31/31** 原文场景已映射到已有 XP01..20 + 新补 XP21..28 的验收设计，未运行任何测试；完整 56 项需求仍须细分原子合同。
- [x] WIRE-V01..V06 新数值分配、键码及 SOURCE_TENSION_01=A 的**产品协议语义**已获 Human 正式签收；不再重开候选数值选项。
- [ ] 56/99 需求进一步关联最终原子 ContractID、状态/错误/TestID/Owner，所有 14 模块关键接口、线程所有权、数据生命周期和负例设计独立审签；不以索引行数代替冻结。
- [x] 26 个 Native 黄金向量、12 个异常变异和 AFS-01..12 为**待执行的测试设计依据**，真实 Host/Viewer 互通尚未完成；其结果归适用 Gate-G3/后续工程验收，不是 Gate-D 的伪前置条件。
- [x] Human 已批准五项产品/安全设计取舍 A（不等于实现或冻结）。
- [ ] Human 正式模块设计冻结和独立 Gate-I 实施授权仍未取得。
- [ ] 一切产品代码、实机/资格、全产品发布尚未由本计划启动。

**状态结果**：`D5_PLAN_DRAFT=COMPLETE_FOR_REVIEW`，`DESIGN_COVERAGE=CANDIDATE_FULL_DOMAIN`，`HUMAN_H_T01_DUAL_REVOKE_EDGE_RFB=APPROVED_A`，`V15_TABLE6_31_TO_XP28=DESIGN_COVERED_NOT_RUN`，`NATIVE_WIRE_P_A_AND_TLS_A=HUMAN_APPROVED`，`NATIVE_26_VECTORS=DESIGNED_NOT_RUN`，`NORMATIVE_TRACE=PARTIAL`，`DESIGN_FREEZE=NO`，`IMPLEMENTATION_AUTHORIZED=NO`。

**下一动作**：独立审查 AFS-01..12 并完成 56 项 V15 / 99 小节 V8 到模块最小原子状态、错误、TestID、Owner 与质量 Gate 的余项追溯，继续把 V15 56/V8 99 初步追踪拆为需求→ContractID→State/Error/TestID/Owner；已批准的 Native 26 字节样例/12 负例待未来独立产品实施及真机验证，不以纸面计算冒充实测。目录、图表和版式内容先审好，再独立向 Human 提交 Gate-D 设计冻结；Gate-G3、Gateway #190、Gate-I 分别管理，不创建产品代码。
