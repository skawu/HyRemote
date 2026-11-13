# Zero-viewer resource release (#790) — candidate provenance, rollback and handoff task package

Date: 2026-11-13
Base: `develop@0fdaebcc89955a4bed5b52338c0815c5f6c89ed1` (squash of #995)
Scope: D2→D3 continuous delivery package for #967
Status: source implementation + regression evidence; physical-device measurements are `NOT_RUN_OWNER_DEPENDENCY`

## 1. What this change is

One product contract replaces per-backend idle guessing:

```text
RFB last client disconnect
  -> ClientCountingTransport / ViewerCaptureDemand.setViewerCount(0)   (one latched edge)
  -> ObservedCaptureSource::requestFrame() on the Core capture scheduler thread
  -> CaptureSource::releaseIdleResources()                             (one virtual, default no-op)
  -> composite / GL PBO / D3D11 staging / public grab (no-op)
```

The edge is delivered on the capture scheduler thread, so no backend needs its own
viewer counter, idle timer, platform check or Qt-version branch. This is the same
"one Core + one Shared Runtime + capability-selected CaptureSource" architecture the
merged #995 capability contract uses; it is not a new pipeline and it does not
re-implement the #567 snapshot, per-viewer delivery, `bytesToWrite()==0` suppression
or `m_encodingSocket` backpressure already in `rfb_transport.cpp`.

`CaptureSource::releaseIdleResources()` is documented as: release only pixel/GPU
storage that exists to serve a capture, keep geometry/capabilities/started state,
allocate again on the next request, never block on a GPU fence or Qt render, and
defer the free if a previously accepted request is still in flight.
`src/core/include/hyremote/core/capture_source.hpp` is an explicitly non-stable
internal surface (`docs/v1-api-stability.md`), so this is not an SDK ABI promise.

## 2. Retained bytes actually addressed

| Owner | Retained allocation while 0 viewers | Before | After |
| --- | --- | --- | --- |
| `CompositeCaptureSource::State` | last published `RemoteFrame` (full frame; in the single-surface fast path it aliases the child grab QImage) | pinned until `stop()` | only `QSize lastCanvasSize` is kept |
| `GlPboCaptureSource::State` | one reusable PBO, `W*H*4` bytes | until `stop()`/invalidation | deleted at the zero-viewer edge, reallocated on demand |
| `D3D11StagingCaptureSource::State` | one reusable staging texture, `W*H*4` bytes | until `stop()`/invalidation | released at the zero-viewer edge, recreated on demand |
| Quick public grab (`quick_target.cpp`) | per-frame `QuickImageFrameStorage` only | already released with the frame | unchanged (default no-op) |
| RFB transport | `m_observedFrame`, `m_frames->latest` | already released by `releaseIdleFrameCaches()` at last disconnect | unchanged |

`W*H*4` at 1920x1080 is 8.29 MB per buffer; that number is derived from the
allocation formulas in the cited files, not measured on a device.

Deliberately **not** claimed:

- Qt scenegraph/EGL/D3D driver allocator high-water marks are not owned by HyRemote.
  A logical free does not require `RSS`/`PSS` to return to the pre-connection value;
  no `malloc_trim`, `SetProcessWorkingSetSize`, RK3588 or vendor-specific call is added.
- The one Core scheduler thread is not created per viewer and is not torn down at
  0 viewers; it parks in the demand gate. That is bounded and unchanged.
- The #790 field symptom (RK3588 Generic/EGLFS, ~95 MB RSS growth after a long
  session with repeated connect/disconnect) is not reproduced or closed here.

## 3. Evidence

Canonical entry only (`build.cmd`, single workspace build tree, no manual CMake):

- Red regression (implementation temporarily reverted, tests kept):
  - `hyremote-runtime-automatic-composite-capture-test`
    -> `FAIL: composite kept the last published frame pixels alive after delivery`
  - `hyremote-runtime-notifications-test`
    -> `CHECK failed: state->idleCaptureReleases.load() == 1` (x2) and `== 2`
- Green after the fix: `composite-capture`, `runtime-notifications`,
  `quick-capture` (3 variants), `viewer-capture-demand` all pass.
- Experimental backends compiled and their tests run with the canonical entry:
  `build.cmd test --cmake=HYREMOTE_EXPERIMENTAL_QUICK_GL_PBO=ON
  --cmake=HYREMOTE_EXPERIMENTAL_QUICK_D3D11_STAGING=ON --test-filter=quick-capture`
  and `--test-filter=d3d11-lifecycle` -> pass. Defaults stay OFF.
