# Transparent QPA capture classification — Qt 6.8.3 x86

Status: **V1 production-path classification complete; dual-OS execution evidence pending #74**

Issue: #86, child of #32.

This document classifies the capture paths that the **production HyRemote Transparent QPA Proxy actually uses** for the frozen V1 reference line: Qt 6.8.3 on Windows x86_64 and Linux x86_64. It is deliberately narrower than the historical capture spikes. A spike result is reused only when the production adapter uses the same relevant Qt capture mechanism.

No row in this document becomes a V1 `Supported` compatibility claim until the required Windows and Linux jobs execute successfully. GitHub-hosted jobs are currently failing before runner assignment under #74; therefore implementation presence and historical host evidence are kept separate from acceptance status.

## 1. Production architecture boundary

Transparent QPA does not own a second capture stack. The QPA-private application compositor resolves each application-owned child surface through the same built-in target adapters used by public `HyRemote::RemoteAccess`:

- QWidget top level -> production Widgets adapter;
- QQuickWindow top level -> production Quick adapter;
- unsupported QObject/QWindow families -> no adapter, no silent fallback.

The composite QPA target converts child frames into the logical application canvas, but it does not replace child capture semantics.

### Production QWidget path

`src/remoteaccess/src/widgets/widget_target.cpp` creates owned CPU `RemoteFrame` storage, wraps it in a `QImage` with `QImage::Format_RGBA8888_Premultiplied`, sets the target DPR, clears it, and calls:

```cpp
target->render(&image);
```

The work is marshalled to the Qt GUI thread. The result is CPU-readable RGBA with full-frame damage in the current V1 baseline.

### Production QQuickWindow path

`src/remoteaccess/src/quick/quick_target.cpp` resolves `QQuickWindow::contentItem()` and calls:

```cpp
content->grabToImage(pixelSize);
```

Completion is asynchronous. The adapter retains the bounded Core capture request, converts the completed image to owned RGBA `RemoteFrame` storage, timestamps on completion, and retries temporary hidden/minimized/null-grab cases without inventing a private RHI path.

This is the same public Qt capture mechanism evaluated in #16 / `docs/internal/async-capture-spike.md`.

## 2. V1 capture-class matrix

Status vocabulary in this table is intentionally evidence-aware:

- **Candidate** — production implementation exists and matches the stated path, but required V1 dual-OS execution is still blocked by #74.
- **Historical host evidence** — an earlier Windows/Qt 6.8.3 probe exercised the same relevant Qt mechanism; useful supporting evidence, not QPA product acceptance.
- **Unsupported** — HyRemote intentionally has no V1 production adapter for this family/backend.
- **Unverified** — no sufficient production-path evidence exists; do not infer support.

| Surface family | Production QPA child path | Existing evidence | V1 classification while #74 is open | Important boundary |
|---|---|---|---|---|
| QWidget / raster / custom QPainter hierarchy | `QWidget::render()` into owned RGBA frame | SPIKE-01 measured `QWidget::render()` on Windows Qt 6.8.3; normal Widgets product tests exist | **Candidate** | Final Windows + Linux QPA execution still required |
| QWidget hierarchy containing `QOpenGLWidget` | same parent `QWidget::render()` path | Qt 6.8.3 upstream tests explicitly exercise `QWidget::render()` with QOpenGLWidget; HyRemote adds `hyremote-qpa-widget-opengl-capture-smoke` over the full production QPA -> RemoteAccess -> RFB path | **Candidate; dedicated production E2E pending runner** | Historical HyRemote SPIKE used parent `QWidget::grab()`, so that older result is not substituted for the new production-path test |
| QWidget hierarchy containing `QQuickWidget` | same parent `QWidget::render()` path | SPIKE-01 proved whole-window composition with parent `QWidget::grab()`, not the exact production `render()` path | **Unverified for the mixed QQuickWidget composition claim** | V1 must not upgrade this row without production-path evidence; ordinary QWidget and native QQuickWindow remain independently covered |
| QQuickWindow 2D | `contentItem()->grabToImage(pixelSize)` | #16: pixel-identical to synchronous whole-window reference for measured 2D scenes on Windows Qt 6.8.3, D3D11/OpenGL | **Candidate + historical host evidence** | Visible-window public API baseline; not a zero-copy/performance promise |
| QQuickWindow + same-scene overlay tree | same `contentItem()->grabToImage()` | #16 measured sibling content under `contentItem()`; root-item grabs were shown incomplete | **Candidate for the scene-tree mechanism** | Actual application-specific Controls Popup modes are not generalized beyond their resolved scene/window form |
| Quick3D inside QQuickWindow | same `contentItem()->grabToImage()` | #16 measured 0 differing pixels vs synchronous reference on Windows Qt 6.8.3 | **Candidate + historical host evidence** | Linux/reference-backend QPA execution still required before support claim |
| custom `QQuickFramebufferObject` inside QQuickWindow on OpenGL RHI | same `contentItem()->grabToImage()` | #16 measured fresh custom-FBO content with max channel diff 2 on Windows/OpenGL | **Candidate + historical host evidence for OpenGL RHI** | `QQuickFramebufferObject` itself is OpenGL-specific; no D3D11 support is implied |
| custom `QQuickFramebufferObject` on non-OpenGL RHI | no meaningful rendered content from Qt FBO type | SPIKE evidence: FBO did not render on D3D11 | **Unsupported for this FBO type/backend combination** | This is a Qt rendering/backend constraint, not a HyRemote transport issue |
| independent secondary QQuickWindow | normal Quick child adapter, composed by QPA application target | #85 adds same-viewer multi-QQuickWindow continuity E2E | **Candidate; E2E pending runner** | One RemoteAccess / Session / listener; no per-window public runtime |
| independent QWidget dialog/tool/QMenu popup | normal Widgets child adapter, composed by QPA application target | #84/#85 add same-viewer dialog and QMenu popup E2E | **Candidate; E2E pending runner** | Qt Quick in-window popup is not double-composed as a top level |
| arbitrary `QWindow`, `QOpenGLWindow`, foreign/native OS window without Widgets/Quick adapter | no built-in child adapter | production controller excludes/declines unsupported generic/foreign surfaces | **Unsupported in V1 Transparent QPA** | HyRemote is an application remote-access framework, not an arbitrary desktop/window-server capture system |

