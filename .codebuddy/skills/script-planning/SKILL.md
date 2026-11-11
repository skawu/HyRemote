---
name: script-planning
description: Use during task planning whenever the work may need an executable script, build driver, probe, analyser or diagnostic helper. Requires declaring the script contract before writing any file, and routes scripts to tracked planned directories instead of throwaway scratch files.
---

# Script planning

Root `AGENTS.md` and the canonical build system remain the authority. This Skill
governs only how executable scripts are introduced into the repository.

## 1. Why this exists

Measured failure modes in this repository (2026-11): 60 scripts appeared under
`tmp/` in one month, all written during task execution rather than planned;
five near-identical `build_fgN.cmd` / `run_fgN.cmd` pairs were copies of one
probe; 15 stray `*.obj` landed in the repository root because one-off `cl`
invocations omitted `/Fo`. `tmp/` is excluded from git, so none of this was
visible to review, and `git status` stayed clean while the scratch accumulated
to 1.59 GB. A document-only reminder did not prevent it, and one tracked rule
was even written to *permit* scripts under `tmp/`.

## 2. The gate: contract before file

Before creating or editing any executable script, record in the task contract
(todo list entry, working plan or PR description):

- name and exact tracked path;
- purpose and the specific evidence/decision it supports;
- inputs, outputs, and where outputs may be written;
- which existing planned script or build entry was considered and why it cannot
  be reused;
- how it is registered in the build/CI system (CTest name, CMake target,
  workflow step, or an explicit `build.cmd` invocation);
- expected execution count and PASS/FAIL criteria.

**No contract entry means no new script.** If a genuinely unforeseen need
appears mid-task, stop and add the contract entry first, then write the script.
Never create a script "just to try something" and delete it afterwards.

## 3. Where scripts belong

Tracked, planned directories only:

| Path | For |
|---|---|
| `build.cmd`, `build.yml`, `cmake/*.cmake` | canonical build configuration |
| `tests/**` | registered CTest cases and acceptance checks |
| `.github/scripts/**` | CI governance and policy guards |
| `tools/perf/**`, `tools/local-owner/**` | planned, reusable measurement/analysis tools |

`tmp/` holds **run outputs only**: evidence, logs, generated data and captured
screenshots. It never holds executable scripts. Scripts that are needed again
belong in a tracked directory with a name, a self-test and an owner.

Prefer reuse over creation. The repository already has
`tools/perf/aggregate_cpu_probe.py`, `tools/perf/assess_quick_public_profile.py`,
`.github/scripts/resolve-ci-scope.py` (with `--self-test` positive and negative
cases), and the grouped readiness checks in `tests/release-readiness/`.
`tests/EXECUTION_MANUAL.md` records the precedent: 15 former scripts were
consolidated into two grouped CTests, with no script deleted.

## 4. Parameterise instead of copying

One script with an explicit `--case` / `--profile` argument replaces a family of
near-duplicate files. Copying a working probe and changing one constant is a
defect: it leaves no single owner, no self-test and no registration.

Every new script needs a deterministic self-test with a positive and a negative
case, following the `.github/scripts/resolve-ci-scope.py --self-test` pattern.

## 5. One-off compiler invocations

All build operations go through the canonical build system. When a bounded
single-file compile is genuinely required, set the intermediate output paths
explicitly:

```bat
cl ... "%SRC%\probe.cpp" /Fe:"%OUT%\probe.exe" /Fo:"%OUT%\\" /Fd:"%OUT%\\"
```

or `pushd` into the output directory first. Without `/Fo`, `cl` writes the
`.obj` into the current working directory, which is the repository root.
Never rely on `.gitignore` (`*.obj`) to hide this: ignore rules suppress the
symptom, not the file.

## 6. End-of-turn verification

1. Every script touched this turn appears in the task contract.
2. Every script is under a planned tracked directory; `tmp/**/*.{py,ps1,cmd,sh}`
   has no new entries.
3. No new `*.obj` / `*.pdb` in the repository root.
4. `git status` is clean except for intended tracked changes.
5. Report what was created, what was reused and what was deliberately not run.

## 7. Limits

This is a client-level mechanism: project Rules are injected by CodeBuddy and
this Skill is loadable on demand. It shapes every session on this checkout, but
it is not a server-side merge gate and cannot physically prevent a shell command
issued outside the client. Cross-contributor enforcement additionally belongs
to the CI governance lane and the protected-branch ruleset.