- Real-GL release probe (`runQuickGlPboIdleReleaseProbe`, printed as
  `WIN790_IDLE_RELEASE=`): the state drops the PBO name, the retire worker deletes
  it in the context that created it, and the next request allocates a live buffer.
  It is executed by `hyremote-quick-gl-pbo-session-test`, which is registered only
  for UNIX + `HYREMOTE_EXPERIMENTAL_QUICK_GL_PBO=ON` with an Xvfb/xcb platform
  (`requireGl`, so a missing context fails instead of skipping).
  **NOT_RUN_LOCAL_GL_LANE**: this workstation has no desktop GL context and no
  authorized second build tree, and no hosted PR lane builds that combination, so
  the probe compiles here but does not execute. It must not be reported as PASS.
- Qt 5.15.2: `NOT_RUN_LOCAL_KIT` — this workstation's single authorized build tree is
  the Qt 6.8.3 Kit; Qt5 product compile is covered by the PR CI matrix, not by a
  second local build directory.
- Physical viewer/RSS/CPU: `NOT_RUN_OWNER_DEPENDENCY` (no board/permission here).

## 4. Candidate provenance

Already merged and unchanged by this PR:

- #567 H2 immutable observed-frame snapshot + per-viewer delivery (`rfb_transport.cpp`)
- #823 single-canvas composite path (`automatic/composite_target.cpp`)
- #798 idle storage release on disconnect (`releaseIdleFrameCaches`)
- #789 viewer-demand gate (`detail/viewer_capture_demand.hpp`)
- #995 unified Quick capture capability contract (`quick/quick_capture_capability.hpp`)

Added by this candidate (one commit, revertible as a unit):

- `src/core/include/hyremote/core/capture_source.hpp` — the optional idle-resource hook
- `src/runtime/src/detail/viewer_capture_demand.hpp` — latched zero-viewer edge + `waitForCapture()`
- `src/runtime/src/access_instance.cpp` — delivery on the capture scheduler thread
- `src/runtime/src/automatic/composite_target.cpp` — geometry-only canvas memory, child forwarding
- `src/runtime/src/quick/quick_adaptive_capture_source.hpp` — forward to both providers
- `src/runtime/src/quick/quick_idle_release.hpp` — shared deferred-release latch
- `src/runtime/src/quick/quick_gl_pbo_capture_source.cpp` — worker-mediated PBO release
- `src/runtime/src/quick/quick_d3d11_staging_capture_source.cpp` — dispatcher-mediated staging release
- tests: `test_viewer_capture_demand.cpp`, `automatic_composite_capture_test.cpp`,
  `test_quick_capture.cpp`, `access_instance_notifications_test.cpp`

## 5. Rollback

- Revert this single commit. No protocol, schema, persisted state, configuration
  default or public SDK ABI changes, so a revert needs no data migration and no
  version bump beyond the normal version-integrity rule.
- Native providers remain `OFF` by default; a revert cannot change which capture
  technology a released product uses.
- Rollback does not re-open any correctness hole: the pre-existing behavior stays
  "retain until stop/invalidation".

Release-train interaction (must not bypass the protected gate):

- PR #876 is a separate OPEN controlled five-file backport
  (`composite_target.cpp`, `quick_image_frame_storage.hpp`, `quick_target.cpp`,
  `rfb_transport.cpp`, `test_rfb_wire_robustness.cpp`) for the release branch.
- This candidate also edits `composite_target.cpp`. If #876 is rebased or replayed
  after this lands, keep `lastCanvasSize` + the `releaseIdleResources()` override;
  dropping them silently reintroduces the full-frame pin.
- No release branch is written here, no ruleset/required-check bypass, no
  administrator merge. #95/#594 keep release and version authority.

## 6. Customer-side Codebuddy handoff (one linked task)

Same SHA (the squash of this PR), Qt 6.8.3 Linux ARM64 EGLFS/Generic, no local
patches, canonical entry only:

1. build/install with the registered profile; record the resolved Kit identity.
2. Version/rollback baseline: record `--version`, previous candidate, and the
   rollback command (reinstall previous candidate; no data migration).
3. 1 viewer static scene, 600 s: capture CPU time and RSS/PSS of the host process.
   Acceptance: CPU and end-to-end latency must not regress versus the current
   customer candidate; `#789_CPU` stays `NOT_MEASURED` until this data exists.
4. A→B large-page scroll: real viewer must converge (no long-lived stale strips).
   `#567_VIEWER` stays `NOT_MEASURED` until a real viewer records it.
5. Disconnect, then 600 s idle: record PSS/RSS at 0 s / 60 s / 600 s and compare
   with the pre-connection value. Report both "SDK-owned frame buffers released"
   (source-level, this PR) and "process high-water actually dropped" (device-level)
   separately. `#790_RETAINED_BYTES` is `NOT_MEASURED` until this exists.
6. Reconnect: first-frame latency and DesktopSize must be correct (the composite
   keeps size but no pixels; hidden-surface scenes must not force a 1x1 resize).

If the board or permission is unavailable, the report must say
`NOT_RUN_OWNER_DEPENDENCY` with the exact missing item and must not ask the Human to
run diagnostics by hand or claim the customer defect is fixed.
