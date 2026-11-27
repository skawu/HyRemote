---
name: 禁止操作工作空间外目录
description: 红线：唯一工作空间是 F:\workspace\hyremote\HyRemote；外部路径一律不得访问或清理
type: feedback
---

不得在 `F:\workspace\hyremote\HyRemote` 之外创建、构建、写入、读取或清理任何目录/文件，除非用户对该具体路径给出明确授权。

**Why:** 用户已明确“工作空间只有一个 `F:\workspace\hyremote\HyRemote`，其余都是非法目录”。此前使用 `F:\workspace\hyremote\build` 属于越界；不得再用“清理”作为再次访问外部路径的理由。

**How to apply:** 所有构建、测试、安装、证据和临时文件都放在仓库内唯一 `build` 目录。命令先 `cd` 到仓库根；不要 `Get-ChildItem`、`rd`、`Remove-Item` 或写入仓库外路径。需要外部工具/路径时只调用其可执行文件，不操作其目录内容。
