# HyRemote Compatibility Matrix

Status: **V1.0.0.0 qualification pending; no GA support row may be marked Supported until required evidence actually executes.**

HyRemote does not infer support from API similarity or from a different operating system. The V1 release target is Windows x86_64 + Linux x86_64. Embedded Linux is the next platform expansion and is not a V1 GA blocker unless the x86 implementation exposes a real architectural defect.

## Status definitions

- **Supported** — accepted release evidence exists for the exact claimed environment.
- **Candidate** — implementation and acceptance coverage exist, but mandatory release evidence is incomplete/unexecuted.
- **Experimental** — bounded historical or development evidence exists but is not a product compatibility promise.
- **Unsupported** — known architectural/implementation limitation for the stated combination.
- **Unverified** — no sufficient evidence; no support claim may be inferred.

## V1.0.0.0 reference matrix

The integrated GA workflow qualifies exact Qt 6.8.3 on both reference operating systems. The public C++/QML direction is the Qt 6.8 LTS line, but a different 6.8.x patch is not automatically Supported merely because the public API compiles.

| Qt | OS / architecture | Mode | Native/QPA path | Application scope | Status | Required evidence |
| --- | --- | --- | --- | --- | --- | --- |
| 6.8.3 | Windows x86_64 | Embedded C++ | public Qt APIs | supported QWidget + QQuickWindow targets | Candidate | #30 + #104 executable Windows acceptance + required physical evidence |
| 6.8.3 | Linux x86_64 | Embedded C++ | public Qt APIs | supported QWidget + QQuickWindow targets | Candidate | #30 + #104 executable Linux acceptance + required physical evidence |
| 6.8.3 | Windows x86_64 | Declarative QML | thin wrapper over shared `RemoteAccess` | QML target over supported Quick path | Candidate | #31 + #104 executable Windows acceptance |
| 6.8.3 | Linux x86_64 | Declarative QML | thin wrapper over shared `RemoteAccess` | QML target over supported Quick path | Candidate | #31 + #104 executable Linux acceptance |
| 6.8.3 | Windows x86_64 | Transparent QPA | `hyremote` -> native `qwindows` delegate | qualified application-owned QWidget/QQuickWindow surfaces | Candidate | #32 + #104 + required physical native local+remote evidence |
| 6.8.3 | Linux x86_64 | Transparent QPA | `hyremote` -> native `qxcb` delegate | qualified application-owned QWidget/QQuickWindow surfaces | Candidate | #32 + #104 + required physical native local+remote evidence |

#74 currently prevents the required hosted jobs from receiving runners. Therefore none of these rows is upgraded to Supported from repository implementation alone.

## V1 artifact compatibility contract

The normal V1 artifact shape is itself part of compatibility:

| Artifact | V1 form | Application expectation |
| --- | --- | --- |
| `HyRemote::RemoteAccess` | shared library | normal C++ application links this one HyRemote product target |
| `hyremote-core` | static/internal | not a separately deployed normal application runtime |
| `qhyremote` | Qt platform MODULE | existing Qt app can use `-platform hyremote` without HyRemote linkage |
| QML `HyRemote` module | thin wrapper | uses the same shared `RemoteAccess` runtime |

A build where `BUILD_SHARED_LIBS` changes this normal product model is not the frozen V1 artifact contract.

## V1 capture/application scope

### Widgets

The production correctness path captures supported QWidget top levels through Qt public widget rendering into owned CPU-readable frame storage. QPA composes qualified application-owned top-level surfaces into one remote application canvas.

### Qt Quick

The production correctness path uses public asynchronous `QQuickWindow::contentItem()->grabToImage()` behavior. It is a correctness baseline, not a zero-copy/performance promise.

### Configuration-specific graphics cases

Historical Windows Qt 6.8.3 spike evidence exists for QWidget raster, QOpenGLWidget, QQuickWidget, Quick 2D, Quick3D and custom Quick/OpenGL cases. Those measurements are useful architecture evidence but do not automatically become release support for every integration mode or graphics backend.

Detailed evidence remains in:

- `docs/capture-spike.md`;
- `docs/async-capture-spike.md`;
- `docs/widgets-capture.md`;
- `docs/qpa-capture-classification-qt-6.8.3.md`.

Important V1 limits:

- arbitrary generic/foreign native `QWindow` capture is not a Transparent QPA claim;
- a working QWidget raster case does not imply every QOpenGLWidget/QQuickWidget configuration;
- a working Quick 2D case does not imply every Quick3D/custom-FBO/backend combination;
- QPA claims are exact-Qt/private-ABI qualified, not generic `Qt 6.8+` promises.

## V1 transport/viewer boundary

The current production correctness transport is bounded RFB 3.8 with SecurityType None. It is intended to establish remote-view/input correctness and standard VNC interoperability, not Internet-safe security.

The V1 automated product path uses maintained `vncdotool` plus raw protocol checks. Additional viewer products such as TigerVNC can be added to the compatibility matrix only after versioned acceptance evidence exists.

## Post-V1 embedded expansion

The following are important next-platform candidates but are **not V1.0.0.0 GA acceptance rows**:

| Qt / target | Platform direction | Application types | Status | Notes |
| --- | --- | --- | --- | --- |
| Qt 6.8.x / RK3588 | Embedded Linux / EGLFS/OpenGL ES | Widgets + Quick | Unverified | high-priority post-V1 target; validate on actual BSP/display/input stack |
| Qt 6.8.x / NXP i.MX class | Embedded Linux | Widgets + Quick | Unverified | high-priority post-V1 platform family |
| future qualified line | OpenHarmony | Widgets/Quick feasibility | Unverified | longer-term direction, not V1 gate |

DMA-BUF/GBM, RKMPP/hardware encoding and other platform acceleration remain optional implementation optimizations. They must not force a more complex normal application API.

## Evidence required to upgrade a row

Record at minimum:

- exact Qt version;
- OS/BSP and architecture;
- compiler/toolchain;
- native QPA/graphics backend;
- application/sample and integration mode;
- capture/input path exercised;
- transport/viewer version;
- resolution/DPR where relevant;
- connect/view/input/disconnect/reconnect results;
- local rendering/input impact where required;
- deployment/runtime-path result;
- known limitations;
- link to reproducible test/issue/acceptance evidence.

## Rules

1. Candidate/Unverified/Experimental must never be described as Supported.
2. Windows evidence does not substitute for Linux evidence or vice versa.
3. x86 desktop evidence does not imply Embedded Linux/EGLFS support.
4. public Qt API compatibility does not imply QPA private-ABI compatibility.
5. graphics-family evidence is configuration-specific unless an acceptance matrix explicitly broadens it.
6. hardware acceleration support is independent of the stable application-facing product contract.
7. V1 usability remains one shared C++ facade or the QPA plugin path; platform optimization must stay behind that boundary.
