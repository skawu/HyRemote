# HyRemote V1.7｜Gate-D 正式技术评审准备与分范围 Gate-I 准入意见

2026-11-23。**Human 已确认 V1.7 版本路线图与 36 项 WBS 作为正式集中评审基线。** 仅批准评审基线，不等于 V1.7 正式冻结、View 候选版号批准、Public P-EXT 编号通过、G3 PASS、Gate-I 或发布许可。

**核查依据**：Draft PR #1049 的 V1.7 CT-01..15 设计、D1–D5 修改、36 项 WBS、V15 56/56、V8 99/99 结构索引与 V15 表6 的 31 个场景；Human D-PROC=B、D-REVOKE=A 及 H-*、Native P-A/A 已批准约束；现行 HyRemote Public 版本路线、View #5 G3 及 #95/#190。以下为纸面技术与治理反证，**不是独立代码 Review、编译、Qt 真机、PoC 或发行证据**。

## 一、Gate-D 总体意见

| 项目 | 本次建议判定 | 判断依据 |
|---|---|---|
| 版本路线/36 WBS 评审基线 | HUMAN_ACCEPTED_FOR_REVIEW | Human 本轮确认，旧 Public 权威路线不变 |
| 技术评审材料 | READY_TO_REVIEW_WITH_FINDINGS | 15 合同、14 模块、36 WBS 与源索引覆盖，有明确关键 OPEN |
| Gate-D 技术冻结 | **NOT_FROZEN / 当前不建议直接通过** | 下述 RD-01..RD-03 必须审签，Draft PR #1049 尚未获得独立 Review |
| Gate-I 新增代码 | **各新增路径均未获授权** | Gate-D 尚未通过、适用 G3 未 PASS、P-EXT / View 版本尚未正式审定 |
| 原已授权 Public 维护 | PRESERVE_EXISTING_AUTHORITY | 0.3.5.0 仍 HOLD/NO-GO，维护及 #95 发布保护不因 View 停摆 |

**防止循环准入**：Gate-D 评审设计合同及可证伪负例，不要求先实施正式产品代码；运行性能/设备 TLS/QtKit 真机资格属于适用 G3/实施后产品 Gate。不能用未测试替代设计，也不能以未经授权的正式代码反过来作为 Gate-D 前置。

## 二、关键开放问题（仅复用现有 WBS/CT，不创建无限新任务）

### RD-01｜P0：索引覆盖不能冒充逐原子需求技术签收

Owner：#843 架构审查 / WBS A01，依赖 CT-01..15。

V15 56 行、V8 99 行已全部对应合同，是**结构映射**，不是 155 条原子行为正式签收。审查抽样发现 V15 §9.1 文本与文件整体延期错误：H-O01=A 只决定文件/可信业务回执延期；剪贴板另有 #171 受控产品分期。V15 §7.3 真实触控另有 #172 后期能力；V8 A.3 合法键码需要 CT-09。已对抽查确认的 10 行修正候选文档，不能因此声称其余所有行语义已逐个核对。正式 V15/V8/ADR DOCX 原件本轮仍未逐字获取，不能宣称原文逐字审签。

**关闭条件**：对本次实施范围内的高风险原子义务形成来源→唯一 Owner→CT→允许状态/失败闭合→正负验收规格→当期或经批准延期的可核验对应；对条件/未来能力记明确理由，而不是笼统 CURRENT 或 DEFERRED。无需为文档版式重新设计技术路线。

### RD-02｜P0：NativeHostPort 二进制边界仍是接口形状而非冻结 IDL

Owner：Public P01/P02 + Private N01，CT-02/03/14。

已确认同 HMI OS 进程、Private Native TLS/parser/codec、Public Host 最终授权、C ABI 单入口/opaque handle、HostEpoch/SessionEpoch/ModuleGeneration、FrameLease 和停止/撤权三阶段。但尚未审签 exact ABI 表的入口/返回、size/major/minor、对齐/调用约定、Host/Private allocator 与 buffer 归还、重入/回调/异步完成、Qt 网络线程与 GUI thread handoff、异常不可跨 ABI、错误值域、句柄代际、停止与可卸载条件。C ABI **不等于**跨 Qt5/6 二进制兼容或进程级沙箱。

**关闭条件**：两仓唯一受控接口/IDL 设计，至少包含错 ABI、已失效 handle、错误线程、已取消回调、撤权后仍持 FrameLease 的拒绝和回收合同；实施 Owner 不再需要选择新的 IPC/插件框架或擅加私有 wire。此处设计签收，不要求先生成正式头文件或运行代码。

### RD-03｜P0：Native 五按钮/17 个 Keypad 与 Qt 最后输入映射未签收

