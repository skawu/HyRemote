# #967 P1 user candidate — one common Qt-backend path

Date: 2026-11-13
Base: `develop@0f5e8333c27556e25d8cbbec98b2bc543532d7ab` (#995 + #996 merged)
Design freeze: `14c5eb2b0efaa3e05b26685d9724e12d91791945`
Scope: ONE user candidate for #789 / #567 / #790 under the single cross-Qt-backend
architecture. This is not a second Feature version and not a release approval.

`SOURCE_SHA` is the exact final head of the single implementation PR. It is
reported in the #967 handoff comment and in the PR description (GitHub is the
single continuation anchor); a file cannot contain the hash of the commit that
contains it. All other candidate identity below is content-addressed and fixed.

## 1. Frozen candidate source manifest

Content hashes are `git hash-object` blobs of the source files at `SOURCE_SHA`:

| File | Blob |
| --- | --- |
| `src/runtime/src/quick/quick_frame_reuse.hpp` | `34ecdb47d94b02359150c0fd0c735be9bca31632` |
| `src/runtime/src/quick/quick_target.cpp` | `cd184895f2cfddb5c8789aa7648ac9ffa8b7a8cd` |
| `src/runtime/tests/test_quick_capture.cpp` | `002ab9071d811ba9d5015678b9ddac1df37192d0` |
| `docs/internal/967-p1-design-freeze-2026-11-13.md` | `8c9b3497e478f77423bebdd62c15e2c9985ec258` |

Also required: this candidate package document. `CaptureSource`, `RemoteFrame`,
`FrameStorage`, `rfb_transport.cpp`, `rfb_delivery_state.hpp`, `rfb_trle.hpp`,
the watermark compositor/decorator, `composite_target.cpp`, `access_instance.cpp`
and all native GL/D3D11 providers are **unchanged by this increment**. The
#991/H2 served-frame snapshot, per-viewer accumulated/committed damage,
Raw/TRLE and `bytesToWrite()==0` / `m_encodingSocket` backpressure remain the
correctness baseline. No new capability table, no new Runtime, no per-SoC code.

## 2. Target Kit, product route and graphics backend

- Windows x86_64, Qt 6.8.3 `msvc2022_64` on the local canonical build profile;
  Qt 5.15.2 is covered by the existing exact-head PR CI lane.
- Product route: public Qt `grabToImage()`. Startup log identity is
  `HYREMOTE_EXPERIMENTAL_QUICK_CAPTURE=PUBLIC_QT_FALLBACK`. Native GL PBO and
  D3D11 staging remain opt-in / default OFF; no automatic native preference is
  added and the #995 `Admitted/Transient/Unsupported + Reason` table is untouched.
- Qualification truth: Qt 5.15.2 DirectGL and Qt 6.8.3 desktop GL small-target
  real-GPU PASS (native, default OFF); Windows D3D11 static-1V CPU/private-bytes
  gain with viewer latency `NOT_QUALIFIED`; Qt 6 GLES/EGLFS, redirected FBO,
  high DPI, multi-GPU and other RHI remain `NOT_QUALIFIED` and stay on the
  public Qt fallback. No EGLFS or ARM64 claim is made from a Windows result.
- C++/QML/Generic/QPA all use this same Quick `CaptureSource` through the Shared
  Runtime; the capability-selected Adaptive source uses it as `m_public`.

## 3. Install and rollback identity

- Install: `build.cmd build` / `build.cmd install` at `SOURCE_SHA` with the same
  `build.yml` profile and the configured workspace `build` tree. No public SDK
  header, ABI, configuration file, schema or Viewer-side change: the installed
  SDK identity and all existing examples/consumers are unchanged.
- Rollback: revert the single implementation PR (or `git revert` its commits)
  and rebuild with the same entry. The previous product baseline is
  `develop@0f5e8333`. No data migration and no persisted state is introduced.
- Release delta: `release/v0.3.5.0@b0680d5df4f76d8b84b02a1b950030a65a89ddca`
  and the OPEN selective backport PR #876 (`f5971906`, five files) do **not**
  contain this change. This increment does not widen #876's five-file
  whitelist, does not backmerge all of `develop`, and does not bypass the
  protected-branch topology check or the independent-App-bound required
  ruleset. A `develop` merge is not release qualification; #95 owns that gate.

## 4. What the candidate actually changes

Qt returns a new QImage (and therefore a new `FrameStorage`) for every
`grabToImage()` even when the scene did not change a single visible byte. On the
default public path shared by every Qt graphics backend, that fresh identity
forced a whole-frame watermark recomposition, a whole-frame visible-pixel
comparison and a whole-frame encode/send on every accepted static frame.

The candidate publishes a NEW `RemoteFrame` (own request identity, PTS and
completion time) whose immutable `FrameStorage` is shared with the previously
published frame **only when the visible RGBA bytes and the geometry are exactly
equal**. Downstream, the already-existing storage-identity fast paths then skip
the repeated decoration/compare/encode. Any changed byte allocates a new owner
and takes the unchanged full-delivery path. The one-slot cache is released on
the same single zero-viewer edge as #996, and on start/stop/generation changes.

Deterministic source-level effect: repeated whole-frame CPU work on unchanged
scenes is removed. It is NOT a measured CPU percentage on any device. No
already-written TCP byte is truncated, no FBU request behaviour is changed,
and no standard-VNC physical atomic present is promised.

## 5. Verification and honest gaps (no new research loop)

- Assertions inside the existing `quick-capture` CTest target: equal pixels
  reuse; one changed visible byte does not reuse; padded stride with equal
  visible bytes is equivalent; geometry/generation changes do not reuse;
  zero-viewer `release()` drops the SDK-owned reference.
- One canonical `build.cmd test` at the exact head, plus exact-head PR CI on the
  existing Qt6/Qt5 lanes. No 9x120s matrix, no new synthetic suite, no
  temporary probe program and no separate test research cycle.
- `NOT_MEASURED`: Host physical GPU counters (no Host PID), Viewer decode and
  physical present, real EGLFS/RK3588 CPU/PSS, and real large-page A->B visual
  convergence on customer hardware. Socket queued is not Viewer present.
- If the test-side Codebuddy cannot independently prove the running SHA, write
  `RUNTIME_SHA=NOT_VERIFIED` and do not attribute any observation to this code.

## 6. Single-link A/B handoff

- Development Codebuddy A owns the user's existing integration workflow, build
  and owner reporting. The User decides and performs the upgrade manually.
- Test Codebuddy B only measures the currently running program: independent
  desensitised 0V/1V CPU, one authorized A->B->A page convergence, last
  disconnect CPU/RSS/PSS/threads, and reconnect. B needs no build manifest
  from A and does not upgrade for the User.
- The User is not asked to repeat the earlier multi-round test programme.
  Human participates only for a real customer-site change authorization or a
  major release-scope decision; ordinary code, build, test and PR need no Human.

## 7. Acceptance is not "PR merged"

The issue stays OPEN until the same-SHA evidence shows: #789 Host CPU really
improves without worse Viewer P95; #567 large pages show no persistent stale
frame overlay/top-bottom split and converge acceptably; #790 0V SDK-owned
live bytes are released with bounded long-run resources/threads. Remaining
allocator high-water must be attributed with evidence and a clear scope.
Any failure keeps the issue open and names the shortest next source fix.
