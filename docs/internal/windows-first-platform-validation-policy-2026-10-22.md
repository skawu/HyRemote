# Windows-first platform validation and user-driven port hotfixes (2026-10-22)

> **PRODUCT OWNER DECISION #563.** This platform evidence policy supersedes #558 only where #558 treated Linux x86_64, ARM64/EGLFS, Wayland or a non-Windows physical reference as a blocker for V0.3.3.0/V0.4/V1.0. It does **not** retroactively change historical release artifacts, undo Windows acceptance or create a blanket Linux support promise. Future version numbers are still not reserved.

## 1. Truthful product/support tiers

| Classification | Meaning | Release decision |
| --- | --- | --- |
| Windows-qualified | The exact Windows x86_64 Qt/version/route/UI/profile cell has current, executable exact-SHA evidence for install, runtime, viewer, correct input/security, performance and release readiness | Required before declaring that cell Supported or accepting the candidate |
| Community-validation pending | Linux desktop, Wayland, EGLFS or ARM64 code/build path expected to benefit from Qt portability, but no maintained exact runtime/device evidence | No physical/Linux viewer/board requirement for any active V0.3.3, RC or GA gate; not Supported/Qualified |
| Experimental / known-limited | A user/CI reproduced partial behavior or relevant platform-specific limitation | Publish the limitation; target-specific fix through Issue/hotfix when reproducible and feasible; no general support claim |
| Unsupported / Not Applicable | Actual failure or architecture/delegate impossibility for an exact QPA/Qt platform cell | Must not be described as working, even though that platform does not block Windows releases |

Qt portability is a design expectation, **not** automatic proof of binary compatibility, QPA private ABI, dynamic loader closure, input/cursor delivery, graphics capture or safety. Existing optional Linux CI may continue when cheap and available, but no missing host, board, user viewer or Linux workflow can turn a Windows release gate red *solely because evidence is unavailable*. `NOT_RUN_NON_BLOCKING` is **not** `PASS`. A credible, reproduced Windows regression or shipped security/correctness violation still blocks the applicable claimed cell; genuine user reports must never be deleted to manufacture green.

## 2. Release and feature boundaries

- **V0.3.3.0:** Qt 6.8.3 Windows x86_64 target and viewer environment is the required physical admission matrix. #144 Raw + TRLE incremental/damage and #175 real maintained GUI viewer path require correct Windows input/cursor/resize/auth where applicable, #370 bounded real-viewer performance, current SHA and same-SHA L2. Linux GUI viewers/boards are evidence opportunities, not release prerequisites.
- **Remaining V0.3 family:** Windows SDK/Qt compatibility implementation and product acceptance where an exact Windows cell is promised (Qt5/Qt6.5 still require explicit Windows qualification before claiming support). Optional Linux/embedded source/build portability repairs may be made without delaying unrelated Windows implementation. No cross-platform hardware qualification milestone is reserved or mandatory.
- **V0.4.0.0 RC:** inactive until the Windows-applicable, declared GA capability set is implemented and support/evidence matrix frozen; RC qualifies **Windows**; no Linux/ARM64/EGLFS/Wayland physical acceptance condition. Candidate/RC gates still run and enforce actual Windows quality. V0.4 is not a new-function dumping ground.
- **V1.0.0.0 GA:** contract and stable API apply to **verified Windows cells only**. Linux desktop, ARM64/EGLFS and Wayland are labelled Community-validation pending / Experimental / Unsupported / Not Applicable with exact evidence, not advertised as general GA-supported. Future real customer data may justify supported-cell promotion via separate reviewed release.
- **Historical release maintenance exception:** the already-published `v0.2.0.1` lineage had a Windows+Linux hotfix evidence contract. A specifically dispatched `release-readiness` FULL_GATE on `release/v0.2.0.1` or `v0.2.0.1` may opt into `legacy_dual_platform=true` to preserve that historical obligation; this is **never** the default for normal PRs, V0.3.3/V0.4/V1, and is not a future Linux platform gate. Linux user reports on newly Windows-only qualified versions follow the nonblocking process below.
- **Hotfix:** a user Issue is a triage request, *not* a guarantee every user configuration will receive a patch. A confirmed issue is fixed through version-policy-compliant maintenance/hotfix flow, on affected supported release where feasible, then tested on Windows and on reporter-provided Linux/device evidence if available; release notes disclose tested scope. Avoid retroactive edits to published tags.

## 3. User-reported Linux/embedded issue procedure

Collect: tag/build SHA and artifact origin; Qt full patch + toolchain; distro/kernel/libc + architecture; board/BSP/rootfs/graphics/QPA; the chosen C++/QML/Generic/QPA integration; exact launch/deploy command and runtime dependencies; Viewer product/version/RFB settings; minimal reproduction and expected/actual result; bounded secret-free logs/screenshot/video; network/auth/encryption reality.

Agent triages: **shared Runtime correctness** (test Windows and review for all platforms), **target-specific fix** (patch exact cell with supplied evidence), **downstream Qt/BSP/toolchain behavior**, **unsupported QPA delegate** (document explicit limitation). Issue carries affected versions, severity, repro status, exact fix SHA, reviewer disposition, Windows regression and available target evidence, release train/version, advisory/release notes and remaining restrictions. Never call untested Linux or board hotfix physically PASS.

## 4. Owner/Agent execution rule

`AGENT_PRIMARY_HUMAN_ASSISTED`. Owner expects Windows-first real development and no repeated requests for Linux hardware. Human is needed only for necessary real Windows interactions and user-provided cross-platform evidence. Agents implement and test Windows work, preserve regression safety and proactively accept user Issues. Do not hold any release on missing Linux/embedded/Wayland physical validation, even if previous Issues or documents say `MANDATORY_PRE_V04`; this document and approved #563 must be synchronized into those Issue bodies/machine map before the next release candidate.

The support matrix and release notes must explicitly distinguish tested Windows from unverified Linux/embedded. Optional already-running Linux CI is diagnostic value, not a mandatory release condition unless a separate future Owner decision promotes that exact platform to Supported.

Refs #1 #24 #33 #57 #95 #109 #144 #175 #203 #235 #236 #343 #346 #370 #393 #558 #563.
