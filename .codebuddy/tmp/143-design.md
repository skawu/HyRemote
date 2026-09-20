## Design step frozen: authenticated and encrypted transport (owner rulings 2026-09-19)

Four design questions were put to the owner and all four were answered as recommended. They are now written as a design record rather than left as intent.

| Question | Ruling |
| --- | --- |
| Authentication mechanism for the first step | **RFB VNC authentication (security type 2)**, including the RFB 3.8 blind challenge. `SecurityType None` stays available, but only as an **explicitly selected** mode - never an implicit downgrade |
| Cryptographic primitives | **OpenSSL**, not an in-tree implementation (a hand-written DES is a security anti-pattern). This is the repository's first third-party security dependency and carries the package/licence/notice/deployed-payload obligations the release-readiness authority already anticipates |
| Where the design lives | **`docs/security-model.md` section 10, corrected and extended, plus ADR-0006** - not a new document, so the repository does not acquire a third security narrative |
| Non-loopback binding | **Accepted only while an authentication mode is enabled**, fail-closed otherwise - which is exactly what the existing guard `c2898c5` ("fail closed on a non-loopback listener until authenticated transport exists") was waiting for |

### What the design now says

- **Sequence**: authentication first, encryption as a **separate later step**, so viewer interoperability is not a precondition for the first authentication evidence.
- **Failure semantics**: bounded attempts, a bounded challenge/response exchange, no unbounded buffering, and the failure is **reported** through `TransportEventCode::AuthenticationRejected` (already present in the transport contract, currently a no-op) instead of being silently tolerated.
- **Authentication is not authorization**: the input-policy boundary is untouched, view-only remains the default.
- **Safe defaults unchanged**: no implicit listener, loopback by default, remote input off.
- **The contradiction is fixed**: section 10 said authenticated/encrypted transport was "a post-baseline capability unless separately accepted into a later release", while `docs/development-roadmap.md` makes it a **V1.0.0.0 requirement, not a `V1.x` track**. The roadmap governs, so the section was corrected and the design recorded as section 10.1.

### A consequence that affects scheduling

`SecurityType None` is **machine-pinned**, not merely prose: `tests/release-readiness/check_release_metadata.cmake` requires the phrase in every milestone release note, `.github/workflows/git-flow-policy.yml` requires it in release-branch and tag notes, and roughly twenty user-facing documents state it. Landing authentication is therefore a coordinated documentation-plus-gate change made in the same change as the capability - not in this one.

### What was prepared

`docs/security-model.md` (section 10 corrected, section 10.1 added), `docs/adr/0006-authenticated-transport-design.md` (new), `docs/architecture.md` (index line). 37 insertions, 2 deletions, verified as a minimal diff: an earlier text-mode round-trip rewrote an entire file's line endings, so the edits were redone in byte mode reusing each file's own newline.

**Status, stated plainly**: the change is on disk but **not committed and not pushed**. `git checkout -b`, `commit` and `push` require an approval prompt that has timed out repeatedly in this session, and this agent will not bypass that mechanism by driving git from a script. The intended PR base is `feature/104-v1-ga-acceptance-matrix`.

**ADR numbering**: this record takes 0006 because 0004 and 0005 are already reserved by the in-flight documentation-zone branch (`docs/zones-and-qpa-correctness`, `bc41df1`).

**Next in the plan sequence**: the authentication implementation itself, once this lands.
