# HyRemote 阶段 03｜Gateway—FactoryEdge 受管接入协议与威胁模型（D2）

**审批基线**：HYR-PD-001/V15、HYR-ARCH-001/V8；关联 #831。**状态**：协议设计候选，非已发布 wire 标准，也不是 Native 1.0 的改写。企业网络中任何明文 RFB 不应因为外面加一层 Gateway TLS 就被误写为端到端受保护。

## 1. 产品边界与安全目标

**四个参与者：** 异地 HyRemoteView (Operator Client, O)、Gateway (G)、工厂 HyRemoteView 的 FactoryEdge (E)、HMI Framework (H)。

**三条独立授权轴：**
- `SiteAccess`：G 对自然人/组织能否发现某站点和申请某 HMI Route 的准入；
- `RouteAdmission`：E 再次校验该 HMI 是否仍在本地共享清单、身份可信、票据未过期且该连接符合站点策略；
- `DeviceAccess`：H 独立核验真实设备身份，决定 ViewGrant/ControlGrant/TargetScope/业务授权。

任何两层 PASS 不推导第三层 PASS。G 和 E 不能因为掌握路线票据而伪装 H，不能读取端到端受保护画面/输入；跨租户、跨站点及任意内网代理全拒绝。

## 2. 控制面与数据面

### 控制面

G 提供可自托管服务，最小职责：Gateway trust/tenant+site registry、认证后的 Operator scope、Edge 在线记录、共享设备元数据、RouteTicket 一次性配对、会话短 TTL 和审计元数据。Gateway 自己不实现 Qt 画面抓取、Viewer 控制/输入映射、设备授权或业务命令。

E 主动建立出站可信控制连接（MVP 候选：TLS1.3/WSS/TCP443）。E 对每个收到的 `RouteOffer` **在本地重新执行**设备身份/清单/安全配置校验；若任何不一致，应拒绝且不对外披露内网细节。Operator Client 的出站连接也必须经过受信 Gateway 校验。

### 数据面

G 把两个经过验证且持有匹配的一次性 RouteTicket 的连接**配对成一条有界字节通道**。E 只按 HMI ID 映射到已登记、重新验证的具体设备端点，双方 FIFO 转发，任何溢出通过背压/超时/终止处理。

设备协议依旧为 RFB 或 Native，不新增“Gateway 远控协议”。若采用 Native TLS，TLS 的设备证书与端到端握手必须是 O ↔ H，**透过** G/E 的字节转发而不在中间节点终止；G↔E 与 O↔G 的外层接入认证保护控制面及防未准入滥用，并不取代该 O↔H 信任。

### 协议安全资格

| 设备协议模式 | Direct | 通过受管路线的初始资格 |
| --- | --- | --- |
| Native TLS 且 O 验证真实 HMI 身份，H 独立发放 Grant | 依当前批准的 Native 设计独立资格 | **首选 Managed 安全方案**，但尚无实现/真实资格，不能直接标 Supported |
| Native plain | 仅按其受信网络/配置声明使用 | 不得直接按公网安全能力发布 |
| 传统 RFB VNC Auth（不加密） | 根据已有直接连接真实支持范围 | **初始受管产品不纳入默认安全支持范围**；外层 TLS 只能保护它的部分网络跳段，无法建立 O↔H 的强身份与端到端隐私 |
| 第三方 RFB + 经过严格资格的 E2E 安全隧道 | 仅在另有证据时声明 | 后续另行评审，不用“网关兼容 VNC”当作已支持 |

以上是**产品安全最低门槛的详细设计建议**。如果 Native TLS 真实实现未到位，Managed 受管查看功能应维持 `NOT_AVAILABLE`，而非临时用不安全模式上线。

## 3. 控制协议草案（仅为 Gateway/Edge，不变 Native wire）

控制帧统一使用 `protocol_version`、`message_type`、`request_id`、`body_length`、`auth_context`；大小、版本、字段和未知消息在分配前有上限，所有解析失败均返回受控错误或断开，绝不能变成远程命令执行。

