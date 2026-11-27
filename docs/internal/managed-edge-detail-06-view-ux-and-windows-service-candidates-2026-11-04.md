# HyRemote 阶段 03｜首版受管接入技术契约候选（D4）

**依据**：HYR-PD-001/V15、HYR-ARCH-001/V8；以 #830 / PR #833 已合并的五份详细设计文件为权威，本稿仅补充 **产品 UI 与 Windows 运行模式原型候选**，不得与已归档的 G3/G4/ADS 门禁相冲突。**性质**：面向研发的可否证设计候选，未完成技术冻结、真实安全审查或性能验收。此处不允许直接修改 Native 1.0 wire，也不把受管协议做成第三种 HMI 远控协议。

## 1. 明确选择的产品首版切片

**首版目标候选（在 G3/G4/Native 安全和 Gateway 证据通过以后）**：工厂 PC 一个 HyRemoteView 产品可同时 Direct 查看多个 HMI、管理员授权共享指定 HMI、在许可的有人值守模式中主动连接自托管 Gateway；异地 HyRemoteView 通过同一程序直接建立受管的 Native TLS 会话，无需另装 Support Bridge/独立 Edge Connector；设备端最终授权决定画面与输入；Gateway 不保留画面。

**明确不承诺**：独立第三方 VNC Viewer 的受管公网路径、任意 TCP 透传、NAT P2P 直连、所有 Linux/嵌入式平台无人值守、自建云 SaaS 企业多区域 HA、设备端 Native TLS 已经发布、任何具体设备规模已经实测通过。

这是一条按用户最小可行旅程划定的切片，并不阻止后续扩展已有 V15/V8 功能。

## 2. Windows 工厂 PC：真正的“只安装 HyRemoteView”

**按照已合并 #830 的实施顺序，首个可验证级别是用户登录期间的同产品运行模式；用户明确退出或注销时 Edge 真实离线。** 只有该级别、HyRemoteView #11 ADS 采用门禁与 #5 G3 完成后，才研究无人值守级别。对于未来无需登录也能访问的资格，建议比较**同一个主程序二种进程运行角色**（不是独立安装软件）：

- `HyRemoteView.exe`：普通用户图形客户端（Direct/Managed GUI），启动参数及内部受信 IPC 向后台申请管理状态；
- `HyRemoteView.exe --managed-edge-service`：由同一个二进制在 Windows Service Control Manager 下运行的可选 Edge 实例，采用 `QCoreApplication` 等非 GUI 事件循环，不访问用户桌面；仅在客户管理员明确授予后台运行权限后注册和启动。

**无人值守资格**依赖 OS 服务账户/凭据访问许可、注销/重启/升级/崩溃恢复测试，未通过不得标 supported。统一安装、卸载和更新；SCM 服务名称、日志路径、OS ACL 和首次授权显示在 HyRemoteView 的受控配置页。配置不能成为任意用户修改 SYSTEM 服务路由目标的提权入口。

**为什么优先同二进制双模式：** 直接兑现“不再额外部署其它软件”，并可在用户注销后保留被明确允许的工厂 Edge；GUI 故障不必直接终止 Edge。需独立审查 GUI 依赖对服务进程的体积、攻击面、部署许可和 Qt 插件加载。若此方式无法满足 OS 安全/可用性，允许保持**同一产品同一安装包**的内部服务组件方案，但必须证明没有额外客户安装步骤；这种内部进程拆分不作为独立产品发布。

**如果未取得真正无人值守资格：** 工厂 PC 可以在已登录的普通 GUI 进程中运行有人值守 Edge；登出或退出会把异地连接标为不可用，不能伪称无人值守在线；不能为了服务长期存活要求客户自行设置系统 VPN 或开公网入站端口。

### GUI 与服务之间的信任边界

- 站点共享清单的修改必须走 OS 身份认证 + 产品管理员授权 + 受限 IPC，服务在最终写盘前再次检查操作者权限；普通查看用户不能修改站点白名单或 Gateway 信任根。
- IPC 消息采用命令固定白名单：查询状态、请求开关、增删经验证 HMI、撤销/轮换凭据、查审计摘要。不支持远程执行 shell、传入任意原始网络端点或动态加载代码。
- GUI 可以退出而 Edge 服务继续；进程关闭时全部未授权 GUI 输入自动结束。服务异常时 GUI 以真实状态提示，不缓存旧 ONLINE。
- 共享目录和凭据属于机器级安全存储，敏感凭据限制给受控服务身份。使用 Windows DPAPI/机器凭据存储等受支持的 OS 机制，不能明文写入 JSON 或命令行。

## 3. 控制协议 IDL 级草案

初版建议控制面 `wss://<approved-gateway>/edge/v1/control`（E 侧）与 `.../client/v1/control`（O 侧），每个连接具有外层 TLS1.3、端点身份和明确请求角色。**这些 URL 路径与 JSON 字段目前只是设计草案，不得作为已批准公共接口发布。**

