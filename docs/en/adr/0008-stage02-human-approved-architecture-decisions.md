# ADR-0008: Stage 02 Human-approved architecture decisions and applicability

> Language / 语言: **English** | [中文](../../adr/0008-stage02-human-approved-architecture-decisions.md)

Status: **Human APPROVED the overall Stage 02 product architecture DESIGN freeze (G7) on 2026-11-04; all five prior local choices remain in force. Not an implementation, test or release claim.**  
Record owner: [HyRemote #814](https://github.com/skawu/HyRemote/issues/814)  
Product authority: Human-approved *HyRemote Product Design* `HYR-PD-001 / V14` (2026-11-03; §§2.2, 3.2–3.4, 4–6, 10, 14).

## Decisions

| ID | Choice | Precise boundary |
| --- | --- | --- |
| D-02 | A | Multiple clients can connect and share transport-level input coordination; this does **not** automatically grant concurrent control of the same application scope. V14 §6.1 defaults to **one remote operator per Scope**, with concurrent viewers; host-approved independent, non-conflicting Scopes may be controlled separately. |
| D-08 | A | Preserve the RFB **configuration default** `0.0.0.0:5921`, not unconditional permission to bind all interfaces. Native endpoints and security settings are independent. |
| AR-04 | A | Allow Qt Private only in the shared Runtime's dedicated window-system remote-input/high-DPI internal unit (currently `qt_window_system_input.cpp` / `Qt6::GuiPrivate`). Core, public SDK types and unrelated Runtime code remain isolated; the QPA-specific private platform-entry seam stays separate and requires exact Qt Kit/platform qualification. See [ADR-0007](0007-four-integration-runtime-architecture.md). |
| AR-05 | A | Preserve standard RFB **and** a complete independent Native protocol as peer remote-access paths. Native owns device trust, sessions, authorized target discovery/subscription, framebuffer, input, control request/revocation and baseline negotiation; it does not depend on the RFB picture. The previous HyRemoteView G2 standalone Enhanced TLS Control/Diagnostics side channel is no longer a mandatory base path. Optional enhancements require capability negotiation and host authorization. |
| H-02 | a | In **future new-architecture deployments**, trusted DeploymentAdmission is required **before opening any protocol listener**. Validate the app instance and provenance, allowed targets, actual network endpoint, protocol security, default view-only policy, and trusted configuration source. Reject missing/invalid approval before bind; never silently switch address, port or security level. Preserve RFB getter defaults, but accept the explicit behavior change that legacy `start()` without admission no longer binds. **No legacy bypass switch.** |

## Mandatory cross-layer constraints

1. **Device-side authority:** distinguish device identity, Session, Target, ViewGrant, ControlGrant, Scope and expiry. Connection state or a global input-enabled flag is not a grant. Recheck before input admission and immediately before Qt delivery; revoke stale queued input, held keys/touches and unsent restricted frames.
2. **Privacy and output:** shared raw capture is permitted, but authorization/masking/watermark/output policy and pending buffers must be enforced per session to prevent cross-protocol data leakage.
3. **Protocol and security:** standard RFB interoperability remains independent; existing RFB VNC Authentication is **not encrypted**. Native TLS/password, verified device identity, certificate-change pause and no automatic downgrade to plaintext RFB are **future design requirements, not shipped features**.
4. **Lifecycle and budgets:** RFB-only, Native-only and Dual builds serve four C++/QML/Generic/QPA peer entrances through one Core/Runtime; protocol failures may be isolated, while device-side grants remain authoritative. Slow clients and input/frame buffers must remain bounded. Numbers in design annexes are not measured performance claims.
5. **Product ownership:** Framework enforces device-side remote grants/targets/protocols; HyRemoteView owns the operator experience, client device trust and its own updater; HyRemoteTool owns integration diagnostics. Gateway/enterprise services are optional and cannot be required for offline Direct base access.

## Supersession and factual status

- This record **amends** the old absolute public-Qt-only Runtime constraint in [ADR-0007](0007-four-integration-runtime-architecture.md), retaining only the explicitly scoped exception. Historical ADR rationale remains visible.
- [HyRemoteView G2 #4](https://github.com/skawu/HyRemoteView/issues/4): AR-05=A supersedes the **base transport choice** of an independent Enhanced side channel. The original G2/G3 evidence remains historical.
- [HyRemote #246](https://github.com/skawu/HyRemote/issues/246): the HRC-5 stand-alone Enhanced endpoint is no longer a **base architectural dependency**; diagnostics/enterprise extensions remain future conditional work.
- H-02a is effective only in versions actually implementing the **future admission contract**. Existing released RFB binaries and support claims are not retroactively changed. Migration and explicit error messages will be required at implementation.
- The Stage 02 canonical Native design candidate is R15 (21 message types, 50 positive/48 negative vectors, 15 state scenarios), with R16–R18 security/grant/budget signoff. Vector self-checks are **design-only**: no claim of Native, TLS or Viewer interoperability being implemented.

## G6/G7 archive and freeze status (2026-11-04)

- **G6=PASS:** decision records were merged by [PR #815](https://github.com/skawu/HyRemote/pull/815) into `develop@c1684cf4cd2394c9b06f40edafc63b474658ecd8`; bilateral supersession comments and ADR readback were verified.
- **G7=HUMAN_APPROVED (2026-11-04):** following the Stage 02 G7 whole-architecture freeze approval card, Human explicitly answered **“批准”** (approved). Track archive in [#817](https://github.com/skawu/HyRemote/issues/817). See the exact [Stage 02 frozen design manifest](../../internal/architecture-stage02-g7-freeze-record-2026-11-04.md).
- The frozen **design** encompasses product responsibilities and all 52 traced subsections of `HYR-PD-001/V14`, the single R15 architecture/Native contract, R16 host grant/security/budget addenda, R18 design signoff, and the five locally approved Human decisions interpreted together.
- **Material change control:** a change to default exclusive control scopes, secure DeploymentAdmission, independent protocol/wire semantics, authorization/privacy, four entrances, or Core/Runtime ownership requires documented old/new requirements, impact and rollback/migration analysis, followed by explicit Human design-change approval before implementation.
- **Not authorized:** no inference that Native/TLS/deployment admission is implemented or tested, no development start, release, unqualified Qt support claim, or skipping the structure/content/figures/layout planning process before Word compilation. Next stage is the architecture document's structure and content planning.
