# HyRemote Compatibility Matrix

Status: **bootstrap / evidence required**

HyRemote does not claim support based only on API similarity. A configuration is marked supported only after it has a reproducible build and functional validation.

## Status definitions

- **Supported** — repeatable build + functional validation + documented limitations.
- **Experimental** — works in a limited validation path but is not yet a compatibility promise.
- **Unsupported** — known architectural or implementation limitation.
- **Unverified** — not yet tested; no support claim should be inferred.

## Initial matrix

| Qt | OS / target | QPA / graphics | Application type | Capture backend | Transport | Status | Notes |
|---|---|---|---|---|---|---|---|
| 6.8.x | Embedded Linux / RK3588 | EGLFS / OpenGL ES | Qt Quick 2D | TBD by SPIKE-01 | VNC candidate | Unverified | Primary reference target |
| 6.8.x | Embedded Linux / RK3588 | EGLFS / OpenGL ES | Quick3D | TBD by SPIKE-01 | VNC candidate | Unverified | Must be tested separately |
| 6.8.x | Embedded Linux / RK3588 | EGLFS / OpenGL ES | custom Quick FBO/OpenGL | TBD by SPIKE-01 | VNC candidate | Unverified | Must be tested separately |
| 6.8.x | Embedded Linux / RK3588 | EGLFS | QWidget/raster | TBD by SPIKE-01 | VNC candidate | Unverified | Widgets are first-class support target |
| 6.8.x | Embedded Linux / RK3588 | EGLFS/OpenGL | QOpenGLWidget | TBD by SPIKE-01 | VNC candidate | Unverified | Dedicated GL capture may be required |
| 6.8.x | Embedded Linux / RK3588 | mixed | QQuickWidget | TBD by SPIKE-01 | VNC candidate | Unverified | Composition must be validated |

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
