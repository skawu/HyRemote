# HyRemote Compatibility Matrix

Status: **bootstrap / initial capture evidence collected, embedded validation outstanding**

HyRemote does not claim support based only on API similarity. A configuration is marked supported only after it has a reproducible build and functional validation.

Capture evidence and the reasoning behind the recommended baseline paths are recorded in
[`capture-spike.md`](capture-spike.md) (SPIKE-01, issue #3).

## Status definitions

- **Supported** — repeatable build + functional validation + documented limitations.
- **Experimental** — works in a limited validation path but is not yet a compatibility promise.
- **Unsupported** — known architectural or implementation limitation.
- **Unverified** — not yet tested; no support claim should be inferred.

## Capture rows validated on the host (SPIKE-01)

These rows come from the host runs in `spikes/capture/evidence/` (Windows 11, Qt 6.8.3,
MSVC 19.44, Intel UHD Graphics 770). They are **host-only** evidence: per rule 2 below
they say nothing about Embedded Linux/EGLFS support. The capture backend column names
the mechanism that was actually measured, not a frozen HyRemote API.

| Qt | OS / target | QPA / graphics | Application type | Capture backend | Transport | Status | Notes |
|---|---|---|---|---|---|---|---|
| 6.8.3 | Windows 11 / x86_64 | `windows` / D3D11 | QWidget/raster | `QWidget::render()` into a caller buffer, `QWidget::grab()` | VNC candidate | Experimental | 2.7-3.7 ms at 960x600 including the buffer clear; child paint regions can be mapped into the shared target (deterministic controls pass); popups/dialogs are separate top-level windows |
| 6.8.3 | Windows 11 / x86_64 | `windows` / D3D11 | QWidget with custom QPainter | same | VNC candidate | Experimental | Covered by the `widgets` case |
| 6.8.3 | Windows 11 / x86_64 | `windows` / D3D11 | QOpenGLWidget | parent `QWidget::grab()`; `grabFramebuffer()` measured as an alternative | VNC candidate | Experimental | Parent grab composes GL content at 5.2-9.9 ms; `grabFramebuffer()` returns only the GL widget at 3.8-4.9 ms; first call 70-141 ms |
| 6.8.3 | Windows 11 / x86_64 | `windows` / D3D11 | QQuickWidget | parent `QWidget::grab()`; `grabFramebuffer()` measured as an alternative | VNC candidate | Experimental | Parent grab 4.9-8.2 ms across sessions; `grabFramebuffer()` 4.1-12.7 ms across sessions; parent path preferred for the whole-window contract and damage mapping |
| 6.8.3 | Windows 11 / x86_64 | `windows` / OpenGL RHI | Qt Quick 2D | `QQuickWindow::grabWindow()` | VNC candidate | Experimental | 17.1 ms at 960x600; blocks the GUI thread; no damage region (no QWidget root, only `QEvent::UpdateRequest`) |
| 6.8.3 | Windows 11 / x86_64 | `windows` / OpenGL RHI | Quick3D | `QQuickWindow::grabWindow()` | VNC candidate | Experimental | 16.4 ms; content fidelity verified |
| 6.8.3 | Windows 11 / x86_64 | `windows` / OpenGL RHI | custom Quick FBO/OpenGL | `QQuickWindow::grabWindow()` | VNC candidate | Experimental | Freshness verified with an encoded render counter (24 -> 723 over 723 rendered frames) |
| 6.8.3 | Windows 11 / x86_64 | `windows` / D3D11 | custom Quick FBO/OpenGL | none | VNC candidate | Unsupported | `QQuickFramebufferObject` never rendered on a non-OpenGL RHI backend |
| 6.8.3 | Windows 11 / x86_64 | `windows` / D3D11 | Qt Quick 2D | `QQuickWindow::grabWindow()` once per frame | VNC candidate | Unsupported | The application's own event loop fell to ~1 fps while the call itself reported 15.8 ms |
| 6.8.3 | Windows 11 / x86_64 | `windows` / D3D11 and OpenGL | Qt Quick 2D | `QQuickWindow::contentItem()->grabToImage()` (async) | VNC candidate | Experimental (host) | Pixel-identical to the synchronous whole-window capture; ~32 ms latency at 1 in flight, 115 /s at 4; requires a visible window; see `async-capture-spike.md` |
| 6.8.3 | Windows 11 / x86_64 | `windows` / D3D11 | Quick3D | `contentItem()->grabToImage()` (async) | VNC candidate | Experimental (host) | 0 differing pixels vs the synchronous capture; 123 /s at 4 in flight |
| 6.8.3 | Windows 11 / x86_64 | `windows` / OpenGL | custom Quick FBO/OpenGL | `contentItem()->grabToImage()` (async) | VNC candidate | Experimental (host) | Fresh FBO content per rendered frame (counter 80 -> 105 over 26 frames); max channel diff 2 |
| 6.8.3 | Windows 11 / x86_64 | `windows` / D3D11 | QQuickWidget | `rootObject()->grabToImage()` (async, Quick content only) | VNC candidate | Experimental (host) | 699x557 Quick content; surrounding widget composition still requires the synchronous parent `grab()` |
| 6.8.3 | Windows 11 / x86_64 | `windows` / any | QOpenGLWidget | none | VNC candidate | Unsupported | No public asynchronous capture API exists for this family |

## Rows still awaiting evidence

Every row below needs the same treatment on the target itself before any claim.

| Qt | OS / target | QPA / graphics | Application type | Capture backend | Transport | Status | Notes |
|---|---|---|---|---|---|---|---|
| 6.8.x | Embedded Linux / RK3588 | EGLFS / OpenGL ES | Qt Quick 2D | recommended by SPIKE-01, unverified on target | VNC candidate | Unverified | Primary reference target; acceptance gate of issue #3 |
| 6.8.x | Embedded Linux / RK3588 | EGLFS / OpenGL ES | Quick3D | recommended by SPIKE-01, unverified on target | VNC candidate | Unverified | Must be tested separately |
| 6.8.x | Embedded Linux / RK3588 | EGLFS / OpenGL ES | custom Quick FBO/OpenGL | recommended by SPIKE-01, unverified on target | VNC candidate | Unverified | Must be tested separately |
| 6.8.x | Embedded Linux / RK3588 | EGLFS | QWidget/raster | recommended by SPIKE-01, unverified on target | VNC candidate | Unverified | Widgets are a first-class support target |
| 6.8.x | Embedded Linux / RK3588 | EGLFS/OpenGL | QOpenGLWidget | recommended by SPIKE-01, unverified on target | VNC candidate | Unverified | Composing a child GL surface without a window system is unproven |
| 6.8.x | Embedded Linux / RK3588 | mixed | QQuickWidget | recommended by SPIKE-01, unverified on target | VNC candidate | Unverified | Composition must be validated |
| 6.8.x | Linux desktop (X11/Wayland) | any | any | not measured | VNC candidate | Unverified | SPIKE-01 was executed on Windows only |

## Required evidence per entry

Record:

- exact Qt version;
- compiler/toolchain;
- board/OS/BSP when embedded;
- QPA platform;
- Qt Quick graphics API/backend where relevant;
- application/sample used;
- capture backend;
- transport + viewer version;
- resolution;
- basic performance measurements;
- local rendering/input impact;
- known limitations;
- link to test/Issue/PR evidence.

## Viewer matrix

The first baseline should distinguish standard RFB compatibility from optional H.264 support.

| Viewer | Version/build | Standard RFB | H.264 | Status | Notes |
|---|---|---|---|---|---|
| TigerVNC | TBD | Unverified | Unverified | Unverified | H.264 may depend on build options |
| Other standard VNC viewer | TBD | Unverified | Not assumed | Unverified | Add only after test |

## Rules

1. `Unverified` must never be described as supported in README/release notes.
2. Desktop Linux results do not automatically imply Embedded Linux/EGLFS support.
3. A working Qt Quick 2D case does not automatically imply Quick3D or custom OpenGL/FBO support.
4. A working QWidget raster case does not automatically imply QOpenGLWidget/QQuickWidget support.
5. Hardware encoder support is tracked separately from the generic capture/transport path.
