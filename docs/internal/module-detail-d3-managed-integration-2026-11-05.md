# D3｜HyRemoteView 内置 Edge / 可选 Gateway 跨模块接口收口（候选）

> **2026-11-05 决策状态更新**：[Human #843 决策记录](https://github.com/skawu/HyRemote/issues/843#issuecomment-6646988698) 已批准 H-T01/H-DUAL/H-REVOKE/H-EDGE/H-RFB 均选择 A。V15/V8 等效原文已取得，Native P-A 数值/TLS-first A 已获批准；其余内部接口、性能假设、原子验收与 Gate-D 仍待设计审签，不因上述决定自动冻结。

> **设计主线**：全产品 #843 的 E-01/G-01，与 D1 HMI Device Contract、D2 HyRemoteView 客户端合同对接。现有 [Stage04 module](stage04-managed-edge-module-design-2026-11-04.md)、[Stage04 security](stage04-managed-edge-protocol-security-2026-11-04.md)、[Stage05 IDL](stage05-managed-edge-control-idl-2026-11-04.md)、[Stage05 DAG](stage05-managed-edge-implementation-dag-2026-11-04.md) 仍是细节主源；本稿**只做跨产品消费接口/状态/冲突消歧，不重新创建第二份 Gateway Control wire**。  
> **批准约束**：HYR-PD-001/V15、HYR-ARCH-001/V8；工厂 PC **只安装 HyRemoteView**；Direct 无中心依赖；HMI Framework 是最终权限裁决者；Gateway 可由厂商或客户自托管；#190 `POST_GA_CONDITIONAL / NOT_ACTIVATED`。  
> **状态**：`DESIGN_CANDIDATE / NOT_FROZEN / NOT_ACTIVATED / NO_PRODUCT_CODE`。所有 Token、IDL、UI、Service 名称均为设计内逻辑候选，不冒充已发布 API/端点或安全合格。

## 1. 只存在一个 View 产品、一条真实设备会话和两类路由方式

~~~mermaid
flowchart LR
  OV["异地 HyRemoteView / Session + Native DeviceTrust"] --> GW["可选 Gateway: Identity / Registry / RoutePair / FIFO Relay"]
  GW --> EV["工厂 HyRemoteView: EdgeSiteManager / LocalACL / FIFO"]
  EV --> HMI["HMI Framework: Native DeviceTLS + AccessSession + View/ControlGrant"]
  LOCAL["同一工厂 View 的 Direct UI"] --> HMI
  HMI --- HOST["HMI 本地 Qt 业务 / 现场优先"]
~~~

一台工厂 PC **一个安装/升级/卸载的 HyRemoteView 产品**；它可同时拥有普通 Viewer 工作区与内置边缘路由角色，但不能出现必须另行安装“Edge Connector/Agent/Bridge/VPN”的出门步骤。内部跨进程/Windows 服务是同产品实现选择，不能当第二软件向客户收费、独立部署或自行开网段代理。

异地 View→Gateway→Factory Edge→HMI 的 Route 只是 ByteStream 送达能力；Device Trust、DeviceSession、Target List、ViewGrant、ControlGrant、被看见的画面及输入始终由异地 View 与 **HMI 自身**通过完整 Native 1.0/TLS 处理。工厂 Edge/Gateway 无设备 TLS 私钥，不解密/二次捕获 HMI 像素，不在 DeviceSession 上增加云端身份字节。

**允许的访问路径**：DirectTCP for LAN（RFB3.8/Native），ManagedByteStream for a **specifically enrolled HMI**（当前只可在完整 Native E2E 合格后支持公网 Managed）；公网 plain RFB 经 WSS 不等于 E2E 加密。

## 2. 统一的跨域身份与可信数据字典（逻辑 IDL，非 wire freeze）

| 名称 | 建立/核验方 | 作用域与持久化原则 | 不能替代 |
| --- | --- | --- | --- |
| `OperatorPrincipal` | Gateway 对当前登录者的真实认证链 | Tenant/Site/受限时效；登录登出取消未来 Route 资格 | Device Native 用户身份 |
| `SiteId / TenantId` | Gateway 管理面 + 客户管理员批准 | 隔离两个站点和同名 HMI，默认不跨租户 | HMI DeviceId、IP 子网 |
| `EdgeInstanceId` | 工厂 View 产品安装实例安全密钥/站点登记 | 只允许本实例代表当前 SitePolicyRevision 出站登记 | PC 名称、GUI 登录用户名 |
| `HmiDeviceId` | HMI 本身可信设备材料，经工厂管理员**受信**登记；远端 View 独立复核 | 绑定精准 Endpoint、受信 HMI 身份材料、有效期/修订 | LAN IP、RFB banner、Gateway 显示名 |
| `AllowedEndpointBinding` | 工厂 Edge 本地管理员批准白名单 | 唯一准入 IP/DNS+端口+协议档位+HmiDeviceId；变更需重新验证 | 动态用户填写的任意 host:port |
| `SitePolicyRevision` | Factory Edge 当前受信持久配置 | 单调递增，同 revision 下白名单/共享状态一致；离线仍生效 | Gateway 缓存的旧 policy |
| `RouteId/RouteEpoch` | Gateway 分配、Factory Edge 二次准入 | 只一次活跃配对、时间限制及连接身份绑定；重连新 route | HMI DeviceSessionId |
| `RoutePermit/PairCredential` | Gateway 受信控制面 | 固定 Operator/Tenant/Site/Edge/HMI/protocol/expiry/nonce/受众；原子一次消费 | 设备 ViewGrant/ControlGrant |
| `AccessSessionId/GrantEpoch` | **HMI Framework** 独立 Native 会话 | 每设备 TCP/TLS 连接新 Session；按 HMI PolicyRevision/Scope 发权 | RoutePermit、Edge URL、Gateway 登录 |
| `DisplayFreshness` | Remote View 实际解码+呈现计量 | 按 SessionGeneration 的已呈现合法内容与时间 | DeviceReachable/RoutePaired/SiteOnline |

**四种时钟不可混淆**：Gateway 绝对有效期（需要可信时间/偏差策略）、Route 本地单调 timeout、Factory 本地共享 `SitePolicyRevision`、HMI 自有 `GrantEpoch/PolicyRevision`。Edge 不能把 SitePolicyRevision 投射为 HMI GrantEpoch。

## 3. 同一份控制合同的命名/状态消歧（不立第二控制 IDL）

| 较早设计出现的名称 | Stage05 当前语义中心 | 本轮明确的同义/禁止转换 |
| --- | --- | --- |
| `HmiId`、`DeviceId`、`HmiDeviceId` | `HmiDeviceId` 经可信身份注册 | 展示名/网络地址不能被当作唯一身份 |
| `PolicyRevision`（Edge） | Factory `SitePolicyRevision` | 非 HMI PolicyRevision；不能用云上低版本反向覆盖 |
| `route.intent`、`RouteRequest` | 站点路由请求**语义**，具体 wire 仅 Stage05 IDL 候选 | 新稿不生成额外对外 message type/版本；v0/路径示例不是生产端口 |
| `route.offer`、`RouteOffer` | Gateway 提议、Edge 本地二次批准 | offer 不含可拨号 IP/port、不能转为任意 TCP 代理 |
| `RouteTicket`、`RoutePermit`、`PairCredential` | route 准入证明与配对一次性凭据**职责不同** | 不把票据当设备授权或可多次使用的永久令牌 |
| `route.close`、`RouteClose` | 幂等、终态、单调代际闭路 | 两端网络重连不复活旧 route 或 HMI input |
| `SiteOnline` vs `DeviceReachable` | 网络端/Edge 观测 | 在线不能推出真实 DeviceTrust / ViewGranted / RemoteFrameFresh |
| Windows `managed-edge-service` 进程候选 | 同产品安装体系的无人值守角色 | 不代表必须拆独立安装包或已具备注销后在线资格 |

**优先级**：V15/V8 批准约束 → Stage05 唯一候选语义 IDL → Stage04 模块草案/Stage03 原型参数 → 历史 Issue。若两份 Stage 文档使用不同 `major`、route 名称、30 秒 TTL/8 route 等假设，**未经过技术与 Human 签收之前都不变成正式公共 wire 或客户 SLA**。

## 4. 内置 Edge 实际职责与对外接口（D2 消费）

| 逻辑接口 | 可信调用者 | 必须强制的行为 | 对客户端可见事实 |
| --- | --- | --- | --- |
| `enrollSite` | 有权限的工厂客户管理员（本机或受保护管理 IPC） | 显式信任 Gateway/站点身份、登记 EdgeInstance，默认拒绝未核验根/证书 | Enrolled / CredentialMissing / TrustRejected |
| `registerHmi` | 客户管理员显式选择设备并可信核验 HMI DeviceId | 精确绑定允许的 LAN 端点与证书/可信身份；不自动扫描共享网段 | RegisteredUnshared / IdentityMismatch |
| `setHmiShare` | 管理员权限核验后的明确开关与期限 | 共享默认 OFF；**扩权先受信持久化并验证新 SitePolicyRevision 再开放准入**；**撤权先使本地新 Route 准入失效**，继而持久拒绝修订并关闭受影响 Route；写盘失败保持拒绝/NEEDS_RECOVERY | SharingEnabled / SharingRevoked / PolicyError |
| `startEdgeLink` | 经授权的 Edge instance 生命周期管理 | Outbound 到受信 Gateway；证书验证失败拒绝；不影响本地 Direct | AuthenticatedSiteLink / Unavailable / Disabled |
| `admitRouteOffer` | 当前受信 Gateway + 绑定 Offer | Revalidate Operator/Site/Edge/HMI/Protocol/TTL/revision/当地 ACL；只选本机白名单 Endpoint | RouteAccepted / Denied(reason) |
| `pairByteStream` | 同一 RoutePermit 一次配对 | 配对**仅**受信远端和当前工厂 Edge 双边；按序 FIFO 转发，不解析 Native payload | Paired / Partial / PairConflict / Closed |
| `revokeShare` | 工厂管理员/策略 | 本地拒新 Route、立即关闭受影响现有 Route；Gateway 更新可在之后完成 | HmiUnshared + affected RouteClosed |
| `reportEdgeStatus` | 只读本机 GUI/有权诊断者 | 最小当前事实、来源/时间/可用性，凭据和内网清单脱敏 | SiteLinkState、RouteCount、HMI share state |

**IPC 安全**：若 Windows Service / GUI 分进程，客户端 UI 发来的任何 `addHmiEndpoint`、`setShare` 必须经 OS 进程身份及产品管理员权限再次确认，服务只接受固定命令集合；禁止通过 IPC 传入任意公网来源端 host、shell/exec、插件路径。GUI 的“已点击打开”不等于后台已经安全提交并持久化批准。

### 已批准的 Native TLS/plain 受管消费边界

[Human 批准的 P-A 和 SOURCE_TENSION_01=A](module-detail-w00-native-v8-delta01-approved-2026-11-05.md) 表明两种 Native 档位都由真实 HMI DeviceTLS 先建立设备身份；`plain` 不等于不加密，也不代表有独立用户身份。Managed 必须能验证 HMI 身份与当前用户/Target/ViewGrant/ControlGrant，缺失时拒绝输出或操作。Stage05 的 Edge/Gateway 只能按既有白名单和 RoutePermit 透明 FIFO 转发完整 TLS 字节，不能因为新增值域补充就解析或改写 `auth_mode`、能力位与 ControlState。Direct 继续独立于云端，Gateway #190 仍未激活。

### 4.2 E-01 / G-01 的唯一数据所有者与受信事务（详细设计候选）

本节消费 [Stage05 唯一控制语义 IDL](stage05-managed-edge-control-idl-2026-11-04.md)，**不产生第二份 JSON/CBOR wire 或固定公开版本号**。Stage05 的 `major=0/minor=1`、历史 TTL/队列/Route 数值都是候选，不能在本节转成 SDK 常量或 SLA。

| 内部对象 | 权威及唯一写入所有者 | 容许的派生状态与生存期 | 绝对禁止 |
| --- | --- | --- | --- |
| `SiteRegistry/SitePolicySnapshot` | 工厂 HyRemoteView 内置 Edge；受本机授权管理员变更 | 具有持久、单调 `SitePolicyRevision`，HMI 白名单/分享开关/期限/站点信任作为一个可校验快照提交 | Gateway 远端旧快照覆盖本地撤销，或从 UI 的未授权按钮生成共享许可 |
| `EndpointBinding` | 工厂管理员受信核验 HMI 身份后，由 Edge 本地只读版本化登记 | `HmiDeviceId + exact host/port + protocolProfile + trust material ref + revision`；端点变化需要重新确认 | Operator/RouteOffer 传入任意 host:port/DNS 替换拨号目标或网段扫描 |
| `EdgeInstanceIdentity` | 本机受保护 Edge 身份材料/OS 访问控制 | 与 Tenant/Site、当前登录态及 Gateway 信任分别验证，私钥不在 QML Profile 中 | 拿个人 Viewer 凭据当 Edge 密钥，或在日志/IPC 回传原始密钥 |
| `GatewayRegistry` | Gateway 受信租户/站点认证面 | `OperatorPrincipal, Tenant, Site, EdgeInstance, SharedHmiMetadata` 的最小审计状态 | 以 Gateway 显示名/IP 取代 HMI DeviceId/设备 TLS 证明 |
| `RouteRecord` | Gateway 为每次申请产生唯一 `RouteId + RouteEpoch`，Edge 保存本次关联快照 | 绑定认证 Operator/Tenant/Site/Edge/HMI/协议/Intent/TTL 与当前可用状态；重连必须新代际 | 复用已 CLOSED 的 Epoch 或让路由许可直接等于 ViewGrant |
| `PairLedger` | Gateway 受信一次性消费事务；Edge 另核验己方端点与方向 | 只允许一条 route 的两端各一次带 audience 的有效配对，失败清消耗态/关闭本路 | 重放 token、第三条连接、交换两端角色、迟到 ACK 恢复旧配对 |
| `OpaqueBytePipe` | Edge/Gateway 每 Route、每方向独立的有界连接队列 | 在 `FORWARDING` 期间只保序传输 TLS bytes；关闭丢未发送数据、销毁队列，指标无明文内容 | 根据 Native FrameId/Grant 作代理层过滤、拼帧、解密或丢加密片段 |
| `EdgeLifecycle` | 同一 HyRemoteView 产品运行时，首批限登录态启用 | 登录/明确启用/站点许可有效才在线，退出/注销立即关闭 Route 与 Link；远端状态带来源与观察时间 | 在用户注销后冒称 Service 在线，或在关闭单个画面窗口时擅自关闭仍明确获准共享 |

**配置提交事务**：本地管理员 `setHmiShare(false)` / 更新白名单时，先验证管理员与预期 `SitePolicyRevision`，再构造不放宽权限的新不可变快照；**本地当前准入门的原子指针先切换为拒绝**，确保新的 `RouteOffer` 与重验证无法通过，随后原子持久化撤销证据、关闭此 HMI 的已配对/未配对 Route，并异步通知 Gateway。若存储失败，不能恢复先前开放许可；保持拒绝/NEEDS_RECOVERY，直至可信管理员重新核验。对会增加权限的开启操作，必须**先成功安全持久化并验证**，才使准入门对外允许新 Route。旧 Gateway ACK 不能回滚较新本地 revision。

**拨号防 TOCTOU**：`RouteOffer` 接收检查当前 revision、Operator/Site/HMI/协议/TTL/受信 Gateway；`PairReady` 之前及每次 `dialApprovedEndpoint` 前再次读取当前受信 `SitePolicySnapshot` 与端点身份 binding。即使 Offer 曾被批准，一旦本地撤权/证书变化/重启代际变化，必须拒绝拨号或关闭既有 Route。DNS 解析输出亦须与当前受信端点/地址约束相符；不得在连接阶段接受外来 host/port 替换。

### 4.2.1 撤销写盘失败及掉电恢复的设备共享准入（Gate-D 审查合同）

本地 setHmiShare(false) 先关闭有效准入再保存拒绝的顺序已经正确，但**当前进程的 deny/NEEDS_RECOVERY 不自动成为掉电后可证明的持久事实**。若 fsync 失败后立即终止，磁盘上可能只有旧共享白名单。禁止把旧文件本身视为已恢复的在线许可。

1. 撤销发起时 E-01 立即 deny 该 HMI 的新 Route、取消旧 Pair/Route，随后以受信版本、原子替换和文件/目录持久同步提交新 SitePolicyRevision。持久同步未证实成功不显示“持久撤销完成”；当前进程始终维持 DENY/NEEDS_RECOVERY，不接受迟到 Gateway ACK 反向恢复。
2. **每次新进程/OS 登录/崩溃恢复都从 Edge OFF 开始**，不从磁盘恢复 EdgeRunLease、PairCredential、旧 RouteEpoch。正常、可证明的安全退出后，已持久批准的逐设备共享白名单仅作配置输入；登录用户仍须按 H-EDGE=A **重新显式启用当前运行期 EdgeRunLease**，经当前 SitePolicy/EndpointBinding/管理员权限校验才能对外准入，**正常重新登录并不强制逐设备重新审批已持久批准的白名单**。若上一运行期异常终止、政策版本不明、签名/持久性无法验证，则保持 OFF/NEEDS_RECOVERY，要求受信管理员重新核对受影响设备并确认才可开启。
3. **异常重启可识别性**：在允许任何 EdgeRunLease 对外服务之前必须先将本次运行的独立启动/策略代次标为持久 DIRTY，并验证文件及目录同步成功；若做不到则不可对外共享。正常 STOP 完成后才将对应代次持久标为 CLEAN。下次启动看到 DIRTY/缺标记/代际不一致/损坏时一律 OFF/NEEDS_RECOVERY，需受信管理员复核受影响设备；不能将旧持久允许清单直接激活。若撤销写盘失败后掉电，可能无法从失败写入还原完整变更请求，**不得宣称已持久撤销**；DIRTY 只提供保守异常恢复识别，不捏造业务操作回执。该启动标记/恢复事务是内部设计候选，不新增 Native/Gateway wire。
4. 该拒绝只影响对应工厂共享/Route，不能误停 HMI 合法 Direct 或其他无冲突 HMI。扩权始终必须先完成可信持久提交再对外准入。

**反证入口**：RACE-05/XP-24 必须在原子写入、文件/目录 fsync、撤销时旧 Offer 抢入及强杀/重登之间注入失败；重启绝无旧 RunLease/Pair 或无新批准的共享。保存 PolicyRevision、RunEpoch、写入结果、admin 新批准证据；仍是 DESIGN_ONLY/NOT_RUN。

### 4.3 E-01 / G-01 的路由、一次性配对与关闭状态机

```mermaid
sequenceDiagram
  participant O as Remote View
  participant GW as Gateway Registry/PairLedger
  participant E as Factory View Edge
  participant H as Whitelisted HMI
  O->>GW: Request exact Site/HMI/role
  GW->>GW: Authorize Operator + bound RouteEpoch
  GW->>E: Offer bound route (no host:port)
  E->>E: Read current SitePolicy/EndpointBinding
  E-->>GW: Approve or fail closed
  GW->>O: Single-use paired route side credential
  GW->>E: Single-use paired route side credential
  GW->>GW: Atomic two-audience PairLedger decision
  E->>E: Revalidate local current policy before dial
  E->>H: Connect exact approved endpoint
  O->>H: HMI DeviceTLS + Native auth/Grant over opaque FIFO
  Note over O,H: Only HMI authorizes Target, view, control
  E->>E: Local share revoke advances current revision
  E->>E: Reject offers and close related routes
  E-->>GW: Later RouteClosed notification
  GW-->>O: Stream closed; no automatic input replay
```

- `REQUESTED→SITE_APPROVED→EDGE_APPROVED→PAIRING→FORWARDING→CLOSING→CLOSED` 严格使用 Stage05 的**内部语义状态**；`CLOSED` 是终态，迟到 `PairReady/RouteClosed/Offer` 与旧 RouteEpoch 无法重开。任何超时/非法版本/身份撤销/一端断线清理本次 Pair 状态；只重建新的 RouteEpoch 与新的 HMI DeviceTLS 会话。
- **单次凭据与双向身份**：Gateway 的 `PairCredential` 同时约束 `RouteId/Epoch`、actor/audience、方向、短期到期、仅一次消费，Edge/Remote View 对自身侧核验。配对成功只证明**两条获准数据通道接到了同一 Route**，不能证明设备 TLS 或具体 HMI Grant。
- **并发撤权**：本地 share revoke、Gateway Operator 许可撤销、Route 超时、工厂用户注销，分别由其所属权威先提交**拒绝门**，后发送异步通知/关闭 socket；并发进入终止时幂等，只能单调走向 `CLOSING/CLOSED`。Gateway 与 Edge 都不得在控制通道断裂但数据通道仍存在且无法确认现行许可时无界持续转发；保活上限如无可证明配置应立即 fail closed。
- **业务命令不可重放**：ByteStream.write 成功仅表示**本地有界队列接受字节**，不表示 HMI 收到、更不表示参数写入/复位等业务已执行。网络断开时不自动重放最后一包 TLS 或 Native INPUT；应用命令在不可确认设备端结果时标 `OUTCOME_UNKNOWN`，由现场可信状态核实。

### 4.4 OpaqueBytePipe 队列、线程和资源合同

| 队列或执行域 | 唯一 Owner 与有界策略 | 满载/取消/隐私行为 |
| --- | --- | --- |
| Gateway control decode/PairLedger | Gateway 自有认证事件循环；读取前限制控制包/嵌套/JSON key/UTF-8，有限 TTL ledger | 拒大包/未知关键字段/重复字段，不开启任意 route |
| Edge site/policy 管理 | 工厂 Edge 控制队列/受保护存储事务；与 Qt Quick render/input 线程隔离 | 不能在 UI 点击后就假报共享已持久化，取消与权限拒绝优先 |
| Route data `O→H` | 一 route、一方向独立 `QrouteBytes` / watermarks；所有写回压、WS/QTcpSocket/TLS/OS buffering 分项计入 | 不修改加密字节，无法保序交付就暂停读/拒新写并在有界超时后整 Route 关闭 |
| Route data `H→O` | 与逆方向独立有界 FIFO；不复用上一 RouteEpoch 缓冲 | 不重排、丢帧、重复密文字节；撤销后不续送旧 TLS 片段 |
| Diagnostics/audit | 两端各维护最小 metadata （RouteId/Epoch、来源、状态/水位及脱敏原因） | 严禁保存 DeviceTLS 明文、HMI 帧/输入/用户密码/原始 PairCredential |

`M_EdgeRSS = base(Qt/OS)+SiteRegistry+Link+sum(Route direction queues/TLS buffers)+audit` 和 `M_GatewayRSS = base+TenantRegistry+PairLedger+sum(Route direction queues/TLS buffers)+audit` 分别预算；未通过实际目标 Kit/企业代理环境测量前，**不填写** Stage03 的 `8 routes/256KiB/30s` 等探索数值为正式 SLA。Qt6.8 WebSocket inbound frame/message/read buffer、写侧积压和外层 TCP/TLS/OS 缓存都必须有单独有界策略/真实水位；若 API 层无法阻止 OS 已接受字节，也不能声称物理追回。Route 满载时仅**背压/整路关闭**，绝不边中继边裁减 Native TLS 内容。

### 4.5 E-01/G-01 技术合同与现有负例证据映射

| 合同 ID | 可实施的单一责任及失败行为 | 既有验收入口 | 当前 |
| --- | --- | --- | --- |
| `EG-01` | 仅工厂管理员能受信登记站点/HMI 精确 Endpoint/证书 | MG-01/02/03/05 | `DESIGN_ONLY` |
| `EG-02` | 本地 SitePolicyRevision 原子持久化、撤权拒绝门先于通知 | MG-08、CTRL-06 | `DESIGN_ONLY` |
| `EG-03` | RouteOffer 与 HMI Dial 两次受信白名单/PolicyRevision 校验 | MG-03/08、CTRL-04/06 | `DESIGN_ONLY` |
| `EG-04` | RoutePermit 绑定身份/租户/设备/期限/当前权限，不生成 HMI Grant | MG-01/02/07、CTRL-02/05 | `DESIGN_ONLY` |
| `EG-05` | PairCredential audience/方向一次性原子消费，第三路/旧 epoch 拒绝 | MG-04、CTRL-07/08 | `DESIGN_ONLY` |
| `EG-06` | 同 RouteEpoch 状态单调，撤销/断网/到期不会复活 CLOSED | MG-04/08/11、CTRL-08 | `DESIGN_ONLY` |
| `EG-07` | Edge/Gateway 逐方向有界、保序 opaque TLS Relay，背压或整路关 | MG-09/16、XP-27 | `DESIGN_ONLY` |
| `EG-08` | HMI DeviceTLS/最终 Target、ViewGrant、ControlGrant 完全留在 HMI | MG-06/07、XP-10、AFS-11 | `DESIGN_ONLY` |
| `EG-09` | H-EDGE=A 登录期仅显式启用在线，退出/注销关闭；关闭单个画面不擅自下线 | MG-10、XP-12/24 | `DESIGN_ONLY` |
| `EG-10` | 站点/Route/HMI 授权失败分来源 UI，不能伪称 DeviceAccess 或业务成功 | MG-06/07/11/13/14 | `DESIGN_ONLY` |
| `EG-11` | 工厂密钥/管理 IPC/升级回滚/撤销存储拒绝不可信权限提升 | MG-05/12/15、XP-18/24 | `DESIGN_ONLY` |
| `EG-12` | Gateway #190 未激活时只保留设计与试验资格，不公开声明可托管 WAN 服务 | MG-13/14/16、XP-08/10 | `DESIGN_ONLY` |

**本节不是 Gateway 实施激活**：`EG-01..12=DESIGN_CONTRACT_FOR_REVIEW`，`STAGE05_CONTROL_IDL=EXISTING_CANDIDATE_ONLY`，`NATIVE_P_A_A=HUMAN_APPROVED`，`GATEWAY_190=NOT_ACTIVATED`，`GATE_D=NO`，`GATE_I=NO`。无新的公开控制消息、版本号/路由限额、产品代码或“公网已安全发布”声明。

## 5. D2 ManagedRemoteSession 到 Stage05 路由的调用合同

~~~mermaid
sequenceDiagram
  participant O as Remote HyRemoteView
  participant GW as Gateway
  participant E as Factory View Edge
  participant H as HMI Native/TLS
  O->>GW: Gateway login + Site/Device discovery under ACL
  GW-->>O: Only entitled HmiDeviceId metadata
  O->>GW: RouteRequest(Site,HMI,Protocol,Intent)
  GW->>GW: Permit bound to current Operator/Site/Edge/HMI/TTL
  GW->>E: RouteOffer without arbitrary host/port
  E->>E: Verify locally current ShareRevision + EndpointBinding
  E-->>GW: Admit / deny
  GW-->>O: One-time route pairing
  GW-->>E: Paired route identity
  E->>H: Dial the exact locally approved HMI endpoint only
  O->>H: Over opaque FIFO, independently verify HMI DeviceTLS
  H-->>O: Device identity and Native protocol handshake
  O->>H: Native auth, Target, ViewGrant then optional ControlGrant
  H-->>O: Only HMI-authorized output
  Note over O,H: GW/Edge never decrypt HMI data nor issue View/ControlGrant
  E->>E: Local admin revokes HMI share
  E->>E: Advance local ShareRevision, refuse all new Route
  E->>GW: Revoke share / RouteClose (may arrive later)
  GW-->>O: Route closed
  O->>O: Discard old DeviceSession / frame / input and reverify on new route
~~~

**Direct path**不会执行前六个 Gateway/Edge 消息，也不读取企业 login 缓存作为直接端 HMI 权限；直接 TCP 连 HMI Native/RFB 是另一种 `ConnectionPath`。

### 5.1 F-02 输出与 F-06 授权的受管路径消费约束

[设备 F-02 RT-01..08 / F-06 POL-01..08](module-detail-d1-framework-boundaries-2026-11-05.md) 的生效点在 **HMI 本地 Runtime/Qt**，不是 Edge/Gateway。受管路径职责仅为批准站点/设备/端点的有限期 Route admission、端到端 TLS 字节原序 FIFO、中断/背压的真实报告及撤销路由后停止新接纳。

- `POL-07` 已在 HMI 生效时，即使此前 RoutePermit 尚未撤销，HMI 也不得新排旧许可敏感画面；Edge/Gateway 不能以 RouteReady/PairGranted 替代真实 ViewGrant。Route 被撤时关闭对应字节流，其他合法 Direct/HMI 本地工作不因此失败。
- `RT-05/07` 的 Full/Delta 取舍发生在 HMI Native/RFB 明文协议边界的**编码/加密前**；Edge/Gateway 不丢弃、合并或重排 TLS 加密字节，受限队列达到上限时只能背压或断开受影响 Route。
- Factory View 的登录态 Edge（H-EDGE=A）持有自己的连接/站点/路由生命周期；Qt 客户端窗口关闭与产品进程退出/用户注销必须分别给出真实在线状态。Route 关闭后旧 SessionId/GrantEpoch/已排 GUI 输入均不能复用。

## 6. 状态、断线、撤权和业务结果（五类“在线”不能互推）

| 事件 | 本地 Edge / Gateway 做什么 | Remote View 怎么显示 | HMI 真正效力 |
| --- | --- | --- | --- |
| Gateway 登录成功、Edge 尚离线 | 只能列受权目录，不能配对新路线 | “站点未连通”，不是 HMI 无许可 | HMI 不受影响；本地 Direct 可继续 |
| RoutePaired 但 HMI TLS 身份不对 | Edge 不解释 Device TLS，只转字节 | “设备身份不匹配”，禁输入/清旧图 | HMI/真实设备 Grant 尚未产生 |
| Gateway 授权但 HMI ViewGrant 被拒 | Route 可存在但无受限像素输出 | “设备拒绝查看”，不展示一帧旧 HMI 图 | 由 HMI Policy 判定 |
| Viewer 有 ViewGrant、无 ControlGrant | 画面仍可在合规时继续 | 只读，不发旧控制意图 | HMI 最终禁止按键/触控 |
| 现场管理员关闭共享 HMI1 | Edge **先**推进本地撤销，再关 HMI1 的 Route；异步通知 Gateway | HMI1 受管会话终止；HMI2/Direct 不受影响 | HMI 同机真正许可不靠 Gateway 撤销来决定 |
| Gateway 数据包乱序/重放 PairCredential | 仅有一次配对账本/新 RouteEpoch 生效 | “路由过期/被拒”，不恢复旧 Session | HMI 旧 TLS socket 不成为新 Session |
| 工厂 PC 明确退出 HyRemoteView 或用户注销 | **H-EDGE=A 首批**：关闭 EdgeLink/所有 Route，拒新 Route 并真实标离线；仅关闭单个 RemoteSurface 窗口不自动关闭仍由用户启用的工厂共享 | 工厂侧标真实“Edge 已离线”，不宣称无人值守 Service 在线 | HMI 本地业务持续，其他合法 Direct 不受影响 |
| 远端写业务命令后 WAN 断线 | 关闭 route，不缓冲命令到恢复后重发 | “操作结果未知，须现场核实” | `OUTCOME_UNKNOWN`，不可执行自动 replay |
| 静止画面很久没有新像素 | 心跳/会话有效性与实际用户帧龄分别报告 | 不误将无变化的合法帧识别成断线 | HMI 视频按策略合法，网络无像素不代表离线 |
| 透明 relay 队列积压 | 只暂停读取/有界背压/超时关 Route，不丢中间密文字节 | “通道拥塞/断开”，不能继续标实时 | HMI DeviceSession socket 关闭后释放 held 输入 |

## 7. 运行/进程/更新与信任隔离设计候选

**H-EDGE=A 已获 Human 批准的首批支持合同**：工厂用户登录并明确启用时，HyRemoteView 内置 Edge 才可在线；用户明确退出/注销必须真实离线、关闭 Route、取消旧状态，绝不显示无人值守在线。Stage05 的 Windows Service 仍是后续独立可证明资格，不能提前纳入首批出门口径。

**扩展级别**：同一 HyRemoteView 安装产品可承载独立授权的 Windows Service 或同二进制 `--managed-edge-service` 角色；GUI 与 service 的 UID/Session、凭据可访问性、SCM 服务安装/卸载、可用性、自动更新/回退必须有 OS ACL/真实 Windows evidence。技术更优的“双进程同产品”或“单二进制双运行模式”尚不能靠文字判胜；也不可让客户额外安装一款独立 Agent。

**证书/秘密**：工厂 EdgeInstance key 必须受本机机器级权限保护且与普通个人 Viewer 凭据库隔离。Gateway Trust rotation、HMI Trust replacement、View Operator OIDC credential rotation 是三套独立管理流程。卸载必须安全撤销后台服务与敏感 key；升级时旧 SitePolicyRevision/已撤销 Device 不得因迁移回滚而重启共享。

**自托管 Gateway**：只安装其经明确批准的服务，维护带认证的 tenant/site registry、一次性 route pairing 账本、FIFO relay 与最小 metadata/audit；拒绝无租户绑定的横向 Device enumeration；升级、备份、证书轮换、崩溃恢复、健康探针和清理要明确 runbook。自托管不是“客户自己负责安全，所以可不验证 TLS”。

### 7.1 E-01 工厂登录态 Edge 的可实施生命周期合同（H-EDGE=A 已批准）

本节**完善已批准的工厂 Edge 首批级别**，不重选进程模式、不新增安装产品、也不提前启用无人值守。Factory PC 只有一套 HyRemoteView 安装；单个 RemoteSurface 窗口、工厂站点管理工作区、整个 Viewer 应用进程、OS 登录会话是**四个不同生命周期边界**。

| 对象/存储 | 所有者、写入与保护 | 生命周期与不可混淆状态 |
| --- | --- | --- |
| `SitePolicySnapshot`（持久） | 工厂本机受信 Edge 策略仓，只有经管理员 ACL 校验的命令写入；原子替换，单调 `SitePolicyRevision` | 允许持久保留注册设备、共享白名单、期限和显式共享意图；**持久意图不是当前在线许可**，不可因恢复文件/登录就自动接受 Route |
| `EdgeRunLease`（非持久） | 当前已登录 OS 用户上下文中的 HyRemoteView 工厂运行角色；与产品进程/登录会话绑定 | 包含独立 `EdgeRunEpoch`、当前用户会话、受信启用意图和取消源；每次应用启动/登录重新取得，不恢复上一进程租约 |
| `EdgeEffectiveAdmission`（运行态） | E-01 本机当前许可裁决器，消费最新 SitePolicy + EdgeRunLease + Gateway trust | 只有 `LoginActive ∧ ViewProcessAlive ∧ ExplicitlyEnabled ∧ TrustedSite ∧ HmiShareAllowed ∧ NotExpired` 时，才有可能对**该 HMI/协议**许可新 Route；缺一项即关闭 |
| `SiteLink`（网络态） | E-01 出站受信 Gateway 控制连接 | `LINK_DOWN/CONNECTING/VERIFIED/DEGRADED/CLOSED` 只证明站点控制通道状态，不证明指定 HMI DeviceTLS/Grant |
| `RouteRecord + PairLedger`（每 Route） | Gateway 产生 RouteId/epoch；E-01 当前运行态验证并持有自己的 RouteContext | 仅在当前 `EdgeRunEpoch+SitePolicyRevision+EndpointBinding` 可用，进入 CLOSED 后永不因旧 Offer/ACK 复活 |
| `HmiDeviceSession`（HMI 设备端） | HMI Framework F-05/F-06；Remote View 自己完成 TLS 和设备 Grant | 工厂 Edge 不持有/签发设备 ViewGrant/ControlGrant；关闭 Edge Route 不等于撤销其他合法 Direct session |

#### A. 明确区分关闭窗口、退出进程、注销和故障

| 输入事件 | 必须发生的状态变化 | 不允许发生的行为 |
| --- | --- | --- |
| 用户只关闭某个 RemoteSurface/远程画面窗口 | 仅对应 ViewingSession 关闭，释放其本地输入焦点/帧；如果工厂 View **仍在运行且 EdgeRunLease/Share 仍有效**，已共享 HMI 的其他 Route 保持其原许可 | 不能把“关闭画面”误解释为关闭整个 Factory Edge 站点或注销用户 |
| 用户关闭工厂站点管理 UI 页面，但主 View 产品继续运行 | Site manager 的观察者取消订阅；Edge 的独立产品运行角色按原有效运行租约继续服务 | QML 页面对象析构不能删除共享白名单或改变 Grant/Route，除非用户明确按撤销按钮 |
| 用户明确退出整个 HyRemoteView 产品，或 OS 用户注销 | **同步先关闭本地新 Route Admission**，推进 `EdgeRunEpoch` 并取消 RunLease；随后关闭 Link/所有配对 Route、销毁内存态 PairCredential/Queue；离线通知可延后但本地离线必须真实 | 不得在 GUI 不在且没有获批 Service 资格时保持后台在线，不得仅靠 Gateway 上报 OFFLINE 而实际仍转发 |
| 系统睡眠/网络断开/可信证书过期 | 先使不能验证的 SiteLink/Route 不再新准入，旧 Route 按真实数据通道安全性终止或隔离，观察状态带来源/时戳 | 不得凭上次 `SiteOnline=true` 显示为当前 online，也不重放旧 PairCredential |
| Viewer/Edge 进程崩溃、OS 会话被强制终止 | 进程死亡释放本机 socket/Route/密钥内存；远端旧路由必须因对端断流/租约超时关闭，恢复应用须以 **new EdgeRunEpoch** 重新验证策略和用户启用意图 | 不能重用旧 RouteEpoch、单次 PairCredential 或把历史共享“已开启”记录当当前联网事实 |
| 本地管理员明确 `setHmiShare(false)` / 白名单撤销 | 先以新 PolicyRevision 在本机当前准入门拒绝受影响 HMI，再持久化拒绝并关闭全部相关现有 Route；失败保持 `DENY/NEEDS_RECOVERY` | Gateway ACK 延迟/失败不能使旧共享继续开放；不受影响的其他 HMI 和合法 Direct 不应被误关闭 |
| 管理员增加新共享目标/延长期限 | 先验证本机权限、HMI DeviceId、精确端点与材料，成功持久化**新**策略版本之后才对外开放；有效运行租约仍须单独存在 | 不能依靠 GUI 尚未确认的按钮、旧 Revision 或 Gateway RouteOffer 来激活新 HMI |

**操作系统支持边界**：此处规定的是跨 Windows/Linux 可证伪的行为语义；最初资格仅覆盖真正测试通过的 OS/Qt Kit/登录生命周期。Windows 锁屏是否保持登录会话与用户注销不同，不应默认为同一事件；断电/强制杀进程无法要求应用先发送完整关闭通知，但必须保证**没有授权的后台进程继续转发**，远端必须通过租约/断流收敛。将来无人值守 Service 是**同一产品的另一个独立资格**，不能借本节直接实现/宣称首批支持。

#### B. 启停与更新的事务边界

1. `START`：工厂用户已登录 → HyRemoteView 工厂角色存在 → 经受信管理员启用当前 EdgeRunLease → 读取/核验最新 SitePolicySnapshot + Gateway Trust → 计算有界可共享 HMI 白名单 → 建立出站 SiteLink。**只有真实验证后**报告当前 online；单个 HMI 路由/设备安全状态独立。
2. `OFFER`：Gateway 带绑定的 Tenant/Site/Operator/HmiDeviceId/Protocol/TTL 的 RouteOffer → E-01 原子读取当前 `SitePolicyRevision+EdgeRunEpoch` → 只向对应**白名单 exact endpoint**拨号。准备到 Pairing 或 dial 前再读取同一当前态；revision/epoch 不一致一律拒绝本次 Offer 并关闭新建 socket。
3. `STOP`：撤销 EdgeRunLease 的原子本地停止门 → 标记全部 Route `CLOSING`、销毁受限队列和一次性 PairCredential、关闭 SiteLink → 以单调 `EdgeRunEpoch` 使迟到回调无效。通知 Gateway 只是最佳努力，不作为本机离线成立的前提。
4. `UPDATE/ROLLBACK`：升级必须先执行 STOP/fence，备份持久的白名单/策略及受信材料，校验签名后迁移；回滚若读到较新的撤销版本或不兼容数据，进入 `NEEDS_RECOVERY/DENY`，不得以旧安装包/旧持久状态重新分享。恢复后也必须申请**新运行租约**，禁止复活旧 Route。
5. `CREDENTIAL_ROTATION`：Gateway trust、EdgeInstance secret、HMI DeviceTLS trust 三套流程分别验证，任一失败只关闭受影响入口；原始私钥/密码/token 不跨 QML/日志/无授权 IPC。

#### C. 线程、资源、错误与测试的实施合同

| 设计合同 | 所有者/执行点 | 必须可验证的错误出口与状态 | 既有验收入口 |
| --- | --- | --- | --- |
| `EDGE-LC-01` 单一 RunLease + RunEpoch | E-01 工厂 Edge lifecycle owner，非 QML page | 进程/登录终止时本地 STOP 门先于新 Route；旧回调拒绝 | EG-01/EG-10、XP-12 |
| `EDGE-LC-02` 持久共享意图与当前在线分离 | E-01 SitePolicyStore + V-03 GUI readonly projection | 恢复白名单但未启用登录期 lease：`OFFLINE/NOT_SERVING` | XP-08/XP-12 |
| `EDGE-LC-03` 本地撤销事务 | E-01 PolicyAdmission owner | 存储失败保持 DENY/NEEDS_RECOVERY，不因迟到云 ACK 复活 | EG-03/EG-08、XP-09 |
| `EDGE-LC-04` Offer/Pair/dial TOCTOU | E-01 Route owner + G-01 PairLedger | Epoch/revision/endpoint/trust 改变拒绝或关路由，不得拨任意内网地址 | EG-05/EG-09、XP-10 |
| `EDGE-LC-05` 透明 FIFO/背压和退出释放 | E-01 OpaqueBytePipe owner | 每 Route/方向 queue 受限；只关闭/背压，不能删改 Native TLS 字节 | EG-06/EG-11、XP-17/27 |
| `EDGE-LC-06` 回滚和密钥旋转 | X-01 Installer/Upgrader + E-01 credential owner | 撤权版本单调；回滚不恢复旧共享或 token；凭据不输出日志/UI | XP-24/XP-12 |
| `EDGE-LC-07` 可信状态上报 | E-01 runtime telemetry → V-03 / Gateway | Gateway 旧在线观测仅显示 stale；不推出 HMI DeviceTLS、ViewGrant、实时帧 | XP-11/XP-20 |
| `EDGE-LC-08` Direct 隔离 | V-01 DirectPath + F-05/F-06 HMI | Edge STOP/网断仅结束受管路由，不破坏本机合法 Direct/现场 Qt HMI | XP-11/XP-19 |

**验收证据要求**：测试时分别记录真实 OS 登录/注销、View 进程退出/强退、窗口关闭、RouteId/Epoch、PolicyRevision、受信 HMI 白名单、队列水位、Gateway stale 判定及关联代码 SHA/Qt Kit；所有测试现在是 **DESIGN_ONLY/NOT_RUN**。这不是产品启动、24×7 服务或性能验证报告。

**E-01/G-01 安装回滚消费边界**：D4 的 [X-01 `DEP-01..08` 交付事务合同](module-detail-d4-tool-delivery-quality-2026-11-05.md) 是同一 View 安装产品里 Factory Edge 角色与独立可选 Gateway 版本变更的唯一交付设计。E-01 的登录期 RunLease/RouteEpoch 和 PairCredential 不做持久状态迁移；仅受控 SitePolicySnapshot 可以按不复活撤权的单调 revision 迁移。Gateway 自有 Tenant/ACL/信任备份不能替 HMI DeviceTLS/Grant，升级故障只能安全降为 SiteLink/Route unavailable；本机合法 Direct 继续独立。

## 8. 资源公式与性能/安全负向用例

### 8.1 累计资源，禁止把候选数当合同

`Edge_peak_RSS = Qt/OS_Base + SiteRegistry + EdgeTLS/WSS + Nroutes × (2 × route_direction_buffer + socket_TLS_buffer + bookkeeping) + diagnostics + allocator_overhead`

`Gateway_peak_RSS = Base + TenantSiteRegistry + active_auth_link + active_pair_ledger + Nroutes × (2 × route_direction_buffer + socket_TLS_buffer) + audit_bounded_queue`

`HMI_peak_RSS` 由 D1 独立核算；V8 的 **HMI 128MiB 目标不能移植成 Edge 或 Gateway 的公开 RSS 资格**。

Stage03 32 HMI/8 route/256KiB per direction/30s ticket/64KiB control 等都是探索性起始值，不是正式协议上限或发布 SLA。最终限额必须经过慢端/断网、企业代理、Windows GUI/Service 与实际工厂 PC 的稳定/峰值基准验证；可以压缩范围/拒绝新路线，不能无限排队或删密文任意字节。

### 8.2 受管负向测试矩阵

| ID | 输入/故障 | 正确结局（未来资格） |
| --- | --- | --- |
| MG-01 | Site 未登记、HMI 未显式共享 | 无 Gateway 可达 Device Route；不枚举真实内网端点 |
| MG-02 | 相同显示名、不同 DeviceId/tenant | 不串站、不中继到错误工厂 HMI |
| MG-03 | Client 伪造 Host/IP/port 或 DNS rebinding | Edge 仅拨本地准入 EndpointBinding，拒绝 |
| MG-04 | RoutePermit 被重放/过期/第三端配对 | 原子单次拒绝，无旧 Route 复活 |
| MG-05 | Gateway/Edge 被恶意替换 TLS 证书 | 当前受信链/pin 不通过，拒绝站点或设备访问 |
| MG-06 | Device E2E TLS 证书变化/伪装 HMI | Remote View 在 Auth 前 fail closed，不发送秘密 |
| MG-07 | Gateway 在线、HMI ViewGrant 被拒 | 零敏感像素、禁输入，错误归 HMI |
| MG-08 | 本地关闭 HMI1 共享，WAN 离线 | 本地先禁止新 route + 结束 HMI1 路由，HMI2 Direct 正常 |
| MG-09 | 慢 Viewer / 完全静止 HMI | 有界 FIFO/可信保活，不能随机丢 TLS 字节或错误超时 |
| MG-10 | 设备 GUI 退出、工厂用户注销/OS 重启 | 服务能力按真实资格呈现，不显示假在线 |
| MG-11 | 断网后重连、旧输入 ACK 未知 | 重新 Route/DeviceTLS/HMI 授权，不回放业务命令 |
| MG-12 | 自托管 Gateway 升级/回滚/密钥轮换 | 不复活已撤销共享，不复用旧 token，不丢审计追踪 |
| MG-13 | 版本不兼容/未知关键字段/超长 control | 数据分配前拒绝，记录脱敏分类并保护其他路由 |
| MG-14 | 企业 HTTP Proxy 阻止 WSS/CONNECT | 诚实报告不支持/不可达，不因为穿透失败转成公网明文 |
| MG-15 | Edge 控制 IPC 冒充管理员、以 GUI 写 service ACL | Service 重新核验 OS 权限/产品策略，拒绝改共享或信任根 |
| MG-16 | HMI 受管路由断开后 GUI Direct 仍在线 | Direct 不被 Gateway policy 清除，现场 HMI Qt App 不受影响 |

上述全部为 `DESIGN_ONLY / NOT_RUN`；具体 HMI Device TLS/Viewer G3、Edge/Gateway 产品与安全资格均未取得。

## 9. D3 收口事项：技术方案与 Human 选择分离

| ID | 议题 | 技术建议（候选） | Human 触发 |
| --- | --- | --- | --- |
| D3-01 | Gateway Control serialization / version / IDL | Stage05 唯一逻辑 schema，验证 JSON vs CBOR 与跨语言 parser/未知字段/迁移；不另造控制 wire | 公网接口版本正式冻结与重大产品变更 |
| D3-02 | Factory GUI + Edge Service 支持级别 | **H-EDGE=A 已批准**：首批只支持登录期间的 Edge，用户明确退出/注销即离线；后续同产品内部 Windows Service/OS ACL/凭据研究保留独立验收 | 首批产品级别已定；若未来要求首批无人值守才另报受控变更 |
| D3-03 | Route TTL/数量/缓存/重连 | 以上限可证/队列有界/用户感受测量后选择，保持独立 HMI 预算 | 公开 SLA/成本/版本承诺 |
| D3-04 | 安全 bootstrap：HMI TrustBundle 与 Gateway/Edge 根分离 | 工厂可信登记，远端独立 HMI DeviceTLS 校验；证书换用时人工可信复核 | 修改 V8 信任或引入云端身份委托 |
| D3-05 | Gateway 自托管单节点工程形态 | 优先最小 Registry+Broker+Relay 而非微服务；数据最少/迁移可回滚 | 客户运维/合规或产品定价范围变更 |
| D3-06 | 失联时本地 ACL 撤销与云端 eventual consistency | Edge 权威本地先禁、再通知 Gateway；旧 Pair/Route 不恢复 | 无需改变既定 HMI 权利时技术细化 |
| D3-07 | D2 Client 与 D3 Edge 在一个 View 产品中的隔离 | UI 只发有权限管理意图；EdgeService 二次校验；Direct 引擎永不依赖 Edge state | 修改“只安装 View”或 Direct 离线须 Human |

**设计候选结论**：H-EDGE=A 已关闭首批后台范围争议。E-01/G-01 与 F-01..F-06、V-01..V-03 没有需要加第三 HMI 监听或另装独立 Edge 软件才能成立的结构理由；Stage05 控制/状态名称可以做语义消歧，不必现在创造第二份版本协议。真正产品化仍依赖 Native Device E2E、真实 View G3、站点凭据与自托管运维、首次 Human 设计冻结及 #190 独立激活。

**下一步 D4**：工具与客户端职责分界、安装/更新/Qt Kit、全产品性能/安全/验收证据矩阵；然后 D5 编制统一依赖 DAG 与 Human 决策卡，不把未通过的 PoC 当既成条件。
