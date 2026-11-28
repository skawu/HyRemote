> **Owner #563 (2026-10-22):** Windows x86_64 is the sole mandatory physically verified platform for Feature, V0.4 RC and first GA. Linux/embedded/ARM64/EGLFS/Wayland are Qt-based community/user-validation paths, **not release blockers** and not automatically GA-Supported. Confirmed reported defects are handled through issue-driven maintenance/hotfixes. This overrides older pre-V0.4 hardware gate wording below; see [policy](../internal/windows-first-platform-validation-policy-2026-10-22.md).

# HyRemote Versioning and Product Milestones

> Language / 语言: **English** | [中文](../versioning.md)

HyRemote uses a four-part product version:

```text
Major.Minor.Feature.Maintenance
```

The version describes externally meaningful product capability and compatibility. It does not encode internal architecture work, frontend names, Qt versions, platforms, or CI/WBS order.

## Version fields

### Major: product generation or incompatible boundary

Increase Major when crossing a materially incompatible product boundary, for example:

- an incompatible public API or ABI generation;
- a product architecture change that cannot preserve the previous contract;
- a Qt or framework transition that requires an incompatible application model.

Adding a SoC, OS, transport backend, or acceleration backend does not increase Major merely because the implementation changes.

### Minor: coherent product capability stage

Minor identifies a new product capability line that users can understand.

Post-GA Minor lines open only for activated coherent product outcomes. No technology theme receives a preassigned Minor or Feature version.

### Feature: bounded, independently deliverable capability

Feature is used for a bounded capability that can be delivered and verified inside a Major/Minor line.

Version digits do not encode:

- C++, QML, Generic, or QPA;
- Windows, Linux, or embedded Linux;
- Widgets or Qt Quick;
- Qt version;
- source directory or implementation layer.

Those are compatibility and capability dimensions, recorded separately.

### Maintenance: fixes and non-capability changes

Maintenance preserves the released capability boundary, for example:

- defect fixes;
- security fixes that do not change the public product contract;
- compatibility corrections inside an already supported range;
- performance tuning without a new product capability;
- packaging, documentation, or CI corrections.

Maintenance releases must not hide a new platform or major product capability.

## Pre-GA product lines

| Version | Name | User promise |
| --- | --- | --- |
| **V0.1.0.0** | Developer Preview | A developer can integrate HyRemote and establish an early, limited remote-access path |
| **V0.2.0.0** | First User Trial | A real user can use application-scoped remote view/input from a second LAN machine, with default-off access and explicit unencrypted limits |
| **V0.3.0.0** | Productize It / Product Preview | Four frontends are packaged, deployed, documented, and taught as product paths |
| **V0.4.0.0** | Qualify It / Release Candidate | Feature freeze, Qt/application compatibility, performance, physical qualification, and RC hardening |
| **V1.0.0.0** | Stabilize It / First GA | A formal stable support contract for the mature V0.4 lineage |

V0.x releases describe product maturity, not frontend-specific releases. The retired `V0.0.1.0 = C++`, `V0.0.2.0 = QML`, and `V0.0.3.0 = QPA` plan is no longer used.

## V0.1: Use It

V0.1 was narrow and usable:

- C++, QML, Generic, and QPA are peer integration technologies;
- Windows x86_64 and Linux x86_64 are the desktop reference platforms;
- Qt 6.8.3 is the current reference SDK;
- Widgets and Qt Quick share one Runtime;
- basic SDK, deployment, and onboarding are available;
- security is unauthenticated and unencrypted, and not ready for the public Internet.

## V0.2: First User Trial

V0.2.0.0 delivered a real second-machine trial of application-scoped remote view and input, listener lifecycle, and reconnect.

Accepted security and access facts:

- default listener `0.0.0.0:5921` on the host's IPv4 interfaces;
- default `Insecure` / RFB `SecurityType None`, with no authentication and no transport encryption;
- remote input disabled by default, enabled only through an explicit operator choice;
- optional RFB VNC password authentication when the build capability is enabled and configured, still without stream encryption;
- the unavailable stronger `AuthenticatedEncrypted` profile fails closed with no silent downgrade;
- trusted LAN only, not Internet-safe.

V0.2 security boundary: VNC authentication does not encrypt the transport or provide per-session authorization. Do not equate authentication with encryption or authorization. V0.2.0.1 is a maintenance repair on the same First User Trial lineage.

## V0.3: Productize It

V0.3 turns all four frontends into complete product-deliverable paths:

- C++ API;
- QML API;
- Generic Plugin;
- QPA.

