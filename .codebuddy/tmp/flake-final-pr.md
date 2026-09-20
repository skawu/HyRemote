## The answer CI gave, and what was fixed on the way

`hyremote-core-test-callback-exception-boundary` was intermittently red on branches that do not touch `src/core`
(#198, #199). It is green with this PR, and the investigation found **four** separate things, only one of which was
the original reported symptom.

### 1. The remaining CI failure was never the product's - it was the test's own prologue

A temporary diagnostic on the capture-event case printed, on CI, with the countdown armed at position 0:

```
DIAG-CAPTURE position=0 what=std::bad_alloc state=2(Running)
             captureEvents=0 nonRecoverable=0 accepted=0 ignoredAfterStop=0
```

**Every Core counter is zero**: Core never saw the event, so the allocation that failed happened **before the boundary
was entered**. The test's premise - "position 0 is the callback's own first allocation" - does not hold on that
toolchain, where the first delivery performs one-time work. Both cases now make one **warm-up delivery outside the
armed window**, so position 0 means what the sweep claims it means. That is why the failure was CI-only and
unreproducible here (same configure flags, different toolchain).

### 2. The injection was process-wide while a Session runs worker threads

`t_remainingAllocations` is `thread_local` now: a worker's allocation can no longer spend the injection, and a worker
can no longer be injected into at all. Measured: 200 consecutive runs, 0 failures, against 1 in 30 before.

### 3. Three callback paths copied their event before entering the boundary

`session.cpp` capture-event / input / transport-event callbacks captured their event **by value**, so the closure copy -
and the `std::string` message it carries - was materialised **before** `atCallbackBoundary()`. An allocation failure
there escapes into the backend stack, which is exactly what the boundary exists to prevent. They capture by reference
now; the boundary invokes its callable synchronously, so that is safe. Real defect, independently of §1.

### 4. `Mailbox::push` could leave the mailbox holding less than before the call

The DropOldest path released the oldest frame **before** storing its replacement, so a `std::bad_alloc` from storing
left the mailbox smaller than it was - a weak post-state instead of a strong one, and a silent shrink of the bounded
pipeline. The displaced frame is held until the new one is queued and is put back on failure; a failed push no longer
consumes a FrameId.

## Evidence

| Claim | Instrument |
| --- | --- |
| The CI failure is pre-boundary | The CI diagnostic above: all Core counters zero, session `Running` |
| Thread scoping works | 200 consecutive runs, 0 failures (was 1 in 30) |
| The frame path is stable | 500 runs before the warm-up, 200 after: 0 failures |
| Nothing else regressed | Full suite 50/50, ten release gates |

**Honest scope of the last two rows**: 0 in 500 is below this host's detection floor (p < 0.6% at 95%), not proof of
absence, and the test is inherently position-sensitive - it arms an exact allocation count, so any code change moves
what each position measures. CI across platforms is the only real judge, and CI is green on this branch.

## Process note

The temporary diagnostic was committed deliberately (`3710707`, `cb82017`) so CI would print the stage facts instead
of my guessing, and it is **removed** in this PR rather than left behind. Reading it took two cycles because the first
probe watched the frame case while CI was failing in the capture-event case - the cost of aiming a probe before knowing
which side fails.
