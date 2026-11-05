# D1 横向一致性审查与设备侧→客户端合同（候选，2026-11-05）

> **范围**：#843 D1 全部 F-01..F-06。审查输入：[Framework boundary](module-detail-d1-framework-boundaries-2026-11-05.md)、[RFB/Native device protocols](module-detail-d1-device-protocols-2026-11-05.md)、[HMI Native trust](managed-edge-detail-05-hmi-native-trust-2026-11-04.md)、ADR-0007/0008、Stage04/05、当前代码。  
> **权威**：正式 `HYR-PD-001/V15`、`HYR-ARCH-001/V8` Human 已批准，附录 A/B 仍是 Native 唯一 wire；此审查**不是**重开批准事项，不自行冻结公开 API、协议、安全承诺或研发准入。  
> **源审计**：初始开发树 `develop@268e26dbbccc6aaed4026ee4a888ba500623a372`；V15/V8 的 DOCX 完整原文尚未从本会话允许的 GitHub/Drive/文件检索中找到，不假装已完成原文逐条审计。  
> **状态**：`DESIGN_REVIEW_CANDIDATE / CROSS_MODULE_CONTRACT_V1 / NOT_HUMAN_FROZEN / NO_CODE_CHANGES`。

## 1. 一致性审查结果：统一权威与禁止边界

| ID | 一致性领域 | 最终权威 / 统一解释 | 各模块职责分界 | 本轮结论 |
| --- | --- | --- | --- | --- |
| XC-01 | Core 生命周期与设备授权 | Core 仅知 transport-neutral `Session`；HMI Runtime 的 `AccessSessionRegistry` 才能认可信协议连接与 Grant | F-01 不识别用户/Qt/Native；F-02/F-06 维护授权上下文；F-04/05 提供认证连接事实 | **ALIGNED**；旧 `InputHandler(InputEvent)` 无会话归属，未来内部 seam 必须补 |
| XC-02 | 四入口一致性 | F-03 C++/QML/Generic/QPA 同属一个 Runtime；H-02a 不因入口不同失效 | 入口 bootstrap 和配置映射，Runtime 统一设备策略、监听/报错 | **ALIGNED**；Qt Private 仅 QPA shim + 已批准窗口输入窄例外 |
| XC-03 | RFB/Native 安全 | 两套独立完整协议；RFB 当前标准 3.8/VNCAuth 无加密，Native Device TLS/Grant 尚未实现 | F-04 不能承载 Native 私有字段；F-05 不依赖 RFB 画面；F-06 统一 HMI 权限 | **ALIGNED**；Managed Public RFB 当前不得标合格 |
| XC-04 | 启动与默认监听 | 现行 RFB getter `0.0.0.0:5921` 不代表新架构绑定许可；H-02a 在**任一协议 listener** 前执行 | Host 配置信任源→Runtime Admission→protocol bind；重绑后重新核验 | **ALIGNED**；Dual 原子 bind/局部退化是未闭技术/产品选择 |
| XC-05 | 多 Peer 控制权 | 同一应用 Scope 默认一位远程操作者，除非宿主明确批准分离的不冲突 Scope | Host/F-06 arbitrate；F-04/05 不能“因各有 socket 就都可控制” | **CRITICAL GAP**：旧 RFB 汇总 held keys/按钮并未有设备会话权限/Scope 保证 |
| XC-06 | 双栈共享采集 | 可共享底层 Target/FrameSource，但每连接/Scope 的显示许可、遮罩和输出队列必须独立 | F-02 控资源及 capture；F-06 policy filter；F-04/05 各自编码/baseline | **CRITICAL GAP**：旧 RFB FrameDecorator 单次合成全 peer 复用，不能等同未来隐私授权 |
| XC-07 | 输入两次栅栏 | 入队时与 Qt 最终交付前校验当前 `AccessSessionId/Scope/GrantEpoch/PolicyRevision`；撤权优先现场 | F-04/05 关联可信 socket→F-06 检查→Qt adapter 最终验证 | **CRITICAL GAP**：需要保证同 Scope 不跨 peer 残留/重复 release |
| XC-08 | 画面撤权的真实性 | 发布新政策后不得**新排队**旧受限画面；内部未发缓存清除，已写出网络/已显示的像素不能物理收回 | HMI 清理内部并中断必要连接；Viewer 清屏/标过期 | **OPEN SEMANTICS**：需选精确撤权生效界点、in-flight 处理和可证测试 |
| XC-09 | 传输重连 | 每新设备连接/协议为新 AccessSession；旧 route/grant/输入/帧基线不可复活 | Core/Runtime generation；RFB/Native 新协议状态；View 新 Display epoch | **ALIGNED** |
| XC-10 | 受管信任 | Gateway Operator/Site/RoutePermit 只批准受管路径；Edge 白名单与 HMI DeviceIdentity/Grant 分属不同权威 | E/G 不看 Native 明文/不任意 TCP proxy；HMI 最终裁决 | **ALIGNED**；与 Stage05 没有发现必须更改 Native A/B 的理由 |
| XC-11 | 背压与资源 | Core bounded mailbox 不等于全设备有界；各协议 sender/Socket/TLS/Qt/每 session Filter/FrameStorage 分别累计 | F-02 用全口径预算；F-04/05 独立限额，Edge/Gateway 只能 FIFO 背压/关闭 | **OPEN BUDGET**：128MiB 设计目标不是测量证明 |
| XC-12 | “在线”状态真实性 | Listener Ready、TCP Connected、Authenticated、ViewGranted、ControlGranted、FreshDisplayed、RoutePaired 是独立事实 | Host/Runtime/Protocol/Viewer/Gateway 分层 report，不能互推 | **ALIGNED**；需要 D2 统一用户状态投射 |
| XC-13 | 协议兼容/已交付权益 | 当前 Source 代码已 exact RFB3.8/before-read bound/Normal sweep；#158 旧文字可能落后于代码 | F-04 不重做已修复事项；真实运行资格仍需复现；RFB 3.7 不受支持 | **ALIGNED**；`producedUsefulMessage` 对任一 Normal 完整消息置 true，持续占用需进一步定义 |
| XC-14 | 设备侧 Native wire | V8 附录 A/B 唯一 1.0 权威；设计稿引用了 21 类、头与 Grant，但未见可复核 Word 全文 | F-05 必须在正式原文签收后生成完整 type/field/state/negative vector 追溯 | **PENDING BASELINE INPUT**；不能以已有派生 Markdown 充当正式附录 |

