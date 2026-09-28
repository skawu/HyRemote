# HyRemote Versioning and Product Milestones

HyRemote uses a four-part product version:

```text
Major.Minor.Feature.Maintenance
```

Version numbers describe externally meaningful product capability and compatibility. They do **not** encode internal architecture work, integration frontend names, Qt versions, platforms, or CI/WBS sequencing.

## Version fields

### Major — product generation / incompatible boundary

Increase `Major` when HyRemote crosses a materially incompatible product boundary, for example:

- an incompatible public API/ABI generation;
- a product architecture change that cannot preserve the previous contract;
- a Qt/framework transition that requires an incompatible application model.

Adding another SoC, operating system, transport backend, or acceleration backend does not automatically require a new Major when the public product contract remains compatible.

### Minor — coherent product capability stage

`Minor` identifies a substantial, coherent product stage that users can understand as a new product capability line.

Current post-GA strategy uses Minor lines for major product expansion stages such as embedded deployment, hardware acceleration, and advanced programmable integration.

### Feature — bounded independently deliverable capability

`Feature` is used for a bounded capability that can be delivered and verified inside a Major/Minor product line.

The digits do not encode:

- C++ / QML / Generic / QPA;
- Windows / Linux / Embedded Linux;
- Widgets / Qt Quick;
- Qt version;
- source directory or implementation layer.

Those are compatibility/capability dimensions recorded separately.

### Maintenance — fixes and non-capability changes

`Maintenance` is used for changes that preserve the released product capability boundary, such as:

- defect fixes;
- security fixes that do not redefine the public product contract;
- compatibility corrections inside an already supported range;
- performance tuning without a new product capability;
- packaging/documentation/CI corrections.

Maintenance releases must not be used to hide a newly supported platform or major product capability.

## Pre-GA product lines

HyRemote is delivered progressively before the first GA:

| Version | Name | User promise |
| --- | --- | --- |
| **V0.1.0.0** | **Use It / Developer Preview** | A developer can integrate HyRemote and get a working remote-access path through the primary C++ API or Generic Plugin routes |
| **V0.2.0.0** | **Trust It / Operational Preview** | Security, authenticated sessions, and production network behavior become usable product capabilities |
| **V0.3.0.0** | **Productize It / Product Preview** | All four integration frontends are packaged, deployed, documented, and taught as product paths |
| **V0.4.0.0** | **Qualify It / Release Candidate Line** | Feature freeze, Qt/application compatibility, performance, physical qualification, and release-candidate hardening |
| **V1.0.0.0** | **Stabilize It / First GA** | First formal stable support contract for the mature V0.4 lineage |

The V0.x lines are product-maturity releases, not frontend-specific releases.

The retired planning idea:

```text
V0.0.1.0 = C++
V0.0.2.0 = QML
V0.0.3.0 = QPA
```

is not part of the product version model and must not be revived.

## V0.1 — Use It

V0.1 is intentionally narrow and usable:

- C++ API and Generic Plugin are the primary product paths;
- Windows x86_64 and Linux x86_64 are the desktop reference platforms;
- Qt 6.8.3 is the current reference SDK;
- Widgets and Qt Quick share one Runtime;
- basic SDK/deployment/onboarding is available;
- security is unauthenticated and unencrypted and is not yet production-Internet ready;
- The four integration routes are peers; QML API and QPA are not secondary paths.

## V0.2 — Trust It

V0.2 adds the operational security/session layer:

- encrypted authenticated transport;
- certificate/key policy;
- authenticated session identity/registry;
- bounded admission;
- session events and termination;
- production network policy.

The goal is to move from “it works” to “it can be operated safely in the intended environment.”

## V0.3 — Productize It

V0.3 turns all four integration frontends into complete product-deliverable paths:

- C++ API;
- QML API;
- Generic Plugin;
- QPA.

