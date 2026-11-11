---
name: 只允许一个仓库内 build 目录
description: 红线：唯一工作空间是 F:\workspace\hyremote\HyRemote；只允许该工作空间内的一个 build 目录
type: feedback
---

用户明确：唯一合法工作空间是 `F:\workspace\hyremote\HyRemote`，工作空间外路径不得操作。该工作空间内只允许一个构建目录：

```text
F:\workspace\hyremote\HyRemote\build
```

禁止 `build-*`、`qt5-*`、`qt6-*`、`tool-*`、`alpha-*`、`core-*` 等并行构建目录，也禁止 worktree 规避。

**Why:** 此前我在 `F:\workspace\hyremote\build\HyRemote` 下保留多个临时构建目录，违反了“只允许一个 build”的红线；用户随后明确工作空间只有仓库本身，外部路径均非法。旧记忆“仓库内 build 必须为零”已被该更新决定取代。

**How to apply:** 所有 CMake/CTest 只使用 `F:\workspace\hyremote\HyRemote\build`；切换 Qt Kit 时清空并重建同一目录，不另建并行目录。任务结束前确认工作空间外没有本轮**产物**（只读查看是允许的，写入外部一律拒绝）、仓库内除该 `build` 外没有其他 `build*`。
