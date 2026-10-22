# HyRemote CI: purpose, trigger and limits (Owner #571, 2026-10-22)

## Why this policy exists

The old trigger ran costly full Windows+Linux Qt product builds every time an open PR was updated, repeatedly consuming runners and delaying feature work. Worse, the protected `develop` merge push used a policy-only sentinel, so the expensive work occurred **before every merge**, rather than after a meaningful integration event.

**New rule:** normal feature branch commits and PR opening/updates **do not trigger the `CI` workflow**. Only a push to protected `develop` (normally a merged PR) automatically invokes it. A frozen release candidate or a deliberate diagnostic may trigger `CI` by manual `workflow_dispatch` with an exact commit SHA. This workflow has no scheduled trigger.

Git Flow and the independent `p1-p2-disposition` review check remain lightweight PR checks. GitHub's current protected `develop` ruleset requires `p1-p2-disposition`; it does not require CI to pass before merging. Agents still run local or focused checks on their changes when they have the toolchain. **This deliberately changes the risk model:** failures may be detected *after* merge, so a broken `develop` must be repaired immediately through an Issue-linked PR, without mislabeling it green.

## Contract by event

| Event | Workflow | Required work | Decision |
| --- | --- | --- | --- |
| Ordinary feature commit, PR open/update, draft/review transition | **No `CI` run** | Git Flow topology and independent code/review-disposition checks; local/focused tests by agent as available | PR review, not hosted Qt acceptance |
| Protected `develop` push after merge of product/build/tests code | `CI` automatically | Diff `before..sha`, select impacted frontends, **Windows x86_64 Qt product runner**, real compile/CTest and applicable install/deploy/consumer checks; zero executed tests invalid | `POST_MERGE / OBSERVE`, not a release/candidate approval |
| Protected `develop` push with docs/governance-only scope | `CI` classification only | Seconds-scale scope/governance if relevant, **no Qt SDK install/build** | `POST_MERGE / OBSERVE` with explicit no-product-evidence label |
| Manual `workflow_dispatch` with matching `expected_sha` | Explicit FULL_GATE | Frozen candidate/release profile and all exact-SHA, independently required evidence | `CANDIDATE`, only after admitted gate actually passes |

The automatic post-merge Qt matrix selects **Windows only** as the sole physical/hosted qualified development platform (Owner #563). Linux/embedded user validation is nonblocking and must never be relabelled PASS from a Windows result. The separately dispatched FULL_GATE's pre-existing historical release profiles and exact-sha controls remain intact until independently revised; do not interpret a post-merge OBSERVE result as satisfying FULL_GATE or an old dual-platform historical maintenance contract.

### What CI can verify

- **Reproducible build:** exact commit and declared Qt compiler Kit can configure and compile on the runner.
- **Automated correctness and regressions:** actually discovered and executed CTest/protocol/regression cases pass; a skipped or zero-test result is not product PASS.
- **Install/deploy quality where selected:** SDK installation, dependency closure, selected consumer/test binaries and release metadata satisfy automated contracts.
- **Integration drift:** a merged change does not break the currently tested Windows reference build; failures are surfaced with logs and an affected SHA.
- **Governance in context:** release authority scripts/policies match the repository where they are actually exercised.

### What CI cannot prove

- A code change was safe **before it merged** when no premerge product tests ran.
- Absence of defects, full real GUI Viewer experience, physical keyboard/mouse/display coexistence, frame-age/interaction SLO unless the corresponding exact real measurements ran.
- Linux/ARM64/EGLFS/Wayland operation or any specific user Qt/BSP combination; Qt source portability is not target qualification.
- Network security, TLS or authorization not provided by the shipped RFB implementation; automated success cannot make plaintext RFB suitable for Internet exposure.
- Support of Qt 5.15/6.12 or any unqualified Kit simply because Qt 6.8.3 Windows compiled.
- V0.3 Feature acceptance, V0.4 RC qualification or V1 GA without all **separate** exact-SHA evidence and review authorities.

## Failure and owner response

A post-merge Windows CI failure is **not a green merge retrospectively**. Record exact `develop` SHA, failing Windows job/test/log, affected user claim, owning Issue and regression fix PR. Prefer a bounded repair/rollback decision before stacking unrelated product changes. A post-merge success only validates the tested surfaces and does not close release issues automatically.

If quick premerge validation is desired for a high-risk PR, the developer/agent can run canonical local tests or request a separate bounded manual diagnostic. Do not reintroduce automatic every-commit Qt CI or turn a docs-only PR into a full Qt build by convention.

References: #24, #95, #343, #380, #563, #568, #571; `.github/workflows/ci.yml`, `.github/workflows/git-flow-policy.yml`, `.github/scripts/resolve-ci-scope.py` and branch ruleset "protect develop".
