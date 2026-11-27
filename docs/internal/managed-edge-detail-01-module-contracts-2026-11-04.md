# V15/V8 模块详细设计 01｜产品组件、内部模块与接口契约（设计候选）

> 工程设计工作 #830；基线：经 Human 批准的 HYR-PD-001/V15、HYR-ARCH-001/V8，源 DOCX SHA-256 分别为 `d0408f333d4b9aec186067cd159ff8a4a2206ef66d58243fd4c5cd10ffb82cf1` 与 `1101d5546cfc0ab63e4cfde809ba4b0530e1048ed3760b56b8c9e7be3de6803a`。此文档是**详细设计候选**，未通过 G4/实施准入。与 #827 / PR #828 的差量参考不同，本文件以**正式版 V15/V8**为唯一产品/架构依据。

## 1. 产品结果与严格不变约束

**场景：** 工厂 HMI1/HMI2 已集成 HyRemote Framework；工厂 PC 唯一安装的本地软件是 **HyRemoteView**；客户管理员仅共享已验证身份的 HMI；工厂 HyRemoteView 主动连可选 Gateway；异地 PC 仍使用同产品 HyRemoteView；站点准入后仍须设备端最终授权。

必须同时实现：
- Direct 在 Gateway/企业服务/互联网不存在时合法可用；同一 View 能同时运行本地 Viewer 工作区与工厂 Edge 角色，但两者授权、生命周期、资源相互独立。
- 工厂 PC 不安装独立 Edge Connector/Support Bridge/Agent/VPN/泛用代理。内部类或库允许称 `EdgeConnector`，**不构成新部署软件**；若未来采用同产品内置服务/后台进程，仍受同一安装、停用、更新和卸载责任管理。
- HMI 保持 One Core + One Shared Runtime、四 Qt 接入、RFB/Native 两独立协议、HMI 端 ViewGrant/ControlGrant/隐私及现场优先；不得给每 Viewer 再做一次截图和编码。
- Gateway 和工厂 Edge 只认识站点/设备/路由，**不解释 HMI framebuffer、键鼠、设备业务对象**；不记录明文画面或输入。不具有网段扫描或任意 TCP 代理能力。
- 受管互联网路径要求端到端设备身份与内容保护。**当前已发布 HyRemote 的 RFB VNC Auth 未加密，Native TLS 尚未交付**；因此 Managed 模式不能以当前 RFB 伪装已具有公网安全资格。

## 2. 系统拓扑（逻辑，不代表已有可运行组件）

```text
 异地 PC                           云端/客户 DMZ                   工厂 PC (仅安装 HyRemoteView)          工厂生产网
 ┌─────────────────┐       ┌───────────────────────┐        ┌─────────────────────────┐          ┌───────────────────┐
 │ HyRemoteView    │       │ HyRemote Gateway      │        │ HyRemoteView            │          │ HMI 1 / HMI 2     │
 │ Managed Client  │--TLS->│ Identity + SiteBroker │<--TLS--│ EdgeGatewayLink         │          │ Framework        │
 │ 设备 SecureSession│<->opaque RelayFabric<----------opaque│ EdgeSessionRouter       │--TCP---->│ Native/TLS目标端  │
 │ Direct Client   │       │ 对设备数据不可解码      │        │ EdgeSiteManager + ACL   │          │ 设备端 Grant/隐私│
 └─────────────────┘       └───────────────────────┘        │ Direct Local Viewer     │          └───────────────────┘
                                                           └─────────────────────────┘
                                                              └──── 工厂 LAN Direct ────────────────┘
```

安全身份域：`OperatorId`（个人）、`TenantId`（组织）、`SiteId`（站点）、`EdgeInstanceId`（工厂 View 安装实例）、`DeviceId`（HMI 长期身份）、`RouteId`（短时转发）、`DeviceSessionId`（HMI 设备端会话）严格区分；IP、设备显示名、站点名不是可信身份。

## 3. 模块职责及依赖（不强制进程/动态库边界）

