# HyRemote Security Model

Status: **V1 architecture/security model; release acceptance still pending**

HyRemote is remote-access infrastructure. A working viewer connection is not, by itself, a secure deployment architecture.

This document defines the security architecture and threat boundaries that the V1 product must preserve. For the concise user-facing deployment rules and exact implemented behavior, see [`security.md`](security.md).

A requirement described as **future** here is not a V1 product claim.

## 1. V1 security baseline

The current V1 candidate uses HyRemote's bounded internal RFB 3.8 correctness transport across Embedded C++, Declarative QML and Transparent QPA.

The implemented V1 security boundary is deliberately narrow and explicit:

- constructing `HyRemote::RemoteAccess` does **not** open a listener;
- Embedded C++ starts remote access only through explicit `start()`;
- QML starts only after an explicit `enabled: true` request is applied after component completion;
- Transparent QPA starts only when the process is deliberately launched through `-platform hyremote`;
- the default listener address is loopback (`127.0.0.1`);
- remote input is disabled by default;
- the current RFB transport negotiates **SecurityType None**;
- V1 authenticates the viewer when an authenticated security profile is configured, and provides **no transport encryption** in any configuration;
- passwords, TLS certificates/private keys, authenticated identities, roles and per-client authorization are not part of the V1 public product surface.

These facts are release constraints. Documentation, examples and compatibility claims must not imply stronger security.

## 2. Trust boundaries

```text
+-------------------- Qt application/process --------------------+
|                                                               |
| Application UI/business code                                  |
|          |                                                    |
| HyRemote public facade / QML facade / QPA controller          |
|          |                                                    |
| Core Session + target/capture/input adapters                  |
|          |                                                    |
| bounded internal RFB transport                                |
+----------+----------------------------------------------------+
           |
           | unencrypted; unauthenticated unless a profile is set
           v
       remote viewer
```

Transparent QPA additionally delegates native display/input behavior to the qualified native Qt platform plugin (`qwindows` or `qxcb` on the V1 reference line). HyRemote must remain additive to that native path rather than replacing local operation.

External boundaries may be placed around HyRemote, for example host/network ACLs, a VPN or another separately managed secure tunnel. Those controls are deployment infrastructure, not HyRemote V1 transport-security features.

## 3. Threats considered

At minimum, V1 design and documentation must account for:

- unauthorized viewing when a listener becomes reachable;
- unauthorized remote control when remote input is enabled;
- accidental non-loopback or public-Internet exposure;
- malformed or deliberately incomplete RFB clients;
- denial of service through slow clients, excessive connection attempts or unbounded protocol state;
- stale input state after abrupt viewer disconnect;
- sensitive information leaking through diagnostics;
- remote input escaping the intended Qt application/surface boundary;
- local display/input being disrupted by Transparent QPA operation;
- future transport-security configuration being mistaken for already implemented V1 capability.

## 4. Secure-default requirements

### 4.1 No implicit listener

Installing or constructing HyRemote must not create a remotely reachable service.

- C++ construction is inert.
- QML defaults `enabled` to `false`.
- QPA requires explicit selection of the `hyremote` platform path.

### 4.2 Loopback by default

The default listener is loopback-only. A non-loopback bind is an explicit widening of the network trust boundary; it does not add authentication or encryption.

Examples must not use wildcard/public binds merely for convenience.

### 4.3 View and control are separate policies

Remote input is disabled by default and must be explicitly enabled.

For the frozen V1 application contract:

- Embedded C++ and QML remote-input configuration is mutable while the runtime is stopped; changing it for an active service uses the explicit `stop -> configure -> start` lifecycle;
- Transparent QPA uses an explicit startup policy (`hyremote-input=true`); returning to view-only requires relaunch without that option;
- V1 does **not** invent a hidden runtime authorization/control channel solely to avoid this lifecycle;
- local native input remains independent from remote-input policy.

A future additive API may support live authorization downgrade or per-client control, but that is not a V1 acceptance requirement and must not be retroactively inferred from this threat model.

### 4.4 Connection state is not authorization

