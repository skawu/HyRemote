---
name: 研发效率债务是优先级最高的流程问题
description: 用户明确指出的 CI 等待、测试冗余、examples 名不副实、过早冻结 release 等问题，已严重拖慢进度；后续必须优先修复而不是继续加功能
type: feedback
---

**规则：CI/测试/examples/发布冻结这四类工程效率债务，优先级高于新增功能；不要用“再加一个测试/再等一次 CI”掩盖流程问题。**

**Why:** 2026-10-19 的连续几轮中，用户指出：为验收反复弹 GUI、频繁同步等 CI、`tests/` 有 111 个 CTest（QPA 25、release 28，含重复和失效登记）、examples 被文档称为“成体系教程”但实际目录与任务不对应、release 分支在 Tool 重新定义前就版本化导致二次 re-freeze。这些都在直接拖慢整体研发进度。

**How to apply:**
- CI：本地 `build.cmd build --tests` + 目标 CTest 优先；每个 PR 只在 head 跑一次完整矩阵；WIP 用 Draft，合并/打 tag 前才查；不再 `Start-Sleep` 轮询。
- 测试：新增测试必须先映射到用户任务或发布合同；同一任务的多个 smoke 合并；QPA deploy-helper/release-profile 表驱动；删除失效/未注册/重复登记；目标 CTest 111→80 左右、CI 墙钟 -30~40%。
- Examples：按 #41 的用户任务模型收敛，先 inventory + 任务→示例→文档→测试矩阵，再删除重复；V0.3.0.0 期间不重构。
- 发布：scope/companion tool/文档测试稳定后才冻结 release 分支；冻结后不接受产品变更；避免 post-version correction。
- 可见 GUI 启动仍必须先征得同意。
- 上述修复作为 V0.3.0.0 之后的独立工程健康里程碑，优先于 V0.3.x 新功能。
