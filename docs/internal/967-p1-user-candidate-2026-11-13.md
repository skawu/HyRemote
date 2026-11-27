# #967 P1 — single user candidate (CORRECTED): initial zero-viewer lifecycle fix only

Date: 2026-11-13, revised after [PR #997 review 5891712978](https://github.com/skawu/HyRemote/pull/997#pullrequestreview-5891712978)
Base: `develop@0f5e8333c27556e25d8cbbec98b2bc543532d7ab` (#995 + #996 merged)
Status: source-level correctness fix with existing-CTest evidence. **No performance
improvement is claimed.** Physical-device and Viewer evidence remains
`NOT_RUN_OWNER_DEPENDENCY`.

`SOURCE_SHA` is the exact final head of PR #997 and is reported in the PR
description / review reply (GitHub is the single continuation anchor; a file
cannot contain the hash of the commit that contains it). All other candidate
identity below is content-addressed and fixed.

## 1. Decision and corrected scope

```text
DEFAULT_ROUTE_OPTIMIZATION = NO_PRODUCT_PATCH_JUSTIFIED (WITHDRAWN)
PR #997 PRODUCT CHANGE      = initial zero-viewer SDK-owned resource release only
#789_CPU                    = NOT_MEASURED; no Host CPU improvement claimed
#567_VIEWER                 = NOT_MEASURED; RFB/wire/H2/Raw/TRLE untouched
#790_RETAINED_BYTES         = source-level correctness fix at initial 0 viewers;
                              field RSS/PSS NOT_MEASURED
PRODUCT_GO                  = NO
```

The earlier claim that the default public Qt route "removes repeated whole-frame
encoding/sending" was wrong and has been retracted. The byte-identical frame
reuse implementation and all of its assertions were removed from this PR;
`quick_target.cpp` and `test_quick_capture.cpp` are restored exactly to the
`develop` baseline. See `docs/internal/967-p1-design-freeze-2026-11-13.md` §2-3
for the source evidence and the insufficient, `UNSTABLE_BASELINE` probe result.

## 2. Required source manifest

Content hashes are `git hash-object` blobs of the source files at `SOURCE_SHA`:

| File | Blob |
| --- | --- |
| `src/runtime/src/detail/viewer_capture_demand.hpp` | `7bebf0163c441e8a0ee64bc6d682dcf46496b07f` |
| `src/runtime/tests/test_viewer_capture_demand.cpp` | `0d5685795d74c67699cd3179e1ee51686beff2c1` |
| `src/runtime/tests/access_instance_notifications_test.cpp` | `fde11797dd2f5a2ca48b10cf3b49a3c43ac1a24b` |
| `docs/internal/967-p1-design-freeze-2026-11-13.md` | `006f9d9eee12d3234bf7f26a1cc22e10a9d42439` |

Also required: this candidate package document. **No new product header, no
new cache, no new CaptureSource hook, no RFB/transport/watermark/native-provider
change.** `quick_frame_reuse.hpp` does not exist in this candidate.

## 3. Target Kit, route and graphics backend

- Windows x86_64, Qt 6.8.3 `msvc2022_64` local canonical profile; Qt 5.15.2
  covered by the existing exact-head PR CI lane.
- Route: public Qt `grabToImage()` fallback
  (`HYREMOTE_EXPERIMENTAL_QUICK_CAPTURE=PUBLIC_QT_FALLBACK`). Native GL PBO and
  D3D11 staging remain opt-in / default OFF; no automatic native preference.
- The fix lives in the **Shared Runtime demand gate**, so it is common to
  C++/QML/Generic/QPA and every Qt graphics backend. It contains no Qt API,
  no backend capability decision and no per-device branch.
- Qualification truth is unchanged: Qt 5.15.2 DirectGL / Qt 6.8.3 desktop GL
  small-target real-GPU PASS (native OFF), Windows D3D11 static-1V CPU/private
  bytes gain with viewer latency `NOT_QUALIFIED`, Qt 6 GLES/EGLFS, redirected
  FBO, high DPI, multi-GPU and other RHI `NOT_QUALIFIED` on the public fallback.
  Windows results are not extrapolated to ARM64.

## 4. Install and rollback identity

- Install: `build.cmd build` / `build.cmd install` at `SOURCE_SHA` with the same
  `build.yml` profile and the configured workspace `build` tree. No public SDK
  header, ABI, configuration file, schema or Viewer-side change.
- Rollback: revert PR #997 (or `git revert` its commits) and rebuild with the
  same entry; the previous product baseline is `develop@0f5e8333`. No data
  migration or persisted state is introduced.
- Release delta: `release/v0.3.5.0@b0680d5df4f76d8b84b02a1b950030a65a89ddca`
  and the OPEN selective backport PR #876 (`f5971906`, five files) do not
  contain this change. The whitelist is not widened, no full `develop`
  backmerge, no admin bypass, no topology-check or independent-App ruleset
  bypass. `develop` merge is not release qualification; #95 owns that gate.

## 5. What the candidate actually changes

`ViewerCaptureDemand::start()` begins at `m_viewers = 0` and
`waitForCapture()` deliberately admits ONE clientless bootstrap capture so RFB
can complete ServerInit for a first client. That frame can fill a
`CaptureSource` idle cache (GL PBO, D3D11 staging, composite). Because no
`>0 -> 0` transition ever occurred, `setViewerCount()` never latched an
`IdleRelease`, so those SDK-owned bytes stayed pinned for the whole run when no
Viewer ever connected — a real #996 invariant hole, independent of the
withdrawn optimization.

`frameQueued()` now latches the same one-shot `m_idleReleasePending` when a
usable clientless bootstrap frame is queued. `waitForCapture()` delivers it on
the capture scheduler thread, and `ObservedCaptureSource::requestFrame()` calls
`CaptureSource::releaseIdleResources()` for every backend. The transport has
already recorded the ServerInit geometry, so the first handshake is unchanged;
an early reconnect drops the stale edge; a later reconnect gets fresh pixels.
No per-backend viewer counter, timer or platform branch is added.

This is a **correctness fix**, not a CPU optimization. It does not change wire
bytes, damage, encoding, capture pacing, or any visible pixel.

## 6. Verification and honest gaps

- Existing CTest targets only, canonical `build.cmd test` with bounded filters:
  `viewer-capture-demand` 1/1, `runtime-notifications` 1/1,
  `hyremote-quick-capture` 3/3 (including both DPR variants).
- New assertions cover start -> bootstrap -> 0V idle, 0->1->0, late
  callback/reconnect, unusable bootstrap geometry, and the end-to-end
  `idleCaptureReleases` sequence. The old assertion "a listener that has never
  served a Viewer is not a 0-viewer edge" encoded the defect and was replaced.
- Exact-head PR CI on the existing Qt6/Qt5 lanes; no 9x120s matrix, no new
  synthetic suite, no temporary probe program, no new executable script.
- `NOT_MEASURED`: Host CPU change, GUI responsiveness, Viewer P95, physical
  present, RFB wire byte delta, real EGLFS/RK3588 CPU/PSS, field A->B large-page
  convergence. The one existing 1080p public-grab probe run at the
  pre-withdrawal head was `UNSTABLE_BASELINE` (A1/A2 spread 36.36% > 15%), so
  per test governance no CPU claim is made and no resampling is performed.
- If the test-side Codebuddy cannot independently prove the running SHA, write
  `RUNTIME_SHA=NOT_VERIFIED` and do not attribute any observation to this code.

## 7. Single-link A/B handoff (unchanged)

- Development Codebuddy A owns the user's existing integration workflow, build
  and owner reporting. The User decides and performs the upgrade manually.
- Test Codebuddy B only measures the currently running program: independent
  desensitised 0V/1V CPU, one authorized A->B->A page convergence, last
  disconnect CPU/RSS/PSS/threads, and reconnect. B needs no build manifest from
  A and does not upgrade for the User.
- The User is not asked to repeat earlier multi-round testing. Human
  participates only for a real customer-site change authorization or a major
  release-scope decision.

## 8. Acceptance is not "PR merged"

#789 / #567 / #790 stay OPEN until same-SHA field evidence shows real CPU
improvement without worse Viewer P95, acceptable large-page convergence, and
bounded 0V SDK-owned live bytes / threads. Remaining allocator high-water needs
evidence and a clear scope. This candidate does **not** claim those outcomes.
