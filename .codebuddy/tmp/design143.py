"""Land the #143 design-stage deliverables on the candidate base.

Rulings applied (all option A): VNC authentication first, OpenSSL for primitives, design in
docs/security-model.md section 10 plus an ADR, non-loopback accepted only with authentication enabled.

Safety: line endings are detected and preserved, every replacement must match exactly once, and a failed
assertion aborts before anything is written. ADR numbering takes 0006 because 0004/0005 are already taken by
the in-flight documentation-zone branch (docs/zones-and-qpa-corrections, commit bc41df1).
"""

import pathlib

ROOT = pathlib.Path(r"f:/workspace/hyremote/hyremote-wt-106")

SEC = ROOT / "docs/security-model.md"
ARCH = ROOT / "docs/architecture.md"
ADR = ROOT / "docs/adr/0006-authenticated-transport-design.md"

raw = SEC.read_bytes()
newline = "\r\n" if b"\r\n" in raw[:2000] else "\n"
text = raw.decode("utf-8")
print("detected newline: %r" % newline)

# --- 1) Section 10: correct the sentence that contradicts the plan, and reword the lead-in. ---
old10 = (
    "Authenticated/encrypted transport support is a post-baseline capability unless separately accepted into a later"
    " release." + newline + newline +
    "A future transport may add, behind the stable application model:"
)
new10 = (
    "Authenticated and encrypted transport is a **V1.0.0.0 requirement, not a `V1.x` track**: `[SEC-01]` #143 is x86"
    " work, sequenced design -> authentication -> encryption -> per-client authorization/audit. This supersedes the"
    " earlier statement that it was a post-baseline capability accepted into some later release, which contradicted"
    " the release roadmap." + newline + newline +
    "It is added behind the stable application model:"
)
assert text.count(old10) == 1, "section 10 opening did not match exactly once"

# --- 2) Section 10: record the frozen design, before section 11. ---
design = newline.join([
    "### 10.1 Design frozen before implementation (2026-09-19)",
    "",
    "**Nothing here changes today's product statement.** Until the authentication step actually lands, the stream remains",
    "unauthenticated and unencrypted, and no user-facing document may claim otherwise.",
    "",
    "**Authentication step.** RFB VNC authentication (security type 2), including the RFB 3.8 challenge/response and its",
    "blinding variant, because that is what the mainstream viewers already implement. `SecurityType None` stays available",
    "but only as an **explicitly selected** mode: a client that does not select the configured type is rejected and the",
    "failure is reported, never downgraded.",
    "",
    "**Encryption step.** Kept separate and later, so that viewer interoperability is not a precondition for the first",
    "authentication evidence. Until it lands, the release notes and the compatibility statements keep saying the stream",
    "is unauthenticated and unencrypted.",
    "",
    "**Primitives and dependency.** VNC authentication needs DES and SHA-1, and the encryption step needs TLS. These come",
    "from **OpenSSL** deliberately, rather than from code written in this repository: a hand-written DES is a security",
    "anti-pattern. This is the repository's first third-party security dependency, carrying the obligations the release",
    "readiness authority already anticipates - `package`, `LICENSE`, `NOTICE` and the deployed payload list are updated",
    "in the change that lands the dependency, never in advance.",
    "",
    "**Bind policy.** A non-loopback listener stays fail-closed until an authentication mode is actually enabled: with",
    "authentication enabled a non-loopback bind may be accepted; with authentication disabled it is still rejected. The",
    "construction and listener defaults do not change (no implicit listener, loopback, remote input off), and",
    "authentication is not remote input - view-only remains the default input policy.",
    "",
    "**Failure semantics.** Authentication failure closes the connection after a bounded number of attempts, with a",
    "bounded challenge/response exchange and no unbounded buffering, and is reported through the transport event already",
    "reserved for it (`TransportEventCode::AuthenticationRejected`) instead of being silently tolerated. Authentication is",
    "not authorization: an authenticated connection still has no licence to control input.",
    "",
    "**Documentation and gate consequence.** `SecurityType None` is currently pinned as a required phrase by",
    "`tests/release-readiness/check_release_metadata.cmake` and by the release-policy workflow, and is stated in roughly",
    "twenty user-facing documents. Landing authentication is therefore a coordinated, separately reviewed change: the",
    "gates that pin the current baseline move in the same change that lands the capability.",
    "",
])
marker11 = "## 11. Non-goals"
assert text.count(marker11) == 1, "section 11 heading did not match exactly once"

