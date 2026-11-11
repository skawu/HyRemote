# HyRemote CodeBuddy project assets

These files are **project property**, tracked in git and shared with every
contributor and agent that opens this repository. Root `AGENTS.md` remains the
single execution entry and outranks everything here; nothing in this directory
may redefine product scope, architecture, release authority or acceptance gates.

## Layout

| Path | Loaded by CodeBuddy | Purpose |
|---|---|---|
| `AGENTS.md` (repo root) | yes, as project guidance | authority order, architecture invariants, stop/continuation contract |
| `.codebuddy/rules/*.md` | **injected by the client**, `alwaysApply: true` every turn or conditional by `paths` | small operating constraints; an injection layer, not a hard block |
| `.codebuddy/skills/<name>/SKILL.md` | yes, project-level Skills | on-demand detailed procedures; no context cost when unused |
| `.codebuddy/memory/MEMORY.md` + topic files | this directory is the versioned copy | durable long-term memory |
| `memory/archive/legacy-durable-index.md` | in git, not indexed | superseded 2026-09 durable index, kept for provenance |
| `memory/20??-??-*.md`, `daily/`, `archive/MEMORY-2026-09-19-pre-distill.md` | **excluded from git** | per-task daily chronicles only |

## Rules

Markdown with YAML frontmatter. `description` states when the rule applies;
`alwaysApply: false` + `paths` makes it a conditional rule so it is injected
only when the touched files match, instead of consuming context on every turn.
Per CodeBuddy docs, project-level Rules load only from the current workspace's
`.codebuddy/rules/` directory, never from a parent directory.

| Rule | Applies when |
|---|---|
| `session-anchor.md` | **every turn** (`alwaysApply: true`), deliberately short |
| `build-and-test-authority.md` | touching `build.cmd`, `build.yml`, CMake files or CI workflows |
| `evidence-and-workspace-discipline.md` | writing evidence/logs, creating scripts, one-off compiles, `git status` |
| `test-governance.md` | adding/modifying/executing tests, physical scenarios or CI waits |

Exactly one rule is always-on. It is short on purpose: a long always-on rule
recreates the context dilution it is meant to fix. The other three are
conditional so they cost no context on unrelated turns. Broad generic wording
without an identified, previously observed failure mode is not accepted.

### Why the enforcement lives here, not in a document

A rule in `docs/` or in root `AGENTS.md` only works if the current session
happens to read it. Project-level Rules with `alwaysApply: true` are injected by
the client into **every** session on this checkout, without a read decision,
and project-level Skills are discovered from `.codebuddy/skills/`. That is the
only mechanism layer used here. The design deliberately does **not** touch
user-level `settings.json`, plugin enablement, native hook configuration, hook
scripts or interpreter paths — those are client internals whose failure would
affect normal CodeBuddy operation.

`session-anchor.md` carries the short gates (script contract before any new
script, three hard rules, end-of-turn verification); `script-planning/SKILL.md`
carries the full procedure on demand. Client-side injection is a soft
constraint, so merge-level enforcement is handled separately below.

## Merge-level enforcement (machine, not memory)

`tests/release-readiness/check_script_discipline.cmake` is registered in the
`facts-layout` group of `tests/release-readiness/run_grouped_checks.cmake`. It
runs in the Qt-free `authority-governance` lane; `ci-merge-verdict` requires that
lane to succeed, and the protected develop ruleset requires `ci-merge-verdict`.
No client setting, plugin or hook is involved.

The guard fails closed when:

- the anchor, the workspace rule or the `script-planning` Skill is removed, or
  the anchor loses `alwaysApply: true` / the `tmp/` script gate;
- any tracked executable script lives outside the planned directories
  (`tmp/`, the repository root and `.codebuddy/` are always rejected);
- the guard is no longer registered in the grouped runner.

`.github/scripts/resolve-ci-scope.py` turns governance on for any PR that adds
or edits an executable script, changes `.codebuddy/` assets, or edits the guard
or its registration. This is additive: it only enables the cheap governance lane
and never changes product lane selection. Both the guard and the classifier
carry self-tests with positive and negative cases.

Remaining limit: the guard sees tracked files, so an untracked scratch script
that never enters a PR is invisible to it. Client-side Rules cover that case by
injection, but only as a soft constraint.

### Why `session-anchor.md` exists

