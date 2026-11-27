# HyRemote — long-term memory

Durable facts only. Per-task chronicles belong in the dated daily files, not here; this file is rewritten to stay small.

## Owner-set project conventions (override my own habits)

- **Ports require Human review.** Never choose a port myself, not even for a temporary check. Ports are numbered from
  the confirmed `5921` upward, assigned **business/functional first, then debugging tools, then test cases**;
  `5900` (the old `RemoteAccess` default) is not confirmed and is never used for a local run. Any port in evidence must
  be named in that evidence.
- **Work is not complete until it is merged**, and "merged" must name the branch: `feature/*` -> `develop` ->
  `release/vX.Y.Z.W` -> `main` + annotated tag. A parked candidate line is not delivery even when every local gate is green.
- **Never make the product more complicated than its own goal requires**: before adding a mechanism ask which product
  goal needs it, prefer the smaller surface, and if something merged is complexity without capability, propose removing
  it. Complexity that does not advance a mandatory item is a defect.
- **When to ask (owner-stated 2026-09-20)**: do not ask for confirmation on internal engineering - decide and execute
  structure, naming, file placement, gates, documentation wording, cleanup of my own artifacts and PR mechanics, then
  report. Ask only when (a) it changes a **published promise** (release/acceptance definition, public API, licence,
  compatibility or support claim), (b) it is **irreversible and not recoverable from the repository**, or (c) two
  **authorities conflict**. Never re-ask what was already ruled.
- **All build operations go through the existing build system**: `compile.cmd` / `clean.cmd` (one file that is both a
  POSIX shell script and a Windows batch file), tests through `ctest`, acceptance through the repository's own
  `product_fit.py` tools. No ad-hoc sequences, no throwaway scripts; my own tooling lives under `.codebuddy/` and is
  never presented as the acceptance procedure.
- **Check the issue tracker FIRST, for everything** (`gh issue list --state all`, `gh api search/issues?...`, this file,
  `docs/internal/repository-layout.md`). Only a search may support "this does not exist yet".
- **ONE working copy, ONE build directory, branches only** (owner ruling 2026-09-20): work in the single checkout by
  switching branches - never a second `git worktree`, never a per-task `build-<something>`, never copy the repository.

## CURRENT TRUTH (authoritative as of 2026-09-22)