| 模块（逻辑名称） | 负责的状态/行为 | 明确禁止 | 依赖方向 |
| --- | --- | --- | --- |
| `LocalViewerWorkspace`（View） | Direct 设备列表、视觉工作区、输入焦点、失效提示 | 云端未登录时禁用 Direct | UI -> View Domain |
| `RemoteSessionCore`（View） | RFB/Native client、像素/输入及 E2E 设备身份、每连接取消 | 根据 Gateway 登录自动授予设备输入 | View Domain -> Net/Render |
| `ManagedRemoteSession`（View） | 受管路由与真正设备安全 Session 联合状态 | 把 RouteId 当 DeviceSessionId | View Domain -> Gateway Client + RemoteSessionCore |
| `RemoteGatewayClient`（View） | OIDC/受管凭据、站点目录、路由申请、会话显示 | 无需最终 HMI 授权就显示在线可控制 | View Infra -> Gateway Control |
| `EdgeSiteManager`（同一 View） | 站点启停、客户管理员操作、设备白名单与有效期、单设备撤权 | 默认共享、自动扫描整个 LAN | Edge Domain -> Store/Policy |
| `EdgeGatewayLink`（同一 View） | 实例出站可信连接、心跳、重连、证书/密钥轮换 | 忽略证书错误或降低认证方式 | Edge Infra -> Gateway Control |
| `EdgeRouteAdmission`（同一 View） | 核对 Gateway route request 与本地清单、目标端点、身份指纹/期限 | 根据任意公网用户所填 IP/port 拨号 | Edge Domain -> SiteManager/Policy |
| `EdgeSessionRouter`（同一 View） | 单 Route 的按序透明字节转发、有界队列、背压、撤权中断 | 解码、重抓/重编、任意裁剪加密 Native/RFB 字节 | Edge Infra -> BoundedStream |
| `EdgeDiagnostics`（同一 View） | 本地站点/设备/Route 四维状态与结构化元数据 | 记录 token、画面/输入、旧在线状态当实时 | Edge Domain -> redacted Observability |
| `GatewayIdentity`（可选服务） | 操作者/Edge 实例认证与租户隔离 | 代替 HMI Grant | Gateway Control |
| `GatewayRegistry`（可选服务） | Site/Edge presence、HMI 映射元数据、可见性与版本 | 拥有 HMI 内网探测权限 | Gateway Control |
| `GatewaySessionBroker`（可选服务） | Route 请求/一用许可、端点配对、路由租约/撤销 | 任意两连接配对或直接授予 ControlGrant | Gateway Control -> Relay |
| `GatewayRelay`（可选服务） | 两条经认证路由通道的透明流转发、限速、背压 | 解析设备协议或持久化画面 | Gateway Data |
| `HmiSecureEndpoint`（Framework 端） | 设备协议、可信身份、Native 端到端安全、设备 Session/Grant/隐私、断线输入释放 | 以站点登录取代设备最终授权 | HMI Runtime/Core 边界 |

其中 `RemoteSessionCore`、`HmiSecureEndpoint` 是**目标职责**，不是当前源码已实现 Native/TLS 的声明。详细实现必须走各自仓库已有阶段 Gate：Viewer 的 G3 未通过，不得从本设计直接建立正式 product source。

## 4. 领域数据与可替换内部接口

下述是**行为契约伪接口**，不是编译可用的公共 SDK/C++/Qt API，也不冻结 wire 编号和具体类名。

```text
SiteId        := opaque stable identifier
EdgeInstanceId:= public-key-bound identity of installed HyRemoteView edge
DeviceId      := cryptographically verifiable HMI identity (not LAN address)
RouteId       := opaque one-use, short-lived route instance id
RoutePermit   := {tenant,site,edge,device,operator,protocol,roleIntent,nonce,
                  audience,issuedAt,expiresAt,routeId,credentialBinding}
RegisteredHmi := {site,device,displayLabel,approvedEndpointHint,trustMaterialRef,
                  allowedProtocolSet,sharingStatus,sharingExpiry,configRevision}
RouteRequest  := {requestedDeviceId, protocolKind, roleIntent, identityContext,
                  requestId,clientCapabilitySet}
Admission     := {ALLOW|DENY, reasonCode, policyRevision, expiresAt}
RouteHandle   := {routeId,deviceId,operatorId,startedAt,leaseExpiry,trafficBudget,
                  cancellationToken,closedReason}
```

