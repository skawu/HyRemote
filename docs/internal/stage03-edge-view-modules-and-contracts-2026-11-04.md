# HyRemote 阶段 03｜统一客户端与工厂 Edge 详细模块设计（D1）

**基线**：Human 批准的产品设计 HYR-PD-001/V15、产品架构 HYR-ARCH-001/V8；追踪 #831。**性质**：实现前模块/接口设计候选，不声称现有代码已经具备此能力。禁止覆盖原正式文档或绕开 G7 受控变更。

## 1. 产品目标与部署不变量

同一个 HyRemoteView 安装体系同时提供：
- **DirectViewer**：在工厂 LAN 直接连接集成 Framework 的 HMI，不依赖云；
- **ManagedViewer**：异地用户经可信 Gateway 和指定工厂 Edge 请求某一 HMI 的会话；
- **FactoryEdge**：客户管理员从本地 HyRemoteView 显式启用，主动联系 Gateway，只转发已登记、获准 HMI 的特定会话字节流。

客户在工厂 PC **只安装 HyRemoteView**，无独立 Edge Connector、Support Bridge、VPN、通用网关程序。云端 Gateway 是独立可选服务，**不属于工厂 PC 软件部署要求**。HMI 只集成 Framework，不新增每设备公网依赖。本地 Direct 离线权益与现场 HMI 业务优先不可因 Managed 模式失效而停止。

## 2. 产品模块边界与依赖

| 模块（逻辑职责，非规定单独进程） | 唯一责任 | 必要输入/输出 | 不得执行 |
| --- | --- | --- | --- |
| `ViewerWorkspace` | 统一设备列表、Direct/Managed 会话 UI、目标选择、显示/输入 | `ConnectionIntent`、`DeviceTrust`、`ViewGrant/ControlGrant` 观察事件 | 伪造设备授权、因登录云端而自动发送键鼠 |
| `SiteAdmin` | 本地管理员明确启用 Edge、维护站点与 HMI 共享白名单 | `SitePolicyRevision`、`RegisteredHmi` 增删/撤销 | 自动扫描并共享 LAN、按任意 hostname/port 放通 |
| `EdgePresence` | 工厂端主动出站注册、心跳、令牌刷新、故障退避 | 固定 Gateway trust、站点/Edge 身份、`PresenceState` | 监听公网入口、将在线等同于设备可控 |
| `RouteAdmission` | 再次验证站点身份、路线许可、受管会话与 HMI 白名单 | `RouteOffer`、`RouteDecision`、`RouteBinding` | 接受请求端自选任意 LAN 目标 |
| `RoutePump` | 限额的两个方向**透明字节转发**与资源释放 | `RouteBinding`、端点流、queue budget | 解码/编码画面、任意裁剪 Native 密文/RFB 增量字节 |
| `GatewayClient` | 异地站点/设备目录、云端用户登录与短时路线申请 | `UserSession`、`SiteAccess`、`RouteTicket` | 将 Gateway 登录冒充 HMI 认证 |
| `DeviceProtocolSession` | 与目标 HMI 进行端到端 Native/RFB 握手及最终身份/权限校验 | 设备证书/身份、协议流、真实 HMI Grant | 因途经路由而降低设备协议安全模式 |
| `LocalSupervisor` | 同一 HyRemoteView 安装体系的运行期、配置迁移、日志和升级 | 开机/注销/退出/升级事件、Edge 状态 | 依赖用户另装一款 Agent/服务 |
| `EdgeDiagnostics` | 记录站点/网络/权限/设备各自状态，输出可核实运行证据 | 非敏感状态与聚合指标 | 记录屏幕明文、输入、密码及完整票据 |

模块名供研发分工使用，不冻结为公开 C++、QML SDK 或动态库名称。UI 只面向用户展示必要状态，管理/运维信息在明确进入站点管理视图后显示。

## 3. 重要数据对象（草案接口）

