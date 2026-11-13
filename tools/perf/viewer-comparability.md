# #967 Offline Viewer comparison

Run `python tools/perf/audit_viewer_comparability.py A1 A2` before interpreting
a one-Viewer A/A pair. Each directory contains the original result.json and
runtime-frame-trace.jsonl. Add comparison-environment.json from observations
made during the run. Record qt_version, qt_kit_sha256, rhi_backend, render_loop,
gpu_driver, display_mode (including scale), refresh_hz, display_session,
window_visibility, rfb_encoding, sampler_sha256, capture_route (public or gl),
trace_sha256 and viewer_qpc_hz. Use host_clock=host_monotonic_ns,
viewer_clock=viewer_qpc and interval_policy=separate_clock_domains only if
the original measurement supports those statements. Missing values stay
NOT_INSTRUMENTED. Legacy archives may lack the actual QSG selection and QPC
frequency. Do not reconstruct them from desired settings.

Exit 2 blocks interpretation. MATCHED_METADATA checks the recorded identity and
sample accounting, and still requires comparing Host stage and local swap
drift against the preregistered bounds. It does not attest the environment,
validate a product or measure Viewer decode or physical present. Use --mode ab
only for an authorized same-SHA comparison with distinct Runtime builds. The
same Host binary is required to isolate the Runtime change.

## Tagged Qt source and what to observe

* Qt 5.15.2 (104eae5b17b0ec700391e9539ee3a4f638588194) and Qt 6.8.3
  (2ec235a2235a119887683e55787b93f99dc5f12f) implement update as scheduling.
  GUI/basic performs polish and sync in renderWindow; threaded polishes on the
  GUI side and syncAndRender operates on the render thread. Qt 5 also has a
  Windows-specific loop. Actual exposure and context teardown matter; the
  chosen loop cannot be inferred from the OS or environment request alone.
  [Qt5 update](https://github.com/qt/qtdeclarative/blob/104eae5b17b0ec700391e9539ee3a4f638588194/src/quick/items/qquickwindow.cpp#L418)
  [Qt6 update](https://github.com/qt/qtdeclarative/blob/2ec235a2235a119887683e55787b93f99dc5f12f/src/quick/items/qquickwindow.cpp#L378)
  [Qt5 basic](https://github.com/qt/qtdeclarative/blob/104eae5b17b0ec700391e9539ee3a4f638588194/src/quick/scenegraph/qsgrenderloop.cpp#L504)
  [Qt5 Windows loop](https://github.com/qt/qtdeclarative/blob/104eae5b17b0ec700391e9539ee3a4f638588194/src/quick/scenegraph/qsgwindowsrenderloop.cpp#L446)
  [Qt6 basic](https://github.com/qt/qtdeclarative/blob/2ec235a2235a119887683e55787b93f99dc5f12f/src/quick/scenegraph/qsgrenderloop.cpp#L563)
  [Qt6 threaded polish/exposure](https://github.com/qt/qtdeclarative/blob/2ec235a2235a119887683e55787b93f99dc5f12f/src/quick/scenegraph/qsgthreadedrenderloop.cpp#L1219)
* Qt5 emits afterRendering after drawing, before direct GL swap; optional RHI
  takes its own endFrame path. Qt6 emits afterRendering before endFrame and
  afterFrameEnd at the end of frame processing. The public frameSwapped signal
  means queued presentation. These callbacks establish neither physical present
  nor Viewer decode. Qt6 begin/endExternalCommands delegates to the current RHI
  command buffer. It does not make an arbitrary read framebuffer eligible.
  D3D11 submission and staging are independent of the GL PBO route.
  [Qt5 render callback](https://github.com/qt/qtdeclarative/blob/104eae5b17b0ec700391e9539ee3a4f638588194/src/quick/items/qquickwindow.cpp#L619)
  [Qt5 threaded swap](https://github.com/qt/qtdeclarative/blob/104eae5b17b0ec700391e9539ee3a4f638588194/src/quick/scenegraph/qsgthreadedrenderloop.cpp#L850-L875)
  [Qt6 threaded endFrame](https://github.com/qt/qtdeclarative/blob/2ec235a2235a119887683e55787b93f99dc5f12f/src/quick/scenegraph/qsgthreadedrenderloop.cpp#L765-L802)
  [Qt6 external commands](https://github.com/qt/qtdeclarative/blob/2ec235a2235a119887683e55787b93f99dc5f12f/src/quick/items/qquickwindow.cpp#L3420-L3470)
* The public grab contract is asynchronous. Both tagged implementations
  connect setup/render directly to scene graph signals and post completion to
  the result object's event loop. Qt5 sets the texture size directly subject
  to minimum FBO size; Qt6 multiplies by effective DPR and sets image DPR.
  The internal render implementation is a diagnostic reference.
  [Qt5 grab](https://github.com/qt/qtdeclarative/blob/104eae5b17b0ec700391e9539ee3a4f638588194/src/quick/items/qquickitemgrabresult.cpp#L227-L347)
  [Qt6 grab](https://github.com/qt/qtdeclarative/blob/2ec235a2235a119887683e55787b93f99dc5f12f/src/quick/items/qquickitemgrabresult.cpp#L215-L339)

HyRemote calls grabToImage in quick_target.cpp:619 and hooks afterRendering
in quick_gl_pbo_capture_source.cpp:582, bracketing Qt6 external commands at
line 1107 (develop 01a3941). Compare request-to-submit and submit-to-complete
separately using the existing stage analyzer. The latter interval contains
multiple costs and cannot isolate GPU execution. If identical OFF A/A still
drifts, stop before attributing any difference to the GL route. If Qt Host
timings stay stable while desktop detection changes, inspect Viewer polling
and desktop composition before changing capture callbacks.
