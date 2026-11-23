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

**独立自动技术审查实况**：Codex 对 PR #1049 的 `8ec6c634` 给出一条真实 P2：WBS V02 共享 Qt Quick Surface 依赖 `A03R`，经 V05 向 Native-only 传播 RFB parser G3 的错误前置。已在当前候选的 WBS 修正为：RFB 使用 A03R 的显示资格；Native-only 使用 A03N 中专门的协议中立 Qt Quick FrameSnapshot/RenderSurface 真实正负证据。**不更改 View #5 现行 A～H 全量门禁**，只有 View Owner 未来正式批准范围化 Gate 才可按该条件链申请 Gate-I。原自动审查为 COMMENTED / P2 发现，并非批准、Human 签收或测试 PASS。

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



## 六、RD-01..RD-03 设计合同精化（纳入本次同一 Gate-D 候选，不新增 WBS）

本节完成现有 A01/P01/N04 责任中的**技术方案收口**，不是 Gate-D 正式独立审签或现有代码已实现。证据基础：已批准 P-A/A 数值来源、D1/D2/D5/W00 的源索引及 develop 中 `src/core/include/hyremote/core/input.hpp` 与 `src/runtime/src/detail/input_mailbox_admission.hpp` 代码事实。由于正式 V15/V8/ADR DOCX 本轮仍无法取回，**此处签收状态只能是“来源索引 + 受控补充规则的设计候选”，不能写“原字节逐字核签 PASS”**。

### RD-01｜覆盖全部 56+99 索引的高风险原子语义反证结果

统一设计正文中 155 行保持完整；以下 23 条高风险语义断面进行**跨章节复合义务审查**，不是再创建 23 个子项目。未列的一般条款继续保留现有映射且为 `INDEXED_NOT_ATOMICALLY_SIGNED`，不能因数量完整而提升审核等级。

