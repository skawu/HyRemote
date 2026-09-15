# HyRemote

**Qt Embedded Remote Access Framework**

<p align="center">
  <img src="logo/huayan-software-horizontal.png" alt="HyRemote by Huayan Software" width="420">
</p>

HyRemote is an open-source remote access framework for Qt applications. It is designed to add remote display and remote input capabilities to existing Qt applications without forcing an application to adopt a single integration model, rendering stack, transport, or SoC-specific implementation.

> Status: **pre-alpha**. Product capabilities, compatibility claims, and release milestones are still being completed toward the first GA release.

## Product integration modes

HyRemote is productized through three integration modes:

1. **Embedded C++ API** — stable, explicit integration for maximum control and performance.
2. **Declarative QML API** — convenient integration for Qt Quick applications using the same core semantics.
3. **Transparent QPA Proxy** — optional low/zero-source-change compatibility path for existing Qt applications.

Qt Widgets and Qt Quick are first-class application types. The integration mode is a product capability; capture, transport, input, and hardware acceleration remain replaceable implementation layers.

## Architecture direction

```text
Qt Application
  ├─ Qt Widgets
  ├─ Qt Quick
  ├─ Quick3D
  ├─ QOpenGLWidget
  └─ QQuickWidget
         │
    Target Adapter
         │
    Capture Backend
         │
  ┌──────┼───────────┐
  │      │           │
Raster  GL/PBO   GBM/DMA-BUF
  │      │           │
  └──────┴─────┬─────┘
               │
          RemoteFrame
      pixels/damage/PTS
               │
         Transport Layer
               │
        VNC/RFB first
               │
            Viewer

Viewer Input → Transport → Input Adapter → Qt Event
```

The first VNC/RFB transport candidate is [NeatVNC](https://github.com/any1/neatvnc). HyRemote should reuse a mature protocol implementation instead of reimplementing RFB from scratch.

## Design principles

- **Existing applications first.** Adding remote access must not require rewriting application UI or business logic.
- **Three product integration modes.** C++ API, QML API, and optional QPA Proxy are distinct user-facing capabilities.
- **Qt Widgets and Qt Quick are peers.** Neither is treated as a compatibility afterthought.
- **Public API first.** The stable core should prefer Qt public APIs. Qt private/QPA integration is isolated behind optional adapters.
- **Backend separation.** Target, capture, transport, input, and hardware encoding are independent interfaces.
- **Portable baseline before hardware optimization.** Correctness comes before PBO, DMA-BUF, GBM, or hardware video encoding.
- **Hardware acceleration is pluggable.** SoC-specific acceleration may be added without becoming a generic-core dependency.
- **No protocol lock-in.** VNC/RFB is the first transport, not the permanent boundary of the project.

## Product roadmap and versioning

HyRemote uses four-part product versions:

```text
Major.Minor.Feature.Maintenance
```

The version number represents product capability and compatibility, not the internal technical work breakdown. The canonical rules are in [`docs/versioning.md`](docs/versioning.md).

### Linux x86_64 reference platform

Linux x86_64 is the standard/reference platform used to complete the first HyRemote product generation.

| Version | Product milestone |
| --- | --- |
| `V0.0.1.0` | Linux x86_64 — Embedded C++ API |
| `V0.0.2.0` | Linux x86_64 — Declarative QML API |
| `V0.0.3.0` | Linux x86_64 — Transparent QPA Proxy |
| `V1.0.0.0` | Linux x86_64 GA — all three integration modes productized |

### Embedded-platform expansion after V1.0

After `V1.0.0.0`, each newly formalized embedded platform family receives a new `V1.x.0.0` product line. Within that line, independently deliverable integration modes use `V1.x.y.0`.

For example:

```text
V1.x.0.0   platform family enters formal support
V1.x.1.0   Embedded C++ API
V1.x.2.0   Declarative QML API
V1.x.3.0   Transparent QPA Proxy
```

Platform ordering is a product-roadmap decision based on Linux/Qt ecosystem support, industrial relevance, BSP maturity, hardware availability, and customer demand. Rockchip and NXP i.MX are current high-priority candidates; other mainstream Linux embedded platforms can be added under the same model.

Core, capture, transport, RemoteFrame, DMA-BUF, hardware encoding, CI, and similar engineering work are WBS/tasks under these product milestones; they are not top-level product milestones themselves.

## Compatibility policy

A platform or integration mode is considered **supported** only when it has repeatable evidence for its claimed environment, including:

- a reproducible build;
- functional remote-view and, where applicable, remote-input validation;
- a compatibility entry;
- documented known limitations.

Desktop validation must not be used as a substitute for an embedded-platform support claim.

## Current non-goals

The early product will not:

- replace a general-purpose Linux remote desktop system;
- implement the RFB protocol from scratch when a suitable maintained library exists;
- require Wayland migration merely to enable remote maintenance;
- promise zero-copy or hardware H.264 before measured proof;
- make Qt private/QPA APIs part of the stable core contract.

## Contributing

Contribution and architecture rules are documented in [`CONTRIBUTING.md`](CONTRIBUTING.md) and `docs/`.

## License

HyRemote is licensed under the **Apache License 2.0**. See [`LICENSE`](LICENSE).

Third-party components remain subject to their own licenses and attribution requirements.

### Logo and brand assets

<p align="center">
  <img src="logo/huayan-logo-single.png" alt="Huayan brand mark" width="96">
</p>

| Asset | Project use |
| --- | --- |
| [`logo/huayan-software-horizontal.png`](logo/huayan-software-horizontal.png) | Primary repository/project lockup; rendered at the top of this README |
| [`logo/huayan-logo-single.png`](logo/huayan-logo-single.png) | Compact brand mark for square/icon placements and project documentation |

The image files are distributed with this repository under Apache-2.0. Apache License 2.0 section 6 does **not** grant trademark rights: copying/redistribution of the files does not grant permission to present a fork, derivative, or unrelated product as the owner of these marks. Downstream products should use their own branding unless separately authorized.
