# W00｜V15 表6 的 31 个真实验收场景 ↔ D4 XP 设计矩阵（2026-11-05）

> 源：Human 给出的完整 `HYR-PD-001/V15` 送审稿，SHA 与 #827 历史归档精确匹配且 Human 确认正文内容与正式版相同。V15 表 6 有 **31 个场景，Word 表中无 S 编号**；本文件按原表行序建立 **S01..S31 派生索引**，不改变原文编号或任何验收事实。D4 原已有 `XP-01..XP-20`；对其中无法直接、明确证明的领域补设计 `XP-21..XP-28`，这些 ID 仅为**未来可执行验收计划**，不是运行通过。

## 1. 31/31 源场景对照验收用例设计

| 派生源行 ID | V15 表6 场景原文 | D4 设计用例 ID | 必须观察/负向失败要点（源文归纳） | 执行状态 |
| --- | --- | --- | --- | --- |
| S01（V15 表6 行 1） | 只读连接 | XP-03,XP-04,XP-20 | 只读会话不得本地或设备端越过 ControlGrant | `DESIGN_COVERED_NOT_RUN` |
| S02（V15 表6 行 2） | 多屏授权改变 | XP-06,XP-26 | 撤销目标旧坐标、待发送帧及 View 缓存失效 | `DESIGN_COVERED_NOT_RUN` |
| S03（V15 表6 行 3） | 标准 VNC 访问 | XP-02 | 标准 RFB 独立第三方 Viewer，不依赖私有扩展 | `DESIGN_COVERED_NOT_RUN` |
| S04（V15 表6 行 4） | 私有协议访问 | XP-03,XP-07 | Native 独立身份/完整帧/输入/授权；A/B 值域仍 OPEN | `DESIGN_COVERED_NOT_RUN` |
| S05（V15 表6 行 5） | 控制撤销、断线重连 | XP-05,XP-26 | held 清理及不自动恢复旧许可/业务操作 | `DESIGN_COVERED_NOT_RUN` |
| S06（V15 表6 行 6） | 中文输入与触控 | XP-21 | 新补：CommittedText/IME/本地手势/触控能力的真实边界 | `DESIGN_COVERED_NOT_RUN` |
| S07（V15 表6 行 7） | 上传业务配置 | XP-22 | 新补：传输、宿主接收及业务生效分段确认/未知不重交 | `DESIGN_COVERED_NOT_RUN` |
| S08（V15 表6 行 8） | 客户接管 OEM 设备 | XP-23 | 新补：客户接管撤旧凭据、售后委托可撤销、确认来源 | `DESIGN_COVERED_NOT_RUN` |
| S09（V15 表6 行 9） | 客户端自身更新 | XP-24 | 新补：可信更新/离线升级/配置迁移/失败回退 | `DESIGN_COVERED_NOT_RUN` |
| S10（V15 表6 行 10） | 高负载与慢 Viewer | XP-17 | 设备端 GUI/撤权优先，用户态预算/慢客户端有界 | `DESIGN_COVERED_NOT_RUN` |
| S11（V15 表6 行 11） | 首次连接与身份变更 | XP-07,XP-20 | 设备身份首次核验，证书变化 fail closed | `DESIGN_COVERED_NOT_RUN` |
| S12（V15 表6 行 12） | 跨协议控制权撤销 | XP-04,XP-05 | 两协议共 Scope 与 Qt 最终栅栏 | `DESIGN_COVERED_NOT_RUN` |
| S13（V15 表6 行 13） | 隐私策略变化及旧画面 | XP-06,XP-26 | 敏感旧帧失效，不仅 UI 遮罩 | `DESIGN_COVERED_NOT_RUN` |
| S14（V15 表6 行 14） | 传统 RFB 权限限制 | XP-02,XP-04,XP-20 | 共享 VNC 口令不伪称个人身份或 DeviceTLS | `DESIGN_COVERED_NOT_RUN` |
| S15（V15 表6 行 15） | 免改源码接入的权限边界 | XP-01,XP-20 | Generic/QPA 不能公开未知 Qt 窗口 | `DESIGN_COVERED_NOT_RUN` |
| S16（V15 表6 行 16） | HyRemoteTool 集成排查 | XP-14 | Qt Kit 实测结果/未执行/未知区分和脱敏 | `DESIGN_COVERED_NOT_RUN` |
| S17（V15 表6 行 17） | 第三方查看器的隐私边界 | XP-06,XP-25 | 新补：撤权停止新输出，但不宣称删除第三方已存副本 | `DESIGN_COVERED_NOT_RUN` |
| S18（V15 表6 行 18） | 现场接管与跨协议控制 | XP-04,XP-05 | 现场抢占后两协议 held 清理，业务结果独立确认 | `DESIGN_COVERED_NOT_RUN` |
| S19（V15 表6 行 19） | 断线旧画面与在线撤销 | XP-26 | 新补：普通已允许过期帧与敏感/未知旧帧区分 | `DESIGN_COVERED_NOT_RUN` |
| S20（V15 表6 行 20） | 离线售后授权到期 | XP-23,XP-28 | 新补：未送达撤销待确认、委托不续活、永久离线权不被误关 | `DESIGN_COVERED_NOT_RUN` |
| S21（V15 表6 行 21） | 工厂仅安装 HyRemoteView | XP-08,XP-15 | 同一 View 产品内置 Edge，无独立 Sidecar/VPN | `DESIGN_COVERED_NOT_RUN` |
| S22（V15 表6 行 22） | 云断网仍内网可用 | XP-11,XP-19 | 合法 Direct 不依赖 Gateway/企业账号 | `DESIGN_COVERED_NOT_RUN` |
| S23（V15 表6 行 23） | 共享默认关闭与设备白名单 | XP-08,XP-09 | 仅分享获准 HMI，不变泛 TCP 网段代理 | `DESIGN_COVERED_NOT_RUN` |
| S24（V15 表6 行 24） | 跨工厂同名 HMI | XP-09,XP-13 | Site/HMI 身份与 IP/显示名分离 | `DESIGN_COVERED_NOT_RUN` |
| S25（V15 表6 行 25） | 站点准入但设备无权 | XP-10 | Gateway RoutePermit 不代替 HMI ViewGrant | `DESIGN_COVERED_NOT_RUN` |
| S26（V15 表6 行 26） | 远程控制及现场接管 | XP-04,XP-05 | Host 独占 Scope、现场撤权最终生效 | `DESIGN_COVERED_NOT_RUN` |
| S27（V15 表6 行 27） | 隐私与中转内容 | XP-06,XP-10 | HMI 先过滤，Edge/Gateway 不收明文 | `DESIGN_COVERED_NOT_RUN` |
| S28（V15 表6 行 28） | 网关/边缘/HMI 多点故障 | XP-11,XP-20,XP-26 | 失败原因真实、无自动弱降级/旧业务重放 | `DESIGN_COVERED_NOT_RUN` |
| S29（V15 表6 行 29） | 边缘后台、退出与升级 | XP-12,XP-24 | H-EDGE=A 首批登录期可用，注销离线；单产品升级 | `DESIGN_COVERED_NOT_RUN` |
| S30（V15 表6 行 30） | 慢客户端与多 HMI | XP-17,XP-27 | 新补：透明 FIFO/有界回压、HMI 不二次采集 | `DESIGN_COVERED_NOT_RUN` |
| S31（V15 表6 行 31） | 离线永久使用权益 | XP-19,XP-28 | 新补：托管到期不剥夺已合法取得的 Direct 基础权益 | `DESIGN_COVERED_NOT_RUN` |

