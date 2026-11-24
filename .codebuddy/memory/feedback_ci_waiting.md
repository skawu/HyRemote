---
name: 不要用固定长 sleep 等 CI
description: User rejects fixed multi-minute sleeps for GitHub CI; use event-driven watch/poll and exit as soon as checks finish
type: feedback
---

Never wait for CI with a fixed `Start-Sleep` of minutes. Use an event-driven command that returns when the checks finish, preferably `gh pr checks <number> --watch --interval 10`; for workflow runs use `gh run watch <run-id> --interval 10`. If polling manually, poll every 10–15 seconds, stop on the first non-pending state, and use a bounded timeout.

**Why:** a `Start-Sleep 200` kept the session idle after the Windows job had already passed, wasting time and making the agent look unresponsive.

**How to apply:** after opening or updating a PR, run `gh pr checks <n> --watch --interval 10` (add `--fail-fast` only when early failure is desired), inspect the final result, then merge. Do not substitute a long fixed sleep for watch mode.
