# Known Limitations

This file separates implemented repository behavior from released support claims. `docs/compatibility.md` remains the per-configuration evidence matrix.

## V1 x86 milestone boundary

HyRemote V1 targets `QWidget` and `QQuickWindow` application paths on Windows x86_64 and Linux x86_64 through three mandatory integration modes: Embedded C++, Declarative QML and Transparent QPA Proxy. Their milestone issues #30, #31 and #32 remain open until required executable/reference-environment evidence is complete.

Implementation in an open Draft PR is not itself a released support claim.

### Transport security

The current bounded RFB correctness transport uses **SecurityType None**. It provides neither transport authentication nor transport encryption for direct public/untrusted-network exposure. Loopback is the safe default; remote input is opt-in.

See [`security.md`](security.md) for the implemented V1 security boundary. [`security-model.md`](security-model.md) is broader architecture/threat-model context and includes future authentication/encryption requirements that are not current product capabilities.

### Capture coverage

- Widgets: the production correctness baseline uses Qt public widget rendering/capture behavior.
- Quick: the production correctness baseline uses public asynchronous item capture.
- Transparent QPA has a separately qualified multi-surface composition path and exact capture-family matrix.
- `QOpenGLWidget`, Quick3D, custom FBO/OpenGL and `QQuickWidget` evidence is configuration-specific and must not be generalized across all modes/backends.
- Generic `QWindow`, `QOpenGLWindow` and arbitrary foreign/native OS windows without a qualified adapter are not V1 Transparent QPA claims.
- Desktop x86 evidence does not imply Embedded Linux/EGLFS/OpenHarmony support.

See the QPA capture-classification document and `compatibility.md` for exact status.

### Input and abrupt disconnect

The normalized input contract covers pointer/button/wheel, logical key/modifier and committed-text behavior. Full IME composition/dead-key/international-layout parity is not claimed.

#90 / Draft PR #93 implements bounded held-key/button tracking and balancing releases when an RFB viewer disconnects abruptly. The implementation is present, but the required exact Windows/Linux product-fit jobs still have not executed because #74 prevents runner assignment. Therefore #30/#31/#32 must not yet claim this correctness gate as accepted.

Ordinary key/button release and reconnect paths are separate and already represented in their corresponding product-fit tests.

### Connected-viewer diagnostics

Lifecycle `Running` means that the remote runtime/listener is active; it does **not** mean a viewer is connected.

#91 / Draft PR #92 implements the backend-neutral `RemoteAccess::connectedClientCount()` product diagnostic. The same diagnostic is consumed by the QML wrapper and E1/E2/E3/E5 branches, whose product-fit tests require `0 → 1 → 0 → 1 → 0` across connect/disconnect/reconnect. Those tests remain acceptance-pending until the reference jobs execute.

Applications and documentation must not infer Connected from Running.

### Performance and acceleration

V1 is a correctness-first CPU-readable frame baseline. It does not promise:

- universal zero-copy capture;
- DMA-BUF/GBM production support;
- hardware H.264/RKMPP support;
- optimized per-surface damage on every Qt application type;
- universal Quick3D/custom-OpenGL acceleration.

Those remain replaceable implementation/back-end concerns and may not alter the stable application product model merely to optimize one platform.

### Multi-window and desktop scope

HyRemote targets a Qt application, not an arbitrary whole operating-system desktop.

Embedded C++/QML do not gain a blanket arbitrary-native-window claim. Transparent QPA has a dedicated multi-surface/composite qualification chain for supported application-owned QWidget and QQuickWindow families, including tested dialog/popup/window churn, while unsupported foreign/native families remain explicit.

### Transparent QPA private ABI

Qt does not guarantee QPA source/binary compatibility. The current V1 QPA package is qualified specifically for **Qt 6.8.3** and delegates to `qwindows` on Windows and `qxcb` on the Linux reference path.

Do not infer compatibility with another Qt patch/minor, Wayland, EGLFS or a different native delegate. QPA package deployment also enforces the exact qualified Qt version.

### Physical local + remote coexistence

Hosted offscreen/software tests can prove viewer → transport → shared runtime → Qt target behavior. They do **not** prove that a physical local display and local keyboard/mouse remained usable at the same time.

Physical local-visible/local-input coexistence remains a final #30/#32/#33 acceptance boundary. When repository/hosted implementation has otherwise converged, this is a legitimate local-only evidence task and may require the designated reference machine. It must not be replaced by an offscreen screenshot or inferred native-window creation result.

### Hosted CI infrastructure

#74 currently causes multiple independent GitHub Actions jobs to terminate before a runner is assigned (`steps=[]`, `runner_id=0`). Such runs execute no repository commands and therefore prove neither pass nor failure of the code under test.

Repository work continues, but Draft PRs, milestone release branches and release tags must remain gated on actual executable evidence according to `docs/git-flow-release.md` once that policy is integrated.

## Qt version envelope

Qt 6.8.x is the current public V1 reference line for public-API modes; current automated product work uses Qt 6.8.3. Transparent QPA is more narrowly exact-version-coupled to Qt 6.8.3.

Issue #57 tracks post-V1 qualification of Qt 5.15 and additional Qt LTS lines. A newly released Qt version enters qualification; it is not automatically a supported HyRemote configuration.
