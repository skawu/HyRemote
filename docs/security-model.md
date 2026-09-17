# HyRemote Security Model

Status: **Architecture baseline**

HyRemote is remote-access infrastructure. A working remote viewer is not, by itself, a safe deployment architecture.

This document defines security requirements for the framework and its adapters. Transport-specific implementation details may vary, but they must preserve these controls.

## 1. Security goals

HyRemote should make it possible for an embedding application to control:

- whether a remote service is running;
- which interface/address it listens on;
- whether remote viewing is allowed;
- whether remote input is allowed;
- how the selected transport authenticates/encrypts clients;
- when clients connect/disconnect;
- how sessions are terminated;
- how security-relevant events are observed by the host application.

HyRemote should **not** become an identity/account platform. Product-specific users, roles, cloud identity, centralized audit storage, and fleet authorization belong above the generic framework.

## 2. Trust boundaries

```text
+---------------- Qt Application ----------------+
|                                                |
| Business/UI code                               |
|      |                                         |
| HyRemote Core                                  |
|      |                                         |
| Target/Capture/Input adapters                  |
|      |                                         |
| Transport adapter (e.g. NeatVNC)               |
+------+-----------------------------------------+
       |
       | network boundary
       v
Remote client / viewer
       |
       +-- potentially untrusted network
```

Additional boundaries may exist around:

- TLS/VPN tunnels;
- centralized device-management systems;
- Linux permissions for system-level input backends;
- platform-specific hardware/buffer APIs.

## 3. Threats considered

At minimum:

- unauthorized viewing of application content;
- unauthorized remote control/input;
- credential interception;
- connection from an unintended network interface;
- brute-force or repeated connection attempts;
- malformed protocol input from a remote client;
- denial of service through slow clients or excessive connections;
- sensitive credential/log leakage;
- stale sessions surviving application security-state changes;
- privilege escalation through system-level input injection;
- accidental Internet exposure of a maintenance interface.

## 4. Secure-default requirements

### 4.1 No implicit listener

Constructing a HyRemote object must not open a network port.

The host application must explicitly enable/start remote access.

The QML API should default `enabled` to `false`.

The zero-code/QPA mode must also require explicit opt-in through configuration/environment rather than silently exposing a service merely because the adapter library is installed.

### 4.2 Listener scope is explicit

The transport API must make bind address/interface configuration explicit.

When no bind address is provided, the safest portable default is loopback-only unless a transport/backend documents a stronger platform-specific mechanism.

Binding to wildcard/non-loopback addresses should be visible in logs/diagnostics and documentation.

### 4.3 Viewing and control are separate capabilities

Remote viewing and remote input must be independently controllable.

Conceptually:

```text
serverEnabled
viewEnabled
remoteInputEnabled
```

Disabling remote input must prevent keyboard/pointer/touch events from reaching the Qt application without requiring the connection to be closed.

Changing authorization state should be able to terminate or downgrade active sessions.

### 4.4 Security mode is transport-specific but explicit

HyRemote core should not invent a universal password API that hides meaningful differences between transports.

Instead:

- core exposes high-level security/session controls;
- each transport exposes typed configuration for its supported authentication/encryption mechanisms;
- documentation states which modes are safe only on trusted networks.

### 4.5 Credentials are never diagnostics

Passwords, private keys, raw authentication credentials, tokens, or equivalent secrets must not be written to normal logs, exceptions, tracing output, crash context, or example configuration committed to the repository.

## 5. NeatVNC transport baseline

