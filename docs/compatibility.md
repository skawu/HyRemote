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

### Qt line policy: LTS only, adaptive otherwise

HyRemote targets **Qt LTS lines only**. The qualified reference line is **6.8.3 (Qt 6.8 LTS)**, and the public C++/QML direction is that same LTS line.

The build detects the Qt it is given and adapts instead of refusing:

- an **LTS line** (5.15, 6.2, 6.5, 6.8) is reported as such during configure;
- a **non-LTS line** produces one actionable warning and then **builds anyway**, against the **6.8 API baseline** that every feature search in this repository already asks for - the most compatible configuration available for a line this project has not qualified - so a user on a newer Qt is never blocked;
- a Qt **below 6.8** cannot configure the product targets at all, and the configure error says so and names the supported way to build Core alone on purpose.

The **Transparent QPA payload is not part of that adaptation**: it is qualified against **exactly Qt 6.8.3 private ABI** and is skipped unless that exact SDK is present, because a public-API-compatible Qt is not a private-ABI-compatible one. Qualifying any other line is owned by #57, which carries the Supported / Experimental / Unsupported conclusion for non-reference lines.

| Qt | OS / architecture | Mode | Native/QPA path | Application scope | Status | Required evidence |
| --- | --- | --- | --- | --- | --- | --- |
| 6.8.3 | Windows x86_64 | Embedded C++ | public Qt APIs | supported QWidget + QQuickWindow targets | Candidate | #30 + #104 executable Windows acceptance + #109 physical coexistence |
| 6.8.3 | Linux x86_64 | Embedded C++ | public Qt APIs | supported QWidget + QQuickWindow targets | Candidate | #30 + #104 executable Linux acceptance + #109 physical coexistence |
| 6.8.3 | Windows x86_64 | Declarative QML | thin wrapper over shared `RemoteAccess` | QML target over supported Quick path | Candidate | #31 + #104 executable Windows acceptance + #109 physical coexistence |
| 6.8.3 | Linux x86_64 | Declarative QML | thin wrapper over shared `RemoteAccess` | QML target over supported Quick path | Candidate | #31 + #104 executable Linux acceptance + #109 physical coexistence |
| 6.8.3 | Windows x86_64 | Transparent QPA | `hyremote` -> native `qwindows` delegate | qualified application-owned QWidget/QQuickWindow surfaces | Candidate | #32 + #104 + #109 physical native local+remote evidence |
| 6.8.3 | Linux x86_64 | Transparent QPA | `hyremote` -> native `qxcb` delegate | qualified application-owned QWidget/QQuickWindow surfaces | Candidate | #32 + #104 + #109 physical native local+remote evidence |

#74 currently makes hosted runner assignment intermittent. Some PR #106 jobs on 2026-09-17 received real Windows/Linux runners and produced valid configure/build/test evidence, while newer exact-candidate jobs can still remain queued with no executed steps. No row is upgraded to Supported until the required current-candidate #104 Windows/Linux pass and distinct #109 physical/native evidence are complete.

## V1 artifact compatibility contract

The installed V1 product surface is deliberately smaller than the repository's internal architecture:

| Artifact | V1 form | Application expectation |
| --- | --- | --- |
| `HyRemote::RemoteAccess` | shared library + exported CMake target | the one normal C++ HyRemote product target |
| `hyremote-core` | static source/internal component | not installed/exported as a V1 SDK target and not a separately deployed runtime |
| `qhyremote` | Qt platform MODULE payload | selected through `hyremote_deploy(... QPA)` / `-platform hyremote`; no installed `HyRemote::QpaPlatform` link target |
| QML `HyRemote` module | declarative payload over shared runtime | consumed through `import HyRemote`; backing library is not a second C++ SDK target |

`BUILD_SHARED_LIBS` does not change this normal V1 product model.

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

Input compatibility evidence is lifecycle-sensitive. A row cannot be upgraded merely because ordinary pointer/key events work: required evidence must also cover abrupt viewer disconnect with held supported state and explicit HyRemote stop/policy transition with delivered held state, proving that the next/local input state is neutral and that pending undelivered remote input is not injected after stop.

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
- abrupt-disconnect held-input cleanup result where control is enabled;
- explicit remote-runtime stop/policy-transition held-input cleanup result and confirmation that no pending remote input was delivered after stop;
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
7. V1 usability remains one exported shared C++ facade, a declarative QML payload, or the QPA plugin launch path; platform optimization must stay behind that boundary.
