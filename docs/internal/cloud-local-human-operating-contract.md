# HyRemote Cloud Owner ↔ Local Owner ↔ Human operational contract

Status: **normative for agent handoff and human-intervention boundaries**.
Authority: #768 delivery integration, #95 release governance, `AGENTS.md`, the reviewed
[Local Owner worker](../../tools/local-owner/README.md), machine release trains and each task's
exact target SHA. This document is **not** a new product release or permission grant.
It supersedes conversational "please ask for the next task" practices and old version-specific
Owner instructions **only as an operational role/handoff protocol**. It does not override
product authority, branch protection, source evidence or official security constraints.

## 1. Operating rule — Agent primary, Human on exception only

**Human is not the task dispatcher, issue reader, PR reviewer, build runner, status
poller, evidence archivist, or message courier between two Agents.**

- **Cloud Owner is accountable** for product outcome, issue/task backlog, task authorization,
  cloud code changes, independent review, scope/risk decisions, release eligibility and
  communicating the *verified* overall state. It must create machine-readable task
  orders, then check ACK/result/evidence through a real connection. Publishing a comment
  is not dispatch.
- **Local Owner is accountable** for the configured Windows machine and its qualified
  Kit, local agent/worker provisioning within authorized privileges, exact-SHA builds,
  local G6, installed examples, physical Viewer coordination, data minimization and
  truthful machine-specific test evidence. It pulls its next approved order; Human does
  **not** need to tell it what task to do next after worker provisioning.
- **Human is an exception handler**, providing only an explicit product/security/permission
  decision that cannot be made by the Agents, or physical device/Viewer interaction that
  cannot be automated. Human may give one-time consent to install/authorize the worker,
  but does **not** repeat consent for every normal, already approved task.

Neither ChatGPT conversation runs continuously after the conversation stops. A local
worker / connected automation must actually be **installed and running** for unattended
delivery; cloud monitoring requires a running Cloud agent, configured automation or
connected service. Do not claim background observation, instant delivery, self-start
or scheduled follow-up merely because a GitHub Issue was updated.

## 2. RACI (A = answerable/approval, R = performs, C = consult, I = notified only if necessary)

| Activity | Cloud Owner | Local Owner | Human |
| --- | --- | --- | --- |
| Plan versions, qualify feature scope, approve security/support promises | **A/R** within established mandate; escalate exceptional decisions | C | C only when product mandate changes |
| Search existing Issues/PRs, implement source fixes, code review and merge | **A/R** | C; local-only fix where specifically assigned | — |
| Create, review, register typed work order and choose exact SHA / Kit / gates | **A/R** | C for real machine capability | — |
| Automatically discover and ACK an approved order | A for delivery contract | **R**, only if a worker is running | — |
| GitHub-hosted exact-SHA L2 dispatch | **A**, R whenever tool permissions allow | R only through approved allowlisted worker when cloud cannot dispatch | — |
| Windows local G6, installed Generic/QPA test, device logs | A for evidence acceptance | **R** | — |
| Real physical Viewer/image judgment where machine automation cannot establish fact | A for user outcome | **R** for setup/capture/evidence | R *only* for unavoidable visual/physical action |
| Classify FAIL vs BLOCKED, fix source, decide new candidate and retest scope | **A/R** | R for local reproduction and bounded rerun | — |
| Protected release→main, annotated tag, Release and backmerge | **A/R** | I | R only for administrator permission unavailable to agent |
| Recovery of stopped worker, missing Kit, stale ledger, failed GitHub login | A for task/risk and escalation | **R** for diagnosis and permitted local repair | R only if OS/credential boundary demonstrably requires user |
| Ongoing communication/status and next task | **A/R**, when actually running or scheduled | **R** through real ACK/status | Not a status dispatcher |

The Local Owner cannot change the product roadmap, release authorization or public
support/crypto claims. Cloud Owner cannot assert local physical evidence from hosted CI.
Human should never become the fallback worker merely because an Agent stopped asking.

## 3. The only approved handoff route

```text
Cloud Owner: selects scope/Issue/immutable candidate SHA and gates
  -> reviewed machine-order PR to develop/.github/local-owner/orders/
  -> Cloud verifies merged order, queue and expiry; status=RECORDED
  -> installed Windows worker polls the reviewed inbox (only after opt-in)
  -> validates schema, allowlisted action, SHA/ref, expiry, local Kit/workspace
  -> posts actual RECEIVED ACK to owning Issue
  -> STARTED, executes only a fixed, authorized task implementation
  -> posts DISPATCHED | MACHINE_PASS | FAIL | BLOCKED | PHYSICAL_REQUIRED
  -> Cloud verifies run IDs, evidence provenance and outcome
  -> next order/repair/retest/release decision automatically enters the queue
```

A GitHub Issue comment, PR body, `@mention`, or verbal "task assigned" is
**not** an executable order and never wakes a separate Local Owner Agent. A
`workflow_dispatch` event runs GitHub Actions, not the local Windows Worker.
Do not copy text from Issue comments into a shell as an unattended order.

**Delivery/accountability states**:

| State | Proof required | What Cloud may truthfully claim |
| --- | --- | --- |
| `RECORDED_UNDELIVERED` | Reviewed order exists, but no machine ACK | Written/queued only |
| `RECEIVED` | Real worker ACK, matching task ID and SHA in #95 | Delivered, not executed |
| `STARTED` | Actual worker STARTED ACK | Started, not PASS |
| `DISPATCHED` | GitHub workflow submitted or reused, run ID when available | Triggered, not L2 PASS |
| `MACHINE_PASS` | Executed canonical local checks with nonzero counts, exact Kit/SHA, logs | Machine result only |
| `FAIL` | Actual test/product contradiction | Repair / issue triage required |
| `BLOCKED` | Explicit no permission, wrong SHA/Kit, missing artifact, etc. | Prerequisite not satisfied, never PASS |
| `PHYSICAL_REQUIRED` | Automated policy checks completed, Viewer observation not done | Human/physical visual evidence still missing |
| `CLOUD_ACCEPTED` | Cloud independently checks proof, same-SHA gates and claims | Evidence accepted; still not released |
| `RELEASED` | Protected promotion, exact tag, truthful public Release/backmerge completed | Published version |

