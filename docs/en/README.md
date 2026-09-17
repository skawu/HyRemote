# HyRemote documentation

> Language / 语言: **English** | [中文](../README.md)

HyRemote keeps the application-facing model deliberately small: **one shared C++ library** plus two optional
integration payloads. This index is organised by reader intent rather than by development history.

## 1. Start here

| I want to | Read |
| --- | --- |
| Install, build and integrate HyRemote | [`guide/install.md`](guide/install.md) | [中文](../guide/install.md) |
| Integrate with the Embedded C++ API | [`../getting-started/cpp.md`](../getting-started/cpp.md) |
| Integrate with Declarative QML | [`../getting-started/qml.md`](../getting-started/qml.md) |
| Use Transparent QPA without touching application source | [`../getting-started/qpa-proxy.md`](../getting-started/qpa-proxy.md) |
| Package and deploy | [`../deployment.md`](../deployment.md) |
| Run and connect a viewer | [`../viewer-connection.md`](../viewer-connection.md) |
| Diagnose a problem | [`../troubleshooting.md`](../troubleshooting.md) |

## 2. Three zones

| Zone | For | Language | Content rule |
| --- | --- | --- | --- |
| `docs/guide/**` | End users | Chinese primary + `docs/en/**` mirror | Final shape only: install, integrate, deploy, troubleshoot. **No process content** |
| `docs/reference/**` | Product final-state contracts | Same | Architecture, capture/input model, API stability, compatibility, security boundary, versioning |
| Remainder of `docs/` plus `adr/`, `releases/`, `proposals/` | Maintainers / release | English (frozen by release gates) | Acceptance runbooks, repository administration, layout authority, milestone records |

**Process content** - issue numbers and tracking, acceptance scheduling and status boards, milestone chronicles,
investigation logs and one-off checklists - is excluded from the user-facing zone by definition. It belongs to the
internal zone, or to the research evidence under `research/`.

## 3. Reference (product final state)

| Document | Content |
| --- | --- |
| [`../architecture.md`](../architecture.md) | Layer model, dependency rules, threading principles |
| [`../v1-api-stability.md`](../v1-api-stability.md) | V1 public API stability contract (stable and non-stable surfaces) |
| [`../compatibility.md`](../compatibility.md) | Exact evidence/status matrix |
| [`../known-limitations.md`](../known-limitations.md) | Explicit V1 limitations |
| [`../security.md`](../security.md) | Implemented security boundary |
| [`../security-model.md`](../security-model.md) | Threat model and release security gate |
| [`../versioning.md`](../versioning.md) | Versioning and milestone policy |
| [`../release-package-manifest.md`](../release-package-manifest.md) | V1 release package manifest (installed payload contract) |
| [`../dependency-policy.md`](../dependency-policy.md) | Dependency policy, including optional build-time dependencies |
| [`../widgets-capture.md`](../widgets-capture.md) | [`../quick-capture.md`](../quick-capture.md) | [`../input-model.md`](../input-model.md) | Capture and input model |

## 4. Internal / release documents (not user documentation)

[`../repository-layout.md`](../repository-layout.md) (layout authority), [`../branch-lifecycle.md`](../branch-lifecycle.md),
[`../git-flow-release.md`](../git-flow-release.md), [`../v1-ga-acceptance.md`](../v1-ga-acceptance.md),
[`../v1-physical-acceptance.md`](../v1-physical-acceptance.md), [`../v1-repository-admin.md`](../v1-repository-admin.md),
[`../release-candidate-checklist.md`](../release-candidate-checklist.md), [`../adr/`](../adr/), [`../releases/`](../releases/),
[`../development-roadmap.md`](../development-roadmap.md), and the research/evaluation records
(`capture-spike.md`, `async-capture-spike.md`, `neatvnc-evaluation.md`, `x86-vnc-transport-evaluation.md`,
`qpa-*-qt-6.8.3.md`, [`../proposals/`](../proposals/)).

Ordinary integration work does not require this zone.

## 5. Writing conventions (summary)

- **Bilingual**: the Chinese primary document lives at `docs/<path>` and the English mirror at `docs/en/<path>`.
  The two trees must stay isomorphic (one-to-one by relative path), and each document carries a one-line language
  switch at the top. A change to a user/reference document updates both languages in the same change.
- **One document per intent**: user documentation is consolidated by reader intent, not split by development
  stage. Prefer one detailed document over five that each cover a fragment.
- **Source comments**: Chinese primary with an English counterpart (`// 中文说明 — English`), rolled out in stages
  (public headers, then internal code, then tests).
- **Links**: when a document moves, update every reference in the repository; no forwarding copies.

Full rules: [`CONTRIBUTING.md`](../../CONTRIBUTING.md).

## 6. Migration status

| Stage | Status |
| --- | --- |
| `guide/install.md` (Windows/Linux guides + installed SDK + source consumption merged) | Done, bilingual |
| Rest of `guide/` (cpp, qml, qpa-proxy, deploy, operate) | Pending |
| `reference/` zone (architecture, capture-and-input, merged security, ...) | Pending |
| Research/evaluation records moving to `research/`, process content removed from user docs | Pending |
