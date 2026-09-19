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

### Listener address family and reachability

**V1 is IPv4-first, and the IPv6 rows below are measured rather than promised.** The contract is pinned by
`src/remoteaccess/tests/test_listener_address_matrix.cpp`, which chooses every port through the operating system, so the
rows are deterministic and need no fixed port.

Measured on **Windows x86_64 / Qt 6.8.3** (the Linux column is still pending #109 - it must not be inferred from this
one):

| What the application asks for | Measured behaviour |
|---|---|
| default (no address configured) | `127.0.0.1` - loopback, unchanged |
| `127.0.0.1` | binds; `stop()` releases the endpoint, so the same port is immediately rebindable |
| an address not assigned to any interface | fails before `Running`; the state stays `Stopped` and nothing is left listening |
| a port already in use | fails before `Running`; the state stays `Stopped` and nothing is left listening |
| `0.0.0.0` | binds; reachable through `127.0.0.1` |
| `::1` | binds; reachable |
| `::` | binds and is reachable through `::1`, but **not** through `127.0.0.1` |

**The `::` row is an explicit V1 boundary: it is an IPv6-only listener on this platform, not a dual-stack one.** An
application that needs both families should listen for them deliberately rather than rely on a wildcard being
dual-stack. Hostnames and DNS names are **not** accepted - V1 is numeric-address only - and there is no silent fallback:
a rejected address never broadens to a wildcard scope.

Two further limits apply to every row: a rejected bind currently reports only "transport failed to start", which does not
name the address or the port (so treat a failed start as needing the configuration to be inspected rather than the message
alone); and all of the above is an **address-family** statement, not a security one - a non-loopback listener is still an
explicit widening of the trust boundary, as `security-model.md` and `security.md` describe.

### Capture coverage

- Widgets: the production correctness baseline uses Qt public widget rendering/capture behavior.
- Quick: the production correctness baseline uses public asynchronous item capture.
- Transparent QPA has a separately qualified multi-surface composition path and exact capture-family matrix.
- `QOpenGLWidget`, Quick3D, custom FBO/OpenGL and `QQuickWidget` evidence is configuration-specific and must not be generalized across all modes/backends.
- Generic `QWindow`, `QOpenGLWindow` and arbitrary foreign/native OS windows without a qualified adapter are not V1 Transparent QPA claims.
- Desktop x86 evidence does not imply Embedded Linux/EGLFS/OpenHarmony support.

See the QPA capture-classification document and `compatibility.md` for exact status.

### Input lifecycle, abrupt disconnect and explicit stop

The normalized input contract covers pointer/button/wheel, logical key/modifier and committed-text behavior. Full IME composition/dead-key/international-layout parity is not claimed.

#90's held-key/button disconnect correction has been absorbed into the single V1 candidate #106. It balances recognized held input when a viewer disappears abruptly. The candidate also now treats explicit HyRemote runtime stop as a separate terminal target-input boundary: after transport/Core callbacks are quiescent, pending remote events not yet delivered to Qt are discarded, while supported remote key/button state already delivered to the target is balanced before the target input adapter retires. QML reuses the same runtime semantics, and QPA propagates terminal cleanup to child surfaces before retiring their adapters.

The bounded RFB candidate also treats simultaneous viewers as contributors to one shared logical Qt input device rather than as independent virtual keyboards/mice. Per-viewer protocol state remains isolated, but overlapping holds of the same normalized key or left/middle/right button are internally reference-counted: one viewer disconnecting or releasing cannot release the shared target while another viewer still holds the same logical input. The target transition returns to up only when the final holder releases/disconnects. Aggregate remote modifier state is used for the shared target. HyRemote V1 does not expose per-viewer cursors, independent focus contexts, input ownership arbitration, or a live per-client authorization API.

