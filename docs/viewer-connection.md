# Viewer Connection and Remote Control

HyRemote V1 uses one bounded internal RFB correctness transport behind all three integration modes. The protocol backend is not part of the application-facing API.

## Start the target application

### Embedded C++

The application explicitly calls `RemoteAccess::start()`.

### Declarative QML

The application explicitly requests start through `enabled: true`; the wrapper applies that request after QML component completion.

### Transparent QPA

The application is deliberately launched through the `hyremote` platform plugin, for example:

```text
MyApp -platform hyremote
```

The default product configuration in every mode listens on loopback port 5900 and leaves remote input disabled.

## Connect

With the default configuration, point a standard VNC/RFB client at:

```text
127.0.0.1:5900
```

Viewer syntax varies. The automated product-fit suite uses maintained `vncdotool` as an interoperability client; a GA compatibility entry must record the exact viewer/version used for its claim.

## View-only versus control

Remote viewing and remote input are separate policies. The safe product default is view-only.

Embedded C++ explicitly enables control before start:

```cpp
remote.setRemoteInputEnabled(true);
remote.start();
```

QML exposes the equivalent `remoteInputEnabled` policy while stopped.

Transparent QPA keeps zero-source-change policy in launch configuration:

```text
-platform hyremote                         # view-only
-platform hyremote:hyremote-input=true     # remote control
```

Unsupported key/IME behavior remains a documented limitation rather than being approximated silently. See `docs/reference/input-model.md` and `docs/reference/known-limitations.md`.

## Disconnect and reconnect

A viewer may disconnect and reconnect without recreating the target application. Remote client lifetime is owned by the shared transport; the Qt target and local application continue independently.

`RemoteAccess::stop()` tears down the Embedded C++/QML session/listener and returns the facade to Stopped. Transparent QPA retains its one application session across supported surface churn and normal viewer reconnects until the plugin/controller lifecycle ends.

The #90 held-key/button disconnect correction is already absorbed into the V1 candidate: recognized remote pressed state is balanced when a viewer disappears abruptly. Exact dual-OS acceptance remains pending because #74 prevents the reference product-fit jobs from executing.

## Listening versus connected

`RemoteAccessState::Running` means the remote runtime/listener is running. It is not equivalent to “a viewer is connected”.

The current product facade/QML wrapper exposes backend-neutral `connectedClientCount()` diagnostics. E1/E2/E3/E5 product-fit uses the expected `0 → 1 → 0 → 1 → 0` lifecycle across connect, disconnect and reconnect. Those commands are implemented but are not accepted support evidence until the reference jobs actually run.

Transparent QPA does not expose a second application diagnostics API to an otherwise unmodified application merely to mirror this value.

## Local + remote coexistence

Hosted/offscreen/Xvfb viewer tests prove only the path they execute. V1 additionally requires physical native local display/input to remain usable while the remote viewer is active where the integration mode claims coexistence.

That cross-mode physical evidence envelope is tracked by #109 and is separate from standard-viewer interoperability.

## Security boundary

The current RFB correctness baseline uses **SecurityType None**: no transport authentication and no transport encryption. It is suitable for loopback/trusted test use, not direct untrusted-network exposure.

Read [`security.md`](reference/security.md) for the implemented V1 security boundary before changing the bind address away from loopback. [`security-model.md`](reference/security-model.md) is broader future threat-model context, not a claim that authentication/encryption already exists.