```text
SiteId, EdgeInstanceId, HmiDeviceId, OperatorId:
    不可从 IP/设备显示名/Qt 指针反推的受信标识；不同信任源不可混用。
RegisteredHmi:
    {site_id, hmi_id, device_trust_anchor, resolved_endpoint_set,
     expected_protocol_security, share_policy_revision, enabled, expires_at}
    resolved_endpoint_set 仅为设备身份校验后的地址材料，不是客户端自由输入的连接目标。
RouteIntent:
    {request_id, operator_id, site_id, hmi_id, protocol_profile, requested_scope}
RouteTicket:
    {route_id, operator_binding, site_id, edge_id, hmi_id, protocol_profile,
     ticket_nonce, audience, issued_at, expires_at, policy_revision}
RouteBinding:
    {route_id, authenticated_gateway_peer, registered_hmi_id,
     verified_endpoint, byte_budget, time_budget, state}
DeviceSession:
    {device_identity, access_session_id, target_id, view_grant, control_grant,
     grant_epoch, policy_generation}
```

**RouteTicket 不是设备端授权令牌**，只能开立受限的网络路线；`DeviceSession` 是由真实 HMI 端根据现有协议产生的另外一层结果。设备 ControlGrant 的撤销与路由票据注销分别作用于各自层次。

标识、时间与状态须抗重放：票据唯一 nonce、短期 TTL、绑定 Edge/站点/目标/请求方/协议、安全校验源，任一不匹配直接拒绝；绝不能仅依赖显示名、已缓存 IP 或云端报“在线”。

## 4. 数据流与状态机

### 4.1 工厂 Edge 生命周期

```text
DISABLED
  --local admin enables / valid policy--> ENROLLMENT_REQUIRED
  --trusted enrollment accepted--> CONNECTING
  --outbound authenticated Gateway--> ONLINE
  --network/auth temporarily failed--> DEGRADED/RECONNECTING
  --re-auth + revalidate registry--> ONLINE
  --admin disable/revoke--> DRAINING --> DISABLED
  --unrecoverable identity/policy failure--> BLOCKED (fail closed)
```

状态属性不能混为一项：“Edge 已连 Gateway”“指定 HMI 内网可达”“异地路线有效”“真实 HMI ViewGrant/ControlGrant 已授予”分别独立。没有实时证据时状态应为 `UNKNOWN` 或 `UNREACHABLE`，不得继续显示“可控制”。

### 4.2 受管路线生命周期

```text
REQUESTED → GATEWAY_AUTHORIZED → EDGE_POLICY_CHECKED
        → DEVICE_ENDPOINT_VERIFIED → ROUTE_PAIRED
        → E2E_DEVICE_HANDSHAKE → DEVICE_VIEW_AUTHORIZED
        → (OPTIONAL CONTROL_AUTHORIZED) → ACTIVE
        → REVOKING / PEER_LOST / EXPIRED / OVER_BUDGET
        → CLOSED
```

网关配对完成不能跳过 HMI 身份校验。收到撤权、站点共享删除、设备信任改变、票据过期或内网 IP 与可信设备不匹配：拒绝新路线，并以有界资源清理当前路线。不能把关闭网络连接本身作为已证实 HMI 完成业务操作的凭据。

### 4.3 端到端时序（成功分支）

1. 工厂管理员通过 HyRemoteView 在 LAN 验证 HMI 身份，加入默认关闭的共享清单，批准 Edge 连接指定 Gateway；
2. 工厂 Edge 使用站点/实例凭据主动建立加密控制连接，报告有限且可见的共享元数据；
3. 异地 HyRemoteView 登录 Gateway，选择**获站点许可**的 HMI，Gateway 校验请求方范围；
4. Gateway 发出只含受信标识和临时绑定的路线请求；FactoryEdge **重新核验**当前共享清单、策略版本和本地设备身份；
5. 边缘端仅对对应 HMI 的已验证本地端点建立连接，Gateway 配对两个经认证的受管端点；
6. 远端 Viewer 通过透明字节路线与 HMI 完成独立设备协议及证书/身份校验，取得设备实际 ViewGrant，才显示目标；独立申请 ControlGrant 后才发送输入；
7. 任意层撤权或故障均使所属权限/路线失败关闭；Direct 路线继续独立存在。

## 5. 关键接口约束

