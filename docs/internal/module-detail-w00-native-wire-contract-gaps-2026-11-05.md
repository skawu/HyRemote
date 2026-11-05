# W00｜Native 1.0 A/B 独立互操作合同与 WIRE-V01..V07 收口准则（草案）

> **唯一协议规范**：Human 确认正文等效的 `HYR-ARCH-001/V8` 附录 A.1–A.6、B.1–B.3；本文件只建立“已定义内容/未定义取值/互操作向量”的审核目录，**不另立 Native 版本或 wire 字节规范**。源摘要及 21/21 索引见 [W00 上位原文审计](module-detail-source-audit-v15-v8-2026-11-05.md)。当前所有向量都未编译、未运行，且不同于设备端 Native E2E 已实现。

## 1. 必须固定、禁止随意重写的上位事实

- 每条报文通用 **8-byte big-endian** `type:u16,flags:u16,payload_length:u32`；V1.0 `flags=0`，按真实正文长度预验，异常闭合。
- V8 附录 A.2 表23 已列 **21 个消息 ID、字段序和长度公式**；附录 A.3 的 `INPUT` 五种子型 numeric **1..5** 已给出；附录 A.5 表26 的 **13 个错误码**及 `CLOSE.reason=1..5` 有确切数值；不要将这些已定义的常量误列入待审批。
- 绝不将 Native 解释为 RFB 的补充诊断通道，不能使用 RFB 像素流实现 Native。Managed Edge/Gateway **只转发经过批准的 HMI 端点的完整字节**，中继不解码 HMI wire，不增补消息。
- 运行时任何 View/ControlGrant 都由 HMI 当前会话、Target、Scope、PolicyRevision、GrantEpoch 判定；权限过期时设备端与 Qt 最终投递前双重栅栏。**GrantEpoch 是防旧消息代次，不是 bearer token**。不回退明文或假个人身份。

## 2. 七组未冻结精确问题和设计处置类型

| ID | 上位字段 | 已有确定性 | 未完备之处 | 设计收口原则/当前状态 |
| --- | --- | --- | --- | --- |
| WIRE-V01 | HELLO/WELCOME `caps:u32` | 固定字段宽度、两端协商、主版本与资源限额已知 | bit 号、能力交集、未知位/超集处理 | 从已有上位文字查找；若没有，提出独立“数值定义候选”，需规范变更审签，**不发明新 cap 值** |
| WIRE-V02 | `WELCOME.auth_mode:u8`；§8.2 TLS 顺序与 A.1 两模式 | 正文同时出现 `plain`、`tls-password`、TLS-first 握手的描述；Managed 不能用无 HMI 身份的弱通道 | 两档准确 numeric 值，Direct plain 是否绕过 TLS 前置、Native 可接受链路组合、拒绝和错误顺序 | **SOURCE_TENSION_01**：§8.2 的无条件 TLS-first 与 A.1 `plain` 部署语义存在适用范围未解释清楚；不得由客户端“看到 plain 就连上”私下解释；冻结前审核是否需改正式 V8 |
| WIRE-V03 | `pixel_format:u8`、`input_hint:u8`、`encoding:u8` | FRAME_RECT 固定 4 bytes/pixel、矩形范围及长度公式可核 | 枚举数值、RGB/A 顺序、alpha、像素色域与 Qt 内部图像映射 | 安全默认 **未知值拒绝**，不能用客户端本机格式猜编码；如改变 wire 规范，走 Human 受控差量 |
| WIRE-V04 | `FRAME_BEGIN.update_type:u8`、`TARGET_LIST.flags:u8`、`TARGET_EVENT.op/flags:u8` | FULL/增量、Target add/remove/change 语义有正文；Type 已定 | 各具体数值、flags 有效位/互斥和未知位 | 分离内容/目标状态机与精确数值定义，暂不签名放行任何“完整互操作” |
| WIRE-V05 | `TARGET_SUBSCRIBE.subscribe:u8`、`TARGET_STATUS.status:u8`、`CONTROL_STATE.status:u8`、`AUTH_RESULT.success:u8` | 订阅、拒绝、批准、撤销、到期等概念来自 §5/Table7，字段宽度已知 | u8 数字、未知值行为、失败后的单向转换和安全展示 | 无已批准数值表不得擅自用 `enum` 的 C++ 编译顺序；负例须覆盖乱序/重复/迟到状态 |
| WIRE-V06 | INPUT `button:u8,pressed:u8,modifiers:u16,key:u16` | 5 个 subtype ID、29-byte 输入公共前缀，表25 网络键码**部分**值域已明确 | 鼠标按钮、按压布尔、modifier bit assignment、特殊键 0x0101..0x010E 名称→单值绑定/小键盘完整码表 | 已定义的六修饰语义与键区间不可改；具体数值未列不得从 Qt::Key 转型或冒充交叉平台兼容 |
| WIRE-V07 | A.2–A.6 + B.1/B.2 全状态 | 21 type/len、13 errors、5 close reasons、各上界等可作为正/负例输入 | 字节级 golden vectors、错误+close 时序、parser fuzz、独立 Host/Viewer 真互通 | 本文第3–4节仅列**待执行向量规格**；需在数值域审签后计算确定性 bytes 和互操作结果，不可写 PASS |