```json
{
  "v": 1,
  "type": "route.intent",
  "request_id": "opaque-128-bit-id",
  "site_id": "opaque-site-id",
  "hmi_id": "opaque-device-id",
  "protocol": "native-tls-1",
  "requested_access": "view",
  "created_at": "UTC-with-milliseconds"
}
```

其余受控消息为：`edge.enroll` / `edge.presence` / `site.share.changed` / `route.offer` / `route.accept` / `route.deny` / `route.pair` / `route.close` / `edge.policy.revoked`。大字段/未知关键字段/版本不符时**分配前拒绝**；协议字段类型、单位、长度、安全保护和版本升级规则最终冻结为 IDL，不从任意用户文本命令生成网络操作。

**`route.offer` 不包含由外地用户指定的 IP/port/主机名。** 其目标仅为 `SiteId/HmiDeviceId/ProtocolProfile` 与签名或等效防篡改的一次性 Ticket。E 从本地获准目录查询 HMI 的受信端点，并在建立设备连接时再次确认真实设备身份。

### RouteTicket 的安全不变量

```text
route_id, ticket_id, nonce, site_id, edge_id, hmi_id, operator_binding,
protocol_profile, audience, issued_at, expires_at, policy_revision,
authentication_proof (MAC / signature, exact scheme pending)
```

- 票据有效期**初始工程假设：30 秒**，只能被配对一次；不自动重复使用或转化为长期登录状态。使用服务端有界去重账本与短期随机 nonce。
- 业务 UTC 用于可理解的有效期/审计；本机单调时钟用于本连接计时与超时，必须检查时钟漂移，拒绝明显不合理的时间。
- 绑定操作人、站点、Edge、HMI、协议安全 profile、受众以及当前 SitePolicyRevision；任何不符拒绝。
- 只有中继路由资格，**没有 HMI 设备 ViewGrant/ControlGrant 权能**。
- 如果丢失或撤销 Gateway 的签名私钥/根身份，中间层不能把旧 Ticket 转为有效业务凭据；必须重新注册/重新授权。

### 初始错误码候选

| code | 源 | 含义与 UI 行为 |
| --- | --- | --- |
| `SITE_NOT_AUTHORIZED` | G | 操作者没有站点/设备发现和申请资格，不泄露地址 |
| `EDGE_OFFLINE` | G | 工厂 Edge 不可达；不能显示设备端授权状态 |
| `HMI_NOT_SHARED` | E | 对应设备不在当前白名单，拒路由 |
| `HMI_IDENTITY_MISMATCH` | E/O | 本地端点或端到端证书与可信 HmiId 不符 |
| `TICKET_EXPIRED_OR_REPLAYED` | G/E | 原路线票据失效，需要新 Route |
| `POLICY_REVISION_CHANGED` | E | 站点共享状态已变，必须重新申请 |
| `DEVICE_VIEW_DENIED` | H | Route 可达但 HMI 拒查看（由设备协议返回，不伪装为 Gateway 错） |
| `DEVICE_CONTROL_DENIED` | H | 只能查看，无法远控 |
| `ROUTE_OVER_BUDGET` | G/E | 通道队列、带宽、超时或并发限额导致受控断开 |
| `TRUST_VERIFICATION_FAILED` | O/E | Gateway/HMI 认证失败，禁止静默回退到 plain |

具体分组、编码与可国际化信息在 UI/协议二次评审时冻结。任何敏感安全失败不把 HMI 内网 IP、用户名或凭据回传未经授权方。

## 4. 数据流抽象与透明安全

为了在**不提供 Bridge 监听端口**的情况下从异地 HyRemoteView 直接接入，`ManagedRemoteSession` 需要一个等效可靠 FIFO 字节流 `ByteStream` 抽象：

```text
open(route_binding) -> StreamHandle
write(bytes) -> accepted-by-local-queue (never means HMI executed)
read() -> ordered bytes or close/failure
shutdown(reason) -> terminal, idempotent
```

同一上层 `DeviceProtocolSession` 可以使用 direct TCP ByteStream 或 Managed Relay ByteStream；两者的 HMI Native 安全握手和设备权限语义**必须相同**。Managed Relay ByteStream 通过 Gateway 控制面协商并传递端到端设备协议字节，G/E 不理解 Native payload。此 API 是内部架构接口建议，并未批准公开 SDK 名称。

**完整性约束：** 任意丢字节或更换两个数据通路的顺序都可能导致 TLS/RFB/Native 协议断流，故只能 FIFO、有界背压、超限整体中断。HMI 端在编码/加密之前依协议自身帧调度合帧；设备已加密输出后的内容不可在 E/G 选择性删除。

## 5. 初始资源预算假设（只有实测后可成为验收阈值）

以下数值为防止实现出现无限队列而设的**原型工程候选**，不是 V15/V8 产品 SLO，也不表示已支持相应容量：

