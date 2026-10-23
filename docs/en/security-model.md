# HyRemote Security Model

> Language / 语言: **English** | [中文](../security-model.md)

Status: security model for the current V0.3.0.0 product. This document describes implemented behavior and the security boundary later product work must preserve.

HyRemote is remote-access infrastructure. A working viewer connection does not make a deployment secure. See [`security.md`](security.md) for the concise user-facing deployment rules.

Capabilities marked as later work are not current product claims.

## 1. Current security baseline

The current product uses a bounded RFB 3.8 transport across the C++, QML, Generic, and QPA frontends.

Implemented security boundary:

- constructing `HyRemote::RemoteAccess` does not open a listener;
- C++ starts remote access only through explicit `start()`;
- QML starts only after an explicit `enabled: true`;
- QPA starts only when the process is launched with `-platform hyremote`;
- the default listener is `0.0.0.0:5921`, reachable on the host's IPv4 interfaces. It is for a trusted LAN only and is not Internet-safe;
- remote input is disabled by default;
- the current RFB transport can negotiate `SecurityType None`;
- RFB VNC authentication is available when an authenticated profile is configured;
- no configuration currently provides transport encryption;
- passwords, certificates, private keys, authenticated identities, roles, and per-client authorization are outside the current public product surface.

Documentation, examples, and compatibility claims must not imply stronger security than these facts.

## 2. Trust boundary

```text
+-------------------- Qt application/process --------------------+
|                                                                |
| application UI and business code                               |
|          |                                                     |
| HyRemote public entry / QML / QPA control                      |
|          |                                                     |
| Core Session + target/capture/input adapters                   |
|          |                                                     |
| bounded internal RFB transport                                 |
+----------+-----------------------------------------------------+
           |
           | unencrypted; unauthenticated unless a profile is set
           v
       remote viewer
```

QPA also delegates local display and input to the qualified native Qt platform plugin (`qwindows` or `qxcb`). HyRemote remains additive to local operation.

Deployments may add host or network ACLs, a VPN, or a separately managed secure tunnel. Those are deployment infrastructure, not HyRemote transport-security features.

## 3. Threats considered

- unauthorized viewing when a listener becomes reachable;
- unauthorized control when remote input is enabled;
- accidental public-Internet exposure of a listener that is reachable by default;
- malformed or deliberately incomplete RFB clients;
- denial of service through slow clients, excessive connections, or unbounded protocol state;
- stale input state after an abrupt viewer disconnect;
- sensitive information leaking through diagnostics;
- remote input escaping the intended Qt application or surface boundary;
- QPA disrupting local display or input;
- later security configuration being mistaken for implemented capability.

## 4. Secure defaults

### 4.1 No implicit listener

Installing or constructing HyRemote must not create a remotely reachable service.

- C++ construction is inert.
- QML defaults `enabled` to `false`.
- QPA requires explicit selection of the `hyremote` platform path.

### 4.2 Reachability stated directly

The default listener is `0.0.0.0:5921`, reachable on the host's IPv4 interfaces. Binding one local IPv4 address or one named interface narrows reachability. It does not add authentication or encryption.

Examples must not widen the default. The shipped security state tells the user how far to trust that reachability.

### 4.3 View and control are separate policies

Remote input is disabled by default and must be explicitly enabled.

Current application contract:

- C++ and QML remote-input configuration is mutable only while the Runtime is stopped; changing a running service uses `stop -> configure -> start`;
- QPA uses the startup policy `hyremote-input=true`; returning to view-only requires a relaunch without that option;
- the current version does not invent a hidden runtime authorization channel to bypass that lifecycle;
- local native input remains independent from remote-input policy.

A later additive API may support live downgrade or per-client control. That is not a current acceptance requirement and must not be inferred from this document.

### 4.4 Connection state is not authorization

`Running` means the Remote Runtime or listener is active. `connectedClientCount()` is a connection diagnostic. Neither is an authentication or authorization result.

Under `SecurityType None`, a reachable viewer is not authenticated by HyRemote. A configured authenticated profile authenticates it with RFB VNC authentication, but the stream remains unencrypted.

## 5. Input safety

The normal input path delivers normalized input only to the intended Qt application target and supported surface semantics.

The current version does not use Linux `uinput`, Windows virtual-HID injection, or desktop-wide OS input injection as its default mechanism.

The input boundary distinguishes pointer motion and buttons, wheel, key and modifier state, and committed text when the protocol provides enough information. Unsupported key, input-method, and composition behavior is skipped or documented, not guessed.

When a viewer disconnects abruptly, recognized held remote key and button state must be balanced so the local Qt application does not keep a pressed state.

