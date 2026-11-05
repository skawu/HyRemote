# HyRemote 模块详细设计：V15/V8 受控源全文核对与 Native A/B 差距登记（2026-11-05）

> **状态**：\`SOURCE_TEXT_AUDITED / DESIGN_TRACE_PARTIAL / GATE_D_NOT_FROZEN\`。本文仅是从 Human 提供的 Word **送审稿**读取的来源与差距索引，不重写 Native 1.0 报文规范，不对已批准正式 Word 进行二进制替代，也不批准产品研发。设计总控：[#843](https://github.com/skawu/HyRemote/issues/843)；五项 Human 决策已选 A。

## 1. 可信来源与证据等级

| 受控材料 | 2026-11-05 收到的文件及实际 SHA-256 | 历史审批链 | 审计结论 |
| --- | --- | --- | --- |
| \`HYR-PD-001/V15\` 内容依据 | \`HyRemote 产品设计文档_送审稿_V15.docx\`; \`b753ebc69bbed47c2d8761f2eaf5f616001fb3e6ae32cff42f432b8930084b6b\` | 与 [#827 V15 送审稿](https://github.com/skawu/HyRemote/issues/827#issuecomment-6628545801) 归档 SHA **完全匹配**；正式版批准记录 SHA \`d0408f333d4b9aec186067cd159ff8a4a2206ef66d58243fd4c5cd10ffb82cf1\` | 原始送审稿文件真实、结构完整；Human 于本轮明确确认“内容一样”，作为**正文等效声明**。没有用正式版文件的二进制摘要冒充本稿 |
| \`HYR-ARCH-001/V8\` 内容依据 | \`HyRemote 产品架构文档_送审稿_V8.docx\`; \`3d89a90559ae62852538e919b7f0ad2188f54806ef13d806a095474dccce5824\` | 与 [#827 V8 送审稿](https://github.com/skawu/HyRemote/issues/827#issuecomment-6628545801) SHA **完全匹配**；正式版批准记录 SHA \`1101d5546cfc0ab63e4cfde809ba4b0530e1048ed3760b56b8c9e7be3de6803a\` | 同上：可作为规范正文审阅，不代表正式版二进制已经取回、进行过逐字节差异比较 |

**审计方法**：独立读入两份挂载的 DOCX，检查 ZIP 成员完整性、段落/Heading 样式、Word 表格和附录 A/B 的完整字段，不依据转换 PDF 的分页或历史摘要推算内容。未把送审稿封面“待批准”文本视作 V15/V8 现在的审批状态：**Human 已于 2026-11-04 批准 V15/V8 正式版本**，并在 2026-11-05 对两份送审稿给出内容等效说明。上述 Word 原件尚未提交到 GitHub repo，故本仓库仅保留 SHA 与可复核的来源索引，不能把 Markdown 当 DOCX 原件。

## 2. 源结构核对与跨文档追踪

| 核验项 | 实测 | 结论 |
| --- | --- | --- |
| V15 二级条款 | Word \`Heading 2\` **56** 个，ID 互不重复；14 章，另有术语附录；7 个 Word 表，11 个内联图 | \`SOURCE_STRUCTURE_PASS\` |
| V8 二级小节 | Word \`Heading 2\` **99** 个，ID 互不重复；16 章及 A–F 六附录；35 个 Word 表，16 个内联图 | \`SOURCE_STRUCTURE_PASS\` |
| V15↔V8 规范对应 | V8 附录 D 的表 32 含 **56** 条 \`V15 ID → 正式小节标题 → 主责架构章节 → 补充引用\`；与 V15 实际 56 个标题**逐 ID、逐标题 56/56 精确相符**；缺失/多余/标题不符均 **0** | \`CROSS_DOCUMENT_SECTION_TRACE_PASS\`，**不等于** 56 个条款的所有行为已在 D1–D5 逐句验证 |
| V8 A.2 Native 字节协议 | 表 23 有 **21** 类互异 \`type\`，均有方向、字段顺序、正文长度/公式；A.3 有 5 个 INPUT subtype，A.5 有 13 个错误码和 5 个 CLOSE 原因，A.6 有 9 个强制限额项 | \`WIRE_TYPE_FIELD_INVENTORY=21/21\`，具体数值枚举缺口见第 4 节 |
| V8 B.1/B.2/B.3 | 7 项宿主接口设计、7 项授权事件约束、1 个**非实际批准清单**的字段示例 | \`HOST_INTERFACE_SOURCE_INDEXED\`，不把示例配置当 DeploymentAdmission 资格 |
| 21 类消息固定正文算术 | 对 17 种固定长度或定长前缀类型，按字段大小重算并比对 A.2 表 23 的长度，**17/17 匹配**；其余 4 类变长（AUTH、INPUT、TARGET_LIST、ERROR）按现有公式登记，未生成 wire 测试向量 | \`STATIC_LENGTH_ARITHMETIC_PASS\`，**不等于** parser / 未知值 / 恶意输入互操作 PASS |

### 2.1 56 条 V15 的产品域→已有设计责任（目录级，仍须行为级追踪）

| V15 二级小节范围 | 计数 | V8 依据 | 已有 D1–D5 设计责任与下一步 |
| --- | ---: | --- | --- |
| 1.1–1.4 产品定位 | 4 | V8 1–2、16、附录 D | D5/W00 规范优先级、禁止能力与验收 |
| 2.1–2.3 组件/共享运行时/业务确认 | 3 | V8 2–3、8、13 | D1 一套 Core/Runtime、D2/View、D3/Edge、D4/Tool |
| 3.1–3.4 四接入 | 4 | V8 4、5、9、10、15 | D1 F-01..06 与 D4 Qt Kit/部署资格 |
| 4.1–4.3 目标与显示 | 3 | V8 5–7、11 | D1 Target/Policy + D2 View/RemoteSurface |
| 5.1–5.4 双协议/Direct/Managed | 4 | V8 8–9、11、15 | D1 Protocol / D2 Direct / D3 Managed |
| 6.1–6.3 会话/控制/现场 | 3 | V8 5、7、10 | D1 Arbiter、Grant/epoch、D2 权限显示 |
| 7.1–7.3 键鼠/输入法/触控 | 3 | V8 7–8、11、13 | D1 Input/Native、D2 输入映射 |
| 8.1–8.7 HyRemoteView/Edge | 7 | V8 3、8–11、16 | D2 V-01..03 + D3 E-01/G-01 |
| 9.1–9.4 业务交换 | 4 | V8 13 | D1 宿主业务边界，D2 客户端能力，D4 验收 |
| 10.1–10.3 安全/隐私/审计 | 3 | V8 5–6、9、12 | D1 授权/隐私，D3 Managed，D4 全域 |
| 11.1–11.4 诊断/性能 | 4 | V8 10–12、15 | D1 资源、D4 SDK/Kit/质保、D2 View |
| 12.1–12.3 OEM/权益 | 3 | V8 11、14–15 | D2/ D4 产品升级、授权/离线持续使用 |
| 13.1–13.9 业务流程 | 9 | V8 4–11、14、16 | D1/D2/D3/D4 端到端 XP 场景 |
| 14.1–14.2 验收与一致性 | 2 | V8 1、16 | D4 XP01..20、D5 W00 和审批 Gate |
| **合计** | **56** | **V8 附录 D 逐条 56/56** | **设计责任已定位；逐项行为、状态、测试、Owner、Evidence 不能据此标 \`PASS\`** |

### 2.2 V8 的 99 个小节精确盘点（不是按估算汇总）

| 章节 | 二级节计数 | 设计接收方 |
| --- | ---: | --- |
| 1 / 2 / 3 / 4 | 4 / 4 / 6 / 6 | D5 / D1+D4 / D1+D3 / D1 |
| 5 / 6 / 7 / 8 | 5 / 5 / 6 / 7 | D1 / D1 / D1 / D1+D3 |
| 9 / 10 / 11 / 12 | 7 / 6 / 6 / 6 | D1+D3 / D1+D3 / D2+D3 / D4 |
| 13 / 14 / 15 / 16 | 6 / 5 / 5 / 6 | D2+D4 / D4 / D4 / D4+D5 |
| 附录 A / B | 6 / 3 | D1 Protocol + Host 安全合同 |
| **合计** | **99** | **来源目录完整；具体架构行为仍需与各模块状态机/异常/测试互查** |

**注意**：V8 附录 D 的新增 8.6/8.7/13.8/13.9 四项出现在对应表末尾而非数字顺序，这是原受控 Word 的排布事实，不因此误称条款缺失或私自改写 V8。未来模块总文档可按 ID 排序呈现派生追踪，不改上位版本。

## 3. V8 附录 A/B 的规范事实（只引用、不另立协议）

- **A.1 公共头**：\`[type:u16, flags:u16, payload_length:u32]\` **8 字节**、网络大端、v1.0 \`flags=0\`，在读取/分配正文前验证上限；\`plain\` / \`tls-password\` 在监听前配置，不支持 STARTTLS 或自动降级。**跨公网 Managed 仍要满足独立 HMI DeviceTLS/E2E 安全约束，不能因为 \`plain\` 字样就开放不可信网络**。
- **A.2 21 type 索引**：\`0001 HELLO(28)\`、\`0002 WELCOME(25)\`、\`0003 AUTH(4+u+p≤196)\`、\`0004 AUTH_RESULT(1)\`、\`0005 SCREEN_INFO(30)\`、\`0010 FRAME_BEGIN(31)\`、\`0011 FRAME_RECT(37+4wh)\`、\`0012 FRAME_END(20)\`、\`0013 REQUEST_FULL(20)\`、\`0020 INPUT(A.3)\`、\`0030 PING(8)\`、\`0031 PONG(8)\`、\`0040 TARGET_LIST(10+Σ(19+n)≤4714)\`、\`0041 TARGET_SUBSCRIBE(9)\`、\`0042 TARGET_STATUS(17)\`、\`0043 TARGET_EVENT(28+n)\`、\`0050 CONTROL_REQUEST(24)\`、\`0051 CONTROL_STATE(45)\`、\`0052 CONTROL_RELEASE(24)\`、\`00FE ERROR(4+n≤1028)\`、\`00FF CLOSE(2)\`。全类型字段顺序**以原 V8 附录 A.2 表 23 为唯一权威**；此索引禁止开发者据此替代完整来源。
- **A.3 输入**：所有 \`INPUT\` 使用 29 字节前缀 \`target_id:u64, scope_id:u64, grant_epoch:u64, generation:u32, input_type:u8\`；子类型 1 \`POINTER_MOVE(37)\`、2 \`POINTER_BUTTON(39)\`、3 \`POINTER_SCROLL(41)\`、4 \`KEY(34)\`、5 \`COMMITTED_TEXT(31+n; 1≤n≤4096)\`。A.3 表 25 单独列网络键码，不得把其直接 cast 到 Qt::Key。
- **A.4**：每目标独立 \`generation/frame_id\`，\`FRAME_BEGIN–RECT–END\` 不交错、FULL 无重叠/遗漏；发现基准错误后进入 WAIT_FULL 并 REQUEST_FULL；无逐帧 ACK。
- **A.5**：13 个 ERROR 码，以及 CLOSE=1 NORMAL / 2 PROTOCOL_ERROR / 3 POLICY / 4 SHUTDOWN / 5 RESOURCE；TLS 握手失败不能输出应用层 ERROR 后自动降级。
- **A.6**：最多 32 个可见 Target；UTF-8 label 最多 128 字节；尺寸每边 1..8192、总像素数不超过 8,388,608；RECT body 协商 64..256 KiB；每帧 RECT 1..4096；AUTH 用户 1..64、密码 1..128 字节。预算与协议限制不同，不能当作真实设备 RSS/公网资格证明。
- **B.1/B.2**：C++/QML/Generic/QPA 一套受控 Host Policy 与 Target 接口（7 项接口表）；\`registerTarget\`、\`publishPolicySnapshot\`、\`requestControl\`、\`resolveControl\`、\`revokeControl\`、\`revokeTarget\`、\`readCurrentPolicy\` 共 7 类事件；\`InputEnvelope\` 在接纳前及 Qt 实际执行前均检 \`RuntimeEpoch/AccessSessionId/TargetId/ScopeId/GrantEpoch/PolicyRevision\`。B.3 配置只是示例，不是有效签名/授权。
- **受控性能边界**（V8 §10.4/附录 C）：全局最多 8 连接、每目标普通 Qt 输入 64、保护性释放 136、每连接 Native 用户态待发 512 KiB、共享原始帧 96 MiB 属于受管 128 MiB 内部子池；**不是进程 RSS/客户 SLA 或已通过实测**。Edge/Gateway 另计自身资源预算，不得替 HMI 暗删加密帧。

## 4. 真正影响协议冻结的精确差距（不能静默补数值）

V8 A.2 **明确定义字段宽度和顺序**，但以下字段的具体 wire **数值域/位含义**在提供的 Word 正文、表 23–29 中**未找到完整、可执行分配表**。这不是声称 V8 没有定义协议，而是区分“字段存在/长度已定”与“两个独立实现可互通的枚举已完整签收”。

| 缺口 ID | V8 来源字段 | 观察到的未定部分 | 冻结前必须形成的受控输出 |
| --- | --- | --- | --- |
| \`WIRE-V01\` | \`HELLO.caps/WELCOME.caps\`（u32） | capability bit 分配、协商未知位处理及协商结果语义不完整 | 源内既有值检查；若无则提出版本化 bit table，连同负向向量 |
| \`WIRE-V02\` | \`WELCOME.auth_mode\`（u8） | \`plain\` / \`tls-password\` 的数字常量及 plain 与章节 8.2 TLS 顺序的适用档位说明缺一明确绑定表 | 统一取值、Direct/Managed 允许组合、安全拒绝与负向用例；**不得自动弱协议回退** |
| \`WIRE-V03\` | \`SCREEN_INFO.pixel_format/input_hint\`、\`FRAME_RECT.encoding\` | u8 枚举值、颜色/字节顺序、未知值和 RGBA 编码契约未单列 | 互通精确像素语义与未知值负测；不从 Qt 本地格式猜值 |
| \`WIRE-V04\` | \`FRAME_BEGIN.update_type\`、\`TARGET_LIST.flags\`、\`TARGET_EVENT.op/flags\` | FULL/INCREMENTAL 及 Target flags/op 的 wire 常量未完整列出 | 定义合法/非法值、目标失效/重订阅及基准恢复负测 |
| \`WIRE-V05\` | \`TARGET_SUBSCRIBE.subscribe\`、\`TARGET_STATUS.status\`、\`CONTROL_STATE.status\`、\`AUTH_RESULT.success\` | 请求布尔值与授权/拒绝/撤销/过期状态的 wire 数字值及未知行为缺明示枚举表 | 逐值状态机、跨版本与拒绝默认策略、无法越 Scope 的互通测试 |
| \`WIRE-V06\` | \`INPUT\` 的 \`button/pressed/modifiers\`、网络键码表 | 特殊键等表 25 仅列区间或部分键码、modifier 具体 bit 含义与按钮取值仍需复核 | 逐键码/输入法/滚轮/拒绝后 held 清理和跨 Qt 平台的受控映射；不得反向修改原字段宽度 |
| \`WIRE-V07\` | A.4 state/重同步 | 没有全量按消息、逐状态正/负向的 **确定性测试向量** 包 | 用 V8 既定字段生成测试 vectors、Fuzz/断流/重放/未知顺序/Frame 基线；声明未运行 |

这些是**技术详细设计/规范值绑定问题**，不是新的 5 项产品路线投票。若需要为 V8 **新增或改变 wire 取值或协议主版本语义**，应走上位受控变更和 Human 审签；在此之前不得由独立 Host/Viewer 开发者各自填空，也不能宣称 Native ABI/wire frozen。保留已批准的 **21 类 type ID、字段顺序、长度/资源边界和设备端最终 Grant** 不变。

## 5. 五项 Human A 的原文一致性审查

| 已批准选择 | V15/V8 原文对应 | 初步结论/剩余技术动作 |
| --- | --- | --- |
| H-T01=A | V15 §§2.1,11.2–11.3；V8 §§2.1–2.2,12 | **一致**：View 是完整访问主线，Tool 是 SDK/诊断；保留已发布 Tool 基础 RFB 与 #586 待验收事实 |
| H-DUAL=A | V15 §3.2/5.2；V8 §§4.5–4.6,9.1,10.1–10.2、表 14 | **一致**：初始配置冲突回滚、必需监听原子对外可用，运行期单协议降级可报告；OS 物理端口同时完成非承诺 |
| H-REVOKE=A | V15 §§4.3,6.1,10.2；V8 §§5.5,6.4,7.4、表 8 | **一致**：新 policy 生效后停止旧输出/输入、清仍可控队列、必要时断链，已交付 bytes/已见画面不能撤回 |
| H-EDGE=A | V15 §§8.6–8.7；V8 §§3.6,10.6,11.6 | **一致但需限定**：文档要求不同关闭/注销场景按已验证的平台资格如实声明；首批只支持登录态，保留“关一个远程画面窗口不应结束获准站点共享”的独立窗口/程序生命周期语义 |
| H-RFB=A | V15 §§5.1,6.1,10.1；V8 §§5.2–5.3,9.2 | **一致**：RFB 标准基础远程输入保留但受设备端授权、默认只读/Scope 独占；共享口令不构成个人身份或 Native Grant，明文 RFB 不自动获得公网资格 |

上述“**一致**”仅指**人已选的产品语义与上传的上位 Word 正文没有发现直接冲突**。不推出实现、实际平台、协议值或所有 56+99 细分合同都已验收。

## 6. 阶段 Gate 和有界收口清单

- \`SOURCE_DRAFT_SHA_MATCH_ARCHIVE=PASS\`、\`HUMAN_CONTENT_EQUIVALENCE=AFFIRMED\`、\`FORMAL_BINARY_SHA_CHECK=NOT_DONE\`（送审稿与正式稿 SHA 本应不同）。
- \`PD56_TO_V8_APPENDIX_D56=PASS\`、\`ARCH99_SOURCE_OUTLINE=PASS\`、\`NATIVE21_TYPE_FIELD_LENGTH_INVENTORY=PASS\`、\`FIXED_LENGTH_ARITHMETIC=PASS\`。
- \`NORMATIVE_TO_MODULE_BEHAVIOR_TRACE=PARTIAL\`：本稿完成真实来源与章/域接收方定位，仍需 D1–D5 **逐行为**关联 State/Failure/Test/Owner；不能用计数代替设计签收。
- \`NATIVE_WIRE_VALUE_BINDINGS=WIRE-V01..V07_OPEN\`；\`WIRE_FROZEN=NO\`。
- \`HUMAN_H_T01_DUAL_REVOKE_EDGE_RFB=APPROVED_A\`；\`DETAIL_DESIGN_FROZEN=NO\`、\`VIEW_G3=NOT_PASSED\`、\`NATIVE_E2E=NOT_IMPLEMENTED\`、\`GATEWAY_190=NOT_ACTIVATED\`、\`GATE_I_IMPLEMENTATION_AUTHORIZED=NO\`。

**下一步只做设计收口**：优先补 \`WIRE-V01..V07\` 的值域/正负向合同及 Native 直接/受管安全档位说明；以本来源清单对照 D1–D5 的接口和验收状态，产出真正可验的 56/99 行级追踪并审查。若确认必须改变已批准 V8 Native wire，提出精确差量而非代码先行。文档和测试规划不等于自动生成产品代码或正式 Word。
