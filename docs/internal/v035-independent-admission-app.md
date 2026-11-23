# V0.3.5.0 independently sourced GitHub App admission

**Status:** Source candidate only. D-ADMISSION=A is approved, but this code
does **not** register an App, deploy a service or activate a required ruleset.
The release remains HOLD until all three actions and adversarial enforcement
checks are evidenced. See HyRemote #95 and #1050.

## Authority and threat boundary

This external issuer is separate from all `release/v0.3.5.0` PR code and
GitHub Actions `workflow_run` advisory checks. It **never clones, executes or
imports** a release PR's head checkout. Its packaged source is pinned to a
reviewed immutable commit/artifact digest and deployed under separately
managed credentials, with read-only Contents + Pull requests and write Checks.
The app's private key and webhook secret must never enter the repository,
GitHub Actions from candidate PRs, chat transcripts or build artifacts.

Only one precisely reviewed correction is accepted:
`release/v0.3.5.0@b0680d5d` plus
`feature/589-release-preparation@681b51b1` with exactly four known blobs.
The other 511 Git entries, including notes, CMake, manifests and scripts,
must match the original full source tree, with no published `v0.3.5.0` tag.

## Registration / secure deployment (repository administrator)

1. Create a **new independently administered GitHub App**, not a
   `GitHub Actions` workflow or ordinary commit status. Install on the
   **selected repository** `skawu/HyRemote` only.
   Repository permissions: **Contents: Read**, **Pull requests: Read**,
   **Checks: Read and write**. Subscribe to **Pull request** and **Push**
   events; a signed tag creation triggers a newer FAILURE check and revokes
   any earlier approval. Deleting the tag does not restore authorization.
   Configure an HTTPS webhook URL `/github/webhook` and random webhook secret.
2. Deploy an immutable reviewed artifact from the approved App source commit
   onto independently administered compute. Pin image digest / file checksum.
   Build with Python >=3.11 and `PyJWT[crypto]>=2.10,<3` on that machine.
   Keep GitHub App private key in a secrets manager, mounted at a read-only
   file path and never present in logs. Mount a separately managed, restricted,
   **persistent writable state volume** for `APP_STATE_DIR`. It stores the
   monotonic `v035-<SHA>.revoked` marker, `*.success-attempted` pre-POST
   journal, `*.acked` **last observed FAILURE** record, and shared
   `*.issuance.lock` file. Use a Linux/POSIX local durable filesystem
   with working `flock(2)`, file fsync and directory fsync. Do not use a
   network mount with uncertain lock/durability semantics or per-process mounts.
   Never put this directory in the repository, container writable layer,
   ephemeral cache, build artifact, or user home.
3. Required environment: `GITHUB_APP_ID`, `GITHUB_INSTALLATION_ID`,
   `GITHUB_APP_PRIVATE_KEY_FILE`, `GITHUB_WEBHOOK_SECRET`,
   `APP_STATE_DIR` (durable, pre-created directory).
   Bind the built-in HTTP server to `127.0.0.1:8097`; use a secure TLS
   ingress/reverse proxy with egress to `api.github.com`, resource/rate
   limits, and ingress webhook signature verification. No public direct HTTP.
   Use exactly one running issuer service instance. Its webhook threads,
   retry worker, and any trusted manual `--check-pr` process MUST share the
   same `APP_STATE_DIR` and cross-process POSIX `flock` lock; an in-process
   mutex alone does not protect CLI issuance. The process lock encompasses
   the final revocation check and the check-runs POST. CLI from a different
   host or volume is prohibited. GitHub webhooks are asynchronous and
   cannot make tag creation and release merge a single atomic transaction:
   retain independent tag/release authority and recheck the tag immediately
   before formal promotion.
