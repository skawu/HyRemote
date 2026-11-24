# HyRemote Stage 05｜Managed Edge/Gateway 接口 IDL 与撤销状态机（设计候选 V1）

> 归属 [#839](https://github.com/skawu/HyRemote/issues/839)；输入为 Human 已批准 HYR-PD-001/V15、HYR-ARCH-001/V8，以及已合并 [Stage04 #838](https://github.com/skawu/HyRemote/pull/838)。**规范级别：DESIGN_CANDIDATE，非发布 wire/API，未运行 PoC。** 本文件不修改 V8 附录 A 的 Native 1.0；不授权实现/部署/发布 Managed Public。与 Stage04 不一致时应回到批准的 V15/V8 和 Human 产品取舍核对，不能自行改产品。

## 1. 用户结果与分层责任

**完整闭环**：工厂 PC 只安装一个 HyRemoteView，在同一产品内登录站点/明确共享已核验 HMI；外地 HyRemoteView 经客户可选云端或自托管 Gateway 申请指定设备通路；工厂 HyRemoteView Edge 仅连接本地白名单端点；外地 View 通过透明隧道独立核验 HMI Native 设备身份和当前会话 ViewGrant/ControlGrant。Direct 继续离线运行；工厂 HMI 本地业务始终优先。

| 层 | 负责判断 | 不拥有的权利 |
| --- | --- | --- |
| Gateway Identity / Site Broker | Operator、Tenant、Site、站点路由可见性，RoutePermit 与通道配对 | 不签发 HMI ViewGrant/ControlGrant；不解析 HMI 字节 |
| 工厂 View SiteRegistry / EdgeAdmission | 本地管理员共享清单、可信 DeviceIdentity/允许的准确 LAN 端点、修订版本、期限 | 不将任意请求 IP:Port 转发；不授予 HMI 权利 |
| View RemoteSessionCore | Direct/Managed 共享设备会话接口；远程 HMI 证书/身份、Native Auth、画面/输入显示 | 不把 Gateway 登录、Route Ready 视作设备控制许可 |
| HMI Framework NativeEndpoint | Native TLS 设备身份、认证、Target/Scope、隐私、View/ControlGrant、输入最终复核 | 不信任未经证明的网关 OperatorId 字符串 |

**安全不变量 S-01~S-08**：S-01 默认无共享；S-02 每 route 仅一登记 HMI；S-03 RoutePermit 不是设备授权；S-04 Edge/Gateway 不终止 HMI Device TLS、不获取设备明文密钥；S-05 撤权先在权威一侧生效再异步传播；S-06 Relay 只做 FIFO、有界背压或整路断开，不删密文字节；S-07 断网重连不复活旧 Grant/输入；S-08 Direct 不受云可用性和订阅/站点状态影响。

## 2. 身份、对象与来源（IDL 语义、未冻结序列化）

| 名称 | 语义 / 产生或验证方 | 绑定关系与禁止来源 |
| --- | --- | --- |
| `TenantId` / `SiteId` | Gateway 经认证企业/站点 Registry 取得 | 不信任客户端自己填写的所属租户 |
| `OperatorPrincipal` | Gateway 认证上下文派生（OIDC PKCE 或受评审等价方式） | Payload 中自报的 `operatorId` 仅用于比对，不可决定授权 |
| `EdgeInstanceId` | 工厂同一 View 产品安装实例，关联登记公钥或等价密钥 | Gateway 验证持有证明；不能使用 PC 昵称替代 |
| `HmiDeviceId` | 经可信登记的 HMI 设备身份；远端 View 再经 HMI E2E 验证 | 不是 IP、主机名或显示标签；身份材料变更需复核 |
| `EndpointBinding` | 工厂 View 本地受控登记：地址/端口/协议档位/可信材料/修订 | 仅 Edge 可以解析；RouteOffer/客户端不得提供可拨号目标 |
| `PolicyRevision` | 工厂管理员修改共享策略时本地单调递增、持久化 | Gateway 远程快照不能覆盖本地较新版本 |
| `RouteId` + `RouteEpoch` | Gateway 生成的不可预测路由实例/代际 | 代际变更即新路由，不跨重连恢复旧 socket 或 HMI session |
| `RoutePermit` | Gateway 对指定 Operator/Tenant/Site/Edge/HMI/Profile/TTL 的带完整性保护授权证明 | 只能提请 Edge 当前策略二次批准；绝非 Grant |
| `PairCredential` | Gateway 为此 route 两端各生成的短时、单次、绑定身份/方向的秘密 | 只在认证保护通道发送；消费必须原子，日志不可见 |
| `DeviceSessionId` / `GrantEpoch` | HMI Native 端依 V8 附录 A 管理 | 不装入 Gateway IDL，不由云端续期或提升权限 |

最小持久状态：工厂 View 保存明确获准的 `RegisteredHmi`、站点开关与本地 `PolicyRevision`，密钥使用合格的受保护存储；Gateway 保存租户/站点/Edge 身份登记、共享设备的最小元数据、当前有效路由和有限时间的一次性票据消费记录。完整 HMI 帧、输入、设备端密码、Native 会话内容和裸票据不得持久化在 Edge/Gateway。

### 2.1 异地 Viewer 取得 HMI 可信身份的引导（必须先证据证明）

**不能用“Gateway 返回的指纹就是可信指纹”来证明端到端身份。** 远端 View 的可信 HMI 公钥/CA/指纹绑定必须有独立于可被入侵 Gateway 的信任根：候选方式为（a）客户受控 CA 签发并通过可信链校验设备身份；（b）现场管理员核对设备出厂凭据/物理指纹后，由客户受控签名身份签发受限、版本化设备信任声明供获准 View 核验。控制 Gateway 可以代为**传输**已签名声明，但不得创造或替换可信根，不可仅凭同一 Gateway 的目录 API 给出的 pin 完成首次信任。

候选 `DeviceTrustBundle` 语义字段：`siteId`、`hmiDeviceId`、`deviceIdentityKeyOrCaRef`、`issuerIdentity`、`bindingRevision`、`validFrom/validUntil`、`policyScope`、`signatureOrCaProof`。这是**设计数据对象，不是新 Native 1.0 消息或已冻结 Gateway wire**；必须防跨站、过期、回滚和重放，Remote View 使用**事先信任**的客户根验证声明，再通过实际 HMI TLS 与登记身份作独立匹配。

若信任声明缺失、无法证明来源、远端没有可信根、设备公钥更换且无可验证的安全轮换链，必须 `NEEDS_REVERIFY/DEVICE_REVERIFY_REQUIRED`，**不建立 Managed HMI 会话**。密钥轮换可由已受信旧密钥/客户根证明并经管理员批准；丢失旧密钥时需客户独立复核，不能由 Gateway 自行“修复设备证书”。Native 账户认证与 HMI 设备端 Grant 仍各自独立，不把客户站点签名声明当作 ViewGrant。

## 3. 控制信封（唯一候选语义模型）

本节是**逻辑 IDL**，JSON 仅供易读举例；JSON/CBOR、二进制编码、签名机制和正式版本号仍需 PoC 对比。禁止同时以 Stage03 `version=0` 和 Stage04 `major/minor` 开发两个互不兼容控制协议。

```json
{
  "protocol": "hyr.edge.control",
  "major": 0,
  "minor": 1,
  "kind": "OpenRouteRequest",
  "requestId": "<opaque-per-request-id>",
  "issuedAt": "<UTC>",
  "expiresAt": "<UTC>",
  "body": {
    "siteId": "<authorized-site-id>",
    "hmiId": "<authorized-hmi-id>",
    "protocolProfile": "native-e2e",
    "roleIntent": "view"
  }
}
```

- `major=0/minor=1` 是设计占位值，**不代表发布版本或实际可用服务**。在认证会话建立后才解析消息；会话绑定的 Operator/Edge 身份是权威，消息字段只是请求目标或相符性校验。
- `protocol` / `major` / `kind` / `requestId` / `issuedAt` / `expiresAt` / `body` 逻辑必需；附加身份域应按消息 schema 显式列举。不要信任自报的 `authContext`：**认证上下文在传输认证层建立**，不依赖 payload 文字；需在处理时与当前会话状态关联。
- 版本不兼容、未知强制字段、重复 JSON key、错误类型、过深嵌套、无效 UTF-8、超长消息，在分配大对象/发起网络连接之前拒绝；权限字段绝不采用忽略未知后默认开放。错误响应的 `requestId` 仅用于当前认证会话关联，不接受成为文件名或查询路径。
- 全部时间使用带明确 UTC 语义的序列化；凭证过期校验考虑时钟偏差上界与单调时钟观察，不允许因系统回拨把已过期许可续活。时间上界数值尚非产品 SLO。
- 控制消息与数据通道**独立认证、独立有限队列**。控制信封不得混入 HMI Native 1.0，也不让 DeviceSession 的输入流解析 Gateway JSON。

### 3.1 消息状态/角色矩阵

| 消息 | 发起 → 接收 | 必需请求/结果语义 | 权威校验和拒绝条件 |
| --- | --- | --- | --- |
| `EnrollStart/EnrollConfirm` | 工厂管理员经 Edge ↔ Gateway | Site/Edge 公钥证明、一次性登记挑战、明确管理员确认 | 未批准的管理员/旧挑战/站点变更拒绝；不可自动开户 |
| `EdgeHello/EdgeAccepted` | Edge ↔ Gateway | SiteId、EdgeInstanceId、sessionEpoch、capabilities、policyRevision | 与认证的 Edge 实例不一致拒绝；未知版本不能降级 |
| `ShareSnapshot` | Edge → Gateway | 仅已共享的 HmiId、协议档位、ShareRevision、鲜度 | 不是 HMI 已在线证明；撤销不能被延迟旧快照覆盖 |
| `ListAuthorizedDevices` | Operator ↔ Gateway | 经当前身份过滤站点/设备目录；附观测时间 | 无权者不能枚举 IP、未共享设备或其他 Tenant |
| `OpenRouteRequest` | Operator → Gateway | SiteId、HmiId、protocolProfile、roleIntent、requestId | 身份/租户/站点及安全档位检查；不能含 LAN 地址 |
| `RouteOffer/RouteDecision` | Gateway ↔ Edge | RouteId/Epoch、已绑定身份、HmiId、Profile、TTL、policyRevision、ALLOW/DENY | Edge 根据本地实时 ACL、设备身份/精确端点、当前 Revision 再判；不可仅信 cloud permit |
| `PairGrant/PairAttach/PairReady` | Gateway ↔ 两端受管 View | routeId/Epoch、单次凭据、双端角色、audience、到期 | 两条端点各自认证；原子消费；跨 route/重放/第三条通道失败 |
| `RouteRevoke/RouteClosed` | 权威管理员/Edge/Gateway → 对端 | routeId/Epoch、newRevision、关闭原因、幂等确认 | 权威撤销即时生效；陈旧完成消息不能重新进入 FORWARDING |
| `LinkHeartbeat/LinkStatus` | 控制端 ↔ Gateway | sessionEpoch、时间、最小链路健康 | 只能证明网络连接；不能证明设备 Grant 或 HMI live |

### 3.2 票据/配对绑定不变量

`RoutePermit` 的完整性保护必须覆盖：`tenantId/siteId/edgeId/hmiId/operatorBinding/protocolProfile/roleIntent/routeId/routeEpoch/policyRevision/audience/nonce/issuedAt/expiresAt`。Gateway 只对当前有效 Operator + Site 可授予；Edge 仍要在准入与**实际拨号前**检查真实白名单。每个 `PairCredential` 同时绑定一条确切 route 与一侧身份/方向；一条完整 route 仅能成功配对两条经过认证的数据连接。超时、撤销、连接失败要立即回收配对状态，任一重连使用新的凭证及 Epoch。**单次配对与 Epoch 隔离只约束网络路由，不能保证 HMI 业务命令“恰好执行一次”。** 如果断线发生在命令送达后、设备执行回执返回前，结果必须标为 `OUTCOME_UNKNOWN`，客户端不得自动重放报警确认、参数修改、复位等非幂等动作；应由操作者通过设备端可信状态/审计确认后决定下一步。若未来需要安全自动重试或去重，必须由 HMI 独立定义命令标识、设备端幂等性/持久结果记录、确认和异常恢复的业务合同，并在受控协议版本与实机验证后另行准入，不能由 Gateway PairCredential 替代。

## 4. 状态机、撤销竞态与终止语义

四类状态**不可折叠成一个“在线”布尔值**：

| 层 | 状态 | 必须满足的前置和终止条件 |
| --- | --- | --- |
| Site | DISABLED → ENROLLING → ENABLED / SUSPENDED / FAULTED | 默认 DISABLED；管理员撤权本地提交先于 Gateway 通知 |
| Gateway Link | DISCONNECTED → AUTHENTICATING → READY / RETRY_WAIT / REVOKED | 认证/证书失败不自动切明文或延续旧认证；重连需新 sessionEpoch |
| Registered HMI | DISABLED → VERIFYING → SHARED / PAUSED / NEEDS_REVERIFY | 设备地址/证书/身份变化立即 NEEDS_REVERIFY，拒绝新 Route |
| Route | REQUESTED → SITE_APPROVED → EDGE_APPROVED → PAIRING → FORWARDING → CLOSING → CLOSED | FORWARDING 只是透明网络连通；HMI DeviceSession TLS/Grant 另行进行；CLOSED 为最终态 |
| HMI session | TLS/Native Auth → ViewGrant → 可选 ControlGrant → REVOKED/CLOSED | 由设备端独立管理；不得由 Gateway 状态转移产生 |

**撤销优先级与竞态用例：**
1. Edge 本地撤销 HMI 分享：原子提升并持久化 `PolicyRevision`；使新 `RouteOffer`、尚未拨号的 Route、正在 Forward 的对应 Route 同步进入拒绝/关闭；随后异步通知 Gateway。通知丢失时本地拒绝持续生效。
2. Gateway 用户/租户许可撤销：服务端拒绝新申请与未完成 Pair；已建立 Route 向两端发送撤销并立即停止中继。在失联时不得凭旧快照继续放行新 Route。
3. HMI ViewGrant/ControlGrant 撤销：HMI 自己在数据输出/输入最终提交点执行权限检查，更新政策/GrantEpoch、清除 held 输入，防止排队旧输入被执行；Edge/Gateway 不伪造或解释这个 Grant。
4. `RouteOffer` 与本地 ShareRevoke 并发：必须以本地策略**提交点**为线性化点；offer 通过先前检查后、HMI 拨号前必须二次验证，避免 TOCTOU。
5. 断网、重启、租约超时、证书错误、数据通道超限：本次 Route 进入 CLOSING/CLOSED，清理两个方向数据队列与 HMI socket；**不能重接旧 TLS 流、续送旧加密片段、旧输入或业务指令**。
6. 控制断链但数据链接仍活跃：是否允许短暂保活取决于可证明的撤权传递上界；在无法及时确证权限时**先 fail-closed**，不得无限转发。恢复时完整建立新 Route 与 DeviceSession。
7. `RouteClosed`/ACK 乱序、重复或迟到：以 routeId+Epoch 区分，幂等处理；旧 epoch 不改变当前新路由状态。不得在 CLOSED 后进入 FORWARDING。

## 5. 透明 ByteStream 与资源上界

业务上层仅看一个 FIFO、全双工、可取消的 `ByteStream`：`open(verifiedRouteBinding)`；`write(bytes)` 返回“本地受限队列接受的字节数”（**不是 HMI 执行成功**）；`read()` 返回顺序字节或关闭；`shutdown(reason)` 幂等、取消未发送内容并传播关闭。Direct TCP 与 Managed Relay 只能替换下层传输，不可改变设备端 Native TLS/Auth/Grant 的行为。WebSocket message 边界不等于 Native message 边界。

| 约束 | 设计判据 | 失败行为 |
| --- | --- | --- |
| 入口先准入 | 有真实 Route+Epoch、两端配对、Edge 精确 HMI 绑定，才能传入字节 | 无凭据/无 ACL → 拒绝，不尝试 LAN 拨号 |
| FIFO 全量 | 各方向严格保序；不解释/裁剪 Native TLS/RFB 字节或自行丢弃“旧帧” | 无法及时交付 → 背压/关闭整个 Route |
| 有界所有层 | 控制包/二进制帧/完整 WebSocket message/Qt read buffer/业务队列/写队列/OS socket 都要显式预算与测试 | 限制内存、停止读取/超时终止，不能无界积压 |
| 现场资源优先 | Edge 网络线程与工厂 View Qt GUI/本地 Direct/设备 HMI UI 分离，异地慢端不得阻塞现场业务 | 资源过载拒新 route 或终止慢 route |
| 无持久敏感内容 | Gateway/Edge 不存 HMI 帧、键盘、业务请求；诊断仅脱敏 metadata | 违反视为安全失败 |

**Qt 6.8 公共 API 核对（2026-11-04）：** Qt 6.8 的 `QWebSocket::setReadBufferSize`、`setMaxAllowedIncomingFrameSize`、`setMaxAllowedIncomingMessageSize` **均存在**。默认 `readBufferSize=0` 代表无限制；允许的 inbound frame/message 默认最大也不能当作业务安全上限。必须同时设置三类上限、加上写出未发送量与业务队列水位测试；外层 Qt/TLS/OS 缓存和运行时峰值必须实测。参考 https://doc.qt.io/qt-6.8/qwebsocket.html 。Qt5.15 HMI Framework 不参与此 Gateway Control wire，HMI 仅以已批准的 Native 1.0 协议（Qt5/Qt6 通用线格式）接入；禁止用 Qt 元对象私有序列化互通。

**容量不是 SLA：** 每方向队列上限可写成设计参数 `QrouteBytes`，控制消息 `CmaxBytes`，最大路由 `Nroute`，最大等待 `Tbackpressure`，总可观测 RSS/私有内存以真实工厂 PC 分档测量后签收。Stage03 文档出现的 256KiB/8 Routes/30s 等只属 `DESIGN_DEFAULT` 的 PoC 假设，不进入产品支持承诺。Qt6.8 API 存在也**不证明**所有路径物理内存有界。

## 6. 错误分类、显示与重试责任

| 失败来源 | 候选规范分类 | 可展示用户含义 / 恢复规则 |
| --- | --- | --- |
| Gateway 身份或站点 | AUTH_REQUIRED / SITE_DENIED / SITE_NOT_FOUND | 无权不得泄漏设备存在；重新登录或管理员授权后申请新 Route |
| Edge 策略 | HMI_NOT_SHARED / POLICY_REVISION_CHANGED / DEVICE_REVERIFY_REQUIRED | 本地共享关闭或设备变更；没有静默重新信任 |
| Gateway 配对 | ROUTE_EXPIRED / PAIR_REPLAY / PEER_MISMATCH | 新建 Route 与 PairCredential；旧通道不能恢复 |
| 网络/容量 | GATEWAY_UNAVAILABLE / ROUTE_OVER_BUDGET / LINK_UNVERIFIED | Managed 不可用，Direct 不受影响；受限退避后仅重建新 route |
| HMI TLS / Native | HMI_IDENTITY_MISMATCH / DEVICE_AUTH_DENIED / DEVICE_VIEW_DENIED / DEVICE_CONTROL_DENIED | 必须区分“到达设备”与“能看/能控”；不可回退到明文 RFB |
| 已结束 | ROUTE_REVOKED / ROUTE_CLOSED / STALE_EPOCH | 清掉 UI 控制状态/待发输入；按当前权限重新发起 |

所有错误先从真实产生层归类；Gateway 不假装 HMI 授权结果。对无权用户显示统一授权失败，不暴露邻站设备与 LAN IP。重试仅对可恢复网络类错误进行有限退避，认证失败/证书冲突/撤销不自动重试到弱安全模式。输入/报警确认等非幂等业务动作**不能**靠 Route 重试来保证自动重放。

## 7. 最小可证伪检查（全部 NOT_RUN）

| ID | 负向/正向 fixture | 期望可断言结果 |
| --- | --- | --- |
| CTRL-01 | 重复/未知关键 JSON 字段、长度和嵌套超限、非法 UTF-8 | 解析前限额；无大内存分配；确定性拒绝 |
| CTRL-02 | 自报 operatorId/tenant 与认证会话不符 | 绝不更换身份或扩大目录 |
| CTRL-03 | major 版本不兼容 / 未协商能力 | 显式拒绝，无降级或任意执行 |
| CTRL-04 | 外地请求携带 host:port 或 DNS | Edge 不使用该地址、不拨号 |
| CTRL-05 | 未共享 DeviceId 或跨租户 HmiId/RouteId | Gateway/Edge 分别拒绝，无设备枚举泄漏 |
| CTRL-06 | 旧 PolicyRevision 下 RouteOffer 与 ShareRevoke 并发 | 本地提交撤权后无新 socket/Route |
| CTRL-07 | 配对 token 重放/跨端点/过期/第三连接 | 原子一次使用，额外连接拒绝 |
| CTRL-08 | 旧 epoch 迟到 ACK/PairReady/RouteClosed | 不能改变新 epoch 状态 |
| CTRL-09 | TCP 半开、单边失联、控制断链、Gateway 重启 | Route 有界收敛 CLOSED，不复活旧 TLS/Input |
| DATA-01 | 双向随机二进制流拆包/合包/含零字节 | 顺序和字节 hash 一致；不依赖 WS 分帧 |
| DATA-02 | 缓慢消费者+高频发送、所有层统计与限额 | 有界内存/高水位，最终背压或整路关闭 |
| DATA-03 | 注入部分 TLS 记录或中途撤权 | 不裁字节，不解析设备帧；整路关闭 |
| TRUST-01 | 替换 HMI TLS 身份、错误 CA、证书变更 | E2E 认证失败且无设备像素输出 |
| TRUST-02 | Gateway 已允许但 HMI 未授予 View/Control | 无 ViewGrant 不显示像素；无 ControlGrant 不提交输入 |
| TRUST-03 | HMI 撤销时输入已在 GUI 队列 | 最终提交点复核并释放 held 状态 |
| TRUST-04 | Gateway 替换 HMI 公钥/指纹、跨站重放信任声明、未经批准的 HMI 换证 | Remote View 按独立可信客户根/证书链拒绝；没有可信引导绝不开设备会话 |
| OPS-01 | 工厂 PC 只装 View、退出/注销/重启/升级 | 精确反映当前支持等级，卸载无独立 sidecar |
| PRODUCT-01 | Gateway/WAN 故障 | HMI 本地 UI 与合法 Direct 仍工作 |
| PRODUCT-02 | 双 HMI/多 Viewer / 一台不共享 | 共享和租户严格隔离、资源受限 |

真实 PoC 需 exact SHA、Qt Kit、OS/设备、网络/代理条件、构建测试命令、可复现 raw result、异常用例与撤销时间/内存证据；没有执行过的一律 NOT_RUN。`Managed Public=NOT_QUALIFIED`；`Gateway Binary Wire=NOT_FROZEN`；`Native E2E=NOT_IMPLEMENTED`。

## 8. 仍需技术证据驱动的决策

1. JSON vs CBOR、实际版本协商与字段编码：以恶意输入解析、消息大小、调试/跨语言维护成本和兼容测试取舍；不先定永久 public wire。
2. Edge 身份：mTLS 或等效持有证明、现场密钥保护/轮换/丢失恢复；凭证方案须真实自托管运维闭环。
3. WebSocket/TLS、HTTP/2 CONNECT 及企业代理：默认验证 Qt6.8 公开 API，确有证据再替换；无需预先引入 QUIC/UDP。
4. 同一 HyRemoteView 安装器下登录期/同进程、独立内部后台运行组件的支持级别：用户注销后在线**不是**未经实测的默认承诺。
5. HMI Native TLS/Trust/Grant 依已批准 V8 附录 A，实现不能通过本 Gateway IDL 偷改线协议。重大产品/安全取舍须提交 Human，普通技术 PoC 由 Owner 连续推进。

**阶段状态：** `DESIGN_DRAFT_READY`（待 PR 审查），`IMPLEMENTATION_NOT_STARTED`，`RUN_EVIDENCE_NOT_RUN`；不修改现有 release train。