## CURRENT TRUTH (authoritative as of 2026-09-19 - the dated sections below are history)

- **The goal is V1.0.0.0 and the repository plan governs.** Plan artifacts must be read **at the candidate SHA**
  (`git show <sha>:<path>`, or a worktree at that SHA) - never from the shared working tree, which sits on an older
  branch. Reading it there already produced one wrong conclusion (corrected on #33 `5737340379`). Inbound requests are
  recorded and classified but must not act as scheduling input.
- **V1 mandatory set (machine, authoritative)**: `.github/release/v1-mandatory-issues.json`, pinned by
  `tests/release-readiness/check_release_authority_policy.cmake`, with #33/#106/#107/#95 all deferring to it -
  mandatory `9 30 31 32 33 39 41 57 101 104 107 109 143 144 157 158 159 162 163 164 165`,
  embedded-deferred `7 10 17 18`; `scope_authority` 157, `candidate_freeze_authority` 165.
- **Mandatory items with zero implementation**: **#143** authenticated/encrypted transport (plan sequence: design ->
  authentication -> encryption -> per-client authorization/audit), **#144** V1 B1/B2 negotiated/compressed encoding and
  **#9** x86 damage-aware/asynchronous capture, **#57** the Qt 5.15/LTS qualification **decision** (which may legitimately
  conclude Unsupported). These are the plan's own scope, not optional expansion.
- **#157 closed 2026-09-19** (`completed`) after all five acceptance criteria were verified against the real texts, not
  summaries - evidence `5737346831`, posted and read back before closing. **PR #168 merged** into the convergence line as
  `cc1adca`, where **all nine integration workflows pass**, checked at job and step level (Linux job's
  `Full deterministic CTest - Linux`, Windows job's `Configure build and CTest - Windows / MSVC x64`). At that same head
  all eleven release-readiness scripts pass locally: ten standalone, plus the parameterized `check_release_profile.cmake`
  with its seven registered cases (four of them "must be rejected").
- **CI trigger fact**: `v1-ga-acceptance.yml` fires only when the PR **base** is `develop`/`main`; the adapter workflows
  fire on `paths:` filters. `no checks reported` on a candidate-targeted PR is therefore expected - and must not be read
  as "nothing was wrong".
- **#109 physical acceptance**: Windows cells partly taken; explicitly owed are the held-input explicit stop and
  return-to-view-only step, the bounded slow-viewer observation, and the DPR interaction (this host is a single
  1920x1080 display at DPR 1.00). The **Linux column is blocked** on the VM's SSH key not being authorized
  (`Permission denied` with the key demonstrably offered, and no "no such identity" warning).
- **Roadmap `L126`**: no milestone is taggable while mandatory Windows/Linux evidence is unexecuted. #74 jobs with no
  runner/steps count as neither pass nor failure.
- **Instrument rules learned by failing in this session**: (a) GitHub issue comments come back **oldest-first** - read the
  last page, not the first; (b) `gh api` bodies must go through a JSON file with `--input`, never shell-built quoting
  (PowerShell array flattening produced duplicate posts, and `--jq` quoting silently broke verification); (c) the shell
  has fallen back from PowerShell to `cmd` mid-session, so prefer Python for anything non-trivial; (d) read the plan at
  the candidate SHA; (e) a gate that fails may be **missing arguments** rather than drifting - check your own call before
  blaming the tree; (f) verify a post by reading the stored body back, never by trusting the returned URL.