**2026-11-05 设计补充（未批准）**：已将所有缺失数值的候选、来源张力以及 V8 变更影响集中于 [NATIVE-V8-WIRE-DELTA-01 候选](module-detail-w00-native-wire-value-proposal-2026-11-05.md)。该文件的数字**不是 V8 已批准常量**，当前正向互操作向量依旧 BLOCKED；Host/Viewer 不得基于提案各自实现。

**流程建议（不修改 V8）**：先抽取全部已定义表/章节 → 写出“原文有/缺值”审计证据 → 尝试不用新增值就形成确定性正/负例 → 缺值项提出单一受控 `Wire Value Allocation / Compatibility Appendix` 变更建议及迁移风险 → 仅 Human 审批后改变正式协议承诺与 Host/Viewer 解析器。五项已批准 A 产品取舍保持生效；只有**上位 wire 取值或安全档位实质变更**才走新 Human Gate。

## 3. 21 类消息逐型的正/负向设计向量计划（未运行）

| V8 A.2 type | 原文已明确的字段/长度 | 缺少绑定或要复核的部分 | 至少一种必测拒绝/恢复行为 | 证据状态 |
| --- | --- | --- | --- | --- |
| `0x0001` HELLO | 固定 28；`HYRP`、major/minor、caps/max_* | cap bit、未知 bit 协商与错误路径 | 错误 magic/major/len/caps 越界 | `DESIGN_VECTOR_NOT_EXECUTED` |
| `0x0002` WELCOME | 固定 25；含 `auth_mode:u8` | plain/tls-password 实际数值与能力交集 | 错误模式/未匹配 TLS/版本协商 | `DESIGN_VECTOR_NOT_EXECUTED` |
| `0x0003` AUTH | 变长 `4+user+password≤196`；用户名 1–64，密码 1–128 字节 | 仅 TLS 档位及密码验证状态机的正例 | 无 TLS、长度 0/超限、账户枚举 | `DESIGN_VECTOR_NOT_EXECUTED` |
| `0x0004` AUTH_RESULT | 固定 1；`success:u8` | success/failed 数值与后续可见 Target 准入 | 未知值、失败后仍枚举 | `DESIGN_VECTOR_NOT_EXECUTED` |
| `0x0005` SCREEN_INFO | 固定 30；含 target/generation/dim/pixel_format/input_hint/policy_revision | `pixel_format`、`input_hint` 数值与像素语义 | 未知格式、超尺寸、旧 epoch | `DESIGN_VECTOR_NOT_EXECUTED` |
| `0x0010` FRAME_BEGIN | 固定 31；frame_id/base_frame_id/update_type/rect_count | `update_type` FULL/DELTA 数值 | 错误基线、0/超限矩形、交叉帧 | `DESIGN_VECTOR_NOT_EXECUTED` |
| `0x0011` FRAME_RECT | 变长 `37+4*w*h`；字段含 encoding:u8 | encoding 取值，RGBA 通道顺序/alpha 规则 | 乘法溢出、出界、Rect 超额、截断 | `DESIGN_VECTOR_NOT_EXECUTED` |
| `0x0012` FRAME_END | 固定 20；target/generation/frame_id | 无新增 enum | 无 BEGIN、帧 ID 不符、字节交错 | `DESIGN_VECTOR_NOT_EXECUTED` |
| `0x0013` REQUEST_FULL | 固定 20；target/generation/last_valid_frame_id | 无新增 enum；WAIT_FULL 交互需签收 | 旧代次、重复/并发重同步 | `DESIGN_VECTOR_NOT_EXECUTED` |
| `0x0020` INPUT | 前缀 29 + subtype；type 1..5 已编号 | button/pressed/modifier 等域与 keyboard keycode 补全 | 越界坐标、失效 Grant、未知 subtype/held | `DESIGN_VECTOR_NOT_EXECUTED` |
| `0x0030` PING | 固定 8 nonce:u64 | 无新增 enum | 超长/短帧、非法 state、未认证滥发 | `DESIGN_VECTOR_NOT_EXECUTED` |
| `0x0031` PONG | 固定 8 nonce:u64 | 无新增 enum | nonce 不匹配、伪 RTT/资源消耗 | `DESIGN_VECTOR_NOT_EXECUTED` |
| `0x0040` TARGET_LIST | 变长 `10+Σ(19+n)≤4714`；最多 32/label 128 bytes | flags 位及候选 Target 可见性语义 | 非授权枚举、重复 Target、非法 UTF-8 | `DESIGN_VECTOR_NOT_EXECUTED` |
| `0x0041` TARGET_SUBSCRIBE | 固定 9；subscribe:u8 | 布尔值 0/1 是否明定、取消后状态 | 未知值、旧目标/越权订阅 | `DESIGN_VECTOR_NOT_EXECUTED` |
| `0x0042` TARGET_STATUS | 固定 17；status:u8 | 目标订阅状态各数值/非法值 | 旧 target/status、失权后仍发屏 | `DESIGN_VECTOR_NOT_EXECUTED` |
| `0x0043` TARGET_EVENT | 变长 `28+n`；op/flags:u8 | 目标增删/变更 op 与 flags 数值 | 删除/更新乱序、label 越界、策略旧版 | `DESIGN_VECTOR_NOT_EXECUTED` |
| `0x0050` CONTROL_REQUEST | 固定 24；request/target/scope 三个 u64 | 无新增 enum；旧 requestId 语义 | 等待期输入、重放/交叉 Scope | `DESIGN_VECTOR_NOT_EXECUTED` |
| `0x0051` CONTROL_STATE | 固定 45；status:u8+grant_epoch/lease_ms/policy_revision | 批准/拒绝/撤销/到期/释放的 wire 状态码 | 过期状态、旧 Grant 恢复/权限混淆 | `DESIGN_VECTOR_NOT_EXECUTED` |
| `0x0052` CONTROL_RELEASE | 固定 24；target/scope/grant_epoch | 无新增 enum；释放幂等界限 | 重复释放/错误 Scope/旧 epoch | `DESIGN_VECTOR_NOT_EXECUTED` |
| `0x00FE` ERROR | 变长 `4+n≤1028`；13 error_code 已编号 | 未知 error detail/消息状态的精确断链策略 | 非法 error code、过长/敏感 detail | `DESIGN_VECTOR_NOT_EXECUTED` |
| `0x00FF` CLOSE | 固定 2；1–5 原因已编号 | 无新增 enum | 未知 reason/重复关闭/截断 | `DESIGN_VECTOR_NOT_EXECUTED` |

