# HyRemote documentation

> Language / 语言: **English** | [中文](../README.md)

HyRemote is a remote-access framework for existing Qt applications. The product provides **one Shared Runtime** and four peer integration frontends: C++ API, QML API, Generic Plugin, and QPA. Qt Widgets and Qt Quick are Runtime target types rather than separate products.

This area provides product integration, deployment, security, compatibility, and capability-boundary guidance.

## Start here

| Your scenario | Recommended entry | Current status |
| --- | --- | --- |
| Add a small amount of C++ and control the remote-access lifecycle explicitly | [`getting-started/cpp.md`](getting-started/cpp.md) | peer route |
| Keep the application Qt-only and preserve its native Qt platform | [`getting-started/generic.md`](getting-started/generic.md) | peer route |
| Prefer declarative configuration in a Qt Quick application | [`getting-started/qml.md`](getting-started/qml.md) | peer route |
| Need the specialized `-platform hyremote` zero-code route | [`getting-started/qpa-proxy.md`](getting-started/qpa-proxy.md) | peer route |
| Install, build, and acquire the SDK | [`guide/install.md`](guide/install.md) · [中文](../guide/install.md) | Product guide |
| Package and deploy | [`guide/deployment.md`](guide/deployment.md) · [中文](../guide/deployment.md) | Product guide |
| Connect a viewer, enable control, and reconnect | [`guide/viewer-connection.md`](guide/viewer-connection.md) | Product guide |
| Security model and deployment boundary | [`security.md`](security.md) · [中文](../security.md) | Product guide |
| Troubleshoot | [`guide/troubleshooting.md`](guide/troubleshooting.md) | Product guide |

## Product overview

Start with [`product-overview.md`](product-overview.md) ｜ [中文](../product-overview.md) for positioning and integration. **The single current product-outcome roadmap** is [`../internal/product-version-delivery-roadmap-2026-10-23.md`](../internal/product-version-delivery-roadmap-2026-10-23.md) (Owner #594/#605); [`versioning.md`](versioning.md) defines digit semantics and `.github/release/release-trains.json` alone authorizes exact activated Feature scopes. First GA targets qualified Windows Qt 5.15/6.8 cells only; Qt 6.5/6.12 are post-GA.

The following describes the historical **V0.3.0.0 Product Preview** baseline; **the public `v0.3.3.0` tag/Release created on 2026-10-23 was RETRACTED** due to release-process and same-SHA evidence failures and is **not an accepted product release**. The candidate included Raw/TRLE and Windows maintained-viewer evidence, but must be requalified on one exact candidate. Its exact publication record is [`../releases/v0.3.3.0.md`](../releases/v0.3.3.0.md):

- C++ / QML / Generic / QPA are peer product integration routes with different mechanics, not different product rank;
- **Historical published V0.3.0.0 matrix:** Qt 6.8.3 on Windows **and Linux** x86_64 were release-time reference/support claims (see `compatibility.md`); do not rewrite those historical facts. **For all new Feature/RC/GA qualification:** Windows x86_64 is the only required physical evidence platform, Linux/ARM64 are user-validation pending and must not be claimed newly Supported without fresh evidence (#563);
- Widgets and Qt Quick use the same Shared Runtime;
- the default listener is `0.0.0.0:5921` and remote input is disabled by default;
- route-specific compatibility limits are documented explicitly rather than expressed as Primary/Preview status;
- unshipped capabilities remain identified as limitations and must not be presented as available.

## Product reference

| Document | Purpose |
| --- | --- |
| [`product-overview.md`](product-overview.md) ｜ [中文](../product-overview.md) | Product positioning, integration frontends, capability boundaries, roadmap |
| [`architecture.md`](architecture.md) ｜ [中文](../architecture.md) | Core, Shared Runtime, and four integration frontends |
| [`performance-optimization.md`](performance-optimization.md) ｜ [中文](../performance-optimization.md) | Interaction-latency SLO, freshness-first delivery, adaptive capture/transport, and regression gates |
| [`compatibility.md`](compatibility.md) ｜ [中文](../compatibility.md) | Current platform, Qt, and integration compatibility matrix |
| [`security.md`](security.md) ｜ [中文](../security.md) | Current security behavior, defaults, and unsupported security capabilities |
| [`security-model.md`](security-model.md) ｜ [中文](../security-model.md) | Trust boundaries, threat model, and later security requirements |
| [`known-limitations.md`](known-limitations.md) ｜ [中文](../known-limitations.md) | Known product limitations |
| [`versioning.md`](versioning.md) ｜ [中文](../versioning.md) | Version-digit semantics and exact release admission (not a competing feature roadmap) |
| [`../internal/product-version-delivery-roadmap-2026-10-23.md`](../internal/product-version-delivery-roadmap-2026-10-23.md) | **Sole current feature roadmap**: sequencing, Qt LTS, Tool, watermark, Docker, post-GA |
| [`v1-api-stability.md`](v1-api-stability.md) ｜ [中文](../v1-api-stability.md) | Public API stability boundary |
| [`widgets-capture.md`](widgets-capture.md) ｜ [中文](../widgets-capture.md) | Widgets capture model |
| [`quick-capture.md`](quick-capture.md) ｜ [中文](../quick-capture.md) | Qt Quick capture model |
| [`input-model.md`](input-model.md) ｜ [中文](../input-model.md) | Remote-input model |
| [`qml-consumption.md`](qml-consumption.md) ｜ [中文](../qml-consumption.md) | QML API consumption, deployment, and lifecycle |
| [`dependency-policy.md`](dependency-policy.md) ｜ [中文](../dependency-policy.md) | Product dependency policy |
| [`release-package-manifest.md`](release-package-manifest.md) ｜ [中文](../release-package-manifest.md) | SDK/runtime payload definition |
| [`sdk-consumption.md`](sdk-consumption.md) ｜ [中文](../sdk-consumption.md) | SDK acquisition, integration, and deployment contract |

## How the four frontends relate

```text
C++ API ---------\
QML API ----------\
Generic Plugin ----> Shared Runtime -> Core
QPA --------------/
```

The four frontends are peers:

- **C++ API**: application links `HyRemote::RemoteAccess`;
- **QML API**: `import HyRemote`, a thin declarative frontend over the same Runtime;
- **Generic Plugin**: public Qt plugin route that keeps the native QPA/platform identity;
- **QPA**: private-ABI Factory Trampoline that delegates to the native platform integration.

No frontend creates a second Session, capture, input, or transport architecture.

## Capability boundaries

The four integration routes are **peers**; each has its own stated conditions (QPA needs an **exact** Qt private-ABI
match). Capability that does not exist is stated as such rather than parked behind a placeholder label.

Security, compatibility, and platform claims are limited to the explicitly stated matrix; similar environments are not assumed to be supported.

## Maintainer material

Architecture decisions, repository governance, release procedures, research records, and acceptance material live under `docs/adr/`, `docs/internal/`, `docs/releases/`, and `docs/proposals/`. Ordinary application integration does not require those files.\n
## Documentation style

See [STYLE.md](STYLE.md) ｜ [中文](../STYLE.md). The checker usage is in section 5.