| 消息 | 发送方 → 接收方 | 必要业务语义 | 失败行为 |
| --- | --- | --- | --- |
| `EDGE_ENROLL_REQUEST/RESULT` | E↔G | 站点管理员许可、Edge 身份、Gateway trust、设备最小范围 | 未授权不能自动成为在线 Edge |
| `EDGE_PRESENCE` | E→G | 心跳、身份代次、策略代次、可用性；不得附带内网扫描结果 | 超时将在线状态变 UNKNOWN/OFFLINE |
| `SHARED_HMI_UPDATE` | E→G | 经过管理员审核的 HMI ID 与可见元数据，带策略版本 | 缺版本/越权更新不能发布 |
| `ROUTE_INTENT` | O→G | 目标站点与 HMI ID、设备协议意图、用户身份上下文 | 无站点许可或目标不在可见范围时拒绝 |
| `ROUTE_OFFER` | G→E | 短期路由 ID、站点/Edge/HMI/操作者绑定、nonce、TTL | E 必须重新校验 ACL，不接受传入原始 IP |
| `ROUTE_ACCEPT/DENY` | E→G | E 选择是否开始受控路由，不附加 HMI 设备授权声明 | 缺失或超时不打开数据通道 |
| `ROUTE_PAIR` | G→O/E | 一次性、精确受众、精确路由和两端配对资格 | 重放/错配/过期全部拒绝 |
| `ROUTE_CLOSE` | O/E/G→其余 | 关闭原因、清理代次，不能携带明文凭据 | 幂等关闭，不恢复旧 Grant |
| `EDGE_POLICY_REVOKE` | E→G | 站点/单设备撤销及新策略版本 | 拒绝旧代次新路线，关闭受影响路线 |

**严格区别：** `ROUTE_ACCEPT` 仅说明 FactoryEdge 愿意转发，不是 HMI `ViewGrant`；`ROUTE_PAIR` 也不赋予任何 HMI 输入权。要暴露的 HMI 最小信息必须由授权后的客户端获取，不得从未经验证的全局目录泄露厂区资产名称和地址。

## 4. 信任与威胁矩阵

| 攻击或错误 | 关键防护 | 独立负向验证 |
| --- | --- | --- |
| 伪造 Gateway | E/O 预置或管理员注册的可信根、证书名与更新策略，禁止接受任意自签 | 替换证书/错误域名必须失败关闭 |
| 伪造 FactoryEdge / 跨站点配对 | Edge 设备凭据绑定 SiteId/EdgeInstanceId；每次票据有唯一 nonce/audience/TTL | 不同 SiteId 的票据不能通过配对 |
| 失窃或重放 RouteTicket | 一次性消费、短有效期、绑定 O/E/G/目标/协议与策略代次；可靠服务器时钟 | 同票据第二次使用拒绝，过期拒绝 |
| 伪造 HMI / DNS 重绑定 / 内网地址变化 | RegisteredHmi 的设备信任锚 + 受信端点核验；连接后仍校验 HMI 端到端设备身份 | IP 指到另一设备必须拒绝 |
| 站点用户越权浏览设备 | G 限制最小可见目录，E 本地共享白名单二次核验 | HMI1 共享、HMI2 未共享时绝不能路由 HMI2 |
| 绕过设备查看与控制授权 | O↔H 设备会话握手、View/ControlGrant；不能由 Relay 代授权 | 站点准入 PASS 但设备拒绝时无可见画面/输入 |
| 中间节点读画面/注入输入 | O↔H 原生设备身份与端到端加密/完整性；Relay 只得外层通道权限 | G/E 攻陷模型不得获得新设备 ControlGrant |
| 暴露任意工厂 TCP 服务 | E 只根据已注册 HmiId 绑定受信 HMI endpoint，不接受公网请求者 IP/端口 | 请求打印机/数据库/路由器端口拒绝 |
| 慢连接队列暴涨 | 每方向固定/有界队列、背压、最大时长、速率与并发限制 | 模拟0速率接收，内存有硬上界，超时关闭 |
| 故障/撤权后旧会话还可写入 | 策略代次、Route 取消、端点 Close、HMI ControlGrant epoch 检查、held-input release | 站点撤销/设备端撤权后不接收旧输入 |
| 证书或凭据轮换 | 明确有效期、预备新密钥、安全撤销、审计 | 旧凭据撤销后连接失败，不回退到 plain |
| Gateway / E 日志泄密 | 默认仅结构化元数据、最小保存期、日志脱敏 | 检索日志不应含明文屏幕/密钥/输入/完整 ticket |