This line also completes the deployment matrix, the task-oriented self-service documentation/example path (#240), the four-peer SDK/deploy productization (#264) and the minimum self-service diagnostics (#335). Further V0.3-family breadth (Qt 5.15 adaptation, compression/damage delivery, session operations, branding) is planned and executed under #343/#370 as coherent outcomes, without pre-reserving exact Feature version numbers for it (#333). behavior needed for practical delivery.

## V0.4 — Qualify It

V0.4 is the release-candidate line: **qualification only**. It is not selectable until the V0.3.0.0 self-service foundation is complete, the activated V0.3-family pre-GA baseline (#343/#370) is complete, and the GA support matrix is frozen; its activation authority is #235 and its lineage is the completed, activated V0.3-family outcome - never a pre-reserved technical slot (#333). Large architecture changes and major new capabilities stop here.

Focus areas include:

- Qt LTS compatibility qualification, including the planned Qt 5.15 LTS line;
- real-world application qualification;
- performance measurements and blocker-driven optimization;
- physical Windows/Linux local+remote coexistence;
- release package and support-statement hardening.

Maintenance iterations may use:

```text
V0.4.0.1
V0.4.0.2
...
```

until known GA blockers are removed.

## V1.0 — First GA

V1.0 is the first formal stable support commitment.

It should not introduce a new Runtime architecture, frontend, protocol family, or large public API at the last moment. The mature V0.4 product is stabilized into a GA contract covering API, SDK, compatibility statements, packaging, documentation, and known limitations.

## Post-GA product outcomes

Post-GA work is activated as coherent user/product outcomes under their own authority issues. Version numbers are never pre-reserved for a technical theme (#333): the former fixed slots "V1.1 = Embedded", "V1.2 = Acceleration" and "V1.3 = Differentiate" are retired and no replacement slots are invented. The capability areas below are the standing possibility space, entered only through an activated outcome:

- industrial/embedded deployment (ARM64 Embedded Linux, qualified SoC/BSP families, EGLFS/Wayland where applicable, cross compilation, dependency/feature slicing for Quick-only or Widgets-only products, embedded SDK/package manifests, physical target qualification) - with the architecture rule of **one logical Runtime** and physical dependency slicing where needed, never separate public `RuntimeQuick`/`RuntimeWidgets` products;
- measured hardware/performance work (DMA-BUF/GBM/external buffers, GPU-assisted conversion/scaling, RKMPP/VAAPI/D3D-class paths, bounded buffer/fence ownership, reduced CPU copies and latency) - introduced only where product measurements justify it;
- programmable C++/QML product value (advanced session policy, business admission decisions, dynamic target selection, privacy/exclusion policy, Runtime health/observability, application-state integration) - while Generic and QPA stay focused on reliable zero-code/startup configuration rather than becoming hidden business-logic APIs.

### Later transport/product expansion

RFB is the current transport baseline, not HyRemote's permanent product identity.

A future media-oriented transport should be introduced only if real workloads such as high-motion visualization, video, Quick3D, or map-heavy applications demonstrate a product ceiling that RFB cannot meet acceptably.

No future version number is allocated merely because a technology is interesting.

## Support and compatibility claims

A version can claim a platform/integration combination only when the product can state:

- exact or bounded Qt compatibility;
- operating system/architecture;
- integration frontend;
- application/UI scope;
- deployment model;
- security boundary;
- known limitations.

Windows results do not imply Linux results. Desktop results do not imply Embedded Linux. Public Qt API compatibility does not imply QPA private-ABI compatibility.

## Versioning principle

The product roadmap can be summarized as:

```text
V0.1  let users use it
V0.2  let users trust it
V0.3  make it easy to productize and deliver
V0.4  prove it is qualified
V1.0  make the stable support commitment
```

Internal engineering work belongs under the product line it enables; it does not get its own version number.

Post-GA exact Features are activated as coherent outcomes; no version number is pre-assigned to a technical theme (#333).