These repository behaviors are covered by deterministic transport, facade, Widgets, Quick and QPA tests. On 2026-09-17, some PR #106 Windows/Linux hosted jobs received real runners and executed repository configure/build/test steps, exposing concrete CI/repository defects that have since been corrected on the convergence branch. Runner assignment remains intermittent, however, and the current exact candidate still lacks the required complete Windows/Linux passing execution envelope. Therefore milestone authorities must not yet describe abrupt-disconnect, concurrent-viewer held-state isolation or explicit-stop cleanup as accepted release support solely from repository implementation or partial historical runs.

The following remain explicit V1 boundaries rather than hidden promises:

- terminal target-input cleanup is an internal composition contract, not a new application-facing reset API;
- simultaneous viewers share one logical remote input device; HyRemote V1 does not promise independent per-viewer pointer/focus state;
- only supported normalized key/button state can be balanced; unsupported IME/composition semantics are not reconstructed or guessed;
- pending input that never reached the Qt GUI is dropped on explicit stop rather than replayed after stop;
- repeated teardown is intended to be idempotent and must not synthesize duplicate releases;
- physical local-input neutrality after an abrupt disconnect or explicit stop/policy transition still requires #109 evidence on each claimed OS.

Ordinary key/button release and reconnect paths are separately represented in product-fit tests.

### Connected-viewer diagnostics

Lifecycle `Running` means that the remote runtime/listener is active; it does **not** mean a viewer is connected.

#91's backend-neutral `RemoteAccess::connectedClientCount()` implementation has also been absorbed into #106. E1/E2/E3/E5 product-fit requires the expected connect/disconnect/reconnect lifecycle. That evidence remains acceptance-pending until the reference jobs actually execute and pass on the exact candidate.

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

The clean installed-QPA acceptance fixture is intentionally bound to the real E4 Qt-only application source rather than maintaining a second look-alike application. This prevents fixture success from hiding drift in the defining existing-application example.

### Physical local + remote coexistence

Hosted offscreen/software/Xvfb tests can prove viewer → transport → shared runtime → Qt target behavior. They do **not** prove that a physical local display and local keyboard/mouse remained usable at the same time.

Physical local-visible/local-input coexistence for the GA cross-mode envelope is tracked by #109, including E1/E2/E3/E4 on Windows/Linux as applicable. The repository execution/evidence template is `v1-physical-acceptance.md`; its existence or partial completion is not physical acceptance. For control-enabled paths the required evidence includes both abrupt-disconnect held-state cleanup and explicit HyRemote stop/policy-transition cleanup with no late queued remote input. This is a legitimate local-only evidence task once repository/hosted prerequisites reach its execution gate. It must not be replaced by an offscreen screenshot or inferred native-window creation result.

### Hosted CI infrastructure

#74 is currently an **intermittent hosted-runner assignment problem**. Historical jobs with `steps=[]`/`steps=null` and `runner_id=0` executed no repository commands and prove neither pass nor code failure. Other PR #106 jobs on 2026-09-17 did receive real Windows/Linux runners and reached concrete configure/build/test steps; those step-level results are valid evidence and repository-owned failures have been acted on.

Newer exact-candidate runs can still remain queued with no step execution. Such queued/no-step runs do not satisfy #104, and a prior partial run does not substitute for a complete current-candidate Windows/Linux pass. Repository work continues on the single V1 candidate #106; no release branch or milestone tag may be authorized from incomplete hosted evidence.

## Qt version envelope

Qt 6.8.x is the current public V1 reference line for public-API modes; current automated product work uses Qt 6.8.3. Transparent QPA is more narrowly exact-version-coupled to Qt 6.8.3.

Issue #57 tracks the qualification of Qt 5.15 and additional Qt LTS lines. That is x86 work, so it is a **V1.0.0.0**
requirement rather than a post-V1 one: only work that must run on an embedded platform may stay outside V1, because
embedded platforms are far less debuggable than x86. A newly released Qt version enters qualification; it is not
automatically a supported HyRemote configuration.