## 2. 补充 XP-21..XP-28 设计验收规格（不扩大既有版本发布范围）

| 拟增验收 ID | 用户与场景 | 可观察的正向结果 | 必须拒绝/不能误报 | 设计责任方 | 状态 |
| --- | --- | --- | --- | --- | --- |
| XP-21 | 中文/IME 与触控输入边界 | 连接合法 Native/RFB，输入中文组合、提交和取消、主键/小键盘与鼠标仿真或本地缩放手势；Native 1.0 原生多点触控不作为已实现基础 | 无文字重复/误快捷键；不因文本输入自动开启剪贴板；本地手势不能触发设备操作，未协商原生触控显示不可用 | F-05/F-06; V-02 | `SPEC_ONLY_NOT_RUN` |
| XP-22 | 文件传输与设备业务结果分段 | 宿主开启明确资源/大小/方向/期限授权，上传后分别返回传输、完整性、宿主接收/拒绝、业务应用结果；断线时业务结果可能未知 | 未经授权的目录/路径穿越拒绝；结果未知不自动二次提交或假报设备应用成功 | F-06; V-03; X-02 | `SPEC_ONLY_NOT_RUN` |
| XP-23 | 客户接管、OEM 委托及离线期限 | 客户接管时撤旧维护凭据；现场或离线售后委托有可核验目标、Scope、期限与撤销送达/确认状态 | 未送达撤销只能待确认；离线时无法可靠判断时不延长售后权限；不误关永久基础离线使用权益 | F-06; V-03; X-02 | `SPEC_ONLY_NOT_RUN` |
| XP-24 | View 自身可信更新与离线恢复 | View 验证来源/完整性、配置与设备档案/敏感凭据迁移；离线更新可用，下载或安装失败可恢复；Edge 同产品更新 | 更新失败不丢档案、不泄漏凭据、不出现未认证假在线；宿主 Qt 业务程序不被要求同步升级 | V-03; E-01; X-01 | `SPEC_ONLY_NOT_RUN` |
| XP-25 | 第三方 Viewer 已交付画面的真实撤权边界 | 授权新 epoch 生效后 HMI 不新输出旧权限帧，清理自有缓存/必要断链，第一方 View 清旧帧；对第三方保存内容列明不可追溯擦除 | 不能宣称远程删除第三方截图、录屏、文件；不能把仅客户端遮罩写成设备端隐私隔离 | F-02/F-06; V-02; X-02 | `SPEC_ONLY_NOT_RUN` |
| XP-26 | 断线旧帧和在线撤权分级 | 网络中断后已获宿主准许普通目标可短期带过期标记；敏感/未知目标遮蔽，重新接入按现行 DeviceId/Target/Grant | 不得把历史像素显示成实时，恢复后不得自动重授权/重放旧命令；在线撤权受限旧帧清理 | F-02/F-06; V-02/V-03 | `SPEC_ONLY_NOT_RUN` |
| XP-27 | 慢端多 HMI 与中继背压 | 同站点多个 HMI、多个 Viewer、慢链路与撤 Route；HMI 编码/加密前按协议处理过期帧，Edge/Gateway 透明 FIFO 有界队列 | Edge/Gateway 不拆删加密 Native 字节、不重新采集/编码 HMI、队列不无界，合法 Direct/本地 HMI 不被拖垮 | F-02; E-01/G-01; X-02 | `SPEC_ONLY_NOT_RUN` |
| XP-28 | 永久基础 Direct 与临时企业/售后权益分离 | 合法永久离线基础使用权存在时，断网/订阅到期/售后委托到期后仍按 HMI 当前安全授权使用已有 Direct 基础能力 | 不因企业服务停用自动锁死合法 Direct；也不把商业权益等同设备身份、控制许可或售后委托期限 | F-06; V-03; X-01/X-02 | `SPEC_ONLY_NOT_RUN` |