Owner：Public F-03/F-06 + Private F14，CT-09，原 D1 Native 详设 §4.2.1 也明确标未定。

V8 P-A 合法五按钮（含 X1/X2）、17 个独立 keypad 与中文 committed text 不能通过粗暴转换既有 Public InputEvent enum 丢失身份或将合法 wire 误拒为 INVALID_FIELD。协议已接收、Qt 实际输入已执行和业务生效是三个不同事实。

**关闭条件**：定义受信内部 NativeInputEnvelope 到 Qt5/Qt6 真实 Target Adapter 的合法键码/修饰符、Down/Up/失焦/撤权来源 held、IME 与 Key 分离、最后授权重查及不可执行 QtKit 失败语义，确保不破坏现有 Public API/ABI、不虚报执行成功。实际 Kit 正负测试留在获批的 G3/开发验收。

### RD-04｜P1 Gate-I 治理：View #5 G3 与 WBS 分路径不一致

WBS A03R/A03N 将 RFB Direct 与 Native DeviceTLS 技术资格拆分，是避免全部产品被错误 AND 阻塞的**建议**。但 View #5 现行权威仍要求 POC-A..H 全部 PASS 才有整体 G3 PASS_TO_G4；尚无已批准的 RFB-only 单独 G3 放行规则。

**关闭条件**：View Owner 正式决定保持 #5 全量 G3，或者批准按 RFB-only/Native Direct 明确拆分每路径适用安全/许可证/性能与真实证据。**在该决定前即使 A03R 单项通过，也不能推出 View Gate-I 已准入。** 这是正式实施前的治理条件，不把 G3 真机测试作为 Gate-D 冻结的逻辑前置。

### RD-05｜P1 能力声明：Qt Surface 与资源约束需明确准入档位

只允许已取得合法 RenderComplete/FrameSource/几何和敏感 overlay 信息的 Qt Raster/Quick/OpenGL/native/video Target 输出。未知混合来源拒帧/遮蔽，不能把旧 pixels 标新 FrameId；CPU readback 与 GPU zero-copy 不能笼统等价高 FPS。128MiB managed/96MiB raw/512KiB per Native peer pending 不能无条件同时满足任意 4K、多 mask、多 Peer。需按具体 Kit/Surface 标 FULL_VALID、LIMITED_VALID 或 UNAVAILABLE 与失败/降级路径。**这可作为设计条件冻结，不要求当前已经取得全部平台真实性能 PASS。**

### RD-06｜P1 正式版本 Owner 裁决仍未完成

Human 已确认的是路线图与 WBS **评审基线**，不是 View 0.1/0.2/0.3/0.4/1.0 的发行承诺，或 Public P-EXT 的正式版号分配。Public 仍以已合并 canonical roadmap 和机读 release train 为权威；#594 决定 P-EXT 新用户成果的具体版本归属、旧→新影响，View #1 自己决定候选版本。未批准前不能在 v0.3.5.0、0.4 RC 或已占后续版次中擅插产品代码。**这是相应 Gate-I/路线激活条件，而不是基础技术 Gate-D 的循环前置。**

### RD-07｜P1 独立审查与当前分支

PR #1049 仍为 Draft、没有独立 Review；候选与 develop 存在一项后进提交（此前比对差异为水印测试/源码，不应由本设计 PR 修改）。Gate-D 签收需以当时的精确 HEAD/受控来源完成设计差异 Review，并在必要时由维护者同步最新 base；不自动合并、不动 release/v0.3.5.0/#876。

## 三、正式 Gate-D 核心检查表与专业自审结论

| 审查项 | 初审判断 | 正式 Review 要检查的具体事实 |
|---|---|---|
| Human 批准架构 | 已明确 | D-PROC=B 同进程 Private、D-REVOKE=A T0/T1/T2，Public RFB 独立，不重开独立 Provider |
| 源追踪及产品分期 | **OPEN RD-01** | 56/99 索引及表6 31 场景是入口；对当期原子义务/延期/选装签收 |
| Private/Public 源码归属 | 候选一致 | D5 F14 Private Native HMI，F15 Public 无 Private 编译前置；Private 不向 Public 泄漏代码 |
| HostPort 二进制/线程 | **OPEN RD-02** | ABI/ownership、错误、回调、安全停用的唯一可实施规范 |
| Qt Input 细分 | **OPEN RD-03** | X1/X2、17 KP、IME、来源 held、QtFinalGate，无合法 wire 假成功 |
| T0/T1/T2 时序 | 有条件一致 | FinalCheck 与 Socket write 同 Peer 串行域；迟到任务永拒旧 epoch，TLS 残留需 abort，T2 还结算输入 |
| Qt Capture 与 HMI 现场 | 条件性能力 | RenderComplete 后合法帧、隐私覆盖/Source 生命周期/资源准入和无高 FPS 虚假宣称 |
| RFB / Native / Managed | 合同明确 | RFB 3.8 不改 wire；Native 21 type+TLS-first；RoutePermit≠DeviceGrant |
| 工厂 Edge / Gateway | 仅设计候选 | 工厂一个 View 安装、默认 OFF、Gateway #190 尚未激活 |
| 版本/WBS 与发行 | 基线已接受 | 36 WBS 不形成全产品 AND；Public 0.3.5 HOLD，0.4 RC inactive，View/P-EXT 版次未正式批准 |
| 独立技术 Review | **PENDING** | 不是本文自审取代第三方 PR 审查，更不是 GitHub 通过即 Human Gate-D 通过 |

