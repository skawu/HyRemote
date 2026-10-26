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

Native TLS/VeNCrypt, certificate lifecycle, authenticated Session Registry, roles, admission, and per-session termination are not V0.2 capabilities. V0.2.0.1 is a maintenance repair on the same First User Trial lineage, not a security or session Feature.

## V0.3: Productize It

V0.3 turns all four frontends into complete product-deliverable paths:

- C++ API;
- QML API;
- Generic Plugin;
- QPA.

The line also completes the deployment matrix, task-oriented self-service documentation and examples (#240), four-frontend SDK/deployment productization (#264), and minimum self-service diagnostics (#335).

V0.3.0.0 additionally ships **HyRemoteTool**, an out-of-process access-plane companion client. It connects to a remote HyRemote-enabled application by explicit IP and port and provides viewing, view-only or control input, FPS/throughput telemetry, and connection diagnostics. Management-plane capabilities are outside V0.3.0.0. See [`../internal/hyremote-tool-plan.md`](../internal/hyremote-tool-plan.md).

The V0.3 family carries two separate classes of work:

### Mandatory pre-GA V0.3 baseline

**Current Owner product roadmap (#594/#605, 2026-10-23):** [complete product/version outcome roadmap](../internal/product-version-delivery-roadmap-2026-10-23.md). The **only mandatory first-GA Qt LTS families are Qt 5.15 and Qt 6.8** on actually qualified Windows cells. **Qt 6.5 and Qt 6.12 are postponed until AFTER V1.0 GA**. Active V0.3.3.0 #144/#175 stays frozen; the next Feature prioritizes Qt5.15/Qt6.8 (#603/#604), and HyRemoteTool evolves incrementally across releases. V0.4 is qualification-only; V1 promotes accepted RC. No future exact Feature version is reserved; activation/mandatory children come only from `.github/release/release-trains.json` after reviewed admission. Historical releases are unchanged.

Only **Windows x86_64 Qt 5.15 and Qt 6.8 applicable supported cells** require physical/real-runtime pre-GA qualification, alongside RFB Raw/TRLE incremental damage, maintained Viewer interoperability, #370 performance, SDK installation and packaging. Linux/ARM64/qxcb/Wayland/EGLFS remain user-validation pending and non-blocking (#563). Qt6.5/6.12 are not pre-RC or first-GA conditions.

### Optional, quality, or differentiating outcomes

Session Registry and Session Operations, full branding expansion, HyRemoteTool management-plane enhancements, native TLS, and additional BSP/SoC/platform expansion beyond the declared GA reference cells are activated separately as coherent product outcomes. The V0.3.0.0 access-plane companion baseline is part of that release, not an optional enhancement.

### Activated V0.3 family Feature

**V0.3.3.0: Delivery Efficiency & Viewer Interoperability** is the first activated Product breadth Feature after V0.3.0.0:

- practical damage, compression, and incremental delivery; profiling selected Raw + TRLE, and ZRLE is not currently required;
- the #370 performance baseline;
- maintained standard-viewer interoperability;
- **Windows x86_64 Qt 6.8.3** exact reference and two maintained external GUI viewers; Linux validation is optional/user-led, and every integration frontend reports its actual applicable cell.

`V0.3.1.0` (Qt5 slot) and `V0.3.2.0` (Bandwidth/ZRLE slot) are retired historical records and cannot be selected. V0.3.3.0 is a new coherent outcome. See [`../internal/v0.3.3.0-plan.md`](../internal/v0.3.3.0-plan.md).

First-GA Qt5.15/Qt6.8 compatibility is the next high-priority V0.3.x outcome; Tool improves incrementally, and Host config/text-watermark and Docker are later independent candidates. Qt6.5/Qt6.12 are post-V1 GA; Management/TLS remains separately authorized. Other future Feature numbers are not reserved; V0.3.4.0 is now explicitly assigned as the next development Feature. Current [product roadmap](../internal/product-version-delivery-roadmap-2026-10-23.md) is authoritative.

## Assigned next development Feature: V0.3.4.0 (2026-10-26)

**Local Owner development target: V0.3.4.0, not V0.4.0.0.** The next coherent user outcome is genuinely tested **Windows x86_64 Qt 5.15.2 + Qt 6.8.3** SDK/Runtime compatibility: selected exact Qt Kit/compiler, real source and installed consumers, application UI/input, applicable C++/QML/Generic/QPA and Widgets/Quick cells, exact-private-ABI QPA and maintained external Viewer. Product owner issue #603; implementation #265/#566; per-cell pre-GA evidence #57; architecture guard #209. See the [local development handoff](../internal/v0.3.4.0-windows-qt-lts-development-charter.md).

- **V0.3.4.0: current exact development target**, with candidate release only after machine-mandatory issues, exact-SHA Windows QA and independent current-head review.
- **V0.3.5.0: next *non-activated* candidate**, host config/state paths and server-side static text watermark #591/#589; requires separate admission.
- **V0.4.0.0: NOT ACTIVE RC**, strictly qualification of already implemented functionality. It cannot absorb missing V0.3 Qt LTS/SDK/Tool feature implementation.

This roadmap correction does not reopen retired V0.3.1/V0.3.2 slots, imply Qt6.5/6.12 pre-GA support, or authorize a release/tag before the reviewed release-train change is merged.

## Complete numbered planning for known product outcomes (editable, 2026-10-26)

**Owner update:** Assign a concrete *planning version* to every known, testable product outcome so the local owner has an unambiguous development target. A planned number is **not** an activated machine release train, qualified feature, release branch or permission to tag. The [sole current product roadmap](../internal/product-version-delivery-roadmap-2026-10-23.md) owns dependencies, test criteria, owner Issues, exceptions and changes. Research without an admitted user deliverable stays a research obligation, not an invented feature.

| Version | Planned user-deliverable outcome | Status |
| --- | --- | --- |
| V0.3.4.0 | Windows Qt5.15/Qt6.8 real SDK and Runtime product compatibility #603 | Current development target; exact machine activation PR #655 |
| V0.3.5.0 | Host config/state paths and server-side static text watermark #591/#589 | Planned, not activated |
| V0.3.6.0 | HyRemoteTool S1 workspace, safe control, profiles, view, basic diagnostics, locale #581–#586/#590 | Planned, not activated |
| V0.3.7.0 | Real user Qt app + HyRemote Docker deployment #592 | Conditional plan; actual container required |
| V0.4.0.0 | Qualification-only RC, **not development of missing features** | Inactive conditional RC |
| V1.0.0.0 | Windows first GA qualified Qt5.15/Qt6.8 support contract | Not released |
| V1.0.1.0 / V1.0.2.0 | Independently qualify Qt6.5 / Qt6.12 post-GA #605 | Planned, not activated |
| V1.0.3.0 / V1.0.4.0 | Tool S2 imported diagnostics #587 / S3 local SDK guidance #588 | Planned, not activated |
| V1.0.5.0 / V1.0.6.0 / V1.0.7.0 | Logo #593 / no-install viewer #204 / permissioned text clipboard #171 | Post-GA; latter two conditional |
| V1.1.0.0 / V1.1.1.0 | Secure Internet Gateway #190 / authenticated Session Operations #170 | Strategic conditional plan |

| V1.2.0.0 | Real Qt Quick/ARM64 HMI adoption without an accidental Qt Widgets dependency #149/#203/#7/#236 | Conditional; actual hardware evidence |
| V1.2.1.0 | Native Linux Wayland support and local GUI preservation #346/#148 | Conditional, platform evidence |
| V1.2.2.0 | True single- and multi-touch in Qt Quick / industrial HMI #172 | Conditional, viewer and hardware evidence |
| V1.2.3.0 | Application-owned admission, control rights, and privacy policy #225 | Strategic conditional, after #170 |
| V1.2.4.0 | Secure file exchange or a documented external secure alternative #173 | Discovery gate / GO–NO-GO |
| V1.2.5.0 | Official viewer enhanced server capability/diagnostic/secure-channel contract #246 | Discovery gate / GO–NO-GO |
| V1.2.6.0 | Appliance capture-on-demand and bounded idle resource use #151 | Conditional after device measurement |
| V1.2.7.0 | Operational performance diagnostics beyond baseline #180 | Conditional on real support needs |

**Change control:** any rearrangement needs a #594 Owner decision, a reviewed roadmap PR, old→new version mapping, reason, impact on dependencies/CI/support claims, and an auditable merge SHA. Do not create unapproved `release/*` or tags. The local owner must implement **V0.3.4.0 now**, not V0.4.0.0.

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

Platform or BSP expansion beyond the declared GA reference cells, evidence-driven acceleration where still useful, and advanced programmable capabilities are admitted separately. They must preserve one logical Runtime and must not create separate public `RuntimeQuick` or `RuntimeWidgets` products.

## Release authority

The selectable release trains, mandatory children, candidate prerequisites, evidence, and lineage are defined in `.github/release/release-trains.json`. A version number does not authorize a release by itself. Unrecorded versions, retired slots, and inactive V0.4 fail closed.

Internal engineering work belongs to the product line it supports and does not receive its own version number.
