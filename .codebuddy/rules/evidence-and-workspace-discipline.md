---
description: 本地工作区洁净、tmp scratch 边界、证据可追溯与 fail-closed 规则。在落盘证据、探针、脚本、日志，或准备清理目录、检查 git status 时加载。
alwaysApply: false
enabled: true
paths:
  - "tmp/**"
  - "tools/perf/**"
  - "docs/internal/**"
---

# Evidence and workspace discipline

Root `AGENTS.md` remains the single execution entry. This rule narrows local
working habits that repeatedly caused defects.

- The only workspace is the repository root itself. Do not create worktrees,
  parallel build trees, or ad-hoc top-level directories. Everything not part of
  the product structure goes into the repository `tmp/` directory.
- `tmp/` is scratch only: evidence, probes, scripts and logs. It is excluded by
  `.git/info/exclude` and must never enter git. Do not add it to the tracked
  `.gitignore` without approval.
- Do not touch paths outside the workspace unless that exact path was
  explicitly authorised by Human. This includes cleanup.
- At the end of every turn, check `git status`, and that `main`/`develop` are
  in sync with `origin`. Report what was deleted and what was retained.
- Evidence is fail-closed: `NOT_RUN`, a red test, a missing sample window, or an
  absent event timeline is an evidence gap, never a PASS. Do not relabel
  invalid historical data as successful, and do not infer product status from
  chat memory or superseded PR prose.
- Never upload customer screen content, customer paths, device addresses or
  credentials. Keep evidence desensitised.
- Long-running local work needs a user-visible progress update at least every
  60 seconds (actual phase, command status, evidence, blocker, next action).
  This is a reporting rule, not a build timeout; do not kill a valid build for
  being silent.
