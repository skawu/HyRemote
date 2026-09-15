# HyRemote V1 Security

Status: **V1 user-facing implemented-security boundary**

HyRemote is remote-access infrastructure. A working viewer connection is not evidence of a secure deployment.

This page describes the security behavior that is **actually implemented in the current V1 reference line**. [`security-model.md`](security-model.md) is the broader architecture/threat-model document and includes future transport-security requirements; do not read those future requirements as already implemented capabilities.

## Current V1 baseline

The current V1 correctness transport is HyRemote's bounded internal RFB transport.

Implemented defaults and controls:

- constructing `HyRemote::RemoteAccess` does **not** open a listener;
- the application must call `start()` explicitly;
- the default listener is loopback (`127.0.0.1`);
- remote input is disabled by default;
- remote viewing and remote input are independent policies;
- malformed/incomplete clients are bounded by protocol/input and handshake limits;
- concurrent clients are bounded by the transport implementation;
- passwords, certificates, TLS configuration, user accounts, or authentication identities are **not** currently part of the V1 public product surface.

## Critical limitation: RFB SecurityType None

The current transport negotiates **RFB SecurityType None**.

That means HyRemote V1 currently provides **no transport authentication and no transport encryption**. Anyone who can reach a non-loopback listener may be able to view the exposed application, subject to network controls outside HyRemote. If remote input is enabled, an unauthorized reachable viewer may also be able to control the target application.

Therefore:

- loopback/trusted local testing is the safe default use;
- do not expose the listener directly to the public Internet;
- do not describe the current transport as password-protected, authenticated, encrypted, TLS-secured, or Internet-safe;
- do not assume that a VNC viewer's password UI means the HyRemote server supports password authentication;
- non-loopback deployment requires an external trusted security boundary appropriate to the deployment, such as host/network access controls or a separately managed secure tunnel. Such external infrastructure is outside HyRemote's V1 feature claim.

## Listener exposure

Default:

```cpp
HyRemote::RemoteAccess remote(&window);
// listenAddress() defaults to 127.0.0.1
```

Changing the bind address is an explicit application decision:

```cpp
remote.setListenAddress(QHostAddress(QStringLiteral("192.0.2.10")));
```

Do not use a wildcard or externally reachable address merely to make viewer setup easier. First decide which network security layer is responsible for restricting access.

The same rule applies to Transparent QPA Proxy configuration: changing `hyremote-address` from loopback widens the network trust boundary; it does not add authentication.

## Remote viewing versus control

The default Embedded C++ policy is view-only:

```cpp
remote.setRemoteInputEnabled(false);
```

Remote control must be explicitly enabled while the facade is stopped:

```cpp
remote.setRemoteInputEnabled(true);
remote.start();
```

For the current public facade, configuration mutation is accepted only while Stopped. Examples that need to change control policy therefore stop, update the policy, and start the **same** `RemoteAccess` instance rather than creating a second runtime.

The declarative QML API mirrors the same semantics. Transparent QPA currently receives its zero-code input policy through startup configuration and must not pretend to provide an application-owned runtime toggle that does not exist.

## Connection state is not authorization

`RemoteAccessState::Running` means the remote runtime/listener is running. It does not mean that a viewer is authenticated or even connected.

#91 / PR #92 introduces a backend-neutral connected-client count so applications can distinguish listening from connected state without reading RFB/socket internals. This is an operational diagnostic, **not an authentication identity or authorization result**.

## Input safety boundary

The Embedded C++ and Declarative QML modes inject normalized input only into the attached Qt application target through the Qt adapter path. They do not use Linux `uinput`, a Windows virtual-HID driver, or desktop-wide OS input injection as the V1 default.

The input model deliberately separates:

- pointer motion/buttons;
- scroll;
- physical/logical key events;
- modifier state;
- committed text where the transport provides sufficient information.

Unsupported keys, composition, or IME behavior must be skipped or documented rather than guessed.

#90 tracks disconnect-time balancing releases so an abruptly disconnected viewer cannot leave a held key/button state behind in the Qt target.

## Resource / denial-of-service boundaries

The current RFB baseline includes bounded behavior such as:

- maximum concurrent client slots;
- bounded per-client protocol input buffering;
- bounded advertised encoding count;
- bounded cut-text payload length even though clipboard transfer is not a V1 feature;
- handshake timeout for incomplete clients;
- bounded/latest-frame-oriented Core and transport handoff.

These controls reduce accidental/unbounded resource growth. They do **not** turn an unauthenticated listener into a safe hostile-Internet service.

## Secrets and logging

Because the current V1 transport has no password/TLS credential surface, examples and logs must not invent credentials.

Future security mechanisms must preserve the architecture rule that passwords, tokens, private keys, and equivalent secrets are not written to normal diagnostic output. Do not place secrets in command-line examples unless a future reviewed transport explicitly requires and safely supports that mechanism.

## Recommended V1 deployment profiles

### Local developer / same machine

Use the default loopback listener. Enable remote input only when intentionally testing control.

### Controlled lab or trusted maintenance network

Prefer a specific bind address and apply network-level access restrictions. Treat the RFB payload itself as unauthenticated and unencrypted. Document the external protection used by the deployment; it is not a HyRemote transport-security feature.

### Public Internet

**Not a supported direct-deployment profile for the current V1 transport.**

Do not bind HyRemote's SecurityType None listener directly to a public interface and rely on obscurity, a non-default port, or viewer settings as protection.

## What is not implemented yet

Do not claim the following as current V1 capabilities unless a later accepted issue/PR changes the product baseline:

- RFB/VNC password authentication;
- TLS encryption;
- certificate/private-key configuration;
- authenticated username/identity;
- per-client authorization or roles;
- per-client terminate/ban controls;
- centralized audit/account management;
- VPN/tunnel provisioning;
- firewall management.

The architecture threat model discusses several of these as future requirements/candidates. That is roadmap/design context, not an implementation claim.

## Release acceptance rule

A V1 guide, example, release note, compatibility row, or UI must never imply stronger security than this page.

Until authenticated/encrypted transport support is implemented and separately accepted, the concise user-facing statement is:

> HyRemote V1 uses an unauthenticated, unencrypted RFB correctness transport. It defaults to loopback and remote input off. Keep it behind an appropriate trusted access boundary; do not expose it directly to the public Internet.
