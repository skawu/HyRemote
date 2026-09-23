# ADR-0006: Authenticated and Encrypted Transport Design (`[SEC-01]` #143)

Status: **Proposed for `[SEC-01]` (#143), design step**

> **Superseded in part by #174 (2026-09-23).** The reachability statements below describe the design as it was
> decided. The shipped contract is that the listener defaults to `0.0.0.0:5921`, may be narrowed to one exact IPv4
> or one named interface, and that the security state actually shipped is reported truthfully instead of the bind
> being narrowed as a security workaround.

## Context

The release roadmap makes authenticated and encrypted transport a **V1.0.0.0 requirement, not a `V1.x` track**,
sequenced design -> authentication -> encryption -> per-client authorization/audit - while `docs/security-model.md`
section 10 still called it a post-baseline capability to be accepted into a later release. Two release authorities
disagreed, which is the class of defect the converged scope authority exists to prevent.

The transport today advertises exactly one RFB security type, `None`, as an inline literal in the handshake; the
repository contains **no** cryptographic code and **no** third-party security dependency; and a non-loopback listener
is currently rejected by the "fail closed on a non-loopback listener until authenticated transport exists" guard,
which is precisely the guard a real mechanism is meant to replace.

## Decision

1. The authentication step is **RFB VNC authentication (security type 2)**, with `None` retained only as an
   explicitly selected mode - never as an implicit downgrade.
2. **Encryption is a separate, later step**, so viewer interoperability is not a precondition for the first
   authentication evidence.
3. Cryptographic primitives come from **OpenSSL**, accepting the repository's first third-party security dependency
   together with its package/licence/notice/deployment obligations.
4. A **non-loopback bind is accepted only while an authentication mode is enabled**, and stays fail-closed otherwise;
   construction and listener defaults are unchanged.
5. **Authentication is not authorization**: the input policy boundary is untouched, failures are bounded, and no
   user-facing document may describe the stream as authenticated or encrypted before the step lands.

The interface detail, failure semantics and the documentation/gate consequences are recorded in
`docs/security-model.md` section 10.1, which is this work's threat-model home rather than a new document.

## Consequences

- Implementation is unblocked in the planned order, and the documentation/gate change is a coordinated, separately
  reviewed release change because the current baseline statement is machine-pinned.
- **ADR numbering**: 0004 and 0005 are reserved by the in-flight documentation-zone branch
  (`docs/zones-and-qpa-correctness`, `bc41df1`), so this record takes **0006**; its index entry is added to
  `docs/architecture.md` at the path that exists on the candidate line.
- Deferred, not decided here: certificate/key provisioning and trust-store management, the per-client
  authorization/audit detail, and whether a TLS-only deployment must also be supported for viewers that cannot do
  VNC authentication.
