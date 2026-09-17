# Security Policy

HyRemote is currently in **pre-alpha architecture development** and has not yet made a public production-ready release.

## Supported versions

No released version is currently supported for production security updates.

## Security-sensitive areas

Remote access software has a high security impact. Changes involving any of the following require explicit security review:

- authentication or authorization;
- network listeners and protocol negotiation;
- TLS/transport security;
- remote input enablement;
- default bind address or exposed port;
- credential storage;
- session enable/disable controls;
- system-level input injection such as `uinput`;
- third-party protocol or crypto libraries.

## Default security direction

Until a security model is frozen:

- remote access must not be assumed safe for direct Internet exposure;
- insecure or legacy authentication must not be presented as sufficient protection;
- transports should support binding to restricted interfaces/addresses where possible;
- remote input should be independently disableable from remote viewing;
- higher-level deployments should use a trusted network, VPN, or equivalent secure tunnel when transport security is insufficient.

## Reporting a vulnerability

The repository is public. Please **do not** open a public issue containing exploit details.

- Preferred channel: GitHub's private vulnerability reporting for this repository (`Security` tab, then
  `Report a vulnerability`). *Repository action outstanding: the private reporting feature is not yet
  enabled; enabling it makes this the documented channel.*
- Until it is enabled: contact the maintainer directly using the contact details on the repository
  owner's profile and state clearly that the report is a security issue.

## Current transport security state

The transport shipped today (the bounded RFB 3.8 baseline) implements **SecurityType `None` over
plaintext** and cannot express authentication or encryption. Treat the 0.0.x line as **local/loopback or
trusted-tunnel only** and never expose the listener to an untrusted network. Requirements and the
release gate that must be satisfied before any stronger claim: [`docs/security-model.md`](docs/security-model.md).
