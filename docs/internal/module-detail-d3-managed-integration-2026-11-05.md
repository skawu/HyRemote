# D3｜HyRemoteView 内置 Edge / 可选 Gateway 跨模块接口收口（候选）

> **2026-11-05 决策状态更新**：[Human #843 决策记录](https://github.com/skawu/HyRemote/issues/843#issuecomment-6646988698) 已批准 H-T01/H-DUAL/H-REVOKE/H-EDGE/H-RFB 均选择 A。其他接口、性能和 Native A/B 原文仍是待审候选，不因本次选择而自动冻结。

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
| `setHmiShare` | 管理员权限核验后的明确开关与期限 | 本地先持久化并推进 SitePolicyRevision；共享默认 OFF；撤权先断 Route | SharingEnabled / SharingRevoked / PolicyError |
| `startEdgeLink` | 经授权的 Edge instance 生命周期管理 | Outbound 到受信 Gateway；证书验证失败拒绝；不影响本地 Direct | AuthenticatedSiteLink / Unavailable / Disabled |
| `admitRouteOffer` | 当前受信 Gateway + 绑定 Offer | Revalidate Operator/Site/Edge/HMI/Protocol/TTL/revision/当地 ACL；只选本机白名单 Endpoint | RouteAccepted / Denied(reason) |
| `pairByteStream` | 同一 RoutePermit 一次配对 | 配对**仅**受信远端和当前工厂 Edge 双边；按序 FIFO 转发，不解析 Native payload | Paired / Partial / PairConflict / Closed |
| `revokeShare` | 工厂管理员/策略 | 本地拒新 Route、立即关闭受影响现有 Route；Gateway 更新可在之后完成 | HmiUnshared + affected RouteClosed |
| `reportEdgeStatus` | 只读本机 GUI/有权诊断者 | 最小当前事实、来源/时间/可用性，凭据和内网清单脱敏 | SiteLinkState、RouteCount、HMI share state |

**IPC 安全**：若 Windows Service / GUI 分进程，客户端 UI 发来的任何 `addHmiEndpoint`、`setShare` 必须经 OS 进程身份及产品管理员权限再次确认，服务只接受固定命令集合；禁止通过 IPC 传入任意公网来源端 host、shell/exec、插件路径。GUI 的“已点击打开”不等于后台已经安全提交并持久化批准。

### 已批准的 Native TLS/plain 受管消费边界

[Human 批准的 P-A 和 SOURCE_TENSION_01=A](module-detail-w00-native-v8-delta01-approved-2026-11-05.md) 表明两种 Native 档位都由真实 HMI DeviceTLS 先建立设备身份；`plain` 不等于不加密，也不代表有独立用户身份。Managed 必须能验证 HMI 身份与当前用户/Target/ViewGrant/ControlGrant，缺失时拒绝输出或操作。Stage05 的 Edge/Gateway 只能按既有白名单和 RoutePermit 透明 FIFO 转发完整 TLS 字节，不能因为新增值域补充就解析或改写 `auth_mode`、能力位与 ControlState。Direct 继续独立于云端，Gateway #190 仍未激活。

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
| 工厂 PC GUI 退出/Windows 注销 | 按真实当前合格后台运行模式结束或保持 Edge；不假在线 | 工厂侧标真实“边缘离线/服务在线” | HMI 本地业务持续，其他合法 Direct 不受影响 |
| 远端写业务命令后 WAN 断线 | 关闭 route，不缓冲命令到恢复后重发 | “操作结果未知，须现场核实” | `OUTCOME_UNKNOWN`，不可执行自动 replay |
| 静止画面很久没有新像素 | 心跳/会话有效性与实际用户帧龄分别报告 | 不误将无变化的合法帧识别成断线 | HMI 视频按策略合法，网络无像素不代表离线 |
| 透明 relay 队列积压 | 只暂停读取/有界背压/超时关 Route，不丢中间密文字节 | “通道拥塞/断开”，不能继续标实时 | HMI DeviceSession socket 关闭后释放 held 输入 |

## 7. 运行/进程/更新与信任隔离设计候选

**H-EDGE=A 已获 Human 批准的首批支持合同**：工厂用户登录并明确启用时，HyRemoteView 内置 Edge 才可在线；用户明确退出/注销必须真实离线、关闭 Route、取消旧状态，绝不显示无人值守在线。Stage05 的 Windows Service 仍是后续独立可证明资格，不能提前纳入首批出门口径。

**扩展级别**：同一 HyRemoteView 安装产品可承载独立授权的 Windows Service 或同二进制 `--managed-edge-service` 角色；GUI 与 service 的 UID/Session、凭据可访问性、SCM 服务安装/卸载、可用性、自动更新/回退必须有 OS ACL/真实 Windows evidence。技术更优的“双进程同产品”或“单二进制双运行模式”尚不能靠文字判胜；也不可让客户额外安装一款独立 Agent。

**证书/秘密**：工厂 EdgeInstance key 必须受本机机器级权限保护且与普通个人 Viewer 凭据库隔离。Gateway Trust rotation、HMI Trust replacement、View Operator OIDC credential rotation 是三套独立管理流程。卸载必须安全撤销后台服务与敏感 key；升级时旧 SitePolicyRevision/已撤销 Device 不得因迁移回滚而重启共享。

**自托管 Gateway**：只安装其经明确批准的服务，维护带认证的 tenant/site registry、一次性 route pairing 账本、FIFO relay 与最小 metadata/audit；拒绝无租户绑定的横向 Device enumeration；升级、备份、证书轮换、崩溃恢复、健康探针和清理要明确 runbook。自托管不是“客户自己负责安全，所以可不验证 TLS”。

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
