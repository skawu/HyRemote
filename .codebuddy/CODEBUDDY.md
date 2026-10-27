# CodeBuddy project entry (adapter only)

This file is the CodeBuddy-specific entry point, not a second source of project truth.

Before non-trivial work:
1. Read root `AGENTS.md` and `CONTRIBUTING.md`.
2. Read the target GitHub Issue and its latest authoritative comments; inspect current `develop`, overlapping open Issue/PR ownership, exact code, tests, and the relevant canonical docs.
3. Follow the current roadmap and machine release authority (including `.github/release/release-trains.json`) when release scope or compatibility is relevant. Current repository facts and exact-SHA evidence beat stale chat, branches, and historical prose.
4. Use `.codebuddy/skills/engineering-task-execution/SKILL.md` for non-trivial engineering work. The Skill is an execution procedure, not a replacement for repository authority.

Keep the existing architecture, branch, build, CI, security, and release rules. The canonical full-suite command is `build.cmd test` on Windows or `sh ./build.cmd test` on POSIX. Do not invent a parallel acceptance build path.

The root `AGENTS.md` already defines the Stop / Continuation Contract: continue all nonblocked work, do not treat `NOT_RUN`, a red test, or an OBSERVE CI result as a legal stop by itself, and stop only for a real Human decision, administrator action, credential, environment, or external blocker after automatable alternatives are exhausted. Preserve the useful progress update rule for long-running local work. Never claim a test, runtime, installed-consumer, viewer, platform, or release result that has not actually been evidenced.
