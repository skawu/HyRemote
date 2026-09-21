# Security Policy

HyRemote is remote-access infrastructure and should be treated as security-sensitive software.

The current product line is **V0.1 Developer Preview**. It is intended for development, evaluation, and trusted-network use; it is **not** an Internet-facing production remote-access service.

## Supported product versions

No HyRemote version has reached the V1 GA security-support contract yet.

Current product documentation describes the behavior and limitations of the V0.x preview line. Security capabilities marked **TODO** or **Preview** are not production-security claims.

See [`docs/security.md`](docs/security.md) and [`docs/compatibility.md`](docs/compatibility.md) for the current product boundary.

## Current transport security

HyRemote currently uses a bounded RFB/VNC transport baseline.

V0.1 behavior:

- listener defaults to loopback (`127.0.0.1`);
- remote input is disabled by default;
- `Insecure` is loopback-only and non-loopback startup is rejected;
- `Authenticated` requires a transport-security-enabled build plus a valid security descriptor; when available it uses RFB VNC authentication but does not encrypt the stream;
- the default V0.1 build/profile does not imply authenticated transport is compiled in;
- `AuthenticatedEncrypted` is not implemented and always fails closed before listener creation, without fallback to a weaker profile.

Authentication is not encryption. A viewer password prompt does not make the transport confidential.

Do not expose the current HyRemote listener directly to the public Internet or another untrusted network.

> **TODO V0.2:** encrypted transport, certificate policy, authenticated sessions, and production network policy.

## Security defaults apply to every frontend

The four product frontends converge on the same Shared Runtime and security model:

- **C++ API** — explicit Runtime lifecycle and policy;
- **Generic Plugin** — zero-code public-Qt plugin path;
- **QML API** — declarative frontend over the same Runtime;
- **QPA** — specialized private-ABI frontend.

Changing frontend does not silently create a stronger transport-security profile.

## Security invariants

HyRemote's product design preserves these boundaries:

- constructing or installing HyRemote does not by itself open a listener;
- loopback is the default bind scope;
- remote viewing and remote control are separate policies;
- remote input is opt-in;
- `Running` and `connectedClientCount()` are operational state, not authentication or authorization;
- malformed-client/protocol/frame/input state must remain bounded;
- recognized held remote input is balanced on disconnect and Runtime teardown;
- secrets must not be written to ordinary logs or diagnostics;
- a requested security capability fails closed rather than silently downgrading;
- Generic keeps the application's native Qt platform identity;
- QPA private-ABI use does not weaken the security boundary or create an undocumented bypass.

## Security-sensitive changes

Changes involving any of the following require explicit security review:

- authentication or authorization;
- network listeners and protocol negotiation;
- TLS or other transport encryption;
- remote-input enablement;
- default bind address or exposed port;
- credentials, certificates, tokens, or private keys;
- session admission/termination controls;
- viewer lifecycle and held-input cleanup;
- malformed-client/resource bounds;
- system-level input injection;
- Qt private/QPA platform integration;
- third-party protocol or cryptographic libraries.

## Reporting a vulnerability

Do **not** open a public issue containing exploit details, credentials, proof-of-concept payloads, private keys, or other sensitive vulnerability information.

Use GitHub's private vulnerability reporting flow from the repository's **Security** area when that option is available.

If no private reporting option is available, contact the repository owner through a private contact method published on the owner's GitHub profile. If no private channel can be found, open only a non-sensitive public issue requesting a security contact and do not include vulnerability details.

A useful private report includes, when practical:

- affected HyRemote version or commit;
- operating system, architecture, and Qt version;
- integration frontend (C++, Generic, QML, or QPA);
- reproduction conditions;
- security impact;
- whether non-default bind/input/security policy is required;
- any known temporary mitigation.

## Disclosure

Please allow reasonable time for triage, remediation, and coordinated disclosure.

A security fix does not automatically broaden the product compatibility matrix. Updated security/support claims are published through the normal product documentation and release notes.

HyRemote's Apache-2.0 license does not replace the license, security-update, or redistribution obligations of Qt or any other third-party component used by a downstream product.
