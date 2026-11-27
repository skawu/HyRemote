---
description: 本地工作区洁净、tmp 只放运行产物（不放可执行脚本）、脚本规划前置、编译中间产物输出目录、证据 fail-closed 规则。在落盘证据、日志，准备新建脚本、单文件编译或检查 git status 时加载。
alwaysApply: false
enabled: true
paths:
  - "tmp/**"
  - "tools/perf/**"
  - "tools/local-owner/**"
  - "tests/**"
  - ".github/scripts/**"
  - "cmake/**"
  - "src/**/tests/**"
  - "docs/internal/**"
---

# Evidence and workspace discipline

Root `AGENTS.md` remains the single execution entry. This rule narrows local
working habits that repeatedly caused defects.

- The only workspace is the repository root itself. Do not create worktrees,
  parallel build trees, or ad-hoc top-level directories. Run outputs (evidence,
  logs, generated data) go into the repository `tmp/` directory.
- **`tmp/` holds run outputs only, never executable scripts.** Scripts must be
  declared in the task contract before they are written and live in a tracked
  planned directory (`tests/`, `.github/scripts/`, `tools/perf/`,
  `tools/local-owner/`, `cmake/`); load the `script-planning` Skill. `tmp/` is
  excluded by `.git/info/exclude` and must never enter git. Do not add it to the
  tracked `.gitignore` without approval.
- A one-off single-file compile must set `/Fo:` and `/Fd:` into an output
  directory (or `pushd` there first). Without `/Fo`, `cl` emits `.obj` into the
  repository root. `*.obj` being ignored is not a fix.
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
