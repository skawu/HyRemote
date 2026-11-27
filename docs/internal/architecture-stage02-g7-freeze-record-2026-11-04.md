# HyRemote Stage 02 — Human G7 overall architecture design freeze record (2026-11-04)

Status: **HUMAN_APPROVED / ARCHITECTURE_DESIGN_FROZEN**.  
Archive task: [#817](https://github.com/skawu/HyRemote/issues/817).  
Previous prerequisite: **G6=PASS**, [PR #815](https://github.com/skawu/HyRemote/pull/815), merged `develop@c1684cf4cd2394c9b06f40edafc63b474658ecd8`; bilateral supersession readbacks completed.  
This is a **decision record** for Stage 02, not an implementation, CI runtime, acceptance or software-release claim.

## Approval event and scope

On **2026-11-04**, Human answered **“批准”** to the Cloud Owner's explicit request to approve **Stage 02's overall product architecture design freeze**, after being presented with the `HyRemote_阶段02_G7_总体架构冻结审批卡.md`. The exact approval-card SHA-256 is
`4eee773ddb607ccbbd0c7695c923d8b291176ffc3ee5a22e9f58b4271b65e9ba`.
This approval incorporates the five previously approved decisions `D-02=A; D-08=A; AR-04=A; AR-05=A; H-02=H-02a` (see [ADR-0008](../adr/0008-stage02-human-approved-architecture-decisions.md)). It does **not** reopen them or retroactively change the published RFB binary behavior.

The approved design scope covers the following source artifacts **as a combined, priority-ordered baseline**:

| Priority | Source and actual verified SHA-256 | Authority |
| --- | --- | --- |
| P0 | Human-approved product design `HYR-PD-001 / V14`, source `HyRemote 产品设计文档_正式版_V14.docx`, `b9c6029faf686535ea8f9dc830ae2106a8d2ceef12779e805d70ab1fe336cd4c` | Product requirements and original 52 subsections |
| P1 | Five Human-approved local decisions in [ADR-0008](../adr/0008-stage02-human-approved-architecture-decisions.md), and G7 approval card SHA above | Deliberate precedence and approved scope, including the RFB listener/security distinction |
| P2 | R15 `HyRemote_阶段02_第15轮唯一技术契约候选包.zip`, `631fbcde3b0c7db7c5eff47d74e227d96098aa24180b1880a98734569771ddaf` | Single canonical Native 1.0 wire design, grant/privacy, build/config/resource and 52-section trace |
| P3 | R16 `HyRemote_阶段02_第16轮技术签收与H02决策材料.zip`, `4f9be4ab668383f310f4c218bbd4b80a31fc3054ddcdec03d16ddfbaa7aa47d6` | Later host authorization/zero-code, Native credential and budget refinement; H-02a approved instead of options |
| P4 | R18 `HyRemote_阶段02_第18轮技术签收与正式决策记录包.zip`, `39db1ded732f7a54de7e48c8605c8c6b96b03efa86b50eda70df2f7890222e30` | Final design-level technical signoff and freeze gate acceptance |
| P5 | G6 accepted documentation [ADR-0007](../adr/0007-four-integration-runtime-architecture.md), [ADR-0008](../adr/0008-stage02-human-approved-architecture-decisions.md) and cross-repo G2/HRC-5 supersession comments | Traceable repository record; does not claim to reproduce all annex bytes |

**Source availability note:** Original V14 DOCX and full zipped technical annexes were supplied in the decision conversation and are identified above by exact digest; they are not pasted into this public GitHub repository as part of the freeze-record PR. A digest is an integrity identifier, not a substitute for access to the source. Stage 03/04/05 document planning must explicitly label coverage derived from these identified sources and must not invent missing text or diagrams.

## Frozen product architecture boundary

- Three product responsibilities: Framework integrated in the host Qt app; standalone HyRemoteView for user access/updating; HyRemoteTool for SDK/Qt integration diagnostics; Gateway/enterprise services optional and not required for secure Direct/offline access.
- One UI/protocol-independent Core and one shared Qt Runtime; four peer integration facades C++/QML/Generic/QPA; Widgets/Quick are target adapters. Runtime may use Qt Private only inside the approved remote-input/high-DPI internal exception; QPA's independent private platform-entry seam remains exact-Kit qualified.
- Two independent server transports: standards-interoperable RFB and complete Native screen/input/session/identity/control capability. RFB-only, Native-only and Dual share device-side session/target/privacy/authorization logic. No mandatory third Enhanced TLS side channel.
- Security: RFB default configuration remains `0.0.0.0:5921` but, once the **future new-architecture** DeploymentAdmission is implemented, actual listener startup requires explicit trusted approval first, regardless of protocol. Input off by default; legacy VNC Auth is not transport encryption; Native secure handshake and credential design cannot silently downgrade. Existing binary/release facts unchanged.
- Authorization: per-session Target/ViewGrant/ControlGrant/Scope separation; default **one remote operator per application scope** while multiple clients can view/connect and technically coordinate input; host may permit independent nonconflicting Scopes. Privileged actions, data access, recording and host business outcomes do not inherit basic pointer/control permissions.
- Device-side session-specific output privacy/masking/unsent-buffer revocation, bounded input/frames/network resources, physical local UI/input priority. Viewer must not replay stale control or imply device trust simply from remembered address. Implementation and scenario-test evidence remain separately required.
- Native 1.0 candidate as amended in R15/R16/R18; earlier R11 single-target wire format and R14 partial vectors are superseded and must **never be revived as parallel 1.0 implementations**.

## Change-control gate

**After this G7 approval, material architecture changes require explicit Human change approval**. Initiator must provide exact approved baseline clause, proposed delta, necessity/evidence, security/compatibility implications, migration/rollback, design test effects and trace update. No implementation convenience, CI behavior or inferred memory may silently override G7.

Changes that are purely editorial/typographic and preserve frozen semantics may proceed through normal document review; engineering implementation may refine internal code without altering the product/API/wire/security contract, subject to separate development authorization.

## Stage transition and explicit non-authorization

G7 ends **Stage 02 design deliberation**. Next: **Stage 03 architecture document TOC and content plan → Stage 04 diagrams/figures/table inventory and content plan → Stage 05 visual/layout plan → Stage 06 Word artifact compilation/QA**. Each stage is planning/design before artifact compilation; earlier stages may be refined without silently changing frozen architecture.

G7 **does not authorize** development work, Native/TLS/DeploymentAdmission implementation, release/GA activation, QPA/Qt compatibility support claims, test PASS labels, or immediate Word generation. The 128 MiB Runtime managed-memory budget remains a **design target**, not proven RSS; Native packet vectors remain design-structure checks, not real interop.

## Status ledger

- `G1–G5=DESIGN_PASS` (product tracking, contracts, security/resources, freeze review; **not implementation validation**).
- `G6=PASS` (merged/cross-repo verified by PR #815).
- `G7=HUMAN_APPROVED_2026-11-04` (this archived decision).
- `STAGE03=OPEN` for architecture document directory/content planning.
