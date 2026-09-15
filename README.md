# HyRemote

**Qt Embedded Remote Access Framework**

<p align="center">
  <img src="logo/huayan-software-horizontal.png" alt="Huayan Software" width="420">
</p>

HyRemote is an open-source remote access framework for Qt Embedded applications. It is designed to add remote display and remote input capabilities to existing Qt applications without forcing an application to adopt a single integration model, rendering stack, transport, or SoC-specific implementation.

> Status: **early architecture / pre-alpha**. APIs and project boundaries are not yet stable.

## Goals

HyRemote aims to support both **Qt Widgets** and **Qt Quick** as first-class application types, with three integration modes:

1. **Embedded C++ API** — stable, explicit integration for maximum control and performance.
2. **Declarative QML API** — convenient integration for Qt Quick applications.
3. **Zero-code platform proxy** — optional compatibility mode for existing applications that cannot be modified.

The project starts with Qt 6 on Embedded Linux and EGLFS/OpenGL. The architecture is intentionally not tied to VNC, EGLFS, RK3588, or any single capture/encoding backend.

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
- **Multiple integration modes.** Developers choose the integration style that fits their application.
- **Qt Widgets and Qt Quick are peers.** Neither is treated as a compatibility afterthought.
- **Public API first.** The stable core should prefer Qt public APIs. Qt private/QPA integration is isolated behind optional adapters.
- **Backend separation.** Target, capture, transport, input, and hardware encoding are independent interfaces.
- **Portable baseline before hardware optimization.** Correctness comes before PBO, DMA-BUF, GBM, or hardware video encoding.
- **Hardware acceleration is pluggable.** RK3588/RKMPP may become the first optimized backend, but not a core dependency.
- **No protocol lock-in.** VNC/RFB is the first transport, not the permanent boundary of the project.

## Initial roadmap

### v0.1 — Architecture and functional baseline

- project governance and public API boundaries
- Qt Widgets target adapter
- Qt Quick target adapter
- portable/raster capture baseline
- OpenGL capture baseline
- VNC/RFB transport integration
- remote pointer and keyboard input into Qt
- Widgets and Qt Quick examples
- compatibility and performance test matrix

### v0.2 — Performance

- asynchronous OpenGL/PBO readback
- damage-region propagation
- frame pacing/backpressure
- Quick3D, QOpenGLWidget, and QQuickWidget validation

### v0.3 — Hardware acceleration

- GBM/DMA-BUF experiments
- hardware encoder abstraction
- RK3588/RKMPP H.264 backend candidate
- end-to-end low-copy path where technically viable

### Later

- zero-code QPA/platform proxy
- additional Linux graphics backends
- additional transports such as WebRTC or custom low-latency video transports
- additional SoC hardware encoder backends

## Current non-goals

The early project will not:

- replace a general-purpose Linux remote desktop system;
- implement the RFB protocol from scratch when a suitable maintained library exists;
- require Wayland migration merely to enable remote maintenance;
- promise zero-copy or hardware H.264 before an architecture spike proves the buffer path;
- make Qt private/QPA APIs part of the stable core contract.

## Supported environments

The initial reference environment is:

- Qt 6.8.x Open Source
- Embedded Linux
- EGLFS
- OpenGL ES
- Qt Widgets and Qt Quick

Other environments will be added only after they have repeatable tests and a documented compatibility level.

## Contributing

HyRemote is being structured as an independent open-source project. Contribution and architecture rules will live in [`CONTRIBUTING.md`](CONTRIBUTING.md) and `docs/` as the baseline is established.

## License

**TBD before public release.** The repository is currently in project-bootstrap stage and a permissive open-source license will be selected explicitly before the first public release.

### Logo and brand assets

| Asset | Intended use |
|---|---|
| [`logo/huayan-software-horizontal.png`](logo/huayan-software-horizontal.png) | horizontal lockup, used at the top of this README |
| [`logo/huayan-logo-single.png`](logo/huayan-logo-single.png) | vertical mark for square/icon placements |

These assets are provided by the project maintainer and are **intended to be distributed under the same license as the rest of this repository**, to be applied once that license is selected.

Because the repository license is still undecided, **no license is granted for these assets in the meantime: they remain all rights reserved**. Downstream forks and derivatives should ship their own branding unless they have permission to keep these marks.
