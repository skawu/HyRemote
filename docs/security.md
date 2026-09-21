# HyRemote Security

HyRemote is remote-access infrastructure. A working viewer connection does not by itself make a deployment secure.

This page describes the **current product security behavior**. Capabilities that are not available yet are marked as TODO rather than described as future acceptance work.

## V0.1 security profile

V0.1 is a Developer Preview with a loopback-first security boundary.

The Shared Runtime applies the same security behavior to the C++ API, QML API, Generic Plugin, and QPA frontends:

- default bind: `127.0.0.1`;
- default port: `5921`;
- remote input: disabled by default;
- constructing a C++ `HyRemote::RemoteAccess` object does not open a listener;
- QML starts only when `enabled: true` is requested;
- Generic/QPA start only when explicitly activated through their Qt startup mechanisms;
- unauthenticated non-loopback exposure is rejected;
- malformed or incomplete protocol clients are bounded by transport limits;
- sensitive credential material must not be written to normal diagnostics.

V0.1 must not be exposed directly to the public Internet.

## Security profiles

### Insecure

`Insecure` is intended for local/trusted development use.

- loopback is the normal/default deployment;
- no viewer authentication is provided;
- the stream is not encrypted;
- remote input remains a separate opt-in policy.

An unauthenticated configuration is not accepted for non-loopback exposure.

### Authenticated

`Authenticated` adds RFB VNC authentication when a valid credential configuration is provided.

This authenticates the viewer at the RFB layer, but **does not encrypt the stream**. Authentication and encryption are separate product capabilities.

Use this profile only inside an appropriate trusted network boundary until encrypted transport is available.

### AuthenticatedEncrypted

The public product model reserves `AuthenticatedEncrypted` for authenticated + encrypted transport.

The TLS/VeNCrypt backend is not implemented in V0.1. Selecting this profile therefore:

```text
AuthenticatedEncrypted
  -> SecurityUnavailable
  -> no listener opened
  -> no fallback to Authenticated
  -> no fallback to Insecure
```

This fail-closed behavior is intentional. Certificate/private-key fields being syntactically valid must not make the product pretend that encrypted transport exists.

> **TODO V0.2:** implement VeNCrypt/TLS, certificate/private-key policy, cipher/protocol policy, and encrypted authenticated sessions.

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

Generic and QPA expose the corresponding startup configuration through their plugin specifications.

Leaving remote input disabled keeps the session view-only. View-only is **not** a substitute for authentication or encryption.

## Listener exposure

The safest V0.1 use is the default loopback listener:

```text
127.0.0.1:5921
```

Changing the bind address changes the network trust boundary. It does not automatically add authentication or encryption.

Use a specific numeric address rather than widening exposure casually. Exact address-family behavior is documented in [`known-limitations.md`](known-limitations.md).

## Connection state is not authorization

`RemoteAccessState::Running` means the Runtime/listener is active. It does not mean a viewer is authenticated or even connected.

`connectedClientCount()` is operational information. It is not a user identity, role, or authorization result.

> **TODO V0.2:** authenticated Session Registry, bounded admission policy, session events, and explicit session termination APIs.

## Input safety

Remote input is normalized and routed only to the attached Qt application target. HyRemote does not use desktop-wide virtual HID / `uinput` injection as the normal product path.

The Runtime tracks supported held key/button state so disconnecting a remote peer or stopping the Runtime does not intentionally leave the local application in a stuck input state.

Unsupported key/composition/IME cases are documented rather than guessed.

## Resource boundaries

The RFB baseline uses bounded behavior for items such as:

- concurrent clients;
- protocol input buffering;
- handshake lifetime;
- advertised encoding lists;
- clipboard/cut-text payload limits where applicable;
- Core frame handoff;
- GUI input delivery.

These limits reduce accidental or malicious resource growth. They do not make the current transport suitable for direct hostile-Internet exposure.

## Secrets and diagnostics

Passwords, tokens, private keys, and equivalent secrets must not be written to normal logs, error strings, UI diagnostics, or command examples.

Where a frontend needs credential configuration, prefer an external configuration/descriptor mechanism rather than placing secrets directly in a process command line.

## Recommended deployment profiles

### Local developer / same machine

Use the default loopback listener. Enable remote input only when needed.

### Trusted lab or maintenance network

Use an authenticated profile where appropriate and restrict network reachability. Remember that V0.1 authentication does not encrypt the RFB stream.

### Public Internet

**Not supported as a direct V0.1 deployment profile.**

Do not expose a HyRemote V0.1 listener directly to the public Internet.

## Current security capability matrix

| Capability | V0.1 status |
| --- | --- |
| Loopback-first default | Available |
| Remote input off by default | Available |
| Reject unauthenticated non-loopback exposure | Available |
| RFB VNC authentication | Available with `Authenticated` profile |
| Encrypted transport | **TODO V0.2** |
| Certificate/private-key production policy | **TODO V0.2** |
| Authenticated session identity/registry | **TODO V0.2** |
| Per-session admission/termination | **TODO V0.2** |
| Central account/role management | Not a V0.1 capability |
| VPN/tunnel/firewall provisioning | Outside HyRemote product scope |

The concise V0.1 statement is:

> HyRemote V0.1 defaults to loopback with remote input off. An authenticated profile can authenticate a viewer with RFB VNC authentication, but the stream is not encrypted. `AuthenticatedEncrypted` fails closed until the encrypted backend is implemented. Do not expose V0.1 directly to the public Internet.