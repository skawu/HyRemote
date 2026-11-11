---
description: 每轮会话的固定开工/收尾动作，用于防止重复盘点、注意力漂移和遗漏记忆更新。
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

**During:** stay on that one item until it is done or genuinely blocked. A new
interesting finding is recorded as evidence, not a new parallel investigation.
Report phase/evidence at least every 60 s.

**End (three checks):**
1. `git status`; report what changed, what was deleted, what was retained.
2. If the Human corrected an approach or confirmed a non-obvious judgment,
   update durable memory now (`memory/` + `MEMORY.md` index), not next session.
3. Mirror `memory/` to the IDE runtime copy; verify index/topic counts agree.

Never put PR numbers, CI run ids, SHAs, dated logs or device addresses into
durable memory. Those belong in git, `tmp/` or archive. Current status always
comes from the repository, never from a memory hook.