**审查结论**：未发现足以授权另起协议/第五接入/第二采集的设计理由；但上表的 CRITICAL GAP、OPEN SEMANTICS 和 PENDING 阻止“D1 已完成冻结”。它们是**设计闭合工作**，不是现在开产品代码工单。

## 2. 接口合同：哪一层产生什么事实

### 2.1 HMI DeviceSideContract（内部语义；非 Native wire / SDK）

| 契约事件/操作（示例逻辑名） | 权威生产方 | 消费者与最小字段 | 允许的状态 / 不变量 |
| --- | --- | --- | --- |
| `DeploymentAdmitted` | HMI Host + Runtime Admission | Listener：获准的端点/协议/安全 profile/Target scope/revision | 必须在 bind 之前；撤销或材料失效时拒绝新 bind |
| `PeerSecurityEstablished` | 对应 F-04/05 协议安全栈 | Runtime：protocol/peer generation/实际认证强度/非秘密身份依据 | 不等于设备用户身份或 ViewGrant；RFB None 不可伪装 Authenticated |
| `AccessSessionOpened` | HMI Runtime 连接注册 | Policy：server-generated sessionId + socket generation + scope eligibility | 任何重连新 ID；不接受客户端自造 identity |
| `TargetViewGranted/Revoked` | HMI 当前 Host policy | FramePublisher + Viewer：target/scope/policyRevision/有效性 | 无此事件不能输出敏感 pixels；Direct 与 Managed 一致 |
| `ControlGranted/Revoked` | HMI ControlArbiter | Input admission：scope/grantEpoch/策略代际/holder | 同一冲突 Scope 默认一个远程 Holder；不等于“remoteInputEnabled=true” |
| `OutputFrameEligible` | HMI session output gate | protocol encoder：仅授权 frame + generation + baseline identity | 原始 base frame 不得直接经过许可缺失的 per-client encoder |
| `InputAdmissionTicket` | HMI input gate | Qt target sink：session/scope/grantEpoch/runtimeEpoch/policyRevision/序列 | 在 GUI 最终投递前**再次读取**当前政策；不可用客户端传入 token 取代 |
| `PeerClosed` | F-04/05 + HMI registry | frame/input/state：once close release、baseline invalidate | 旧回调和 queued input 失效，其他 peer/Scope 可继续 |
| `DisplayFreshnessEvidence` | HMI 提供帧序/时间，View 自测呈现 | D2 UI：源帧时戳、到达/呈现时戳、可靠性状态 | Gateway presence 和 TCP connected 不能替代 Viewer 实际新像素 |

