---
name: 每轮结束保持本地工作区干净
description: Keep a clean local Git workspace after each task and run a cleanliness/sync audit after abnormal exits; only scoped, verified cleanup
type: feedback
---

After every task round, leave the local checkout clean: no uncommitted tracked or unexpected untracked changes, one main worktree, temporary task worktrees removed, and local `main`/`develop` fast-forwarded to `origin`. On the first turn after an abnormal exit, run the same audit before doing new work.

**Why:** repeated per-task worktrees and a generated `hyremote-release-notes-fixtures/` directory once accumulated in the root checkout; local `main`/`develop` stayed behind because only `origin/*` was fetched, and the main worktree was left on a stale merged feature branch. A later review also found that treating `build/` and `tmp/` as routinely disposable would delete retained physical evidence and current project assets, so the cleanup rule is scoped rather than automatic.

**How to apply:** start with `git fetch --prune`, `git status --short --branch`, `git worktree list`, and compare `main`/`develop` with their upstreams. **Do not create Git worktrees.** Work only in the existing main checkout: create a task branch in place, commit/push it, merge or close the PR, switch back to `develop`, fast-forward `main`/`develop`, and delete the local task branch. End with `git status --porcelain` empty and `git worktree list` showing exactly one worktree.

Cleanup is scoped and verified, never a routine end-of-turn action: preserve the configured `build/` identity and any retained evidence, verify what is regenerable before deleting, and report what was deleted and what was retained. `tmp/` is scratch and stays out of git; small physical evidence may be archived outside the repo before removal, but only after confirming it is no longer needed. Current tracked project assets (including `.codebuddy/` assets such as `rules/`, `skills/` and `memory/`) are not cleanup targets; only dated notes, local scratch and superseded indexes are. Local historical branches and stashes do not need to mirror remote; report them rather than deleting without approval.
