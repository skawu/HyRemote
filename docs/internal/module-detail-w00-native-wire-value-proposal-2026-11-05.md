# W00｜Native 1.0 未定义数值域的**待批准**精确技术补充与安全档位取舍（2026-11-05）

> **性质：`PROPOSAL_ONLY / NOT_NORMATIVE / NOT_WIRE_FROZEN`。** 本文明确区分：① **V8 已有** type/字段宽度/长度/某些常量；② **Cloud Owner 提议**的缺失 enum/bit/字节语义；③ 仍需 Human/受控上位版本决定的安全语义。**下文数字都是“候选”，不是从 V8 原文提取出来的常量，不能被 Host/View 代码据此自行实现**。唯一权威仍是已批准 `HYR-ARCH-001/V8` 附录 A/B；源 Word 经 Human 确认正文等效的送审稿 SHA 核验见 [来源审计](module-detail-source-audit-v15-v8-2026-11-05.md)。

## 1. 确实不需要变更、必须原样保留的 V8 规范

1. 公共头 `[type:u16,flags:u16,payload_length:u32]`、网络大端、8 字节、v1.0 `flags=0`。
2. 21 种消息 `0x0001..0x00FF` 已有各自离散 type ID、字段顺序及表 23 的全部长度公式（不能新增“旁侧诊断第三协议”）；5 个 INPUT 子型数字 `1..5`、13 个 ERROR 数字 `1..13`、CLOSE 数字 `1..5` 已获批准。
3. `INPUT` 共同 29 字节前缀、`FRAME_RECT 37+4*w*h`，FULL/DELTA 基线、无逐帧 ACK；32 可见目标、128 字节标签、8192 单边、8388608 像素、RECT 协商上界等以 V8 原文约束为准。
4. 会话/设备/Scope/Grant/GrantEpoch 及 Qt 二次栅栏以设备端宿主现行授权为准；RoutePermit 与 Gateway 不提供 HMI 的 View/ControlGrant。
5. 双协议 RFB 3.8/Native 1.0 独立；标准 RFB 无加密/个人身份时的合法范围和 H-RFB=A 均不被本技术补充修改。

## 2. 候选数值提案（提案 P-A，仅供评审；不构成有效 wire）

