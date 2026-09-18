# Acceptance evidence

This directory holds **recorded evidence**, one directory per candidate. Evidence is documentation: it is what a
reviewer reads before a status is called accepted. It is **not test code** and never participates in the test graph -
the deterministic suites live under `tests/`, and the repository-layout authority states the boundary.

The runbook that produces the physical evidence, and the template for a record, is
[`../v1-physical-acceptance.md`](../v1-physical-acceptance.md). That runbook is itself preparation only:
it does not authorize or prove acceptance, and the acceptance authority is the tracking issue, not this directory.

## Layout

```text
docs/acceptance/
└─ <candidate-sha-or-version>/
   ├─ windows-x86_64.md      records taken on a Windows host
   ├─ linux-x86_64.md        records taken on a Linux host
   └─ artifacts/             screenshots, logs and other referenced captures (not edited by hand)
```

## What a record must state

Every claim in a record is only worth what its environment says, so a record states at least:

- **candidate commit SHA** and the release line it belongs to;
- **operating system and architecture**, and whether the display/input stack is physical or hosted;
- **exact toolchain**: Qt version and kit, compiler, CMake generator, build type;
- **the unit under observation** (which example/application and which integration mode);
- **what was done**, step by step, including how an input was produced (a real local key event is not the same
  evidence as a posted window message, and the record says which one was used);
- **the observation itself** - the application's own output, or a directly measured fact such as a frame difference,
  a window count, or a refused connection - quoted rather than summarized;
- **accepted weaknesses**: what the instrument cannot decide. Frame evidence cannot show whether an application
  retained a synthetic press; a hosted display cannot stand in for a physical one; an offscreen or Xvfb run does not
  count for local display/input coexistence;
- **who reviewed it and what was decided**, with the decision living on the tracking issue rather than here.

## Rules

- One directory per candidate; do not append to another candidate's directory.
- Evidence is copied out of the tracking issue when it is settled, so the repository keeps a versioned record rather
  than a chat-only one.
- A record never upgrades a status by itself. `docs/reference/compatibility.md` changes only in the change that lands
  a capability, and only after the evidence has been reviewed.
- Non-executed or infrastructure-failed jobs are recorded as **unexecuted**, never as pass and never as code failure.
