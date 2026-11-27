# V15/V8 模块详细设计 05｜HMI Native/TLS 端点与设备最终授权（设计候选）

> #830；归属 HMI Framework / `skawu/HyRemote`。本文件以正式版 V8 第 5、6、7、8、9 章和附录 A/B 为准；**绝不另造 Native wire 字段或消息**。此文件是如何实施已批准设备端设计的内部技术设计，不是“Native 已完成”证明，不授权跳过技术验证或发布流程。工厂 Edge/Gateway 仍只转发字节，不迁入设备授权。

## 1. 目标与唯一最终身份链

```text
经 Gateway/Edge 建立的 Route（只允许字节到达指定 HMI）
   ≠ HMI Device TLS 身份通过
   ≠ HMI Native 用户/会话认证通过
   ≠ Target 被允许查看
   ≠ 当前 ControlGrant 可执行键鼠/业务输入
```

工厂 HMI 的 Native 安全端点必须在**设备自己**的受控监听范围内完成 TLS 服务端身份与 Native 用户认证。异地 HyRemoteView 通过 Gateway+Edge 透明路由建立 **View ↔ HMI** 的 TLS 会话（Gateway/Edge 不持有该 HMI TLS 私钥、会话密钥或完整 HMI 解密内容）。

当前已发布版 RFB VNC Auth 不提供加密，`AuthenticatedEncrypted` 不可用；**不得**以“经过 Gateway WSS”代替最终 HMI 安全连接，更不能自动降级 RFB Insecure。Native 是未来需要实现和单独合格的设备端协议，受管 Gateway 不改变 V8 已批准的 Native 1.0。

## 2. Native 1.0 协议接口复用，不加新字段

Native 设备协议的 Wire 形状**唯一权威为 V8 附录 A**：

- **公共头：** `[type:u16, flags:u16, payload_length:u32]`，共 8 字节，网络大端；Native 1.0 `flags=0`，每条消息先限额再分配。
- **会话握手：** `HELLO(0x0001)` 28 字节正文 → `WELCOME(0x0002)` 25 字节正文；使用 `HYRP` magic、版本/能力/分辨率/Rect 预算；`WELCOME.auth_mode` 必须匹配受控部署的 `plain` 或 `tls-password` 配置。**该两个模式必须在监听前配置，禁止 STARTTLS 或自动降级。**
- **认证：** 仅在已受信 TLS 的 `tls-password` 通道传 `AUTH(0x0003)` 和 `AUTH_RESULT(0x0004)`；用户名 1..64 UTF-8 字节，密码 1..128 字节；不能把 VNC 8 字节共享口令当作 Native 用户身份或凭据库。
- **目标/画面：** `TARGET_LIST/STATUS/EVENT/SUBSCRIBE`，`SCREEN_INFO`，`FRAME_BEGIN/RECT/END`，`REQUEST_FULL`；受会话许可、Target/Scope 和 PolicyRevision 控制，全部沿用 V8 A.2/A.4。
- **操作授权：** `CONTROL_REQUEST(0x0050)`、`CONTROL_STATE(0x0051)`、`CONTROL_RELEASE(0x0052)`；`INPUT(0x0020)` 携带 V8 A.3 规定的 `target_id,scope_id,grant_epoch,generation,input_type`，**GrantEpoch 不是 bearer token**。
- **关闭/故障：** `ERROR(0x00FE)` 与 `CLOSE(0x00FF)` 使用 A.5 的编号/原因；TLS 证书或握手失败时不得发送普通 Native ERROR 试图降级。
- **上限：** V8 A.6 已批准可见目标最多 32；宽高 1..8192、总像素 ≤8,388,608、每帧矩形最多 4096、单 Rect 正文 64..256KiB 协商上限、标签 UTF-8 最多 128 字节。不得把 Gateway Control 02 文档中的消息/预算误加入 Native wire。

**注意：** Gateway 的 `RouteId`、`SiteId`、`OperatorId`、租户权限均不得静默增加到 Native 1.0 消息正文。设备端独立 Native 用户身份与 AccessSessionId 由 HMI TLS/Native 连接关联。如果将来确需把云端人类身份映射到设备端，应单独设计经批准的身份委托/签名证明与协议版本，不能因 Gateway 登录成功自动赋予 Grant。

## 3. 设备端逻辑模块（内部职责，不指定具体源码文件名）

