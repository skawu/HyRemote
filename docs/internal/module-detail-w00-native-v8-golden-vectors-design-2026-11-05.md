# W00｜Native V8 已批准补充值的**字节级黄金向量设计**（2026-11-05）

> **权威与局限**：本文件严格依据 [原 V8 附录 A/B + 已获 Human 批准的 NATIVE-V8-WIRE-DELTA-01 P-A / SOURCE_TENSION_01=A](module-detail-w00-native-v8-delta01-approved-2026-11-05.md)，由机器按网络大端自动计算预期字节，**不是产品 Parser/Host/Viewer 实际运行结果**。本轮 `VECTOR_BYTES_COMPUTED=26/26`；`PARSER_TEST_RUN=0`；`HMI_VIEWER_E2E=NOT_IMPLEMENTED`。

## 1. 生成约定和不变量

- 所有行已经在**独立可信 HMI DeviceTLS 通道之后**解释为应用层 HYRP 消息。TLS 握手失败时**零个** HYRP 字节能发送；下文从不代表 TLS 握手自身密文。
- 字节序一律网络大端，头为 `type:u16 flags:u16=0 payload_length:u32`。十六进制每两位一个字节，`payload_length` **仅**是头后正文长度。
- 标准化纯示例常量：`target_id=1`、`scope_id=2`、`grant_epoch=3`、`generation=4`、`policy_revision=5`、`frame_id=6`、`request_id=7`，`HELLO.max_rect_body=65536`、`max_width=1024`、`max_height=768`、`max_pixels=786432`。它们**不是产品固定设备 ID/Policy**。
- WELCOME `auth_mode=0`（TLS 下不做独立密码 AUTH）和 `=1`（TLS + 密码 AUTH）各占一个向量。INPUT 同一消息类型 `0x0020` 有五子型样例。无实际用户密码使用，`AUTH` 示例的 `op/pw` 是无效测试字符串，**不得做真实产品登录凭据**。
- FRAME 样例 1×1 像素 `11 22 33 ff` 与 SCREEN_INFO 的 RGBA8888 相符，FULL base=0, rect_count=1。多目标/遮罩/撤权需额外状态测试，不由这一帧冒充。
- 向量**只定义严格字节级预期和解析上下文**，不批准版本切换、跳过 TLS、Gateway 解密、隐式 ViewGrant、RFB 非标准扩展或独立产品代码实现。

## 2. Native 21/21 消息 type，26 个可复核字节向量