**语义检查补充**：`FRAME_BEGIN→RECT×N→END` 不得与其他帧/消息交错，FULL 完整无重叠覆盖，DELTA 只引用当前有效已完整应用的 base；错误基线进入 `WAIT_FULL` 并 `REQUEST_FULL`。协议无逐帧 ACK，TCP 发送成功不能表示客户端已经显示。与同一 TCP 字节流的边界守护一起用旧 policy epoch 限制输出，不能在代理层任意删截字节。

## 4. 首批**不依赖缺失枚举**的确定性负例（设计待实现）

| TestSpec | 已知数值/边界 | 预期安全结果 | 当前 |
| --- | --- | --- | --- |
| `NV-01` | 通用头 `flags=0x0001` | 按 A.1 的 v1.0 flags=0 限制拒绝该消息；不接受或解析正文内容 | `NOT_RUN` |
| `NV-02` | `type=0x0001`, HELLO 正文 27/29 字节（应 28） | 返回适用既有错误/关闭，且不触发认证/资源分配 | `NOT_RUN` |
| `NV-03` | HELLO 魔数非 `HYRP`、主版本 != 1 | 拒绝握手，不能以旧单目标 wire 兜底 | `NOT_RUN` |
| `NV-04` | `0x0011 FRAME_RECT` 的声明长度与 `37+4×w×h` 不符、整型乘溢或越界 | 在分配之前拒绝；无半帧应用 | `NOT_RUN` |
| `NV-05` | `0x0040 TARGET_LIST` 数量 33、label 129 bytes、总正文 >4714 | 防溢出、不可泄漏未经授权的 Target 名 | `NOT_RUN` |
| `NV-06` | `0x0020 INPUT` subtype=0、6 或旧 GrantEpoch | 分别按既有 INVALID_FIELD/策略拒绝行为不更新 held/Qt 输入 | `NOT_RUN` |
| `NV-07` | 未发有效 BEGIN 就来 `FRAME_END`，或 generation/frame_id 不匹配 | 拒绝无效帧事务、恢复到合法 WAIT_FULL/新连接 | `NOT_RUN` |
| `NV-08` | 两设备/双协议/一 Scope 并发申请控制，撤权后迟到输入在 Qt 邮箱排队 | 设备端一人独占、旧输入最终 Qt 栅栏拒绝、残留 held 释放 | `NOT_RUN` |
| `NV-09` | Native TLS 失败或 DeviceId/pin 不符，经 Managed 中继出站 | **零 HYRP 应用数据**、无无保护路径回退，不把 Gateway RoutePermit 当 ViewGrant | `NOT_RUN` |
| `NV-10` | Native 认证成功但 Target/Scope 未授权 | `TARGET_LIST` 不泄露；输入被拒绝、可信错误归因 | `NOT_RUN` |
| `NV-11` | 一个慢 View 卡住，另一会话已撤销敏感目标 | HMI 原始采集可共享，但衍生敏感帧按会话隔离、网络发队有界；Edge/Gateway 不丢加密字节 | `NOT_RUN` |
| `NV-12` | 进程关闭、目标消失、同名新 Target 或证书轮换、策略旧回调 | RuntimeEpoch/TargetId/GrantEpoch 清理，重连先核验，绝不旧会话自动复权 | `NOT_RUN` |

