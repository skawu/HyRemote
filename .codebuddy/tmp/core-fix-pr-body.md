## What this PR actually is

Originally opened as "scope the allocation injection to the thread that armed it", on the finding that the injection was
process-wide while a Session runs worker threads. **That was right but incomplete**, and the honest state is:

1. **Thread-scoped injection** (`t_remainingAllocations` is `thread_local`) - keeps a worker's allocation from spending
   the injection and keeps a worker from being injected into at all. Verified: 200 consecutive runs, 0 failures, where
   the previous code failed 1 in 30.
2. **Three callback paths captured their event by value** (`session.cpp`, capture-event / input / transport-event), so
   the closure copy was materialised **before** `atCallbackBoundary()` was entered. An allocation failure there escapes
   into the backend stack - the exact thing the boundary exists to prevent - and it is what CI was reporting as
   "escaped the capture event boundary at allocation position 0". They capture by reference now, which is safe because
   the boundary invokes its callable synchronously.
3. **Still open: the frame path.** With both fixes in, the same binary still fails about **2 runs in 100** at
   `framesAccepted >= 1`: an allocation failure on the frame path can leave the session unable to accept the next
   frame. This PR therefore does **not** turn that test green, and the test's remaining failure is a Core containment
   question rather than a test artefact.

Evidence for each step is measured, not inferred from reading: the thread-scoping by 200 runs, the closure copy by CI
failing at position 0 on the same line after the first fix was compiled in, and the remaining frame-path failure by
100 local runs. Full suite: 50/50.

## What is asked

A ruling on how far the frame-path containment should go (make the frame path allocation-safe, or bound and document
what an allocation failure there may do), because that is a product-quality decision about what the boundary promises -
not a test detail.

## Notes

- No production behaviour changes beyond the three capture-by-reference fixes; the injection scoping is test-only.
- `AUTOMOC`/moc unrelated; the facade notifier work is a separate PR (#202).
