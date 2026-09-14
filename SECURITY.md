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

The repository is currently private during bootstrap. Security reporting instructions for external contributors will be added before the repository is made public.

Do not open a public issue containing exploit details after public launch; use the private reporting mechanism documented here at that time.
