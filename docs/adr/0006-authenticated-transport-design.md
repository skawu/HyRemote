# ADR-0006：认证与加密传输设计（`[SEC-01]` #143）

> 语言 / Language：**中文** ｜ [English](../en/adr/0006-authenticated-transport-design.md)

状态：**`[SEC-01]`（#143）设计阶段提议，部分已被 #174 取代**

> **#174（2026-09-23）部分取代本记录。** 下面的可达性描述是当时的设计。实际发布契约是：listener 默认 `0.0.0.0:5921`，可以收窄到一个精确 IPv4 地址或一个命名接口；实际发布的安全状态必须如实报告，不能把收窄 bind 当作安全 workaround。

## 背景

发布路线图把认证与加密传输定为 **V1.0.0.0 要求，而不是 `V1.x` 后续轨道**，顺序是设计、认证、加密、每客户端授权/审计。但 `docs/security-model.md` 第 10 节当时仍把它写成 baseline 之后的能力。两个发布权威不一致，这正是统一范围权威要防止的缺陷。

当时的传输只声明一个 RFB 安全类型 `None`，在握手中以内联字面量出现；仓库没有密码学代码，也没有第三方安全依赖；非 loopback listener 被“认证传输存在前 fail closed”的保护拒绝。真实机制应当替换这条保护。

## 决策

1. 认证步骤使用 **RFB VNC authentication（security type 2）**，`None` 只作为显式选择的模式保留，绝不作为隐式降级。
2. **加密是独立的后续步骤**，因此 viewer 互操作不是第一批认证证据的前提。
3. 密码学原语来自 **OpenSSL**，接受仓库第一个第三方安全依赖及其 package/licence/notice/deployment 义务。
4. **只有启用认证模式时才接受非 loopback bind**，否则保持 fail closed；构造和 listener 默认值不变。
5. **认证不等于授权**：输入策略边界不变，失败有界；在步骤落地前，任何用户文档都不得声称流已认证或已加密。

接口细节、失败语义以及文档/门禁后果记录在 `docs/security-model.md` 第 10.1 节。该节是这项工作的 threat-model home，不另建文档。

## 后果

- 实现按计划顺序解阻；文档/门禁变化是协调的、单独评审的发布变更，因为当前 baseline 声明是机器固定的。
- **ADR 编号**：0004 和 0005 已被进行中的文档分区分支 `docs/zones-and-qpa-correctness`（`bc41df1`）占用，因此本记录使用 **0006**；其索引条目在候选线现有路径上加入 `docs/architecture.md`。
- 本记录不决定、留待后续：证书/私钥 provisioning 与 trust-store 管理、每客户端授权/审计细节，以及是否必须为不能做 VNC authentication 的 viewer 支持 TLS-only 部署。
