---
name: HyRemote PR 合并被 cancelled 检查项阻塞时的处置
description: 同一 SHA 上 PR review gate 因 concurrency 产生 cancelled 重复检查，会让 mergeStateStatus=BLOCKED；重跑被取消的运行即可，不得用 --admin 绕过
type: feedback
---

同一 commit 上若有同名检查项以 `cancelled` 结束（PR review gate 的 concurrency 会取消重复的 `synchronize` / `pull_request_review` 触发），GitHub 的 statusCheckRollup 判为 `FAILURE`，`gh pr merge --squash` 报 "base branch policy prohibits the merge"，即使同一 SHA 上已有同名 `success`。

**Why:** 2026-10-28 合并 #696 时，解决问题评论（reply + resolve thread）后又触发了额外 review-gate 运行，两个 duplicate 被取消；rollup 因此 FAILURE，实际产品/评审门禁全绿。

**How to apply:** 用 `gh api repos/<owner>/<repo>/actions/runs?branch=<branch>` 找出该 SHA 上 `conclusion=cancelled` 的同名运行，`gh run rerun <id>` 逐条重跑，确认 GraphQL `mergeStateStatus` 变 `CLEAN` 后再合并。绝不使用 `gh pr merge --admin` 或关闭必需检查来绕过；线程已 resolved、`p1-p2-disposition` 已 success 时才可重试合并。
