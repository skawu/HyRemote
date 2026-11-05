# D1｜设备协议详细设计：RFB 3.8 / Native 1.0 独立互操作、授权与状态契约（候选 V1）

> **2026-11-05 Human 决策已生效**：[Human #843 决策记录](https://github.com/skawu/HyRemote/issues/843#issuecomment-6646988698)。相关产品/安全取舍选 A；本文初始“待 Human 选择”语句如与本次批准冲突，以审批后的具体条款为准。批准不等于 V15/V8 原文审计、产品实现或设计冻结。

> **归属：** [全产品模块设计总控 #843](https://github.com/skawu/HyRemote/issues/843)；继承 [D1 Framework 边界稿](module-detail-d1-framework-boundaries-2026-11-05.md)（PR #844）。  
> **上位权威：** Human 批准 HYR-PD-001/V15、HYR-ARCH-001/V8（#827/#830）；Stage02 G7 + ADR-0007/0008。V8 **附录 A/B 原文是 Native 1.0 唯一 wire 权威**，本文件没有替代资格。  
> **代码事实基线：** `HyRemote/develop@02bd993174016439f22fe8c3dc304e20d8e3ed69`；`docs/security.md`、`src/runtime/src/transport/rfb_transport.cpp` 和 `rfb_delivery_state.hpp`。  
> **效力：** `DESIGN_CANDIDATE / NOT_FROZEN / NO_PRODUCT_CODE / NO_NEW_WIRE / NO_PRODUCT_IMPLEMENTATION_ADMISSION`。本稿的接口和状态名称仅用于内部设计，不是新公开 C++ API 或发布协议定义。

## 0. 本轮产品结果与设计范围

面向两个可独立成立的结果：

1. **标准 Direct/RFB：** 在明确适用的可信 LAN 与用户配置内，继续服务无需 HyRemoteView 的标准 RFB 3.8 Viewer；互操作、输入边界、只读和不加密的事实必须如实呈现，不能为了受管安全修改标准 RFB wire。
2. **完整 Direct/Native：** 同一个 HMI Framework 的独立 Native 1.0 协议将能够完成设备身份、认证、获准 Target、帧、输入和独立 ViewGrant/ControlGrant；经不解密的受管 ByteStream 仍由远端 Viewer 独立核验 HMI 证书与最终权限。

RFB/Native **仅共享**设备端采集源/目标索引、可信 DeploymentAdmission、授权策略、输出隐私、资源控制与最终 Qt 输入栅栏；**不共享**握手、安全认证方式、wire、编码/画面增量基线、网络队列或错误协议编号。不得让 Native 依附 RFB 画面，不得新造第三必需 Enhanced 侧信道。

当前实施事实：RFB 有真实源码；Native 设备 TLS / 完整 Native endpoint **尚未实现**。阶段任务是详细设计，不启动开发、G3、Gateway 或安装包改造。

## 1. 事实分级、旧 Issue 时效性和唯一规范

| 事实/约束 | 来源 | 证据等级和处理 |
| --- | --- | --- |
| 现行 RFB 已精确比较 `RFB 003.008\n`，拒绝其余版本后才继续安全选项 | `rfb_transport.cpp` 的 `AwaitVersion` | **CODE_FACT**；#158 中旧“接受 3.7”的审计文字已过时，不得据此创建重复修复 |
| 已在 `acceptPendingClients()` 设置 `QTcpSocket::setReadBufferSize(256KiB)`；`readClient()` 在 `readAll()` 之前检查 pending + 已缓存限额 | 当前 RFB 源码 | **CODE_FACT**；旧“先 readAll 后判断”的记录已过时，不能据此宣称已实测全进程 RSS 及恶意 burst 结果 |
| RFB Normal 设 250ms 周期扫查，5s 不完整消息进度和 10s 首次有用消息时限；握手 3s，ClientInit/初帧 30s，最多 8 客户端 | `rfb_transport.cpp` | **CODE_FACT**，不是 V8 新架构的冻结指标，也不是全部恶意场景通过 |
| `consumeClientMessage()` 对**任意完整** Normal 消息设置 `producedUsefulMessage=true`，没有每次 FBU 后自动重新进入有限 usefulness 窗口 | 当前 `rfb_transport.cpp`；#158 后期评论要求更强的持续占用复核 | **DESIGN_GAP**：现有“首次完整消息”不等价于“持续生产有用显示请求”；须在协议模块详细设计选择正确的进度/活跃条件，再决定是否需要有限实现变更 |
| RFB 当前 Raw + TRLE（按协商选择），DesktopSize、Cursor、Fence 的有界空 flags 回应、ContinuousUpdates 可选；Clipboard CutText 读取/限额后忽略 | `rfb_transport.cpp` / `rfb-delivery-profile.md` | **CODE_FACT**；不是所有维护中 Viewer 或剪贴板功能的发布保证；Fence 不能假称 Qt 最终输入完成同步 |
| VNC Authentication 可在具备构建能力时使用，但不加密；`AuthenticatedEncrypted` 当前 `SecurityUnavailable`；默认 `0.0.0.0:5921` / remote input disabled | `docs/security.md` / Runtime `access_instance.cpp` | **CURRENT_RELEASE_BEHAVIOR**；不得把历史默认值与未来 H-02a 合为一项 |
| 独立完整 Native 1.0：8-byte BE header、21 类消息、设备 TLS 与 Grant/Target/Scope/帧基线 | V8 附录 A/B（经 #830 文档 05 引述） | **APPROVED_DESIGN_REPORTED / ANNEX_FULLTEXT_NOT_REVERIFIED**，无真实设备端/Viewer 互操作资格。**不得用本稿重新编号或补填缺失的 21 项** |
| H-02a 未来新架构启动任何协议监听前应先接受可信 DeploymentAdmission | ADR-0008 / V8 | **APPROVED_FUTURE_DESIGN, NOT CURRENT_IMPLEMENTATION**；保留已发布 RFB 行为事实 |
| Gateway/Edge 只做已登记 HMI 的有界透明 FIFO 字节路由；Managed 安全必须 View↔HMI 端到端设备身份/TLS | V15/V8、Stage04/05 | **APPROVED_DESIGN**；不能用 WSS/代理签发的 route token 取代设备授信 |

**来源限制：** 可见 GitHub 仓库没有可逐条回读的正式 V15/V8 DOCX 原文及 Native 附录 A/B 的逐字全文；该项需要在 D0/跨模块评审期完成真实源文件的条款/字节/消息结构哈希核验。本文描述的 Native 字段/长度只引用已合并候选文件，不做“已对正式 Word 二次验真”声明。

## 2. 双协议逻辑拓扑与“共享但不串权”

~~~mermaid
flowchart TB
  HOST["HMI Host: Target/Scope + Policy Authority"] --> P["Shared Runtime: DeploymentAdmission / AccessPolicy / AccessSessionRegistry"]
  CAP["Qt Widgets / Quick FrameSource"] --> F["共享基础采集与 Frame Storage"]
  F --> RFP["RFB per-peer Output Gate + RFB Encoder"]
  F --> NFP["Native per-session Output Gate + Native Encoder"]
  P --> RFP
  P --> NFP
  RFB["RFB 3.8 Listener: Auth/ClientState"] --> RFP
  NAT["Native 1.0 Listener: TLS/Auth/Session"] --> NFP
  RFB --> IN["可信输入上下文 + AdmissionFence"]
  NAT --> IN
  P --> IN
  IN --> GUI["Qt GUI 投递前再次验证 Grant/Epoch"]
  RFP --> RV["标准 RFB Viewer，可信 Direct"]
  NFP --> NV["HyRemoteView: Direct / E2E Managed Native"]
~~~

- 一台 HMI 可以依批准的构建模式配置 `RFB-only`、`Native-only` 或 `Dual`，但不创建第二套 Core/Shared Runtime。两个独立 listener 可以有各自的生命周期/隔离，统一遵守 H-02a 的部署准入。
- 每条协议连接必须拥有**独立的协议状态与会话实例**。跨协议 Scope 控制冲突由**同一 HMI 策略**裁决，而不是由同一个 RFB Socket 或 Gateway Route 决定。
- 凡当前代码仍只有 `InputHandler(InputEvent)`、全局 `remoteInputEnabled`、每个 RFB Client 的键鼠按住计数，均不构成经证明的未来设备端会话授权。安全设计需要在 Runtime 内部扩展可信上下文和 admission seam，但**尚不批准公共 Core API 更动**。
- 两个协议的输出最终发帧前都需做 ViewGrant/Scope/PrivacyRevision 栅栏；不同身份/授权的输出缓存不可误共享。Raw/Delta 前的设备侧帧丢弃与 Gateway 密文字节的无损 FIFO 是两个独立控制问题。

## 3. F-04 RFB 3.8：标准互操作与设备端策略映射

### 3.1 协议状态机（事实与未来策略接入分离）

**当前源码事实：**

~~~text
TCP Accepted
  -> AwaitVersion (server advertises RFB 003.008 / exact client reply)
  -> AwaitSecurityChoice (None 1 or VNC Auth 2 if provisioned)
  -> AwaitAuthResponse (only VNC Auth)
  -> AwaitClientInit
  -> AwaitInitialFrame (only if initial geometry unavailable)
  -> Normal (SetPixelFormat/SetEncodings/FBU/Key/Pointer/...)
  -> Disconnect/abort (held-state cleanup)
~~~

- 服务端 `ServerInit` 当前使用固定产品名与几何数据；它不是现有设备端 ViewGrant/TargetScope 的证明。
- 连接成功、Native 同机可用、RFB VNCAuth 通过、`RemoteAccessState::Running` 与 `connectedClientCount>0` 都不能被推导为某个自然人获得 ControlGrant。
- **未来策略接入点（候选）**：新架构 DeploymentAdmission 成功 → RFB 安全协商 → HMI 本地对该 RFB 连接当前所能代表的策略身份/配置授权 → 仅在允许查看的唯一受控 Target 下进入可发帧 Normal；Key/Pointer 必须再经当前 ControlGrant 最终检查。是否使用可证明身份还是仅 Host-managed 匿名只读能力必须明确，而不是隐含“已认证用户”。
- RFB 协议没有 Native 的 `TARGET_LIST/CONTROL_REQUEST/GrantEpoch` 原生消息。**绝不把私有 Native 字段插进标准 RFB 3.8 数据流**；对无法表达的会话角色、Scope 与多 Target 需求，由 Host 的受限外部策略/部署绑定处理，不可自动用云操作员身份补全。

### 3.2 RFB 传统客户端的身份能力上限（必须形成产品兼容裁决）

| 模式 | 现有可证明事实 | 目标设计安全表达（候选） | 不得声称 |
| --- | --- | --- | --- |
| RFB None / Insecure | 不认证、不加密，可在配置 LAN 端口访问 | HMI trusted deployment 确定可共享的单一 Scope/只读；若政策要求不可证明的身份，拒绝该请求或不启用该模式 | 不能凭 IP/握手推断用户身份或 ControlGrant；不能用于不可信公网 |
| RFB VNC Auth | 共享密码 challenge-response（构建/配置条件）；内容未加密 | 仅说明当前连接通过 **共享口令**校验；由宿主局部策略确定可访问的视域与是否允许输入，绝不假定一人一身份 | 共享 VNC 密码不是个人身份/证书或端到端加密 |
| RFB 具有明确批准的**未来**设备端端到端加密方式 | 目前没有已交付资格 | 若经独立威胁审查、实现和真实 Viewer 验证，可另行确定 Managed 资格 | 不能把 Gateway WSS、端口转发、代理 TLS 自动算作 RFB 设备 E2E |
| RFB 3.7 或其他版本 | 当前代码要求精确 3.8 | 作为不受支持的客户端版本按标准语义拒绝 | 不把旧 `lenient37` 握手现象包装成严格 3.7 互通 |

**开放问题 RFB-ID**：V8 设备端每会话许可可以与传统 RFB 匿名/共享密码兼容到什么程度？初步建议分开：
1. 可信 LAN 上仅对经宿主显式允许的固定 Target 提供 read-only 标准 Viewer 路径；
2. 需要个人身份、动态 Target/Scope、设备端细粒度 Grant 的交互优先 Native；
3. RFB 输入仅在 HMI 自身**明确且当前**授权了连接范围的情况下允许，默认关闭；单 Scope 控制独占需要设备 Runtime 中真正的 Gate，不能靠 Viewer 端自律。
该建议**不是**对既有已发布 RFB 使用权益或 V8 安全要求作出单方面缩减；如与 V15/V8 或对客户承诺冲突，列出影响、兼容和迁移方案交由 Human 决定。

### 3.3 RFB 数据、资源与时间界限的详细契约

| 领域 | 当前实现参考 | 下一设计出门要求 |
| --- | --- | --- |
| 协商 | exact 3.8，None/VNCAuth 条件分支，ClientInit/ServerInit | 非法版本不进入安全列表；错误安全类型不默认降级；部署准入先于 listener |
| 像素/更新 | Raw/TRLE、每 peer 像素格式与 `RfbUpdateState`；Full/Delta、resize 失效、DesktopSize | 每 Viewer 授权输出独立，策略更新后旧 Full/Delta/待发密文不能继续泄漏；必要时拒绝/重建基线 |
| 扩展 | Cursor/Fence/ContinuousUpdates 是协商能力；当前 Fence supported flags 为 0；CutText 有界丢弃 | 不把 Cursor 元数据、RFB name、剪贴板当成无授权可泄漏渠道；不夸大 Fence 同步或 Clipboard 支持 |
| 读缓冲 | `setReadBufferSize(256KiB)` + `bytesAvailable` 前置检查；最多 8 peer；编码数量≤1024、CutText≤64KiB、held keys≤64 为代码常数 | 总占用仍需包含 Qt/OS socket、send buffers、per-peer Frame/Delta、Encoding 临时；不能仅拿 256KiB 判断整机有界 |
| 阶段超时 | 握手 3s，ClientInit 30s；Normal 部分消息 5s，首次有用消息 10s（当前代码） | 不在合法静态 outstanding FBU 上实施普通空闲超时；同时不得让任意完整无关配置消息永久占用 8 个槽位；期限应按协议状态明确、由数据验证，而非先指定产品 SLA |
| 控制/释放 | `deliverKey/Pointer` 汇总多 peer held state，断开时释放；Core `InputEvent` 没有可信会话身份字段 | 多 peer 同 Scope 默认一操作者；逐连接可信上下文带到 Qt 最终栅栏；撤权/跨协议独立输入清理不得重复释放他人的 held key |
| 错误与恢复 | `protocolFailure`、`authenticationRejected`、`deferClientAbort`、断开回收 | 网络/协议错误不能提升 Host 权限；停止/重绑后一切旧会话/输入/敏感基线无效 |

特别审查当前 `consumeClientMessage()`：对任一完整 Normal 消息将 `producedUsefulMessage=true`。因此“曾发完整 SetEncodings”与“目前有显示 FBU 请求或有效持续交互”并未被严格区分；协议工作包应设计**按真实有用进展**的可复核状态转移，同时防止静态帧、慢 Viewer 被误杀。**本稿不直接指定修改超时或现行发布验收结论。**

## 4. F-05 Native 1.0：严格复用已批准的完整设备协议

### 4.1 已有派生设计核对范围（需 V8 A/B 全文最后签收）

| 环节 | 当前 Stage03/05 候选中的 V8 引用（非全文二次验证） | 设计责任 |
| --- | --- | --- |
| 帧头/协商 | `[type:u16, flags:u16, payload_length:u32]`，网络大端 8 byte；1.0 `flags=0`；`HELLO` / `WELCOME`，`HYRP` magic 和版本/能力/大小预算 | `NativeFrameParser` 先校验版本/类型/长度/状态，再分配与处理；禁止 Qt QVariant/QDataStream 外泄 |
| 设备安全 | 原生配置的 `plain` 或 `tls-password` 模式，不允许运行时 STARTTLS/隐式安全回退；TLS 身份与用户凭证分开 | `NativeTlsListener`、`NativeAuthVerifier`；Managed 只能在真实经认证的 Device TLS 与独立 Viewer 信任成立后使用 |
| 会话/目标 | `AUTH`/`AUTH_RESULT`（受要求的 TLS 模式）；`TARGET_LIST/STATUS/EVENT/SUBSCRIBE` 与 `SCREEN_INFO` | HMI 独立 Registry 决定可枚举/可订阅 Target，跨 Scope/Session 不泄漏 |
| 画面 | `FRAME_BEGIN/RECT/END`、`REQUEST_FULL`；Frame generation/PolicyRevision 基线 | 每授权会话维护 Full/Delta/Rect 与状态；变更隐私或目标后强制新合法基线 |
| 输入/控制 | `CONTROL_REQUEST/STATE/RELEASE`、`INPUT`；按既有附录字段校验 `target_id/scope_id/grant_epoch/generation` | ControlGrant 由设备发放，网络收包和 Qt GUI 投递前二次检查；不能把 GrantEpoch 当 bearer token |
| 错误/关闭 | `ERROR`、`CLOSE` 按 V8 原有编号，不再加 Gateway 路由字段 | 失败关闭、释放 held state、清除旧 Session/Queue；TLS 认证错误不通过普通 Native ERROR 降级 |
| 资源 | 已有派生设计报告目标上限（Target/geometry/Rect/UTF-8），以 V8 A.6 为准 | 不创造新常数；最终必须从完整附录生成逐类型长度/状态/符号向量矩阵 |

- **严禁**在 Native 1.0 header/body 塞 `RouteId/OperatorId/SiteId`，借 Gateway 登录发 Grant，或把 21 类消息拼成第二份“本文件定义的 Native 1.0”。
- Native 1.0 必须完成一套独立画面、输入、目标/会话/授信能力。RFB 作为平行传输不提供 Native 帧依赖；Native-only 编译和 Direct 实用性须在方案验收证明。
- `plain` 的使用范围与 TLS 安全模式必须严格服从已批准 V8 部署合同；**本稿不推断 plain 模式可作为不可信公网安全通道**。受管模式不能自动回退到它。

### 4.2 Native 设备会话和帧基线状态机（语义候选，非 wire 新增）

~~~text
DeploymentAdmitted
  -> NativeListenerReady
  -> TransportSecured? (由批准的安全模式决定；Managed 必须 Device TLS)
  -> HELLO/WELCOME exact V8 version and bounds
  -> AUTH required by configured mode
  -> DeviceSessionAuthenticated
  -> AuthorizedTargetList (仅返回当前许可)
  -> ViewGrant(Target/Scope) -> Subscribe
  -> SCREEN_INFO -> FULL complete -> DELTA permitted
  -> ControlRequest -> independent ControlGrant -> INPUT allowed
  -> Revoke/PolicyChange/TargetLost/Disconnect
       -> input fences revoked / held released
       -> queued sensitive frames invalidated / baseline reset
       -> closed; new TCP+TLS => new AccessSession
~~~

**候选状态维度（不得误写为 Native 枚举）：**

- `TransportTrust`：NotEstablished / DeviceIdentityVerified / Failed；只表示可信设备会话，不表示查看权。
- `DeviceAuth`：Unauthenticated / Authenticated / Denied；必须防止 AUTH 不在合法安全模式中发送。
- `ViewGate`：NoTarget / ViewDenied / ViewGranted / Revoked；没有 ViewGrant 时发送零未授权像素。
- `FrameBaseline`：Absent / FullInProgress / FullCommitted / DeltaEligible / Invalidated；按 Target/Scope/Session/PrivacyRevision 限定。
- `ControlGate`：NotRequested / Denied / Granted(epoch) / Revoking / Revoked；只有 HMI 的当前快照和最终 Qt 栅栏授予执行资格。

**三项必须设计并确认的竞态语义：**

1. **输出撤权：** 当前 PolicyRevision 更新后，不可有旧敏感帧从 device output 队列继续发送；已有写入 TLS/内核不可撤回字节的风险需定义明确的事件生效界点、连接关闭方式及相关剩余泄漏上界。不能简单声称“清缓存=撤回所有已发送字节”。
2. **输入撤权：** `INPUT` 进入协议队列不等于最终允许执行；在 GUI 真正提交前复核 `AccessSessionId/GrantEpoch/Scope/PolicyRevision`。授权更替和输入提交的线性化顺序需能证明旧 input 不在撤权生效后执行。
3. **断线重连：** 网络通道恢复不恢复旧 `ControlGrant`、未确认业务操作或 DELTA 基线；新连接须从可信 TLS/Native 身份与独立 Grant 开始。

### 4.3 独立设备 TLS 的部署与安全要求

HMI 的密钥材料与 DeviceId 由设备制造商/客户可信交付或受信根管理；Viewer 必须独立验证真实 HMI 的证书链/标识与已确认 DeviceId。工厂 View Edge/Gateway 仅经授权转发字节，绝不拿到 HMI 设备 TLS 私钥或充当信任根。证书变化、过期、吊销和被替身攻击应在 View 端失败关闭，不得显示旧像素冒充实时当前画面。

设备 TLS 和会话用户名/密码属独立层：未经 E2E 身份核验不得传秘密或任何授权输入；用户名/口令材料只经合格端到端安全通道传输。验证配置/受信来源失效时拒绝，不自动切换 RFB 明文。任何密码算法/安全参数选择以 V8 9.4及目标硬件成本验收为准，不能在设计稿里增加凭据明文存储或临时 CLI 参数。

## 5. 多模式启动、降级与权限的精确定义

| 构建/部署 | 允许的结构 | 必须拒绝的行为 |
| --- | --- | --- |
| RFB-only | Shared Runtime + Core + 单独的 RFB transport；面向经批准的本地部署信任范围 | 不能因为 Native 编译关闭就放松 H-02a 或输入/输出隐私栅栏 |
| Native-only | Shared Runtime + Core + 完整 Native transport、会话和设备安全适配 | 不借 RFB `FramebufferUpdate` 完成 Native 的画面路径；无 RFB 监听 |
| Dual | Shared Runtime + Core；各自 RFB/Native 连接、认证、输入状态；共享设备 Scope/Policy/采集 | 不因为某一协议认证通过使另一协议连接获权；不能让故障造成未授权 listener 部分可用 |
| Managed Native | 经 Gateway/Edge **只透明中转**完整 Native 安全字节 | 无真实 Device TLS/独立设备信任时拒绝；Gateway 的 RoutePermit 不提供 View/ControlGrant |
| Managed RFB | 当前**不具备**不可信公网的端到端设备保密资格 | 不能仅凭 WSS/HTTP Proxy/VNCAuth 进入 Managed Public 支持清单；等价 E2E 技术若未来提案需独立审查与实测 |

**待决 D1-PROT-04**：Dual 的两个 listener 在初始 bind/config 失败时，建议“**只要任意被要求的协议不能安全启动，则没有有效部分启动承诺**”；如果按宿主显式允许单协议退化，必须提前固定失败条件、警告与权限隔离。不得擅自改变既有已发布 RFB 运行习惯；此处仍为设计候选。

## 6. 跨协议安全仲裁：真实设备权威只在一个地方

| 事件 | RFB transport 做什么 | Native transport 做什么 | HMI 共享策略做什么 |
| --- | --- | --- | --- |
| 新连接 | 协商 RFB 安全类型；记录可证的**连接能力** | 验证 TLS/Native 认证；创建独立设备会话 | 校验 Deployment、可见 Target/Scope 与新的 ViewGrant |
| 只读查看 | 发送受准入保护的标准像素 | 发送当前 Scope 授权的 Native FULL/DELTA | 每连接输出前隐私过滤，撤权时清待发受限数据 |
| 控制竞争 | 标准 Key/Pointer 没有 GrantEpoch，须依受信连接上下文到设备端检查 | `CONTROL_REQUEST/STATE` 和 `INPUT` 只按既有 V8 字段 | 同一 Scope 默认一名远程控制操作者，宿主现场优先；另一个协议不能抢默认控制权 |
| 本地撤权 | 切断其当前可用的输入/输出，安全释放所持 Qt 状态；必要时关闭连接 | Grant epoch 变化，拒绝旧 INPUT、旧基线和 VIEW | 原子推进权威 epoch，在 GUI 最终投递前禁止旧操作 |
| 证书改变 | RFB 本身无法通过 VNCAuth 提供设备证书身份 | Viewer 必须独立拒绝旧不可信 DeviceId | Policy/Trust 更新不得被 Gateway/Edge 替代 |
| 目标切换/移除 | 不能通过标准私有命令隐式枚举所有 HMI 业务目标 | 按 Native 1.0 既有 TARGET/Scope 规则重新授权 | 拒绝越 Scope，不跨 Session 泄露 Target 名或旧画面 |
| 慢客户端 | 独立 per-peer damage/output 队列与背压/关闭 | per-session FULL/DELTA+有界输出队列与重建 | 设备主线程不因任何 remote peer 阻塞；不将过时画面谎称新鲜 |
| 丢连接 | 刷新 per-client key/button 计数并正确释放 | 销毁 AccessSession、GrantEpoch、FrameBaseline | Scope 恢复为实际策略，其他正常连接不被误撤权 |

**统一的内部、非 wire 语义接口（候选）**：

~~~text
admitConnection(protocol, independentlyVerifiedSecurity, deploymentRevision)
  -> AccessSessionId or DENY

authorizeView(session, target, scope, currentPolicyRevision)
  -> AllowedOutputPolicy | DENY

requestControl(session, scope, requestedIntent)
  -> Granted(epoch) | DENY

enqueueInput(session, normalizedInput, verifiedProtocolContext)
  -> BoundedQueueTicket | DENY

deliverQtInput(ticket, livePolicy)
  -> EXECUTED | REVOKED | TARGET_LOST

prepareFrameForSession(session, sourceFrame, livePrivacyPolicy)
  -> AllowedFullOrDelta | WAIT_FULL | DENY

revokeAccessScope(scope, newPolicyRevision)
  -> atomic deny boundary + invalidation & asynchronous notifications
~~~

**非伪造原则**：RFB 的 `InputEvent` 当前不含当前用户和 Grant 字段；Native wire `INPUT` 虽有已批准的字段，也不能替代本地 AccessSession 绑定检查。内部 `verifiedProtocolContext` 必须由协议实现关联受信 socket/session，绝不能直接信任客户端自报 `operatorId` / `ScopeId`。以上函数是审查用语义，不是即刻新增 public API。

## 7. 跨版本/三种客户端兼容性矩阵（设计与真实支持分列）

| Client → HMI Server | 设计期预期 | 当前可验证事实 / 不得宣称 |
| --- | --- | --- |
| 第三方标准 RFB 3.8 → 已发布 RFB | Direct 标准帧/输入依真实安全配置；不强制安装 View | 源码含 RFB 3.8/Raw/TRLE 及部分扩展；每 Viewer/平台/当前 SHA 互操作资格须独立记录 |
| 旧 RFB 3.7 → 新架构 RFB 3.8 | 拒绝版本，不暗中发送 3.8 安全帧冒充 3.7 | 当前源码已做 exact 3.8；历史 #158 旧注释不能被当成仍在运行的 parser |
| 仅 RFB 的旧 HMI → 新 HyRemoteView | 在可信 LAN、按实际协商且设备公开安全条件满足时可用 Direct | 不代表它能提供 Native 设备证书、Target List 或细粒度 Grant |
| Native 1.0 Viewer → Native-only HMI | 独立 Native 全画面、输入/会话/TLS/Target/Grant | **NOT_IMPLEMENTED/NOT_RUN**；必须与正式 V8 A/B 精确一致 |
| Native Viewer → Dual HMI | Native 不依赖 RFB；同设备 Scope 权限共享 | **NOT_RUN**；不可因 RFB 已测推导 Native 互操作 |
| 第三方 RFB Viewer → Dual HMI | RFB 标准直接连接，仍无必须的 Enhanced side channel | 身份/细粒度控制能力上限由 RFB 与 Host 安全配置决定 |
| Managed Native View → Gateway/Edge → Native HMI | Edge/GW 不终止 Device TLS、不处理帧语义；View 自证设备身份、设备端 Grant | **NOT_IMPLEMENTED/NOT_QUALIFIED**，待 HMI 实现 + View G3/G4 + 真 E2E 证据 |
| 公网 Managed plain RFB → Gateway/Edge → HMI | 当前**拒绝支持声明** | 现有 VNCAuth 无内容保密，不能因为 route 上有 WSS 就称 E2E |
| Qt5.15/Qt6.8 各四种 HMI 接入 → 双传输 | 同一个 Runtime 逻辑、受控 Qt Private 单元隔离 | 实际每个 Kit、构建模式、Target family 的资格分列；不能跨平台外推 |

## 8. 设计验收与证据映射（目标用例，不是本轮运行结果）

| ID | 合同测试/审核对象 | 判定要点 | 状态 |
| --- | --- | --- | --- |
| DP-01 | RFB 3.8 正确协议握手/版本拒绝 | 3.8 成功；3.7 在安全协商前拒绝；严格客户端不乱序 | `SPEC_READY, CURRENT_CODE_SEAM_PRESENT` |
| DP-02 | RFB None/VNCAuth 安全差异和未加密事实 | 未授权及加密要求缺实现时 fail closed；不在公网声称设备保密 | `SPEC_READY` |
| DP-03 | RFB Raw/TRLE/DesktopSize/Continuous/Fence/Cursor 互通 | 正确协商、每连接独立更新与基线、Fence 不声称 UI 输入已完成 | `SPEC_READY` |
| DP-04 | RFB 资源/半包/多 peer 占用 | 在当前实现限额上做 burst/partial/usefulness/静态 outstanding FBU/重入及持键清理；不以已存在代码替代 exact-SHA 实验 | `SPEC_READY, MEASUREMENT_PENDING` |
| DP-05 | 传统 RFB 的 Scope/View/ControlGrant 能力边界 | RFB 无 Native 自定义字段，HMI 本地承认的信任/Scope/只读/输入权有限且可证明 | `DESIGN_DECISION_OPEN` |
| DP-06 | 单 Scope 下 RFB+Native 多连接控制竞争 | HMI 统一限制默认一操作者，另一协议不会绕过；现场撤权阻断所有旧输入 | `NOT_IMPLEMENTED` |
| DP-07 | Native 1.0 21 类消息对 V8 附录 A/B | 所有 opcode/字段/限额/握手/非法序列/错误号逐项与正式原文一致；不造第二 wire | `ANNEX_FULLTEXT_PENDING` |
| DP-08 | Native TLS/设备身份、用户名密码安全认证 | 证书替身/过期/错误身份拒绝；TLS/口令变更不降级 | `NOT_IMPLEMENTED` |
| DP-09 | Native Target/Scope/ViewGrant/ControlGrant 分离 | 未授权不能枚举/看/控；GrantEpoch 旧输入在 Qt 最终投递前拒绝 | `NOT_IMPLEMENTED` |
| DP-10 | Native FULL/DELTA/generation/PrivacyRevision | 独立每 Session 基线、撤权/遮罩变更无旧敏感帧输出、慢端恢复 FULL | `NOT_IMPLEMENTED` |
| DP-11 | RFB-only/Native-only/Dual 部署准入及失败 | H-02a 未批准零新 listener，独立构建无额外依赖、Native 不依赖 RFB | `NOT_IMPLEMENTED` |
| DP-12 | Direct 离线与 Managed E2E 独立 | LAN Direct 不依赖云；公网 Native 必须 E2E；RFB 不走不可信公网明文 | `NOT_QUALIFIED` |
| DP-13 | 完整性能/资源边界 | Core + peer caches + Qt/OS/TLS/network 全口径有界；现场 CPU/帧龄可测 | `NOT_RUN` |
| DP-14 | Host 安全拒绝与 GUI 最终栅栏 | 撤权与当前输入执行具有可定义的线性化点，释放 held 状态一次，不回放业务操作 | `NOT_IMPLEMENTED` |

本轮只核对代码与文档；没有执行新测试。已公布过的历史 Windows/Qt/Viewer 证据继续有效于**原确切 SHA 和实际平台**，但不自动晋升为 Native/新架构/当前 PR 的产品验证。

## 9. 待设计决定（不能让开发时临时拍板）

| 决策 ID | 问题 | 当前候选 / 应取得的证据 | 人员审批边界 |
| --- | --- | --- | --- |
| D1-PROT-01 | 标准 RFB View/Control 与 Host Scope | **H-RFB=A 已批准**：标准 RFB 保留 Host 过去明确开启的远程输入；新设备策略默认只读，Host 为 Scope/Target 明确授予控制，并与 Native 共用单 Controller 仲裁；不把共享密码当个人/Grant | 产品选择已关闭；高保证动态身份用 Native，维护中 Viewer/权限负向验证仍待证 |
| D1-PROT-02 | 新架构 RFB 的 ServerInit、Cursor、Target name/geometry 在 ViewGrant 前能透露什么 | 优先只暴露政策允许的非敏感协议元数据；确需延迟/拒绝 handshake 时审查维护中的标准 Viewer 容错 | 如对公开互操作构成实质变化，Human |
| D1-PROT-03 | RFB 既有任意完整 Normal 消息置 useful 与长期占用保护差距 | 精确定义 FBU outstanding、配置消息、progress、有效客户端慢端与静态图的时序；不简单加全局 idle timeout | 一般工程细化可经技术审查 |
| D1-PROT-04 | Dual 的启动/运行期故障 | **H-DUAL=A 已批准**：声明为必需的协议端点全部经过 Host Admission 并原子启动，失败整体拒绝；RFB-only/Native-only 仍是独立 profile；运行期局部故障隔离，无自动明文回退 | 产品选择已关闭；实际监听回滚、状态反馈与兼容性测试待证 |
| D1-PROT-05 | RFB 原聚合 held count 与未来多 Scope 权限争用的实现关系 | 先在 Host 授权后形成 per-session/per-scope logical held 账本，再按目标 Qt state 做合法汇总和撤权释放；与 Qt Private 窗口投递例外隔离 | 涉及默认控制角色的修改必须遵守 D-02=A |
| D1-PROT-06 | Native 认证与正式 V8 A/B 的逐字/逐字段追踪 | 取得正式 V8 原始 Word；列齐 21 types、正/异常 vectors、有效期、错误、A/B 附录不变量，记录差异与 digest | 基线变更只能 Human |
| D1-PROT-07 | 设备 TLS 信任材料更换、站点信任绑定、跨 protocol identity 映射 | 设备根与 Gateway/Edge 独立；不自动把云 OperatorId 变 Native 用户；如需委托另立版本/架构讨论 | 需要扩充身份/Device wire 则 Human |
| D1-PROT-08 | 撤权与在途字节 | **H-REVOKE=A 已批准**：新 Policy/GrantEpoch 生效后不新选择/生成/排队旧受限输出和输入；清本进程可控缓冲并必要时断链；View 清/遮旧显示。已进 kernel/TLS/远端显示的字节不声称可撤回 | 产品安全承诺已确认；线性化点、队列拥有者与竞态/网络负向测试需技术审核 |
| D1-PROT-09 | RFB 与 Native 的资源预算及正式支持矩阵 | 用当前代码限额做基线，按 V8 128MiB 设计目标及实测拓扑完善；不预填 TLS/Native 未验证数值 | 一般技术评审，实质支持声明需要授权 |

## 10. 与其他设计包的边界与下一轮接续

- [D1 第一轮](module-detail-d1-framework-boundaries-2026-11-05.md)：总体 Core/Runtime/Policy/四入口的设计继续作为父级；本稿只展开 **F-04 / F-05**。如果出现冲突，先核对正式 V15/V8，再在全产品评审统一处理，不生成平行权威。
- [设备 Native 设计 05](managed-edge-detail-05-hmi-native-trust-2026-11-04.md)：对 HMI TLS/AccessSession/Grant 已形成更具体候选，本稿仅引用其协议生命周期，**不覆盖**或重号 V8 A/B。
- [Stage05 IDL](stage05-managed-edge-control-idl-2026-11-04.md)：属于 Gateway/Edge Route Control。其 `OperatorId/RouteId` 不被植入 Native 会话，`OUTCOME_UNKNOWN` 仅是宿主业务响应未知、不代表二次自动执行。
- [HyRemoteView #5](https://github.com/skawu/HyRemoteView/issues/5)/[#9](https://github.com/skawu/HyRemoteView/issues/9)/[#7](https://github.com/skawu/HyRemoteView/issues/7)：是研发前的技术验证/资格依赖，不能替代 HMI 正式 wire/Device TLS 设计。
- [#158](https://github.com/skawu/HyRemote/issues/158)：其历史审计的“3.7 可接受”“先 readAll”“未设 Normal 超时”已经与当前源代码不一致；需根据新 SHA 复核真实 remaining gap，但本阶段**不执行修复或提前关闭**。

**本稿通过后，D1 的六个领域 F-01..F-06 都已有可复用的跨模块设计材料，但尚未完成正式 V15/V8 正文/附录校对、所有开放技术取舍的审签和一体化状态/权限矩阵，因此仍是 `D1_DESIGN_CANDIDATE / D1_FREEZE=NO`。**

下一轮回到 **D1 跨六模块一致性审查与正式条款缺口审计**：查是否存在相互矛盾的 Scope、Frame 撤权、Native wire、RFB 兼容、安全启动、线程/内存约束，形成唯一问题/决策清单及 D2 Viewer 可引用的设备侧接口契约。**不启动产品代码或新的测试专项**。

## 11. 本轮设计自检

- [x] 以实际 `develop` RFB 源码而非旧 Issue 快照作为现行代码事实。
- [x] 分离现有 RFB 能力、Native 经批准的未来设计和未经验证的实现。
- [x] RFB 标准 wire 与 Native 独立 wire 不混合、无 Enhanced 基础侧通道。
- [x] 设备端 View/ControlGrant、Target/Scope 与 per-session 输出隐私由 HMI 最终裁决。
- [x] 显式记录多客户端 held 状态、RFB 标准身份不足、当前 usefulness 语义、隐私 in-flight 字节四类真实设计难题。
- [x] Legacy Direct 兼容与未来 Managed E2E 安全声明分开；不启用 #190。
- [ ] V15/V8 正式全文、Native A/B 的逐项条款/字节追踪仍未完成。
- [ ] D1-PROT-01..09 经必要证据/产品决策审签，尚未冻结。
- [ ] 未来新架构 Native/TLS/Grant/RFB-H02a 的任何正式实现/实测资格：均未获得。

**状态：** `F04_F05_PROTOCOL_DESIGN=CANDIDATE`；`D1_CROSS_MODULE_REVIEW=PENDING`；`NATIVE_DEVICE_IMPLEMENTATION=NO`；`FORMAL_PRODUCT_DEV_AUTHORIZATION=NO`。
