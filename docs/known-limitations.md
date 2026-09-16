# Known Limitations

This file separates implemented repository behavior from released support claims. `docs/compatibility.md` remains the per-configuration evidence matrix.

## V1 x86 milestone boundary

HyRemote V1 targets supported `QWidget` and `QQuickWindow` application paths on Windows x86_64 and Linux x86_64 through Embedded C++, Declarative QML and Transparent QPA Proxy. Milestone issues #30, #31 and #32 remain open until their required executable/reference-environment evidence is complete.

Implementation in Draft PR #106 is not itself a released support claim.

### Product artifact boundary

V1 intentionally fixes the normal installed surface to keep consumption simple:

- Core is a static source/internal composition component and is **not installed/exported** as a V1 SDK target;
- `HyRemote::RemoteAccess` is the one shared C++ product library and the one exported normal CMake target;
- `qhyremote` is a Transparent QPA platform MODULE payload selected through the deployment helper and is **not** an installed consumer link target;
- QML is a thin declarative payload over the same shared runtime; its backing library is not a second C++ SDK target.

Static `RemoteAccess` consumption is not a second V1 product personality. `BUILD_SHARED_LIBS` must not be interpreted as a supported switch between two normal application distribution models.

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

#90's held-key/button disconnect correction has been absorbed into the single V1 candidate #106. It balances recognized held input when a viewer disappears abruptly, but the exact Windows/Linux product-fit jobs still have not executed because #74 prevents runner assignment. Therefore the milestone authorities must not yet claim this correctness gate as accepted.

Ordinary key/button release and reconnect paths are separately represented in product-fit tests.

### Connected-viewer diagnostics

Lifecycle `Running` means that the remote runtime/listener is active; it does **not** mean a viewer is connected.

#91's backend-neutral `RemoteAccess::connectedClientCount()` implementation has also been absorbed into #106. E1/E2/E3/E5 product-fit requires the expected connect/disconnect/reconnect lifecycle. That evidence remains acceptance-pending until the reference jobs actually execute.

Applications and documentation must not infer Connected from Running.

### QML initialization lifecycle

Declarative `RemoteAccess { target: window; enabled: true }` defers the actual runtime start until QML component completion so initial target/policy bindings can settle. The property is a request during object construction, not permission for an implicit listener before component completion.

A failed start remains transactional: `enabled` returns to false and product-level error state is exposed. This convenience does not weaken the shared C++ rule that construction itself is inert.

### Performance and acceleration

V1 is a correctness-first CPU-readable frame baseline. It does not promise:

- universal zero-copy capture;
- DMA-BUF/GBM production support;
- hardware H.264/RKMPP support;
- optimized per-surface damage on every Qt application type;
- universal Quick3D/custom-OpenGL acceleration.

Those remain replaceable implementation/backend concerns and may not alter the stable application product model merely to optimize one platform.

### Multi-window and desktop scope

HyRemote targets a Qt application, not an arbitrary whole operating-system desktop.

Embedded C++/QML do not gain a blanket arbitrary-native-window claim. Transparent QPA has a dedicated multi-surface/composite path for supported application-owned QWidget and QQuickWindow families, including dialog/popup/window churn, while unsupported foreign/native families remain explicit.

### Transparent QPA private ABI

Qt does not guarantee QPA source/binary compatibility. The current V1 QPA package is qualified specifically for **Qt 6.8.3** and delegates to `qwindows` on Windows and `qxcb` on the Linux reference path.

Do not infer compatibility with another Qt patch/minor, Wayland, EGLFS or a different native delegate. QPA package deployment enforces the exact qualified Qt version.

### Deployment scope

Normal C++/QML deployment uses `hyremote_deploy()` to carry the shared `HyRemoteRemoteAccess` runtime. Transparent QPA uses the same helper to carry that runtime plus the package-owned `qhyremote` payload. Applications do not link a QPA CMake target.

A deployment that only works because the original HyRemote SDK/build tree is still on `PATH`, `LD_LIBRARY_PATH`, `QT_PLUGIN_PATH` or `QT_QPA_PLATFORM_PLUGIN_PATH` does not satisfy the V1 product contract.

### Physical local + remote coexistence

Hosted offscreen/software/Xvfb tests can prove viewer → transport → shared runtime → Qt target behavior. They do **not** prove that a physical local display and local keyboard/mouse remained usable at the same time.

Physical local-visible/local-input coexistence for the GA cross-mode envelope is tracked by #109, including E1/E2/E3/E4 on Windows/Linux as applicable. This is a legitimate local-only evidence task once repository/hosted prerequisites reach its execution gate. It must not be replaced by an offscreen screenshot or inferred native-window creation result.

### Hosted CI infrastructure

#74 currently causes multiple independent GitHub Actions jobs to terminate before a runner is assigned (`steps=[]`, `runner_id=0`). Such runs execute no repository commands and therefore prove neither pass nor failure of the code under test.

Repository work continues on the single V1 candidate #106. No release branch or milestone tag may be authorized from unexecuted hosted jobs.

## Qt version envelope

Qt 6.8.x is the current public V1 reference line for public-API modes; current automated product work uses Qt 6.8.3. Transparent QPA is more narrowly exact-version-coupled to Qt 6.8.3.

Issue #57 tracks post-V1 qualification of Qt 5.15 and additional Qt LTS lines. A newly released Qt version enters qualification; it is not automatically a supported HyRemote configuration.
