# HyRemote Security

HyRemote is remote-access infrastructure. A working viewer connection does not by itself make a deployment secure.

This page describes the **current product security behavior**. Capabilities that are not available yet are marked as TODO.

## V0.1 security boundary

V0.1 is a Developer Preview with a loopback-first security boundary.

The Shared Runtime applies the same security model to the C++ API, QML API, Generic Plugin, and QPA frontends.

Default behavior:

- bind address: `127.0.0.1`;
- port: `5921`;
- remote input: disabled;
- constructing a C++ `HyRemote::RemoteAccess` object does not open a listener;
- QML starts only after `enabled: true` is requested;
- Generic/QPA start only when explicitly activated through their Qt startup mechanisms;
- unauthenticated non-loopback exposure is rejected;
- malformed/incomplete clients remain subject to bounded transport limits;
- secrets must not be written to normal diagnostics.

The default V0.1 build profile does **not** imply that authenticated or encrypted transport is present merely because the public API contains security-profile values.

Do not expose V0.1 directly to the public Internet.

## Security profiles

### Insecure

`Insecure` is the normal V0.1 development profile.

- no viewer authentication;
- no transport encryption;
- loopback is required;
- remote input remains a separate opt-in policy.

If `Insecure` is combined with a non-loopback address, `start()` fails before any listener is opened. HyRemote does not silently widen an unauthenticated listener.

### Authenticated

`Authenticated` is a **conditional capability**.

It is usable only when all of the following are true:

1. the HyRemote build includes the transport-security capability;
2. a security descriptor path is configured;
3. the descriptor is valid for the selected profile.

When those conditions are satisfied, the current transport uses RFB VNC authentication for viewer authentication.

The stream is still **not encrypted**.

If the required build capability is absent, `start()` fails with `SecurityUnavailable` before target, transport, or listener creation. If the descriptor is missing or invalid, startup fails rather than falling back to a weaker profile.

The default V0.1 developer build/profile should therefore be treated as **Insecure loopback-only unless the authenticated capability was explicitly built and configured**.

### AuthenticatedEncrypted

`AuthenticatedEncrypted` is declared in the product model but is **not implemented in V0.1**.

Selecting it always fails closed before a listener is opened, including in builds that can provide `Authenticated` VNC authentication:

```text
AuthenticatedEncrypted
  -> SecurityUnavailable
  -> no target/transport/listener composed
  -> no fallback to Authenticated
  -> no fallback to Insecure
```

A readable certificate/private-key descriptor does not make encrypted transport available by itself.

> **TODO V0.2:** implement the final encrypted transport profile, certificate/private-key policy, protocol/cipher policy, and authenticated encrypted sessions.

## Remote viewing versus remote control

Viewing and control are separate policies.

C++ example:

```cpp
HyRemote::RemoteAccess remote(&window);
remote.setRemoteInputEnabled(true);
remote.start();
```

QML example:

```qml
RemoteAccess {
    target: mainWindow
    remoteInputEnabled: true
    enabled: true
}
```

Generic and QPA expose corresponding startup configuration through their plugin specifications.

Leaving remote input disabled keeps the session view-only. View-only is **not** authentication and is **not** encryption.

## Listener exposure

The safest and default V0.1 listener is:

```text
127.0.0.1:5921
```

Changing the bind address changes the network trust boundary. It does not automatically enable authentication or encryption.

For `Insecure`, non-loopback startup is rejected. For `Authenticated`, a non-loopback listener is possible only when the authenticated build capability and descriptor are valid.

Exact address-family behavior is documented in [`known-limitations.md`](known-limitations.md).

## Connection state is not authorization

`RemoteAccessState::Running` means the Runtime/listener is active. It does not mean a viewer is authenticated or even connected.

`connectedClientCount()` is operational information. It is not a user identity, role, or authorization result.

> **TODO V0.2:** authenticated Session Registry, bounded admission policy, session events, and explicit session termination APIs.

## Input safety

Remote input is normalized and routed only to the attached Qt application target. HyRemote does not use desktop-wide virtual HID / `uinput` injection as the normal product path.

Supported held key/button state is balanced across viewer disconnect and Runtime teardown so a remote peer should not leave the local application stuck in a pressed state.

Unsupported key/composition/IME cases are documented rather than guessed.

## Resource boundaries

The RFB baseline uses bounded behavior for items such as concurrent clients, protocol buffering, handshake lifetime, frame handoff, and GUI input delivery.

These limits reduce accidental or malicious resource growth. They do not make the current transport suitable for direct hostile-Internet exposure.

## Secrets and diagnostics

Passwords, tokens, private keys, and equivalent secrets must not be written to normal logs, error strings, UI diagnostics, or command examples.

Security configuration should use an external descriptor/configuration mechanism instead of placing secrets directly on a process command line.

A descriptor confirms what the operator configured; it does not prove that the selected security mechanism exists in the build.

## Recommended V0.1 deployment profiles

### Local developer / same machine

Use the default `Insecure` loopback listener. Enable remote input only when required.

### Trusted lab or maintenance network

Use `Authenticated` only when the package was explicitly built with the required security capability and a valid descriptor is configured. The stream remains unencrypted, so network reachability still needs an appropriate trust boundary.

### Public Internet

**Not supported as a direct V0.1 deployment profile.**

Do not expose the current HyRemote listener directly to the public Internet.

## Current security capability matrix

| Capability | V0.1 status |
| --- | --- |
| Loopback-first default | Available |
| Remote input off by default | Available |
| Reject `Insecure` non-loopback exposure | Available |
| `Authenticated` API/configuration surface | Available |
| RFB VNC authentication | **Conditional: requires transport-security-enabled build + valid descriptor** |
| Default V0.1 build includes authenticated transport | **No** |
| `AuthenticatedEncrypted` | **Unavailable; fail-closed** |
| Stream encryption | **TODO V0.2** |
| Certificate/private-key production policy | **TODO V0.2** |
| Authenticated session identity/registry | **TODO V0.2** |
| Per-session admission/termination | **TODO V0.2** |
| VPN/tunnel/firewall provisioning | Outside HyRemote product scope |

The concise V0.1 statement is:

> HyRemote V0.1 defaults to an unauthenticated, unencrypted loopback listener with remote input off. `Authenticated` is available only in a transport-security-enabled build with a valid security descriptor and currently provides VNC authentication without encryption. `AuthenticatedEncrypted` is not implemented and always fails closed. Do not expose V0.1 directly to the public Internet.
