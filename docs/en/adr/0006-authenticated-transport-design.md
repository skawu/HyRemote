# ADR-0006: Authenticated and Encrypted Transport Design (`[SEC-01]` #143)

> Language / 语言: **English** | [中文](../../adr/0006-authenticated-transport-design.md)

Status: **Proposed for `[SEC-01]` (#143), design step; partly superseded by #174**

> **Partly superseded by #174 (2026-09-23).** The reachability statements below describe the design as decided. The shipped contract is: the listener defaults to `0.0.0.0:5921`, may be narrowed to one exact IPv4 address or one named interface, and the security state actually shipped is reported honestly. Narrowing the bind is not a security workaround.

## Context

The release roadmap makes authenticated and encrypted transport a **V1.0.0.0 requirement, not a `V1.x` track**, sequenced as design, authentication, encryption, per-client authorization/audit. At the time, `docs/security-model.md` section 10 still described it as a post-baseline capability. Two release authorities disagreed, which is the class of defect the converged scope authority exists to prevent.

The transport advertised exactly one RFB security type, `None`, as an inline literal in the handshake. The repository contained no cryptographic code and no third-party security dependency. A non-loopback listener was rejected by the “fail closed until authenticated transport exists” guard, which is the guard a real mechanism should replace.

## Decision

1. Authentication uses **RFB VNC authentication (security type 2)**. `None` remains only as an explicitly selected mode, never as an implicit downgrade.
2. **Encryption is a separate, later step**, so viewer interoperability is not a precondition for the first authentication evidence.
3. Cryptographic primitives come from **OpenSSL**, accepting the repository's first third-party security dependency with its package, licence, notice, and deployment obligations.
4. **A non-loopback bind is accepted only while an authentication mode is enabled**; otherwise it stays fail-closed. Construction and listener defaults are unchanged.
5. **Authentication is not authorization**: the input-policy boundary is untouched, failures are bounded, and no user-facing document may describe the stream as authenticated or encrypted before the step lands.

Interface detail, failure semantics, and documentation/gate consequences are recorded in `docs/security-model.md` section 10.1. That section is this work's threat-model home; no separate document is created.

## Consequences

- Implementation is unblocked in the planned order. The documentation/gate change is a coordinated, separately reviewed release change because the current baseline statement is machine-pinned.
- **ADR numbering**: 0004 and 0005 are reserved by the in-flight documentation-zone branch `docs/zones-and-qpa-correctness` (`bc41df1`), so this record takes **0006**. Its index entry is added to `docs/architecture.md` at the path that exists on the candidate line.
- Deferred, not decided here: certificate/key provisioning and trust-store management, per-client authorization/audit detail, and whether a TLS-only deployment must be supported for viewers that cannot perform VNC authentication.