**数值约束优先级**：`ERROR.error_code=1..13` 及 `CLOSE.reason=1..5` 是 V8 表26/A.5 的真实现有取值；`INPUT.input_type=1..5` 是表24 的真实取值。**其余 u8/bit 不因本文展示一个测试而分配新数值**。Native 直接/受管中涉及 TLS-first/plain 适用范围的测试需等待 SOURCE_TENSION_01 正式消歧，不能先测一版实现再倒推设计。

## 5. 与其他模块的收口消费接口

- `F-05 Native`：只按 V8 A/B 锁消息 ID、字节顺序/长度与已知数字；对未定义模式值 `fail closed`，提供设计时明示 `WIRE_BINDING_PENDING`，不能交出“已实现 Native 全协议”的假资格。
- `F-06 Host Policy`：`ControlGrant/PolicySnapshot` 内部状态与 wire status 的可观察映射必须经下一轮逐值签收；业务执行结果仍由宿主决定。
- `V-01 Native client`：不按自己的局部假设实现与 F-05 不同的枚举；DeviceId、Trust 和 SitePermit 三层不混同。
- `D3 Edge/Gateway`：无需识别 Native A.2 类型即可有序 FIFO；获准路由端点绑定，不修改协议字节，不为获得性能任意丢字节。
- `D4 X-02 质量`：把 `NV-01..NV-12` 当待执行场景规格，并要求 Host/Viewer 独立实现的 exact-SHA 与原生 Windows/Linux/Qt Kit 证据；PoC 与纸面通过不算 PASS。

**结果**：`WIRE_SOURCE_TYPE_ID_FIELD_LEN=REVIEWED`; `KNOWN_NUMERICS=DOCUMENTED_ONLY`; `MISSING_NUMERICS=WIRE-V01..V06`; `TEST_VECTORS=WIRE-V07_DESIGN_ONLY`; `SOURCE_TENSION_01=OPEN`; `WIRE_FROZEN=NO`; `GATE_D=NO`; `GATE_I=NO`.