## 6. Resource and denial-of-service boundaries

The current transport remains bounded whether or not it authenticates the viewer.

Implemented or required bounded behavior:

- concurrent client slots;
- per-client protocol input buffering;
- advertised encoding count;
- cut-text payload length, even though clipboard transfer is not a current feature;
- incomplete-handshake lifetime;
- Core frame mailbox and backpressure;
- transport frame handoff;
- GUI input delivery and coalescing;
- callback lifetime and quiescent stop.

A slow or malformed client must not create unbounded memory growth or indefinitely block the Qt GUI or render path.

These controls reduce failure and denial-of-service exposure. They do not make `SecurityType None` safe for a hostile public network, and viewer authentication alone does not either.

## 7. Diagnostics and sensitive data

The current version has no password or TLS credential surface, so examples and diagnostics must not invent credentials or imply authenticated identities.

Later security mechanisms must preserve these rules:

- passwords, tokens, private keys, and equivalent secrets are never written to normal logs, errors, or traces;
- an authenticated identity, if introduced later, must be distinguished from a connection count or socket address;
- security-relevant failures should remain observable without disclosing secrets.

## 8. Deployment profiles

### Local development

Use the `0.0.0.0:5921` default on a trusted LAN. Enable remote input only for testing or when remote control is required.

### Controlled lab or industrial maintenance network

Use a specific bind address and an external trusted access boundary appropriate to the deployment. Treat the HyRemote RFB payload as unencrypted, and as unauthenticated unless an authenticated profile is configured.

### VPN or separately secured maintenance tunnel

A separately managed secure tunnel may constrain who can reach the HyRemote listener. The tunnel is outside the current HyRemote version and must not be described as built-in authentication or transport encryption.

### Direct public Internet

**Not a supported deployment profile for the current version.**

Do not expose a `SecurityType None` listener directly to the public Internet, and do not treat a non-default port or viewer password UI as protection.

## 9. QPA security boundary

Zero-code or minimal-source-change integration must not weaken defaults.

QPA launch configuration supports explicit process-start policy for:

- listener address;
- port;
- remote input enabled or disabled.

QPA does not add a hidden credential system or dynamic policy service. Local display and input remain delegated to the qualified native platform implementation. Physical local-display and local-input coexistence with remote access is tracked by #109 and #32.

## 10. Later transport-security requirements

Until that backend exists, the two profiles are not interchangeable:

- `Authenticated` is RFB VNC authentication over an unencrypted stream;
- `AuthenticatedEncrypted` fails closed in the current product line. It returns `SecurityUnavailable` before creating a listener, composes no target, transport, or socket, and never falls back to `Authenticated` or `SecurityType None`;
- a syntactically valid certificate or private-key descriptor does not make encrypted transport available. Descriptor validity and backend capability are separate facts.

Authenticated encrypted transport is a later V1.0.0.0 product requirement, not current capability. The work must preserve backend-neutral public boundaries and must not force backend-specific types or toolchains on normal application consumers.

Historical NeatVNC or rustvncserver investigation is research evidence only, not the selected transport or a security promise.

## 11. Non-goals

The current version does not provide:

- user or account management;
- fleet identity;
- cloud authorization;
- centralized audit storage;
- VPN or tunnel provisioning;
- firewall management;
- public-Internet-safe transport security;
- per-client identity or role management.

Products may place those systems around HyRemote. They remain separate trust boundaries.

## 12. Release security gate

Before `v1.0.0.0` is authorized, evidence must confirm at least:

- [x] Apache-2.0 project licensing is frozen;
- [x] construction and installation do not implicitly open a listener;
- [x] the default bind is `0.0.0.0:5921` and is described directly;
- [x] remote input is disabled by default and explicitly controlled;
- [x] the current `SecurityType None` limitation is explicit in user docs and release notes;
- [x] bounded protocol, frame, input, and handshake behavior is represented in implementation and tests;
- [x] current docs do not claim password, TLS, or authenticated-identity support;
- [x] Windows x86_64 reference acceptance has executed and passed;
- [x] Linux x86_64 reference acceptance has executed and passed;
- [ ] physical native local-display/local-input and remote coexistence evidence required by #109 and #32 passes.

A GitHub Actions job that never receives a runner is neither security evidence nor a code failure. #74 must not be bypassed by relabeling no-runner results.

The concise current statement:

> HyRemote uses RFB VNC authentication when an authenticated security descriptor is configured and does not encrypt the stream. It listens on `0.0.0.0:5921` by default with remote input off. Keep the listener behind an appropriate trusted access boundary and do not expose it directly to the public Internet.