| 维度 | 首轮原型候选值 | 达限行为 |
| --- | --- | --- |
| Edge 共享设备清单 | 32 个 HMI/站点 | 拒绝新登记，提示管理上限 |
| 活跃 Route | 8 个/Edge 实例 | 拒绝新建路线，不牺牲既有合法会话 |
| 单路线方向待发缓存 | 256 KiB，上/下行各自独立 | 背压，连续超时则关闭整条路线 |
| 一次性 Ticket TTL | 30 s | 到期失效不自动续权 |
| 未验证控制消息体 | 64 KiB 硬上限 | 先拒绝后分配，超限计数/受控断开 |
| 路由空闲超时 | 60 s 原型起始值，须区分静态视频的合法无流量 | 仅在有受信心跳与授权有效性条件下判定；不得把无像素更新直接当离线 |
| Gateway 重连退避 | 1～60 s 内指数回退与抖动 | 不进行紧密 CPU 重试 |
| 每站点业务控制日志 | 必要元数据/政策版本，按客户保留策略 | 默认不记录明文画面/按键/完整票据 |

内存仅从单路每方向缓冲至少 `8 × 2 × 256 KiB = 4 MiB` 开始计算，**还须加 TCP/TLS/Qt 运行时/GPU/日志/设备清单/证书/系统缓冲与峰值碎片**。真实内存上限、带宽、设备规模需要按典型工厂 PC 等级和 WAN 负载测试定量确认，不能把上表写成“已达标”。

**审查特别点：** 路由空闲值不能导致“工业 HMI 画面完全静止但合法会话正在观看”被错误踢出；应有独立已认证保活与权限心跳，而不能只依赖帧数据到达频率。

## 6. 产品 UI 交互规格（不以技术表单取代真实使用）

### 现场工厂 PC（HyRemoteView）

普通工作区主要区域仍是“设备列表→目标→画面/控制”。开启站点管理才进入“工厂共享”：显示受信 Gateway 名称与指纹摘要、站点名称和 ID、是否已开启、已登记 HMI、是否对外共享、当前会话、最近连接失败。默认开关为 OFF；首次开启须管理员权限、二次确认共享范围并验证本地 HMI 身份。

共享 HMI 时显示：设备可信身份、设备显示名、所在站点、当前 LAN 可达性、允许受管的设备协议档位、共享有效期和启用状态。**不提供“共享整个网段”“转发任意端口”选项**。

禁用共享必须明确终止站点当前受管 Route；禁用单设备不能顺带断开其他获准 HMI 的 Direct 或无关 Route。不能因 Gateway 在线而把所有 HMI 标为可查看/可控制。

### 异地 PC（HyRemoteView）

工作区提供“本地设备”与“受管站点”两种入口，最终画面/输入采用同一设备会话模型。选择站点/设备后依次显示：站点许可→工厂 Edge 在线→目标设备身份已核验→**设备查看许可**→（可选）控制许可。连接成功但无 ViewGrant 时不得先显示一帧受限画面或“临时预览”。

错误提示区别：云端登录失败、站点不可用、HMI 被取消共享、设备身份变化、设备端拒绝查看、只读控制受限、网络超时。所有失败可在非敏感审计记录中得到对应事件，不让用户猜“连接失败”。

### 不可用与恢复

GUI 窗口关闭不等于 Edge 服务关闭；退出产品/注销/重启的行为依平台实际运行资格，界面必须明确说明当前可用性。服务未安装/未授权却显示“后台在线”为错误。恢复后重新申请网络路由并核对 HMI Grant，绝不重发已经发出但未确认的参数修改、报警复位等业务操作。

## 7. 需要经过的技术验证与签收

1. 单二进制作为 GUI 与 Windows Service 的 Qt6/Qt5 进程运行隔离、安全安装/卸载与凭据访问能力；不能因为 Windows 可注册 Service 就宣称已满足产品要求。
2. Native TLS O↔H 在 Managed ByteStream 上的证书核验与会话授权；中间节点可读明文的情况必须否定。
3. WSS/TLS through corporate proxy、Gateway 自托管证书及实际双向收发、断网/恢复/撤权/拒绝。
4. 32 HMI、8 Route、256 KiB 和 30s Ticket 等全部属于**待测候选数**；分别以可测量指标确定最终规模与资源上界。
5. Endpoint IP 与证书错配、ticket 重放、策略修订、用户注销、服务升级和旧输入释放的负向测试。
6. 在 V15/V8 内确认的其他产品功能未实现之前，只对已实际合格的精确功能/OS/Kit 标注可用，不能对外发布完整新架构 PASS。

**Gate 优先顺序：** 首先完成 HyRemoteView #11 ADS 事实核验与 #5 G3；本文件不得直接授权 G4 正式 Viewer 产品代码、Native TLS 实现或 Gateway 生产服务。\n\n**下一步详细技术合同**：锁定身份引导/撤销、控制消息 IDL 的正式字段与错误码、OS 后台运行安装契约以及真实设备/网络资源预算；按实际验证与独立审查收口。正式实现准入依赖技术合同签收与实施分期而非仅文档提交。