| 审查断面 | 对应源条款/档位 | 必须保留的原子行为 | 唯一主要责任 | 实施合同 | 具体失败事实或安全退路 | 分期与审查结论 |
|---|---|---|---|---|---|---|
| AR-01 | V15 §2.2；V8 §3.1/§8.1 | 唯一 Host、RFB 独立与 Private Native 双端源码 | Public Host/RFB；View F14 | CT-01/03/07 | Public-only 缺 Private 仍正常；Native 不用第三专有报文 | 当前双协议设计；未授权实现 |
| AR-02 | V15 §2.3；V8 §13.1 | Host 业务回执与 Qt 输入“已投递”事实分离 | Host F-06 | CT-09/15 | 真实业务状态缺失必须 UNKNOWN；H-O01 文件业务 ACK 延期 | 当前防伪成功，未来可信业务回执延期 |
| AR-03 | V15 §3.1–3.4；V8 §4.1–4.6 | 四 Qt 接入统一 Host，静态 HMI 不得虚称任意 Native 零改造 | Public F-03 | CT-02/11 | 无合法装载入口拒 Private；RFB-only 独立 | 当前适用 Kit/profile；跨设备无限兼容不声明 |
| AR-04 | V15 §4.1–4.3；V8 §6.1–6.5 | Target/Scope/Privacy/ContentRevision 完整来源 | Public F-02/F-06 | CT-05/06 | 必需混合层未知、旧帧代际不合法→拒帧 | 当前按 Surface Profile 受限 |
| AR-05 | V15 §5.1/5.2；V8 §8.1–8.6 | 标准 RFB 与 Native 完整独立设备协议 | Public F-04；Private F14 | CT-07 | RFB 不插 Native 字段；Native 21 type/TLS-first | 当前目标；Native 未获实现许可 |
| AR-06 | V15 §5.3；V8 §8.7/§9.7 | Direct 可离线，Managed 只是透明路由 | View V-01；Edge/Gateway | CT-07/12 | RoutePermit 不产生 Host Grant；不静默降为弱协议 | Direct 本期，Managed 条件 #190 |
| AR-07 | V15 §6.1–6.3；V8 §5.1–5.5 | 设备端 View/Control 独立 Grant，现场抢占 | Public F-06 | CT-04/06/09 | Control-only 撤权保留合法 View；View 撤权级联 Control | 当前 |
| AR-08 | V15 §7.1；V8 §7.1–7.5 | 键鼠和来源 Held 带当前 Session/Scope/Grant | Public F-06 + Private F14 | CT-09 | T0 后旧输入不可开始 Qt 执行；释放不误伤别的来源 | 当前；RD-03 签收待完成 |
| AR-09 | V15 §7.2；V8 §7.5 | IME CommittedText 不等于 Key 或剪贴板 | Public F-06/View V-02 | CT-09 | 不重复中文提交、不自动写剪贴板、未知业务结果不冒成功 | 本期安全基础；真 Kit 资格在实施验收 |
| AR-10 | V15 §7.3；V8 §7.5 | 触控仿真和真多点原生触控分离 | Public F-03/View V-02 | CT-09/15 | 手势不触发设备；原生多点未配置不得假支持 | #172 分期，不能由 Native input_hint=0 推断 |
| AR-11 | V15 §8.1–8.5；V8 §11.1–11.5 | Device Workspace、可信身份、FrameSnapshot 生命周期 | View V-01..V-03 | CT-10 | 身份/显示会话/设备目标相互独立；断线不复活旧 Grant | View 候选分期，未编码 |
| AR-12 | V15 §8.6–8.7；V8 §11.6 | 工厂同一 View 安装的可选 Edge、Managed 路由 | View E-01/可选 G-01 | CT-12 | Default OFF；注销/过期立即撤 Route；不能代表 HMI 撤 Grant | Edge 候选条件版，Gateway #190 未激活 |
| AR-13 | V15 §9.1；V8 §13.2 | CommittedText/剪贴板/文件是不同能力 | Public F-06；View V-03 | CT-09/15 | H-O01=A 只延期文件/可信业务 ACK；剪贴板按 #171 独立版本和授权 | 不得整组延期，也不得默认支持 |
| AR-14 | V15 §9.2；V8 §13.3 | 文件交付与业务结果不可混为传输已完成 | View V-03 / Host | CT-15 | 无受信 Host ACK 不重放有副作用指令 | H-O01=A 文件和受信 Host 业务回执延期 |
| AR-15 | V15 §9.3–9.4；V8 §13.4–13.6 | 拖放/打印/录制/音频各自独立选择 | View V-03/产品 Owner | CT-15 | 未获版本准入不自动实现/开启或记录敏感内容 | 条件/可选；逐能力 Owner 定版本 |
| AR-16 | V15 §10.1–10.3；V8 §9.1–9.7 | 设备 TLS、身份、权限、隐私与审计归因分离 | Public F-06/View Trust | CT-04/07/12/13 | Shared VNC pass≠自然人；Managed Route≠Grant；遮罩不可证则拒帧 | 当前安全语义；Native 真实 G3 待办 |
| AR-17 | V15 §11.1–11.4；V8 §10.4/§12 | 真实诊断及现场 HMI 优先 | Public Runtime/Tool/View | CT-08/13 | Unobserved≠PASS；慢 Peer 不饿死 GUI；Secret 不入诊断 | Public/Tool 独立路径 |
| AR-18 | V15 §12.1–12.3；V8 §14.1–14.5 | 离线售后授权、设备事实与永久 Direct 权益 | Host F-06/View V-03 | CT-14/15 | 云端待送达不得报 HMI 已撤；永久合法 Direct 不由云订阅撤销 | H-O02=A 本地期限；可信设备收件回执延期 |
| AR-19 | V15 §13.1–13.9；V8 §16.1–16.6 | 典型集成/双屏/现场优先/Managed 的端到端场景 | A01/各领域 Owner | CT-01..15 | 场景的入口对应真实失败与测试编号，未执行标 NOT_RUN | 按当前 RFB/Native/Managed 资格分段验收 |
| AR-20 | V8 A.1/A.2/A.4–A.6 | Native 8-byte BE/21 种消息和 FULL/DELTA 基线 | Private HMI+Viewer | CT-07 | 字段数值 P-A/A 不变，错误/状态不加新 wire | 已批准协议设计，互操作未运行 |
| AR-21 | V8 A.3；P-A §2.1 | Native 5 按钮、17 个 Keypad 与修饰符位完整合法 | Private Parser → Public F-06 | CT-07/09 | 4/5 不能当 INVALID_FIELD；高位未定义修饰符必须拒；72 Held 上限不扩大 | RD-03 下方适配候选，Owner 尚未审签 |
| AR-22 | V8 B.1/B.2/B.3 | 宿主七接口/七类授权事件/受控清单约束 | Public HostPort + Private F14 | CT-02/03/04/09 | 不能用五组 HostPort 操作删掉原 B.1/B.2 义务；清单不是签名授权 | RD-02 候选接口表，原 Word 逐字段核签仍需 |
| AR-23 | V8 附录 C/D/E/F | 受管128MiB/raw96MiB/每Peer512KiB；V15追踪/术语/横向不变量 | Runtime/QA | CT-01/08/15 | 4K 多副本资源先预留/限额；RouteGrant 不与 HostGrant 混同 | 设计约束成立，真实 Kit 性能未测 |

