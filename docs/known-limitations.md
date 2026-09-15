# Known Limitations

This file separates the current correctness/product baseline from capabilities that are not yet release claims. `docs/compatibility.md` remains the per-configuration evidence matrix.

## V0.0.1 Embedded C++ baseline

The intended first milestone covers `QWidget` and `QQuickWindow` applications on Windows x86_64 and Linux x86_64 through the same `HyRemote::RemoteAccess` facade. Final milestone acceptance remains #30 and must not be inferred solely from merged implementation components.

### Transport security

The current bounded RFB correctness transport uses SecurityType None. It does not provide a production-grade authentication/encryption boundary for direct public/untrusted-network exposure. Loopback is the safe default; remote input is opt-in.

### Capture coverage

- Widgets: correctness baseline uses Qt public widget rendering/capture behavior; separate native top-level windows such as some popups/dialogs require explicit compatibility evidence.
- Quick: correctness baseline uses public asynchronous item capture; visible/same-scene content is the validated design basis, while hidden/minimized/occluded and native-window variants need per-configuration evidence.
- Quick3D, custom FBO/OpenGL, `QOpenGLWidget`, and `QQuickWidget` have spike evidence in selected configurations but are not automatically part of the V0.0.1 supported matrix.
- Desktop x86 evidence does not imply Embedded Linux/EGLFS behavior.

### Input

The first normalized input contract covers the product-required pointer/button/wheel, logical-key/modifier and committed-text baseline. Full IME composition/dead-key/international-layout parity is not claimed.

### Performance/acceleration

V0.0.1 is a correctness-first CPU-readable frame baseline. It does not promise:

- zero-copy capture;
- DMA-BUF/GBM production support;
- hardware H.264/RKMPP;
- optimized per-surface damage on every Qt application type;
- universal Quick3D/custom-OpenGL acceleration.

Those capabilities must remain replaceable backends rather than alter the public product API.

### Multi-window and desktop scope

HyRemote targets an application, not an arbitrary whole operating-system desktop. Composition of multiple independent top-level/native windows is not a blanket V0.0.1 claim unless the exact application/capture path is validated.

## Later mandatory V1 modes

Declarative QML and Transparent QPA Proxy are mandatory for V1.0.0.0 but have their own milestone gates (#31 and #32). Their existence in the roadmap is not a statement that the current V0.0.1 Embedded C++ release already provides those modes.

The QPA Proxy is additionally tied to exact Qt private-API compatibility and must preserve the native local platform path. It cannot be generalized from a single Qt/OS smoke result.

## Qt version envelope

Qt 6.8.x is the current V1 reference line. Issue #57 tracks post-V1 qualification of Qt 5.15 and additional LTS lines. A newly released Qt LTS enters qualification; it is not automatically a supported HyRemote configuration.