这里的名称是**协议/组件间内部操作语义**，不要求 V8 A/B 增加任何一条新消息。标准 RFB 没有原生 GrantChanged wire 时，设备 Runtime 可在服务端生效并关闭/降级相应 RFB 连接；不能假称旧标准 Viewer 收到 Native Grant event。

### 2.2 真正的生命周期组合（分层，而不是一个巨大 Running）

~~~mermaid
sequenceDiagram
  participant Host as HMI Host
  participant Run as Runtime Policy
  participant Proto as RFB / Native Adapter
  participant View as Remote Viewer
  participant GUI as Qt Input
  Host->>Run: DeployProfile(Target/Scope/bind/security)
  Run->>Run: Admit deployment / reject before listener
  Run->>Proto: Bind only approved protocol endpoints
  View->>Proto: Open TCP + protocol-specific security
  Proto->>Run: Verified peer security facts
  Run->>Run: New AccessSession / enforce View policy
  Run-->>Proto: Per-session permitted frame scope or deny
  Proto-->>View: Only allowed image/protocol response
  View->>Proto: Control intent or standard RFB input
  Proto->>Run: Peer-bound input/control context
  Run->>Run: Current grant + scope check (admission)
  Run->>GUI: Bounded ticket
  Host->>Run: Revoke Grant / change scope or privacy
  Run->>Run: Advance epoch; deny new input/output
  Run->>Proto: Purge unsent sensitive data / abort if needed
  GUI->>Run: Revalidate at final delivery
  Run-->>GUI: Reject old ticket; balance held input
  Proto-->>View: Revoke / close / policy error as protocol permits
  View->>View: Clear stale confidential frame and disable controls
~~~

### 2.3 共享设备端 input arbitration 与 GUI held-state 清理

- **统一 Scope 冲突图**（Host 注册）：每个目标与 Scope 有唯一受信描述，冲突判定按当前 Host Policy。不同协议用同一 `ControlArbiter` 判定；同一 Target 的两个 RFB 客户端不因同用密码拥有两份默认 Controller。
- **会话归属稳定**：每个接受的 key/button/touch press 必须账本化为 `(AccessSession,Scope,Target,logicalInput,generation)`；由 HMI 当前权限先批准，再允许由 Qt adapter 汇总为正确宿主事件。对没有被许可的 press 不得生成后续 protected release。
- **撤销/断开**：只释放该会话实际持有的 logical state，不能把其他会话当前合法保持的键/按钮错误释放或继承；即使出错也不能把旧 held 状态带到下一个 RuntimeEpoch。
- **GUI 最终栅栏**：Qt 线程的交付点必须针对**当前** epoch 重新验，而不是从队列票据携带的旧 snapshot 直接判断。撤权发布和 GUI 提交必须拥有可测、线性化的先后规则；如果无法证明应拒绝和清理。
- **不引入 OS HID 注入**，保持 Qt 宿主窗口语义；AR-04 窗口输入 Qt Private 例外不能外溢到公开 API。

### 2.4 输出撤权与网络发送的物理界限

| 字节位置 | HMI 可作的真实保证 | 需要 View 配合 |
| --- | --- | --- |
| 尚未生成/编码/进入 session queue | 读取当前 ViewGrant 与 PrivacyRevision，拒绝旧敏感画面 | 等待新获准 Full |
| 设备端仍在可清除的 user-space queue | 清掉受限 frame/encoded buffers、失效 Delta baseline | 不显示上一 policy 的画面为实时 |
| 已经交给 TLS/socket 或 OS kernel send buffer | **不可声称可以物理收回已发送字节**；必要时关闭连接以限制继续投递并记录事件 | 连接失效时立即清理本地缓存/遮罩，禁用输入；不恢复旧 frame |
| 已到达 Viewer/display | HMI 无法单方面擦除外地用户实际已见像素 | View 在 Revoked/Close/TrustChange 时清屏、清凭据和输入缓存；这不追溯撤回已看信息 |