Cloud owns **reconciliation**: a missing ACK, stale record or missing result must
be classified and surfaced by the Cloud workflow/monitor, not discovered by Human
asking for instructions. A heartbeat/notification monitor is **not provided
automatically by this document**; see #768 until connected and verified.

## 4. Human interruption policy — when and how

**Do not contact Human** for: next task request, order forwarding, CI red/green
queries, ordinary permission-free code work, reviewing logs, duplicate tests,
identifying local Kit/profile paths already in the Local Owner's environment,
GitHub issue/PR hygiene, or polling a worker.

Escalate **only** when both Agents have exhausted authorized alternatives:

1. **One-time local activation**: Local Owner cannot itself provision its
   non-admin Task Scheduler entry or the needed GitHub credentials. Ask Human
   for **one exact approval/action**, with machine, purpose, least-privilege
   scope and safe rollback. Do not ask Human to run the six test cases.
2. **Physical operation**: A maintained Viewer requires a real mouse/keyboard,
   visual on-device observation or physical second-machine handling that the
   Agent cannot supply. Local Owner prepares the session/screenshot capture;
   Human performs **only that specific gesture or observation** and returns
   the observed outcome. Agent owns evidence packaging.
3. **Exceptional decision**: A product/support/security/release scope change
   exceeds the Cloud Owner's delegated authority, or a GitHub admin action
   cannot be performed with agent credentials. Present one decision with
   concrete options and impact.

Every intervention request must include one compact, actionable record:

```text
BLOCKER_TYPE=HumanDecision | HumanAdmin | Credential | Physical
TASK_ID=<existing machine order or Issue>
CANDIDATE_SHA=<40 hex>
WHY_AGENTS_CANNOT_COMPLETE=<specific failed permission/physical boundary>
ALTERNATIVES_ATTEMPTED=<actual work; never blank speculation>
HUMAN_ONE_ACTION=<one click/permission/physical observation>
SAFE_EXPECTED_RESULT=<observable outcome>
NEXT_AUTOMATIC_OWNER=Cloud | Local
```

After that single action, ownership **automatically returns** to the Agent
that requested it. Human is not responsible for following up to see if the
Agent continued. If no notification channel is configured, the status remains
in the owning Issue; never pretend the Human received a push notification.

## 5. Escalations, outages and safeguards

- **No ACK**: Cloud must verify the order was merged, the Windows worker was
  provisioned and authenticated, and the release SHA still matches. Do not
  silently declare `DISPATCHED`. If a local service is absent, Local Owner
  handles provisioning once; Human is contacted only for an ungrantable permission.
- **Stuck STARTED**: Local Owner reconciles log/ledger before any rerun. No
  automatic replay or clearing state. Cloud may issue a *new* authorized order
  only after evaluating duplicate execution and candidate evidence.
- **FAIL**: Local Owner provides bounded source-first reproduction with logs,
  Cloud repairs via reviewed PR and decides exact new candidate SHA.
  If source changes, earlier candidate evidence is **historical**, not new PASS.
- **BLOCKED wrong SHA or Kit**: Fail closed. Do not auto-checkout over dirty
  local worktree, switch unknown prefix, purge user state or rewrite tags.
- **Physical evidence**: No Worker may return `Viewer PASS` from a policy
  file alone. Human should not take over build/test orchestration.

Avoid noisy, deadline-free reminders. A future monitoring automation may
notify appropriate maintainers only on **specific transitions** (e.g. no ACK
after a configured threshold, FAIL/BLOCKED, physical input ready, publication
completed). No notification destination, time threshold or autonomous monitor
is currently implied; those must be connected and validated explicitly.

## 6. V0.3.5.0 RC2 application (current bounded example)

```text
REPO=skawu/HyRemote
RELEASE_REF=release/v0.3.5.0
RC2_SHA=92c1ab1a354a3e8c9ad93311e308d039da59317f
CLOUD_ACCOUNTABLE=#95
INTEGRATION_ISSUE=#768
INBOX=.github/local-owner/orders/
WORKER=tools/local-owner/worker.py
ORDERS=rc2-hosted-l2-20261102,
       rc2-canonical-g6-20261102,
       rc2-installed-unicode-20261102
```

The reviewed worker implementation is merged to `develop` via #769,
but **as of the latest documented check** its Windows installation and first
actual `RECEIVED` are not verified. No RC2 G6, L2 or RealVNC acceptance is
inferred from this contract. The expiry is 2026-11-30; the agent must not
run an expired order. RC2 and its pending release status are unaffected.

**Concrete transition needed once, not per release:** an authorized Local
Owner session provisions the Worker per [README](../../tools/local-owner/README.md)
from a current develop checkout, on the qualified Windows user, with its own
approved credentials/Kit and clean detached candidate worktree. Human only
authorizes an OS/credential/physical exception if the Local Owner cannot.
Once an actual ACK is visible, Cloud takes over tracking and phase transitions.

Future product releases use new reviewed order IDs and exact SHA, **not
instructions copied by Human from the last chat**. Until the source code,
queue registration, running worker, ACK and result are all proven, do not call
the pipeline autonomous.