| Vector | 消息类型及批准枚举示例 | 正文长度（byte） | 通用头＋正文（十六进制字节） | 解码/权限预期 |
| --- | --- | ---: | --- | --- |
| GV-01 | HELLO (`0x0001`) | 28 | `00 01 00 00 00 00 00 1c 48 59 52 50 00 01 00 00 00 00 00 00 00 01 00 00 00 00 04 00 00 00 03 00 00 0c 00 00` | 建立 v1.0/caps0 协商候选；仍须 TLS 先完成 |
| GV-02 | WELCOME plain (`0x0002`) | 25 | `00 02 00 00 00 00 00 19 00 01 00 00 00 00 00 00 00 01 00 00 00 00 04 00 00 00 03 00 00 0c 00 00 00` | 模式与受控部署一致，不因 TLS 自动授 Grant |
| GV-03 | WELCOME tls-password (`0x0002`) | 25 | `00 02 00 00 00 00 00 19 00 01 00 00 00 00 00 00 00 01 00 00 00 00 04 00 00 00 03 00 00 0c 00 00 01` | 模式与受控部署一致，不因 TLS 自动授 Grant |
| GV-04 | AUTH (`0x0003`) | 8 | `00 03 00 00 00 00 00 08 00 02 6f 70 00 02 70 77` | 仅 tls-password TLS 后；不能打印凭据日志 |
| GV-05 | AUTH_RESULT success (`0x0004`) | 1 | `00 04 00 00 00 00 00 01 01` | 认证成功不等于 ViewGrant |
| GV-06 | SCREEN_INFO RGBA (`0x0005`) | 30 | `00 05 00 00 00 00 00 1e 00 00 00 00 00 00 00 01 00 00 00 04 00 00 00 01 00 00 00 01 01 00 00 00 00 00 00 00 00 05` | 单像素 1×1、RGBA8888、不授控制 |
| GV-07 | FRAME_BEGIN FULL (`0x0010`) | 31 | `00 10 00 00 00 00 00 1f 00 00 00 00 00 00 00 01 00 00 00 04 00 00 00 00 00 00 00 06 00 00 00 00 00 00 00 00 00 00 01` | base=0；后续 1 个完整 RECT |
| GV-08 | FRAME_RECT 1x1 (`0x0011`) | 41 | `00 11 00 00 00 00 00 29 00 00 00 00 00 00 00 01 00 00 00 04 00 00 00 00 00 00 00 06 00 00 00 00 00 00 00 00 00 00 00 01 00 00 00 01 01 11 22 33 ff` | RGBA #112233, alpha ff，完整帧的 1 个矩形 |
| GV-09 | FRAME_END (`0x0012`) | 20 | `00 12 00 00 00 00 00 14 00 00 00 00 00 00 00 01 00 00 00 04 00 00 00 00 00 00 00 06` | 匹配 BEGIN 的 Target/Generation/FrameId |
| GV-10 | REQUEST_FULL (`0x0013`) | 20 | `00 13 00 00 00 00 00 14 00 00 00 00 00 00 00 01 00 00 00 04 00 00 00 00 00 00 00 00` | 恢复时请新 FULL，不重新应用失效 DELTA |
| GV-11 | INPUT POINTER_MOVE (`0x0020`) | 37 | `00 20 00 00 00 00 00 25 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 02 00 00 00 00 00 00 00 03 00 00 00 04 01 00 00 00 00 00 00 00 00` | 仅当前 ControlGrant/Target/Scope/epoch 有效才入队 |
| GV-12 | INPUT POINTER_BUTTON (`0x0020`) | 39 | `00 20 00 00 00 00 00 27 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 02 00 00 00 00 00 00 00 03 00 00 00 04 02 00 00 00 00 00 00 00 00 01 01` | LEFT PRESS；Qt 投递前再次检 epoch |
| GV-13 | INPUT POINTER_SCROLL (`0x0020`) | 41 | `00 20 00 00 00 00 00 29 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 02 00 00 00 00 00 00 00 03 00 00 00 04 03 00 00 00 00 00 00 00 00 00 01 ff ff` | dx=1，dy=-1（i16），需合法坐标 |
| GV-14 | INPUT KEY (`0x0020`) | 34 | `00 20 00 00 00 00 00 22 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 02 00 00 00 00 00 00 00 03 00 00 00 04 04 00 41 01 00 01` | 主字母 A PRESS + SHIFT bit0，不映射 Qt::Key 编号 |
| GV-15 | INPUT COMMITTED_TEXT (`0x0020`) | 34 | `00 20 00 00 00 00 00 22 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 02 00 00 00 00 00 00 00 03 00 00 00 04 05 00 03 e4 b8 ad` | UTF-8「中」三字节，不额外注入 KeyDown |
| GV-16 | PING (`0x0030`) | 8 | `00 30 00 00 00 00 00 08 00 00 00 00 00 00 00 01` | Nonce 需由回包原样回显 |
| GV-17 | PONG (`0x0031`) | 8 | `00 31 00 00 00 00 00 08 00 00 00 00 00 00 00 01` | Nonce 与最近有效 PING 匹配，不能凭它授予权限 |
| GV-18 | TARGET_LIST HMI (`0x0040`) | 32 | `00 40 00 00 00 00 00 20 00 00 00 00 00 00 00 05 00 01 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 02 00 00 03 48 4d 49` | 列出当前已授权 Target；flags=0 |
| GV-19 | TARGET_SUBSCRIBE (`0x0041`) | 9 | `00 41 00 00 00 00 00 09 00 00 00 00 00 00 00 01 01` | subscribe=1，尚需 HMI Target 授权 |
| GV-20 | TARGET_STATUS (`0x0042`) | 17 | `00 42 00 00 00 00 00 11 00 00 00 00 00 00 00 01 01 00 00 00 00 00 00 00 05` | status=1 SUBSCRIBED，仍不等于 ControlGrant |
| GV-21 | TARGET_EVENT ADD (`0x0043`) | 31 | `00 43 00 00 00 00 00 1f 00 00 00 00 00 00 00 05 01 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 02 00 00 03 48 4d 49` | op=1 ADD，flags=0；先检查 PolicyRev |
| GV-22 | CONTROL_REQUEST (`0x0050`) | 24 | `00 50 00 00 00 00 00 18 00 00 00 00 00 00 00 07 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 02` | 设备端按当前 Scope/现场优先决定 |
| GV-23 | CONTROL_STATE GRANTED (`0x0051`) | 45 | `00 51 00 00 00 00 00 2d 00 00 00 00 00 00 00 07 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 02 02 00 00 00 00 00 00 00 03 00 00 27 10 00 00 00 00 00 00 00 05` | status=2 GRANTED，grantEpoch=3；只有真实 Policy 当前有效可用 |
| GV-24 | CONTROL_RELEASE (`0x0052`) | 24 | `00 52 00 00 00 00 00 18 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 02 00 00 00 00 00 00 00 03` | 请求释放，不表示旧输入已经执行 |
| GV-25 | ERROR INVALID_FIELD (`0x00fe`) | 4 | `00 fe 00 00 00 00 00 04 00 05 00 00` | error_code=5，detail_len=0，不泄漏机密 |
| GV-26 | CLOSE POLICY (`0x00ff`) | 2 | `00 ff 00 00 00 00 00 02 00 03` | reason=3 POLICY，需收尾停止 Input/帧 |