**未解决的端到端难点：** 仅拥有“Gateway 到工厂 Edge”的 TLS 不等于“Operator 到 HMI”的 E2E。若设计要求在 Gateway 或 Edge 终止设备 TLS 才能看到数据，则它违反这里的默认信任模型，需要作为实质设计冲突提交决策，而不是默认采用。

## 5. 资源与速率控制（框架，不冒充签收数值）

设站点允许设备数 `D`、活跃路由 `R`、单路上行队列 `B_up`、下行队列 `B_down`、路由状态元数据 `M_r`、设备登记元数据 `M_d`；Edge 最低受管理预算需覆盖：

`M_edge ≥ R × (B_up + B_down + M_r) + D × M_d + M_control`。

此式是**核算结构**，不是性能目标或总进程 RSS 承诺。具体 D/R/B 的最大值待基准设备和真实场景选定后定量验收；达上限在分配前拒绝。每路输入流读写必须遵守 FIFO；不能对端到端加密流任意丢字节。当下游阻塞时应使上游背压，超时后关掉整条 Route，HMI 端按自己协议规则处理增量帧/撤权。

`Runtime` 原有受管理 128 MiB 预算仍适用于 HMI Framework，不能按此数字推导一个工厂 PC 能转发多少设备。

## 6. 架构选型评估（证据等级与当前结论）

| 选项 | 产品价值 | 代价与限制 | 当前选择 |
| --- | --- | --- | --- |
| WSS/TLS1.3 TCP443 | 多数受限企业网络更容易允许出站 TCP443，易接入代理与常见运维 | 不保证所有 HTTP 代理允许 WebSocket；跨反向代理负载处理复杂 | **MVP 优先候选**，必须做真实代理/防火墙兼容验证 |
| HTTP2 CONNECT | 同连接多路流，可共享流控 | 企业代理支持差异和实现复杂度 | 后续候选，不为初版预先实现 |
| QUIC/HTTP3 | 独立流及路径迁移 | 工业网 UDP 常受限 | 不作强制连通路径 |
| Gateway opaque TCP relay | 不需了解 RFB/Native，易隔离信任面 | 不能按显示内容做智能丢帧 | **优先设计**；帧调度在 HMI |
| 在 Gateway 终止 Native TLS | 中继可识别业务内容 | 使中间节点获得设备会话明文，违背 E2E 最小信任 | **拒绝默认方案** |
| 独立 Edge Connector 或 Support Bridge 安装包 | 历史 #190 的隔离思路 | 违反工厂只安装 View | **明确排除** |

不把候选协议写成已批准唯一 wire；最终首版协议必须经过签名/证书/密钥轮换与现有 Qt/OS Kit 资格复核才可冻结。

## 7. Gateway 数据与运维

建议以**单节点自托管**作为第一个可验证部署：认证/目录/短期 Route Broker 与 relay data plane 可以同节点、逻辑隔离；可选后续多节点。持久存储仅保留最少的站点/Edge/HMI 分享范围、审计元数据和密钥版本，不保存画面与输入。进程重启后的旧内存 Route 统一作废、Edge/Client 重新登录并重新取得设备最终授权。

Gateway 必须提供安全升级/回滚、凭据撤销、日志清理、健康检查、证书更新及站点隔离的管理员操作说明，不能把托管云可用性写成 Direct 成立的前提。

## 8. 需下一阶段冻结的具体技术契约

1. Gateway/Edge 控制协议 IDL、最大帧长度、字段版本化、错误码与拒绝日志；
2. Edge/Operator 双向身份引导、证书轮换与撤销流程；
3. RouteTicket 的格式/签名机制及 nonce 去重存储在 Gateway/Edge 间的职责；
4. 站点管理员权限与设备分享策略迁移，包含现场注销/升级；
5. Managed Native E2E 的真实设备身份证据；RFB 受管路径的资格及禁止项；
6. 断线/撤权的有界处理时限与在设备协议层的 held-input 释放证据；
7. 产品支持的精确 OS、Qt Kit、工厂 PC 等级、Gateway 自托管环境和资源指标。

以上未签收前，`SECURE_MANAGED_ACCESS=NOT_IMPLEMENTED`、`PERFORMANCE=NOT_MEASURED`、`PROTOCOL=DESIGN_CANDIDATE`。
