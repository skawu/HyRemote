# NATIVE-V8-WIRE-DELTA-01｜Native 1.0 已批准数值绑定与 TLS-first 档位补充（2026-11-05）

> **规范状态：`HUMAN_APPROVED_A / APPLIES_TO_V8_ANNEX_A_B / SOURCE_NORMATIVE_ADDENDUM`。** Human 明确批准 `NATIVE-V8-WIRE-DELTA-01=P-A; SOURCE_TENSION_01=A`，证据：[总控 #843 审批记录](https://github.com/skawu/HyRemote/issues/843#issuecomment-6648340105)。本文件是在原 `HYR-ARCH-001/V8` 正文和附录 A/B **已存在但未分配具体值的字段**上的受控补充；不是一套平行 Native 协议，也没有改变批准版 Word 的二进制内容。与 V8 原始字段/限额/消息顺序冲突时必须停止并报告受控变更，**不得私自改 V8**。
>
> 来源基线：已获 Human 正文等效确认的 `HyRemote 产品架构文档_送审稿_V8.docx`，原送审稿 SHA-256 `3d89a90559ae62852538e919b7f0ad2188f54806ef13d806a095474dccce5824`；其与已批准正式版**正文内容相同**，但正式版 DOCX 二进制 SHA 不同。本补充保留 [原 P-A 候选及备选影响](module-detail-w00-native-wire-value-proposal-2026-11-05.md) 的 Git 历史，以下表格才是**审批后单一值源**。

## 1. 保持不变的原 V8 明确规定

- 公共头固定 `type:u16, flags:u16, payload_length:u32`，共 **8 字节**，无结构填充、网络大端；Native 1.0 `flags=0`，读取正文/内存分配前验证长度。
- 附录 A.2 原表：**21 个 message type**及各自方向、字段顺序、长度公式不变。A.3：`INPUT.input_type = 1 POINTER_MOVE, 2 POINTER_BUTTON, 3 POINTER_SCROLL, 4 KEY, 5 COMMITTED_TEXT` 不变；A.5：13 种 `ERROR.error_code=1..13`、`CLOSE.reason=1..5` 不变。
- Frame baseline、Target/Scope/GrantEpoch、设备权限控制、每 Target 的 `generation/frame_id`，FULL 完整无重叠、错误 DELTA 后 WAIT_FULL/REQUEST_FULL，以及资源限额，全部继续遵守 V8。
- Native 与标准 RFB3.8 **各为完整独立的屏幕+输入协议**；不加第三强制诊断监听、不让 Native 依赖 RFB。HMI 的设备身份、ViewGrant/ControlGrant/现场授权依旧为唯一设备端权威。

## 2. 已批准的原 P-A 精确值域（新增绑定，不追溯冒称 V8 原有）

| 字段（V8 已存在） | Human 批准的线缆值及含义 | 未知/不支持值处理 |
| --- | --- | --- |
| `HELLO.caps:u32`、`WELCOME.caps:u32` | Native 1.0 基线 **仅 `0x00000000`**；附录 A.2 既有基本能力为必备 | 任何非零或未知 bit 拒绝，`UNSUPPORTED_CAPABILITY=6`；未来扩展必须受控改表 |
| `WELCOME.auth_mode:u8` | **`0=plain`、`1=tls-password`**；两种都在 **DeviceTLS 后**协商 | 其他值 `INVALID_FIELD=5`，不以 unknown 当默认/降级 |
| `SCREEN_INFO.pixel_format:u8` | **`1=RGBA8888`**；像素线缆顺序 **R,G,B,A**，每像素 4 字节，**A=255**（不透明画面） | 其他值 `UNSUPPORTED_FORMAT=11`，不直接发送 Qt BGRA / premultiplied 内存 |
| `SCREEN_INFO.input_hint:u8` | **`0=NO_HINT`**；不是权限授予 | 其他值 `INVALID_FIELD=5`；不可据 hint 自行提供原生多点触控 |
| `FRAME_RECT.encoding:u8` | **`1=RAW_RGBA8888`**，字节按 R,G,B,A，长度仍 `37+4*w*h` | 其他值 `UNSUPPORTED_FORMAT=11` |
| `FRAME_BEGIN.update_type:u8` | **`0=FULL`、`1=INCREMENTAL`**；FULL `base_frame_id=0` | 其他值 `INVALID_FIELD=5`；不把未知变成 FULL |
| `TARGET_LIST.flags:u8`、`TARGET_EVENT.flags:u8` | Native 1.0 **仅 `0`**；不携带隐含控制权/身份 | 非零 `INVALID_FIELD=5`，不读取未知权限位 |
| `TARGET_EVENT.op:u8` | **`1=ADD`、`2=UPDATE`、`3=REMOVE`**，REMOVE 时 `label_len=0` | 其他值 `INVALID_FIELD=5`，删除后旧订阅/缓存清理 |
| `TARGET_SUBSCRIBE.subscribe:u8` | **`0=UNSUBSCRIBE`、`1=SUBSCRIBE`** | 其他值 `INVALID_FIELD=5`，不得显示已订阅 |
| `TARGET_STATUS.status:u8` | **`1=SUBSCRIBED`、`2=UNSUBSCRIBED`、`3=DENIED`、`4=REMOVED`** | 其他值 `INVALID_FIELD=5`；拒绝/删除不投递画面 |
| `CONTROL_STATE.status:u8` | **`1=PENDING`、`2=GRANTED`、`3=DENIED`、`4=REVOKED`、`5=EXPIRED`、`6=RELEASED`** | 其他值 `INVALID_FIELD=5`；只有设备当前实际 GRANTED + GrantEpoch/Scope/Target 有效才允许输入 |
| `AUTH_RESULT.success:u8` | **`0=FAILED`、`1=SUCCEEDED`** | 其他值 `INVALID_FIELD=5`；认证成功不等于 ViewGrant |
| `INPUT.POINTER_BUTTON.button:u8` | **`1=LEFT`、`2=MIDDLE`、`3=RIGHT`、`4=X1`、`5=X2`** | 其他值 `INVALID_FIELD=5`，不映射为其他按键 |
| `INPUT.POINTER_BUTTON.pressed:u8`、`INPUT.KEY.pressed:u8` | **`0=RELEASE`、`1=PRESS`** | 其他值 `INVALID_FIELD=5`；held 仅在实际 Accepted 后改变 |
| `INPUT.KEY.modifiers:u16` | **bit0 SHIFT、bit1 CTRL、bit2 ALT、bit3 META、bit4 CAPS、bit5 NUM**；bit6..15 必须为 0 | 未定义高位 `INVALID_FIELD=5`；不直接 cast `Qt::KeyboardModifier` |

### 2.1 同次批准的网络键码数值分配

| 来源（V8 已定义区间） | Human 批准的精确值→名称 | 边界 |
| --- | --- | --- |
| `0x0101..0x010E` | `0101 ENTER`、`0102 ESCAPE`、`0103 TAB`、`0104 BACKSPACE`、`0105 DELETE`、`0106 INSERT`、`0107 HOME`、`0108 END`、`0109 PAGE_UP`、`010A PAGE_DOWN`、`010B LEFT`、`010C RIGHT`、`010D UP`、`010E DOWN` | V8 所给区间不变；不复用 Qt::Key 常量 |
| `0x0201..0x0206` | `0201 SHIFT`、`0202 CTRL`、`0203 ALT`、`0204 META`、`0205 CAPS_LOCK`、`0206 NUM_LOCK` | 与 modifier 位意义各司其职，不自动生成系统级危险组合键 |
| `0x0400..0x0410` | `0400..0409 KP_0..KP_9`、`040A DECIMAL`、`040B ENTER`、`040C ADD`、`040D SUBTRACT`、`040E MULTIPLY`、`040F DIVIDE`、`0410 EQUAL` | 0x0400..0409 依十六进制连续对应小键盘数字 0..9，区别主数字区 |
| V8 既定散值/范围 | `0x0020 SPACE`、`0x0030..0x0039 主键数字`、`0x0041..0x005A A..Z`、`0x0301..0x030C F1..F12` | 完全沿用原值，不另起一套键码 |

**数值归属规则**：第 2、2.1 节由 Human 本次 P-A **批准为 V8 的受控增补**，不能再标为“任意候选”；但仍属于**尚未完成整体验收的协议设计基线**，实际实现和互通资格需单独审查。此前 P-A 原文件的“建议/候选”只代表批准前历史状态，引用数值以**本文件**为准。

## 3. 已批准 SOURCE_TENSION_01=A：两种模式均 TLS-first

| 预先受控配置的 Native 模式 | 最低真实握手顺序 | 身份/权限条件 | 禁止行为 |
| --- | --- | --- | --- |
| `plain=0` | `TCP → HMI DeviceTLS 证书/身份验证 → HELLO → WELCOME(auth_mode=0)`，**不要求独立 Native 用户密码 AUTH** | TLS 证明的是**设备身份**，不证明具体远端自然人；设备 Host 须有明确有效、可信的当前授权来源，否则 Target 列表/帧/输入全部拒绝 | **不是 Native 明文**；TLS 不通过时零 HYRP；不以设备身份证书自动授 ViewGrant/ControlGrant |
| `tls-password=1` | `TCP → HMI DeviceTLS 证书/身份验证 → HELLO → WELCOME(auth_mode=1) → AUTH → AUTH_RESULT`，成功后仍独立核 Target/ViewGrant/ControlGrant | 独立用户名+加盐密码 verifier；成功认证只形成用户可信依据，具体授权由 HMI Host 的 Policy/Grant 当前态决定 | 不重用 VNCAuth 的 8 字节共享口令，不在无可信 TLS 情况发送 AUTH |
| Managed 通路 | `Remote View ↔ Edge ↔ Gateway ↔ Factory Edge ↔ **HMI DeviceTLS**`（代理只转 bytes） | 真实 HMI 身份 + 可归责用户身份 + HMI Scope/Grant；若 plain 档位不能提供可核用户身份/权限，则 **Managed 必须拒绝**，不能由 RoutePermit 代替 | 中继不得解开 HMI TLS、替客户端批准目标、改变 Native 模式或退回明文/RFB |
| LAN Direct | `View ↔ HMI DeviceTLS`，不依赖 Gateway | `plain` 不自带用户身份；只有 HMI 经有效 Host 策略**明确允许**的非敏感查看/许可操作才可用，未能证明则默认只读/拒绝目标 | 不因属于 LAN 就跳过证书核验，不因 TLS 完成就宣称已经获控 |

注：原 V8 §8.2 的 TLS-first 顺序与 A.1 的双预配置档位现在由 Human **明确按上述 A 解释**；之前 B（局域网 Native 明文）和 C（首批禁用 plain）**未获批准**。这不改变 H-DUAL=A 必需 listeners 原子服务准入，也不扩大 Gateway #190 的发布范围。

## 4. Host / Viewer 必须共同满足的解析与状态不变量

1. **单一值源**：所有平台使用第 2–2.1 节同一批准数值表；不在 F-05 和 V-01 维护两套等价但数值不同的 enum。公开 SDK API 不得因为补值被静默修改。
2. **状态、版本和未知值**：先检查头 flags、长度、消息顺序、caps=0、数值枚举，再决定转发给业务状态机。未知数值不得被解释为 authenticated、GRANTED、FULL、有效 Target 或被动回退。
3. **认证与授权**：DeviceTLS、Native 密码认证、目标可见、ViewGrant、ControlGrant 是不同状态；`AuthResult=SUCCEEDED` 不能绕过 Host 的 `authorizeView`。
4. **输入终审**：只在 Host 当前 AccessSession/Target/Scope/GrantEpoch 有效时入队，Qt 实际执行前再验证；失效时禁新输入、清 held，无旧资格重放。对 RFB/Native 同 Scope 争用执行已批准 H-RFB=A 的单控制者仲裁。
5. **帧保密**：先 HMI per-session Filter/Watermark 后再编码/TLS，grant epoch 生效后不能新排旧受限帧；旧 socket/TLS 已发送或远端已见数据物理上不能追回；View 自有敏感旧帧须遮蔽。
6. **失败区分**：`BAD_HEADER=1`、`UNSUPPORTED_VERSION=2`、`BAD_ORDER=3`、`BAD_LENGTH=4`、`INVALID_FIELD=5`、`UNSUPPORTED_CAPABILITY=6`、`AUTH_REQUIRED=7`、`AUTH_FAILED=8`、`RESOURCE_LIMIT=9`、`STALE_GENERATION=10`、`UNSUPPORTED_FORMAT=11`、`INTERNAL_ERROR=12`、`POLICY_DENIED=13` 保持 V8 原号；TLS 握手失败**不能**发送 HYRP ERROR 后降级。
7. **互操作前置**：第 2–2.1 节已获 Human 规范批准，但消息 positive/negative golden vectors、独立 HMI/Viewer 对拍、Qt/Windows/Linux/目标设备可信 TLS/策略资格仍待完成；不得写 `NATIVE_E2E=PASS`。

## 5. 受控版次、证据与独立 Gate

| 项目 | 当前状态 |
| --- | --- |
| Native 1.0 V8 原本已有字段/消息类型 | `V8_SOURCE_APPROVED_UNCHANGED` |
| 缺少数值/键码的 Human 补充 | `NATIVE_V8_WIRE_DELTA01_P_A=HUMAN_APPROVED` |
| TLS-first / plain 模式语义 | `SOURCE_TENSION_01=A_HUMAN_APPROVED` |
| 具体线缆数值与安全档位作为设计合同 | `NUMERIC_AND_MODE_SEMANTICS_APPROVED` |
| 本受控补充的 GitHub 独立审查与合并状态 | 以本文件所属的 GitHub PR 审查、要求检查及实际合并结果为准；不得用 Human 审批代替代码/文档审查 |
| 基于受控值的 golden vectors / Host-Viewer 实施与真互通 | `VECTOR_DESIGN_PENDING / IMPLEMENTATION_NOT_AUTHORIZED` |
| 全产品详细设计冻结 / Native 真实 E2E / View G3 / Gate-I / Gateway 激活 | **`NO / NOT_IMPLEMENTED / NOT_PASSED / NO / NOT_ACTIVATED`** |

**版本纪律**：不对已批准 V8 DOCX 原件做后台替换，也不宣称正式 Word 的 SHA 与送审稿一致。本受控 GitHub 附录是 **V8 附录 A/B 数值绑定与部署模式的增补审批记录**，不是已经发布的 SDK/协议实现版本。其内容需在后续“先目录/图表/版式、后正式编制”的全产品模块详细设计文档中交叉引用；任何重新分配数字、允许明文 Native 或改变 DeviceTLS 顺序，都需要新的 Human 决定。