## 3. QOpenGLWidget production qualification

`hyremote-qpa-widget-opengl-capture-smoke` is intentionally end-to-end and does not instantiate a private test capture source.

The test creates:

1. a normal top-level QWidget;
2. a red `QOpenGLWidget` child that clears its real GL framebuffer;
3. a blue raster QWidget sibling;
4. the real `hyremote` platform plugin;
5. the real QPA `RemoteController` and public `RemoteAccess` runtime;
6. the built-in RFB transport and a test-side RFB 3.8 viewer.

The test decodes the returned Raw framebuffer and samples one pixel inside each child. Passing therefore proves the **production parent `QWidget::render()` path** contains both GL and raster content. It does not merely prove that `QOpenGLWidget::grabFramebuffer()` works.

Until the exact Windows/Linux Qt 6.8.3 jobs execute, this test is executable evidence waiting to run, not a claimed pass.

## 4. Qt Quick evidence reuse rule

The Quick production adapter and #16 use the same relevant public mechanism: `QQuickWindow::contentItem()->grabToImage()`.

Therefore the following #16 fidelity findings are valid supporting evidence for the production mechanism:

- measured 2D scene: pixel-identical to synchronous whole-window reference;
- measured Quick3D scene: pixel-identical to synchronous reference;
- measured custom `QQuickFramebufferObject`: captured fresh FBO content; differences were confined to its independently advancing render counter;
- grabbing the QML root item alone is insufficient because overlay siblings live under `contentItem()`;
- the public asynchronous call requires a visible attached window and needs caller-imposed bounded in-flight/backpressure policy.

Production HyRemote already supplies the missing bounded Core request ownership and completion-time frame ownership around this path. The historical throughput numbers remain measurements of the spike host, not V1 performance guarantees.

## 5. QQuickWidget boundary

A `QQuickWidget` is not a QQuickWindow top-level surface in the Transparent QPA model. It is a child inside a QWidget hierarchy, so the current production path is the **parent QWidget adapter** (`QWidget::render()`).

Historical SPIKE-01 showed whole-window composition using parent `QWidget::grab()`, while #16 showed that `rootObject()->grabToImage()` captures only the embedded Quick content and not the surrounding QWidget hierarchy. Neither result is identical to the production parent `QWidget::render()` path.

For that reason QQuickWidget mixed-composition remains **Unverified** in this classification unless a dedicated production-path test is added and executed. This limitation does not weaken the first-class native QWidget and QQuickWindow V1 targets.

## 6. DPI, resize, and multi-surface coordinate rule

QPA application composition uses a logical-pixel application canvas. Each child adapter may capture at its actual device-pixel ratio; the composite source scales the child image into that surface's logical geometry before placing it in the canvas.

The RFB client is informed of application-canvas geometry changes through the existing DesktopSize encoding. #84/#85 tests retain one viewer socket while the canvas expands/contracts. This is a correctness baseline for resize and mixed placement; it is not a zero-copy or native-resolution multi-DPR optimization claim.

Final dual-OS validation remains pending #74.

## 7. What V1 does not claim

V1.0.0.0 does **not** require or claim:

- arbitrary native/foreign desktop-window capture;
- a universal custom `QWindow`/`QOpenGLWindow` capture backend;
- zero-copy RHI readback;
- GL/PBO capture merely to replace a public path that already meets correctness requirements;
- universal performance parity across D3D11, OpenGL and every future RHI backend;
- Embedded Linux/EGLFS/RK3588 support as part of the x86 GA gate.

Those are separate evidence-driven roadmap items. A future optimization must not change the stable `RemoteAccess` or Transparent QPA user-facing contract.

## 8. Remaining acceptance work

Before #86 / #32 can use the word `Supported` for the claimed combinations:

1. restore GitHub-hosted execution under #74;
2. run the exact Qt 6.8.3 Windows x86_64 and Linux x86_64 QPA matrix;
3. require the production OpenGL-widget capture smoke to pass where OpenGLWidgets is available;
4. run the QPA multi-surface/popup/Quick continuity tests from #84/#85;
5. record exact graphics/backend combinations that actually passed;
6. keep QQuickWidget mixed composition `Unverified` unless separately proven;
7. complete #32's physical local-visible + remote coexistence evidence separately — hosted/headless execution must not be presented as that proof.

Governance mode: `transitional-explicit`.
