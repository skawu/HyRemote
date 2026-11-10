---
name: 每轮结束保持本地工作区干净
description: User requires a clean local Git workspace after every task and a cleanliness/sync audit after abnormal exits
type: feedback
---

After every task round, leave the local HyRemote checkout clean: no uncommitted tracked/untracked changes, one main worktree, temporary task worktrees removed, and local `main`/`develop` fast-forwarded to `origin`. On the first turn after an abnormal exit, run the same audit before doing new work.

**Why:** repeated per-task worktrees and a generated `hyremote-release-notes-fixtures/` directory accumulated in the root checkout; local `main`/`develop` stayed behind because only `origin/*` was fetched, and the main worktree was left on a stale merged feature branch.

**How to apply:** start with `git fetch --prune`, `git status --short --branch`, `git worktree list`, and compare `main`/`develop` with their upstreams. **Do not create Git worktrees.** Work only in the existing main checkout: create a task branch in place, commit/push it, merge or close the PR, switch back to `develop`, fast-forward `main`/`develop`, and delete the local task branch. End with `git status --porcelain` empty and `git worktree list` showing exactly one worktree. Also routinely clear `build/`, `tmp/`, and `.cache/` (all regenerable) between rounds; archive small physical evidence and release summaries outside the repo (for example `F:/workspace/hyremote/HyRemote-evidence/<date>/`) before deleting `tmp/`. Keep `.codebuddy/` limited to `memory/`; historical handoff, consumer, release, WIP and helper files do not belong there. Local historical branches and stashes do not need to mirror remote; report them rather than deleting without approval.