**机械校验结果**：`TYPE_COUNT=21/21`; `VECTOR_COUNT=26`; `HEADER_LENGTH_MATCH=26/26`; `V8_A2_FIXED_OR_VARIABLE_LENGTH_FORMULA=26/26`; `APPROVED_ENUM_FIELDS_ONLY=YES`。上述校验在设计内容构造时按已批准字段尺寸计算，**不是代码、构建、UI 或操作系统测试**。

## 3. 解析器必须拒绝的独立向量（此处为精确变异规格，全部 NOT_RUN）

| ID | 从正例变异哪些线缆字节 | 必须观察的否定结果 |
| --- | --- | --- |
| GN-01 | 任意头 `flags` 从 `00 00` 改为 `00 01` | V8 v1.0 无有效 flags，拒绝正文；不能创建会话或提升权限 |
| GN-02 | HELLO 的 `caps` 四字节由 `00 00 00 00` 改为 `00 00 00 01` | `UNSUPPORTED_CAPABILITY=6`，不猜未来能力 |
| GN-03 | WELCOME 的末字节 `auth_mode=02` | `INVALID_FIELD=5`，不降到 plain 或 RFB |
| GN-04 | SCREEN_INFO 的 `pixel_format=00` 或 FRAME_RECT 的 `encoding=00` | `UNSUPPORTED_FORMAT=11`，不会错误解码本地 Qt BGRA |
| GN-05 | FRAME_BEGIN 的 `update_type=02` | `INVALID_FIELD=5`，不把未知字段按 FULL 处理 |
| GN-06 | CONTROL_STATE 的 `status=07` | `INVALID_FIELD=5`，不得将未知状态视为 GRANTED |
| GN-07 | TARGET_LIST/TARGET_EVENT 的 `flags=01` 或 `TARGET_EVENT.op=04` | `INVALID_FIELD=5`，不以未知 bit 暗示扩大授权 |
| GN-08 | INPUT KEY `modifiers=00 40`（未定义 bit6）或 `pressed=02` | `INVALID_FIELD=5`，无 Qt 按键和 held 状态污染 |
| GN-09 | INPUT 的 `grant_epoch=2`（示例当前为 3），且在 Qt GUI 邮箱已排队 | HMI 网络准入与 Qt 最终执行前均不能接受已撤销 Grant，保证受保护释放 |
| GN-10 | `WELCOME` 在 TLS 之前被要求发送、HMI 身份不可信、证书更换未受信 | **零** HYRP 应用数据、拒绝连接；不可自动尝试无 TLS plain |
| GN-11 | `plain` 模式得到 TLS 后未独立完成可归责用户/Host 授权，却请求 Managed Target/Control | 设备目标/画面/输入全部拒绝，Edge/Gateway RoutePermit 不替代 HMI 授权 |
| GN-12 | HEADER `payload_length` 比真实正文小/大 1，或 FRAME_RECT `37+4wh` 溢出/尺寸越界 | 读取/分配前拒绝；无半帧与越界内存分配；具体 ERROR/CLOSE 依 V8 已有类型 |
| GN-13 | 基于 GV-12 的合法 INPUT POINTER_BUTTON，将正文最后 2 字节由 `01 01` 改为 `04 01`、`05 01`，并分别用 `04 00`、`05 00` 收尾（P-A X1/X2） | 已授权、资源可用的真实 Qt Kit 必须保留 X1/X2 身份并有正确 Qt DOWN/UP；不得经公开三按钮 `PointerButton::None` 静默丢弃，Kit 无法证明语义须明确不可用而非 INVALID_FIELD |
| GN-14 | GV-14 KEY 正文末 5 字节中的 `00 41` 改为 `04 00`（KP_0）、`04 0b`（KP_ENTER）、`04 10`（KP_EQUAL）；其余 pressed/modifiers 按既有结构保持合法 | 这些码由已批准 P-A 承认，设备必须区分小键盘与普通数字或普通 ENTER；Viewer 无受信 Qt Keypad 来源时不能猜码，不能按 UNKNOWN/非法 enum 静默映射 |
| GN-15 | 同真实 Qt 叶子、同当前 Grant 下依次使用 P-A 全部合法键/按钮值，保持最多 72 个不同 held 类别；再发送第 73 个合法新 DOWN，并随后释放已持有的键 | 91 个**可表达**类别不等于 91 个同时持有资格；第 73 个按下必须在 F-06 准入时作为有界资源拒绝，不得输入 Qt、不得改变现有 held；既有保护 UP 仍成功，实际 Qt protected mailbox 不越 136 |
| GN-16 | 两个真实 Peer（可分别为 Native/RFB）指向同一 QtInputDomain：A 已交付 X1 与 KP_ENTER 按下，B 发送不属于自身的 UP，继以 A 失权、Ctrl 切换及延迟叶子 GUI 投递 | 未获准 peer/过代 UP 不释放 A/现场的 held；A 撤权后只对 A 的真实物理持有结算一次，旧票据不复活；不能借不同 ScopeId 绕过同 Qt 输入域的冲突约束 |


