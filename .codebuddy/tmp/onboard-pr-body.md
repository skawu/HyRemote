## Why

A product review of the consumer surface (measured, not opined) found the golden path already good - four lines of
C++, one public target, eleven lines of CMake - but two places where the surface was larger than the substance. This
PR fixes both, **without changing a single default**.

## 1. Maintainer switches are no longer consumer options

Ten switches were presented by every configure run: `BUILD_TESTS`, `BUILD_EXAMPLES`, `BUILD_CORE`,
`BUILD_REMOTE_ACCESS`, `BUILD_WIDGETS_ADAPTER`, `BUILD_QUICK_ADAPTER`, `WITH_VNC`, `BUILD_SPIKES`, `WITH_GBM`,
`WITH_RKMPP`. They stay in the cache (tests and CI drive them) but they are now `mark_as_advanced()`.

- **Measured**: `cmake -L` lists **six** `HYREMOTE_` options instead of sixteen; `cmake -LA` still lists all.
- The consumer-simplicity gate's own comment says a consumer "must not need to know internal component switches" -
  the visible surface now matches that intent, and the gate's verbatim option lines are untouched, so it still
  passes.
- The call sits at the end of the file deliberately: `mark_as_advanced()` does nothing for an option that has not
  been declared yet. The first attempt placed it with the options and the measurement caught that only half of them
  were hidden.

## 2. A thirty-line onboarding example

`examples/hello-remote`: the normal consumer path - construct with the window, `start()`, `stop()` - with none of the
acceptance instrumentation. The existing examples are release evidence and read like it: 131-274 lines each, mostly a
`READY` / `CLIENT_COUNT` / `POLICY_*` protocol plus an input probe, with five to fifteen lines of actual HyRemote use.

- Explicitly **outside** the E1-E6 matrix and marked as such in `examples/README.md`: it carries no release role and
  its success is not release evidence.
- Linked from `docs/getting-started/cpp.md`, so it is discoverable rather than another orphan directory.

## Evidence

- Builds with zero errors in a configure that has tests and examples on.
- **Runs**: started offscreen, alive after three seconds, **listening on `127.0.0.1:5921`**, and it prints
  `HyRemote is listening on 127.0.0.1:5921 - connect a VNC viewer, view-only.`
- Full suite green and **10/10** release gates with the option lines untouched.
- Two defects found by actually running things rather than assuming: `mark_as_advanced()` ordering (above), and an
  unflushed `std::cout` line lost when a scripted check stops the process - hence `std::endl`, with the reason in the
  code.

## Not in this PR

Deliberately left alone until the owner rules: adding change notifications to the C++ facade. That one is blocked by
`docs/v1-api-stability.md:68-70`, which freezes `RemoteAccess` as "non-copyable and movable" while a `QObject` base
class is not movable - so it needs a decision, not a patch.
