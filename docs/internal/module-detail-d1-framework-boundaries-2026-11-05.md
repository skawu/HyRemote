# D1｜Framework 设备端模块详细设计：Core / Shared Runtime / 四接入 / 设备授权（候选 V1）

> **2026-11-05 Human 决策已生效**：[Human #843 决策记录](https://github.com/skawu/HyRemote/issues/843#issuecomment-6646988698)。相关产品/安全取舍选 A；本文初始“待 Human 选择”语句如与本次批准冲突，以审批后的具体条款为准。批准不等于 V15/V8 原文审计、产品实现或设计冻结。

> **归属**：[全产品模块设计总控 #843](https://github.com/skawu/HyRemote/issues/843)，D1 首轮；参考 [#830](https://github.com/skawu/HyRemote/issues/830) 的 HMI Native 安全设计与已合并 Stage04/05。  
> **批准的上位基线**：HYR-PD-001/V15（SHA-256 `d0408f333d4b9aec186067cd159ff8a4a2206ef66d58243fd4c5cd10ffb82cf1`）、HYR-ARCH-001/V8（SHA-256 `1101d5546cfc0ab63e4cfde809ba4b0530e1048ed3760b56b8c9e7be3de6803a`），均已获 Human 批准，见 #827/#830；阶段02 G7 与 ADR-0007/0008 保留为上位先行约束。  
> **代码事实快照**：`skawu/HyRemote@develop:36ac22d5a2cea287e655d13129114db223db0087`（2026-11-05），只作差距分析，不代表所有历史发布二进制。  
> **效力**：`DESIGN_CANDIDATE / NOT_FROZEN / NO_CODE_CHANGE / NO_IMPLEMENTATION_ADMISSION`。本稿只冻结“设计议题的覆盖范围”，**不冻结新增 public SDK、C++ API、Native wire 或类名**。

## 0. 设计目标、来源完整性与非目标

本稿交付 #843 中 F-01 Core、F-02 Shared Runtime、F-03 四个并列接入前端、F-06 设备部署授权/会话许可/隐私及最终输入栅栏的**跨模块职责、内部接口语义、状态与失败关闭契约**，作为下一轮 F-04 RFB/F-05 Native 协议详设和 V-01 View 客户端设计的稳定输入。

**来源完整性说明（按 2026-11-05 后续 W00 审计更新）**：本仓库可直接阅读 ADR-0007/0008、`docs/architecture.md`、`docs/security.md`、正式冻结记录及源代码；正式版 DOCX 二进制没有同步成 GitHub 规范全文，但 Human 已提供与历史送审稿 SHA 精确匹配、确认正文等效的 V15/V8 完整原文。见 [来源审计](module-detail-source-audit-v15-v8-2026-11-05.md)：V15 56 条、V8 99 节和附录 A/B 的 21 类 Native 字段已作来源核对；原 V8 未绑定的数值和 TLS-first 解释已有 [Human P-A/A 受控增补](module-detail-w00-native-v8-delta01-approved-2026-11-05.md)。**这仍不等于 56/99 逐原子合同审签、Native E2E 真实验证或设计冻结**；本稿最初代码快照及历史差距作为当时事实保留，新合同与上位审批冲突必须提交 Human 受控修订。

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

### 4.4 F-02 运行时对象、线程所有权与生命周期（详细设计闭合候选）

本节将 4.1 的逻辑责任转换为**可直接实施和审查的内部合同**。类型名为内部设计记号，非已存在 C++ 声明、Native 新字段或公开 SDK ABI；部署许可、四入口一 Runtime、RFB/Native 双独立协议、H-REVOKE=A 均继续以 V15/V8 和已批准方案为准。

| 对象/资源 | 创建及唯一所有者 | 跨线程交换 | 销毁及不可复活条件 |
| --- | --- | --- | --- |
| `RuntimeGeneration` | F-02 Coordinator 每次 Start/Rebind 生成不可复用 generation | listener、Target、回调、队列和 Qt 投递均携带当前 generation | Stop/故障/重绑先关闭 admission；旧代次 callback 一律拒绝 |
| `TargetRecord` | F-02 TargetScopeRegistry 持有 ID、Scope 冲突图、Qt 对象弱引用/存活令牌 | GUI 目标访问只能在正确 Qt 对象线程；网络线程只持不可变描述/ID | target 丢失/重建必须新 incarnation；原 ID/授权/采集缓存不继承 |
| `BaseFrameRef` | 每 Target F-02 FrameSourceRegistry 持有受控帧存储与 generation | 采集线程只交付带 Target/生成代次/PTS 的 owned frame；不得借用已销毁 Qt 内存 | 最后一有效消费者释放；不是任何会话输出许可 |
| `OutputSessionRecord` | F-02 AccessSessionRegistry 为每条真实连接唯一建立 | policy/encoder 持会话代次、Target/Scope 与过滤快照；网络并非授权源 | Close/Revoked/PolicyChange 失效所有未发送过滤帧与基线 |
| `SessionEgressQueue` | 对应 F-04 RFB 或 F-05 Native 连接独占 | **仅授权后的帧/协议 payload** 进入各自有界队列；TCP/TLS 发送队列再单独计费 | 撤权清可控队列，无法清已交 OS/TLS in-flight 时必要断链 |
| `QtInputTicket` | F-06 InputAdmissionFence 创建不可客户端伪造的内部临时票据 | GUI 最终处理时凭 Session、Target、Scope、RuntimeEpoch、GrantEpoch、PolicyRevision 重验 | policy/target/session 变化后不可跨事件循环重用，完成/拒绝一次消费 |
| `RuntimeMetricSnapshot` | F-02 Diagnostics 单点聚合 | 仅脱敏计数、队列水位、状态与明确来源的时戳 | 不包含密码、像素、输入全文或可复用 Grant |

**线程边界**：Qt Target 的生存期/输入投递由所属 GUI 线程拥有；采集完成回调可从后端线程到达，但只递送不可变持有的 FrameRef；策略写入经 Host 受信发布路径，网络协议会话在各自事件循环解析，不能在 GUI 线程等待 socket/TLS/编码。跨线程任务一律附 `RuntimeGeneration` 和对象存活令牌，并在消费点验证；取消标志不等于任务完成，stop 必须追踪持有资源的队列已失效。

**启动/停止状态合同**：

1. `STOPPED→PREPARING`：检查签名/Host Profile/Target/Scope、required listeners、Native TLS 材料；均为 **不可对外接纳业务**。单一 H-DUAL=A admission snapshot；RFB-only/Native-only 是合法独立部署档位。
2. `PREPARING→ACTIVE`：全部 required endpoint 已准备好，提交一个 `RuntimeGeneration + AdmissionSnapshot`，**对外可服务与 Running 可见性**同时开放。多个 OS `listen()` 不承诺物理瞬时原子；准备期间任何试探连接不得获得目标、画面或输入。
3. `PREPARING→STOPPED(ABORT)`：任一必需准备失败时撤回该轮所有 socket/session、TLS 材料与弱引用，返回真实根因，不自动降为另一协议或让 GUI 生产业务停止。
4. `ACTIVE→STOPPING`：先原子关闭**新会话、新输出、新输入**的接纳门；推进 generation、作废所有在途任务及 Session；清本进程可控缓冲、释放 held，再依次撤掉协议连接/采集/Qt Target 适配，最后成为 `STOPPED`。此顺序的对外结果不依赖远端 Viewer 先 ACK。
5. `ACTIVE→DEGRADED` 仅用于**提交后**局部协议运行时损坏且其他已许可协议仍安全；状态必须如实报告，不能重做未授权的 profile 切换。目标消失、重绑或策略版本改变时按受影响 Scope 最小隔离，未经许可不能自行复活。

### 4.5 F-02 出帧流水线的原子步骤、缓冲水位与隐私隔离

```text
Qt Target capture (one source, target incarnation)
  → immutable BaseFrameRef [TargetId, Generation, PTS, damage]
  → per AccessSession current ViewGrant/Policy/Privacy gate
  → per-session mask/watermark → per-session FULL/DELTA decision
  → bounded protocol-specific encoder/cleartext queue
  → Native TLS or legacy RFB transport
  → network/OS bytes (already handed off cannot be recalled)
```

- `BaseFrameRef` 可以共享**仅未加个人权限的原始存储引用**；任何已遮罩、加水印或差分基线默认**每 Session 独占**。跨 Session 去重只能在未来证明 Target/Policy/Privacy/Identity 完全同一且撤权能同步失效的情况下单独审核，当前不作为实施默认值。
- **输出提交点**：生成/选择帧前读取 HMI 当前 PolicySnapshot；进入协议队列前再次校验当前 `RuntimeGeneration/AccessSession/Target incarnation/ViewGrant/PrivacyRevision`，失配即废弃并失效该会话的 Delta baseline。两次之间若已撤权，不可将旧帧放进可控待发队列。
- `FULL` 只有在对应权限快照已处理完整 Target 覆盖后才可标为可用基线；`DELTA` 必须引用该 Session/Target/generation 下**已完整构成**的本地基线。丢弃过时帧后无法证明差分正确，则进入 `WAIT_FULL` 并重新生成当前权限下的 FULL，不向客户端发送不连续 Delta。
- **背压域分离**：采集回调与 Core mailbox、每 Session Filter/Encoder、每协议待发、Native TLS buffers、OS socket、Qt 输入邮箱分别记账。V8 的连接数、每目标 Qt 输入容量、Native 每连接待发上界和受管 128 MiB 设备侧预算分别在其**原文适用层**校验，不将共享引用重复计费，也不把未计 TLS/OS/遮罩的 Core 限额冒充整机内存。
- **慢端处置**：只有在编码/TLS 前且可以正确恢复 FULL 的会话级帧才可丢/合并；密文 Native FIFO 不能剪字节或凭 FrameId 重排序。超过受控内存/等待上限则拒绝本会话进一步入队、报告真实 `RESOURCE_LIMIT` 并按协议关闭受影响连接；本地 HMI 与其他正常会话不被拖死。
- **撤权互锁**：Policy/GrantEpoch 生效后，不新增旧许可像素/输入、不将旧受限帧新入队；清所有仍可控的旧明文/已过滤缓存与待发送 Session 队列，旧基线失效。Native TLS/OS 已交出的字节**不保证追回**；必要时断链，第一方 View 遮旧图、禁输入。恢复查看必须以新授权、新 Full 和当前 Session 为条件。

### 4.5.1 最后出队校验与逐会话差量基线（Gate-D 审查合同）

本节只规定 F-02/F-04/F-05 的**内部发送事务**，不增加 Native 消息、逐帧 ACK 或 SDK 公共 ABI。先验授权、已完成 FULL、已排队和远端已呈现是四种不同事实。

| 阶段 | 唯一责任与提交条件 | 失败及恢复 |
| --- | --- | --- |
| 帧候选 | F-02 逐 Session Publisher 生成不可变候选，绑定 RuntimeGeneration、AccessSessionId、TargetIncarnation、GrantEpoch、PrivacyRevision、FrameGeneration、FrameId 与 baseline generation | 任一版本或 Target 不匹配时整帧丢弃，失效此 Session 的 DELTA 基线 |
| 最后明文出队 | F-04/F-05 受控发送队列在进入 TLS/Socket 前读取**当前** F-06 Policy、Session、Target、隐私快照；同连接 FULL/RECT/END 保序且不交叉 | 已撤权者禁止新增明文字节提交；未出队旧数据可删除，已部分送 TLS/OS 无法安全剪切则关闭该连接 |
| 基线有效 | 仅完整 FULL 获得保证**不被后续静默丢弃、重排**的可靠有序发送位置后，才能为该同一连接的后续 DELTA 提供内部发送基线；无逐帧 ACK，不能宣称 Viewer 已显示 | 可控队列丢任何必需更新、传输不连续或策略/尺寸/会话变更，改 WAIT_FULL；FULL 部分交 TLS 且无法修复时闭路重建新 Session |
| 资源与证据 | 独立记录清晰候选、明文队列、TLS/OS 交付、高水位、实际发送与本地不可追回边界 | SocketWrite、Ping/Pong、队列排空不构成 Viewer 已呈现/Host 业务已应用 |

**禁止绕过**：Core 的通用 Transport.enqueueFrame 既非 ViewGrant 也非可信输出许可；新受信 Runtime 的任何对外像素必须统一经过 F-06 当前授权、F-02 逐会话过滤及上述最后出队复核，不可由另一入口直接发布未过滤 BaseFrameRef。旧公开 ABI 按已发布事实另行兼容。

**反证入口**：RACE-02、FW-07/08、AFS-07/08 必须在过滤后入队前、FULL 未完整排入、FULL 部分交 TLS、DELTA 排队后丢弃/撤权四处可确定性注入。审签时至少有来源、代次、出队/发送结果、基线及拒绝恢复的预期表；全部仍 DESIGN_ONLY/NOT_RUN。

### 4.6 F-02 明确的实施接口输入与异常出口

| 合同 ID | 输入→内部输出 | 不变量与失败可观察性 | 设计验证入口 |
| --- | --- | --- | --- |
| `RT-01` | Host 配置→`prepare/commit/abort` admission | H-DUAL=A 的 required 端点对外服务全有或全无；准备连接不泄漏目标 | FW-02/03 |
| `RT-02` | Qt Target 注册/销毁→带 incarnation 的目标描述 | 一次 Qt 目标重建不继承旧 ID/Grant/Frame baseline | FW-06/09 |
| `RT-03` | CaptureSource 帧→共享 owned BaseFrameRef | 生命周期独立且不在 GUI 回调执行网络慢工作 | FW-07/12 |
| `RT-04` | BaseFrameRef + Session 当前 Policy→过滤后的帧 | 跨 Session 不串 Mask/Watermark/Scope，未知 Policy 零敏感像素 | FW-07 |
| `RT-05` | 每 Session 输出→可发送 FULL/DELTA/bounded bytes | 任何丢帧/撤权/切目标损坏基线都必须重新 FULL | FW-08/DP-08 |
| `RT-06` | Stop/Rebind/目标移除→取消所有受影响回调 | 旧 RuntimeGeneration/Qt 弱引用不在新轮复活 | FW-06/09 |
| `RT-07` | 负载/网络背压→仅局部限制或关闭连接 | 本地 Qt 业务和别的 Scope 不阻塞；OS/TLS 队列真实计入 | FW-12/XP-27 |
| `RT-08` | Host 撤权→旧输出队列与基线失效 | H-REVOKE=A；清可控缓冲但不承诺收回已交 bytes | FW-06/08/XP-25 |

**4.4–4.6 的闭合等级**：`F02_DESIGN_CONTRACT_DEFINED_FOR_REVIEW`，不是公开头文件/ABI/单元测试/Qt Kit 的实施证明。新参数、容量和类名若需进入公共 SDK，必须先独立兼容审查，而不是直接照搬本内部合同。

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

### 5.1 F-03 四入口的配置来源、调用时机与唯一 Runtime 归属

以已接受 [ADR-0007](../adr/0007-four-integration-runtime-architecture.md)、[ADR-0008](../adr/0008-stage02-human-approved-architecture-decisions.md) 及当前源码的四入口公共合同为准。本节区分**现有已发布入口事实**和**新 V8 受信部署准入的目标设计**；接口名只是内部语义，不是追加一个 `RemoteAccess` 公共 setter、第五入口或新的 Qt Private 依赖。

| 接入入口 | 当前实际配置/启动事实 | 目标 Runtime 内部动作 | 禁止和失败边界 |
| --- | --- | --- | --- |
| **Embedded C++** `src/integrations/cpp` | 已安装 `HyRemote::RemoteAccess` 持有 target、`setListenAddress/Interface/Port`、`setRemoteInputEnabled`、`setSecurityProfile`、`setSecurityConfigFile`、watermark、`start/stop`；设置要求 Stopped | Facade 将请求包装为**内部部署描述**，在 Host 权威签署的 Policy/Target/Scope 有效后申请 F-02 Coordinator 的启动；Target 可为 Widgets 或 Quick | `start()` 是申请不是无条件监听；旧配置 getter `0.0.0.0:5921` **保留但不授予绑定权**；不扩展现有公开 ABI |
| **QML** `src/integrations/qml` | `QmlRemoteAccess` 的 `enabled`、`QQmlParserStatus.componentComplete()`、QML 属性和 typed `state/error` 通知 | **组件完成并解析稳定配置后**才向同一 F-02 Runtime 模型提交启动；通知从工作线程排队到 QML 对象线程 | 属性求值/对象构造中不得开 Listener；热改目标/安全材料需遵守已有 Stopped/重启合同，不越过 Host admission |
| **Generic Plugin** `src/integrations/generic` | 标准 `QGenericPlugin`、`-plugin hyremote`/`QT_QPA_GENERIC_PLUGINS`；`address` 与 `interface` 互斥，插件只解析启动语法 | 调用 **Runtime 共用的 AutomaticAccessController** 和同一 Target/Frame/Policy 适配；继续保留原生 QPA | 启动命令行和环境变量只是意图，**不是签名部署授权**；不依赖 QPA 私有 ABI，也不能把错误参数解释成更宽松默认 |
| **QPA Factory Trampoline** `src/integrations/qpa` | `QT_QPA_PLATFORM=hyremote:...` 等加载；专属 hyremote 参数解析/剥离、选择并创建已资格化**原生** delegate | 先忠实构造原生 Qt QPA 委托并保持其平台/窗口/输入语义，再在可用的 Qt/Host 生命周期阶段委托相同 AutomaticAccessController | 不创建另一套 compositor、CaptureSource 或私有 Host 授权；native delegate 失败要区分平台失败，远控准入失败不应杀死仍可运行的原生 HMI |
| **共有 F-02 实现** | `src/runtime` 共享 Qt target/automatic/composite/input/transport，`src/core` 保持无 UI/协议依赖 | 四入口消费同一**实现与受信政策决策**，每个实际运行实例与 Session 独立可追踪；不得把“共享 Runtime 代码”偷换成不受 Host 管控的全进程无条件单例 | 不因两个入口各自成功解析配置就各开一套授权/采集/Scope Registry；真实不同授权配置不能静默相互覆盖 |

**关键部署事实**：保留当前已发布二进制/头文件/默认 getter 的真实能力；H-02a/V8 的 `DeploymentAdmission` 属未来新架构实施时必须验证的规则，不能声称现行发布物已支持。已批准 H-DUAL=A 的必需协议原子提交，针对的是实际 F-02 部署事务；不能靠 Generic/QPA 的解析成功来宣布双协议已在线。

### 5.2 同一个部署请求的归一化、冲突与生命周期

```text
C++ / QML / Generic / QPA 入口配置
     └─> Frontend parse + syntax/Qt target lifetime validation
           └─> Runtime-normalized DeploymentIntent (untrusted request)
                 └─> Host trusted DeploymentProfile/PolicyProvider
                       └─> F-06 DeploymentAdmission / Scope / Target checks
                             └─> F-02 prepare required endpoints → commit one RuntimeGeneration
                                   └─> F-04 RFB and/or F-05 Native listeners
```

- **归一化输入必须覆盖**：请求来源/入口类型、Target 身份与 Qt 存活令牌、`address` 或网络 interface 选择、端口、必需协议 profile、现有 public security intent、remoteInputEnabled、watermark 当前政策、Host trusted Profile 版本。普通 CLI/QML/C++ 可变参数不能直接变成 `AuthorizedTarget`、ListenerPermit 或 DeviceTLS 身份。
- **互斥与参数检查**：`address` 与 `interface` 最终只可选一种模式。**Generic/QPA 启动字符串中同时给出二者应按既有解析合同拒绝**；而已发布的 C++ `setListenAddress` 会明确清除旧 interface，`setListenInterface` 会切换为 interface 模式，QML 也遵守其已发布属性语义，不能以新设计禁止合法的显式 setter 切换。非法端口/目标、无法验证的接口、无效 watermark（包含请求启用但无法渲染）、未知 hyremote-prefixed QPA 参数或弱档位不能被静默改为另一默认。QPA native delegate 专属 token 必须按现有语法正确剥离，不可因 hyremote 参数污染原生插件参数。
- **多入口同时存在**：同一宿主可能既实例化 `RemoteAccess` 也加载一个插件。Runtime 的共享实现**不代表两个 frontend 可互相继承权限**：每个调用者形成可追踪的 `AccessInstance/RuntimeGeneration`，只有当明确由**同一 Host 签署的同一 Target/Scope/配置/安全 profile** 管理并证明能共享资源时，才可复用受控 Source/Runtime 对象；不匹配时返回现有公开错误类的真实配置/绑定失败，并保持既有有效实例，不擅自关闭另一入口。是否允许多个对外不同端点依实际 Host Profile，不能将不同实例的 Grant 账本合并成一个用户。
- **变更与停机**：现行 C++ 配置在 `Stopped` 才可变更；QML `enabled=false` 或对象析构只撤掉其明确拥有的引用和 Session，不得因另一个合法前端的资源仍有效就越权停止它。Host 整体撤销/Target 对象销毁则由 F-02/F-06 统一失效相关 RuntimeGeneration、frame/input queue；不能以重建 QML 组件复活旧 Grant。
- **状态反馈**：Frontend 只显示 F-02 来源的真实 `Stopped/Starting/Running/Stopping/Faulted/Unavailable` 和现有可表达 `RemoteAccessErrorCode`；`ListenerBound` 不表示 Native DeviceTLS/UserAuth/ViewGrant/ControlGrant。任何新的内部 `POLICY_INVALID/PROFILE_CONFLICT` 类别必须无损地转换为真实已有 public error + 不泄密上下文；不得在此文档自动追加公共 enum。
- **没有受信 Host Provider**：从四入口到达的零代码参数不足以履行 H-02a 的“宿主明确授权”。未获得可验证 Target/Scope/Policy 时**默认不打开对外 Listener**，宿主 Qt 的原生 GUI 和在原生 QPA 上的现场显示/输入应继续按其能否独立运行的真实状态运行，不能拿远控准入失败等同“宿主程序崩溃”。

### 5.3 Qt/QPA 兼容与线程/插件加载的禁止行为

1. **Core 继续 UI/Qt/QPA/具体协议无关**；Qt Private 只能保留 ADR-0008 `AR-04=A` 的已批准 Runtime 内部窗口输入/高 DPI 专用例外，以及 QPA facade 自身 private ABI 入口。Generic/QML/C++ facade 公共类型不 include `QPlatformIntegration` 或本地 Native/TLS 内部类。
2. **精确 Kit**：`Qt 5.15 / Qt 6.8` 及对应 Windows/Linux x86_64/ARM、不同补丁/BSP 应按实际支持单元分别认证；特别 QPA Factory/Runtime private 输入窄单元不能跨 minor、编译器、厂商分支共享一个二进制。一般 Qt public API 可编译不代表 private ABI 可部署。
3. **QML 时机**：`componentComplete()` 前状态只允许保存属性与校验错误，不得引发网络 listener/认证。QML typed 通知跨线程必须 queued，旧 generation 的通知不能覆盖新实例 UI；QML 元素销毁时先使其异步回调失效。
4. **Generic**：`QGenericPlugin` 保留 Qt 本机 platform key；只从 public Qt 插件入口装配同一 automatic access controller。无法从 Qt surface 明确界定安全 Target 时拒绝远控可见范围，不静默广播整个桌面。
5. **QPA**：由 Factory Trampoline 返回原生 `QPlatformIntegration*`。原生 delegate 初始化/失败与远控 Runtime 准入/失败有不同来源；插件不得因为 RemotePort 被占用就自创另一种 Qt Platform、退出宿主业务或默默换网络端点。
6. **无第五协议/入口**：Widgets 和 Quick 是 Runtime Target adapter 家族，不是额外接入入口；内置 Edge/Gateway 更不是设备端的第五集成方式。

### 5.4 F-03 可以直接审签的接口与负向验收

| ID | 源接口/配置场景 | 同一 Runtime 的必须行为 | 负例与未来证据 |
| --- | --- | --- | --- |
| `INT-01` | C++ target Widgets/Quick→start | 相同 Host Profile/Target/Scope 经 F-06 准入后调用 F-02，无 facade 专属帧/权限实现 | target 销毁或缺 Host 准入零 listener；FW-01/02 |
| `INT-02` | QML `enabled` 与 `componentComplete` | Component 完成前不能启动，工作线程通知转 GUI 线程，取消防旧 callback | 组件中途销毁/快速 enabled toggle；FW-01/09 |
| `INT-03` | Generic `address/interface` 与初始只读参数 | 互斥配置、公共 QGenericPlugin API、同一 AutomaticAccessController | 参数冲突/未知命令行不能变宽松；FW-01/02/10 |
| `INT-04` | QPA 专用参数/原生 delegate | 私有兼容 shim 与已资格原生 QPA 各司其职，远控失败与原生 GUI 命运隔离 | delegate 参数污染、插件加载顺序、远控 port 拒绝；FW-01/09/10 |
| `INT-05` | 四入口同 Target/Host Profile | 一套 policy/source/输入行为，四个 Facade 只作启动/通知，不自造 Grant | 相同权限下 RFB/Native frame、撤权、Scope/held 不应因入口不同而漂移；FW-01/04/07 |
| `INT-06` | C++ facade 与零代码 Plugin 同进程 | 多入口调用按 Host 授权实例隔离/安全共享；配置冲突按真实错误拒绝，已有合法服务不被静默覆盖 | 双 bind、QML 热重建、旧代次回调；FW-03/09 |
| `INT-07` | Invalid watermark / TLS 材料 / Native-only | 必需 profile 不满足则 H-DUAL=A 全事务失败，不变成无水印/明文 RFB | 安全配置冲突、端口占用、未知密钥；FW-02/03 |
| `INT-08` | Qt Private/多 OS exact Kit | Core/前端 public ABI 稳定，QPA 和 Runtime 窄 Qt Private 单元精确标注分发资格 | 同源码不同 Kit 二进制混装/Qt 插件部署失败；FW-10/XP-01 |
| `INT-09` | Host 撤权/Target 消失、Facade stop | F-06 优先失效旧输入/像素，F-02 停受影响端点/引用，不伤本地 HMI/其他合法 Scope | 旧 QML signal / QPA callback / C++ owner 离场；FW-06/09 |
| `INT-10` | 合法离线 Direct/Tool SDK 诊断 | 不强制 Gateway/Edge/TLS side channel 做 Runtime 依赖；诊断只输出当前真实配置能力 | 未安装 Gateway、证书缺失、旧 Tool 不能伪称 Native；XP-01/14/19 |

**F-03 设计审查状态**：`INT-01..10=DESIGN_CONTRACT_FOR_REVIEW`，当前 GitHub 源码仅证明旧公共 API/Plugin/Qt 适配事实，尚未实现/验收 H-02a、Native DeviceTLS、Dual 新准入；因此 `IMPLEMENTATION_NOT_AUTHORIZED`、`GATE_D_NOT_FROZEN`。若精确 Kit 证明必须修改已批准 API/ABI、QtPrivate 范围或四入口模型，才提交有旧/新差量的 Human 受控变更，不以功能编写时临时修改决策。

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

### 6.4 F-06 授权快照的原子性与权限执行点（详细设计闭合候选）

**单一权威数据流**：Host 创建可信 `DeploymentProfile`，登记目标/冲突 Scope 后发布 **不可变** `PolicySnapshot(revision, effectiveEpoch, perTargetView/Privacy, perScopeControl, expiresAt)`。F-06 在受控设备运行时保存指向当前快照的原子可见引用；协议端仅提交已经验证的连接事实，不得把 Viewer 的 Target 名字、Gateway RoutePermit 或 Native `grant_epoch` 自造为授权。任何当前快照无法确认、过期或 Target incarnation 不符即默认 `DENY`。

| 顺序 | 责任方 | 线性化点/提交规则 | 拒绝与可观察结果 |
| --- | --- | --- | --- |
| `POL-01` Deploy / Update | Host PolicyProvider→F-06 | 受信来源、Scope 冲突图、期限全部验证后发布**一个**新 Revision/epoch；无效输入不替换为放宽策略 | `DEPLOYMENT_DENIED/POLICY_INVALID` 仅内部诊断，不新设 Native error |
| `POL-02` Session Open | F-04/F-05 认证层→F-02 Registry | server-generated `AccessSessionId` 绑定协议、peer generation、实际认证强度及 RuntimeGeneration | 证书/用户/历史共享 RFB 密码不自动推出个人身份/Grant |
| `POL-03` View/Target | F-06 Host authorizeView | 每次枚举、过滤/入队前以当前 Target incarnation、Session、Scope 与授权快照裁决 | 拒绝时不枚举未授权 Target，不产生其敏感帧 |
| `POL-04` Control | Host ControlArbiter | 对**冲突 Scope** 的授权/释放做串行提交，返回真实 `GrantEpoch`；现场抢占优先 | 旧 request/PENDING/Denied 不获得 Qt 输入权 |
| `POL-05` Input admission | F-06 InputAdmissionFence | 网络入队前先验 `SessionId/RuntimeGeneration/Target/Scope/GrantEpoch/PolicyRevision` 及资源预算 | 失败不占 held、不创建 Qt 事件 |
| `POL-06` Input final delivery | Qt GUI 目标适配器 + F-06 | **最后实际投递 Qt 目标事件前**在同一有序决策域复核最新政策。撤权提交与此投递形成可判断先后顺序 | 撤权已生效的待消费 ticket 拒绝，保护性释放按原持有者 ledger |
| `POL-07` Revoke | Host PolicyProvider→F-06 | 提升有效 epoch/使失权 View/Control/Privacy 原子生效为拒绝；网络通知**在后** | 旧输入与新输出被拒，相关缓存失效；未发通知不影响立即撤权 |
| `POL-08` Close/Expiry/GeometryChange | F-06 Registry + Qt Adapter | Session 关闭、期限到期、Target 丢失时注销对应会话票据/held；**同一 Target 发生尺寸/输入坐标几何代次变化时**，先阻断该 Target 新 DOWN，再以设备端持有账本对受影响远程来源幂等执行保护性释放；此几何处理**不自动撤销仍合法的 View/ControlGrant** | 旧输入坐标票据失效、重复/迟到 UP 不误释放其他来源；之后新的几何代次输入须重新检现行许可 |

**并发实现必须证明的线性化边界**：单纯在网络线程“检查过后投递 `postEvent`”**不满足** `POL-06`，因为撤权可能在 GUI 消费前生效。设计约束是：Host 撤权发布能立即使旧 PolicyEpoch 失效，GUI 最后投递事件与本地当前 Policy 判定具有可测试的相对顺序；如果实际 Qt 事件传递还存在异步阶段，则最终**事件应用点**继续判定，不得在较早的 enqueue 阶段认定操作已经完成。锁/线程串行化实现可以在现有 ABI 允许范围内选择，但不得以持锁执行用户业务、TLS、文件 IO 或不可控回调来假装原子性。

### 6.4.1 多级 Qt 输入队列的最终许可栅栏（Gate-D 审查合同）

**已核源码插入点**：当前 CompositeInputSink 用 Qt::QueuedConnection 排入 GUI，Widgets/Quick 叶子 InputSink 还有第二级 queued drain；叶子最终通过 QtWindowSystemInput 的同步 QWS 投递或 QWidget/QQuickWindow 输入投递。当前公共 InputEvent 不带已验证 Session/Grant 上下文，故只在网络入口或复合 GUI drain 检查不可能满足 H-REVOKE=A。

**内部可信票据**：F-06 在网络准入后创建 AuthorizedInputTicket（仅 Runtime 内部），绑定真实 AccessSessionId、peer/runtime generation、TargetId+Incarnation、ScopeId、PolicyRevision、GrantEpoch、GeometryEpoch、来源 held ledger key、接收时戳及取消标志。复合队列、目标路由、Widgets/Quick 叶子队列必须携带整张票据，不得从普通 InputEvent、Qt 对象名或远端字段重新推断权限；不改现有 public InputEvent ABI 或 Native 消息。

**不可拆分的时序**：① 初次准入仅可宣称 QUEUED，不登记已交付 held；② 最后一级 Qt 对象所属线程在同步 QWS/sendEvent 的**最后实际调用前**，检查当前可信 policy/deny latch/Session/Scope/Grant/Target/geometry，拒绝迟到旧票据；若叶子之后还有异步投递，就必须将最终检查后移至真正应用点；③ Host 撤权先发布 scope denyPending 立刻拒新普通 DOWN/旧帧，随后与 Qt 最后投递域协调旧在途票据及每来源保护性释放，只有能证明旧票据不会再被应用才可报告 REVOKE_EFFECTIVE；同步调用已进入时按其与撤权的实际线性化先后归因，不能假装撤回已进入宿主的事件；④ Qt GUI 线程不得等待网络、磁盘或宿主业务回执。无法证明最终顺序时保持 deny/断链并报 PENDING/NEEDS_RECOVERY，不得报撤销成功。

**资源账本**：SessionHeld 与 TargetAggregateHeld 对已交 Qt 的 DOWN 逐来源登记，普通新输入和保护性 UP 分权；沿用每目标 64 个普通事件及 136 个保护性释放容量。若多合法不冲突 Scope 共用真实 Qt 叶子适配器，授予控制前必须证明跨来源最坏 held/预留释放有界；资源不足拒绝新准入，不通过每会话多开队列绕过总配额。

**确定性负例**：RACE-03、FW-06、AFS-09 分别暂停复合队列、叶子队列、同步 Qt 事件开始点，再并发撤权/现场输入；验证无撤权后新的旧 DOWN、旧 held 不跨 peer 误释放、重复 UP 幂等及真实最终执行来源。此处仅为设计要求，具体 Qt Kit 可行性与互通均待独立审签，NOT_RUN。

### 6.4.2 多来源输入的 Qt 目标等价关系与 136 保护配额证明（Gate-D 审查候选）

**事实基线**：当前 src/runtime/src/detail/input_mailbox_admission.hpp 将 69 个已识别逻辑键位和 3 个指针按钮合计为 **72 个已接受 held 类别**，普通待处理预算 64、保护性释放预算 **72+64=136**；这些是已批准 V8 在实际 Qt 目标适配器上的容量，不是允许**每个远程会话重新复制 136 个物理释放槽**。当前叶子类按逻辑按键标记 accepted，并不含 F-06 要求的会话、Scope 或 Grant 身份；不能直接当成新多来源授权实现。

| 设计对象 | 唯一所有者及原子不变量 | 失败拒绝 |
| --- | --- | --- |
| QtDispatchDomain / TargetInputIdentity | F-06 与 F-02 从**真实 Qt 输入接收者/叶子适配器 + TargetIncarnation**推导唯一物理投递域；不同逻辑 Target/Scope 若指向同一个不可独立仲裁的键鼠/焦点域，必须在 ScopeConflictGraph 中视为**有冲突**，除非能证明每个受控操作确实有隔离的 Qt 叶子与 held domain | 仅靠不同 ScopeId 或不同 QWidget 子对象名字不得同时给同一 QWindow 焦点多个远程控制者；证明不足直接拒绝第二个 Controller |
| SessionHeld / AggregateHeld | GUI 最终投递域存按 AccessSessionId/PeerGeneration/TargetIncarnation/实际 Qt leaf/KeyOrButton 的**已投递**来源账本；AggregateHeld 为每物理键位的已生效来源引用数，只在 0→1 形成真实 Qt DOWN、1→0 形成真实 Qt UP | 对未实际投递的 DOWN 的撤销只取消待处理票据，不补发 Qt UP；来源不符的 UP 永不改变其他 peer/现场 |
| PendingTicket / DispatchPermit | F-06 入队/叶子必须携带来源；对每次同步 QWS 或 sendEvent 调用，最终检查与进入调用的先后经可观测 DispatchPermit/在途计数定义，进入前先记录 in-flight，返回后才结算实际投递/失败 | 若撤权先设置 deny，则后来的 permit 拒绝；permit 已进入时撤权进入 REVOKING 并待 in-flight 收敛，不提前回报 REVOKE_EFFECTIVE |
| ProtectedReleaseSchedule | 每真实 Qt leaf 的普通输入预算 64 和保护释放预算 136 不因多会话/多 Scope 放大。撤权/离焦时按**物理键位**聚合源感知释放意图，最多一个未完成 Qt UP 对应一个实际已按下的物理键位；来源级释放意图可在每连接固定上限的 ledger/bitset 中合并，不把每来源重复 UP 逐个入 Qt queue | 如果合法来源组合、重复 DOWN/UP 或新提案 key 码使保留容量无法证明，拒绝新增 ControlGrant/关闭受影响输入，不允许吞掉最后 UP 或无界入队 |

**配额算术及适用限制**：每个物理叶子已投递不同 held 类别最多 72，现行普通事件待处理上限 64，因此需要保留的实际 Qt 保护性释放上界为 **72+64=136**；上界成立的前提是**同物理键位的多个合法来源不是多个 Qt key-down 实体**，来源计数和取消意图在有界设备内部账本完成，Qt 叶子只有一次物理状态转移。8 个全局连接对单叶子最多构成 **8×72=576 个来源/键位组合的账本记录**（这是存储/验证上界，**不能**当成 576 个 Qt 释放事件）。多会话同物理 leaf 若无法证明 Scope 冲突隔离、来源 refcount 的串行结算和最后物理 UP 则**不签收此优化**，最小合法方案是对该 leaf 同时至多一个远程 Controller。保护性释放与普通输入必须分队列/配额，不能让普通事件耗尽释放资格。

**撤权与重入**：同步 Qt sendEvent/QWS 仍可能进入宿主回调。DispatchPermit 记录该调用从准入到返回的状态，Host BeginRevoke **先拒新票据和敏感帧**，不持同步 Qt 回调锁等待 GUI；in-flight 全部安全结算、受影响来源实际 held 保护释放完成后才可陈述 REVOKE_EFFECTIVE。事件若卡在宿主代码且无法证明已完成，就保持 REVOKING/NEEDS_RECOVERY、隔离远端连接，绝不把网络 ACK/队列为空当作“设备端业务已停”。

**确定性反证**：RACE-03 必测 (a) 两 peer 同 Scope 争控制；(b) 两个称“不冲突”Scope 却共享实际 QWindow/Qt 输入叶子，第二控制请求应拒绝或有确证独立适配；(c) 8 个会话的重复键/按钮 Down 与跨 peer Up，不错误释放其他远程/现场；(d) 69 键+3 按钮持有、64 正常待发、连环撤权/Resize，实际 Qt 保护性事件不越 136 且无卡键；(e) 同步 sendEvent 期间 Host 重入撤权，有确定先后及未完成真实状态。量化结论为**设计证明前置候选**，所有 Qt5/Qt6 Kit/代码实现仍 NOT_RUN。

### 6.5 F-06 每连接按键账本与受保护释放（详细设计闭合候选）

- **两个独立维度**：`SessionHeld[SessionId,Target incarnation,Scope,logicalKeyOrButton]` 持有“该连接实际获准并提交的 DOWN”；`TargetAggregateHeld[Target incarnation,logicalKeyOrButton]` 持有合计引用数/活跃来源，全部更新在同一 Qt 输入决策顺序域。不同协议和两个 peer 都不能只凭远端声称 DOWN/UP 来修改其他来源的 held 状态。
- **合法 DOWN**：`POL-05/POL-06` 均通过才登记该 SessionHeld；从合计 0→1 时向目标发送一次真实 Down。同一来源重复 DOWN 不增加引用数，按协议既有重复规则做幂等处置/拒绝，不生成“另一只按着的键”。
- **合法 UP**：仅释放该来源此前实际被设备登记的 DOWN；当 TargetAggregateHeld 由 1→0 时发最终 Up。未知来源的重复 UP 不影响其他来源或现场输入；不能靠全局 `releaseAll` 误放掉另一个合法 Controller。
- **撤权/断开/Target 销毁**：先令该来源新增 DOWN 的资格失效，再按持有账本逐一释放其真实 held，必要时走 V8 保护性释放保留通道/配额；重复 Close 或迟到网络 UP 不造成第二次释放。现场 Qt 物理输入有独立来源，不被错误合并为远程强制 UP。
- **设备画面尺寸/旋转/输入映射几何代次变化（仍为同一 Target）**：F-02 Target/FrameSource 在 GUI 所有者域提交新几何代次时，F-06 的 `POL-08` 先将该目标的**旧坐标输入票据**失效，阻断新的旧变换 DOWN；随后按其**设备端实际接受的 per-source held ledger** 在 Qt 最终投递域执行受保护 UP，引用计数归零才向 Qt 目标发实际 UP。该清理属于 Target 输入几何转换而非用户撤权：有效 ControlGrant 可以保留，新尺寸的输入必须重新建立坐标变换。客户端可能先行发送合法 UP，但设备账本确保后续几何保护性清理/迟到 UP **幂等且不释放其他合法 peer/现场来源**。即使 RFB/Native 客户端因网络丢包、旧变换代次或失焦没能发送 UP，设备端仍完成清理，避免远端鼠标/按键卡住。此事件不新增 Native 报文，也不把普通 damage 当作几何代次变更。
- **无权正常操作和授权清理不同**：即使旧 Grant 不再允许新的 `KEY_DOWN`，本地**已登记合法 DOWN** 的清理仍须可执行，且这种保护释放不得被误解释为设备重新授权远程控制。RFB 输入共享密码不改变单一 Scope 仲裁。
- **不能宣称“已执行”**：排队成功只是 `QUEUED`；Qt 适配器实际交付前通过终审且成功进入目标才是 `DISPATCHED`，目标业务是否采纳是宿主另一个可观察结果。失去业务 ACK 时只能 `OUTCOME_UNKNOWN`，不能自动重放参数写入/复位/告警确认。

### 6.6 F-06 的跨模块输出与技术设计检查

| 合同 | 消费者 | 可读出且必须唯一的事实 | 失败与后续验证 |
| --- | --- | --- | --- |
| `POL-01/02` | F-02、F-04、F-05 | 设备可信部署/会话真实性/实际认证强度 | FW-02/09、AFS-01..03；TLS-first plain 无个人身份 |
| `POL-03` | F-02 Publisher、V-01 | 当前获准 Target / ViewGrant / 隐私修订 | FW-05/07、AFS-04/08；旧 Target/帧拒绝 |
| `POL-04` | F-04 RFB、F-05 Native、V-03 | 当前 Scope holder/GrantEpoch，现场优先 | FW-04、AFS-05/06；不把 UI 意图当 Grant |
| `POL-05/06` | Qt Input Adapter、V-02 | 真实入队许可/最后投递/拒绝/未知 | FW-06、AFS-09；晚撤权竞态，禁止旧事件执行 |
| `POL-07` | F-02 Frame Publisher、Session queues、V-02 | 当前策略失效时的可控输出/输入清理与状态可见性 | FW-06/08、XP-25/26；已交 bytes 不虚构追回 |
| `POL-08` | F-02 Target/Coordinator、Qt Input Adapter、V-02 | Close/Expiry/GeometryChange 各自终止旧会话或旧输入坐标代次；几何变化对已登记 held 做幂等保护性释放但不凭空撤 ControlGrant | FW-06/09、VIEW-06/07、SURF-04/07；同 Scope 多来源 held 不互相释放 |

**F-06 设计审核通过的充分条件（当前尚未签收）**：独立审查上述对象唯一所有者、两次/最后输入栅栏、撤权与 GUI 线性化、Scope 冲突图、按连接 held ledger、每会话隐私和业务结果分别可验证；对每条 `POL` 能追溯 FW/AFS/XP 负例，且没有要求修改 Native V8 wire 或公开 SDK ABI。实际 Windows/Linux/Qt Kit、Resource/RSS 和 Native E2E 测试属于相应后续资格，不在本轮自报 PASS。

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

**标准 RFB F-04 的固定 Endpoint/Peer 准入合同已明确**：详见 [F-04 `RLG-01..09` 的 Legacy Target/Scope/Input 设计](module-detail-d1-device-protocols-2026-11-05.md)。它使用**设备 Host/F-06 唯一 ControlArbiter**，每 RFB peer 独立 AccessSession、Target/Scope、GrantEpoch 有效性和 held ledger，不修改现行 RFB 3.8 wire；默认只读，宿主明确打开 legacy 输入时可在当前 Scope 控制许可下接入。该边界不影响 Native P-A/A 已批准协议。真实旧第三方 Viewer 兼容/Qt 输入负例仍 `NOT_RUN`。

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