**后续验证合同**：F-05 与 V-01 应各自以**相同 26 个已批准字节向量**做读写对拍、畸形帧/fuzz、消息状态序列/证书与策略拒绝、Qt held 释放与双协议隔离，并记录 Host SHA、Viewer SHA、Kit/OS、每项生成/解码差异及 PASS/FAIL 日志。两端同一个“自实现解析器”互相证明不足以替代 V8 golden vector；Edge/Gateway 不得重编码或删除这些字节。

**GD-05 新增规范检查与证据状态**：GN-13..GN-16 是针对**已经批准** P-A KEY/POINTER_BUTTON 合法取值和 F-06 真实输入模型边界的**负向实现在位验证**，不是新增线缆消息、重新分配编号，也不是把“合法输入必须在资源上限外接受”写成新规范。26 个原字节正向向量保持字节不变；负向/资源/来源反证规格现合计 **GN-01..16（16 项，全部 NOT_RUN）**。X1/X2 与小键盘的正向可用性必须在具体 Qt Kit、DeviceTrust、当前 ControlGrant 及资源容量均满足时才能验收；无法支持的 Kit 仍不得宣称当前该能力已经可用。

## 4. 按协议来源隔离审签和交付状态

| 层 | 状态 |
| --- | --- |
| V8 原始 21 type、A.2/A.3/A.5 字段/限额 | `UPPER_SOURCE_APPROVED_UNCHANGED` |
| P-A 精确补值、SOURCE_TENSION_01=A | `HUMAN_APPROVED` |
| 本文 26 个向量的十六进制/长度构造 | `DESIGN_COMPUTED / REVIEW_PENDING` |
| Host/Viewer parser/interoperability/恶意输入真实测试（GN-01..16） | `NOT_RUN` |
| Native E2E / View G3 / Gateway #190 / Gate-D / Gate-I | `NOT_IMPLEMENTED / NOT_PASSED / NOT_ACTIVATED / NO / NO` |

备注：本文件是协议**测试数据和正负向设计计划**，不是产品代码生成器；正式 Word 的目录、图表和版式仍先于正文编制审查，不因数值已获批而跳过全产品 Gate。
