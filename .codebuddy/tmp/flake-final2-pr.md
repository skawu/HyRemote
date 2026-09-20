## What was actually wrong, and a correction of my own reporting

The CI failure is the three callback paths in `session.cpp` capturing their event **by value**, so the closure copy -
and the `std::string` message it carries - was materialised **before** `atCallbackBoundary()` was entered. An allocation
failure there escapes into the backend stack, which is exactly what the boundary exists to prevent.

**Correction**: I reported earlier that this fix had not worked, based on a CI run that failed in the same place. That
run did not contain the fix. It had been committed to the wrong branch and only the mailbox fix from the same batch was
carried across. The evidence that identifies the cause was there all along and matches it exactly:

```
DIAG-CAPTURE position=0 what=std::bad_alloc state=2(Running)
             captureEvents=0 nonRecoverable=0 accepted=0 ignoredAfterStop=0
```

Every Core counter is zero - Core never saw the event, so the failing allocation happened before the boundary was
entered. With a by-value closure copy that is precisely what happens: the copy is made while building the callable, one
step before the boundary. The counters cannot move because the boundary has not run yet.

A warm-up delivery was added on the strength of a weaker reading of that line and is **reverted here**: CI still failed
in the same place with it, so it explained nothing and is not kept.

## The four things this branch now contains

| # | Change | Why |
| --- | --- | --- |
| 1 | `t_remainingAllocations` is `thread_local` | a Session runs worker threads; a process-wide counter let a worker spend the injection, and let a worker be injected into |
| 2 | capture-event / input / transport-event callbacks capture by reference | the by-value copy allocates before the boundary - the CI failure above. Safe because the boundary invokes its callable synchronously |
| 3 | `Mailbox::push` keeps the displaced frame until the new one is stored | DropOldest released the oldest first, so a failed store left the mailbox smaller than before the call - a weak post-state and a silent shrink of the bounded pipeline |
| 4 | `#include <iostream>` documented as required | the check macro prints to `std::cerr`; mingw picks it up transitively, MSVC does not, so removing it broke the Windows jobs |

## Evidence

- **150 consecutive runs** of the binary: 0 failures, and 500 runs before the warm-up was removed.
- Full suite **50/50**, ten release gates.
- The CI diagnostic above is the only direct evidence of the cause; the rest is measured stability.

**Scope of the claim**: 0 in 150 is below this host's detection floor, not proof of absence, and the test is inherently
position-sensitive - it arms an exact allocation count, so any code change moves what a position measures. The Linux CI
job is the arbiter, and it fails deterministically without change 2 and has no reason to after it.

## Process note

Two commits went to the wrong branch during this investigation (this fix and the mailbox fix); the mailbox one was
carried over immediately, this one was not, and the gap produced a wrong conclusion that I reported before checking
which build CI had actually tested. Both are now on the right branch, and the notifier PR carries a visible revert
rather than a force-push.
