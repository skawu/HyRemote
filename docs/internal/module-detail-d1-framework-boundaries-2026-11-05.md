# D1｜Framework 设备端模块详细设计：Core / Shared Runtime / 四接入 / 设备授权（候选 V1）

> **2026-11-05 Human 决策已生效**：[Human #843 决策记录](https://github.com/skawu/HyRemote/issues/843#issuecomment-6646988698)。相关产品/安全取舍选 A；本文初始“待 Human 选择”语句如与本次批准冲突，以审批后的具体条款为准。批准不等于 V15/V8 原文审计、产品实现或设计冻结。

> **归属**：[全产品模块设计总控 #843](https://github.com/skawu/HyRemote/issues/843)，D1 首轮；参考 [#830](https://github.com/skawu/HyRemote/issues/830) 的 HMI Native 安全设计与已合并 Stage04/05。  
> **批准的上位基线**：HYR-PD-001/V15（SHA-256 `d0408f333d4b9aec186067cd159ff8a4a2206ef66d58243fd4c5cd10ffb82cf1`）、HYR-ARCH-001/V8（SHA-256 `1101d5546cfc0ab63e4cfde809ba4b0530e1048ed3760b56b8c9e7be3de6803a`），均已获 Human 批准，见 #827/#830；阶段02 G7 与 ADR-0007/0008 保留为上位先行约束。  
> **代码事实快照**：`skawu/HyRemote@develop:36ac22d5a2cea287e655d13129114db223db0087`（2026-11-05），只作差距分析，不代表所有历史发布二进制。  
> **效力**：`DESIGN_CANDIDATE / NOT_FROZEN / NO_CODE_CHANGE / NO_IMPLEMENTATION_ADMISSION`。本稿只冻结“设计议题的覆盖范围”，**不冻结新增 public SDK、C++ API、Native wire 或类名**。

## 0. 设计目标、来源完整性与非目标

本稿交付 #843 中 F-01 Core、F-02 Shared Runtime、F-03 四个并列接入前端、F-06 设备部署授权/会话许可/隐私及最终输入栅栏的**跨模块职责、内部接口语义、状态与失败关闭契约**，作为下一轮 F-04 RFB/F-05 Native 协议详设和 V-01 View 客户端设计的稳定输入。

**来源完整性说明**：本仓库可直接阅读 ADR-0007/0008、`docs/architecture.md`、`docs/security.md`、正式冻结记录及源代码；V15/V8 **完整 Word 原文与 V8 附录 A/B 没有同步为可逐条核对的 GitHub 规范全文**。已有 #830 的 HMI 详细设计 05 引用了附录 A/B 的结构，但引用不是独立复核。因此本稿按已批准原则开展详细设计，标明需原文签收的内容，不宣称完成 V15 已有 56 个二级标题所涵盖的条款/V8 全附录逐条追溯；任何实质差异提交 Human 变更决策。

本轮不实施 Native、DeploymentAdmission、会话授权、RFB 改造、Qt 采集性能优化或 View/Gateway/Edge；不新设第五接入方式、不强行改变已发布 API、RFB 监听默认配置、Native 1.0 消息、版本路线或测试 Gate。

## 1. 当前实现事实与目标设计的区别

| 项 | 当前仓库直接可核对的实现 | V15/V8 已批准目标 / 设计差距 |
| --- | --- | --- |
| Core 会话 | `src/core/include/hyremote/core/session.hpp`：`Session` 持有一份 CaptureSource、Transport、可选 InputSink；Stopped/Starting/Running/Stopping/Faulted，启动撤销/确定性停止；mailbox 等待容量 + 当前 dispatch 一帧 | 维持 UI/协议无关语义；**每连接 DeviceSession、Grant/Target/Scope、PolicyRevision 不在当前 `Session` 契约中**，不能误把它当设备授权机关 |
| Runtime | `src/runtime/src/access_instance.{hpp,cpp}`：单个 AccessInstance 组合 TargetComponents、RFB Transport、Core Session；重绑会先关闭旧 run，持有代际 token；当前 Runtime 提供 `AccessState::Unavailable` | 未来统一设备侧认证、Scope 授权、每会话帧输出策略、RFB/Native 独立适配；**不是**“每个 Viewer 创建第二份采集” |
| 四接入 | `src/integrations/{cpp,qml,generic,qpa}`；均映射共享 Runtime；QML 的 `componentComplete()` 之后按 enabled 请求启动 | 公共/零代码入口仅转换请求、报告状态；不复制权限政策或各自开发 Native/Runtime |
| 设备安全 | `docs/security.md`：RFB Insecure 无认证加密；Authenticated/VNCAuth 条件能力但不加密；AuthenticatedEncrypted 当前 fail closed；remoteInputEnabled 默认 false | 独立完整 Native 1.0/TLS、设备可信身份、View/ControlGrant 与设备端策略需未来实施；不能将现有 `remoteInputEnabled=true` 理解为按人/Scope 授权 |
| 启动准入 | 现有 Runtime 可在配置/安全材料有效后打开 RFB listener；RFB 地址默认 AnyIPv4/5921 | **H-02a 目标**：未来新架构任何协议打开监听前必须通过可信 DeploymentAdmission；当前未实现，旧发布事实不被追溯改写 |
| 输出和隐私 | `src/runtime/src/transport/rfb_transport.hpp`：当前 `RfbFrameDecorator` 每帧装饰一次，再对 viewer 复用；静态文字水印策略已存在 | 会话/Target/Scope 差异化的遮罩/水印/权限必须在设备端每会话安全地隔离；共享基础帧不等于共享最终可发数据 |
| 输入 | Core `InputEvent` 标准化并交给 InputSink；Runtime Widgets/Quick 适配有保护释放的有界 mailbox | 设备端需**入队时 + Qt 最终交付前**核验最新授权与 Epoch，撤权后同步阻断旧输入，不能靠 UI 禁用按钮 |
| 性能证据 | `docs/internal/qt-quick-capture-path-qualification-2026-11-04.md` / #609/#789 仍有目标端不足 | 资源目标与实机资格独立；不能把设计预算标记 PASS |

注意区分**产品复用**与**运行对象基数**：One Core + One Shared Runtime 指唯一实现体系，不代表全进程强制只有一个 `Session` 对象。当前每 AccessInstance 组合一个 Session；未来多 Target 的生命周期/共享采集基数需由 Runtime 显式治理，不能推导出“每 Viewer 一份 Session/截图”。

## 2. 组件拓扑、所有权与依赖方向

~~~mermaid
flowchart TB
  C["C++ facade"] --> R["Shared Runtime · 唯一产品实现"]
  Q["QML facade"] --> R
  G["Generic Plugin"] --> R
  P["QPA Factory Trampoline"] --> R
  R --> K["Qt Target / Capture / Input Adapter"]
  R --> A["设备策略与会话许可 · 目标设计"]
  R --> T["RFB 和 Native 独立传输适配 · 目标形态"]
  R --> S["Core · Session / RemoteFrame / InputEvent / Mailbox"]
  T --> A
  K --> A
  P -. "仅兼容委托" .-> N["原生 Qt QPA"]
  A -. "策略授权" .-> H["HMI 宿主 / 本地控制权威"]
~~~

- **Core 不认识** Qt GUI、TLS/证书、RFB/Native 消息、用户、租户/Gateway 或现场策略实现。
- **Runtime 拥有** Qt 目标与采集、传输装配、策略实际执行、InputSink 的最终投递以及输出隐私隔离；调用 Core 的通用抽象。
- **四接入前端**负责生命周期入口、属性/配置与状态映射；不能持有自己的第二份 capture/transport/authorization engine。Widgets/Quick 是 Runtime 目标适配维度，而不是第五、第六接入。
- **宿主 HMI 应用**是 Target/Scope、现场优先及最终权限策略的数据权威；策略的设备端有效性判断由 Shared Runtime 内部强制实施，Gateway/Edge 不得代替。
- QPA 私有 API 仅属于受认证的 QPA 入口；`qt_window_system_input.cpp` 的窗口系统输入/高 DPI Qt Private 例外按 AR-04 单独受控，不可传染至 Core、公共 SDK 或全部 Runtime。

## 3. F-01 Core 模块：稳定抽象（明确不承担业务许可）

| 接口/对象 | 当前事实/保留契约 | 需要的设计保护 |
| --- | --- | --- |
| `Session` | Config、capture、transport、input sink、生命周期、统计；Stopped 状态才能替换 capture/transport | `start()` 不开放未经 Runtime 授权的 listener；`stop()` 不等待远端业务，不允许旧回调跨 generation 进入新 run |
| `CaptureSource` | `requestFrame()` 异步有界，`frameReady` 任何后端线程；`stop()` 后回调静默 | 不在 callback 做网络/编码/策略阻塞；丢失 Target 的故障语义规范化 |
| `RemoteFrame` / FrameStorage | Owned storage + damage/PTS/FrameId，生产者与消费者的存储生存期明确 | 存储允许被设备端共享基础采集，但“共享 storage”不表示“每客户端可见相同像素” |
| `Transport` | `enqueueFrame(RemoteFrame)` 必须有界非阻塞；传输保留自己的 per-client queue | Native/RFB 必须分别实现 transport/profile；传输侧做授权的设备会话封装但不可向 Core 引入协议依赖 |
| `InputEvent` / `InputSink` | 标准化 pointer/key/text，目标适配投递到 Qt 线程；`setInputSink` 可动态替换 | 现有 `InputEvent` **不具备 AccessSessionId/GrantEpoch**；未来授权检查必须保留可信会话上下文，不能通过未认证的全局 onInput handler 假定身份 |
| bounded mailbox | DropOldest 默认，ProducerThrottle 可选；Core 等待帧容量 N，worker-owned 至多额外 1 帧 | 只可在**编码/加密之前且不破坏差量基线**丢弃/合并过时画面；密文 byte relay 不在 Core 帧级背压之列 |

**F-01 验收设计规则**：不新增 Qt/加密协议/GUI dependency；不把业务授权序号塞进全局 InputEvent 公共定义来满足一次 PoC；如因精确实现需要扩展 Core 内部上下文，须说明 ABI/API 和 ADR 影响后另经审查。

## 4. F-02 Shared Runtime：共享与分层

### 4.1 设备端目标实现的内部逻辑职责

| 单元（逻辑名，不指定正式类名） | 唯一责任 | 上下游 |
| --- | --- | --- |
| `RuntimeInstanceCoordinator` | 统一 start/stop/rebind/epoch，保护旧 run，提供前端一致快照 | facade → Core/Qt/transport |
| `DeploymentAdmission` | 监听前校验可信配置、宿主身份、允许 Target/endpoint、安全模式和初始只读 | Host/Policy → 各 transport listener |
| `TargetScopeRegistry` | 规范 TargetId/ScopeId 和宿主对象生存期、授权视域、Scope 冲突关系 | Host → Frame/Policy/Input |
| `FrameSourceRegistry` | 依 Target/source 共享基础采集、维持存储引用和成本核算 | Qt Target → Core / publishers |
| `AccessSessionRegistry` | 认证连接 → 独立 AccessSessionId、设备身份绑定、Target/Scope/Grant 的权威关联 | Native/RFB → Policy |
| `PolicySnapshotProvider` | 得到宿主受信的当前 View/Control/Privacy/Revision 状态；变更可观察 | Host → Runtime |
| `PerSessionFramePublisher` | 授权过滤、按会话遮罩/水印、Full/Delta 基线/缓存废止；输出数据只给拥有 ViewGrant 的连接 | FrameSource + Policy → transport |
| `InputAdmissionFence` | 可信认证上下文检查、入队/GUI 最终执行前重判；撤权清理按下键、触点和队列 | transport + Policy → Qt InputSink |
| `RuntimeDiagnostics` | 分层状态和脱敏计数；不泄漏密码、画面和未授权 Target 名称 | Runtime → facades/Tool |

**代码归属推导（候选）**：策略语义不改 Core 的现有 transport-neutral API，先放在 Runtime 侧的内部 policy/session/egress/input seam；Native 与 RFB 各自持有网络握手/解析/序列化和 per-connection 资源。正式类名、跨动态库 ABI、线程模型及每目标 Source 复用策略须另做可量化选择，不能从此表直接生成公开 API。

### 4.2 共享底图与逐连接受保护输出

1. `FrameSourceRegistry` 可复用宿主当前合法采集帧；基础帧必须带可追溯 Target/source 及 generation，不视为“任何 Viewer 已被授权”。
2. `PerSessionFramePublisher` **先**读取当前会话许可、Scope 与 PrivacyRevision，**再**决定输出哪些像素、遮罩/水印、Full/Delta；不得先把原始敏感帧放进每客户端待发队列后再查询权限。
3. 对两个 Viewer 不同的隐私/水印，不得复用同一已装饰帧的缓存；仅在“政策身份完全一致且可证明”时才可考虑安全去重（候选优化，不能作为默认捷径）。
4. PolicyRevision/Target epoch 改变时，撤销受限未发送数据、失效原 Delta baseline；新 ViewGrant 必须从授权的 Full 基线起步。缓存失效处理顺序在 D1/F05 评审中确认。
5. 在设备编码/加密前可基于合法的基线合并/丢过时帧；**Gateway/Edge** 只能原序字节背压或整体断开，绝不尝试帧语义重编码。

### 4.3 状态维度绝不混为一个布尔值

- `RuntimeState`：Stopped / Starting / Running / Stopping / Faulted / Unavailable；用于设备端 Runtime 的真实生命周期。
- `ListenerState`：NotBound / Admitted / Bound / Closed；Bound 只表示监听，**不表示客户端认证**。
- `AccessSessionState`：Connected / Authenticating / Authenticated / ViewAuthorized / ControlAuthorized / Revoking / Closed；RFB 原生认证弱能力需按实际协议降级语义显式记录，不能伪造用户身份。
- `TargetState`：Registered / AuthorizedForSession / Lost / PolicyChanged。
- `InputState`：Disabled / ViewOnly / ControlGranted / Revoking；受当前 Scope/epoch 约束。
- `FrameState`：BaselineMissing / FullReady / DeltaPermitted / Invalidated，不能通过状态“Running”推断已发出可信画面。

新状态域是**内部设计候选**，不是现有 Runtime 枚举/API 已增加这些值。

## 5. F-03 四种接入方式的映射合同

| 前端 | 请求进入形式 | 前端有权处理 | 前端禁止处理 |
| --- | --- | --- | --- |
| C++ `RemoteAccess` | 构造/配置/`start()`；显式 Qt target | 对 Runtime 请求进行类型校验、展示状态与错误 | new Native wire、拥有认证 Grant、重开 Listener 绕过准入 |
| QML `RemoteAccess` | `enabled` 与 `componentComplete()` 后启动，Qt target + 信号 | 维护 QML 属性/通知与 QML 线程交付 | 在属性绑定时提前打开网络、复制 Runtime 授权引擎 |
| Generic Plugin | Qt 公共 QGenericPlugin 的启动参数 | 解析和拒绝歧义配置，转为同一 Runtime 内部请求 | 无授权自动共享网段、依赖 QPA private/单独采集 |
| QPA Factory Trampoline | 精确 Kit/private ABI shim，委托原生 QPA | 解析专属启动参数、保证原生平台语义 | 将设备权限、合成/采集重新搬回前端；替换厂商 QPA 实现 |

四入口共享同一失败类别和设备安全决定。**兼容性出门条件**：对相同 Target、部署授权、Scope、监听配置，四入口只能在 bootstrap 或配置可表达范围上有差异，不得出现 C++ 严格拒绝而 Generic/QPA 绕过的权限路径。

- 已安装 `RemoteAccess` public API 不是默认拥有可表达 V8 所有未来身份/Scope 的能力；宿主如何配置复杂策略是独立设计项，不准借一行新 setter 偷偷改变现有 ABI。
- QPA/Qt Private 的精确 Qt Kit 验证归 X-01；不能把 Qt6.8 通过推导为 Qt5.15/Qt6.12/ARM。
- Host 的“有权调用 `start()`”只是请求；H-02a 生效后 `DeploymentAdmission` 是真正创建任何监听前的必要条件，保留 RFB getter 默认 `0.0.0.0:5921` 的语义。

## 6. F-06 设备授权：主体、状态、不变量与策略快照

### 6.1 最小逻辑对象（**非** Native wire / 公共 C++ 类型）

| 对象 | 可信来源 | 生命周期/使用位置 |
| --- | --- | --- |
| `DeploymentProfile` | 本机宿主受信交付配置/批准来源 | 监听启动/重绑前核验；不可从远程 viewer 参数自动生成 |
| `TargetDescriptor` / `ScopeDescriptor` | 宿主 Qt App 注册、当前目标及运行时状态 | 任一 Target 消失或跨 Scope 变更使旧权限失效 |
| `ConnectionSecurityContext` | 独立协议的已验证连接（Native device TLS/认证，RFB 按真实能力分类） | 每连接/每认证代次，不能从 RoutePermit/用户名字符串伪造 |
| `AccessSessionRecord` | HMI 本地 Registry | 绑定 Connection、Target、Scope、获准角色、授权版本，关闭后不可复用 ID |
| `PolicySnapshot` | 宿主授权决策 + Runtime 单调版本 | 原子地包含 policy revision、许可范围、privacy/output 决定与有效期 |
| `ViewGrant` / `ControlGrant` | HMI 最终策略检查 | View 与 Control 独立；Control 不由 Gateway/Edge 的 routeIntent 自动获得 |
| `InputAdmissionContext` | HMI 可信会话、已校验的协议字段、当前 Runtime/Grant epoch | 用于进入队列与 Qt GUI 最终投递两次检查；拒绝陈旧/跨会话请求 |
| `OutputPolicyContext` | HMI 会话当前 Target/Scope/PolicyRevision/遮罩 | 用于输出前过滤和队列清除；不是客户端展示建议 |

这里的 `GrantEpoch` 等语义由 V8 已批准设计承担；**不得擅自添加/调整 V8 附录 A 的消息、字段或编号**。Runtime 内部传递已验证的会话/策略上下文，不等于 Native wire 必须增加字段。

### 6.2 必须满足的权限不变量

1. **默认零权限**：没有可信部署准入不能监听；仅网络连通/证书通过不构成认证；仅认证不构成 ViewGrant；仅 ViewGrant 不构成 ControlGrant。
2. **单 Scope 控制独占**：同一应用 Scope 默认至多一名远程操作者；多个客户端可合法查看，宿主单独批准多个不冲突 Scope 后可独立授予各自控制；网络层并发 ≠ 控制权。
3. **最终裁决在 HMI**：Gateway Identity、Edge ACL、RoutePermit 只能限制谁能建立受管路由；不签发设备 View/ControlGrant，也不能绕过本机物理操作者。
4. **最终输入检查**：协议接收时一次、Qt GUI 投递前一次，检查当前 Session/Target/Scope/RuntimeEpoch/GrantEpoch/PolicyRevision；既有纯全局 `InputEvent` 不能作为认证上下文来源。
5. **撤权线性化点**：HMI 发布新的当前政策/epoch 后，旧请求无权进入/完成新的设备端操作；对已经通过前期准入但尚未执行的输入，必须在最终栅栏拒绝并正确平衡 held key/button。必要时重置所涉目标队列，不能“重连后续跑”。
6. **输出隐私**：没有当前 ViewGrant 时发出零敏感像素；遮罩/Target 权限变动时所有旧待发敏感帧/基线及时失效，不因其他 Viewer 仍在场而继续发送原始内容。
7. **现场优先**：宿主 Qt 应用、现场输入及生产性能不因公网 Route 或远程控制请求而被阻塞；输入被拒后不得用业务操作重试“补偿”。
8. **断线不重放**：新 TLS/Native 连接必须生成新设备会话、重新确认授权；旧输入、告警确认、复位、写参数等未确认业务结果为 `OUTCOME_UNKNOWN`，不默认重试执行。
9. **安全拒绝不可自动降级**：Native 设备身份/TLS/认证失败后，Managed/Public 路径不能自动切到明文 RFB；合法可信 LAN Direct/RFB 的支持范围另行陈述。

### 6.3 设备授权/撤权时序（设计级）

~~~mermaid
sequenceDiagram
  participant Host as HMI 宿主与现场策略
  participant Runtime as Shared Runtime / Policy Registry
  participant Protocol as Native/RFB 协议会话
  participant Output as 帧发布器
  participant GUI as Qt 最终输入适配器
  Host->>Runtime: 提供可信部署及目标范围
  Runtime->>Runtime: DeploymentAdmission 成功后才允许 listener
  Protocol->>Runtime: 协议认证已验证的 ConnectionContext
  Runtime->>Runtime: AccessSession / Target / Scope 资格检查
  Runtime-->>Protocol: ViewGrant 或拒绝
  Runtime->>Output: 会话输出 PolicySnapshot
  Output-->>Protocol: 仅发送当前许可且已过滤的画面
  Protocol->>Runtime: 请求 ControlGrant
  Runtime->>Host: 检查现场优先、Scope 独占和权限期限
  Host-->>Runtime: 批准或拒绝
  Protocol->>Runtime: Input + 可信会话上下文
  Runtime->>Runtime: AdmissionFence 检查
  Runtime->>GUI: 携带不可伪造的许可/epoch 上下文排队
  Host->>Runtime: 现场撤权 / Target 移除 / PrivacyRevision++
  Runtime->>Output: 清除旧待发敏感像素、失效增量基线
  Runtime->>GUI: 失效旧队列，平衡 held state
  GUI->>Runtime: 最终投递前重新检查当前 epoch/Scope/Grant
  Runtime-->>GUI: 旧输入一律 DENY
~~~

**顺序约束**：发布撤权后的“旧输入不得执行”是设备端最终强制条件，不能只依赖异步发送给 Viewer/Gateway 的撤权事件；撤权后的画面缓存释放必须有明确资源所有权。与真正 GUI 线程并发的线性化实现点需进一步审核并做确定性竞态验证，当前不是实现事实。

## 7. 启动、重绑、退出与故障模型

### 7.1 未来新架构启动

~~~text
Constructed / Stopped
 -> ValidateConfig & Host-target identity
 -> DeploymentAdmission(APPROVED / REJECTED)
 -> Compose target source, policy and enabled transports
 -> Bind admitted RFB and/or Native listeners only
 -> Runtime Running (listener reality only)
 -> per-connection authentication -> Target/Scope ViewGrant -> independent ControlGrant
~~~

- `DeploymentAdmission=REJECTED` 时 **零新 listener**；不回退地址、协议或安全模式，不复活上次接受的权限；失败返回设备端具体可诊断但脱敏原因。
- **H-DUAL=A 已获 Human 批准**：仅对宿主显式声明为必需的全部 RFB/Native 端点执行统一 DeploymentAdmission 和原子启动；任何一个必需端点失败，启动事务回滚/停止其他本次新建监听并报告整体启动失败，不能向外发布伪“部分成功”。RFB-only/Native-only 是各自合法独立 profile。运行期单协议故障隔离其他合法 session，但必须报告真实 DEGRADED，不能静默回落为较弱协议。
- RFB `0.0.0.0:5921` 是保留的**配置 getter 默认值**，不是 H-02a 的部署授权；现行旧版监听真实行为按 `docs/security.md` 如实记录。
- Interface unavailable 维持真实 `Unavailable`：没有 listener，不等于合法地“已授权用户在线”。

### 7.1.1 H-DUAL=A 启动原子性的可实现合同（设计，不是当前代码）

- `PREPARE`：Host 对部署签名/Target/Scope、RFB-only/Native-only/Dual 声明及**全部 required 端点**执行同一个策略代际准入；构建每协议的独立 socket、证书材料与会话表，但**不得发布 Running/可用权限**。任何准备失败均 `ABORT` 并释放本事务资源。
- `COMMIT`：仅全部 required 协议准备就绪后，以一个受信 RuntimeEpoch/AdmissionSnapshot 将会话接纳总闸门从 `BLOCKED` 提交为 `ACTIVE`；外部 API 才可报告 `Running`。对在准备/提交期间试探性连入的 socket，不得发送设备画面、认证秘密或接受输入。
- `ABORT`：任一必需端口冲突、TLS 身份缺失或 Host 策略失败时，全部关闭本轮新建 socket/会话，报告 `START_ABORTED` 与真实原因，不得留下可服务的 RFB 或 Native 半启动实例；不得自动改为不加密 RFB。
- `DEGRADED`：**只指已经 COMMIT 后**一个协议因运行时故障失效，其他端点经过现有 Host 授权、未受影响且仍安全时可以继续；健康/监听/会话状态按协议分别如实报告，不隐蔽故障。
- TCP 多端口在 OS 层无法物理实现“两个 `listen()` 指令同一纳秒完成”的绝对保证。H-DUAL=A 要求的是**对外服务、授权和 Running 可见性的事务性全有或全无**，不能虚构网络原子性。设计验证需覆盖端口占用、协议准备后失败、并发连接抢入、策略更新同时回滚、四接入和旧 Qt Kit。

**与 H-RFB=A 的配合**：仅当 Host 明确开启 legacy RFB 输入且当前 Target/Scope/ControlArbiter 仍许可，该 RFB session 才可提交输入；标准 RFB 未定义 Native Grant 状态，因此 Viewer 不得显示“已获得个人 ControlGrant”。任何会话撤销均以 H-REVOKE=A 的当前设备 epoch 作为最终禁止执行/输出栅栏，而不是依赖 GUI 远端通知。

### 7.2 运行期重绑、断线与取消

- Runtime 旧 generation 与新 generation 完全隔离；旧网络/采集/输入回调不可操作新会话。现有 `AccessInstance::Impl::composeAndStart()` 先 `shutdownRuntime()` 再新建 Core Session 的旧代际隔离机制可复用，但未来必须补充当前 PolicySnapshot 的重评估。
- Target 被销毁/注销，作用于相关 Scope 与 Session，不可误杀其他合法独立目标；设备用户注销不自动推出 Gateway 站点权限，也不证明 HMI 授权保持。
- 停止时先阻断新认证/输入、撤销当前 grant、清理受限画面与 held state，再有界关闭传输/采集。设备侧不能因慢网络让 HMI 本地退出卡住。
- 任何协议单独故障不得自动授权另一协议；Direct 与 Managed 的网络可用性/许可状态独立。

### 7.3 错误类别应有唯一权威来源

| 错误类别 | 判断方 | UI/诊断允许陈述 |
| --- | --- | --- |
| `DEPLOYMENT_DENIED` | HMI DeploymentAdmission | 配置/可信部署准入失败，未监听 |
| `LISTENER_UNAVAILABLE` | Runtime/网络绑定 | 设备当前未监听，非设备授权 |
| `DEVICE_AUTH_FAILED` | Native/RFB 对应协议 | 连接认证失败，不自动回退 |
| `TARGET_SCOPE_DENIED` | HMI Policy | 当前目标不可见或不被授权 |
| `VIEW_DENIED` / `CONTROL_DENIED` | HMI Grant | 查看/控制分别拒绝，现有连接无对应权利 |
| `POLICY_REVOKED` | HMI Policy epoch | 旧会话/输入/输出不得继续 |
| `RESOURCE_LIMIT` | 发生限额的设备模块 | 仅对应连接或目标失败，不将业务改为“成功” |
| `TARGET_LOST` | Runtime target adapter | 目标已失效，本地宿主应用不受远端恢复影响 |
| `UNSUPPORTED_CAPABILITY` | 协议/构建能力 | 当前构建未实现，不能改变 wire 隐式兼容 |

该表是未来**内部错误分类候选**，不是新增 `RemoteAccessErrorCode` 枚举。如何对外无损映射见 D1-DEC-05。

## 8. 内部接口签名与原子语义（候选，不是可编译 API）

这些接口描述“什么需要被证明”，不要求新增同名类或扩大公共 SDK：

| 候选调用语义 | 输入 / 结果 | 失败/竞态约束 |
| --- | --- | --- |
| `admitDeployment(hostIdentity, desiredBindings, securityMode, targetPolicyRef)` | `ALLOW(snapshotRevision)` / `DENY(reason)` | 必须在任何协议 bind 之前；无可信材料拒绝 |
| `enumerateTargets(accessSession)` | 当前获准 TargetDescriptors | 未认证/未知 Scope = 不泄漏 Target |
| `authorizeView(accessSession,target,scope)` | ViewGrant + 当前 policy snapshot | policy 未知/目标消失 = DENY |
| `requestControl(accessSession,scope,intent)` | ControlGrant/当前 GrantEpoch 或 DENY | 宿主现场优先，单 Scope 默认独占 |
| `admitInput(accessSession,input,epoch)` | 可信入队 ticket / DENY | 当前政策原子检查，只有已认证会话可产生 ticket |
| `deliverInput(ticket,livePolicy)` | 最终 Qt 投递 / REVOKED | **在 GUI 实际投递前**重验 epoch，不复用旧 snapshot 掩盖撤权 |
| `prepareOutput(accessSession,sourceFrame,policy)` | AllowedFrame/WAIT_FULL/DENY | 先授权及按会话过滤，再编码/加密；拒绝时丢受限缓存 |
| `revoke(policyChange)` | 发布新 epoch、invalidated session/frames/input | 具有设备端不可越过的生效点；事件通知可异步但不能延迟拒绝 |
| `closeAccessSession(id,reason)` | 幂等释放目标状态/held input/帧基线/连接 | 断开不复活旧 grant，也不重复执行业务命令 |

## 9. 资源与可观测预算（设计公式，不伪称测量 PASS）

- Core completed-frame 所有权上限：`waiting_capacity + 1 dispatcher-owned`（当前 `Session` 实现注释）；Transport/Publisher/client 的帧和网络排队需要**单独**加总，不得拿 Core 限额冒充全进程预算。
- Qt 输入路径当前存在普通 pending 64 与受保护释放储备 136 的 **特定内部实现**（`input_mailbox_admission.hpp`）；它不是所有 Native 客户端/Scope/协议的全局内存预算。新增会话身份上下文和撤权队列上限需明确加总/证明。
- 设备总峰值模型（设计候选）：
  `RSS_peak = Core_frame_storage + runtime_capture_inflight + per_session_filtered_frames + protocol_queues + input_queues + TLS_state + Qt_adapters + other_process_baseline`。
  重复引用共享帧须扣除双计数，但不能忽略独立遮罩缓存、TLS 密文缓冲和 OS socket receive buffers。
- V8 设备端 128 MiB 管理预算是**目标**，必须以应用的真实 Kit、Target、窗口系统和适用负载证明，当前 `NOT_QUALIFIED`；高动态场景不得用刷新不及时掩盖 CPU/帧龄问题。
- 只记录脱敏指标：授权拒绝计数、Frame 过滤/丢弃原因、Grant/Policy 代际与 Target 失效状态、缓冲水位及输入释放次数；禁止日志包含口令、明文屏幕、键鼠内容或云端 token。

## 10. F-04/F-05、Viewer 和 Managed 的跨模块边界

- **F-04 标准 RFB**：维护标准客户端互通、现有 RFB 3.8 真实支持事实以及准确的安全等级；当前 VNCAuth 不加密、3.7 历史兼容口径有 #158 未决。RFB 的 Device Grant 等价实施方式/旧客户端支持模型须另行详审，不能虚构 RFB 新标准字段。
- **F-05 Native 1.0**：唯一 wire 由 V8 附录 A/B 决定，采用完整独立协议；已有 `managed-edge-detail-05-hmi-native-trust` 是该设备安全方向的详细设计候选，下一轮从本稿延伸具体 Target/Grant/input/Frame 协议状态机，**不再生成平行 Native 1.0 变体**。
- **V-01 HyRemoteView**：客户端有自己的独立设备信任/Render/input FSM；Gateway Route Ready 不等于 HMI ViewGrant 或 ControlGrant。View 不能依赖 Core 安全枚举来跳过实际设备认证。
- **E-01/G-01 Edge/Gateway**：只能中转已登记 HMI 的 FIFO 设备协议字节；不能作为全局 HMI 客户端/FrameSink、第二画面采集器或输入授权者。
- **HyRemoteTool**：可以展示 SDK/Qt/安全配置/有权读取的诊断状态，但不得下发伪造的 Grant 或默默替客户打开公网 listener。

## 11. 设计验收矩阵（全部是未来实现的测试规格）

| ID | 需证明的事件 | PASS 条件 / 当前状态 |
| --- | --- | --- |
| FW-01 | C++、QML、Generic、QPA 四入口同一配置目标 | 启动、配置拒绝、错误、输入/帧语义一致；`DESIGN_ONLY` |
| FW-02 | 无 DeploymentProfile / 错误网络/目标/安全来源 | RFB/Native 均无新 listener，RFB 默认 getter 不变；`NOT_IMPLEMENTED` |
| FW-03 | Native-only、RFB-only、Dual，以及单协议失败 | 不创建第二套 Core/Runtime，按明确原子启动合同处理；`NOT_RUN` |
| FW-04 | 多 viewer 同 Scope 请求控制，另一个 Scope 独立 | 一 Scope 默认一个授权控制，现场输入优先；`NOT_RUN` |
| FW-05 | ViewGrant 通过、ControlGrant 拒绝 | 无远程输入被投递到 Qt；`NOT_RUN` |
| FW-06 | 输入已进 GUI 队列时宿主撤权/Target 销毁 | 旧输入最终校验拒绝、held key/button 安全释放；`NOT_RUN` |
| FW-07 | 不同会话不同遮罩/水印/Target 权限 | 只向各自授权的连接发当前过滤后的画面；无跨会话缓存泄漏；`NOT_RUN` |
| FW-08 | PolicyRevision 改变且慢客户端存在积压 | 旧发送队列/Delta baseline 失效，重新建 Full；`NOT_RUN` |
| FW-09 | listener 断线、接口 Unavailable、Runtime stop/rebind | 无旧 generation 回调或会话复活、无自动降级；`NOT_RUN` |
| FW-10 | Qt5.15 / Qt6.8 的 Widgets + Qt Quick 与四入口矩阵 | 各实际 Kit 依规则认证，Native 与 Qt private 不扩散到 Core；`NOT_RUN` |
| FW-11 | 工厂 WAN/Gateway 不可用 | HMI 现场业务、合法 Direct 不受影响；`NOT_RUN` |
| FW-12 | 峰值资源与慢 viewer | Runtime/Qt/network 全口径有界、无静态高负载自证问题；`NOT_RUN` |

以上 `NOT_RUN` 不否认已有历史 RFB/Qt 单元测试；表示**未来 V8 权限/双协议新架构的端到端资格未取得**，不是“仓库现有所有测试均未运行”。

## 12. 必须在设计阶段做出的选择（开放讨论，Human 裁决实质变更）

| ID | 设计议题 | 候选方向 / 需有的证据 | 会否触及已冻结基线 |
| --- | --- | --- | --- |
| D1-DEC-01 | 多 Target 的 Source/Session 基数与授权索引 | 以 per-Target 共享 Source、每连接独立 Policy/Transport context 为方向；对当前 AccessInstance 的生命周期、跨目标资源和 GUI 线程做不变性分析 | 不应；若改 Core 公开 Session 语义需上升 |
| D1-DEC-02 | RFB/Native Dual 启动/局部故障 | **H-DUAL=A 已批准**：必需端点在同一受信部署事务中全部成功才 Running，任何缺失都回滚启动；运行期局部故障隔离且不自动弱协议回退 | 产品选择已关闭；实际回滚语义、监听原子可见性、错误与 Qt Kit 负测仍需技术审查 |
| D1-DEC-03 | Host PolicyProvider 交互与 revocation 线性化 | 候选只读版本化 snapshot + 原子更新 + GUI 投递栅栏；需要量化锁竞争与跨 GUI 线程最终检查 | 不应；若改变宿主授权语义需上升 |
| D1-DEC-04 | 不同 Viewer 的隐私输出管线与缓存生命周期 | 候选“共享无权限基础帧、每会话过滤、基线独立、撤权清零”；对当前共享 `RfbFrameDecorator` 运行序和成本做设计签收 | 不应；若削弱已批准隐私则禁止 |
| D1-DEC-05 | 将内部设备许可错误投射到四入口 API | 先保留现有公共枚举/API；用稳定诊断详情和能力协商表示新错误。是否需要公开新字段，待兼容/宿主需求设计后单独审批 | 可能：public SDK/ABI 改动需审批 |
| D1-DEC-06 | 标准 RFB 权限与 Scope | **H-RFB=A 已批准**：保留已发布 Host 明确启用 remote input 的 RFB 能力；新架构默认只读，Host 对具体 Scope/Target 授权并与 Native 共享默认单 Controller 仲裁；共享 VNCAuth 不当个人身份 | 产品选择已关闭；ServerInit/handshake 信息边界与旧 Viewer 互通需要技术/验收证据 |
| D1-DEC-07 | Host 本地可信部署来源、设备凭据与失效后恢复 | 用 V8 Host/DeviceTrustBundle 基线具体核对身份和配置来源、吊销/证书变更，不接受不受信 Gateway 自行造信任 | 实质差异必须 Human 决策 |
| D1-DEC-08 | V15/V8 正式原文与 Native A/B 签字追踪 | 取得经审核的完整原文，建立条款→对象→流程→FW-验收 ID 矩阵；现引用还不是完成追踪 | 本身不改架构 |

所有候选选择必须在全产品一致性复核后，确定属于“已批准约束的细化”还是“材料性的架构变更”，**不能在代码里先选再请求追认**。

## 13. 接下来应移交给 F-04/F-05 的设计输入

1. 基于本稿 Core/Runtime 结构，分别列出 RFB 和 Native 的监听前 Admission、握手/身份、Target/View/Control、帧/输入、重连/撤销的 FSM。Native 必须严格复用 V8 A/B，RFB 不另造私有标准消息。
2. 逐连接状态与共享帧缓存/最终输入交付的权限映射；给出 Scope 冲突/现场优先、旧 delta 和旧输入在撤权期间的竞争条件。
3. 标注协议支持、兼容和安全等级，输出“能做/不能做/设计未选/需实测”四类结果。
4. 全部图表先在 Markdown 设计审查，待全产品模块和实施计划审查完毕才规划正式 Word 的目录、图表、版式与正文落地。

## 14. 本轮设计审查 / 阶段状态

- [x] 已核对 `develop` 的 Core/Runtime/facades/当前 RFB 安全代码事实。
- [x] F-01/02/03/06 的依赖方向、数据权威、成功/拒绝/撤权/故障设计边界已成稿。
- [x] 以未来候选而不是现行实现陈述 DeploymentAdmission、Native 设备信任和会话级授权。
- [x] 与 Stage04/05 的透明转发、HMI 最终授权边界没有相反要求。
- [x] Human 提供完整且历史 SHA 匹配的 V15/V8 送审稿，确认与已批准正式版正文相同；W00 已完成 V15 56、V8 99 小节及 Native A/B 21 type 索引。
- [ ] V15/V8 逐原子要求→最终状态/错误/TestID/Owner 仍需审签；P-A/A 数值已获 Human 批准但真互通/Qt 负例未执行。
- [ ] D1-DEC-01..08 中需进一步技术取舍、兼容和产品审批的内容签收。
- [ ] F-04/F-05 协议详设、F-01/02/03/06 后续跨协议一致性复核。
- [ ] 全产品模块设计冻结、研发实施计划、正式实现准入（均未发生）。

**本轮结论**：`D1_FRAMEWORK_CROSS_MODULE_CONTRACT=DESIGN_CANDIDATE`；`D1_COMPLETE=NO`；`NATIVE_E2E=NOT_IMPLEMENTED`；`FORMAL_PRODUCT_CODE=NOT_AUTHORIZED`。此稿不得作为独立的 Native 1.0 规范或新实施工单使用。
