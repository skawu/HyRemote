# Qt 6.8 RHI / EGLFS existing-resource probe

**Independent research only.** The probe does not link to HyRemote Runtime,
modify Qt-owned swapchain flags, or attach to a customer's HMI. It requires the
Qt6::GuiPrivate SDK matching its Qt 6.8.3 runtime.

## Build (separate from the product)

    cmake -S research/qt68-rhi-readback -B build-rhi-probe -G Ninja \
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/gcc_64
    cmake --build build-rhi-probe --parallel 4

Use the matching MSVC kit when compiling on Windows.

## Run ONLY in a standalone graphics session

Default mode only queries the resource identity and required creation flag:

    QT_QPA_PLATFORM=eglfs ./build-rhi-probe/hyremote-rhi-readback-probe > rhi-inspect.json

Only after the first result reports ELIGIBLE_INSPECTION_ONLY, opt in to a
single actual readback of the standalone window:

    QT_QPA_PLATFORM=eglfs ./build-rhi-probe/hyremote-rhi-readback-probe \
      --submit-readback > rhi-submit.json

**Do not launch this extra Qt/EGLFS fullscreen window while a customer HMI is
running.** Use an isolated maintenance boot or lab target. EGLFS generally
supports one native GL window. For preliminary CI, Xvfb/Mesa OpenGL is useful,
but XCB results are not EGLFS / RK3588 results.

## Outcome taxonomy (a zero exit code is NOT acceptance)

- NO_RENDER_FRAME / NO_RHI / NO_SWAPCHAIN: no useful scenegraph resource.
- RESOURCE_NOT_READBACK_ELIGIBLE: Qt's existing swapchain lacks the required
  UsedAsTransferSource flag. Never add it by mutating a Qt-owned swapchain.
- ELIGIBLE_INSPECTION_ONLY: a prerequisite passed; no readback has occurred.
- NO_COMMAND_BUFFER / NO_CALLBACK_WITHIN_TIMEOUT / EMPTY_OR_INVALID_RESULT:
  opt-in did not prove feasibility.
- CALLBACK_WITH_BYTES_NOT_NONBLOCKING_PROOF: bytes and a callback arrived in
  this standalone window; this does NOT establish pixel fidelity, render
  thread nonblocking, user HMI compatibility or product performance.

Preserve the exact Git SHA, Qt kit identity, QPA, actual backend, raw JSON,
and any failure. The probe submits a maximum of one readback, never calls
QRhi::finish() or starts a second offscreen frame, and runs its lightweight
render callback as a direct connection. A retained outstanding result is
deliberately left allocated until process exit if there is no callback, to
avoid prematurely releasing storage still referenced by RHI.

For risks, evidence doors R0-R5, and the current product NO-GO decision, read
docs/internal/qt68-rhi-eglfs-readback-review-2026-11-11.md.