The line also completes the deployment matrix, task-oriented self-service documentation and examples (#240), four-frontend SDK/deployment productization (#264), and minimum self-service diagnostics (#335).

V0.3.0.0 also shipped a basic RFB access compatibility entry in HyRemoteTool as a historical release fact. Under H-T01=B, the current Tool scope is an SDK development and diagnostics workbench; the released entry is maintained for compatibility only and is not expanded. See [`../internal/hyremote-tool-plan.md`](../internal/hyremote-tool-plan.md).

The V0.3 family carries two separate classes of work:

### Mandatory pre-GA V0.3 baseline

**Current product roadmap (November 28, 2026):** [sole SDK/Runtime version roadmap](../internal/product-version-delivery-roadmap-2026-10-23.md). The latest published release is V0.3.5.1; V0.3.5.2 is an unactivated maintenance plan; V0.3.6.0 is registered as the SDK development and diagnostics workbench. #581 implementation is complete, and #586 new-scope acceptance remains open; the old candidate remains HOLD. First GA is limited to qualified Qt 5.15/6.8 cells; Qt 6.5/6.12 follow GA. V0.4 is qualification only.

Only **Windows x86_64 Qt 5.15 and Qt 6.8 applicable supported cells** require physical/real-runtime pre-GA qualification, alongside RFB Raw/TRLE incremental damage, maintained Viewer interoperability, #370 performance, SDK installation and packaging. Linux/ARM64/qxcb/Wayland/EGLFS remain user-validation pending and non-blocking (#563). Qt6.5/6.12 are not pre-RC or first-GA conditions.

### Scope of this repository

This repository's version plan tracks only the independent open-source SDK/Runtime outcomes: integration, build/deploy, platform compatibility, transport, security, performance, resource lifecycle, and developer diagnostics. Product scope outside this repository is not enumerated here. Research without a user outcome and acceptance criteria does not receive a reserved version.

### Activated V0.3 family Feature

**V0.3.3.0: Delivery Efficiency & Viewer Interoperability** is the first activated Product breadth Feature after V0.3.0.0:

- practical damage, compression, and incremental delivery; profiling selected Raw + TRLE, and ZRLE is not currently required;
- the #370 performance baseline;
- maintained standard-viewer interoperability;
- **Windows x86_64 Qt 6.8.3** exact reference and two maintained external GUI viewers; Linux validation is optional/user-led, and every integration frontend reports its actual applicable cell.

`V0.3.1.0` (Qt5 slot) and `V0.3.2.0` (Bandwidth/ZRLE slot) are retired historical records and cannot be selected. V0.3.3.0 is a new coherent outcome. See [`../internal/v0.3.3.0-plan.md`](../internal/v0.3.3.0-plan.md).

First-GA Qt5.15/Qt6.8 compatibility was the **V0.3.4.0** outcome, released on 2026-10-29 (tag `v0.3.4.0` → `main` `27bf97d`). Tool improves incrementally; Host config/text-watermark and Docker are later independent planned outcomes. Qt6.5/Qt6.12 remain post-V1 GA; no additional Qt compatibility line is implied by this release plan. The canonical [product roadmap](../internal/product-version-delivery-roadmap-2026-10-23.md) assigns planned versions to known user outcomes; this summary does not independently assign versions.

## Current development Feature: V0.3.6.0 (November 28, 2026)

The latest published release is V0.3.5.1. V0.3.5.2 is an unactivated maintenance plan; V0.3.6.0 is registered under H-T01=B as the SDK development and diagnostics workbench. #581 implementation is complete, and #586 new-scope installation/acceptance remains open. The old candidate remains Draft/HOLD; no release authorization is implied.

- **V0.3.4.0: released** (2026-10-29). Post-release correction: the D05 record cited by its notes was added to the released tree and the tag was re-pointed to the corrected `main` HEAD before any announcement.
- **V0.3.5.0: released** (November 24, 2026), host configuration/state and static text watermark #591/#589.
- **V0.3.5.1: latest published release** (November 27, 2026), source distribution.
- **V0.3.5.2: planned maintenance**, not an activated machine train.
- **V0.3.6.0: registered SDK development and diagnostics workbench**; #581 implementation complete, #586 acceptance open.
- **V0.4.0.0: NOT ACTIVE RC**, strictly qualification of already implemented functionality. It cannot absorb missing V0.3 Qt LTS/SDK/Tool feature implementation.

This roadmap correction does not reopen retired V0.3.1/V0.3.2 slots, imply Qt6.5/6.12 pre-GA support, or authorize a release/tag before all mandatory acceptance and #95 release gates pass.

## Complete numbered planning for known product outcomes (summary; editable, 2026-10-26)

> **Source of truth:** This table is a summary, not a second source of truth. The latest merged version of the [canonical product roadmap](../internal/product-version-delivery-roadmap-2026-10-23.md) on `develop` controls planned outcomes, versions, ordering, dependencies, and status. Issues #594/#605 record Owner decisions; `.github/release/release-trains.json` controls machine train registration/mandatory scope; actual support requires release/tag, compatibility data, and exact-SHA evidence.


**Owner update:** Assign a concrete *planning version* to every known, testable product outcome so the local owner has an unambiguous development target. A planned number is **not** an activated machine release train, qualified feature, release branch or permission to tag. The [sole current product roadmap](../internal/product-version-delivery-roadmap-2026-10-23.md) owns dependencies, test criteria, owner Issues, exceptions and changes. Research without an admitted user deliverable stays a research obligation, not an invented feature.

| Version | Planned user-deliverable outcome | Status |
| --- | --- | --- |
| V0.3.4.0 | Windows Qt5.15/Qt6.8 real SDK and Runtime product compatibility #603 | **Released** (2026-10-29, tag `v0.3.4.0` → main `27bf97d`) |
| V0.3.5.0 | Host config/state paths and server-side static text watermark #591/#589 | **Released** (November 24, 2026) |
| V0.3.6.0 | HyRemoteTool SDK development and diagnostics workbench S1: static SDK/Qt Kit inspection, four-route guidance, read-only protocol/frame diagnostics, redacted report and local language #581–#586/#590 | Registered; #581 implementation complete, #586 new-scope acceptance open |
| V0.3.7.0 | Real user Qt app + HyRemote Docker deployment #592 | Conditional plan; actual container required |
| V0.4.0.0 | Qualification-only RC, **not development of missing features** | Inactive conditional RC |
| V1.0.0.0 | Windows first GA qualified Qt5.15/Qt6.8 support contract | Not released |
| V1.0.1.0 / V1.0.2.0 | Independently qualify Qt6.5 / Qt6.12 post-GA #605 | Planned, not activated |
| V1.0.3.0 / V1.0.4.0 | Tool S2 imported diagnostics #587 / S3 local SDK guidance #588 | Planned, not activated |
| V1.0.5.0 | Post-GA server-side Logo/image watermark #593 after the accepted text watermark; reuses the Shared Runtime remote-frame output path, with asset format and safety bounds designed separately | Conditional, not activated |

| V1.2.0.0 | Real Qt Quick/ARM64 HMI adoption without an accidental Qt Widgets dependency #149/#203/#7/#236 | Conditional; actual hardware evidence |
| V1.2.1.0 | Native Linux Wayland support and local GUI preservation #346/#148 | Conditional, platform evidence |
| V1.2.2.0 | True single- and multi-touch in Qt Quick / industrial HMI #172 | Conditional, viewer and hardware evidence |
| V1.2.4.0 | SDK/Runtime secure file-delivery GO/NO-GO evaluation #173; implementation depends on interoperability, authenticated/encrypted transport, application authorization, quotas and path-safety evaluation | Conditional, not activated |
| V1.2.6.0 | Appliance capture-on-demand and bounded idle resource use #151 | Conditional after device measurement |
| V1.2.7.0 | Operational performance diagnostics beyond baseline #180 | Conditional on real support needs |

**Unassigned SDK/Runtime capability candidate:** Authenticated Session Operations (#170) remains a Runtime capability to activate only when real user needs justify it. C++/QML share the same session identity, admission, events and per-session termination semantics; Generic/QPA reuse Runtime enforcement. It is not a HyRemoteTool session-management UI, has no assigned version, and does not block current releases.

**Change control:** any rearrangement needs a #594 Owner decision, a reviewed roadmap PR, old→new version mapping, reason, impact on dependencies/CI/support claims, and an auditable merge SHA. Do not create unapproved `release/*` or tags. The local owner follows the current roadmap and registered train; V0.4 remains qualification-only.

## V0.4: Qualify It

V0.4 is the Release Candidate line and performs qualification only. It is not selectable until:

- the V0.3.0.0 self-service foundation is complete;
- the activated V0.3 family pre-GA baseline (#343/#370) is complete;
- the GA support matrix is frozen.

Its activation authority is #235. Its lineage is the completed, activated V0.3 family outcome, never a preassigned technical slot. Large architecture changes and major new capabilities stop here.

Focus areas:

- Qt LTS compatibility qualification, including Qt 5.15 LTS;
- real-world application qualification;
- performance measurement and blocker-driven optimization;
- physical **Windows x86_64** local and remote coexistence; Linux/ARM64 device evidence remains optional/user-validation pending (#563);
- release package and support-statement hardening.

Maintenance iterations may use:

```text
V0.4.0.1
V0.4.0.2
...
```

until known GA blockers are removed.

## V1.0: First GA

V1.0 is the first formal stable support commitment.

It should not introduce a new Runtime architecture, frontend, protocol family, or large public API at the last moment. The mature V0.4 product is stabilized into a GA contract covering API, SDK, compatibility statements, packaging, documentation, and known limitations.

## Post-GA product outcomes

Post-GA work is activated as coherent user or product outcomes beyond the frozen GA baseline under separate authority issues. Version numbers are never preassigned to technology themes (#333).

The retired fixed slots `V1.1 = Embedded`, `V1.2 = Acceleration`, and `V1.3 = Differentiate` are not used, and no replacement slots are invented.

Post-GA changes may extend only the open-source SDK/Runtime contract after an explicit Owner decision, a user outcome, and exact qualification evidence. Do not reserve unapproved version slots; preserve one shared Runtime.

## Release authority

The selectable release trains, mandatory children, candidate prerequisites, evidence, and lineage are defined in `.github/release/release-trains.json`. A version number does not authorize a release by itself. Unrecorded versions, retired slots, and inactive V0.4 fail closed.

Internal engineering work belongs to the product line it supports and does not receive its own version number.