**从反证得到的修订**：V15 §2.3 的业务结果归 CT-09/15；§7.3 真多点触控不得列为普通 MouseEmulation 当期；§9.1 不能把文本/剪贴板一并随文件延期；§9.3/9.4 可选媒体必须各自获版次批准；§10.3 审计不是单纯设备 Grant；§11.4 现场优先应关联 CT-05/08/09；V8 §13.2 剪贴板与 #171 单独准入；V8 A.3 关联 CT-07/09。已同步回统一稿对应行，仍应由正式独立 Reviewer 对其余 `INDEXED_NOT_ATOMICALLY_SIGNED` 原子语义抽样/原文核签。

**RD-01 当前判定**：`HIGH_RISK_DESIGN_MAPPED`；`FORMAL_SOURCE_ATOMIC_SIGNOFF=OPEN`。缺少正式 Word bytes 的部分不伪造关闭，不因此重新开启总体架构选择。

### RD-02｜进程内 HostPort v1 最小 ABI 合同——明确一套，不让实现者再选

**适用**：Private Native 模块与 Public Host *同进程*，但跨仓、可选装配。不是新的网络协议，不是公用插件生态，不是把 Native parser 加回 Public。Host 可选择合法动态装载或经 HMI OEM 重新编译的静态整机集成，但两者必须共享此语义；不同 Qt Kit/CRT 的二进制不自动兼容。

**ABI 的唯一候选命名与基本布局**：

- 动态符号名固定候选：`hyremote_private_native_get_api_v1`；入口采用 `extern "C"` 与平台实际受控调用约定 `HR_CALL`，**不可跨 ABI 边界抛出 C++ 异常**；该符号的返回仅表示接口协商成功与否，绝不启动 listener。
- 单个 `HrAbiHeader{u32 struct_size; u16 major; u16 minor; u32 feature_flags; u32 reserved}`，候选 v1 `major=1, minor=0, feature_flags=0, reserved=0`；序列化外传禁止，只用于同进程精确构建/装配。对齐由构建 Profile 冻结并做大小/偏移一致性检查；未核实 32 位或异构 CRT 的 Profile 一律 `ABI_UNSUPPORTED`，不谎称稳定二进制兼容。
- 所有结构只出现固定宽度整型、固定单元状态码、`const u8* + u64 len`、固定宽度回调函数指针与不透明 `u64` 句柄。不得让 `QObject*`、`QString`、`QByteArray`、`std::string`、`std::vector`、QtPrivate 类型或可跨 allocator 释放的堆对象成为 ABI 字段。
- 两张表 `HrHostApiV1`（Public 所有）/ `HrNativeApiV1`（Private 所有），首字段都为 `HrAbiHeader`；模块获取后可复制**已知的 struct_size 内字段**；未知尾字段忽略，major 不符拒绝，minor 较高仅当已知必需函数/能力俱在且版本策略允许时接受。Host 指针/函数表必须在全部回调与模块引用真正结束前有效。
- `HrStatusV1` 内部数值单独冻结候选：`0 OK`（仅表示同步步骤完成）、`1 ACCEPTED`（只表示有界请求已入 Host 队列）、`2 DENIED`、`3 BAD_ARGUMENT`、`4 STALE_HANDLE`、`5 WRONG_THREAD`、`6 RESOURCE_LIMIT`、`7 NOT_READY`、`8 ABI_MISMATCH`、`9 CANCELLED`、`10 INTERNAL_FAILURE`；禁止将其当 Native 1.0 `ERROR.error_code` 发到网络。非零 status 或 `ACCEPTED` 都不是业务已执行/撤权完成。

**唯一定义的调用合同**（以下为候选 IDL 语义；非可编译头文件）：