4. Before any SUCCESS Checks POST, the App durably records a
   `*.success-attempted` marker. A request that times out or whose issuing
   process crashes may still be committed by GitHub later; absence of a
   successful HTTP response is **not** proof that SUCCESS was not issued.
   The signed tag-push webhook records `*.revoked` before any REST call.
   Revocation targets the fixed approved `HEAD_SHA` **regardless of whether
   the original PR is open or has the same branch/head**.
   The service queries GitHub Check Runs filtered to that immutable commit,
   exact context and its own App ID. If the latest observed App check is not
   FAILURE, it posts a new FAILURE. The `*.acked` record means **a failure
   was seen at one observation**, not permanent success of revocation.
   After revocation the worker **continues reconciliation indefinitely**,
   including after ACK, to detect and correct late remote SUCCESS after a
   timeout. It re-fsyncs even existing markers before REST/ACK and restarts
   reconciliation when the service restarts.

   This is **eventual reconciliation, not an atomic safety guarantee**:
   remote success can briefly precede the next poll or be replayed while
   the service is down. Require independent service health/uptime alarms,
   durable journal and regular check-result verification, and an independent
   tag/release authority that freezes publication on any pending, degraded
   or stale reconciliation. Do not consider the version REQUIRED source
   gate safe for promotion solely because an ACK marker exists.
   A separately reviewed release policy alone may reset revocation state.
5. Validate with
   `python -m unittest discover -s tests/release-readiness -p test_v035_independent_app.py -v`.
   Before protecting any branch, install the App and verify **the App itself**
   can publish `v035-independent-source-admission` on the one frozen PR SHA.
   For cold-start/manual check from the trusted **same host and mounted state**:
   `python .github/release/v035_independent_app.py --check-pr <PR_NUMBER>`.
   Execute it only using the reviewed pinned App artifact and identical
   `APP_STATE_DIR`; never bypass its cross-process lock or clear markers.
   Never use a personal token to fabricate an identically named check.
6. Once live App check source and ID have been confirmed, generate—not yet
   apply—the narrowly scoped ruleset body using
   `python .github/release/v035_independent_app.py --print-ruleset <REAL_APP_ID>`.
   An authorized repo admin then creates it via repository Rulesets
   (or the GitHub REST `POST /repos/skawu/HyRemote/rulesets` endpoint).
   Confirm `integration_id` is the App's actual **App ID**, the source check
   context is exact, `strict_required_status_checks_policy=true`, the
   matching ref is ONLY `refs/heads/release/v0.3.5.0`, no bypass actor,
   and the existing generic release ruleset remains unchanged.
7. Prove enforcement on a **non-merged, disposable** negative PR/head:
   missing App check blocks, ordinary Actions check with the same name
   does not satisfy the source binding, bad extra file/blob/mode/SHA/tag
   fails, old event/HEAD cannot approve new HEAD; verify concurrent manual
   CLI invocation cannot post SUCCESS after signed tag revocation, and test
   fsync EIO followed by successful re-sync without missing a revocation;
   close/move the original PR before a signed tag event and verify the
   fixed SHA is still revoked; inject a late remote SUCCESS **after** a
   verified FAILURE/ACK and verify the background reconciler repairs it.
   Confirm that service outage or an ambiguous remote request is treated as
   publication HOLD, never as an App admission PASS. Report ruleset ID,
   source integration ID, head SHA, API check publisher and negative outcomes
   to #95. Restore the single frozen candidate state after probes.

Only then can the #1050 P1 be reconsidered for closure. After the reviewed
four-file repair is merged, the **next notes-only stage needs a newly reviewed
independent allowlist or ruleset transition**. Never disable the required
check just to allow finalization.

## Live version / physical claims

D-200DPI=B: Windows system-native 200% scaling is **outside V0.3.5.0's
qualified support scope**. Four physical cases remain NOT_RUN, not PASS.
This neither disables runtime functionality nor waives ordinary-DPI Qt5/Qt6,
watermark fail-closed, reconnect, viewer or release-candidate tests.

No automated source validation grants permission to merge #783, create tag,
GitHub Release, mark G3 passed, or change HyRemoteView's SDK gitlink.
