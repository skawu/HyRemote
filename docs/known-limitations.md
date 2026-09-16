# Known Limitations

This file separates the current correctness/product baseline from capabilities that are not yet release claims. `docs/compatibility.md` remains the per-configuration evidence matrix.

## V0.0.1 Embedded C++ baseline

The intended first milestone covers `QWidget` and `QQuickWindow` applications on Windows x86_64 and Linux x86_64 through the same `HyRemote::RemoteAccess` facade. Final milestone acceptance remains #30 and must not be inferred solely from merged implementation components.

### Transport security

The current bounded RFB correctness transport uses **SecurityType None**. It provides neither transport authentication nor transport encryption for direct public/untrusted-network exposure. Loopback is the safe default; remote input is opt-in.

See [`security.md`](security.md) for the implemented V1 security boundary. [`security-model.md`](security-model.md) is the broader architecture/threat-model document and includes future authentication/encryption requirements that are not current product capabilities.

### Capture coverage

- Widgets: correctness baseline uses Qt public widget rendering/capture behavior; separate native top-level windows such as some popups/dialogs require explicit compatibility evidence for the integration mode claiming them.
- Quick: correctness baseline uses public asynchronous item capture; visible/same-scene content is the validated design basis, while hidden/minimized/occluded and native-window variants need per-configuration evidence.
- Quick3D, custom FBO/OpenGL, `QOpenGLWidget`, and `QQuickWidget` have evidence only in specifically recorded configurations and are not automatically part of every Embedded C++/QPA support claim.
- Desktop x86 evidence does not imply Embedded Linux/EGLFS behavior.

### Input

The normalized input contract covers the product-required pointer/button/wheel, logical-key/modifier and committed-text baseline. Full IME composition/dead-key/international-layout parity is not claimed.

**Open V1 correctness blocker #90:** the current RFB client state records held pointer buttons/modifiers per connection, but the disconnect path still needs a reviewed balancing-release implementation for a viewer that disappears while input is held. Until #90 lands and is rerun through the product path, V0.0.1/#30 must not claim that abrupt viewer disconnect can never leave a Qt key/button state stuck.

This is distinct from ordinary key/button up messages, which are already normalized and delivered, and from reconnect itself, which is already part of the viewer product-fit path.

### Connected-viewer diagnostics

Lifecycle `Running` means that the remote runtime/listener is running; it does not mean a viewer is connected.

#91 / Draft PR #92 adds a backend-neutral connected-client count to the public facade so E1/E2/E3/E5 can display truthful listening/connected status without backend hooks. Until that dependency lands, applications must not infer viewer connection from lifecycle state alone.

### Performance/acceleration

V0.0.1 is a correctness-first CPU-readable frame baseline. It does not promise:

- zero-copy capture;
- DMA-BUF/GBM production support;
- hardware H.264/RKMPP;
- optimized per-surface damage on every Qt application type;
- universal Quick3D/custom-OpenGL acceleration.

Those capabilities must remain replaceable backends rather than alter the public product API.

### Multi-window and desktop scope

HyRemote targets an application, not an arbitrary whole operating-system desktop. Composition of multiple independent top-level/native windows is not a blanket Embedded C++ claim unless the exact application/capture path is validated.

Transparent QPA has a separate multi-surface/composite qualification chain (#76 and successors) and must not be generalized to unrelated native/foreign window families outside its recorded support matrix.

### Physical local + remote coexistence

Hosted offscreen/software tests can prove viewer → transport → shared runtime → Qt target behavior. They do **not** prove that a physical local display and local keyboard/mouse remained usable at the same time.

That real native-local coexistence evidence remains a final #30/#32/#33 acceptance boundary. It may require a local/reference machine when repository/hosted work has otherwise converged; it must not be replaced by an offscreen screenshot claim.

## Later mandatory V1 modes

Declarative QML and Transparent QPA Proxy are mandatory for V1.0.0.0 and have their own milestone gates (#31 and #32). Their implementation branches/PRs do not become supported release claims until their acceptance evidence executes on both reference operating systems.

The QPA Proxy is additionally tied to exact Qt private-API compatibility and must preserve the native local platform path. It cannot be generalized from a single Qt/OS smoke result.

## Qt version envelope

Qt 6.8.x is the current V1 reference line. QPA qualification is intentionally exact-version-coupled to Qt 6.8.3 in the current V1 chain. Issue #57 tracks post-V1 qualification of Qt 5.15 and additional LTS lines. A newly released Qt LTS enters qualification; it is not automatically a supported HyRemote configuration.