`Running` means the remote runtime/listener is active. `connectedClientCount()` is an operational connection diagnostic. Neither is an authentication or authorization result.

With SecurityType None - the mode in use when no authenticated profile is configured - a reachable viewer is not authenticated by HyRemote. A configured authenticated profile does authenticate it (RFB VNC authentication), which is still not encryption.

## 5. Input safety

The normal V1 input path delivers normalized input only to the intended Qt application target/supported surface semantics.

HyRemote V1 does not use Linux `uinput`, Windows virtual-HID injection, or desktop-wide OS input injection as its default mechanism.

The input boundary distinguishes pointer motion/buttons, wheel, key/modifier state and committed text where the protocol provides sufficient information. Unsupported key/IME/composition behavior must be skipped or documented rather than guessed.

Recognized held remote key/button state must be balanced when a viewer disconnects abruptly so stale remote state is not left inside the Qt application.

## 6. Resource and denial-of-service boundaries

V1 must remain bounded whether or not the transport authenticates the viewer.

The current architecture includes or requires bounded behavior for:

- concurrent client slots;
- per-client protocol input buffering;
- advertised encoding count;
- cut-text payload length even though clipboard transfer is not a V1 feature;
- incomplete-handshake lifetime;
- Core frame mailbox/backpressure;
- transport frame handoff;
- GUI input delivery/coalescing;
- callback lifetime and quiescent stop.

A slow or malformed client must not create unbounded memory growth or indefinitely block the Qt GUI/render path.

These resource controls reduce failure/DoS exposure. They do **not** make SecurityType None safe for a hostile public network, and viewer authentication alone does not either.

## 7. Diagnostics and sensitive data

V1 has no password/TLS credential surface, so examples and diagnostics must not invent credentials or imply authentication identities.

Future security mechanisms must preserve these rules:

- passwords, tokens, private keys and equivalent secrets are never written to normal logs/errors/traces;
- an authenticated identity, if introduced later, must be distinguished from a connection count or socket address;
- security-relevant failures should remain observable without disclosing secrets.

## 8. V1 deployment profiles

### Local developer / same machine

Use the default loopback listener. Enable remote input only when intentionally testing or using remote control.

### Controlled lab / industrial maintenance network

Use a specific bind address and an external trusted access boundary appropriate to the deployment. Treat the HyRemote RFB payload itself as unencrypted, and as unauthenticated unless an authenticated profile is configured.

### VPN / separately secured maintenance tunnel

A separately managed secure tunnel may constrain who can reach the HyRemote listener. The tunnel is outside HyRemote V1 and must not be described as built-in HyRemote authentication/TLS.

### Direct public Internet

**Not a supported V1 deployment profile.**

Do not expose a SecurityType None listener directly to the public Internet or rely on a non-default port/viewer password UI as protection.

## 9. Transparent QPA security boundary

Zero/minimal-source-change integration must not weaken defaults.

V1 QPA launch configuration supports explicit process-start policy for:

- listener address;
- port;
- remote input enabled/disabled.

The QPA path does not add a hidden credential system or dynamic policy service. Native local display/input remains delegated to the qualified native platform implementation; physical local-visible/local-input plus remote coexistence is a separate acceptance gate tracked by #109/#32.

## 10. Future transport-security requirements

Authenticated and encrypted transport is a **V1.0.0.0 requirement, not a `V1.x` track**: `[SEC-01]` #143 is x86 work, sequenced design -> authentication -> encryption -> per-client authorization/audit. This supersedes the earlier statement that it was a post-baseline capability accepted into some later release, which contradicted the release roadmap.

It is added behind the stable application model:

- password or stronger authentication;
- TLS or equivalent encryption;
- certificate/private-key provisioning;
- authenticated identities;
- per-client authorization/termination;
- richer audit events.

Such work must preserve backend-neutral public boundaries and must not force backend-specific types/toolchains on normal application consumers. A historical NeatVNC/rustvncserver investigation is research evidence only; it is not the selected V1 transport or a current security promise.

### 10.1 Design frozen before implementation (2026-09-19)