text = text.replace(old10, new10, 1)
text = text.replace(marker11, design + marker11, 1)
SEC.write_bytes(text.encode("utf-8"))

# --- 3) ADR index line. ---
index_old = "- [`ADR-0003 Threading, Scheduling and Backpressure`](adr/0003-threading-backpressure.md);"
assert ARCH.read_text(encoding="utf-8").count(index_old) == 1, "ADR-0003 index line did not match exactly once"
arch = ARCH.read_text(encoding="utf-8").replace(
    index_old,
    index_old + newline
    + "- [`ADR-0006 Authenticated and Encrypted Transport Design`](adr/0006-authenticated-transport-design.md);",
    1,
)
ARCH.write_text(arch, encoding="utf-8")

# --- 4) The ADR itself. ---
adr_text = newline.join([
    "# ADR-0006: Authenticated and Encrypted Transport Design (`[SEC-01]` #143)",
    "",
    "Status: **Proposed for `[SEC-01]` (#143), design step**",
    "",
    "## Context",
    "",
    "The release roadmap makes authenticated and encrypted transport a **V1.0.0.0 requirement, not a `V1.x` track**,",
    "sequenced design -> authentication -> encryption -> per-client authorization/audit - while `docs/security-model.md`",
    "section 10 still called it a post-baseline capability to be accepted into a later release. Two release authorities",
    "disagreed, which is the class of defect the converged scope authority exists to prevent.",
    "",
    "The transport today advertises exactly one RFB security type, `None`, as an inline literal in the handshake; the",
    "repository contains **no** cryptographic code and **no** third-party security dependency; and a non-loopback listener",
    "is currently rejected by the \"fail closed on a non-loopback listener until authenticated transport exists\" guard,",
    "which is precisely the guard a real mechanism is meant to replace.",
    "",
    "## Decision",
    "",
    "1. The authentication step is **RFB VNC authentication (security type 2)**, with `None` retained only as an",
    "   explicitly selected mode - never as an implicit downgrade.",
    "2. **Encryption is a separate, later step**, so viewer interoperability is not a precondition for the first",
    "   authentication evidence.",
    "3. Cryptographic primitives come from **OpenSSL**, accepting the repository's first third-party security dependency",
    "   together with its package/licence/notice/deployment obligations.",
    "4. A **non-loopback bind is accepted only while an authentication mode is enabled**, and stays fail-closed otherwise;",
    "   construction and listener defaults are unchanged.",
    "5. **Authentication is not authorization**: the input policy boundary is untouched, failures are bounded, and no",
    "   user-facing document may describe the stream as authenticated or encrypted before the step lands.",
    "",
    "The interface detail, failure semantics and the documentation/gate consequences are recorded in",
    "`docs/security-model.md` section 10.1, which is this work's threat-model home rather than a new document.",
    "",
    "## Consequences",
    "",
    "- Implementation is unblocked in the planned order, and the documentation/gate change is a coordinated, separately",
    "  reviewed release change because the current baseline statement is machine-pinned.",
    "- **ADR numbering**: 0004 and 0005 are reserved by the in-flight documentation-zone branch",
    "  (`docs/zones-and-qpa-correctness`, `bc41df1`), so this record takes **0006**; its index entry is added to",
    "  `docs/architecture.md` at the path that exists on the candidate line.",
    "- Deferred, not decided here: certificate/key provisioning and trust-store management, the per-client",
    "  authorization/audit detail, and whether a TLS-only deployment must also be supported for viewers that cannot do",
    "  VNC authentication.",
    "",
])
ADR.parent.mkdir(parents=True, exist_ok=True)
ADR.write_bytes(adr_text.encode("utf-8"))

print("wrote: docs/security-model.md (section 10 corrected + 10.1 added)")
print("wrote: docs/adr/0006-authenticated-transport-design.md")
print("wrote: docs/architecture.md (ADR index line)")
print("section 10.1 present in file:", "### 10.1 Design frozen" in SEC.read_text(encoding="utf-8"))