Measured failure modes in this repository: a 30 KB dated ledger inside durable
memory whose opening line still named a superseded development target; an index
hook that asserted version state contradicting `AGENTS.md`; repeated full
re-inventory of `tmp/` (2028 files / 1.59 GB) on unrelated turns; and durable
memories written only after the session had ended. The anchor fixes the process,
not the facts: facts stay in `AGENTS.md`, git and Issues.

## Skills

Discovery entry is `.codebuddy/skills/<name>/SKILL.md`. On this client the
`.agents/skills/` path is **not** discovered (measured 2026-10-27, see
`memory/project_codebuddy_skill_discovery.md`). `engineering-task-execution`
is the project-owned Skill restored from commit `42cf41e` after `9078c1e`
moved it to the non-discoverable path. Its debugging-discipline section is
adapted from `obra/superpowers` `systematic-debugging` v7.0.1 (MIT).

`script-planning` is the second project Skill. It is the detailed procedure
behind the script-contract gate in `session-anchor.md`: contract before file,
planned tracked directories only, parameterise instead of copying, `/Fo` for
one-off compiles, and an end-of-turn verification list.

The Spec Kit pilot Skills (`speckit-*`) are intentionally **not** restored:
their pilot conclusion recorded unverified productivity gain and a missing
canonical gate (#687/#590). Do not copy an asset into a path the client cannot
load; that creates the appearance of capability without the capability.

## Long-term memory

`memory/MEMORY.md` is the index: one line per topic file, each with a one-line
hook. Topic files use frontmatter with `name`, `description` and one of the
types `user`, `feedback`, `project`, `reference`. For `feedback` and `project`
types, lead with the rule/fact, then **Why:** and **How to apply:** lines.

Maintenance rules:

- **Durable only.** Owner conventions, validated decisions, feedback with a
  reason, and pointers to external systems belong here. Per-task chronicles,
  transient state, in-progress work and current conversation context do not.
- **No duplication.** Do not store code patterns, architecture, file paths,
  git history, debugging recipes, or anything already in `AGENTS.md`,
  `.codebuddy/rules/` or the current code.
- **One entry per topic.** Update an existing topic file instead of creating a
  near-duplicate; keep `MEMORY.md` in sync (index line count equals topic file
  count). Remove or archive entries that turn out to be wrong or stale.
- **Verify before recommending.** A memory naming a file, symbol or flag only
  claims it existed when written; confirm against the current repository first.
- **Keep it desensitised.** No credentials, customer paths, device addresses,
  business screen content or personal data.
- **Write memory immediately, not next session.** The trigger is a Human
  correction of an approach, or a Human confirming a non-obvious judgment call
  (not only explicit "remember this"). Also record durable project context:
  who is doing what, why, and by when — with relative dates converted to
  absolute. Do not record code patterns, architecture, paths, git history,
  debugging recipes, ephemeral task state, or anything already in `AGENTS.md`.
- **Two copies, one source of truth.** This directory is the versioned,
  reviewable project asset. The IDE also keeps a runtime copy under
  `%USERPROFILE%/.codebuddy/projects/<workspace-id>/memory/`, which is what is
  auto-loaded into agent context. After editing durable memory here, mirror it
  and reconcile; when the two disagree, prefer the git-tracked copy.
  One-shot sync from the repository root (PowerShell):

  ```powershell
  $src = '.codebuddy/memory'; $dst = Join-Path $env:USERPROFILE '.codebuddy/projects/f-workspace-hyremote-HyRemote/memory'
  Get-ChildItem -LiteralPath $src -File -Filter '*.md' | Copy-Item -Destination $dst -Force
  ```

  Then verify both indexes match their topic files (19 hooks / 19 topics as of
  2026-11-10). Do not automate this as a hook or background task.
- **Daily notes are never committed.** `.codebuddy/.gitignore` excludes dated
  files, `daily/`, the 160 KB pre-distill chronicle dump, plus local scratch.
  They remain usable on this machine only. The superseded durable index in
  `archive/` is intentionally kept in git as provenance, but is not indexed by
  `MEMORY.md` and must not be treated as current.

## Not restored / not added

- `.specify/` (Spec Kit machine-local scaffolding) was removed 2026-11-10 as
  unused; its pilot branches retain the historical artefacts.
- `CODEBUDDY.md` is not created: commit `42cf41e` established `AGENTS.md` as
  the single CodeBuddy entry, and a second entry would create conflicting
  authority.