| 调用 | 发起 → Owner | 请求数据及同步返回 | 异步完成/资源归属 | 必须遵守的拒绝规则 |
|---|---|---|---|---|
| `native.prepare(host_api, config_blob)` | Host → Private | Host API v1/大小/可信 digest、QtKit/CRT/SSL、受控路径；只返回 OK 或失败 | 不启动网络、不得自行开未授权监听 | ABI/QtKit/签名/证书不符拒绝 |
| `native.start_admission(admission_epoch)` | Host → Private | 必需端点已准备；只允许本 epoch 开放应用准入 | Native Peer 网络对象由其所属事件循环拥有 | Native_ONLY/DUAL_REQUIRED 必需端失败，统一应用 AdmissionEpoch 不能公布 |
| `host.open_peer(peer_fact, request_id)` | Private → Host | Peer 代际、真实认证方式与可信设备连接证据；同步 ACCEPTED/DENIED | Host 回调 `on_host_completion(request_id,session_handle,status)`；Host owns session | plain 设备 TLS 不等用户身份/Grant |
| `host.authorize(session,target,scope,kind,request_id)` | Private → Host | 当前 Policy/TargetGeneration、View/Control kind | Host 完成回调授 grant epoch，不能由 Private 生成 | Grant 未生效前不得取帧或控制 |
| `host.acquire_frame(session,target,privacy_rev,credit,request_id)` | Private → Host | 合法 Grant/预算/Frame 版本；ACCEPTED 或拒绝 | 成功回调不可变 `FrameLease{lease_id, storage_generation, bytes,len,format,epoch,expiry}`；Host backing owns，Private 一次 Release | 原始未脱敏帧/混合覆盖未知/失效 Epoch 不提供 |
| `host.release_frame(lease_id, storage_generation)` | Private → Host | 同步还引用，不代表 T1 | 仅真正引用归零才回收；TTL 只阻未来发送不自动释放 | stale/double release 无二次 free |
| `host.submit_input(session,scope,control_epoch,geometry_epoch,seq,payload,request_id)` | Private → Host | 只可接纳受信内部 input（见 RD-03），同步 ACCEPTED/DENIED | Host 最终 Qt 执行结果另记 `DELIVERED_TO_QT` / `REJECTED_AT_FINAL_GATE`，业务效果仍 UNKNOWN | 授权过期、队列满、非法 Target 及不可执行的合法按钮/键必须在接受前拒绝 |
| `host.close_or_revoke(scope,revoke_id,request_id)` | Private/Host 管理 → Host | Host T0 立即安装禁止旧许可 Fence；返回 T0/请求状态而非 T1 | 受影响 Peer 经 Native `apply_egress_fence` 执行并汇聚 T1，再由 Host 完成 T2 | 不能把发送 Barrier 命令成功当作 T1 |
| `native.apply_egress_fence(revoke_id,epoch,affected_peers)` | Host → Private | 带当前 HostPolicyEpoch 的异步请求 | 每 Peer EgressActor 所在线程实际处理 Barrier/必要 TLS abort 后 Private 报 `on_egress_fenced(revoke_id,peer_id)`；Host 汇总 | 最终检查在 T0 后的旧帧拒绝，迟到编码回调仍拒；Worker 未确认→UNCONFIRMED |
| `native.stop_new_peer()/quiesce()/destroy()` | Host → Private | 先停止新准入，后逐 Peer/T1/T2 结算 | callbacks/Lease/QtObject/tasks 真正归零才可卸载；默认无需支持热卸载 | NetworkLoop 已退出/对象未销毁时不能宣称 unload-safe |

**统一回调合同**：

1. 所有跨模块 `request_id` 在 HostEpoch+ModuleGeneration 内唯一，`on_host_completion` 可晚于同步 `ACCEPTED` 且**不保证不同请求之间的回调先后**；同一请求仅一次 terminal Completion。Terminal `CANCELLED/STALE_HANDLE` 之后不再发第二次成功。
2. Host 的 Session/Grant/Frame/QtInput 事实由 Host 所属管理执行域生成；Private 的 QSslSocket 始终只由实际 network QObject 线程访问。Host 不能从异步 callback 在持锁时反向同步等待 Peer Actor；Private callback 也不能同步等待 GUI 执行再等待 Host 资源。
3. 输入/帧任务在任何异步队列必须携 HostEpoch/ModuleGeneration/SessionEpoch/TargetGeneration/PolicyEpoch；**任何 `on_host_completion` 即使返回了旧的成功 Grant/Frame，也必须在消费回调时重新检查 Host 当前代际、政策和有效期**；取消或模块停止之后的旧完成回调只进行安全引用归还，不得对旧会话执行业务。
4. 对内存句柄 `u64` 值零表示无效，跨 HostEpoch、ModuleGeneration、SessionEpoch 永不复活；跨模块 buffer 如需跨调用保持只能由 Host 显式 FrameLease 或调用者 copy，接收者不得越界修改或从不同运行库释放。**所有 `u64 len` 在转换 `size_t` 前做溢出/预算检查，指针/长度二元组的所有权只在已登记的 Request/Lease 有效期内存在；`prepare(config_blob)` 如需异步保留配置必须在返回前完成合法复制**。
5. `ViewGrant` 仅 Host 签发，Private 只持结果事实；敏感帧来源 OutputGuard 已脱敏。Trusted same-process Private 可能因代码缺陷读写 HMI 其它地址，ABI 不是安全沙箱。

