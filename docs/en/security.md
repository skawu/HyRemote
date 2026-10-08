# HyRemote Security

> Language / 语言: **English** | [中文](../security.md)

HyRemote is remote-access infrastructure. A working viewer connection does not by itself make a deployment secure.

This page describes the **current product security behavior**. Capabilities that are not available yet are marked as TODO.

## Current security boundary

The released product has a truthful security boundary.

The same Shared Runtime security model applies to the C++ API, QML API, Generic Plugin, and QPA frontends.

Default behavior:

- bind address: `0.0.0.0` (every IPv4 interface of the host), or one exact local IPv4, or a named interface;
- port: `5921`;
- remote input: disabled;
- constructing `HyRemote::RemoteAccess` does not open a listener;
- `Insecure` is unauthenticated and unencrypted: it is for a **trusted LAN only** and is not Internet-safe;
- requested security capabilities fail closed instead of silently downgrading;
- secrets must not be written to normal diagnostics.

Do not expose the current product directly to the public Internet.

## Security profiles

### Insecure

`Insecure` is the default security profile.

- no viewer authentication;
- no stream encryption;
- listener scope follows the configured bind address (default `0.0.0.0:5921`);
- remote input remains a separate opt-in policy.

An `Insecure` listener is unauthenticated and unencrypted: it is for a trusted LAN only and is not Internet-safe.

### Authenticated

`Authenticated` is a **conditional capability**. It is usable only when:

1. HyRemote was built with the transport-security capability; and
2. a valid security descriptor is configured.

When available, the current RFB transport uses VNC authentication. The stream is still **not encrypted**.

If the required build capability is absent, startup fails with `SecurityUnavailable` before target, transport, or listener creation. Missing/invalid descriptor configuration also fails instead of falling back to a weaker profile.

### AuthenticatedEncrypted

`AuthenticatedEncrypted` is declared in the product model but is **not implemented in the current product line**.

Selecting it always fails closed before listener creation, including in builds that can provide `Authenticated` VNC authentication:

```text
AuthenticatedEncrypted
  -> SecurityUnavailable
  -> no target/transport/listener composed
  -> no fallback to Authenticated
  -> no fallback to Insecure
```

> **Unavailable today:** encrypted transport / `AuthenticatedEncrypted`; it fails closed until a real encrypted backend is shipped. Session-registry/IAM capabilities remain outside the current product contract.

## Security capability matrix

| Capability | Current status |
| --- | --- |
| LAN-capable default (`0.0.0.0:5921`) | **Available** |
| Remote input off by default | **Available** |
| Truthful unauthenticated/unencrypted state | **Available** |
| `Authenticated` API/configuration surface | **Available** |
| RFB VNC authentication | **Conditional: transport-security-enabled build + valid descriptor** |
| Every build/package includes authenticated transport | **No** |
| `AuthenticatedEncrypted` | **Unavailable; fail-closed** |
| Stream encryption | **Unavailable in the current product** |
| Authenticated session registry/admission | **Not part of the current product contract** |

## Remote viewing versus remote control

Remote viewing and remote input are separate policies. View-only mode is not a substitute for authentication or encryption.

## Listener exposure

The default listener is:

```text
0.0.0.0:5921
```

Changing the bind address changes the trust boundary; it does not automatically enable authentication or encryption.

## Connection state is not authorization

`RemoteAccessState::Running` means the Runtime/listener is active. `connectedClientCount()` reports operational connection count. Neither value is a user identity, role, or authorization decision.

## Secrets and diagnostics

Passwords, tokens, private keys, and equivalent secrets must not be written to normal logs, error strings, UI diagnostics, or command examples.

Use an external security descriptor/configuration mechanism instead of putting secrets on the process command line.

## Recommended profiles

- **Local developer / same machine:** a listener on `127.0.0.1` (set it explicitly) stays the tightest option.
- **Trusted lab / maintenance network:** use `Authenticated` only when the package includes the required security capability and a valid descriptor is configured. The stream remains unencrypted.
- **Public Internet:** direct exposure is **not supported** in the current product line.

See [`../known-limitations.md`](../known-limitations.md) for current product limits and [`../compatibility.md`](../compatibility.md) for the compatibility matrix.