**规范建议**：新权限 epoch 线性化后，HMI 不再**选择或排队任何旧政策的敏感新输出**；失效队列被清理，必要时断开连接。若产品要求撤权瞬间既有网络缓冲中字节绝不可送达，则仅靠 TCP/TLS 无法作此绝对承诺；需 Human 对可验证的“撤权后不再产生新发送”界限、UI 处置和端到端在途风险作产品安全确认，不能文档偷偷承诺物理不可能。

## 3. 一致性差距 → 设计关闭动作（无产品代码）

| 缺口 | 对应 D1 已有选择 ID | 普通技术闭合任务 / 可独立形成的设计结论 | 需 Human 的条件 |
| --- | --- | --- | --- |
| RFB 匿名/共享 VNCAuth 的会话权限 | D1-DEC-06 / D1-PROT-01/02 | 给出兼容“单 Target、Host 管控、默认只读”与“拒绝无可证明个人身份的受限 Scope”两档精确能力；测试维护中第三方 Viewer 协议容错 | 若改变现有 RFB 客户可控制权或 V15/V8 人机权利语义 |
| Dual bind 初始原子性与运行期局部退化 | D1-DEC-02 / D1-PROT-04 | 推荐 `requiredProfiles` 原子准入/启动，明确某一协议故障仅影响其自身已授权 session；无静默弱协议回退 | 影响用户安装/发布功能承诺时 |
| Native A/B 原文全集与 21 message 追溯 | D1-DEC-08 / D1-PROT-06 | 建立 source digest→章节/offset/field/limit/test vector 表；不能从派生设计猜字段 | 需正式文件提供/源验证；变更附录由 Human |
| RFB input aggregate 与统一 Scope 仲裁 | D1-DEC-03 / D1-PROT-05 | 推荐每 peer logical-held ledger + current `ControlArbiter`，GUI final fence；保留 Qt 目标输入语义 | 修改 D-02 默认控制独占才需要 Human |
| per-Session privacy、静态 watermark 与帧复用 | D1-DEC-04 / D1-PROT-08 | 推荐共享 unfiltered base storage + per-permission-policy egress；敏感图像 per peer bounded cache；记录策略 revision | 放宽设备隐私/授权则需 Human，不可技术自批 |
| RFB usefulness/occupancy | D1-PROT-03 | 以安全测试矩阵定义“有用显示/合法等待”而不以任意 SetEncodings 作永久许可；独立修复归 #158，不占当前设计主线 | 正常非重大技术闭合 |
| Policy Provider/凭据/Host Identity | D1-DEC-03/07 | 推荐 atomic policy revision、可信部署清单、设备 TrustBundle 与 Gateway trust 分离；无可用安全材料 fail closed | 新引入身份委托或变更设备信任模型需 Human |
| 证据/预算/平台范围 | D1-DEC-01/05、D1-PROT-09 | 按 Qt5.15/Qt6.8/Windows/Linux/ARM cell 真实分层；把实际旧 API 错误映射与未来内部错误域分开 | 变更公开 SDK/强制支持或发布承诺需 Human |

**技术默认优先方案**（均是提案、可讨论推翻）：共享底图逐会话输出；Host-issued scope arbitration；RFB protocol sandbox/固定 Target 的有界能力；Dual 要求的 profile 初始启动原子一致性；Native TLS 与用户密码分开；无新 public SDK ABI；直接/受管同设备会话接口、不同 ByteStream。未获正式全文时仍标 `CANDIDATE`。

## 4. 冲突与上位设计变更登记

| 编号 | 发现的冲突/潜在误读 | 现阶段处理 |
| --- | --- | --- |
| CF-01 | HyRemoteView G2 #4 仍写“RFB + 必需 Enhanced TLS side channel”和 lenient 3.7；V8/ADR-0008 后者已撤回、RFB 源码 exact 3.8 | 记录 `SUPERSEDED_G2_SCOPE`，由 D2 直接消费最新 V8；不再立第三必需监听 |
| CF-02 | 旧 Tool 工作台 RFC 独立实现 Viewer/S1，而 V15 有独立 HyRemoteView 工作台。两个已批准产品定位有**长期职责重叠** | 不追溯删除已交付 Tool 功能；D4 将整理存量维护 vs 新产品功能归属、共用协议客户端复用/迁移方案，涉及产品承诺时由 Human 选择 |
| CF-03 | 现有 RFB 全 peer 静态水印/聚合输入不同于未来 per-session 隐私与 Scope 权限 | 仅是“现行实现 vs 新架构目标”差距，不代表已批准 V8 矛盾；不得误标现行已支持 |
| CF-04 | #158/旧产品材料仍有“接纳 RFB3.7、readAll before cap”等过时事实 | 以最新代码树为准，测试状态单独列出；不重复建修复 Issue |
| CF-05 | 阶段03 后台运行候选仍引用已废止 ADS Gate | ADS 已由 Human 撤销；G3 真实性、安全和设计审查仍有效，不复活 ADS |
| CF-06 | Stage04/05 IDL 候选对 control 序列化、token、超时、资源限额使用不同名称/数值 | 当前上位只是安全语义，D3 应做唯一 IDL 消歧映射；不从示例 JSON 推出正式 wire |
| CF-07 | V15/V8 正式 DOCX 未能在当前文件系统/Drive 检索到可审计原文 | `SOURCE_AUDIT_PENDING`，没有该全文不进行“原文逐条复核 PASS/正式设计冻结”声明 |