**映射 V8 B.1/B.2**：五组业务语义不是五项真实 ABI 函数，以上单个表允许多函数组成；必须保存原 B.1 七项宿主接口和 B.2 七类授权事件的**全部语义**。由于正式 Word 原件字段名未逐字取得，原规范函数签名/事件名映射仍须在正式评审时逐条签收，不能谎称上表已经完成该项原文等价证明。

**RD-02 当前判定**：`MINIMAL_IDL_SPECIFIED_CANDIDATE`（技术 Owner 不必从零决定 IPC、ABI 模式、线程/缓冲/异常）；`DUAL_OWNER_SIGNOFF=OPEN`，不把新创建的数值 enum 冒称既有 ABI 发布值。

### RD-03｜Native 合法键鼠 → Qt5/6 最终输入：无损内部表示

**现有源事实**：Public `PointerButton={None,Left,Middle,Right}`，`KeyCode` 不含独立 KP、`InputMailboxAdmission` 以 69 Keys + 3 Buttons =72 Held、Normal 64、ProtectedRelease 136 建模；因此直接把 Native X1/X2 或 Keypad cast 为 Public `InputEvent` 是错误实现。不修改已发布 Public enum 的字段/数值和旧客户端 ABI；引入 **F-06 内部专用** `AuthorizedInputIntent`，来源必须与 HostPort 当前受信 Session 绑定，禁止客户端自己声明已获授权。

**受信内部数据**：
```text
AuthorizedInputIntent {
  HostEpoch, ModuleGeneration, SessionEpoch, AccessSessionId,
  TargetId, TargetGeneration, ScopeId, ControlGrantEpoch, PolicyEpoch,
  GeometryRevision, SourceId, sequence, timestamp, kind,
  NativeButtonId(1..5) | NativeKeyId(0x0020,0x0101..,0x0201..,
                                    0x0301..,0x0400..0x0410) |
  CommittedTextUtf8 | PointerCoordinates,
  pressed(0/1), nativeModifiers(0..0x3f), source_key_zone(Main/Keypad),
  source_budget_credit
}
```
此概念结构**不跨网络线缆，不进入公开 Core InputEvent ABI，不以 pointer 地址作为 Session 身份**。仅 Private Parser 将受控数值一次转换成有界内部标记，Host 入队及 Qt final leaf 复核所有代际、来源、Scope 与 Qt 对象有效性。

**五鼠标按钮全量确定映射**（仅合法 Target/Qt Kit 才启用 Native）：

| Wire button | 内部身份 | Qt5.15/Qt6.8 目标 Qt::MouseButton | Held/最终投递 |
|---|---|---|---|
| 1 | LEFT | `Qt::LeftButton` | SOURCE+LEFT DOWN/UP |
| 2 | MIDDLE | `Qt::MiddleButton` | SOURCE+MIDDLE DOWN/UP |
| 3 | RIGHT | `Qt::RightButton` | SOURCE+RIGHT DOWN/UP |
| 4 | X1 | `Qt::BackButton` | SOURCE+X1 DOWN/UP，绝不变成 Right 或 None |
| 5 | X2 | `Qt::ForwardButton` | SOURCE+X2 DOWN/UP，绝不变成 Right 或 None |

每次构造 `QMouseEvent` 使用当下正确 Target-local/global、DPR/clip 与本 Source 及其它受管来源聚合后的 `buttons` 状态；Qt5/Qt6 有**不同构造器签名**，由同一语义的版本适配层决定具体构造；不能将 Qt6 ctor 生搬 Qt5。必须在合法 GUI/Target 线程经过 Host `TryBeginExecution` 再进行交付，`postEvent` 入队只代表排队，不代表发生最终许可检查。

**17 个独立小键盘码的确定映射**（所有以下 QKeyEvent 携 `Qt::KeypadModifier`，既保留 source_key_zone 也保留 `pressed`；不能与主数字 held 合并）：

| Native Wire | 内部 Key identity | Qt::Key（候选语义） |
|---|---|---|
| 0x0400..0x0409 | KP_0 .. KP_9 | `Qt::Key_0` .. `Qt::Key_9` + KeypadModifier |
| 0x040A | KP_DECIMAL | `Qt::Key_Period` + KeypadModifier |
| 0x040B | KP_ENTER | `Qt::Key_Enter` + KeypadModifier（主区 Enter≠KP Enter） |
| 0x040C | KP_ADD | `Qt::Key_Plus` + KeypadModifier |
| 0x040D | KP_SUBTRACT | `Qt::Key_Minus` + KeypadModifier |
| 0x040E | KP_MULTIPLY | `Qt::Key_Asterisk` + KeypadModifier |
| 0x040F | KP_DIVIDE | `Qt::Key_Slash` + KeypadModifier |
| 0x0410 | KP_EQUAL | `Qt::Key_Equal` + KeypadModifier |