**Status: the authentication step has landed; the encryption step has not.** The product statement is therefore
"authenticated when an authenticated profile and a credential are configured, and not encrypted in any
configuration". No user-facing document may claim transport encryption, and none may claim authentication for a
build or profile that does not actually provide it - a build without the capability, or without a usable credential,
refuses the authenticated profiles rather than serving them unauthenticated.

**Authentication step.** RFB VNC authentication (security type 2) with the RFB 3.8 challenge/response: the server
sends a random 16-byte challenge and the client returns the DES-encrypted response, because that is what the
mainstream viewers already implement. An earlier revision of this section called this "the blinding variant"; the RFB
specification defines no such variant - security type 2 is simply VNC Authentication - so that unsupported wording is
removed rather than implemented. What the standard exchange does require, and what this implementation relies on, is
that each connection gets a freshly random challenge. `SecurityType None` stays available but only as an
**explicitly selected** mode: a client that does not select the configured type is rejected and the failure is
reported, never downgraded.

**Encryption step.** Kept separate and later, so that viewer interoperability is not a precondition for the first
authentication evidence. Until it lands, the release notes and the compatibility statements say the stream is
**authenticated but not encrypted**, and no document may describe it as encrypted.

**Primitives and dependency.** VNC authentication needs DES and SHA-1, and the encryption step needs TLS. These come
from **OpenSSL** deliberately, rather than from code written in this repository: a hand-written DES is a security
anti-pattern. This is the repository's first third-party security dependency, carrying the obligations the release
readiness authority already anticipates - `package`, `LICENSE`, `NOTICE` and the deployed payload list are updated
in the change that lands the dependency, never in advance.

**Bind policy.** A non-loopback listener stays fail-closed until an authentication mode is actually enabled: with
authentication enabled a non-loopback bind may be accepted; with authentication disabled it is still rejected. The
construction and listener defaults do not change (no implicit listener, loopback, remote input off), and
authentication is not remote input - view-only remains the default input policy.

**Failure semantics.** Authentication failure closes the connection after a bounded number of attempts, with a
bounded challenge/response exchange and no unbounded buffering, and is reported through the transport event already
reserved for it (`TransportEventCode::AuthenticationRejected`) instead of being silently tolerated. Authentication is
not authorization: an authenticated connection still has no licence to control input.

**Documentation and gate consequence.** `SecurityType None` is currently pinned as a required phrase by
`tests/release-readiness/check_release_metadata.cmake` and by the release-policy workflow, and is stated in roughly
twenty user-facing documents. Landing authentication is therefore a coordinated, separately reviewed change: the
gates that pin the current baseline move in the same change that lands the capability.
## 11. Non-goals

HyRemote V1 does not provide:

- user/account management;
- fleet identity;
- cloud authorization;
- centralized audit storage;
- VPN/tunnel provisioning;
- firewall management;
- public-Internet-safe transport security;
- per-client identity/role management.

Products may place those systems around HyRemote, but they remain separate trust boundaries.

## 12. V1 release security gate

Before `v1.0.0.0` is authorized, evidence must confirm at least:

- [x] Apache-2.0 project licensing is frozen;
- [x] construction/installation does not implicitly open a listener;
- [x] loopback is the default bind policy;
- [x] remote input is disabled by default and explicitly controlled;
- [x] the current SecurityType None limitation is explicit in user docs/release notes;
- [x] bounded protocol/frame/input/handshake behavior is represented in implementation/tests;
- [x] no V1 documentation claims password/TLS/authenticated identity support;
- [ ] Windows x86_64 reference acceptance actually executes and passes;
- [ ] Linux x86_64 reference acceptance actually executes and passes;
- [ ] physical native local-display/local-input + remote coexistence evidence required by #109/#32 passes.

A GitHub Actions job that never receives a runner is neither passing security evidence nor a code failure. #74 must not be bypassed by relabeling no-runner results.

The concise V1 user-facing statement remains:

> HyRemote V1 authenticates the viewer when an authenticated security profile is configured (RFB VNC authentication) and does not encrypt the stream. It defaults to loopback with remote input off. Keep a listener behind an appropriate trusted access boundary; do not expose it directly to the public Internet.
