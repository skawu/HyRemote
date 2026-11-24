---
description: 每轮会话的固定开工/收尾动作：脚本规划前置、任务单任务、证据落盘、记忆更新与工作区洁净。
alwaysApply: true
enabled: true
---

# Session anchor

Keep this short on purpose. Root `AGENTS.md` is the authority; this is only a
checklist, not a second entry or a second set of rules.

**Start (once per task, not once per message):**
1. Read the target Issue / latest authoritative comment and `AGENTS.md`'s
   product-phase snapshot. Do not re-read the whole `AGENTS.md` mid-task.
2. Skim `memory/MEMORY.md` hooks; open only the topic files whose hook matches.
   Never load all topics "just in case".
3. Write the task contract into the todo list. Keep exactly one in-progress item.
   If the task needs an executable script, fix its name, purpose, inputs, outputs
   and tracked home **in that contract before writing any file**; load the
   `script-planning` Skill (detailed procedure). No contract entry, no new script.

**Three hard rules (apply even if the Skill was not loaded):**
- Never create a script under `tmp/`. `tmp/` holds run outputs only.
- Every executable script must be tracked in a planned directory
  (`tests/`, `.github/scripts/`, `tools/perf/`, `tools/local-owner/`, `cmake/`,
  `src/**/tests/`, `docs/`) and declared in the task contract.
- A one-off `cl` compile must set `/Fo:` and `/Fd:` into an output directory;
  otherwise `.obj` lands in the repository root. `*.obj` being ignored is not
  a fix.

**During:** stay on that one item until it is done or genuinely blocked. A new
interesting finding is recorded as evidence, not a new parallel investigation.
Report phase/evidence at least every 60 s.

**End (four checks):**
1. `git status`; report what changed, what was deleted, what was retained.
2. Check that every script created this turn appears in the task contract and is
   tracked in a planned directory; `tmp/` holds run outputs only, never scripts.
3. If the Human corrected an approach or confirmed a non-obvious judgment,
   update durable memory now (`memory/` + `MEMORY.md` index), not next session.
4. Mirror `memory/` to the IDE runtime copy; verify index/topic counts agree.

Never put PR numbers, CI run ids, SHAs, dated logs or device addresses into
durable memory. Those belong in git, `tmp/` or archive. Current status always
comes from the repository, never from a memory hook.