| 差距 | 字段 | Cloud Owner **拟议数值**（V8 未列出，非已有批准） | 考量、兼容前提及尚需裁决之处 |
| --- | --- | --- | --- |
| WIRE-V01 | `HELLO/WELCOME.caps:u32` | **v1.0 初版仅允许 `0x00000000`**，所有表23 基础消息能力为 mandatory baseline；非零/未知位拒绝 `UNSUPPORTED_CAPABILITY=6` | 不抢占未来 bit 号、不会出现 Host/Viewer 猜位差异；代价是初版不协商其他可选 capability，增补能力必须在单一版本化表内先完成审批 |
| WIRE-V02 | `WELCOME.auth_mode:u8` | **候选** `0=plain`、`1=tls-password`，其它拒绝 INVALID_FIELD | 数值不是 V8 原文事实；两模式的传输保护和认证前提另见 SOURCE_TENSION_01，未批准前 **均不得当已绑定的 enum** |
| WIRE-V03 | `SCREEN_INFO.pixel_format:u8` | **候选** `1=RGBA8888`，其它 UNSUPPORTED_FORMAT=11 | Wire 上每像素严格 4 字节 R,G,B,A，A=255 为不透明设备画面；不得复用 Qt 机器端序相关的 BGRA/Premultiplied 字节内存直接对发 |
| WIRE-V03 | `SCREEN_INFO.input_hint:u8` | **候选** `0=NO_HINT`，其它 INVALID_FIELD | 输入能力由已授权 Target/Grant/协议能力决定，hint 不能授予 Touch；Native 1.0 不加入未经批准的原生多点触控字段 |
| WIRE-V03 | `FRAME_RECT.encoding:u8` | **候选** `1=RAW_RGBA8888`，其它 UNSUPPORTED_FORMAT=11 | 与原文 37+4wh 的固定原始像素字节数相容，不允许凭本地 QImage 格式推断未知 encoding |
| WIRE-V04 | `FRAME_BEGIN.update_type:u8` | **候选** `0=FULL`、`1=INCREMENTAL`，其它 INVALID_FIELD；FULL 时 `base_frame_id=0` | 每 Target frame generation/基线仍遵 V8 A.4；FULL 完整覆盖无重叠，DELTA 不得引用旧代次 |
| WIRE-V04 | `TARGET_LIST.flags:u8` 与 `TARGET_EVENT.flags:u8` | **候选 v1.0 全部为 `0`**，其它 INVALID_FIELD | 仅列表可见性和权限由 HMI 当前 Session 判定；不擅用 flags 暗示“可控制”或身份权限 |
| WIRE-V04 | `TARGET_EVENT.op:u8` | **候选** `1=ADD`、`2=UPDATE`、`3=REMOVE`；REMOVE `label_len=0` | 报文有原文字段，操作含义可见于 §8.3；具体值/删除后的 TargetId 和缓存清理属于待审协议补充 |
| WIRE-V05 | `TARGET_SUBSCRIBE.subscribe:u8` | **候选** `0=UNSUBSCRIBE`、`1=SUBSCRIBE`，其它 INVALID_FIELD | 取消后不得恢复旧 Grant/历史画面，未授权必须拒绝而非订阅成功 |
| WIRE-V05 | `TARGET_STATUS.status:u8` | **候选** `1=SUBSCRIBED`、`2=UNSUBSCRIBED`、`3=DENIED`、`4=REMOVED` | 源文明确四种状态含义但没有数值；不因客户端本地状态认为 HMI 已批准 |
| WIRE-V05 | `CONTROL_STATE.status:u8` | **候选** `1=PENDING`、`2=GRANTED`、`3=DENIED`、`4=REVOKED`、`5=EXPIRED`、`6=RELEASED` | 对应 V8 §5.3/Table7 的状态语义，但数字是**新提议**；仅 GRANTED 且当前 GrantEpoch/Scope/Target/Policy 真实生效才允许输入 |
| WIRE-V05 | `AUTH_RESULT.success:u8` | **候选** `0=FAILED`、`1=SUCCEEDED` | AUTH_RESULT 不代替 HMI ViewGrant；未知数值一律失败，不区分账户是否存在 |
| WIRE-V06 | `INPUT.POINTER_BUTTON.button:u8` | **候选** `1=LEFT`、`2=MIDDLE`、`3=RIGHT`、`4=X1`、`5=X2` | 仅已有 mouse pointer-button 字段，其他未知值拒绝；不表示新增硬件输入能力 |
| WIRE-V06 | `INPUT.POINTER_BUTTON.pressed:u8`、`INPUT.KEY.pressed:u8` | **候选** `0=RELEASE`、`1=PRESS`，其它 INVALID_FIELD | held 只有 Accepted 才更新，保护释放必须在拒绝新输入与现场接管时仍能执行 |
| WIRE-V06 | `INPUT.KEY.modifiers:u16` | **候选** bit0 SHIFT、bit1 CTRL、bit2 ALT、bit3 META、bit4 CAPS、bit5 NUM；bit6..15 必须为 0 | 仅 V8 表25 已存在的六类 modifier，**位序此前未明确**；不借此支持系统安全组合键 |
| WIRE-V06 | `INPUT.KEY.key:u16` | 表25 已规定普通键范围和特殊码段；缺少逐值绑定时的候选分配见下表 | 不直接 cast `Qt::Key`，不同 Qt Kits、IME 与 keypad 需一致；补值后才能产生 golden vectors |

### 2.1 网络键码映射候选（不增加表 25 的任何合法区间）

| 现有批准区间 | **候选的逐值解释** | 限制 |
| --- | --- | --- |
| `0x0101..0x010E`（V8 已保留“Enter/Escape/Tab/方向等”14 个码） | 0101 ENTER、0102 ESCAPE、0103 TAB、0104 BACKSPACE、0105 DELETE、0106 INSERT、0107 HOME、0108 END、0109 PAGE_UP、010A PAGE_DOWN、010B LEFT、010C RIGHT、010D UP、010E DOWN | 以上 **逐值是候选**；大小写/方向键与 Qt::Key 转换必须另做精确正负向验证 |
| `0x0201..0x0206`（已有六修饰键类别） | 0201 SHIFT、0202 CTRL、0203 ALT、0204 META、0205 CAPS_LOCK、0206 NUM_LOCK | 数字到名称的对应关系是从表25 的排列归纳，冻结前仍要在变更表显式签收 |
| `0x0400..0x0410`（17 个数字小键盘码） | 0400..0409 KP_0..KP_9；040A DECIMAL、040B ENTER、040C ADD、040D SUBTRACT、040E MULTIPLY、040F DIVIDE、0410 EQUAL | 来源只定义整个段并要求与主键区区分；本表 **逐码解释为候选**，不是正式 V8 已签收数值 |

> 已被 V8 明确分配的 `0x0020`、主数字 `0x0030..0x0039`、字母 `0x0041..0x005A`、功能键 `0x0301..0x030C` 数值**不变**。

## 3. SOURCE_TENSION_01 — Native `plain` 与 §8.2 “TLS-first”含义的受控选择

**真实来源的两个断言不能偷偷合并或抹掉**：