必须强制的接口语义：

- `IEdgeSiteRegistry.enrollHmi(attestedIdentity, endpoint, adminApproval)`：仅管理员明确操作；关联可信设备身份材料与指定端点；不提供 `scanSubnet`、`connectAnyTcp`。
- `IEdgeSiteRegistry.revokeHmi(DeviceId)`：原子提升 `configRevision`，拒绝未来准入并取消所有同 DeviceId 的本地 Route；不影响其他 HMI、Direct 会话。
- `IRouteAdmission.evaluate(RoutePermit, RegisteredHmi, currentEdgeIdentity)`：**每个新 Route**复核租户/站点/Edge/设备/协议/角色意图/期限/配置修订，拒绝地址变化或身份信任材料不匹配。
- `IRouteRouter.open(Admission, BoundedDuplexStream)`：数据面必须只收到已核验的 `RouteHandle` 与**已登记设备**，无任意外部 endpoint 字符串；若 HMI 的 E2E 设备认证失败，关闭 Route，标为 DeviceIdentityMismatch。
- `IGatewayBroker.requestRoute(OperatorContext, DeviceId, intent)`：在 Gateway 检查租户、站点与用户访问许可，但返回的 `ALLOW` **仅代表允许建立受管路由**，不代表设备端 `ViewGrant/ControlGrant`。
- `IManagedRemoteSession.start(route)`：必须在 Route Ready 之后独立完成 `HmiSecureEndpoint` 的设备身份验证和设备端授权；一旦任何信任边界失败，给 UI 一个分层故障码，不能切到更弱 RFB 自动重试。

### 4.1 数据持久性与敏感性

| 对象 | 工厂 View 本地存储 | Gateway | 异地 View |
| --- | --- | --- | --- |
| Site/Edge/HMI 非秘密标识 | 可以有受保护配置和版本 | 已认证的登记/目录元数据 | 仅获准可见的目录 |
| Edge 私钥/凭据 | OS 受保护的密钥存储；不可导出为普通配置 | 只存必要公钥/证书及吊销状态 | 无 |
| 操作者令牌 | 无，除非本机同时作为操作客户端 | 只保留令牌散列/校验材料与短期会话状态 | OS secure store，内存最短生存 |
| RoutePermit/配对证明 | 短期内存，一次消费 | 原子一次消费账本和 TTL | 短期内存 |
| HMI 身份/指纹 | 经管理员验证绑定 DeviceId | 仅获准目录及公钥指纹元数据 | 可信注册/交付校验值，必须做 E2E 比较 |
| framebuffer/键鼠/业务数据 | **不持久化、不解码** | **不持久化、不解码** | 受设备端许可的渲染缓存；敏感失效时清除 |
| 故障/审计元数据 | 允许保留脱敏事件和证据来源 | 允许保留必要审计元数据 | 允许显示本人当前会话元数据 |

持久清单必须按安全级别迁移；`SiteId/DeviceId` 对象应使用稳定版本化 schema；任何未知的权限/密钥字段失败关闭而不按默认全开放处理。

## 5. 主流程与状态模型

### 5.1 站点生命周期

`DISABLED` (默认，无 Gateway 共享) → `ENROLLMENT_PENDING`（客户管理员人工建立站点信任）→ `CONNECTING` → `ONLINE_EMPTY`（没有已批准 HMI）/ `ONLINE_SHARED` → `DEGRADED`（任一上游故障）→ `OFFLINE` / `SUSPENDED` / `DISABLED`。

- 这只是**站点连接状态**，不能合并成 `HMI online` 或 `canControl`。
- 工厂管理员单独暂停共享时，`SUSPENDED` 即停止新增 Route 并取消现有受管 Route，不需要卸载 HyRemoteView；Direct 不受影响。
- `DEGRADED/RECONNECTING` 期间所有旧 RoutePermit、旧 HMI Grant 视为不可重新使用；恢复只允许重新发起会话，不自动重放业务操作。

### 5.2 路由状态

