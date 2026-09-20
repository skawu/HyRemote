# HyRemote V1 Security

Status: **V1 user-facing implemented-security boundary; release acceptance still pending**

HyRemote is remote-access infrastructure. A working viewer connection is not evidence of a secure deployment.

This page describes security behavior implemented in the current V1 candidate. [`security-model.md`](security-model.md) is the broader architecture/threat-model document and includes future transport-security requirements; do not read those future requirements as already implemented capabilities.

## Current V1 baseline

The current correctness transport is HyRemote's bounded internal RFB transport shared by Embedded C++, Declarative QML and Transparent QPA.

Implemented defaults and controls:

- constructing C++ `HyRemote::RemoteAccess` does **not** open a listener;
- Embedded C++ requires explicit `start()`;
- QML requires an explicit `enabled: true` request, applied after component completion;
- Transparent QPA starts only when the application is deliberately launched through the `hyremote` platform path;
- the default listener is loopback (`127.0.0.1`);
- remote input is disabled by default;
- remote viewing and remote input policy remain distinct;
- malformed/incomplete clients are bounded by protocol/input and handshake limits;
- concurrent clients are bounded by the transport implementation;
- passwords, certificates, TLS configuration, user accounts, or authentication identities are **not** part of the current V1 product surface.

## Critical limitation: RFB SecurityType None

The current transport negotiates **RFB SecurityType None**.

That means HyRemote provides **no transport encryption**, and provides **no viewer authentication unless an authenticated security profile is configured**. Anyone who can reach a listener that does not authenticate may be able to view the exposed application, subject to network controls outside HyRemote. If remote input is enabled, an unauthorized reachable viewer may also be able to control the target application.

Therefore:

- loopback/trusted local testing is the safe default use;
- do not expose the listener directly to the public Internet;
- do not describe the current transport as password-protected, authenticated, encrypted, TLS-secured, or Internet-safe;
- do not assume that a VNC viewer's password UI means the HyRemote server supports password authentication;
- non-loopback deployment requires an external trusted security boundary appropriate to the deployment, such as host/network access controls or a separately managed secure tunnel. Such external infrastructure is outside HyRemote's V1 feature claim.

## Listener exposure

Embedded C++ defaults to loopback:

```cpp
HyRemote::RemoteAccess remote(&window);
// listenAddress() defaults to 127.0.0.1
```

Changing the bind address is an explicit application decision:

```cpp
remote.setListenAddress(QHostAddress(QStringLiteral("192.0.2.10")));
```

Do not use a wildcard or externally reachable address merely to make viewer setup easier. Which addresses the listener
actually accepts - including that the IPv6 wildcard `::` is an IPv6-only listener on this platform rather than a dual-stack
one - is measured in [`known-limitations.md`](known-limitations.md#listener-address-family-and-reachability). First decide which network security layer is responsible for restricting access.

The same rule applies to QML `listenAddress` and Transparent QPA `hyremote-address`: changing away from loopback widens the network trust boundary; it does not add authentication.

## Remote viewing versus control

The safe default is view-only. For Embedded C++, remote control must be enabled explicitly while stopped:

```cpp
remote.setRemoteInputEnabled(true);
remote.start();
```

The Declarative QML API mirrors the same product policy. A normal compact start can remain view-only:

```qml
RemoteAccess {
    target: mainWindow
    enabled: true
}
```

Transparent QPA uses explicit startup policy rather than inventing an application control API for an otherwise unmodified program:

```text
-platform hyremote                         # view-only default
-platform hyremote:hyremote-input=true     # remote control explicitly enabled
```

In all modes, local input remains separate from the remote-input policy; final physical coexistence evidence is tracked by #109.

## Connection state is not authorization

`RemoteAccessState::Running` means the remote runtime/listener is running. It does not mean that a viewer is authenticated or even connected.

The current candidate exposes backend-neutral `connectedClientCount()` diagnostics so application UI can distinguish listening from connected state without reading RFB/socket internals. This is an operational diagnostic, **not an authentication identity or authorization result**.

Exact Windows/Linux viewer lifecycle acceptance remains pending while #74 prevents the reference jobs from receiving runners.

## Input safety boundary

Embedded C++ and Declarative QML inject normalized input only into the attached Qt application target through the Qt adapter path. Transparent QPA routes remote input through the same normalized product semantics to qualified application surfaces while native local input remains owned by the native delegate.

HyRemote does not use Linux `uinput`, a Windows virtual-HID driver, or desktop-wide OS input injection as the V1 default.

The input model deliberately separates:

- pointer motion/buttons;
- scroll;
- physical/logical key events;
- modifier state;
- committed text where the transport provides sufficient information.

Unsupported keys, composition, or IME behavior must be skipped or documented rather than guessed.

The #90 held-key/button disconnect correction is absorbed into #106: recognized remote held state is balanced when a viewer disappears abruptly. Its exact dual-OS product-fit acceptance is still pending because #74 prevents the corresponding jobs from executing.

## Resource / denial-of-service boundaries

The current RFB baseline includes bounded behavior such as:

- maximum concurrent client slots;
- bounded per-client protocol input buffering;
- bounded advertised encoding count;
- bounded cut-text payload length even though clipboard transfer is not a V1 feature;
- handshake timeout for incomplete clients;
- bounded/latest-frame-oriented Core and transport handoff;
- bounded/coalescing GUI input delivery.

These controls reduce accidental/unbounded resource growth. They do **not** turn a listener into a safe hostile-Internet service, with or without viewer authentication.

## Secrets and logging

Because the current V1 transport has no password/TLS credential surface, examples and logs must not invent credentials.

Future security mechanisms must preserve the architecture rule that passwords, tokens, private keys, and equivalent secrets are not written to normal diagnostic output. Do not place secrets in command-line examples unless a future reviewed transport explicitly requires and safely supports that mechanism.

## Recommended V1 deployment profiles

### Local developer / same machine

Use the default loopback listener. Enable remote input only when intentionally testing control.

### Controlled lab or trusted maintenance network

Prefer a specific bind address and apply network-level access restrictions. Treat the RFB payload itself as unencrypted, and as unauthenticated unless an authenticated profile is configured. Document the external protection used by the deployment; it is not a HyRemote transport-security feature.

### Public Internet

**Not a supported direct-deployment profile for the current V1 transport.**

Do not bind HyRemote's SecurityType None listener directly to a public interface and rely on obscurity, a non-default port, or viewer settings as protection.

## What is not implemented yet

Do not claim the following as current V1 capabilities unless a later accepted change updates the product baseline:

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

> HyRemote V1 authenticates the viewer when an authenticated security profile is configured (RFB VNC authentication) and does not encrypt the stream. It defaults to loopback with remote input off. Keep a listener behind an appropriate trusted access boundary; do not expose it directly to the public Internet.