- V8 §8.2 明文：Native 1.0 在预设模式的 **TLS 握手与服务端身份核验之后**执行 HELLO/WELCOME；使用 `tls-password` 时才随后 AUTH。
- V8 A.1 明文：Native TCP 端点有预先配置的 `plain` 或 `tls-password` 模式，禁止 STARTTLS 和自动降级。A.1 **没有显式写“plain 不加密”**，也**没有显式写“plain 必须 TLS”**，从文字不能凭直觉决定。

| 方案 | 解释/最小变更 | 安全和兼容后果 | 状态 |
| --- | --- | --- | --- |
| **A（推荐的最保守解释候选）** | **两种模式都必须先完成 HMI DeviceTLS**；plain = TLS 且不进行密码 AUTH，但无独立用户身份时 HMI Host 默认不给目标查看/控制（除非受控部署策略能证明准入）；tls-password = TLS + 独立 AUTH。Managed **只允许满足真实用户身份及授权证明**的档位 | 保留 §8.2 TLS-first 字面及 DeviceId E2E，减少明文误用；但若历史预期“plain 即明文 Native”，这是行为变更，需要 Human 明示确认；安全存储、首次信任资格仍须真实验证 | `PROPOSED_NOT_APPROVED` |
| B（仅经 Human 明示更改上位安全语义） | plain = 不加密 Native，仅用于经过可信封闭局域网的明确部署；tls-password = TLS + AUTH；须在 V8 §8.2 明确列出 plain 例外 | 容易被误用于不可信网络/冒充设备身份；必须强制封闭网络和 UI 风险声明；仍不能受管公网安全声称 | `ALTERNATIVE_NOT_APPROVED` |
| C（收紧档位） | 先禁用 plain（文档仍保留名称但当前支持配置仅 tls-password），所有 Native 支持档位 TLS + AUTH | 资格更简单但可能减少已批准 Direct 场景，应核对产品权益和预期；不得由实现者静默减配 | `ALTERNATIVE_NOT_APPROVED` |

**暂时可安全推进的非取舍工作**：为所有 TLS 要求档位设计失败关闭状态机、证书与 TrustStore 来源、网络中断/拒绝场景；任何需要假定 plain 真实安全性质的正向产品用例标 `BLOCKED_BY_SOURCE_TENSION_01`，既不发明实现，也不跳过 Native wire 审签。

## 4. 协议兼容、错误与测试影响（设计验收，不是测试执行）

- 本补充不改变 21 message 的 type 编号、A.2 的任何字段位置/长度、A.3 INPUT subtype 数字或 A.5 ERROR/CLOSE 已有数字。若 Human 批准候选数值，其效力只能通过单一 V8 A/B 的**受控增补**正式生效；不能在 D1 与 View 两个平行表独立维护。
- Host/Viewer 双端必须从同一已批准常量表生成 golden vectors；在批准之前，所有有待定 enum 的正向样例为 `PENDING`，不能写 `PASS`；已完全定义的头部/长度/错误码负向向量可独立设计。
- 全部错误信息优先使用 V8 已定义的 `BAD_HEADER=1`、`UNSUPPORTED_VERSION=2`、`BAD_ORDER=3`、`BAD_LENGTH=4`、`INVALID_FIELD=5`、`UNSUPPORTED_CAPABILITY=6`、`UNSUPPORTED_FORMAT=11`、`POLICY_DENIED=13`；**不能因文中建议而新设 ERROR 编号**。
- 未协商或未知 enum/bit 应拒绝，不能将未知值默认为 FULL、CONTROL_GRANTED、authenticated、Raw RGBA、相同 Scope、允许重连或新权限。
- 如果未来需要保留与**已经出厂的真实 Native 客户端**互通，不能静默将未定义值作为相同版本的 wire 变化；发布前应对真实 deployed version/field values 进行对照并拟迁移/版本协商。当前现有证据是 Native 完整 E2E **未实现、未验收**，不能假称已有 Native 用户生态已兼容。

## 5. 审批和设计冻结门禁（只请求一次受控规范选择）

将该提案作为一份 **`NATIVE-V8-WIRE-DELTA-01` 候选附录**供后续完整设计评审：审批应同时明确 **是否接受第2节的数值表** 和 **SOURCE_TENSION_01=A/B/C**。若任一字段需要修改，明确逐字段差量即可，不让 Human 重审 H-T01/H-DUAL/H-REVOKE/H-EDGE/H-RFB 等五项已批准决策。

在取得**上位受控规范批准**前：`WIRE_VALUE_PROPOSAL_READY_FOR_REVIEW=YES`; `NATIVE_V8_WIRE_DELTA_APPROVED=NO`; `SOURCE_TENSION_01=OPEN`; `WIRE_FROZEN=NO`; `GATE_D=NO`; `GATE_I=NO`; `GATEWAY_190=NOT_ACTIVATED`。本方案不能被转发为产品研发工单或发布声明。