`REQUESTED` → `GATEWAY_ADMITTED` → `EDGE_ADMITTED` → `PAIRED` → `E2E_NEGOTIATING` → `DEVICE_AUTHORIZED_VIEW` [→ `DEVICE_AUTHORIZED_CONTROL`] → `ACTIVE` → `CLOSING` → `CLOSED`。

- 失败可从任意非终止状态进入 `REJECTED/EXPIRED/CLOSED`；**任何状态都不能从 route-only “在线”跃迁为 ControlGrant**。
- 站点失效/设备撤销/令牌失效/旧协议版本/缓冲溢出触发 `CLOSING` 并执行双向取消、输入释放；Gateway 只断网络 Route、HMI 执行最后的设备端输入撤权与隐私过滤。
- 单 Route 一次性配对，`RouteId` 不能被当作恢复 token 复用；重连创建新的 Route 和新的设备安全 Session。

### 5.3 客户端生命周期与“只装一个产品”

**建议的首个可验证运行级别（非正式承诺）：** 登录用户期 HyRemoteView 主进程运行时支持最小化/关闭一个 Viewer 标签，Edge 连接持续；用户明确“退出应用”或系统注销时 Edge 有界关闭并在 Gateway 上标为离线。若产品未来要求注销后无人值守，须比较同一 HyRemoteView 包中内置的受管后台模式并完成 Windows 服务安全/升级验证，不得私下引入第二款客户安装软件；不能把本方案当作当前版本已经支持系统服务。

用户界面至少区分 `VIEW_WINDOW_CLOSED`、`PRODUCT_RUNNING`、`EDGE_OFFLINE`、`EDGE_ENABLED`、`SYSTEM_LOGOUT`，不用泛称“后台在线”。

## 6. 资源与线程契约

- UI/QML 只负责展示/操作事件，不持有 socket、TLS 明文/证书私钥或阻塞 relay 循环。
- Gateway 与 Edge 中每条 Route 应有独立取消令牌、时间与字节预算、双向高低水位背压；**不得无限队列**，不得因一个慢 Viewer 阻塞所有其他设备或 HMI 本地 UI。
- 数据面单向流式转发不得重新解释、丢弃任意 Native 密文或 RFB 增量消息；只有 **HMI 发送端在编码/加密前**可按协议基准合帧/丢中间帧。
- **设计默认候选（仅用于 PoC 容量测试，不是签收 SLO）：** 单 Route 每方向待发上限 256 KiB；单工厂 Edge 默认并发上限 8 Route；控制消息不超过 64 KiB；路由创建凭据有效期不超过 60 秒。真正配额要用设备等级、网络代理/中继开销、并发及现场性能测量反证或调整，并明确“控制面数据”和“设备字节流”各自边界。任何配额超限都必须可观测并且安全关闭。
- Gateway Data Plane 不以画面推理/解码计费，不回传 HMI 原始图像日志；Gateway 元数据缺失不成为 Direct 的运行前置。

## 7. 当前可执行结论与待验证技术项

**已确定（来自 V15/V8）：** 同产品双角色、Direct 独立、指定 HMI 白名单、三层准入、HMI 最终授权、透明转发、无独立厂内 sidecar、E2E 安全先决条件。**不能重新开放为“可选独立 Connector”**。

**仍属工程候选：** Qt/C++ 内部类布局、是否采用单进程 / 同包受管后台服务、WSS 与 HTTP/2 CONNECT 性能选择、Cloud Gateway 实现语言/部署形式、具体协议帧编号、上述限额、DeviceId 证书管理界面。见本设计系列 02/03/04 的验证门禁；在 G3/G4 真正通过前不得把这些当作已冻结实现。

## 8. 追踪

- 产品条款：V15 2.1/2.2、5.3、8.6/8.7、10.1/10.2、11.4、12.2、13.8/13.9、14.1。
- 架构：V8 2.1/2.2、3.6、8.7、9.7、10.6、11.6、14、16.6、附录 D/E/F。
- 当前工程事实：`HyRemote/docs/security.md`、`HyRemoteView#1/#5/#6/#9/#10`、`HyRemote#190/#246/#827`。