拟采用以下**内部语义接口**，最终 IDL/语言在方案收口时定稿：

- `SiteAdmin.enrollHmi(expectedDeviceIdentity, endpoints, sharePolicy)`：必须实际验证 HMI，不接受仅 IP 地址或 DNS 解析作为可信身份；
- `SiteAdmin.revokeHmi(siteId, hmiId, policyRevision)`：原子增加策略代次，禁止旧路线再次通过；已存在路由发出终止信号；
- `EdgePresence.connect(trustedGateway, enrolledIdentity)`：默认无设备共享、失效凭据失败关闭；
- `RouteAdmission.admit(ticket, currentSitePolicy, verifiedDevice)`：检查 audience/nonce/TTL/版本/跨站点/协议安全允许列表/已绑定端点；
- `RoutePump.start(authorizedBinding, streamA, streamB)`：双方独立固定缓冲、读取超时、背压、单路线终止，**不能丢弃部分已加密数据**；
- `DeviceProtocolSession.open(binding, expectedHmiIdentity)`：必须验证目标端到端设备身份，向上只发布真实 HMI 授权事件；
- `LocalSupervisor.stopOrUpgrade()`：先撤销新路线准入，再有界 drain 已建立连接，刷新在线状态；升级失败不得显示旧在线。

边缘进程崩溃或主动重启后不能复用旧 `RouteTicket`、旧 ControlGrant、旧字节队列，必须重新认证并请求新路线。任意未认证的远程控制平面消息不得执行本地操作系统命令、任意文件操作或启动新程序。

## 6. 线程、隔离与资源

**既有四 Qt 接入运行时不受边缘模块反向依赖。** Edge/Gateway 不运行在 HMI 的 Qt 渲染线程内，也不能阻塞 HMI 本地渲染或输入。边缘站点网络 IO 采用独立异步执行域；不得将长期 IO 锁持有到 GUI 事件/Qt 设备回调。GUI 对 Edge 只发受控意图与状态订阅。

设备端 Shared Runtime 的既有 128 MiB **受管理内存目标不能直接挪用来当 Edge 预算**。Edge 单独预算由“每路上/下行队列、连接元数据、站点对象、流量限额、最大并发”组成；下阶段用明确负载和实测确定硬数字。在硬预算未签收前不要宣称可支持固定的工厂设备规模。

慢 Viewer 时仅设备端协议适配层有权在**编码及端到端加密前**根据完整/增量基准合帧或丢弃过时画面；RoutePump 对字节流只能反压、拒绝额外写入、超时或结束路线。

## 7. 内置后台运行方案比较

| 方案 | 单安装包 | 注销后可用性 | 安全隔离 | 阶段判断 |
| --- | --- | --- | --- | --- |
| 单 GUI 进程常驻 | 是 | 通常不保证 | GUI 与网络同进程，故障关联更强 | 适用于**有人值守**的初期资格；不可承诺注销后在线 |
| 同一 HyRemoteView 产品安装的受管后台组件 + GUI | 是，内部组件由一个安装/卸载体系管理 | 可按 OS 服务能力设计 | 可独立权限和故障域 | **无人值守资格的优先设计候选**；不等于另装独立软件 |
| 外部独立 Edge Connector/Bridge/Agent | 否 | 取决于额外部署 | 可能隔离但增加客户安装维护 | **禁止**；违背 V15 |

具体 Windows Service/Linux systemd 的支持范围、进程权限和首次安装管理员准入须先完成平台 UX/安全验证；未取得资格的环境按有人值守运行模式准确声明。**不因预设第二进程就自动允许第二套软件安装包**。

## 8. 本文与其余详细设计的边界

- Gateway/Edge 控制面消息、加密方案与安全威胁 → D2。
- 12 个 EDGE 用户验收、异常注入、运行资源与分期实施 → D3。
- Native 1.0 wire 不在 D1 中修改；受管路线有自己的版本化控制消息，不算第三条 HMI 远控协议。
- 本文不代表 RFB 已获得安全公网资格，不代表 Native TLS 已实现，也不代表 HyRemoteView 产品已交付。
