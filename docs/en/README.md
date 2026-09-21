# HyRemote documentation

> Language / 语言: **English** | [中文](../README.md)

HyRemote is a remote-access framework for existing Qt applications. The product provides **one Shared Runtime** and four peer integration frontends: C++ API, QML API, Generic Plugin, and QPA. Qt Widgets and Qt Quick are Runtime target types rather than separate products.

This area provides product integration, deployment, security, compatibility, and capability-boundary guidance.

## Start here

| Your scenario | Recommended entry | Current status |
| --- | --- | --- |
| Add a small amount of C++ and control the remote-access lifecycle explicitly | [`getting-started/cpp.md`](getting-started/cpp.md) | **V0.1 primary** |
| Keep the application Qt-only and preserve its native Qt platform | [`getting-started/generic.md`](getting-started/generic.md) | **V0.1 primary** |
| Prefer declarative configuration in a Qt Quick application | [`getting-started/qml.md`](getting-started/qml.md) | **Preview** |
| Need the specialized `-platform hyremote` zero-code route | [`getting-started/qpa-proxy.md`](getting-started/qpa-proxy.md) | **Preview** |
| Install, build, and acquire the SDK | [`guide/install.md`](guide/install.md) · [中文](../guide/install.md) | Product guide |
| Package and deploy | [`guide/deployment.md`](guide/deployment.md) · [中文](../guide/deployment.md) | Product guide |
| Connect a viewer, enable control, and reconnect | [`guide/viewer-connection.md`](guide/viewer-connection.md) | Product guide |
| Security model and deployment boundary | [`security.md`](security.md) · [中文](../security.md) | Product guide |
| Troubleshoot | [`guide/troubleshooting.md`](guide/troubleshooting.md) | Product guide |

## Product overview

Start with [`../product-overview.md`](../product-overview.md) for product positioning, the four integration frontends, current product capabilities, and long-term direction.

The V0.1 Developer Preview focuses on **getting users to a working remote-access path quickly**:

- C++ API and Generic Plugin are the primary integration paths;
- Windows x86_64 and Linux x86_64 with Qt 6.8.3 are the current reference environments;
- Widgets and Qt Quick use the same Shared Runtime;
- loopback is the default bind and remote input is disabled by default;
- QML API and QPA exist as Preview paths;
- incomplete capabilities are marked **TODO**.

## Product reference

| Document | Purpose |
| --- | --- |
| [`../product-overview.md`](../product-overview.md) | Product positioning, integration frontends, capability boundaries, roadmap |
| [`../architecture.md`](../architecture.md) | Core, Shared Runtime, and four integration frontends |
| [`../compatibility.md`](../compatibility.md) | Current platform, Qt, and integration compatibility matrix |
| [`security.md`](security.md) | Current security behavior, defaults, and unsupported security capabilities |
| [`../known-limitations.md`](../known-limitations.md) | Known product limitations |
| [`../versioning.md`](../versioning.md) | Product version semantics and V0.1 → V1.x evolution |
| [`../v1-api-stability.md`](../v1-api-stability.md) | Public API stability boundary |
| [`../widgets-capture.md`](../widgets-capture.md) | Widgets capture model |
| [`../quick-capture.md`](../quick-capture.md) | Qt Quick capture model |
| [`../input-model.md`](../input-model.md) | Remote-input model |
| [`../dependency-policy.md`](../dependency-policy.md) | Product dependency policy |
| [`../release-package-manifest.md`](../release-package-manifest.md) | SDK/runtime payload definition |

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

## Product status labels

- **Primary** — the recommended path for the current product line;
- **Preview** — implemented and usable, but not yet promoted to the current line's full supported status;
- **TODO** — planned capability that is not yet an availability or compatibility commitment.

Security, compatibility, and platform claims are limited to the explicitly stated matrix; similar environments are not assumed to be supported.

## Maintainer material

Architecture decisions, repository governance, release procedures, research records, and acceptance material live under `docs/adr/`, `docs/internal/`, `docs/releases/`, and `docs/proposals/`. Ordinary application integration does not require those files.
