# Security Policy

HyRemote is currently in **V1 convergence** and has not yet made a production-ready GA release.

## Supported versions

No released version is currently supported for production security updates. V1.0.0.0 remains acceptance-pending until the documented Windows/Linux and physical/native release gates pass.

## Current transport security state

The bounded internal RFB 3.8 correctness transport negotiates **SecurityType None over plaintext** unless an authenticated profile is configured, in which case the viewer is authenticated with **RFB VNC authentication** (security type 2) and a client that does not select that type is rejected rather than downgraded. HyRemote V1 provides **no transport encryption**: TLS is a separate, later step, and the stream must not be described as encrypted.

The listener defaults to loopback (`127.0.0.1`) and remote input is disabled by default, but those safe defaults are not authentication or encryption. Do not expose the current HyRemote listener directly to the public Internet or an otherwise untrusted network. Use an appropriate trusted network boundary, VPN or separately managed secure tunnel when remote reachability is required.

The complete security architecture and release gate are documented in [`docs/security-model.md`](docs/security-model.md). The concise deployment guidance is in [`docs/security.md`](docs/security.md).

## Security-sensitive areas

Remote access software has a high security impact. Changes involving any of the following require explicit security review:

- authentication or authorization;
- network listeners and protocol negotiation;
- TLS/transport security;
- remote input enablement;
- default bind address or exposed port;
- credential storage;
- runtime/session enable-disable controls;
- viewer lifecycle and held-input cleanup;
- malformed-client/resource bounds;
- system-level input injection such as `uinput`;
- Qt private/QPA platform integration;
- third-party protocol or crypto libraries.

## V1 security invariants

V1 must preserve these boundaries:

- constructing/installing HyRemote does not implicitly open a listener;
- the normal bind default is loopback-only;
- remote viewing and remote control are separate policies;
- remote input is opt-in;
- Embedded C++/QML policy changes use explicit stopped-runtime configuration;
- Transparent QPA remote-input policy is explicit startup/relaunch policy;
- `Running` and `connectedClientCount()` are operational state, not authentication/authorization;
- protocol/frame/client/input state remains bounded;
- recognized held remote input is balanced on abrupt viewer disconnect and explicit runtime shutdown;
- no password/TLS/authenticated-identity capability is implied by the current V1 API;
- native local display/input remains authoritative in the Transparent QPA path.

## Reporting a vulnerability

This repository is public. **Do not open a public issue containing exploit details, credentials, proof-of-concept payloads or other sensitive vulnerability information.**

Preferred reporting channel: GitHub private vulnerability reporting for this repository (`Security` -> `Report a vulnerability`) once that repository feature is enabled.

If the private reporting UI is not available, contact the repository owner through a private contact method published on the owner's GitHub profile. If no private contact channel is available, open only a non-sensitive public issue asking for a security contact; do not include vulnerability details in that issue.

When reporting privately, include when practical:

- affected commit/version;
- operating system / architecture / Qt version;
- integration mode (C++, QML or QPA);
- reproduction conditions;
- security impact;
- whether the default loopback/input-off policy must be changed to trigger the issue;
- any temporary mitigation known to you.

## Disclosure expectations

Please allow reasonable time for triage and remediation before public disclosure. Security fixes must not be described as accepted release evidence until the relevant release/compatibility gates actually execute and pass.

HyRemote's Apache-2.0 license does not change the licensing or security-update obligations of Qt or other third-party components used by a downstream deployment.