**专业评估**：V1.7 无需推翻总体架构；已发现的 P0 是可定位的设计细化/来源校验问题，但尚未关闭，因而不能称 Gate-D READY_TO_FREEZE。该问题清单已足以支撑**一次正式集中技术评审**；不因项目可扩展性假设而继续无限新增并行技术专题。

## 四、分范围 Gate-I 判断（当前真实结论）

| 研发范围 | 能否正式进入 Gate-I / 编码 | 必须先具备 |
|---|---|---|
| Public 已授权维护、V0.3.5 原定修复 | **保留原有维护许可**；发布仍 HOLD/NO-GO | 按 #95 精确候选、P2、CI/独立 review/真实 Viewer；无本次 View Gate-I 依赖 |
| Public P-EXT HostPort/Frame/Policy | **NO，当前不得授权新增代码** | RD-01/02/03 适用设计、Human Gate-D、#594 正式版本分配、独立 Public Gate-I；不等 View G3 |
| View RFB Direct 0.1 候选 | **NO，当前不得授权正式客户端代码** | View #5 全量 G3 或正式批准 RFB scoped G3、真实 QtQuick/RFB/安全许可，Gate-D、View 版次与 Gate-I |
| Private Native HMI + Native Client 0.2 候选 | **NO，当前不得授权正式 Native 产品代码** | 公共 HostPort 受控 ABI、RD-02/03、Native DeviceTLS/21 type/Grant/G3 安全资格、Gate-D、View 版次与 Gate-I |
| View Workspace/诊断 | **NO** | 已合法 RFB 或 Native Direct 可用的真实事实源、View 自有版本、Gate-I；无伪业务回执 |
| 工厂同安装 Edge | **NO** | View Owner 产品范围/RunLease/可信管理与适用安全、版本及 Gate-I；不等完整 Managed |
| Gateway / Managed | **NO** | #190 独立激活、Stage05 IDL/安全与真实 Native DeviceTLS E2E、独立 Gate-I |
| Tool S1 / Public RC/GA | **按原有版本/产品 Gate，不由本次 Gate-I 接管** | 各自现有 train、Owner issue 和真实验收；不可由 WBS 自动激活 |

## 五、技术评审接续机制与最终状态

1. 现有 A01/CT 责任 Owner 关闭 RD-01/02/03；A02 独立 Reviewer 对 D-PROC=B 故障域、T0/T1/T2、双协议/资源/Qt Input 作完整反证签收。**不新增 WBS、不编产品代码、不要求 Human 修排版。**
2. Owner 分别处理 RD-04 的 View G3 规则、RD-06 的 Public P-EXT 与 View 候选版次，均不得靠本文自审自动生效。
3. 再向 Human 提交**一份正式 Gate-D 决策件**：明确无条件冻结/列明适用 Profile 的条件冻结/拒绝冻结；附受审 SHA 和待实施验证约束。Human 本轮未给出该裁决。
4. Gate-I 后续按路径单独申请；所有真实生产/工业设备、Host↔View E2E、性能和发行证据保留适用门禁。未达到资格时保持 NOT_RUN、UNKNOWN 或 NO-GO。

**状态快照**：HUMAN_REVIEW_BASELINE=ACCEPTED；D-PROC=B、D-REVOKE=A=APPROVED；GATE_D_PACKAGE=READY_TO_REVIEW_WITH_FINDINGS；GATE_D=NOT_FROZEN；P0=RD-01/02/03 OPEN；View G3=NOT_PASSED；View G3 分路径=PROPOSED_NOT_APPROVED；Public P-EXT/Private View 版次=OWNER_PENDING；Gate-I 全部新增产品路径=NOT_AUTHORIZED；V0.3.5 Release=HOLD/NO-GO；PR #1049 DOCS_ONLY/DRAFT/UNMERGED；PRODUCT_CODE=UNCHANGED。