| 逻辑模块 | 主责 | 错误处理 |
| --- | --- | --- |
| `DeploymentAdmission` | H-02a 在创建 listener 之前核验可信部署清单：应用身份、批准 Target、端点绑定、安全模式、初始只读、来源 | 缺失/重复/无效拒绝监听；RFB 既有默认 `0.0.0.0:5921` 配置值不变 |
| `NativeTlsListener` | 独立 Native TCP/TLS 入口；TLS1.3 安全配置、服务器身份证明和可用密码库 | TLS/证书不可用立即拒绝，不降级为 plain |
| `NativeAuthVerifier` | `tls-password` 用户密码验证、独立盐的 PBKDF2-HMAC-SHA256、常量时间比较、失败限额 | 错误次数/资源受限；不得回传账户存在与否差异 |
| `AccessSessionRegistry` | 每条认证设备连接关联 AccessSessionId、身份、Target Scope 和 PolicyRevision | 每次新的 TCP/TLS 连接全新会话，不复活旧 Grant |
| `AccessPolicyProvider` | 宿主按当前 TargetKey/Scope 发布原子策略，授权时限/可见范围/隐私 | 未知 Target 隐藏；丢失 Provider 失败关闭 |
| `NativeFramePublisher` | 从共享 FrameSource 取得经会话过滤的画面，维护 per-target generation/frame baseline | 不跨用户共享受限缓存；增量基准错误转 WAIT_FULL |
| `InputAdmission` | 输入到达前与 Qt 最终执行前两次复核 `RuntimeEpoch,AccessSessionId,TargetId,ScopeId,GrantEpoch,PolicyRevision` | 旧控制一律拒绝、释放 held 键/按钮/触点 |
| `SessionLifecycle` | 断开、HMI stop、Target 移除、政策修订、TLS 错误时释放受限资源 | 安全/现场清理优先于画面性能和重试 |

独立 Native 端点不改变现有 RFB Listener 的安全配置或行为；两个协议共享授权策略/FrameSource/Runtime 和 Scope 冲突规则，但会话对象、协议状态机、网络队列和认证各自独立。

## 4. 设备端时序与 Native 安全门禁

1. HMI 宿主提交受控部署授权；`DeploymentAdmission` 在打开 Native/RFB listener 前验证合法性。缺任一必要项失败，不创建部分监听。
2. Native TLS Listener 使用合法可信证书与服务器身份材料启动；工厂 Edge 并不为 HMI 创建新的 listener 或替代应用 Runtime。
3. 异地 HyRemoteView 经过透明 Relay 连接 HMI Native/TLS，**验证 HMI 身份证书/预置 pin/可信 CA 与本次登记 DeviceId 一致**。错误时从 TLS 层关闭，不继续 Native HELLO/AUTH，不允许 fallback RFB。
4. 客户端和 HMI 按 V8 附录 A 的顺序完成 HELLO/WELCOME、配置要求的独立 Native AUTH（若为 tls-password）/AUTH_RESULT、可见目标能力与权限判定。身份认证不能直接被解释为所有 Target 可查看。
5. `TARGET_LIST` 只能枚举对本 AccessSession 授权可见的 Target；`TARGET_SUBSCRIBE` 对应新增 Session ViewGrant 检查，按会话输出前过滤隐私区。
6. 控制申请经设备端宿主当前 Scope 和冲突范围判断；`CONTROL_STATE` 仅反馈设备已批准的当前 `GrantEpoch`，路由/云端的角色声明不能替代它。
7. 每条 `INPUT` 在网络接纳时核验授权，在 Qt GUI/Input mailbox 最终提交前再核验当前策略和 Epoch，防止排队期间宿主现场撤权后旧输入漏出。
8. 政策变化或重连更新 `PolicyRevision` / `generation`；必须按 V8 A.4 重新发送 SCREEN_INFO、FULL，然后才能合法 DELTA。旧策略帧和客户端缓存不得跨新的许可/Frame 基准继续展示。
9. 任何 Gateway/Edge 网络断开只让相应 TCP/TLS Session 结束；HMI 的现场 Qt 应用继续运行，后续新连接使用新的 AccessSessionId。不会自动重发报警确认、文件或其他业务操作。
10. Runtime stop/Target revoke/Control revoke 优先完成输入栅栏与 held input 释放，再完成网络通知；设备端最终拒绝事件可观察。

## 5. 安全材料与部署

Native TLS 的证书/私钥、密码 verifier（不是明文密码）和授权配置由 HMI 设备出厂/客户受控部署清单管理。证书下发可以走出厂可信预置、客户 CA 或现场可信录入，但**不能**由未受信 Gateway 自动发出“任意新 HMI 证书现在合法”的结果。首次信任建立、证书替换与废止必须有可追溯的客户操作和应用端核验；没有安全材料不得启动要求 tls-password 的安全模式。