> **Current implementation state (2026-09-17).** The transport shipped on the x86 product path is the
> bounded custom RFB 3.8 baseline (issue #53), **not** NeatVNC, and it implements **SecurityType `None`
> only, over plaintext**: neither `HyRemote::RemoteAccess` nor `Transport` can express authentication or
> encryption settings yet. The protections that do exist are structural - construction opens no listener,
> the listen address defaults to `QHostAddress::LocalHost`, the client count is capped, the handshake
> expires, and remote input is opt-in and independently controlled. Everything below in this section is
> the **requirement for the transport**, not a description of what exists today: deployments outside
> Profile A (local/loopback) or a trusted tunnel are not supported by the current transport.

The initial NeatVNC candidate supports policy flags including:

- authentication required;
- encryption required;
- username required;
- explicitly allowing legacy/broken cryptography for trusted private networks.

It supports TLS certificate/private-key configuration and asynchronous authentication callbacks when built with the relevant crypto/TLS dependencies.

HyRemote's VNC adapter should therefore expose transport configuration that can express these capabilities without reducing them to an 8-character legacy VNC password abstraction.

### Legacy VNC authentication

Legacy DES/VNC authentication must be documented as a compatibility mechanism, not as sufficient protection for direct untrusted-network/Internet exposure.

## 6. Deployment profiles

### Profile A — local/loopback development

- listener: loopback;
- encryption: optional for local-only development;
- remote input: explicit opt-in;
- no claim of production deployment security.

### Profile B — trusted industrial LAN

- listener: specific interface/address preferred over wildcard;
- authentication required;
- encryption preferred/required according to deployment policy;
- remote input independently controlled;
- host application should expose session events for local audit/operations.

### Profile C — VPN / secure maintenance tunnel

- HyRemote listener reachable only through trusted VPN/tunnel or equivalent access layer;
- transport authentication should still be enabled where practical;
- centralized product authorization may decide when HyRemote starts/stops;
- host application may log session lifecycle and authorization decisions.

### Profile D — direct public Internet

Not a recommended default deployment profile.

HyRemote documentation must not suggest that simply setting a VNC password makes direct Internet exposure safe.

## 7. Public API requirements

The core/API design in ARCH-01 should provide transport-neutral equivalents of:

```text
start / stop
isRunning
setViewEnabled
setRemoteInputEnabled
clientConnected
clientDisconnected
sessionError
terminateClient / terminateAllClients
```

The exact names are not frozen here.

The transport adapter should additionally support:

- bind address/port;
- maximum clients where supported;
- authentication configuration;
- TLS/credential paths or callback providers;
- transport-specific policy flags.

## 8. Input safety

### Qt event backend

The default application-embedded input path should inject only into the intended Qt application/target semantics, not into the Linux input subsystem.

### `uinput` backend

If a system-level `uinput` backend is added later:

- it is optional;
- permissions/privileges are documented;
- it is treated as a separate trust boundary;
- it is never silently selected when direct Qt event delivery is available.

## 9. Resource / denial-of-service controls

The architecture should permit limits on:

- concurrent clients;
- pending/in-flight frames;
- frame rate;
- encoder/network queue growth;
- authentication/handshake lifetime;
- connection retry/backoff at higher layers.

A slow client must not be allowed to block the Qt GUI/render thread indefinitely.

Frame replacement/drop under backpressure is acceptable for remote display and preferable to unbounded memory growth.

## 10. Session events and audit hooks

HyRemote should emit enough information for an embedding product to implement audit without embedding a product-specific audit database.

Potential events:

- server started/stopped;
- client connected/disconnected;
- remote address;
- authenticated username when supplied by transport;
- authentication failure summary without credentials;
- remote input enabled/disabled;
- session termination reason.

## 11. Zero-code/QPA mode

Zero-code integration must not weaken defaults.

Configuration must provide explicit controls for:

- service enablement;
- bind address/port;
- remote input;
- transport security settings.

If secure configuration cannot be expressed safely through environment variables alone, a configuration file/API boundary should be provided rather than encouraging secrets in command lines/process listings.

## 12. Public-release security gate

Before HyRemote is advertised as production-ready, or the repository is made public with deployment
guidance. **Status as of 2026-09-17** - the repository is public, so this gate is live. The third column
is recorded evidence, not intent:

| Requirement | State | Evidence / remaining gap |
|---|---|---|
| project license frozen | met | Apache-2.0 since 2026-09-15 (see [`LICENSE`](../LICENSE)) |
| threat model reviewed against implemented API | **not met** | this document predates the RFB 3.8 baseline; no transport-level threat-model pass has been run against the shipped implementation |
| no listener starts implicitly | met | construction opens nothing; the transport requires an explicit address and port |
| view and input permissions are independent | met | remote input is opt-in (`setRemoteInputEnabled`) and independent of viewing |
| sensitive values are excluded from logs | met (vacuously) | the current transport holds no credentials; the rule becomes binding once authentication exists |
| supported authentication/encryption modes documented | **not met** | SecurityType `None` over plaintext is all the transport implements (see section 5) |
| legacy/insecure modes clearly labeled | met | `None` is the only mode and is labeled as such here, in `SECURITY.md` and in the README |
| malformed-client tests exist for the transport boundary where practical | **partial** | bounds exist in code (256 KB client-input cap, encoding-count limit, CutText limit, handshake expiry, pixel-byte/stride validation) but there is no dedicated malformed-client test suite |
| connection/backpressure limits tested | met | capped client count, bounded input mailbox with coalescing, bounded frame queue with `DropOldest`/`ProducerThrottle`, deterministic lifecycle tests |
| dependency versions and update policy documented | met | [`dependency-policy.md`](dependency-policy.md), including the test/CI tooling |
| security reporting instructions are public and usable | **not met** | [`SECURITY.md`](../SECURITY.md) is published, but GitHub private vulnerability reporting is not enabled for this repository yet |

Re-run this gate after any change to the transport security surface.

## 13. Non-goals

HyRemote does not itself provide:

- user/account management;
- fleet identity;
- cloud authorization;
- centralized audit storage;
- VPN infrastructure;
- firewall management.

It provides the controls and hooks required for those systems to safely govern remote access.
