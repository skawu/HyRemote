# HyRemote Local Owner inbox — opt-in worker (#768)

This is a **real local process** rather than an independent ChatGPT conversation.
It does **not** install or run merely because Cloud Owner posts a GitHub comment.
The authoritative [Cloud/Local/Human responsibility and no-manual-relay contract](../../docs/internal/cloud-local-human-operating-contract.md)
defines task ownership, automatic ACK, exceptional Human intervention and escalation.
**Do not ask Human to request the next task**; once the worker is actually
provisioned, it reads reviewed orders from GitHub automatically.

## Trust and roles

Cloud submits reviewed, machine-readable JSON orders by PR to
the protected develop inbox at .github/local-owner/orders/.
Only an explicitly running worker queries GitHub's contents API for that inbox,
verifies each order's schema, repository, owning Issue #95, exact 40-hex candidate
SHA, expiry, approved Windows Kit, and supported task. Issue comments **cannot**
supply shell commands. A release branch that has moved blocks execution.

The worker writes a durable local ledger outside the Git worktree; posts
RECEIVED and STARTED ACKs to #95, then reports DISPATCHED, MACHINE_PASS, BLOCKED,
or PHYSICAL_REQUIRED. Only a local machine that actually runs this worker can
produce an ACK. Without a provisioned worker the order remains UNDELIVERED.

## One-time setup, Local Owner (Windows)

1. Run this worker from a checked-out develop tree on the qualified Windows
   test host. Provide Python 3.10+, GitHub CLI gh, an existing Qt 6.8.3/MSVC Kit,
   and the known-good RC2 installed prefix. Authenticate gh once with **read
   repository contents, Issue comment and Actions workflow dispatch** permissions.
   Never store a token in a work order, an Issue or an executable argument.
2. Provide a **separate clean detached candidate checkout** at the task's SHA.
   The worker does not switch or clean user trees. Provide the locally owned
   Qt Kit config and the deployed prefix bin path.
3. Run the built-in offline self-test:

    python tools/local-owner/worker.py --self-test

4. Inspect pending orders without triggering anything:

    python tools/local-owner/worker.py --once

5. Explicitly authorize one local execution:

    python tools/local-owner/worker.py --execute --once --workspace "C:\HyRemote\candidate-rc2" --kit-config "C:\HyRemote\build\local-qt683.yml" --prefix-bin "C:\HyRemote\candidate-rc2\build\evidence\prefix\bin"

6. For unattended pickup while this Windows user is logged in, explicitly
   authorize one-time Scheduled Task registration (the helper is opt-in, never
   silent). Without -Register the script prints the plan and exits:

    .\tools\local-owner\register-worker.ps1 -Register -Workspace "C:\HyRemote\candidate-rc2" -KitConfig "C:\HyRemote\build\local-qt683.yml" -PrefixBin "C:\HyRemote\candidate-rc2\build\evidence\prefix\bin"

The task wakes every two minutes and runs one safe inbox cycle under the logged-in
user. No elevated service is installed, and the RC2 source branch is not changed.
**Check the first RECEIVED ACK at #95 to prove actual delivery.**

## Exact RC2 orders already authored in the reviewed queue

- rc2-hosted-l2-20261102: verify exact branch SHA; request GitHub Actions
  ci.yml with purpose=release-readiness and expected_sha=RC2. DISPATCHED is
  **not** L2 PASS. Cloud separately verifies the actual run and Qt lanes.
- rc2-canonical-g6-20261102: reject a dirty or wrong-SHA local worktree;
  run the canonical unfiltered build.cmd test with the locally selected Kit
  profile. A zero-test run cannot return MACHINE_PASS.
- rc2-installed-unicode-20261102: run real installed Generic/QPA examples
  serially with six distinct temporary state directories. Construct ASCII,
  Chinese and quoted-space case values from Unicode code points and compare
  the persisted watermark.policy text. Never post actual watermark content.
  This can return PHYSICAL_REQUIRED, **not** an automated RealVNC Viewer PASS.

The physical RealVNC operator/Local Owner must still record the observed image,
maintained Viewer version and screenshot at #95. These three tasks expire
2026-11-30; a moved candidate branch SHA blocks execution.

## Safety, recovery and audit

- No arbitrary remote shell commands, URL fetches, branch switching, force
  pushes, recursive deletion, privilege elevation, or customer-text logging.
- The local ledger at the user's .hyremote-local-owner/ledger.json (or a
  locally selected state directory) is atomically written. Work orders are
  at-most-once by ID. Reconnection can retry delivery of a finished report,
  not repeat a physical test or dispatch.
- A worker crash at RECEIVED/STARTED requires operator inspection before
  approved recovery. Never blindly clear the ledger to re-execute a task.
- Logs are **local only**. GitHub comments contain short controlled fields,
  not raw test logs, environment variables, watermarks or authentication tokens.
- The polling period does not limit actual build.cmd duration. An unfiltered
  full candidate test may legitimately run for many minutes.
- Task results do not authorize release-main promotion, tag creation,
  security claims or Release publication.

**Deployment truth:** the worker implementation and RC2 orders were merged
through #769, but a source merge does not install the worker on a Windows host.
Check #95 for actual machine `LOCAL_OWNER_WORKER RECEIVED` evidence before
claiming delivery or execution. Without it the only truthful status is
`RECORDED_UNDELIVERED / RELEASE_PROMOTION=HOLD`.
Once the worker is running, Local Owner performs the next reviewed order
without a new Human-prompt or copy/paste handoff. Cloud Owner is responsible for
proactive tracking **through a real active process/monitor**; this README does
not create an always-on Cloud agent or notifications.