- **`build.cmd` is the single build/install entry point** (#326): `build.cmd` / `build` / `install` / `test` / `clean` /
  `rebuild` / `help`, with `build.yml` still the persistent configuration authority and CLI overrides above it.
  `compile.cmd` is only a compatibility shim that prints a deprecation notice and forwards to `build.cmd build` - it
  holds no Qt discovery, no CMake invocation, no option parsing and no clean/test/install semantics. The install root is
  `<build-dir>/install` and the install command prints `HYREMOTE_INSTALL_ROOT=` plus `HYREMOTE_INSTALL_MANIFEST=`;
  `build/` is developer intermediate output, `build/install/` is the product/SDK tree users and consumers take.
  `clean` removes the build tree (which contains the install root) and `rebuild` is clean plus build with no implicit
  install.

- **V0.2.0.0 is a user-first LAN trial, and the machine authority now says so** (#323, PR #325, merge
  `bf3e8881650df21e2cd667556cb5fbbd9a1c1b31`, CLOSED). `origin/develop` = `bf3e888`. The 0.2.0.0 entry requires
  `accepted-v0.1-lineage`, `usable-lan-remote-access-evidence`, `truthful-trial-security-state-evidence` and
  `customer-trial-evidence`; the first candidate may ship `BASIC_TRUSTED_LAN`; encrypted evidence is required only from
  a candidate that actually declares `AUTHENTICATED_ENCRYPTED`; the classification (parent #233, lineage 0.1.0.0,
  children 143/174/259/271, prerequisite #258, non-blockers #170/#239) is unchanged. Deciding rule: **the authority
  states the user promise, never the implementation plan** - if an evidence name would let unfinished work block a
  release the user can use, it does not belong there.
- **The first trial does not need TLS, and #143 is no longer a TLS issue**: #143 is *"Ship the smallest
  viewer-compatible access control; keep TLS non-blocking"*, `#174` allows an explicit numeric IPv4 LAN bind while the
  shipped mode is unencrypted, and `#271` lists a completed VeNCrypt/TLS profile under *not required for the first trial
  when not shipped* while requiring only that the shipped security mode is recorded truthfully. Order of work after
  #323: **#174 (LAN) → #271 (real user trial)**, not more TLS. PR #307 is to be **replaced rather than pruned** - see
  the 2026-09-22 daily note for the KEEP_NOW / DEFER / DELETE-REVERT split.

- **Delivery convergence status**: `#312` (security runtime deploy closure) merged as **#320**, merge
  `ebc67f44c14c4340375494d57fea21440693d070`, CLOSED. `#305`/PR **#306** (VNC-auth tests → Runtime ownership) merged,
  merge `8c84b3fe7b9a3893497461fea830e687b9f00d5a`, CLOSED. `origin/develop` = `8c84b3f`. Remaining: Phase 4 - prune PR
  #307 to the #143 product implementation plus the strictly-required TLS deployment delta, green it on the single
  existing `ci.yml` security evidence, then merge and close #143.
- **Build discipline, restated after a violation (2026-09-22)**: `compile.cmd` and `clean.cmd` are the only build
  entry points, the only build directory is `build`, and their own options (`--security`, `--qt-prefix=`,
  `--cmake=KEY=VALUE`, `--env=KEY=VALUE`) carry toolchain and dependency configuration. Per-task build directories and
  ad-hoc environment wrappers are forbidden - I had created `build312/`, `build-sec`, `build-220`, `build-replay` and
  wrapper scripts, which violated the owner ruling already recorded above; all of it is removed and the compliant
  commands are in the daily note for 2026-09-22.

- **Delivery is progressive**: `V0.1` Use It / Developer Preview (loopback-only) → **`V0.2` Trust It (security, session,
  network)** ← current line → `V0.3` Productize It → `V0.4` Qualify It / RC line → `V1.0` first GA from one mature V0.4
  lineage. Every V0.x is a real public release with a narrower contract. Authorities: `#1` roadmap, `#95` release trains,
  `#165` V0.4→V1.0 candidate freeze, `#33` GA acceptance, `#109` physical acceptance, `#209` foundation reconciliation,
  `#219` architecture (`#24` version semantics is CLOSED). Version digits never encode frontend/platform/Qt/UI.
- **V0.1.0.0 is released and public**: `main` HEAD = `v0.1.0.0^{commit}` = `23547d57e684ab3cddcb430fa44637264fdb6120`,
  annotated tag, GitHub Release `v0.1.0.0` `prerelease=true` with 3 source assets. Its public body had been published as
  **UTF-16LE** (PowerShell `>` redirect) and was repaired in place from the tagged tree's true UTF-8 (4975 bytes, no
  BOM/NUL) with `gh release edit --notes-file`; assets, tag and `main` were untouched.
- **Open governance debt**: **#294** - `release/*` carries a `deletion` ruleset rule while policy expects short-lived
  release branches, so a finished release line cannot be cleaned up. Decide before the next public release (A: authorize
  post-release deletion; B: redefine release branches as retained refs). Not a V0.2 blocker.
- **#258 (V0.2 technical preflight) is GO and closed** (`docs/internal/v0.2-technical-preflight.md`, PR #295 → `e700373`
  for the record, PR #297 → `fd3d92a` for the OpenSSL proof). The frozen wire profile is
  `RFB 3.8 -> VeNCrypt 0.2 -> X509Vnc 261 -> TLS >= 1.2 -> VNC Authentication`, proved against the real viewer
  **TigerVNC 1.16.2** in section A, and the same-socket plaintext→TLS transition is proved on **both** reference
  platforms by the focused lane `.github/workflows/tls-preflight.yml` running the standalone harness in
  `tests/preflight/`.
- **Owner decision (2026-09-22) on the TLS backend: OpenSSL only for V0.2.0.0 `AuthenticatedEncrypted`.** Schannel is
  **not qualified**, the platform default is **not acceptable**, and a runtime fallback to Schannel, to a weaker security
  type or to plaintext is **forbidden** - a missing OpenSSL backend/plugin/runtime must surface as `unavailable`. The
  mechanism is `QSslSocket::setActiveBackend("openssl")` called **before any TLS object exists** (Qt 6.8 exposes
  `availableBackends()` / `activeBackend()` / `setActiveBackend()`); `QT_TLS_BACKEND=openssl` is a diagnostic only, never
  a product mechanism. TLS stays Runtime/transport-private.
- **#259 (typed Runtime notifications) is DONE** (`CLOSED`, PR #303 → `82a91ae`). The private seam is
  `src/runtime/src/detail/runtime_notifications.{hpp,cpp}`: one bounded typed sink with
  `stateChanged(AccessState)`, `connectedClientCountChanged(std::size_t)`, `errorChanged(std::optional<Error>)` and a
  subscription token, exposed on `AccessInstance` as **subscribe/unsubscribe only** so a frontend can consume but never
  invent a notification. Every notification is a snapshot of the runtime truth taken **after Core handled the event**
  (published from the start/stop lifecycle, a `ClientCountingTransport` wrapper and an `ObservedCaptureSource`
  wrapper), ordered count → error → state; unchanged values are dropped while a repeated recoverable error still
  delivers. Each run has an activity token that `stop()` clears before teardown, so a late callback from a previous run
  is inert. **QML must not poll**: `QTimer`/`setInterval`/`refreshRuntimeSnapshot` are gone from the QML payload, the
  wrapper marshals a notification onto its own thread (queued `invokeMethod` with the wrapper as context + `QPointer`)
  and re-reads the snapshot; a frontend-local validation error takes priority over the runtime error.
- **What still has zero implementation in the security line**: **#143** the VeNCrypt/TLS encrypted transport, **#174**
  deterministic numeric-IPv4 listener control, **#170** authenticated session registry. The reserved extension point
  for #170 is a fourth typed callback in the same notification handler bundle, needing no Core change and no event bus.
- **#143 state (2026-09-22, later)**: the **encrypted profile is now implemented and green locally** on
  `feature/143-secure-transport` (unpushed commits `21c247a` + `bf266fd`; develop untouched, #143 still OPEN).
  `RfbSecurityProfile` is a closed enum (`None` / `VncAuth` / `VeNCryptTlsVncAuth`); `detail/security_preflight.*`
  selects `QSslSocket::setActiveBackend("openssl")`, verifies `activeBackend()` and reads/parses/matches the
  certificate and key (`X509_check_private_key`) **before any listener exists**; `RfbWorker` adopts the accepted
  descriptor into a `QSslSocket` (`incomingConnection(qintptr)` → `setSocketDescriptor`), performs the frozen
  VeNCrypt 0.2 / X509Vnc 261 negotiation in plaintext, sends the one TLS-init byte, calls `startServerEncryption()`
  on that same object and only then sends the VNC challenge **inside** TLS. A client asking for None, for plain VNC
  Auth, another sub-type, a version below 0.2, or only TLS 1.0 is refused; TLS/malformed/stall are bounded and the
  listener keeps serving. `HYREMOTE_WITH_TRANSPORT_SECURITY` now requires and links **Crypto + SSL** privately and its
  help text states both profiles; the option name/default are unchanged.
  `src/runtime/tests/test_rfb_secure_transport.cpp` drives the real transport with a deterministic Qt/OpenSSL peer and
  passes (`SECURE_TEST_RESULT=PASS`, failures 0). **Still to do**: update the five `test_remote_access.cpp`
  assertions that still expect `SecurityUnavailable` for `AuthenticatedEncrypted` (they encode the old
  refuse-to-start behaviour and now fail), docs truth, the secure deploy payload (Qt `qopensslbackend` + OpenSSL
  runtime DLLs), NOTICE.md, the focused security-enabled hosted lane, one real TigerVNC interop run, and the PR.
- **Mandatory-set mechanics**: `.github/release/v1-mandatory-issues.json` is machine-authoritative, pinned by
  `tests/release-readiness/check_release_authority_policy.cmake`, which also enforces an **anti-leak** rule - every `#N`
  named in a declared authority document must be classified in that JSON. Cite authorities in words or classify deliberately.
- **Plan artifacts must be read at the candidate SHA** (`git show <sha>:<path>`), never from the shared working tree,
  which sits on another branch. Reading it there already produced one wrong conclusion (corrected on #33 `5737340379`).

## Architecture (issue #219 is the authority)

- Flat `src/` roles: `src/core` (UI/protocol/platform-neutral, Qt-free, not installed, not linkable by a payload),
  `src/runtime` (shared Qt product runtime used by every frontend), `src/cpp` (`HyRemote::RemoteAccess`), `src/qml`
  (`import HyRemote`), `src/generic` (QGenericPlugin zero-code), `src/qpa` (`qhyremote`, Qt 6.8.3 **exact private ABI**,
  long-term Factory Trampoline only). Dependency direction: `cpp | qml | generic | qpa -> runtime -> core`.
- Frontends may not link Core directly, may not compile a second `remote_access.cpp`, and may not reach into another
  frontend; the boundary is enforced by `check_repository_layout.cmake`, not by directory shape.
- **Layout conventions (owner-confirmed)**: the top level answers one question - does it ship? - and is exactly
  `src/ tests/ examples/ docs/ cmake/ .github/`. Never add a second axis there. `research/`, `spikes/`, `integrations/`,
  `verification/`, `assets/` are retired names; the layout gate forbids their return. The brand mark lives at root `logo/`.
- `docs/internal/repository-layout.md` is the complete layout specification; `docs/README.md` + `docs/en/README.md` are
  bilingual navigation roots; user zone `docs/guide/**` + `docs/getting-started/**` must not contain process content.
- Retired harnesses leave a conclusion in `docs/internal/*spike*.md` behind a **Retired.** notice with the recovery
  command, never a repo copy.

## Release / version authority

- A release is `release/vX.Y.Z.W` -> merge to `main` -> annotated tag on that exact `main` HEAD, with a mandatory
  backmerge; `main` holds accepted released history; `develop` may keep the non-release sentinel.
- `tests/release-readiness/check_release_profile.cmake` validates a releasable profile and is deliberately **not** an
  authorization engine; it refuses only the retired `VERSION_LESS "0.1.0.0"` frontend-per-version family.
- **A gate must verify the contract, never the historical token, file name or wording.** When the architecture moved,
  the readiness gates kept asserting old expressions and reported false failures; assert identities/defaults,
  boundaries and where a thing is **generated**, never human-readable prose.
- `git tag -l` was empty before V0.1; milestones and #30/#31/#32 style issues stay open because the deliverable *is* the
  accepted freeze/tag.

## OpenSSL / transport security rules

- Owner rule for third-party libraries: **use what the user's environment provides; else the project may carry it as a
  trimmable submodule; else the feature is simply unavailable** - the build stays valid and nothing degrades silently.
  Never install a library onto the host.
- **OpenSSL version floor is 4** ("4 or newer", verified against 4.0.2). `HYREMOTE_WITH_TRANSPORT_SECURITY` defaults
  **OFF**; `HYREMOTE_OPENSSL_PROVIDER` = `AUTO|SYSTEM|BUNDLED` (default AUTO). A missing provider is **not** a hard
  failure - it reports one actionable message and disables the capability. Fail-closed lives at **runtime**
  (authentication enabled without the capability refuses to start), not in configure. Bundled source build is not implemented.
- The Qt toolchain's own OpenSSL (`<QtRoot>/Tools/mingw1310_64/opt`, **1.1.1k**) is below the floor and is **rejected**.
- Licensing: Apache-2.0 + `NOTICE.md`; Qt is linked dynamically and unpatched from the user's installation; nothing
  third-party is vendored; OpenSSL is a private implementation dependency from the environment; GPL stays out of the
  product. `hyremote_deploy()` currently does not carry Qt's licence material into the deployed tree (known gap).
- **Authentication configuration surface** (PR #185, `36be21b`): `authenticationEnabled()`, `setAuthenticationEnabled()`,
  `setPassword()` (empty clears), `RemoteAccessErrorCode::AuthenticationUnavailable`; setters rejected unless `Stopped`;
  with authentication enabled `start()` refuses **before composing any component**; no message/log ever carries the
  password. Silent downgrade to `SecurityType None` is forbidden. QML parity rule: invokable `setPassword`, read-only
  `authenticationEnabled`; QPA supports enable/disable only.
- Product posture meanwhile: binding a **non-loopback** address must **refuse to start** unless authentication/encryption
  is enabled (loopback default unaffected).

## Hosted CI lanes and cadence (authority: `.github/scripts/resolve-ci-scope.py --self-test`)

| Lane | Trigger | Content |
| --- | --- | --- |
| `PR_DRAFT` | draft PR | governance only; no Qt runner |
| `PR_FAST` | ready PR | affected capabilities + the evidence its real `base...head` diff can affect |
| `DEVELOP_SENTINEL` | push to `develop` | repository policy only; no product matrix |
| `FULL_GATE` | `workflow_dispatch` (`validation_level: full`) | both platforms, four frontends, all readiness and clean-deploy evidence |

- A `develop` push runs only `Classify changes` + `No product CI check needed`; the merged PR's own hosted acceptance is
  the acceptance. Product jobs need `product == 'true'` **and** a ready PR; `ready_for_review` / `converted_to_draft`
  must stay in `pull_request.types`. `examples/**` and `logo/**` select a capability only for real build inputs.
- `git-flow-policy.yml` refuses a PR whose base is no longer the base's tip - merge the base in and re-run rather than
  merging a stale base.
- Artifacts upload only on failure, a clean SDK/deploy contract, or `FULL_GATE`. CI excludes
  `^hyremote-release-readiness-`, so the release gates are a release-path signal and a green lane says nothing about them.
- **Frozen unless a real blocker appears**: the `cmake/HyRemoteBuild.cmake` zero-test fail-closed
  (`DISCOVERED_TEST_COUNT` / `EXECUTED_TEST_COUNT`) and the `ci.yml` guard against an exclusion matching every test.
- CI time profile (2026-09-20, 17 legs): configure+build ≈52% (Windows ≈2.7x Linux), `aqt` Qt install ≈25% - now cached
  (PR #222, keyed `qt-6.8.3-<arch>-<os>-v1`, aqt only runs on a miss because aqt reinstalls unconditionally).
- **An exact-head hosted run needs an event on the head branch** (a push or a dispatch). Merging a stacked PR into its
  base fires neither, and `gh pr checks` then reports "no checks reported" - which must never be read as "nothing wrong".

## Landing work here (learned by failing)

- **Squash merges break ancestry tests.** Use `git merge-tree --write-tree origin/develop <branch>` (equal tree = nothing
  to land), judge branches by their **PR fact** (`gh pr list --state all --head <b>`), and treat a **positive** feature
  probe as proof while a **zero** probe proves nothing. A branch *behind* develop is not merged.
- **A rename is not finished until every reference is updated in every slash form** - a forward-slash grep cannot see a
  backslash path, and "the old directory does not exist" cannot see a stale *reference*.
- Merge convention: ordinary PRs land as merge commits (`gh pr merge --merge --delete-branch`); a branch that accidentally
  carried junk is squash-merged so the junk never enters history. **After any merge into develop, verify develop itself.**
- When merging develop into a long-lived branch, check **both** sides' facts survive - the English mirror is where an
  integration regression hides, because the primary is what gets read and diffed.
- **A gate that fails may be missing arguments** rather than drifting - check my own call before blaming the tree.

## Shell / tooling pitfalls (Windows host)

- **`replace_in_file` is unreliable in this workspace**: it has reported success without applying, and has truncated
  files mid-content. Verify the edit landed (grep the new symbol) and check brace balance after several edits.
- **PowerShell `>` writes UTF-16.** Re-read such files as `utf-16`; for git blobs compare with `git diff rev:old rev2:new`
  instead of redirecting to a file. `Get-Content ... | Set-Content` on the same file is blocked by the environment guard.
- **Multi-line `git commit -m "..."` is split by the wrapper** (silent failure, then a push of an empty branch). Always
  `git commit -F <file>`; for PRs `--body-file`; keep `--title` free of inner quotes.
- **`gh api -f body=@file` does NOT read the file** (it is literal); use `-F body=@file` or `gh pr comment --body-file`.
  Always read a posted comment's body back - six comments were lost this way, one of them a direct question to the owner.
- Test native exit codes with `$LASTEXITCODE`; piping `--json` output straight into `ForEach-Object` prints
  `System.Object[]` - parse with `ConvertFrom-Json` and index explicitly. `gh api` with PowerShell-hostile `--jq`
  quoting fails; write JSON to a file and parse it.
- **History-rewriting and working-tree-overwriting git commands are blocked by the approval policy** (`rebase --onto`,
  `checkout <sha> -- <paths>`, `git restore`, `Remove-Item` time out; `switch -c`, `cherry-pick`, `commit`, `push`, `gh`
  go through). To withdraw content without rewriting: `git show <sha>:<path>` and write it back with the file tools.
- Long silent commands are cancelled by an **idle timeout** while the workload keeps running - inventory processes and
  output files before retrying rather than re-running blindly.
- **`Copy-Item` keeps the old mtime**, so ninja considers the target up to date; touch the file or clean the target.
- `setvbuf(stdout, NULL, _IOLBF, 0)` is invalid on MSVC (use `_IONBF`) - otherwise the process fail-fasts with `0xC0000409`.
- Python printing non-ASCII dies on the GBK console codepage; keep Python output ASCII. `subprocess.Popen(text=True)`
  decodes with the console codepage and one bad byte kills the reader thread - pass `encoding="utf-8", errors="replace"`.
- Qt GUI apps on Windows log to the **debugger, not an inherited pipe**: set `QT_FORCE_STDERR_LOGGING=1` for a spawned
  process or the harness reads zero lines while the app is alive.
- A driver must write its own phase log to a file (backgrounded runs do not echo stdout), print a timestamped banner
  before each wait, drive waits from the application's own log line, and never be able to hang (`vncdotool` retries and
  `captureScreen` can block; use a hard watchdog and `os._exit` after flushing).
- `git ls-files` lines carry `\r`; set `[Environment]::CurrentDirectory` explicitly before .NET file APIs (`Set-Location`
  does not change it). `git add <path>` fails the whole command when any pathspec has no match - use `git add --all --`.
- `.github/scripts/prune-stale-branches.ps1` cannot run here (no `pwsh`); the scheduled `branch-hygiene.yml` (dry-run by
  default, failing closed) is the executor. `delete_branch_on_merge` is **enabled** (2026-09-20).

## Build, CMake and test environment

- `compile.cmd` is the only build/test entry point (`cmake/HyRemoteBuild.cmake` orchestrates, `build.yml` configures; no
  presets, no `scripts/`). Options: `--integrations=`, `--mode=`, `--tests`, `--run-tests`, `--build-dir`, `--qt-prefix`,
  `--clean`, `-jN`, `--xvfb`, `--test-exclude=REGEX`, `-v`, `--print-config`. Defaults: Release/Ninja,
  `integrations: [cpp, qpa]`, examples true, tests false, `HYREMOTE_DEFAULT_PORT=5921`.
- **Exactly one `build/` locally** (git-ignored) for every mode and log; switching mode needs an explicit `clean.cmd`, and
  `compile.cmd` refuses to mix modes in one directory. Workflows may use their own `--build-dir` on ephemeral runners.
- **`ctest` needs Qt's `bin` and every build-tree DLL directory on `PATH`** (mining `*HyRemoteRemoteAccess.dll`, but
  **excluding** deployed copies under `<build>/evidence/`, which carry their own plugins and shadow the build tree), plus
  `QT_PLUGIN_PATH=<build>/plugins` for QPA smokes. Without it, 42 of 83 tests "fail" with `0xc0000135`/`0xc0000602`.
- `while(TRUE)` is **inert** in `cmake -P` script mode (no CMP0012) - a gate written that way checks nothing. Use a
  variable condition. `CMAKE_CURRENT_BINARY_DIR` is empty in script mode, so pass scratch roots explicitly from the
  CTest registration. `string(REPLACE ... ";")` breaks prose scanning (`;` appears in sentences) - use FIND/SUBSTRING.
  Workflow tokens containing `${...}` must be **bracket arguments** in CMake.
- `add_test` registrations live in the owning subdirectory's test CMakeLists - grep the whole tree before calling a gate
  unregistered. `-DNAME=$var` does not expand in PowerShell; quote it or use `$ENV{NAME}`.
- Read and write files in bulk as **bytes**: `read_text`/`write_text` apply newline translation and rewrite every line
  ending (the index is LF even where the working copy is CRLF).
- Qt-linked binaries need `<QtRoot>/bin` on `PATH`; a Qt-less Core-only consumer build is legitimate and must stay valid.

## Key project facts

- Repository `skawu/HyRemote`; Apache-2.0 + `NOTICE.md`; product name **Qt Remote Access Framework** (never "Embedded");
  V1 reference platforms **Windows x86_64 + Linux x86_64**, embedded Linux/EGLFS/OpenGL ES is post-V1 expansion.
- Core (`src/core`) invariants: storage-anchored frame lifetime, `Completion`/`Render`/`Presentation` PTS, `FrameId` in
  acceptance order, tri-state damage, mailbox bound `capacity + 1` with in-flight admission counting, and one dispatch
  worker as the only `Transport::enqueueFrame()` caller. Its lifecycle claim/run-generation serialisation must stay atomic
  (two review rounds settled it). Adapters implement `CaptureSource`/`Transport`/`InputSink` over public headers only;
  `SessionStats` is the intended diagnostics surface.
- **Capture conclusions** (SPIKE-01 / #16, kept as records in docs): the async grab target is
  `QQuickWindow::contentItem()` (a root-item grab misses overlay/popup siblings); `grabToImage()` returns a null
  `QSharedPointer` with no callback for invalid geometry/hidden windows; pipelining returns a *newer* image than the
  request (never call it staleness); take the PTS at completion, not at request; the owner bound is `capacity + 1`; and
  **no GL/PBO/RHI work is justified** without a per-platform frame budget that does not exist yet.
- Never conflate `qvnc` (the in-tree `-platform vnc` replacement backend) with **Qt VNC Server** (a separate commercial
  add-on). HyRemote **preserves** the qualified native delegate and layers remote access on top - that is the defining
  difference, and E4 is its proof. Claims about `qvnc` internals are general descriptions, not repository research.
- #109 physical acceptance: the Windows cells are partly taken; still owed are the held-input explicit-stop and
  return-to-view-only step, a bounded slow-viewer observation, and a DPR interaction (this host is a single 1920x1080
  display at DPR 1.00 - a simulated `QT_SCALE_FACTOR` must not be used to satisfy a physical cell). The **Linux column
  is unverified, not waived**; a VM-sourced cell must say "VM virtual display" and be ruled on by the acceptance authority.
- Real-world application matrix (#134): starts with the L1 lanes (qBittorrent, DB Browser); L2/L3 as dependencies arrive,
  never GA blockers; fetched at a recorded commit, never vendored, and never modified to accommodate a candidate.

## HyRemote operational conventions (durable)

- Release train: `release/vX.Y.Z.W` -> `main` (PR) -> annotated tag on the exact main head -> GitHub Release -> only
  THEN `backmerge/vX.Y.Z.W` -> `develop`. The Git Flow policy triggers only on PRs into `main`/`develop`, and a
  `backmerge/*` head is refused until the annotated tag exists on main (it validates released history).
- `develop` keeps the `0.0.0` version sentinel; a backmerge restores it and never carries the release version across.
- `gh pr merge --match-head-commit` needs the full 40-character SHA.
- On Windows, write helper scripts with the file tool: bash heredocs are not PowerShell syntax, `${...}` in
  double-quoted one-liners is interpolated, an apostrophe ends a single-quoted here-string, and quotes get stripped
  from inline python.
- The single build entry point is `build.cmd` (self-healing identity, quiet by default, `--show-config`/`-v` for the
  full diagnostic dump).
- Assert the required CI checks before merging; the merge command must not be what discovers them.
- Release assets for v0.2.0.0 are kept at `.codebuddy/release/v0.2.0.0/` together with `SHA256SUMS`.