**Modifier 位**：Native bit0 SHIFT、bit1 CTRL、bit2 ALT、bit3 META 可映射相应 `Qt::KeyboardModifiers`；bit4 CAPS 与 bit5 NUM 是**锁定状态**，不对应 `Qt::KeyboardModifier`，不能直接按数值 cast 或为了同步状态擅自合成锁键事件。Key identity `0x0205/0x0206` 才代表真实 Caps/Num 键按下/释放，须保持来源独立。bit6..15 非零在解析阶段仍按已批准 `INVALID_FIELD=5` 拒绝。所有普通 A..Z、主键区数字、F1..12、0x0101..0x010E、0x0201..0x0206 严格使用已批准键码单一查表，不能把 Qt::Key 值作为 Native wire 值。

**Committed Text**：只有已批准 `INPUT.COMMITTED_TEXT` 类型携 UTF-8 已提交文本；Qt 目标适配遵循合法目标文本/IME 接口，在受信 GUI 线程交付一次已提交结果，**不再附带生成相同文字的 KeyPress/KeyRelease**。Key 输入与文本输入分别计费、分别复核授权；未获得安全的 QtKit/Target 文本注入路径则拒本操作，不使用剪贴板或模拟任意危险快捷键兜底，且不虚报业务执行成功。

**Qt 最后栅栏与真实结果**：
- 先在网络接纳检查 Session/Grant/Target/Capability/队列；Host GUI dispatch 在**真实事件交付前**再次检查 `TryBeginExecution(GrantEpoch,PolicyEpoch,GeometryEpoch)`；T0 之后才开始的旧控制不执行；已开始同步执行的操作列入 T2。
- 对已经排队但尚未执行的 Qt `postEvent`，不要假定队列能按会话删掉，必须采用 Host 自有受控投递对象/带代际 ticket，最后交付前复核。Qt `sendEvent` 仅能在 receiver 所在线程中调用。Qt Quick / Widgets / Generic / QPA 依真正 Target Adapter 的合法事件入口而非跨线程直调。
- 错 QtKit、无合法目标、无法保持 Keypad/X1/X2 真实语义时，拒绝 **该 Target 的 Native 输入可用性/整条 Native profile 准入**，不能合法 wire 被错误认作 `INVALID_FIELD`、换键或静默称已执行。Native v1 基线 caps=0 无按键可选能力位；因此产品不能借未声明的 per-button capability 对终端承诺不实的完整能力。
- 网络协议已经接收 INPUT、Host `ACCEPTED`、Qt `DELIVERED_TO_QT` 与设备业务实际效果是四种不同事实；只有具备原业务受信回执才能声明业务生效，目前按 H-O01=A 缺失则结果为 UNKNOWN，不自动重放有副作用动作。

**保护性 Held 与队列守恒**：
1. `HeldKey=(Target,SourceSession,Scope,KeyZone,RawNativeKeyId)`；`HeldButton=(Target,SourceSession,Scope,RawNativeButtonId)`，对已有 Public RFB 输入用稳定的原公共身份。任何 RemoteSession 的撤权只释放它被设备实际接受的 synthetic held，不凭无可观测的现场物理按键推断状态。
2. 虽然逻辑类别扩展为 69+17+5=91，但**每个真实 Qt leaf 的已接受、尚未结清 DOWN 实例信用总数严格限为 72**（跨受管远端来源总计；包含还在 Qt 队列里的待执行 DOWN），普通待处理 64，保护性释放预留 136。单个 DOWN 可经 PENDING_RESERVED→QUEUED→DELIVERED_HELD→RELEASED/REJECTED_AT_FINAL_GATE 转移。**只有 Qt 最终受权实际交付后才把它登记为设备已持有 Synthetic Held**；若 T0 或 TargetDestroy 先阻断一个尚未交付的 DOWN，取消该 token 并归还预留信用，绝不能制造没有真实 DOWN 的 Qt UP。已交付 DOWN 通过受保护安全 UP 结算，匹配释放预留不得被普通事件挤占。没有信用就在接纳 DOWN 前拒绝，不能把 91+64=155 偷换为已批准的 136。
3. 特别注意键按下多来源冲突：**Public F-06 是 RFB 与 Native 共同的唯一 Synthetic Held 账本**；RFB legacy `InputEvent` 先由受信 transport context 加上 SourceSession/Scope，再归一化为内部 `AuthorizedInputIntent`，**不能让现有 RFB mailbox 与 Private Native 各自成为互不知晓的最后设备持有者**。同一 Qt synthetic 键在两个受信来源按住时，先失权的来源释放不导致其它来源误 UP；完全无法区分现场本地硬件状态的 QtKit，不得称能保证物理本地按键和合成远控键绝对互不干扰。
4. 重复 DOWN/无匹配 UP、旧 Session 的迟到 UP、FocusLost、TargetDestroy、QtContextChange、Policy T0/T1/T2 时，必须按 source/epoch 消费或拒绝。**重复 DOWN 对已持有同一 Source+Key 不能重复取得 Held 信用，必须按既有 repeat 语义处理或在无受信重复资格时拒绝**；迟到 UP 不能释放新 Session 同名 Key。保护性 UP 不构成新授权输入，也不得使旧来源获得控制权。
5. 正常 64/保护 136 的预算是**已批准的每 Qt leaf 上限**，不是每 Peer 每码独立提供同等预算；支持多个 Peer 时用同一受管叶子额度和按来源记账，不能把 RAM 限额偷偷乘以 Peer 数。