## 3. 以“设计覆盖”和“真实证据”两套独立口径验收

| 口径 | 本轮客观状态 |
| --- | --- |
| `SOURCE_V15_TABLE6_ROWS` | Word 表6 **31** 行场景已抽取并逐行保留标题 |
| `SOURCE_SCENARIO_TO_XP_DESIGN` | **31/31 有至少一条明确的未来 XP 设计入口**，既有 20 条 + 新补 8 条，允许一条源场景对应多个 XP |
| `NEW_XP_TEST_EXECUTION` | `XP-21..XP-28 = NOT_RUN`，不暗示已实现 Native/Managed/OEM |
| `OLD_XP_TEST_EXECUTION` | 本轮未执行原 XP-01..XP-20，也不从既有 PR CI 推断产品 PASS |
| `REQ56_ATOMIC_COVERAGE` | 另见 [W00 56 项行为初始映射](module-detail-w00-v15-behavior-trace-2026-11-05.md)，不能用 31 个场景替代每个复合产品要求的原子拆分 |
| `NATIVE_WIRE` | `WIRE-V01..V07`、`SOURCE_TENSION_01` 仍未审签；[21 类 Native 报文字段与负向规格](module-detail-w00-native-wire-contract-gaps-2026-11-05.md) 仍是测试计划 |
| `DESIGN_FREEZE/GATE_I` | **NO/NO**；`Gateway #190 = NOT_ACTIVATED`，`HyRemoteView G3 = NOT PASSED` |

**检查结论**：本轮修的是 **验收规格缺口**，不是从需求外增加产品特性。例如 Native 1.0 不包括原生多点触控/文件交换等未协商扩展；XP-21 的“支持与不可用正确表达”与 XP-22 的文件能力取决于宿主受控业务接口，不构成新的 Native 1.0 消息。测试实际执行仍受 Gate-G3/Gate-I 和独立产品/平台的准入约束。
