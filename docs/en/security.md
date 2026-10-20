# HyRemote Security Boundary

> Language / 语言: **English** | [中文](../security.md)

HyRemote is remote-access infrastructure. A working viewer connection does not make a deployment secure.

This page describes the current product's actual security behavior. Capabilities that are not implemented are marked as such.

## 1. Current security boundary

The Shared Runtime applies the same security model to the C++ API, QML API, Generic Plugin, and QPA.

Default behavior:

- bind address: `0.0.0.0`, every IPv4 interface of the host, not only loopback;
- port: `5921`;
- remote input: disabled;
- constructing a C++ `HyRemote::RemoteAccess` object does not open a listener;
- QML starts only after `enabled: true` is requested;
- Generic and QPA start only when explicitly activated through their Qt startup mechanisms;
- `Insecure` is unauthenticated and unencrypted. It is for a trusted LAN only and is not Internet-safe;
- malformed or incomplete clients remain subject to bounded transport limits;
- secrets must not be written to normal diagnostics.

The default build may contain security-profile enum values without providing authenticated or encrypted transport.

Do not expose the product directly to the public Internet.

## 2. Security profiles

### Insecure

`Insecure` is the default development profile.

- no viewer authentication;
- no transport encryption;
- trusted LAN only;
- remote input remains a separate policy.

The `Insecure` listener is reachable wherever it is bound. HyRemote states that fact directly and does not create a security illusion by narrowing the bind address.

### Authenticated

`Authenticated` is a conditional capability. It is usable only when all of the following are true:

1. the build includes the transport-security capability;
2. a security descriptor path is configured;
3. the descriptor is valid for the selected profile.

When those conditions hold, the current transport uses RFB VNC authentication for viewer authentication.

The stream is still **not encrypted**.

If the build capability is absent, `start()` fails with `SecurityUnavailable` before creating a target, transport, or listener. If the descriptor is missing or invalid, startup fails instead of falling back to a weaker profile.

A default development build is therefore unauthenticated and unencrypted.

### AuthenticatedEncrypted

`AuthenticatedEncrypted` is declared in the product model but **not implemented in the current product line**.

Selecting it fails closed even when the build can provide `Authenticated` VNC authentication:

```text
AuthenticatedEncrypted
  -> SecurityUnavailable
  -> no target/transport/listener composed
  -> no fallback to Authenticated
  -> no fallback to Insecure
```

A readable certificate or private-key descriptor does not make encrypted transport available.

> **TODO:** encrypted transport, certificate and private-key policy, protocol and cipher policy, and authenticated encrypted sessions are later product work.

## 3. Viewing and control are separate

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

Generic and QPA expose the corresponding startup configuration through their plugin specifications.

Leaving remote input disabled keeps the session view-only. View-only is not authentication and is not encryption.

## 4. Listener exposure

The default listener is:

```text
0.0.0.0:5921
```

Changing the bind address changes the network trust boundary. It does not automatically enable authentication or encryption.

`Insecure` is reachable wherever it is bound. `Authenticated` allows a non-loopback listener only when the authenticated build capability and descriptor are valid.

Address-family behavior is documented in [`../known-limitations.md`](../known-limitations.md).

## 5. Connection state is not authorization

`RemoteAccessState::Running` means the Runtime or listener is active. It does not mean a viewer is authenticated or connected.

`connectedClientCount()` is operational information, not a user identity, role, or authorization result.

> **TODO:** an authenticated Session Registry, bounded admission policy, session events, and explicit session termination APIs are later product work.

## 6. Input safety

Remote input is normalized and routed only to the attached Qt application target. HyRemote does not use OS-wide virtual HID or uinput injection as the normal product path.

Supported held key and button state is cleaned up on viewer disconnect and Runtime stop, so a remote peer cannot leave the local application in a pressed state.

Unsupported key, input-method, and composition cases are documented, not guessed.

## 7. Resource boundaries

The RFB baseline uses bounded behavior for concurrent clients, protocol buffering, handshake lifetime, frame handoff, and GUI input delivery.

These limits reduce accidental or malicious resource growth. They do not make the current transport safe for direct public-Internet exposure.

## 8. Secrets and diagnostics

Passwords, tokens, private keys, and equivalent secrets must not be written to normal logs, error strings, UI diagnostics, or command examples.

Security configuration should use an external descriptor or configuration mechanism instead of placing secrets directly on a process command line.

A descriptor confirms what the operator configured. It does not prove that the selected security mechanism exists in the build.

## 9. Recommended deployment

### Local development

Use the `Insecure` `0.0.0.0:5921` default on a trusted LAN. Enable remote input only when needed.

### Trusted lab or maintenance network

Use `Authenticated` only when the package was explicitly built with the required security capability and a valid descriptor is configured. The stream remains unencrypted, so the network reachability still needs an appropriate trust boundary.

### Public Internet

**Not supported as a direct deployment profile for the current version.**

Do not expose the current HyRemote listener directly to the public Internet.

## 10. Current capability matrix

| Capability | Current state |
| --- | --- |
| Default LAN listener (`0.0.0.0:5921`) | Available |
| Remote input off by default | Available |
| Explicit unauthenticated and unencrypted state | Available |
| `Authenticated` API and configuration surface | Available |
| RFB VNC authentication | **Conditional: transport-security-enabled build plus valid descriptor** |
| Default build includes authenticated transport | **No** |
| `AuthenticatedEncrypted` | **Unavailable; fails closed** |
| Stream encryption | **Not implemented** |
| Certificate and private-key production policy | **Not implemented** |
| Authenticated session identity or registry | **Not implemented** |
| Per-session admission or termination | **Not implemented** |
| VPN, tunnel, or firewall provisioning | Outside HyRemote product scope |

The concise current statement:

> The current HyRemote release defaults to an unauthenticated, unencrypted listener on `0.0.0.0:5921` with remote input off. `Authenticated` exists only in a transport-security-enabled build with a valid security descriptor and provides VNC authentication without encryption. `AuthenticatedEncrypted` is not implemented and always fails closed. Do not expose the product directly to the public Internet.