**RD-03 当前判定**：`LOSSLESS_INTERNAL_MAPPING_DESIGNED`；`PUBLIC_F03_F06_AND_PRIVATE_F14_SIGNOFF=OPEN`；`QT5_QT6_REAL_TARGET_EVIDENCE=NOT_RUN`。在正式设计 Review 通过前不得宣称“RD-03 已获批准”或“合法五按钮已实现”。

### RD-01..RD-03 设计精化的独立反证检查入口

| 反例 | 必须得到的拒绝/安全结果 | 状态 |
|---|---|---|
| Public SDK 没有 Private 二进制 | RFB 标准客户端继续正常；不装载 Private Native | 设计合同已明确，真实复测 NOT_RUN |
| P-EXT ABI major 不同/结构 size 不足 | MODULE_UNAVAILABLE，Private listener 不开放 | 设计合同已明确，正式 ABI 双 Owner 审签待办 |
| Host T0 已撤 Grant，Late Encode callback 才到 | FinalCheck 拒绝旧 epoch；Peer T1 再收齐 Barrier/必要 abort | 设计合同已明确，真实竞态 NOT_RUN |
| Native X1(4)/X2(5) 但公开 PointerButton 只有三按钮 | 内部 identity→Qt Back/Forward；不可安全投递则事前拒 Native profile，不把合法 wire 当非法值 | 设计合同已明确，真实 Qt5/6 资格 NOT_RUN |
| KP_0 与主数字0 同时按下 | HeldKey 的 KeyZone 不同，分别保留并投递正确 KeypadModifier | 设计合同已明确，GUI 真实结果 NOT_RUN |
| 69旧键全持 + 3按钮已经用尽72 Held | KP/X1 新 DOWN 资源拒绝，拒后不发合成 UP；136 protected reserve 不超 | 设计守恒明确，源码未改、运行 NOT_RUN |
| Caps/Num 位与 KeypadModifier | Lock state 不被强行 cast 成 Qt modifier 或擅造锁键按键 | 设计合同已明确，实际布局/IME 未测 |
| 多 Source 同一键、撤去一个 Peer | 只结算该来源 Synthetic Held，不误释放另一个来源/现场物理键 | 设计合同已明确，实际 Qt 输入聚合未测 |
| CommittedText 与 Key 先后 | 已提交文本只交付一次，不借剪贴板/文本转键重复输入 | 设计合同已明确，中文 IME 实测 NOT_RUN |
| 128MiB 多 Peer 4K | 输入/采集预算分别先预约，再准入，不以 Qt 内部缓冲“自然有界”当已达标 | 设计合同已明确，真实资源未测 |

**本次设计补充结束时的状态**：RD-01 `HIGH_RISK_DESIGN_MAPPED / ATOMIC_SOURCE_SIGNOFF_OPEN`；RD-02 `MINIMAL_IDL_SPECIFIED / DUAL_OWNER_SIGNOFF_OPEN`；RD-03 `LOSSLESS_MAPPING_DESIGNED / QTKIT_AND_OWNER_SIGNOFF_OPEN`。三个 P0 已有具体、可反证的唯一实施合同候选，但**尚不可据此单方面称 Gate-D 已满足全部冻结条件**。下一道仅为现有 A02 的独立 Owner/Reviewer 签收和 Human 一次集中 Gate-D 决策，不新增研究专题。