## 5. 对 D2 HyRemoteView 的最小设备端消费合同

| 设备源事实 | RFB Direct 客户端能否可靠知道 | Native Direct/Managed 客户端必须能知道 | D2 UI 允许的行为 |
| --- | --- | --- | --- |
| 设备是否真为登记 HMI DeviceId | 现有 None/VNCAuth 不能证明独立设备证书 | 经直接/中继 TLS 完成设备证书/信任匹配 | RFB 标“未验证设备身份/受限连接”，Native 失败关闭并要求可信重新核验 |
| 当前是否获得查看权 | Legacy RFB 可能只从服务器真实像素推断已接收，不代表个人身份 Grant | 精确 Target/Scope ViewGrant 或拒绝 | 许可状态未知不能显示“已被 HMI 授权”；Native 首帧前明示许可，失败立即清旧帧 |
| 当前能否控制 | `remoteInputEnabled` 与 ServerInit 名称仅辅助显示，不能保证某 peer 的实际 Grant | 有当前 ControlState/GrantEpoch 或拒绝 | 仅 Native 明确权利有效才展示“控制已授权”；RFB 控制请求/提示按协议真实能力呈现，不假造权限 |
| 当前帧是否新鲜 | 可记录最后实际合法解码/呈现的帧时戳、更新请求，不等于 source 时钟一定可靠 | Frame generation 与本地呈现时戳可独立跟踪 | 网络在线≠画面新鲜；连接失效或 Grant/Trust 变更遮蔽旧画面 |
| 当前 Scope/Target 列表 | 标准 RFB 无 Native TargetList | 原生授权 Target/Scope/订阅 | RFB 固定受准入 Target/endpoint，Native 仅展示许可目标；不能枚举未授权 |
| 会话断开/撤权 | 观察 EOF/协议失败及本地输入状态；未必获具体原因 | Native ERROR/CLOSE/Grant 事件（严格按 V8 A/B） | 不管协议如何，必须立即停止本地输入、清 held 状态；不要依赖云更新 |
| Gateway 可用性 | Direct 不需要 | Managed 才涉及且不等于设备权利 | 身份/网络/路由/设备/TLS/Grant/呈现各有状态，避免一个“在线”标签 |

**D2 只消费上述语义层，不得把内部逻辑操作名定义成 Native 1.0 新 wire。** 若未满足服务端能力，只显示 `UNKNOWN/NOT_SUPPORTED`，而不是用 UI 推断成功。

## 6. D1 完成判定与下一阶段

`D1_MODULE_SCOPE=F01..F06_REVIEWED`（已形成并交叉比较三份审查候选）；`D1_INTERFACE_CONTRACT=CONDITIONAL_CANDIDATE`；`D1_FORMAL_DESIGN_FREEZE=NO`。

允许继续 **D2 客户端详细设计**，但不能把开放的 RFB Scope/Grant、Dual 启动、撤权 in-flight、Native 原文校验擅自变成“已批准定案”。D2 需对这些问题采用清楚的条件分支并保留失败关闭契约。

下一阶段设计主线：D2 V-01 Direct/Native 双客户端协议适配，V-02 RemoteSurface/Qt Quick 输入/状态，V-03 Device/Identity/Credential/Diagnostics/Branding 与工厂 Edge 用户界面**界面消费合同**，以及与 Framework 的跨仓版本/能力协商。

**只有审查通过并由 Human 在必要节点做出正式取舍后，才能申请全产品详细设计冻结和实施批准；本稿的 Git 合并仅为工作成果记录。**
