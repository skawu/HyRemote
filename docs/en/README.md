# HyRemote documentation

> Language / 语言: **English** | [中文](../README.md)

HyRemote keeps the application-facing model deliberately small: **one shared C++ library** plus two optional
integration payloads. This index is organised by reader intent rather than by development history.

## 1. Start here

| I want to | Read |
| --- | --- |
| Install, build and integrate HyRemote | [English](guide/install.md) · [中文](../guide/install.md) |
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
| The product final-state contracts at the top level of `docs/` | Product final-state contracts | Same | Architecture, capture/input model, API stability, compatibility, security boundary, versioning (`architecture.md`, `widgets-capture.md`, `quick-capture.md`, `input-model.md`, `v1-api-stability.md`, `compatibility.md`, `security.md`, `versioning.md`, ...) |
| `docs/internal/**` plus `docs/adr/`, `docs/releases/`, `docs/proposals/` | Maintainers / release | English (frozen by release gates) | Acceptance runbooks, repository administration, layout authority, milestone records, research/evaluation records |

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
| [`../widgets-capture.md`](../widgets-capture.md) · [`../quick-capture.md`](../quick-capture.md) · [`../input-model.md`](../input-model.md) | Capture and input model |

## 4. Internal / release documents (not user documentation)

[`../internal/repository-layout.md`](../internal/repository-layout.md) (layout authority),
[`../internal/branch-lifecycle.md`](../internal/branch-lifecycle.md), [`../internal/git-flow-release.md`](../internal/git-flow-release.md),
[`../internal/v1-ga-acceptance.md`](../internal/v1-ga-acceptance.md), [`../internal/v1-physical-acceptance.md`](../internal/v1-physical-acceptance.md),
[`../internal/v1-repository-admin.md`](../internal/v1-repository-admin.md), [`../internal/release-candidate-checklist.md`](../internal/release-candidate-checklist.md),
[`../internal/development-roadmap.md`](../internal/development-roadmap.md), [`../internal/naming-conventions.md`](../internal/naming-conventions.md),
and the research/evaluation records ([`../internal/capture-spike.md`](../internal/capture-spike.md),
[`../internal/async-capture-spike.md`](../internal/async-capture-spike.md), [`../internal/neatvnc-evaluation.md`](../internal/neatvnc-evaluation.md),
[`../internal/x86-vnc-transport-evaluation.md`](../internal/x86-vnc-transport-evaluation.md), `internal/qpa-*-qt-6.8.3.md`),
[`../adr/`](../adr/), [`../releases/`](../releases/), [`../proposals/`](../proposals/).

Ordinary integration work does not require this zone.

## 5. Navigation root and writing conventions

This file is the **bilingual navigation root**; its Chinese counterpart is [`../README.md`](../README.md). The
mirrored zones are `docs/guide/**` and the product final-state contracts at the top level of `docs/`; everything else is the internal/release zone and
stays English.

- **Bilingual mirror**: the Chinese primary document lives at `docs/<path>` and the English mirror at
  `docs/en/<path>`. Files that **enter `guide/`** must correspond one-to-one (same relative path), carry a
  one-line language switch at the top, and change in both languages in the same change. Legacy documents that
  have not moved into that zone yet are not required to be mirrored while the migration runs. The product
  final-state contracts and the `internal/` zone are English-only and are not mirrored.
- **One document per intent**: user documentation is consolidated by reader intent, not split by development
  stage. Prefer one detailed document over five that each cover a fragment.
- **Source comments**: follow the style already in the file. New or substantially edited comments prioritize
  clarity and consistency with their surroundings; bilingual comments are allowed where they materially help
  maintainers, but they are **not mandatory** and there is no bulk comment-only migration.
- **Links**: when a document moves, update every reference in the repository. Keep a **content-free** migration
  pointer only while a release gate still lists the old path, and delete it in the same change that moves the
  gate to the new path.
- **Process content**: issue numbers and tracking, acceptance scheduling and status, milestone chronicles,
  investigation logs and one-off checklists do not belong to the user/reference zones.

Full rules: [`CONTRIBUTING.md`](../../CONTRIBUTING.md).