## 七、与编制过程分开的反向技术检查（同一 AI 的独立视角自审，非第三方 Review）

为了不把“有接口表”误当作“已具备完整准入”，在上述合同形成后用失败驱动逐项反向检查；以下结果只由本轮助手完成，**不能冒充独立 GitHub Reviewer 或两仓 Owner 审签**。

| 反证问题 | 检查出的漏洞 | 当前处理 | 对 Gate-D 的影响 |
|---|---|---|---|
| RFB 与 Native 是否使用两套 Held 账本 | 旧 Public 三按钮 mailbox 与新增 Private 五按钮处理可能在同一 Scope 双方各自释放 | 将最终 Remote Synthetic Held 唯一 Owner 固定 Public F-06；RFB 旧事件附受信 Source 后也进入相同内部输入域；Private 不能直接执行 Qt | 设计修订已写；实现边界待 Public/Private 双 Owner 审签 |
| 72 held/136 release 是否真的守恒 | 若“已队列接纳 DOWN”误作“Qt 真正按下”，撤权可能凭无效按下生成 UP | 分成 PENDING_RESERVED/QUEUED/DELIVERED_HELD/REJECTED/RELEASED；取消未交付 DOWN 释放额度但不 Qt UP，已交付才按来源保护性释放 | 修订后与 64/136 预算不矛盾，真实高并发未测 |
| Async Host callback 是否能在 T0 后重新授权 | 仅检查 request 发起时 Epoch 会使迟到 Host 完成回调激活旧 Grant | 回调**消费时**再核当前 epoch/expiry；只允许旧引用归还，绝不复活旧 View/Control | 设计修订，Owner/线程细节待审签 |
| HostPort frame 指针/大长度 | `u64 len` 到 `size_t` 或回调后存裸指针可能溢出或悬空 | 长度先验预算、Lease pin/Release 责任、配置 Blob 返回前 copy | 设计修订，不代表代码安全已验证 |
| T0 与 Socket write 交错 | 检查完成后实际写仍可能越过 T0 | 只允许 D-REVOKE=A 已批准的**先检查后交错写入**短窗口；T1 Actor 完成全部在途写，TLS 不可选择删除时 abort | 契约清楚；真实 Qt 并发证据留 Gate-G3/实施 |
| Qt5.15 与 Qt6.8 输入构造器 | 不能把某一 Qt6 QMouseEvent/QKeyEvent 构造器原样外推 Qt5 | 只冻结合法 `Qt::BackButton/ForwardButton`、`Qt::KeypadModifier` 语义；具体构造器由精确 QtKit 适配 | 设计条件成立，具体平台资格 NOT_RUN |
| Native 没有可选键码 caps | “某个合法按钮不支持”若每次只是默默拒用户动作，产品却宣称完整 Native | 必须在 Target/Profile Native 准入前完整检验 5 button+17 keypad 的能力，不能通过 per-key 伪成功掩盖不兼容 | 设计约束已写，产品支持档位待实测 |
| 同进程模块安全 | C ABI、签名、工作线程不提供进程隔离 | 风险保持公开；若需求要求恶意代码 OS 隔离，与 Human D-PROC=B 不相容，不能私自改进程路线 | 架构固有代价已批准，不重开 |
| 需求文字“索引有链接即已批准” | 155 源二级索引中部分当期/延期按章节粗推；抽查发现 §9.1 等错误 | 本轮只将 23 高风险复合断面逐项显式审查、10 处原映射纠错；剩余原 Word 原子义务不能伪称全审签 | **RD-01 正式签收仍 OPEN** |
| 同进程停用 & QObject 线程退出 | `STOPPED` 不等于 Qt Object 全销毁，也不能安全 `deleteLater` 永久等 | STOPPED/RESOURCES_QUIESCED/UNLOAD_ALLOWED 区分；无法归零时不可热卸载并保留状态为 UNCONFIRMED | 设计形状明确，owner 复核待办 |

**结论**：RD-02/03 已有足够明确的唯一设计候选，可进入双仓 Owner 技术签收；RD-01 本期高风险语义映射取得实质进展，但**不能从已审的 23 类断面反推出全部 V15/V8 原子要求都完成签收**。本轮没有得到独立 Reviewer 的正式评论，因此 Gate-D 仍为 `REVIEW_READY_WITH_P0_OPEN`，不宣称技术冻结。下一个可执行步骤限于现有 A02 的实际 Owner/Reviewer 复核与必要的原文核签，不继续自动创建新技术主题。