设备端密码使用 V8 9.4 的 PBKDF2-HMAC-SHA256、每账户独立盐、常量时间比较及失败限额；具体迭代因子/认证耗时必须依据实际 HMI CPU 防 DoS 安全测试签收，不能拿商用 PC 默认值硬塞 ARM/EGLFS 设备。凭据/私钥禁止写入 CLI、日志、普通配置导出。若 OS 安全存储不受目标平台支持，必须采用经威胁审核的交付策略或拒绝该安全模式，不能落明文临时文件。

## 6. 帧时效、授权与性能

- 受管路线**不改变 HMI 的基础图像采集**。多个合法 Session 可复用基础帧来源，但每用户/Target Scope 的可见内容、遮罩、授权、缓存和发送队列必须隔离。
- Native 每目标单独记录 generation、frame_id、policy_revision；DELTA 只能基于该 Viewer 已完整接收并提交的合法 Full/Delta 基线，发现不一致则 Client WAIT_FULL + REQUEST_FULL，拒绝继续累计错误像素。
- Gateway/工厂 Edge 只处理字节，**不能**替设备端执行“丢掉上一个增量消息”的策略；慢 Viewer 的合帧/过时帧跳过由 HMI FramePublisher 在编码/加密**之前**做，并正确重建 FULL 基准。
- 带宽/CPU 达不到产品目标时应优先修复真实 HMI 根因（#609 CPU/帧龄专项），不准通过“少发新画面、客户看到旧帧”伪造性能达标。
- 本文件不引入 Native 1.0 新编码、新功能或 Native/Standard RFB 私下耦合。

## 7. 设备端实施与验收 Gate

| Gate | 证明条件 | 失败结论 |
| --- | --- | --- |
| HMI-01 Deployment | 无有效部署授权时 Native/RFB 均不得监听；有效时只开启配置指定端点 | 虚假监听/弱模式回退 FAIL |
| HMI-02 TLS E2E | 真正异地 View 透过 opaque Gateway/Edge 验证 HMI 设备身份，未知/更换证书不能访问 | 缺 E2E 不能称 Managed 安全 |
| HMI-03 Native Wire | V8 附录 A 全部 21 类型合法/异常顺序、长度、版本、授权状态有独立双端验证 | Length 对也不等于协议 PASS |
| HMI-04 Auth/Policy | Native 用户认证、ViewGrant、ControlGrant、Scope 冲突、现场撤权与 RFB 并行独立 | 云权限替代设备权限 FAIL |
| HMI-05 Frame | 遮罩/Target policy 变更后旧帧失效，Full/Delta/generation 对齐 | 旧敏感画面/坏增量 FAIL |
| HMI-06 Input | 输入队列两次校验 Epoch、断线/撤权 held 状态释放 | 撤权后可执行旧输入 FAIL |
| HMI-07 Resource | 真实 Windows + 目标 Qt/ARM/EGLFS 按适用支持单元观察 CPU/内存/帧龄、无二次采集 | 未实机即 NOT_QUALIFIED |
| HMI-08 Deploy/Compat | 禁用 Native 不额外携带其安全库；四接入和已有 RFB Direct 不回退 | 只过 Windows 不可宣称 Linux/ARM PASS |

**当前真实状态：** `HMI_NATIVE_E2E=NOT_IMPLEMENTED`，`MANAGED_PROTOCOL=DESIGN_ONLY`，`G4_HMI_IMPL_GATE=NOT_YET_APPROVED`；具体产品代码仍须在正式设计、技术依赖、代码审查与真实设备 Gate 通过后开发。

## 8. 来源与一致性

- 正式 V8 第 5.1/5.2、8.1–8.7、9.1–9.7、10.6、16.6、附录 A.1–A.6、B.1/B.2；正式 V15 第 5.3、8.6/8.7、10.1/10.2、11.4、13.8/13.9。
- 既有实际安全说明：`docs/security.md`；已有条件 RFB 认证不提供加密，不能用同端口假定 Native 安全。
- 跨仓 HyRemoteView #5 G3、#7 加密研究、HyRemote #246 服务端安全/诊断契约、#190 可选 Gateway 产品计划、#609 性能专项。

**不冻结新的公网端口、证书发行服务、密码默认值或新的 SDK ABI。** 这些若需变更 V15/V8 要经受控变更审批；普通内部实现待 HMI-01..08 验证后选择。
