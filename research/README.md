# `research/` — non-product research evidence

## What this directory is

`research/` holds **non-product architecture evidence**: what was measured, and what was decided from it. Nothing here
is built, installed, packaged or covered by the V1 API stability contract.

- **Nothing under `research/` is part of any build graph.** The root build does not include it, and the
  developer-only spike switch that used to exist was removed with the harnesses it gated
  (`tests/release-readiness/check_repository_layout.cmake` now fails the build if either comes back).
- **No CI workflow builds or runs anything here.**
- **No product or integration module may reference it** - the layout gate forbids `src/core`, `src/remoteaccess`,
  `src/qml/HyRemote` and `src/qpa` from doing so.
- The authoritative statement of its role is `docs/internal/repository-layout.md`.

## What is left here

| Directory | What it is | Decision status |
| --- | --- | --- |
| `vnc-transport-rust-ffi/` | The Rust FFI transport evaluation (#34/#38): a shim exercising `rustvncserver` through HyRemote's own opaque C ABI, plus an interoperability probe. It has no root-build entry and no CI workflow - the two workflows it once had were retired. | **Evaluated and not adopted for V1**; superseded by the bounded C++ RFB transport. Recorded in `docs/internal/x86-vnc-transport-evaluation.md` and `docs/internal/neatvnc-evaluation.md`. |

## What was removed, and why

The two capture harnesses (`research/capture/` from SPIKE-01 and `research/async-capture/` from SPIKE-02) were
**retired** once the capture architecture decision they produced had been implemented in the product:

- their own READMEs described them as throwaway, non-production code that was *expected to be deleted once the capture
  architecture decision is implemented for real*;
- the decision is implemented: Widgets capture (`docs/widgets-capture.md`) and Quick capture
  (`docs/quick-capture.md`), with the ownership/timestamp/backpressure rules frozen in ADR-0001, ADR-0002 and ADR-0003;
- every conclusion they produced is already recorded in `docs/internal/capture-spike.md` and
  `docs/internal/async-capture-spike.md`.

They were removed in the commit whose parent is **`3e6e191`**. The harness source is still in history and can be
recovered without a branch or a tag:

```text
git show 3e6e191:research/capture/README.md
git log --diff-filter=D --name-only -- research/capture     # the removal commit for each file
```

Keeping the repository free of a second, never-built build graph is the point: `research/` is evidence a reviewer can
read, not a tree that has to keep compiling.
