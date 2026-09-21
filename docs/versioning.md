# HyRemote Versioning and Product Milestones

This document is the canonical product versioning policy for HyRemote.

HyRemote uses a four-part version number:

```text
Major.Minor.Feature.Maintenance
```

The version number describes product capability and compatibility. It must not be used as a proxy for internal technical work such as Core refactors, transport implementation, DMA-BUF work, CI, or individual architecture tasks.

## Version fields

### Major — product generation / compatibility boundary

Increase `Major` only when HyRemote enters a new product generation or crosses a materially incompatible compatibility boundary.

Typical reasons include:

- a major framework architecture change that breaks the existing product contract;
- a broad incompatible API/ABI transition;
- support for a substantially higher or lower Qt generation when doing so requires an incompatible product/framework model;
- a new integration level that cannot remain compatible with the existing generation.

Adding another SoC, operating system, or embedded platform family does **not** by itself increase `Major` when the existing product contract can remain compatible.

### Minor — complete product capability stage

`Minor` identifies a clear, complete product capability stage.

After `V1.0.0.0`, each newly formalized embedded platform family receives a new `V1.x.0.0` product line.

Examples:

- `V1.1.0.0` — first embedded platform family support line;
- `V1.2.0.0` — second embedded platform family support line.

The concrete platform ordering is a product-roadmap decision and must be based on supported-OS and Qt ecosystem maturity, industrial relevance, BSP maturity, hardware availability, and customer demand.

### Feature — independently deliverable product capability increment

`Feature` carries a product capability that can be independently delivered and independently verified inside a Minor line.

Integration technologies are **not** Feature increments. The version digits do not encode:

- C++ / QML / Generic / QPA;
- platform;
- Qt version;
- UI family.

`V0.1` and the old `V0.0.1.0 = C++`, `V0.0.2.0 = QML`, `V0.0.3.0 = QPA` mapping was planning history and is not revived.
Integration technologies and UI families are **compatibility dimensions** recorded by the capability/compatibility
matrix, while the version identifies one coherent product state.

### Maintenance — defect fixes and small-scope optimization

`Maintenance` is used only for maintenance releases that do not add a new product capability.

Examples include:

- crash fixes;
- compatibility corrections within an already supported range;
- performance tuning that does not create a new product capability;
- packaging/documentation corrections;
- security fixes that preserve the product contract.

A maintenance release must not be used to introduce a newly supported platform or integration mode.

## Pre-GA x86 reference-platform milestones

The x86_64 reference/standard platform includes **both Windows x86_64 and Linux x86_64**. The first HyRemote product generation is not considered complete on x86 if only one of these operating systems is supported.

Product delivery before first GA is **progressive** (#24, #95). Each train is a real coherent release with a
deliberately narrower support and qualification claim than GA:

| Train | User promise |
| --- | --- |
| `V0.1.0.0` | Use It / Developer Preview — loopback-only, primary C++/Generic paths, minimum deploy/examples |
| `V0.2.0.0` | Trust It / Operational Preview — security, session and network |
| `V0.3.0.0` | Productize It / Product Preview — four product-deliverable integrations, deployment, examples |
| `V0.4.0.0` | Qualify It / Release Candidate Line — feature freeze, qualification and evidence; `V0.4.0.x` maintenance |
| `V1.0.0.0` | Stabilize It / first GA — one mature V0.4 lineage promoted with an explicit support commitment |

The development sentinel `0.0.0` may integrate all in-flight modes concurrently. A formal release version no longer
selects which frontends may exist: the CMake release-profile gate rejects only the retired `V0.0.x` planning labels, and
frontend enablement, support level and preview/qualified/supported status are release-train scope and compatibility
authority. Whether a train may be released is decided by #1, #24 and #95 with that train's own readiness scope, not by
the version digits.

Each pre-GA milestone must be independently validated on both Windows x86_64 and Linux x86_64. Passing on one operating system is useful engineering evidence, but it does not complete the product milestone for the other operating system.

`V1.0.0.0` means the first HyRemote product generation is complete on the x86 reference platform. It is not tied to completion of any particular internal technology such as PBO, DMA-BUF, hardware encoding, or a specific transport optimization.

## Embedded-platform expansion after V1.0

After `V1.0.0.0`, HyRemote expands platform coverage incrementally.

Each formally supported embedded platform family starts a new Minor line:

```text
V1.x.0.0   Platform family enters formal support
V1.x.1.0   Embedded C++ API
V1.x.2.0   Declarative QML API
V1.x.3.0   Transparent QPA Proxy
```

The near-term embedded operating-system baseline is **Embedded Linux**. Rockchip and NXP i.MX are current high-priority platform candidates because they are relevant to industrial Linux/Qt deployments, but the final ordering of platform Minor lines remains a product-roadmap decision until separately frozen.

Other mainstream Linux embedded platforms may be added using the same model when justified by ecosystem maturity and product demand.

### OpenHarmony — long-term embedded operating-system expansion

OpenHarmony is a **long-term HyRemote platform/OS direction**, not a current delivery gate. It must not delay the x86 GA or the initial Embedded Linux platform expansion.

The concrete OpenHarmony version mapping is intentionally **not frozen yet**. It will be assigned only after the Qt/OpenHarmony integration boundary, graphics/input stack, toolchain/BSP maturity, and compatibility with the existing HyRemote Core and three integration modes have been validated.

When OpenHarmony work is scheduled:

- if it can be added while preserving the existing HyRemote product contract, it remains within the current Major generation and receives an appropriate later product version;
- if supporting it requires a materially incompatible framework model, API/ABI boundary, or Qt compatibility change, the Major-version rule above applies.

This keeps OpenHarmony visible in the long-term roadmap without forcing premature version numbering or blocking the Embedded Linux roadmap.

## Milestone rule

Top-level product milestones are defined by externally meaningful product capability, not by internal technical route.

The following are examples of **WBS/tasks**, not product milestones by themselves:

- Core / Session implementation;
- RemoteFrame and storage ownership;
- capture backends;
- NeatVNC or another transport backend;
- input routing;
- GL/PBO optimization;
- DRM/GBM/DMA-BUF;
- RKMPP or other hardware encoders;
- CI/build-system work;
- security hardening;
- ADS/governance migration.

These tasks belong under the product milestone they enable.

## Compatibility and release claims

A platform/integration-mode version can be declared supported only when the capability has repeatable evidence for its claimed environment, including the relevant operating system, build, functional behavior, compatibility statement, and known limitations.

For the x86 reference platform, Windows evidence does not substitute for Linux evidence and Linux evidence does not substitute for Windows evidence.

Desktop/x86 evidence must not be used as a substitute for an embedded-platform support claim, and one embedded SoC/BSP must not be generalized to an entire platform family without documented evidence.
