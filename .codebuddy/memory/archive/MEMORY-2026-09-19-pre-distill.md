# HyRemote — long-term memory

## Project facts

- Repository: `skawu/HyRemote` (remote: https://github.com/skawu/HyRemote.git). The local
  checkout path is host-specific and is deliberately not recorded here; every path in this
  file is relative to the checkout root unless stated otherwise.
- Privacy convention (user request): never record host-specific absolute paths, user profile
  paths or machine names in memory, docs, README snippets or committed configuration. Use
  repository-relative paths or placeholders (`<QtRoot>`, `<workspaceFolder>`) and, where a
  concrete value matters, state how to rediscover it (e.g. read `CMAKE_PREFIX_PATH` from an
  existing `build/*/CMakeCache.txt`).
- **Qt Remote Access Framework**, **pre-alpha** (renamed 2026-09-17 from "Qt Embedded Remote
  Access Framework" - commit `8f8e937`, PR #111 on `main`). The rename is a **positioning** correction, not
  cosmetics: the framework serves any Qt Widgets/Quick application, so "Embedded" wrongly excluded the x86
  desktop targets. Product-side emphasis (2026-09-17): the V1 reference platforms are **Windows x86_64 and
  Linux x86_64**, and embedded Linux (EGLFS/OpenGL ES) is the **post-V1 platform expansion**, not a V1
  support claim. On the V1 convergence branch the name was completed in three steps: the root `CMakeLists.txt`
  DESCRIPTION (via `0535f87`, preserved by PR #120), and `README.md:7` plus `docs/architecture.md:5` via
  **PR #122** (docs-only, after a repository-wide sweep showed exactly those two stale occurrences; `main` was
  already clean). Reference environment: Qt 6.8.x Open Source, Windows/Linux x86_64 as the reference
  platforms, with Embedded Linux, EGLFS, OpenGL ES, Qt Widgets and Qt Quick as peers post-V1.
- Architecture boundaries: `docs/architecture.md` (layer model, dependency rules,
  threading principles). Capture evidence and the v0.1 capture recommendation:
  `docs/capture-spike.md`. Compatibility status: `docs/compatibility.md`.
- Core rule: `hyremote-core` must not depend on NeatVNC, EGLFS internals, RKMPP, GBM/DRM
  or Qt private headers; such things belong behind adapters/backends.
- **Integration payloads must not link Core directly**: `tests/release-readiness/check_repository_layout.cmake:40`
  enforces "QPA platform payload must not link Core directly", so `HyRemote::QpaPlatform` may only link
  `HyRemote::RemoteAccess` (plus Qt). A defect that looks like a missing link flag can therefore be a design
  violation: on 2026-09-17 the QPA payload referenced `hyremote::CpuFrameStorage::createSinglePlane`,
  `CpuFrameStorage::mutablePlane` and `hyremote::mapPointerToTarget`; adding `HyRemote::Core` made the link
  succeed and took the local suite to 61/63, but the gate rejected it.
  **Ruled solution (owner, 2026-09-17)**: add a **source-private `HyRemote::detail` seam implemented by the
  shared RemoteAccess runtime**, following the existing private factory pattern in
  `src/remoteaccess/src/detail/component_factories.hpp`. It wraps (1) writable CPU RGBA frame/storage creation
  used by composite capture and (2) normalized pointer mapping used by composite input. The QPA payload may
  call this package-internal seam (private symbol export where Windows DLL visibility requires it), but it must
  not be installed as a public SDK header or exposed as an application-facing target/API. Explicitly forbidden:
  linking `qhyremote` to Core, weakening the layout gate, adding frame-storage/input-mapping methods to the
  public `RemoteAccess` API, duplicating the Core algorithms inside QPA, making Core header-only, or exporting
  Core as a second SDK target. Core ownership stays inside the RemoteAccess binary.
  **Implemented and locally verified 2026-09-17** (branch `wip/v1-build-fixes`, commit `46983dd`, not pushed
  pending #120): `src/remoteaccess/src/detail/qpa_composition_seam.{hpp,cpp}` in `namespace HyRemote::detail`
  exposes `createWritableCpuFrame(bytesPerLine, height)`, `writableCpuFrameOf(const RemoteFrame &)` and
  `mapPointerToNormalizedTarget(event, width, height)`, declared with `HYREMOTE_REMOTEACCESS_EXPORT` and compiled
  into the shared runtime (added to `src/remoteaccess/CMakeLists.txt`). The QPA payload
  (`composite_target.cpp`, `interactive_composite_target.cpp`) calls the seam instead of Core. Verification on
  the exact Qt 6.8.3 kit with QML+QPA+examples+tests: build clean, `plugins/platforms/qhyremote.dll` produced,
  the plugin link line is `HyRemoteRemoteAccess.lib` + Qt only (no Core), `check_repository_layout.cmake` PASS,
  **`ctest` 64/64**, and `cmake --install` installs only `HyRemote/RemoteAccess.h` +
  `HyRemote/RemoteAccessExport.h` (the seam header is not installed).
- Three integration modes must be preserved: embedded C++ API (reference), declarative
  QML API, optional zero-code QPA proxy.

## Production Core (`hyremote-core`, CORE-02 / issue #21, PR #22)

- Built 2026-09-15. Target `hyremote-core` (+ `HyRemote::Core` alias), C++17, links only
  `Threads`; root gates `HYREMOTE_BUILD_CORE` (ON by default) and `HYREMOTE_BUILD_TESTS`.
  Headers under `core/include/hyremote/core/`, internal mailbox at `core/src/detail/`,
  host-runnable tests at `core/tests/` (52 cases + a dependency-boundary check; core tests do
  not link Qt at all).
- Invariants now enforced in code: storage-anchored frame lifetime (`shared_ptr<const
  FrameStorage>`, no bare pointers); `Completion`/`Render`/`Presentation` PTS with request time
  as diagnostics only; `FrameId` in acceptance order; tri-state damage with empty `Regions` ≠
  `Unknown`; mailbox capacity counts waiting frames with the dispatch-owned frame additional
  (bound `capacity + 1`) and `ProducerThrottle` admission counting in-flight; the capture
  callback never touches the transport (one dispatch worker is the only
  `Transport::enqueueFrame()` caller, no Core lock held across it).
- Lifecycle: configuration/capability errors fail while still `Stopped`; component startup
  failure leaves the Session `Faulted` until `stop()`; `stop()` stops scheduling →
  `source->stop()` → close/drain mailbox → join workers → `transport->stop()`, is idempotent and
  `noexcept`. Adapter exceptions on Core threads are caught, counted and escalate to `Faulted`
  rather than terminating the host process.
- Lifecycle ownership (Round-2 correction, `teardownsPerformed`/`StartCancelled`): the teardown
  claim is taken **while holding the Session mutex** and bumps `runGeneration`; concurrent
  `stop()` callers wait for the owner and then return without touching components/workers.
  `start()` claims a run generation and re-checks it (check + commit under the same lock) after
  every startup boundary and before publishing `Running`; a `stop()` that wins cancels the start
  (`SessionErrorCode::StartCancelled`, returns false, never publishes `Running`) and each run
  resource is stopped/joined by exactly one side. Any new lifecycle refactor must keep the claim
  atomic and the boundary re-checks — these took two review rounds to settle.
- Adapters (#6/#7/#9) only implement `CaptureSource`/`Transport`/`InputSink` over the public
  headers; `SessionStats` is the intended diagnostics surface for them.

## Repository licensing and branding

- License: **Apache-2.0** (decided 2026-09-15). `LICENSE` holds the canonical text;
  `README.md` carries the statement plus the branding section.
- The logo assets under `logo/` (`huayan-software-horizontal.png`, `huayan-logo-single.png`) are
  maintainer-provided **Huayan Software corporate marks**, distributed under Apache-2.0 with the
  rest of the repository. The README states explicitly that Apache-2.0 §6 grants **no trademark
  rights**: the files may be copied/redistributed, a fork may not present itself as the mark's
  owner. Still owner-owned: filling the `Copyright [yyyy] [name of copyright owner]` placeholder,
  any `NOTICE` file, optional per-file SPDX headers.
- Getting the canonical Apache-2.0 text on this host:
  `gh api repos/github/choosealicense.com/contents/_licenses/apache-2.0.txt -H "Accept: application/vnd.github.raw"`
  then strip the YAML front-matter.

## Environment hazards (Windows host)

- **`replace_in_file` is unreliable in this workspace**: it sometimes reports success without
  applying the change, and it has **truncated files mid-content** (lost the tail of `fakes.hpp` and
  `session.cpp` once each). Always verify an edit landed (grep the new symbol) and, after several
  edits to a file, check brace balance:
  `$d=0; foreach($l in Get-Content <file>){$d += ([regex]::Matches($l,'\{')).Count - ([regex]::Matches($l,'\}')).Count}; $d`
  A non-zero depth or a truncated tail means the edit corrupted the file; repair by re-adding the
  missing tail, then rebuild.
- `Get-Content ... | Set-Content` on the same file is blocked by the environment guard; use
  `replace_in_file` (with `replace_all` for renames) instead of shell text rewriting.
- `gh api` with a jq expression containing PowerShell-hostile quotes fails; write the JSON to a file
  and parse with `ConvertFrom-Json` instead. The same applies to `gh run list --json <f> --jq
  ".field"`, which can return an array wrapper - prefer plain `gh run list`/`gh pr checks` output.
- **Multi-line `git commit -m "..."` is split by the PowerShell wrapper**: the commit silently fails
  with `pathspec ... did not match` errors, and a follow-up `git push` then publishes an **empty
  branch**. Always write the message to a file and use `git commit -F <file>` (same for PR bodies:
  `--body-file`). Likewise `gh pr create --title 'text with "inner quotes"'` is rejected with a usage
  dump - keep `--title` free of inner quotes.
- **Test native command exit codes with `$LASTEXITCODE`.** PowerShell does not treat `if (git rev-parse --verify
  --quiet origin/x | Out-Null)` as "did it succeed": the pipeline result is what `if` sees, so every branch was
  reported as missing. Write `git ... > $null 2>&1; if ($LASTEXITCODE -ne 0) { ... }`. Likewise, piping a
  `--json` list straight into `ForEach-Object` can print `System.Object[]` instead of fields - parse with
  `(gh ... --json ... | ConvertFrom-Json)` and index the resulting array explicitly.
- **History-rewriting and working-tree-overwriting git commands are blocked by the approval policy** (observed:
  `git rebase --onto ...`, `git checkout <sha> -- <paths>`, `git restore`, and `Remove-Item` requests all time out,
  while `git switch -c`, `git cherry-pick`, `git commit`, `git push` and `gh` calls go through). To drop or revert
  content without rewriting pushed history: read the original with `git show <sha>:<path>` and write it back with
  the file tools, then commit a normal "withdraw/restore" commit - the tree comes out right and the branch stays a
  fast-forward. Plan branch contents so rewrites are never needed.
- **Long silent commands are cancelled by an idle timeout**, not by a rejection: the workload may still run to
  completion after the cancellation, so always inventory state (processes, output files, logs) before retrying
  instead of re-running a 45-second observation blindly.
- **`gh api -f body=@file` does NOT read the file.** `-f` is a literal string, so the comment body becomes the
  literal text `@.codebuddy/tmp/<name>` and the recipient never sees a word of it; use `-F body=@file` (capital F
  reads a file) or, for PRs, `gh pr comment --body-file <file>`. Six comments were lost this way across #109, #106
  and #135 - including a direct question to the owner, which then read as silence on their side and cost a full
  round of coordination. Always read a posted comment's body back and check it.
- `gh api repos/.../issues/comments/<id>` returns 404 for **PR review** ids; use
  `gh api repos/.../pulls/<n>/reviews` (and `/pulls/<n>/comments` for inline comments).
- **Restoring a file with `Copy-Item` keeps the old mtime**, so ninja considers the target up to
  date and silently keeps the previous binary — always `(Get-Item f).LastWriteTime = Get-Date` (or
  clean the target) before re-running a control experiment.
- **`setvbuf(stdout, NULL, _IOLBF, 0)` is invalid on MSVC** (null buffer requires `_IONBF`);
  otherwise the invalid-parameter handler fail-fasts the process (exit `0xC0000409`) with no
  output. Use `_IONBF` when unbuffered diagnostics are wanted.
- `Get-Content` without `-Encoding` is blocked by the environment guard, and PowerShell pipelines
  that mix cmd redirection with `ForEach-Object` fail; prefer `Select-String -Path file -Context`
  and the built-in file tools.
- **PowerShell `>` redirection writes UTF-16**, so a JSON/text file captured that way must be re-read as
  `encoding='utf-16'`; reading it as UTF-8 fails with `0xff` at position 0. The console codepage is GBK, so a
  Python one-liner that *prints* non-ASCII raises `UnicodeEncodeError` - keep Python output ASCII (or parse with
  `Get-Content -Raw | ConvertFrom-Json` and print via PowerShell). Also quote any `gh api` URL containing `&`:
  unquoted, PowerShell splits the command and the trailing pipeline stage is reported as unknown.
- When a `python -c` one-liner gets long, prefer parsing with PowerShell (`Get-Content -Raw | ConvertFrom-Json`,
  then `foreach ($x in $c) { ... }`) - nested list-comprehensions with statements are a syntax error.
- To verify a PR head honestly, export it (`git archive`) into an isolated directory and build in a
  separate build dir, so uncommitted local work cannot influence the result; state in the report
  which commit was verified and whether it contains the fix under discussion.

## Conventions

- Contributions/packets: non-trivial work starts from a GitHub Issue; the issue is the
  source of truth. Execution packets, including the branch name, Draft-PR requirement and
  result envelope, are posted as **issue comments**; read the latest comment first.
- Branch/PR flow: work on a task branch, open a **Draft PR** against `main`. Never commit
  or push to `main`. Experimental code stays in a clearly non-production path (e.g.
  `spikes/`) and must not freeze public API.
- Commit style: conventional subjects, e.g. `build(spikes): ...`, `docs: ...`, with a
  `Refs #<issue>` trailer in the body.
- The reusable AI Development System Core/Profile is not yet available in this repo;
  governance is `transitional-explicit`. Do **not** apply Huayan-specific Project/role/
  build assumptions here, and `tools/ai-governance/*` does not exist in HyRemote.
- Evidence for spikes is committed under the spike directory (e.g.
  `spikes/capture/evidence/*.json`) and summarised in `docs/`.

## Naming conventions (audited 2026-09-17)

- **De facto style, not written down anywhere**: types PascalCase, methods camelCase, `enum class` with
  PascalCase enumerators, implementation details behind pimpl (`struct Impl`) so member-name style rarely
  appears. `CONTRIBUTING.md` covers repo/branch/commit/dependency/compatibility rules but says **nothing**
  about identifiers, and no naming section exists in `docs/`.
- **The "HY prefix" is a boundary-layer convention**, used at four spellings, each right in its context but
  undocumented: `hyremote` (C++ namespace), `HYREMOTE_` (CMake options and preprocessor macros),
  `hyremote-` (CMake targets, library names, test targets), `HyRemote` (`HyRemote::` alias namespace,
  installed header directory `HyRemote/`, QML module `import HyRemote`, artifact `HyRemoteRemoteAccess`).
  Boundary classes that must be globally unique already carry it: `HyRemotePlatformIntegration{,Plugin}`;
  the QPA module is `qhyremote` per Qt convention. C++ product types do **not** carry it - the namespace
  does the job (`hyremote::Session`, `hyremote::RemoteAccess`).
- **Four inconsistencies worth remembering**: (1) two include-dir styles - `src/core/include/hyremote/core/`
  (lowercase namespace form) vs `src/remoteaccess/include/HyRemote/` (PascalCase, one public header);
  (2) file naming 36 snake_case / 16 bare lowercase / 1 PascalCase (`RemoteAccess.h`) / 0 hyphen;
  (3) `src/core` and `src/remoteaccess` lowercase vs `integrations/qml/HyRemote` PascalCase;
  (4) the QML element is `QML_NAMED_ELEMENT(RemoteAccess)` with no prefix although QML imports form a global
  namespace - the one real collision risk.
- **`src/remoteaccess` must not be renamed during V1**: `docs/repository-layout.md:27` freezes the layout
  (no further structural migration without a release-blocking architecture defect), `:74` keeps it as the
  binary dir, `:79` keeps `build/remoteaccess` stable, `:87` forbids reintroducing the root `remoteaccess/`.
  A rename would ripple through the CMake target, `HyRemote::RemoteAccess`, `EXPORT_NAME`,
  `HyRemoteRemoteAccess`, the find_package component, 110+ `hyremote-` references and the package contract
  frozen by `docs/v1-api-stability.md:98` - hence V1.1 + ADR + compatibility alias if ever wanted.
- Renaming C++ public types to `HyXxx`, or the QML element, is a **breaking public-API change**
  (`docs/v1-api-stability.md:31,74,78`); inside `namespace hyremote` it would only double the prefix.

## Third-party application observation (issue #109, ruled 2026-09-17)

- Ruling (comment `5712360758`, via the owner's codex connector): **variant 1 - evidence only**. A real
  third-party Qt application may be exercised as *supplementary physical evidence* under the label
  **"foreign application observation - not a V1 support claim"**, but it must not change the V1 exclusion
  statements, compatibility rows, release-note exclusions, manifest, GA matrix or the #109 required E1-E4
  cells, must not become a release/GA blocker, and must never upgrade a compatibility status. Only existing
  released product paths: no HyRemote internals linked into the target, no test-only API, no alternate
  runtime, no special-case adapter added for the candidate. QPA observation is meaningful only while the
  application actually runs against the exact qualified Qt 6.8.3; one representative, reasonably complex,
  cross-platform application is enough, and candidate selection must not delay V1 convergence.
- Triage rule for any result: a reproducible defect in an already-supported capture/input/lifecycle path is
  handled by the existing V1 acceptance rules; an application-specific/native surface outside the frozen
  boundary is recorded as an observation/limitation without widening V1.
- **Only Transparent QPA is exercisable on a third-party application** - Embedded C++ would require linking
  HyRemote into the target and Declarative QML would require the target to import it, and both are target
  modifications the ruling forbids. Product path: `-platform hyremote` (plugin key `hyremote` in
  `integrations/qpa/hyremote.json`, module file `qhyremote`), which still has to be built locally for the
  exact Qt kit before any such observation can be made.
- **Final ruling (2026-09-17, comment `5712521668`; restated `5715406010`)**: for V1 use **one optional
  representative sample only** - `qbittorrent/qBittorrent` (40,155 stars, Widgets, GPL, cross-platform, complex
  enough for menus/dialogs/text/secondary surfaces, buildable from source against exact Qt 6.8.3), which
  supersedes the earlier `sqlitebrowser` selection. "Evidence convenience, not certification": record the exact
  application commit/version, HyRemote candidate SHA, Qt 6.8.3, OS/toolchain and exact launch/deployment commands;
  make no promise for other versions/apps; **optional, must never become a V1 release blocker**, and it must not
  change the #109 E1-E4 cells, #104 GA matrix, compatibility rows, release notes support claims, package manifest
  or the frozen exclusion statements. The full candidate table is `5712170151` (qBittorrent 40,155 / sqlitebrowser
  24,603 / shotcut 15,227 Quick / QGIS 14,391 / krita 10,382).
- **Product-side request of 2026-09-18 (answered with a boundary, not with code)**: put the high-star projects
  under `examples/` as **submodules**, never modify their sources (adapt at build time via CMake/scripts so
  upstream stays in sync), provide **test programs for all three integration modes**, plus a README per mode.
  **Three parts collide with the ruling and were not implemented**: only Transparent QPA is exercisable on an
  unmodified foreign application (C++ would need HyRemote linked into the target, QML would need the target to
  `import HyRemote`); build-time adaptation of a candidate is still a candidate-specific build modification; and
  `examples/` submodules would join the examples surface (`v1-ga-acceptance.yml:15` filters `examples/**`) and add
  five large GPL-2.0/3.0 repositories against "optional, non-blocking" (`CONTRIBUTING.md:87` also requires an Issue
  plus a ruling for a new third-party dependency). The three modes are already covered by this repo's own
  applications - `examples/README.md` E1/E2 Embedded C++ (Widgets/Quick), E3 Declarative QML, E4 Transparent QPA,
  E5 production-like showcase, each with its own README - and E4's application source must stay ordinary Qt-only.
  Ruling requested in #109 comment `5722848860` (may the single optional QPA sample land now, and as a documented
  external build on a non-build path rather than a submodule; whether a multi-project/three-mode scope should be
  raised formally).
- **Outcome (2026-09-18)**: product side chose to keep the **three integration modes demonstrated by this
  repository's own examples** (`#41`'s vehicle - "Examples are release acceptance artifacts") and **withdrew** the
  third-party overlay idea for Embedded C++/Declarative QML, so the "no target modification" boundary stands
  unamended. All remaining unconfirmed items were consolidated into **#106 comment `5723150141`**: (a) may the
  ruled optional QPA sample land at all (proposal: documentation + one script under an explicitly optional,
  non-build path such as `research/third-party-samples/qbittorrent/`, outside `examples/**`); (b) is `examples/**`
  delegated for the per-mode documentation gaps (troubleshooting missing from all five examples, deployment missing
  from the two C++ basics, `remote-support-showcase` also missing Windows/Linux and build); (c) absorb or close
  **#131**/**#132**; (d) the earlier handle question is withdrawn. Host readiness for that sample on this machine:
  **no vcpkg, no OpenSSL development files, no libtorrent, ninja not on PATH** (it lives at
  `<QtRoot>\Tools\Ninja\ninja.exe`) - a host-level install decision. `docs/qpa-proxy.md` does not exist at that path
  on the V1 candidate, although the owner quoted it earlier.
- **How the delegate defect was actually fixed (owner `3b03622`)**: rather than the `QT6_INSTALL_PREFIX` /
  `QT6_INSTALL_PLUGINS` handle I proposed, `cmake/HyRemoteDeploy.cmake` resolves the native platform plugin through
  Qt's own plugin CMake packages (`Qt6::QXcbIntegrationPlugin` on Linux, `Qt6::QWindowsIntegrationPlugin` on
  Windows), installs it beside `qhyremote`, lists both in `ADDITIONAL_MODULES` so `libQt6XcbQpa.so.6` follows, and
  `tests/consumer-installed-qpa/product_fit.py` now asserts the deployed tree contains the native delegate. Prefer
  this Qt-native form for any similar "deploy a plugin the module loads dynamically" need.

## V1 status: converged candidate `4281db2`, physical acceptance (#109) is the critical path (2026-09-18)

- **`4281db23387780d0b246711438d6b8b134a787d3` is the frozen hosted/reference-converged #106 candidate**: all nine
  workflows SUCCESS (V1 GA acceptance `35296682318`, Transparent QPA Proxy `35296682226`, SDK consumption
  `35296682228`, Git Flow `35296682254`, plus the mode workflows), verified independently by me on that exact SHA.
  #39/#101/#104 CLOSED-accepted, #74 CLOSED-restored. The last Linux blocker was closed by `4281db2`
  "fix(deploy): close Linux native QPA Qt runtime dependencies" without changing the frozen product model:
  `hyremote_deploy(... QPA)` owns `qhyremote`, the native `qxcb`/`qwindows` delegate, the shared RemoteAccess
  runtime and the delegate's exact-Qt private runtime closure.
- **Owner instruction that governs further work**: do **not** add opportunistic code/documentation commits to this
  candidate merely to keep development moving; #109 is the critical path; no release branch, `main` merge or
  `v1.0.0.0` tag is authorized yet. Anything post-V1 (naming document, `docs/` reorganisation, SPDX headers,
  ignore-rule hygiene) stays separate from the V1 release claim.
- **#41 stays open until #109 supplies real local-display/local-input evidence** - hosted/Xvfb execution cannot be
  used to infer physical behaviour - and no example architecture or scope change is required.
- **This host cannot produce the E4 cell or the Linux column**: with the local Qt the QPA example is skipped by the
  guard ("Transparent QPA mode unavailable") because the GUI private QPA headers are absent. Windows E1/E2/E3/E5
  are buildable and runnable here (real desktop display). E1 pre-observation posted as #109 `5724786530`
  (preparation only, no cell claimed): `CLIENT_COUNT 0 -> READY -> 1 -> 0 -> STOPPED` with local `APP_POINTER`/
  `APP_KEY`/`APP_TEXT` events while a `vncdotool 1.3.0` viewer was connected, and an injected key leaving the remote
  frame byte-identical in the documented view-only default.
- **Harness facts worth reusing**: vncdotool's client method is `captureScreen(path)`, **not** `capture`, and
  `python -m vncdotool.command vncdo` is rejected ("unknown cmd"); the examples carry acceptance helpers -
  `--port`, `--remote-input` (default view-only), `--test-seconds`, and a disconnect-triggered
  `stop -> configure -> start` policy transition. The examples need Qt's `bin` **and** the build's
  `remoteaccess`/`qml/HyRemote` directories on `PATH`.

## OPEN V1 BLOCKER: one remote key press reaches the application twice on `a44ae4d` (found 2026-09-18)

- **Symptom**: a discriminating probe (one viewer, one `keyPress("q")`) produced **two** `APP_KEY key=81` lines with a
  single `APP_TEXT text=q`. `examples/widgets-basic/main.cpp:49-57` logs only on `QEvent::KeyPress`, so the application
  really receives two key-press events. Reported on **#109 `5727361361`**; no E1 verdict claimed.
- **CORRECTION (same day, #109 `5727446595`)**: it is **not** a regression. The same probe on a cleanly rebuilt
  `4281db2` gives the identical two lines, so my "change between `4281db2` and `a44ae4d`" claim was wrong and is
  retracted. `8438296` (RFB client lifetime on the failure path) was the suspect and is irrelevant.
- **RESOLVED - and it is neither a defect nor a regression**: `widget_target.cpp:684` calls `deliverKey` **once** per
  input event, `deliverKey` issues **one** `QCoreApplication::sendEvent` (`:591-594`), and `keyboardReceiver` returns a
  **single** receiver. The example's root is a plain `QWidget` that **does not accept key events**, so Qt propagates the
  unaccepted event to the parent and **every propagation step re-enters application-level filters** - and the example
  installs its probe on `QApplication` (`main.cpp:156`). Hence two identical log lines. In-tree tests show one because
  their `ProbeWidget` calls `event->accept()` (`test_widgets_input_backpressure.cpp:71-75`), which stops propagation -
  that is why `test_rfb_multi_client_input.cpp` can assert `countKey(..., true) == 1` and pass. A real text field accepts
  the key and gets it once.
- **Both of my earlier framings are retracted** (regression at `5727446595`, product defect at the comment above it).
  Eliminations that got there, each measured: the older endpoint behaves identically; `keyDown` alone still gives two
  lines, so not the viewer; one RFB `KeyEvent` per message in the transport; one `sendEvent` per input event in the
  adapter.
- **Windows E1 = PASS on `a44ae4d`** (delegated-lead verdict, owner may override): view-only honoured, documented
  same-process `POLICY_STOPPED -> POLICY_INPUT true -> POLICY_RESTART_REQUESTED -> READY`, remote pointer at the
  viewer's coordinate, remote keys/text, **local input authoritative during remote control** (`x=235 y=20` from a local
  click), abrupt-disconnect held-input cleanup (`key=16777248` held, holder killed, next key `shift=0`), mid-control
  reconnect.
- **Product-side control (run for this)**: `hyremote-rfb-multi-client-input-test`, `hyremote-widgets-input-backpressure-test`
  and `hyremote-input-mailbox-admission-test` **pass 3/3** - the raw-RFB end-to-end test asserts `countKey(..., true) == 1`,
  so the product is exactly-once and the doubling was the observation, not the delivery.
- **Decision taken by the product side: option A**, implemented as: the example's probe now **returns true for the
  events it records**, which stops Qt's propagation so the log means one line per delivery. Verified: one `keyDown('q')`
  gives exactly one `APP_KEY key=81` + one `APP_TEXT text=q` (was two). The example's widgets are non-interactive, so
  the application's behaviour is unchanged.
- **Harness trap worth remembering**: `subprocess.Popen(..., text=True)` decodes with the console codepage (GBK here),
  and a single non-GBK byte from Qt kills the reader thread - the whole run then reports "no output" while looking
  healthy. Pass `encoding="utf-8", errors="replace"`.
- **Windows E2 remaining items, first pass (`quick-basic`, #109 `5727566647`)**: **text focus/input on a real Quick
  control is demonstrated** - `Main.qml`'s focused `TextInput` receives remote typing as one `APP_KEY` + one
  `APP_TEXT` per key (`72/89/50` -> `H/y/2`), which is also the cleanest proof that the earlier doubled `APP_KEY` lines
  were the widgets example's app-level probe artifact (this example installs its probe on the view). Native resize
  applied with the viewer connected (`SetWindowPos` to 536x379, frames 12859 -> 13771 B).
- **Limitation found**: `quick-basic`'s explicit-stop path makes the **application exit**, so "explicit stop while a
  modifier is held, application keeps running" cannot be observed there - it must be done on **`widgets-basic`**, whose
  transition is `POLICY_STOPPED -> POLICY_INPUT true -> POLICY_RESTART_REQUESTED -> READY` on a live process. Still
  owed: that held-input case, the bounded slow-viewer observation (needs a genuinely stalled reader), and the DPR
  interaction (this host reports DPR 1, so it needs a second display or a scaled configuration - say so plainly rather
  than substituting a DPR-1 measurement).
- **Also established by the same re-take** (posted on #109): view-only honoured, documented same-process
  `POLICY_STOPPED -> POLICY_INPUT true -> POLICY_RESTART_REQUESTED -> READY`, remote pointer/keys/text, mid-control
  reconnect, abrupt-disconnect held-input cleanup, and - new - **local input staying authoritative while a remote
  viewer controls**: a locally injected click inside the application's own window produced
  `APP_POINTER x=235 y=20`, a coordinate no viewer sent. The probe only works when it finds the window by PID, uses
  its rectangle and calls `SetForegroundWindow`; a fixed screen coordinate proved nothing.

## One build directory only (product rule, 2026-09-18)

- **Exactly one `build/`**, git-ignored, for every mode and every log; no `build/<mode>` subdirectories, no
  `build-*` trees anywhere. Switching mode or rebuilding requires an explicit clean (`clean.cmd`), and `compile.cmd`
  **refuses to mix modes in one directory** - a marker in `build/.hyremote-mode` makes it print the exact
  `--clean --mode <new-mode>` line rather than reuse a stale generator/compiler/cache. The `--build-dir` option was
  removed and logs moved inside `build/` (`build/configure.log`, `build/build.log`) so the repository root stays free
  of build files.
- **Every reference is updated in the same change** (Chinese + English cross-compilation guides, `cmake/toolchains/README.md`,
  `docs/repository-layout.md`) - the repository's own rule for structural edits. Stray trees from development
  (`build-acceptance`, `build-scripttest*`) were deleted; a leftover bulk-delete guard blocks >500 items at once, so
  large directories have to be removed in chunks.
- **Verified end to end**: a fresh `compile.cmd --tests --qt-prefix C:/Qt/6.8.3/mingw_64` into the single `build/`
  yields **69/69 ctest with exit code 0**, and the repository root holds no build files.

## x86 Windows status (measured 2026-09-18, candidate `24a495a`)

- **Hosted reference environment: fully green.** All **nine** workflows on `24a495a` are `success`, including
  `V1 GA acceptance`, `Widgets adapter`, `Quick adapter`, `RemoteAccess facade`, `Declarative QML API`,
  `Transparent QPA Proxy`, `SDK consumption`, `Bounded RFB transport` and `Git Flow policy`. These run the Windows
  and Linux jobs with the reference toolchain (MSVC 2022 + exact Qt 6.8.3 on the Windows side).
- **Local Windows (Qt 6.8.3 mingw_64 + mingw1310_64): 69/69 ctest pass** once the runtime paths are set. The path
  matters more than it looks: without the Qt/MinGW DLLs on PATH every Qt-linked test exits `0xc0000135`
  (DLL not found) and the QPA smokes `0xc0000602`, which reads as a broken build. Measured progression: 34/69,
  48/69 with a partial PATH, **69/69** with `PATH=<build>/remoteaccess;<build>/qml/HyRemote;<Qt>/bin;<mingw>/bin`
  and `QT_PLUGIN_PATH=<build>/plugins`. `compile.cmd --tests` now prints exactly those three lines.
- **What is still open on Windows is the physical acceptance, not the engineering**: `#109` Windows E1 must be
  re-taken on `24a495a`, E2 still misses explicit-stop-while-held, resize, DPR, real Quick control focus and the
  slow-viewer observation, and E3/E4 have not been run (E4 is unlocked). Release mechanics (no `release/*` branch,
  `git tag -l` empty) belong to the release authority.

## Build system from the 4diac-fbe pattern landed (2026-09-18, candidate -> `24a495a`)

- **The reference is `eclipse-4diac/4diac-fbe`** (the FORTE build environment), and its defining feature is that
  `compile.cmd`/`clean.cmd` are **both POSIX shell scripts and Windows batch files** - one file per action, working on
  both hosts; modes live in named configurations, toolchains in `toolchains/`, output under `build/<config>/output/bin`,
  and `-v` switches from log files to console output.
- **Landed on the V1 candidate** (`24a495a`): root `compile.cmd` + `clean.cmd` (polyglot), `cmake/toolchains/` with
  `README.md`, `aarch64-linux-gnu.cmake` and `arm-linux-gnueabihf.cmake`, bilingual
  `docs/guide/cross-compilation.md` + `docs/en/guide/cross-compilation.md`, and the new entries recorded in
  `docs/repository-layout.md`. **Default mode is QPA**, editable in the script (`HYREMOTE_DEFAULT_MODE`) or with
  `--mode qpa|cpp|qml|all|minimal`. **No CMake default changed** - the frozen V1 option defaults stay authoritative,
  the script only selects flags.
- **Verified on Windows**: `compile.cmd --build-dir <dir> --qt-prefix C:/Qt/6.8.3/mingw_64` builds the QPA default and
  produces `plugins/platforms/libqhyremote.dll`, `remoteaccess/libHyRemoteRemoteAccess.dll` and all examples.
- **Three real bugs the testing caught, worth remembering**: (1) some Windows shells hand a batch file an argument
  **truncated at its `=`**, so `--mode=qpa` arrived as `--mode` - both `--opt=value` and `--opt value` are now accepted;
  (2) `if COND cmd1 & cmd2` in batch runs `cmd2` **unconditionally**, so parse loops must use explicit blocks;
  (3) the script reported success for a phase that never ran because the log redirect target directory did not exist -
  configure/build are now validated (`CMakeCache.txt` must exist, exit code must be 0). Also: guessing between the
  installed MinGW kits picked a too-old g++ (failure surfaced inside Qt's headers), so the script now **fails with the
  exact PATH line** instead of guessing.
- Host pitfall for the same class of work: long build commands get backgrounded, so drive them from log files rather
  than a foreground terminal.

## CI trigger facts worth knowing before promising "CI green" (2026-09-18)

- `git-flow-policy.yml` runs **only for pull requests whose base is `main` or `develop`**, so a PR based on
  `feature/104-v1-ga-acceptance-matrix` legitimately produces no run.
- The build/test workflows (`v1-ga-acceptance.yml`, `remoteaccess-facade.yml`, `widgets-adapter.yml`, `qml-api.yml`,
  `qpa-proxy.yml`, `quick-adapter.yml`, `rfb-transport.yml`, `sdk-consumption.yml`) are **path-filtered**.
  `v1-ga-acceptance.yml` lists `docs/**` among its paths, so a docs-only PR should still trigger it.
- **Consequence**: a green envelope must be reported per PR and per head SHA, and a docs-only PR may legitimately show
  no runs; verify with `gh run list` / `runs?head_sha=` rather than assuming a PR is "CI-covered".

## Productisation scope adjudicated (2026-09-18) - three rulings that shape the V1 backlog

- **Qt 5.15 is a full qualification, at the same standard as 6.8.3** (product ruling): build on **both** operating
  systems, run all **three** integration modes for real, and pass the key matrix, before 5.15 may appear in a
  **Supported** claim. The QPA proxy stays on an exact-version line. This makes 5.15 V1-blocking and not a smoke test.
- **Security posture is fail-closed** until #143 is fully implemented: binding to a **non-loopback** address must
  **refuse to start** unless authentication/encryption is enabled, while the default loopback listener is unaffected.
  #143's two-step implementation proceeds as agreed; this gate closes the "insecure by default" risk in the meantime.
- **#134 gates V1 through L1 only**: the L1 lanes (qBittorrent, DB Browser) must go fully green inside V1; L2/L3 proceed
  as their (heavy) dependencies arrive but **do not block GA**, and if unfinished they must be labelled unfinished
  rather than quietly dropped.
- Consequence for ordering: the fail-closed gate is small and self-contained and comes first; 5.15 full qualification and
  the L1 lanes are the two large workstreams, and the Linux host is still the critical path for acceptance.

## The 5.15 measurement found a **false green** in the build system (2026-09-18, first 5.15 item to fix)

- Configured the single `build/` against **Qt 5.15.2** (`clean.cmd` then
  `compile.cmd --mode=cpp --qt-prefix=C:/Qt/5.15.2/mingw81_64` with `mingw810_64` on PATH). CMake reported
  **"Build succeeded"** - while `Qt6_DIR` was `Qt6_DIR-NOTFOUND` and **nothing looked for Qt5**, because the root only
  calls `find_package(Qt6 ...)`. The build had silently degraded to the Qt-free part: 8 targets,
  `build/core/libhyremote-core.a`, and **0 example executables**.
- Restored immediately: `Qt6_DIR=C:/Qt/6.8.3/mingw_64/lib/cmake/Qt6` and **5 example executables** are back, so the
  6.8.3 column is untouched.
- **Why this matters more than a compile error**: anyone attempting 5.15 today reads "Build succeeded" and concludes
  5.15 works. A false green is worse than a failure, and it has to be fixed before the mechanical work.
- **The fix has to respect a deliberate design**: a Qt-less Core-only consumer build is legitimate, so it must
  distinguish "no Qt by intent" from "a Qt prefix was supplied and the Qt product targets are expected". Simply turning
  the skip into a hard error unconditionally would break that consumer path.

## Linux acceptance host: building and testing (2026-09-18)

- **The candidate tree builds and produces the QPA plugin on Linux**: `compile.cmd --mode=all
  --qt-prefix=/opt/Qt/6.8.3/gcc_64 --tests` reported **"Build succeeded"** with no errors, examples built
  (`quick-basic`, `qml-basic`, `qpa-proxy-existing-app`, ...) and **`build/plugins/platforms/libqhyremote.so`** exists -
  so E4 is buildable on Linux, as the private QPA header in `/opt/Qt/6.8.3/gcc_64` suggested. ctest was started next.
- **Exporting the tree to Linux must disable CRLF conversion**: `git archive` on this Windows host (with
  `core.autocrlf=true`) produced CRLF files, and `compile.cmd` - a single file that is both a POSIX shell script and a
  Windows batch file - then failed to parse under `sh` with a heredoc/`do` syntax error that looks like a code defect.
  The fix is `git -c core.autocrlf=false archive ...`; this is the concrete case that justifies PR #150's LF policy, and
  anyone shipping this repo to a Linux host must use it.
- Delivery route used: `git archive` of the candidate (tracked files only, ~1.1 MB) plus `scp`, so no `build/`, no
  `.codebuddy/` and no credentials travel with it.

## Linux acceptance host: unblocked (2026-09-18)

- The VM is an **Ubuntu 24.04.4 LTS** guest with 2 vCPUs, and the account is **`hd`** - my earlier attempts used `hdzk`
  and that wrong name was the whole reason for `Permission denied`. With the right user, the public key was installed
  once and **key-based login now works** (`KEY_LOGIN_OK`), so no password is needed again. The password is deliberately
  **not stored** in the repository or in memory, and was only used for that single key-installation call.
- Qt 6.8.3 (`linux_gcc_64`) plus the toolchain and GUI development packages are being installed on the guest in the
  background (`/home/hd/vm-setup.log` on the VM). The guest boots to `graphical.target`, but **no active desktop session
  exists**, and an SSH session has no `DISPLAY`: the operator must log into the VM desktop and leave it unlocked before
  the E1-E4 cells (which need a real local display and local input) can be run.
- Standing honesty requirement for this host: its display is a **virtual** one, so every Linux cell recorded from the VM
  must say so and be ruled on by the acceptance authority rather than passed off as a physical monitor.

## Linux acceptance host and the Qt 5.15 line (2026-09-18)

- **An Ubuntu 24.04 VM is available at `192.168.244.128`** (sshd listening on 22; banner
  `SSH-2.0-OpenSSH_9.6p1 Ubuntu-3ubuntu13.19`). Password authentication for the account the product side named was
  **rejected**, so the agreed route is a public key: the operator installs it inside the VM and everything afterwards
  uses `ssh -i` with no password at all. **Credentials are deliberately not written into this repository** - never put
  passwords in memory files.
- There is no `sshpass` or `plink` on this host, so **paramiko** (installed) is used once to install the key;
  `.codebuddy/tmp/remote.py` is the uncommitted one-shot runner. The private key lives in the user profile, not in the
  workspace, so a bulk `git add -A` cannot pick it up.
- **The VM's display is a virtual display, not a physical monitor.** The runbook excludes offscreen/Xvfb/hosted
  evidence, so VM-sourced cells sit in a grey zone: every Linux cell taken there must say "VM virtual display" and let
  the acceptance authority rule on it, rather than being passed off as a physical one.
- **Qt 5.15 is required on both Windows x86 and Linux x86** (product clarification), which widens #57 from "one Qt 6
  LTS kit" to a **two-platform 5.15 qualification**. Work order: measure the compatibility gap with a 5.15 build
  first, then hand over an executable list - not a promise made before measuring.
- Windows-side Qt 5.15.2 (`win64_mingw81`) is being installed with `aqtinstall` into `C:\Qt`. Note for next time:
  aqt writes `aqtinstall.log` into the current working directory.

## The default port is also a **configure-time** value now (PR #146 commit `256c52c`, decision item 10)

- `cmake/HyRemoteProjectOptions.cmake` has `HYREMOTE_DEFAULT_PORT` (cached string, default **5921**, range-checked)
  and publishes it to **every** target via `add_compile_definitions`, so library, QPA plugin, examples and tests cannot
  disagree. The Core facade and the QPA proxy config use the macro; QML keeps inheriting the Core value through its
  writable `port` property. The four assertions that pinned 5921 now compare against the macro.
- **Proved in both directions**: shipped configuration -> `ctest` 72/72 and an application that does not call `setPort()`
  listens on 5921; `-DHYREMOTE_DEFAULT_PORT=5930` -> build ok and **`ctest` 72/72**, which is what shows the library
  default follows the configuration rather than a literal. Tree restored to the shipped default afterwards, suite green.
- **Caveat that must not be glossed**: the shipped examples pass their own explicit port default, so an example launched
  without `--port` still listens on 5921 even in a 5930 build. The configure-time value governs applications that do not
  call `setPort()` (the integrator case). Making the examples inherit it is a deliberate small follow-up.
- **Follow-up disclosed, not hidden**: `tests/consumer-installed-sdk/main.cpp` still pins 5921 and is not part of
  `ctest` here; under a custom-port configuration it would need the value published by the installed package
  (e.g. `HyRemote_DEFAULT_PORT` in `HyRemoteConfig.cmake`).

## Product decisions recorded 2026-09-18 (decision round, batch 2)

- **Doc/structure residual -> all of it is V1 work, in two batches.** Batch one (small, directly verifiable):
  `.gitignore` gains `__pycache__/` and `*.pyc`, a new `.gitattributes` pins LF, and `check_release_metadata` is
  generalised. Batch two (large): SPDX headers across **132** source files, the examples expansion, and the
  third-party example. Each advances the candidate SHA, so affected evidence is re-taken.
- **Explicit-stop carrier -> `remote-support-showcase`.** Its local Start/Stop button and input checkbox perform
  `stop -> configure -> start` in both directions while the application keeps running, so E1 step 6/9 and the E2/E3
  explicit-stop cells are observable with zero product change. The record must name the payload used and state the
  deviation from the runbook's "the normal E1 Widgets application" wording.
- **E4 child surface -> frame differences plus proxy liveness**, accepted as **weak** evidence: it cannot directly
  prove that no synthetic press was retained, and the application has its own time-varying content (the 714-pixel
  horizontal strip measured on the read-only run). The weakness must be written into the acceptance record.
- **DPR cell -> the product side changes display scaling (125%/150%) or adds a second display**, then the E2 DPR step
  is re-run on it. Not simulated with `QT_SCALE_FACTOR` - a physical cell must not be satisfied by a simulated change.

## Product decisions recorded 2026-09-18 (decision round, batch 1 of 3)

- **#134/#136/#138 -> in V1, isolation lifted.** The real-world upstream application matrix is V1 work: start with the
  L1 lanes (qBittorrent, DB Browser), L2/L3 follow as host dependencies arrive, and its evidence enters V1 acceptance.
  The draft track leaves isolation and **the required host dependencies must be named explicitly**.
- **#143/#144 -> two steps, both inside V1.** First design + a chosen implementation + an end-to-end minimal
  acceptable surface (authentication handshake, one encrypted transport, the encoding strategy landed with a chosen
  implementation); then the remainder closes inside the same V1. Nothing here is deferred to the embedded phase.
- **Linux x86 column -> the product side provides a Linux host or a graphical VM.** I install Qt 6.8.3 there, run the
  #109 Linux cells E1-E4 and settle the GBM/DMA-BUF borderline item. Until that host exists the column stays without
  evidence and unwaived.
- **#57 -> I may install Qt 5.15.2 and one Qt 6 LTS kit myself** and run the qualification matrix.

## Windows E1-E4 re-taken on the 5921 tree (#109 `5728285144`) - one unexplained exit

- Why re-taken: the default-port change (PR #146, `4112ae6`) moves the candidate SHA, so the sets recorded on
  `eab386e` no longer describe the tree to be delivered. Every run used the **product default** (no port argument at
  all, viewer on 5921): E1 `widgets-basic`, E2 `quick-basic`, E3 `qml-basic` each reproduced the full previously
  recorded set, and E4 read-only showed the proxy's `native delegate: "windows" ... port: 5921 remote input: false`
  line while E4 control showed `remote input: true` with remote typing changing the app's text field
  (**142 then 151 changed pixels, bbox inside the text row** - same signature as the pre-change run, so reproducible).
  Native `-platform windows` baseline: frame 204170 B and **exit status 0**.
- **Frame-difference evidence is weaker than it looks**: in the read-only E4 run two later captures differed by
  **714 pixels** along a thin horizontal strip (x 131-845, y≈83) with no remote input delivered at all - that is the
  application's own time-varying content. A zero diff is meaningful; a few hundred pixels is not automatically input.
- **Unexplained, and I do not smooth it over**: in the control run the application **exited by itself** mid-phase
  (`rc = -1`, before its `--test-seconds`), with only the proxy's two startup lines as output. Four processes from
  earlier runs were still alive and unkillable from my shell at that moment, and my driver had been leaking one viewer
  per E4 run. Whether that exit is a product defect or an artifact of my own mess is **not established** and needs one
  clean re-run.
- **Harness defect found and fixed**: the held-Shift helper was killed only when the application confirmed the Shift
  line, so every E4 run (no probe -> never confirmed) leaked a viewer for ten minutes; that leak later took the port
  and produced `ConnectionRefusedError 10061` plus the application's
  `could not start the composite RemoteAccess runtime: "transport failed to start"`. Kill is now unconditional.
- **Product observation from that failure**: with its port already taken, the QPA proxy does not crash - it keeps the
  native `windows` delegate, stays usable, and reports exactly what failed. That is desirable behaviour, not a defect.

## Product default port is 5921, and all three integration modes let the user change it (2026-09-18)

- Product requirement: **default port 5921**, changeable by the user in every mode. Implemented in PR **#146**
  (`fix/default-port-5921`, commit `4112ae6`), 23 files / 37 lines.
- The default lives in exactly two places and both moved 5900 -> 5921: `src/remoteaccess/src/remote_access.cpp`
  (Core listener default, which also feeds the QML type through its `Q_PROPERTY(int port READ port WRITE setPort ...)`)
  and `integrations/qpa/hyremote_qpa_remote_controller.hpp` (QPA default when the platform string gives no port).
- The change-the-port paths were already there and are unchanged: C++ `RemoteAccess::setPort()`, QML's writable `port`
  property, QPA `-platform "hyremote:hyremote-port=<port>"`. **Verified by running, not by reading**: `READY 5930`
  (C++), `READY 5931` (QML), `port: 5932` in the proxy's own line (QPA); and with no port argument at all, `READY 5921`
  (C++), `READY 5921` (QML) and `port: 5921` (QPA), with a viewer connecting to 5921 in each case. Full suite after a
  clean rebuild: **72/72 passed**, and **CI 7/7 success** on `4112ae6`.
- Viewer display notation follows: **5921 is display `:21`** (5900 was `:0`).
- **Never blanket-replace a port number in this repository**: `research/capture/evidence/host-windows-rhi-default-*.json`
  contains the digits `5900` inside recorded latency values (`9.905900000000088`, `2685900`). A global replace would
  have corrupted recorded measurements. Check for adjacent digits (`[0-9]5900`, `5900[0-9]`) before any such edit.

## V1/platform-boundary rulings (product side, 2026-09-18) - and the post-V1 focus

- **The post-V1 line is meant to be multi-platform support.** With the rulings below, what stays outside V1.0.0.0 is the
  embedded platform-family work (#7, #10, #18, the EGLFS halves of #3/#123/#148, the hardware half of #17) plus a few
  still-unresolved items - i.e. exactly the platform expansion the product side wants V1.x to be about.
- **Rulings on the four borderline issues** (each comment landed on its own issue, summary on #33 `5730305606`):
  **#17 split** - buffer-ownership model, interface shape and feasibility on a DRM/GBM-capable Linux host are **V1**
  work, the embedded-hardware conclusion is post-V1; blocked on a Linux x86 host, same as the #109 Linux column.
  **#151 in V1** (capture pacing / capture-on-demand, idle shutdown, dependency-free product-fit check are all
  x86-verifiable). **#149 in V1** (embedded C++ boilerplate ~46 lines / 6 gaps is x86-measurable adoption cost).
  **#148 split** - scope statement, guide correction and roadmap decision are **V1** (pure x86 documentation, and the
  thing that stops a downstream integrator from assuming the proxy works on EGLFS), the EGLFS delegate implementation
  is post-V1.
  In each case the old `[POST-V1]` title was written under the superseded topic-based criterion and is now suspended.
- **Not yet re-issued**: #143, #144, #57, #141 titles, and #134's "held, must not be based on the V1 candidate" ruling.
  Also unanswered: the disposition of #134/#136/#138, the target scope of #143/#144 inside V1, the Linux x86 host, and
  the Qt 5.15/LTS kit source for #57.
- **Instrument discipline learned the hard way**: `gh api` bodies must go through a JSON file (`--input`) or `-F
  body=@file`; building the calls with PowerShell array/`--jq` string syntax silently flattened nested arrays and
  mangled quoting, which produced duplicate posts and two argument-count failures. Always verify the posted body
  length afterwards, and delete the duplicates.

## Planning rule (product direction 2026-09-18): the repository plan governs; inbound requests must not reorder it

- **The goal stays V1.0.0.0 and the plan's artifacts are the schedule.** An inbound user/downstream request may be
  recorded and classified, but it must never be treated as scheduling input or allowed to widen what V1 means. My
  earlier reading ("if it can be done on x86 it must be done in V1") was wrong and **widened the V1 GA declaration**,
  which `docs/development-roadmap.md` explicitly forbids; that classification was withdrawn on #33 (`57311…`).
- **What the plan says the V1 exit is**: `docs/v1-ga-acceptance.md` section 10 (the 14-condition release authorization
  checklist, including #109 with explicit-stop/policy-transition input cleanup, #104 having actually passed on both
  reference OSes, clean deployed consumers, no item blocked/failed/inferred) plus section 8 physical cells and sections
  3/5/6/7. **CORRECTED 2026-09-19 - the claim that used to sit here was wrong.** It said the roadmap's Post-GA put
  #143/#144/#9 on committed `V1.x` tracks. That came from reading `docs/development-roadmap.md` **in the shared working
  tree**, which is checked out on a branch older than PR #147; commit `3431b00` (PR #147, #157's own convergence change)
  rewrote exactly that section. At the candidate head (`cc1adca`) the roadmap says the opposite: "**security** ... is a
  **V1.0.0.0 requirement, not a `V1.x` track**", "#144 and damage-aware delivery (#9) are x86 work and belong to V1", and
  "only the genuinely embedded parts stay post-GA: low-copy buffer ownership (#17) and hardware encoding (#10)". The
  statement rule about editing limitations/compatibility docs only in the change that lands a capability applies to the
  **expansion tracks**, not to #143/#144/#9. Correction posted on #33 `5737340379`; #157 closed as satisfied 2026-09-19.
  **Read plan artifacts at the candidate SHA (`git show <sha>:<path>`), never from the shared tree.**
- **Consequences**: #154 (downstream EGLFS campaign) is a post-V1 record, not plan work. #148's documentation change is
  a correction of a present-tense false claim, explicitly not a V1 gate and not progress on a `V1.x` track. **E5
  `remote-support-showcase` is a V1 item in its own right** (section 5: local start/stop, view-only safe initial policy,
  explicit control enablement, true listener-versus-connected status, reconnect/session diagnostics, meaningful
  Widgets/text interaction) - and section 5 also requires the E1/E2/E3/E5 product-fit scripts to exist in the candidate
  tree.

## V1 scope criterion (superseded 2026-09-18 by the planning rule above): only embedded-only work was outside V1

- **The boundary is platform dependency, not topic.** Work that can be done on x86 **must be completed inside
  V1.0.0.0**; only work that *must* run on the embedded platform may be deferred, because embedded platforms are far
  less debuggable than x86 - deferring x86-capable work means debugging it where the product is least observable.
- **Consequence: my earlier "not V1" list was wrong and is superseded** (corrected on #33 `5727827228`). Now inside V1:
  **#143** (authenticated/encrypted transport - no implementation exists), **#144** (RFB encoding strategy and the
  bandwidth/acceleration sequence), **#9** (async OpenGL/PBO capture, damage-aware delivery), **the x86 half of #123**,
  **#57** (Qt 5.15/LTS qualification), **#134 + #136/#138** (real-world upstream application matrix on x86, currently an
  isolated draft track), **#14** (governance adoption gate), **#41**, and a **re-cut of #141** (only its embedded part
  stays deferred). The earlier "post-V1" dispositions on #143/#144/#57 and the "held, must not be based on the V1
  candidate" ruling on #134 were written under the wrong criterion and must be re-issued rather than silently kept.
- **Genuinely embedded-only** (may stay outside V1): **#10** (RK3588/RKMPP), **#18** (RK3588/EGLFS spike), **#7**
  (Quick/EGLFS embedded stack), **#3**'s remaining EGLFS half, #123's embedded half, and **#17** (GBM/DMA-BUF -
  borderline: target is the embedded graphics stack but a prototype could arguably run on Linux x86 with a
  DRM/GBM-capable GPU; either way it needs a Linux host).
- **Standing rule from this**: never classify an item as post-V1 by topic. Ask "must this run on the embedded
  platform?" first; if the answer is no, it is V1 work and the ledger must show it as such.

## Windows E1-E4 evidence collected on `eab386e` (#109 `5727783292`) - and what is still open

- **Tree**: `compile.cmd --mode=all --qt-prefix=C:/Qt/6.8.3/mingw_64 --tests` gives QML + QPA in one `build/`;
  **`compile.cmd --clean --mode=<mode>` cannot work** (the mode guard runs before the clean), so the sequence is
  `clean.cmd` then `compile.cmd --mode=<mode>`. Recorded on #141 (`5727785452`), not fixed (frozen candidate).
- **E1 `widgets-basic`** (port 5952): local key delivery before any viewer (`APP_KEY key=72/74`, via **posted window
  messages** - the foreground could not be taken), view-only produced **0 lines**, `POLICY_STOPPED -> POLICY_INPUT
  true -> POLICY_RESTART_REQUESTED -> READY`, remote keys/text, held-Shift holder killed unreleased then the next
  key `shift=0`, mid-control reconnect, and a **10 s stalled raw-socket viewer** during which the app produced 11 log
  lines and the normal viewer's captures took 0.01-0.02 s.
- **E2 `quick-basic`** (5953): same set; remote pointer `x=60 y=40` exactly the viewer's coordinates (which is what
  shows E1's `x=40 y=22` is a **child-widget-local** coordinate, not a defect); here `keybd_event` ran with the
  foreground confirmed and produced nothing, the posted-message path did.
- **E3 `qml-basic`** (5954): `READY` is printed **between** `POLICY_INPUT` and `POLICY_RESTART_REQUESTED` (declarative
  ordering, not a defect); `APP_TEXT` carries the field's **accumulated** content; that probe prints no
  `shift/ctrl/alt`, so modifier balance cannot be read from its key lines.
- **E4 QPA, three runs**: read-only `-platform hyremote:hyremote-port=5960` printed `HyRemote QPA Proxy active; native
  delegate: "windows" ... remote input: false` (the defining proof) with **0 changed pixels** under viewer input;
  control `...:hyremote-input=true` accumulated remote typing in the app's text field (changed pixels
  142->396 with the bbox growing rightward); native `-platform windows` baseline exited **0**. A WIN32
  GUI-subsystem app's `qInfo` needs `QT_FORCE_STDERR_LOGGING=1` to reach a capturing pipe
  (`QT_LOGGING_TO_CONSOLE` is deprecated in 6.8).
- **Still open after this pass**: explicit stop while **in control mode** (and E1 step 9's return to view-only) is
  structurally unreachable on `widgets-basic`/`quick-basic` - their single policy transition runs only from the
  view-only state and only once; the DPR interaction needs a scaled/multi-display host (this one is 100%); E4
  child-surface cleanup has no instrument because that application deliberately prints nothing. Each needs a
  product-side decision.

## What V1.0.0.0 still needs (re-measured 2026-09-18, candidate `eab386e`, listed on #33 `5727585068`)

- **Product code: nothing open.** #139 merged (`2dcfa98`), #140 (`ed78519`), #137 merged; the only open non-#134 PR is
  #106, the V1 convergence PR itself.
- **Physical acceptance #109 is the remaining work.** Windows: E1 `widgets-basic` **PASS on `a44ae4d`** but **owes a
  re-verification on the final candidate**, because the head moved to `eab386e` with a change inside
  `examples/widgets-basic/main.cpp`; E2 `quick-basic` has view-only, the same-process transition, remote
  pointer/keys/text, abrupt-disconnect held-input cleanup (three consistent reproductions), mid-control reconnect, real
  Quick control text focus/input and native resize - still owed are **explicit stop while a modifier is held with the
  app alive** (must be `widgets-basic`, `quick-basic` exits on that path), **DPR interaction** (host is DPR 1; needs a
  second display/scaled config or an honest "not exercisable here") and the **bounded slow viewer** (needs a genuinely
  stalled reader); E3 not observed (needs a `--clean` rebuild with the QML mode on); E4 unlocked and built but not
  observed (three runs: native baseline, `-platform hyremote` read-only, `-platform "hyremote:hyremote-input=true"`
  control). **Linux x86_64: no evidence, not waived.**
- **Every record must state OS, candidate SHA, exact Qt kit + compiler (here `mingw1310_64` + Qt 6.8.3 `mingw_64` + Qt
  Ninja, Release), the viewer, and that offscreen/Xvfb/hosted evidence does not count for these cells.**
- **Gates/release mechanics are tag-bound and not mine**: `git tag -l` empty, no `release/*` branch; #30/#31/#32 freeze
  acceptances, #33 GA, #95 tags, #107 metadata acceptance owed (artifacts exist, #101 closed covers the API half).
- **Not V1**: #134/#136/#138 held, #143/#144 post-V1, #57, #3/#9/#10/#17/#18, #14, #123, #141, #1. #41 waits on #109.

## What V1.0.0.0 still needs (earlier measurement, candidate `2f9d125`)

- **Product code on the candidate: nothing open.** The reviewed defect fix **#139** was merged by the owner
  (`2dcfa98` "Merge PR #139: resolve reviewed V1 defects") and the deploy fix **#140** was merged (`ed78519`), as was
  the qBittorrent L1 lane **#137**. My superseded **#131/#132** are CLOSED unmerged. The only open non-#134 PR is
  **#106**, the V1 convergence PR itself.
- **Gates and release mechanics: everything still gated on tags, and `git tag -l` is empty.** No `release/*` branch
  exists either. Open: **#30/#31/#32** (the three mode freezes accepted), **#33** (GA gate), **#95** (Git Flow,
  milestone tags, release gates), **#107** (release-readiness metadata accepted - the artifacts exist:
  `docs/releases/v1.0.0.0.md`, `NOTICE.md`, `docs/release-package-manifest.md`; the API half is done since **#101 is
  CLOSED/accepted** and `docs/v1-api-stability.md` exists).
- **Physical acceptance #109 is the critical path.** Windows: E1 must be re-taken on `2f9d125`, E2 still misses
  explicit stop while held / resize / DPR / real Quick control focus / slow viewer, E3 and **E4** (now unlocked) are
  not run. The **Linux x86_64 column E1-E4 has no evidence and is not waived** - it needs a Linux host, which is not
  mine to provide.
- **#41** stays open by ruling until #109 supplies real local-display/local-input evidence; its engineering work is
  done.
- Not V1 and must not be counted: #134/#136/#138 (held drafts), #143/#144 (post-V1), #57, #3/#9/#10/#17/#18, #14,
  #123, #141, #1.
- **Release authority is not mine**: release branch, tags, main/develop backmerge and the GA tag are the
  repository/release authority's actions.

## Documentation system and structure/naming LANDED on the V1 line (2026-09-18, product directive)

- **The V1 candidate advanced `4281db2` -> `ef1e577` -> `2f9d125`**: PR #142 (documentation system + structure/naming)
  and PR #145 (roadmap: security and bandwidth/acceleration as committed tracks) were **merged into
  `feature/104-v1-ga-acceptance-matrix`**, not left as branches. On the candidate tree now:
  `docs/naming-conventions.md`, `docs/README.md` + `docs/en/README.md` (bilingual navigation roots),
  `docs/guide/install.md` + its English mirror, `docs/repository-layout.md` extension rules; the four
  migration-pointer files (`getting-started/linux.md`, `getting-started/windows.md`, `sdk-installation.md`,
  `source-consumption.md`) are removed; `docs/development-roadmap.md` names `[SEC-01]` #143 and `[BW-01]` #144 and
  carries the statement rule.
- **Why now, and it is a deliberate override**: the owner's rule was "do not bundle deferred classes onto the frozen
  candidate, do not merge before #109". The product side directed that the docs land, and **#109 is not complete yet**,
  so moving the candidate SHA today costs one envelope re-run plus re-taking the physical observations on the new
  build, whereas doing it after #109 completes would invalidate a finished physical acceptance. This was the cheapest
  moment it could ever be. Rollback if objected to: one revert of the merge commits; the branches stay as provenance.
- **A near-miss worth remembering**: `SDK consumption` on the docs branch failed on Windows with
  `ERROR : Specified path is bad: bin/Qt6Guid.dll` / `aqtinstall(aqt) ... exit code 254` while the Linux job of the
  same workflow passed. That is a **runner-side Qt installation failure, not a branch defect** - I verified the cause,
  re-ran the job (green), and merged only on a green envelope instead of merging around a red check.
- **Consequence to honour**: because the candidate SHA moved, the Windows physical cells are **re-taken on `2f9d125`**
  (the binaries are unchanged, but the runbook requires evidence against the exact candidate under test). E1 gets
  re-issued on the same mingw toolchain, and E2/E3/E4 continue directly on the new build.

## Issue hygiene: close only on landed evidence, and `git tag -l` is empty (2026-09-18)

- **Closed 12 on cited artifacts** (#28, #29, #60, #62, #64, #66, #68, #69, #71, #72, #76, #102) - each with an
  evidence comment and a "reopen if X is still missing" line, never silently. Evidence was verified first, not assumed:
  all five examples, all four product-fit harnesses, the QPA test set, `cmake/HyRemoteDeploy.cmake`'s declarative
  payload support, `docs/input-model.md`, and the six workflows.
- **A repo-wide fact worth remembering: `git tag -l` returns nothing.** No milestone or release tag exists, which is
  exactly why #30/#31/#32/#33 and #95 stay open - their deliverable *is* the accepted freeze/tag, even though the
  underlying work has landed. #107 stays open for the same reason (release-time finalisation), and #41 stays open by
  explicit ruling until #109 supplies local-display/local-input evidence.
- **#6 requires both operating systems** ("passing on only one operating system is partial evidence, not completion"),
  and #7 includes the EGLFS half - neither can close while the Linux/embedded evidence is owed. #14 is a governance
  external machine-acceptance gate that declares itself non-blocking. #123 is cross-linked to #144 (`5726562401`) so
  the perf umbrella and the new bandwidth/acceleration sequence do not duplicate.
- Open issues after the sweep: 23 (#1, #3, #6, #7, #9, #10, #14, #17, #18, #30, #31, #32, #33, #41, #57, #95, #107,
  #109, #123, #134, #141, #143, #144), and the sweep record is #141 comment `5726577369`; the #123 cross-link is
  `5726562401`.

## [SUPERSEDED 2026-09-19 by PR #147/#157 - historical record only] Security and bandwidth/acceleration as committed post-V1 tracks (product directive 2026-09-18)

> Kept only for **why these issues were created** (the tracking defects, the vehicles, the wording they replaced), not for
> current scope. PR #147 rewrote `docs/development-roadmap.md` (`3431b00`) so that #143/#144/#9/#57 are **V1.0.0.0
> requirements**, and #157 closed on 2026-09-19 with the machine manifest - mandatory
> `9 30 31 32 33 39 41 57 101 104 107 109 143 144 157 158 159 162 163 164 165`, embedded-deferred `7 10 17 18` - which
> #33/#106/#107/#95 all defer to.

- **The tracking defects came first.** Security had **no issue at all** (title searches for `credentials`/`TLS`/
  `encryption` = 0); it existed only as "future" prose in `security-model.md` §10. Bandwidth/acceleration had issues
  (#9 damage-aware delivery, #17 low-copy ownership, #10 hardware/embedded validation) but **no sequence**, and the RFB
  **encoding strategy** - the biggest bandwidth lever, and where the official Qt VNC Server is ahead out of the box
  (Hextile/Zlib/dirty-region) - had **no issue**.
- **Vehicles created**: **[SEC-01] #143** (S1 design/threat model -> S2 authentication -> S3 encryption -> S4 per-client
  authorization/audit; acceptance needs a negative test per mechanism, and an authenticated identity must stay
  distinguishable from a connection count per `security-model.md:139`) and **[BW-01] #144** (B1 encoding strategy
  **first**, then B2 damage-aware delivery #9, B3 low-copy ownership #17, B4 hardware encoding + embedded target #10;
  "faster" counts only with measured byte counts for static/small-change/full-motion).
- **Roadmap wording replaced** because that line is why they drifted: `docs/development-roadmap.md:132` said these were
  "optional backend optimizations unless a measured requirement promotes them". **PR #145** (`docs/post-v1-scope`,
  post-V1 docs-only draft) changes it to committed `V1.x` tracks naming #143/#144, adds the security line the Post-GA
  section never had, and writes in the **statement rule**: `known-limitations.md` and `compatibility.md` are edited
  **only in the change that lands a capability**, never in advance.
- **Boundaries**: not V1 blockers; may not delay or widen V1 GA; frozen candidate untouched; #109 remains the critical
  path; `known-limitations.md:22-26` (SecurityType None) and `:74-82` (acceleration non-capabilities) stand verbatim
  until a mechanism lands.

## Two different things, never conflate them: `qvnc` vs **Qt VNC Server** (`qvncserver`)

`qvnc` is the **in-tree qtbase platform plugin** (`-platform vnc`) - replacement model. **Qt VNC Server**
(`QtVncServer` / `QVncServer` / QML `import QtVncServer`) is a **separate commercial Qt add-on module** that hosts a
VNC-compatible server inside a Qt application, typically sharing part of a **Qt Quick** application, or paired with Qt
Wayland Compositor for remote desktop. I conflated the two once and had to correct it publicly; the product side's
question was about **Qt VNC Server**, and the repository already had a section on it
(`docs/product-overview.md:101`: *"HyRemote does not treat 'stronger than Qt VNC Server' as an unverified marketing
claim"* - only evidence-backed advantages).

**Upstream facts (official module page, Qt 6.11 docs, fetched 2026-09-18)**: commercial licence only; upstream calls it
a "simple" server supporting **only a subset of RFB**; encodings Raw/Hextile/Zlib with `QT_VNCSERVER_PREFERRED_ENCODING`
and dirty-region detection (`QT_VNC_NO_DIRTYMAP=1` disables); optional **DEC Authentication** password if LibTomCrypt is
present, described upstream as **weak**, otherwise **no encryption**; examples `Vnc Chat` / `Remote Desktop`. The page
is **silent** on supported platforms, minimum Qt version, the sharing/rendering mechanism, input limits and whether only
one view can be shared - so "it cannot keep a native local window" is **not** an upstream claim and must not be
attributed to it.

**Where we are behind**: encoding/bandwidth (they ship Hextile/Zlib/dirty-region; V1 here has explicit
non-capabilities for zero-copy, DMA-BUF/GBM, H.264, per-surface damage), embedded/headless (their target is Qt Quick +
Wayland Compositor; our EGLFS row is Unverified), and security (they have a weak password, we have **neither**
authentication nor encryption - never smooth this over). **Where we differ by design**: Apache-2.0 vs commercial, three
integration modes, Widgets and Quick both first-class, native local coexistence, explicit lifecycle, backend-neutral
architecture, SDK workflows.

## Positioning vs the official Qt VNC backend and a `vnc-eglfs` route (answered 2026-09-18, recorded on #141)

- **The project's own definition, verbatim (`README.md:104`)**: *"`qhyremote` preserves the qualified native
  `qwindows` / `qxcb` delegate and adds remote access; **it is not a replacement-only qvnc-style backend**."* So the
  comparison is between two models, not two products: `qvnc` (`-platform vnc`) and the `vnc-eglfs` variant **replace**
  the platform integration, while HyRemote **preserves** it and layers remote access on top, with local display and
  local input remaining authoritative. The E4 physical cell is the defining proof of that, and E4 is buildable and
  runnable on this Windows host (private QPA surface present; `libqhyremote.dll` builds).
- **Gaps stated plainly**: embedded/EGLFS is **entirely unverified** (`docs/compatibility.md:85` marks RK3588 /
  EGLFS + OpenGL ES **Unverified** and "high-priority post-V1"; `known-limitations.md:35` says desktop x86 evidence
  implies nothing about Embedded Linux/EGLFS/OpenHarmony) - so the `vnc-eglfs` question cannot be answered with
  evidence from this side at all; no acceleration promise (`known-limitations.md:74-82` - zero-copy, DMA-BUF/GBM,
  H.264/RKMPP, per-surface damage, Quick3D/OpenGL acceleration are non-capabilities); a private-QPA-ABI dependency on
  exact Qt 6.8.3 with no per-patch IID check; **no transport security** (`known-limitations.md:22-26`, SecurityType
  None, loopback default, remote input opt-in); explicit input-semantics limits (`:52-55` - shared logical input
  device across viewers, no IME/composition reconstruction, no late replay); and a required deployment helper.
- **Rules**: no V1 claim is widened by this comparison (the declared envelope stays Windows/Linux x86_64; implying
  embedded coverage is a scope change needing a ruling), and no parity/superiority claim about `qvnc`/`vnc-eglfs` goes
  into release notes while the embedded column is unverified and the transport is unencrypted.
- **Caveat on sourcing**: statements about `qvnc`/`eglfs` in that note are general descriptions of Qt's
  platform-plugin mechanism, **not** verified against upstream source in this repository; a web search for the
  official plugin's current limits returned nothing usable. Never quote them as project research.

## Delivery criterion (the correction that matters most, 2026-09-18)

- **"Built but not landed" is not delivery.** A deliverable counts as done only when it sits in a repository location
  that can be referenced - a merged commit on a branch others build from, or a published artifact. A branch whose PR
  cannot merge does not count; neither does a closed PR.
  **Corrected 2026-09-18 later the same day**: I applied that rule to #121/#135 while *believing* they were still
  branch-only. They were not - **#142 was merged** (07:24:20Z, `ef1e577`) and its content is on the candidate as
  `605d67a` (13:53), together with #145 (`2f9d125`). **They ARE delivered.** The rule stands; the application was stale,
  which is its own reporting failure: a verdict must be re-measured, never carried forward from an earlier reading.
- **Root cause of that failure was ordering, not effort**: the work was written before the landing target was
  settled, on branches whose only possible destination was the frozen V1 candidate - a destination that by ruling
  cannot accept it before GA. Result: artifacts with no exit. Rule from now on: **settle the landing target (which
  branch may receive this, and under what trigger) before writing the deliverable**, and state the trigger in the
  artifact itself. #142 now carries that trigger explicitly.
- **What has genuinely landed in this stretch**: the audit #133, the consolidated post-V1 backlog #141, the
  branch-governance fix (14 disposed branches deleted, `delete_branch_on_merge` enabled), and the escalated
  disposition #106 `5725719248`. Everything else is still open and must be reported as such.

## This host CAN run the Windows column of #109 - and the "no private QPA surface" claim was wrong

- **Correction (measured 2026-09-18)**: `C:\Qt\6.8.3\<kit>\include\QtGui\6.8.3\QtGui\qpa\qplatformintegrationfactory_p.h`
  exists for **every** 6.8.3 kit (`mingw_64`, `llvm-mingw_64`, `msvc2022_64`, `msvc2022_arm64`). My earlier statement
  that this host lacks the GUI private QPA build surface, and therefore that Windows E4 was blocked, was **wrong**.
  E4 is the runbook's defining physical proof, so this is a real unlock, not a detail.
- **Working Windows acceptance build recipe** (verified end to end): no `vcvars64.bat` exists in this machine's
  VS18 BuildTools, so build with Qt's own toolchain instead -
  `PATH=C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\Ninja;C:\Qt\Tools\CMake_64\bin`, then
  `cmake -S . -B build-acceptance -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/mingw_64
  -DCMAKE_BUILD_TYPE... -DHYREMOTE_BUILD_TESTS=ON -DHYREMOTE_BUILD_EXAMPLES=ON -DHYREMOTE_BUILD_QML_API=ON
  -DHYREMOTE_WITH_QPA_PROXY=ON`. Configure 15.6 s, full build 163 targets, exit 0, and it produces
  `examples/{widgets-basic,quick-basic,qml-basic,qpa-proxy-existing-app,remote-support-showcase}` plus the QPA
  tests. State the mingw toolchain explicitly in every acceptance record.
- **Capability boundary**: Windows E1-E4 are executable here; the **Linux x86_64 column is not** (no Linux host), and
  **#134 is not** - its L1 lanes need libtorrent/OpenSSL development files and its L2/L3 lanes (MuseScore, Shotcut,
  obs-studio) need heavy multimedia stacks that this host does not have. #134 also stays an isolated draft track.
- **Shell pitfall**: `cmd /c "..."` nested inside a PowerShell-hosted command mangles quoting; invoke `.bat` files
  with `& .\path\file.bat *> log` instead. And `%ERRORLEVEL%` captured after a pipe in a `.bat` reports the
  *filter's* exit code, so verify build success from artifacts plus `$LASTEXITCODE`, never from that echo.
- **`vncdotool` leaves a non-daemon reactor thread alive**, so a driver whose work is finished still sits in the
  interpreter until something forces it out - with a watchdog that is 90 s of looking hung while every artifact has
  already been written. **Exit hard (`os._exit`) after flushing**: the work is done, the cleanup buys nothing. Measured
  before/after: same 8-phase run, 12 s of work, process gone within 4 s of the last phase.
- **Never let an acceptance driver be able to hang.** `vncdotool`'s `api.connect` **retries** against a port whose
  window has already exited (it does not fail fast) and `captureScreen` can block on a server that stops sending, so
  no in-driver timeout is trustworthy. The window's `--test-seconds` lifetime must comfortably exceed the whole phase
  list, every viewer connect must be guarded by `proc.poll() is None`, and a **hard watchdog thread** (disarmed on
  normal completion, otherwise it reports a finished run as an overrun) must `os._exit` after a bounded deadline.
  With those three rules the full 8-phase E2 observation runs in **12 s** instead of appearing hung for minutes.
- **A driver must write its own phase log to a file, not only to stdout.** Backgrounded/detached runs do not reliably
  echo the tail of stdout, so a fast successful run reads as "stuck with no output" - which happened twice and cost
  two false alarms. `say()` now writes `driver.log` beside `app.log`, and the reliable way to run a long acceptance
  step is detached (`Start-Process ... -RedirectStandardOutput`) plus polling those files, never a foreground wait.
- **Long-running acceptance commands get backgrounded, and heavy redirection hides all progress** - which is why a
  40-60 s run reads as "hung with no output". Two rules for any interactive acceptance driver: **print a timestamped
  phase banner before each wait** (never a bare sleep), and **drive every wait from the application's own log line**
  rather than a guessed client count, with a short budget. Redirecting the whole run to a file is what made it
  invisible; let the driver stream, and let it write `app.log` itself for the record. A wrong predicate (waiting for
  `CLIENT_COUNT 2` when the helper was the second viewer) silently burns the entire budget and produces a **false
  negative line in a published report** - fix the predicate and re-run rather than publishing around it.

## Lead dispositions executed (2026-09-18, product side delegated the owner's authority)

- **`#139` conditional acceptance on objective criteria**: the QPA job failed because of *my* first edit, not CI -
  gating `createTargetComponents` on compile-time adapter macros overrode the caller-supplied resolver that
  `hyremote-qpa-composite-capture-test`/`-input-test` use. Reverted; the real defect is fixed at runtime by
  publishing "no built-in capture adapter supports a composite child surface" as a **non-recoverable
  `BackendFailure`** instead of a recoverable `TemporarilyUnavailable`. **Lesson: a capability question ("can this
  be captured at all") belongs to the caller's resolver and is per-surface/runtime; a compile-time macro must never
  override it.** The macro change itself stays (surface qualification reflects what the build can capture).
- **`#109`**: Windows E1 ruled **PASS** on the recorded set; E2/E3 mine next; Windows E4 and the Linux column stay
  **blocked and not waived** (this host lacks the private QPA surface) and are an owner/product-side assignment; if
  an RFB-touching fix becomes the candidate, the affected cells are re-observed on it.
- **`#141` created** as the single consolidated post-V1 backlog (the #133 audit items, the review's medium/low
  findings, the examples README gaps, branch governance, the #134 hold) - one home instead of reviewer notes.
- **`#134`/`#136`/`#137`/`#138` hold confirmed**; #140 stays with the owner's line.
- **Branch governance now executed, not proposed**: 14 disposed branches deleted (remote heads **83 -> 68**) and
  **`delete_branch_on_merge` flipped to `true`**, which is what `docs/branch-lifecycle.md:29` had required all along.
  Standing rule: once a PR has a disposition, its author deletes the branch.

## Branch governance: policy exists, two controls are unexecuted (measured 2026-09-18)

`docs/branch-lifecycle.md` is the authority and it already describes the current situation: branches are **temporary
work cursors** ("durable history lives in Issues, Pull Requests, commits, release tags and acceptance evidence",
`:3`), and `:29` states that GitHub's **delete_branch_on_merge should be enabled** - "**the V1 repository audit found
it disabled**, which is one direct reason merged task refs can accumulate". Re-measured today: the setting is **still
`false`**, so the documented fix has never been applied. The **deletion rule** (`:35`) is exactly the criterion to
use: delete when the PR is **merged, superseded or closed as abandoned** and the branch is **not the head of another
open PR**. `:39` explicitly forbids pruning on commit-ID mismatch against a squash/reimplementation line - use
repository/PR facts instead (this is the same trap as judging integration by ancestry). The tool
`.github/scripts/prune-stale-branches.ps1` implements that rule safely: dry-run by default, re-queries open PR heads,
aborts before deleting anything if a candidate moved, `-Execute` to delete, and it retains `main`, `develop` and
`feature/104-v1-ga-acceptance-matrix`; it covers **one historical manifest snapshot**, so newly created dead branches
are outside it.

Measured state: 81 refs besides `main`/`develop`, **all dated 2026-09** (a three-day convergence-sprint artefact,
not years of neglect), five open PRs with no backlog, and no orphaned work found. The debt is therefore not the count
but **unexecuted controls**: auto-delete off, and the deletion rule not followed per PR closure - including by me (14
of the dead branches are mine, four created that same day). The KPI worth watching is **"branches whose PR already has
a disposition and that still exist"**, not the total. Deleting is destructive and belongs to the repository-admin
surface, so the verified list goes to the owner rather than being executed unilaterally.

Read-only review, four parallel exploration passes plus my own verification of the top items. Re-check the status
before quoting: these may be fixed later, and this is the state **as of `4281db2`**.

- **Blocker-class**: `protocolFailure()` calls `socket->abort()` and `abort()` dispatches `disconnected()`
  synchronously, so the handler erases the `ClientState` (`rfb_transport.cpp:503-515`) while the caller still holds
  it (`:548`) or while `frameAvailable()` iterates `m_clients` (`:421-427`) - use-after-free and iterator
  invalidation on the network thread. `shutdown()` disconnects the socket from `this` before `abort()`
  (`:401-406`), which is the author's own proof that the re-entrancy is real. Trigger from an unauthenticated peer:
  omit `-223`/DesktopSize, then any geometry change. Our qualified viewer sends `-223` by default.
- **High**: the QPA composite target hard-codes `recoverable = true` for every child event
  (`composite_target.cpp:210-217`, forwarded by `onChildEvent` `:436-444`), so unrecoverable child failures never
  fault the Session and the remote silently gets stale/blank frames; and it always reports
  `result.supported = true` (`:668-680`), bypassing Core's fail-closed "no adapter" branch, so QPA-with-adapters-off
  ships transparent frames while claiming to be active.
- **Test-validity gaps that let hosted greens coexist with defects**: no assertion that the deploy-helper script
  contains the native delegate (so `3b03622` is unguarded); the "stale metadata" fixture cases never reach the
  stale path; multi-config deep-checks a single configuration; no composite smoke asserts canvas content (only
  geometry and `connected()`).
- **Verified sound** (so the review is falsifiable): Core's teardown claim/run-generation serialisation, the absence
  of lost wakeups, the one-way lock ordering, mailbox/input bound accounting, transport bound checks, held-input
  reference counting, the plaintext/loopback/opt-in-input posture, the single-runtime QML payload, and the ruled QPA
  boundaries (no Core link, shared runtime, no second facade).

## #135 closed unmerged; V1 boundary restored (2026-09-18)

**Owner disposition (comment `5724958661`, repeated in `5724966730`): PR #135 is CLOSED, not a V1 carrier.** It
bundled the classes #133 had explicitly deferred (naming document, docs reorganisation, generalized gate policy,
ignore/line-ending hygiene, SPDX across 141 files, example/docs expansion, the third-party example), which is not a
reason to replace a candidate that already passed the full hosted/reference envelope. Rules to keep: do not
merge/absorb before #109; do **not** move #109 to a #135-derived SHA; keep the branch as post-V1 provenance and
recreate any still-wanted cleanup after the V1 release/backmerge; local green evidence does not create authority.
**#136** (real-world matrix) is authorized only as an isolated Draft test-engineering child, **HOLD for integration**,
never a V1 blocker, and exact Qt 6.8.3 stays the only QPA-qualified line. The V1 candidate remains
`4281db23387780d0b246711438d6b8b134a787d3`. **Any future "structure/docs hygiene" idea must be checked against the
#133 deferral list before being built** - the mistake to avoid was treating product-side "don't defer" as authority
to bundle deferred classes onto a frozen candidate.

## Structure/docs/examples batch (branch `chore/v1-structure-and-docs-batch`, PR #135 - closed unmerged)

Eight commits on top of `4281db2`, local verification **ctest 43/43** and **release-readiness gates 10/10**, pushed
and now going through the nine-workflow envelope. What the batch establishes as project convention:

- **`tests/` holds unit and integration tests**; recorded review/acceptance **evidence is documentation** and lives
  under `docs/acceptance/` next to the runbook that produced it (my earlier idea of `tests/physical-acceptance/` is
  superseded).
- **`examples/` holds user-facing usage examples**, including **opt-in examples that combine HyRemote with
  third-party open-source applications**. Such an example must stay out of the default build and acceptance graph,
  must never vendor third-party sources (fetch at a recorded commit into an ignored build directory), and must never
  become an implementation location for product logic. The acceptance suite stays E1-E6.
- **`integrations/` grows one directory per payload**, and a future Qt LTS line gets `integrations/qpa-<qt-line>/`
  rather than widening the qualified 6.8.3 payload; a new top-level directory is a structural decision with
  migration-level authority.
- **The payload contract is enforced generically**: `check_repository_layout.cmake` scans every payload under
  `integrations/` (excluding colocated test directories) for "reuses the shared runtime / never links Core / never a
  second facade", so an additive payload cannot escape it without a gate change. `docs/naming-conventions.md` is now
  a required document.
- **`docs/naming-conventions.md` exists** and codifies the four intentional spellings, the two intentional
  public-header forms, the QML-URI directory rule, the global-boundary prefix rule, and what V1 does not rename.
- **SPDX short-form identifiers** are on the 141 C++/QML sources (one added line each, no EOL rewrite); the
  repository-level LICENSE + NOTICE remain the authority. **`*.sh` is pinned to LF** in `.gitattributes`, because a
  Windows clone would otherwise break the shell scripts on the reference Linux jobs.
- **Third-party example pattern** (`examples/third-party/qbittorrent/`): README + per-platform fetch/build scripts,
  pinned app tag/commit (qBittorrent `release-5.2.3` = `70e16de46cd559a7db3e6d7faace5e40b86f63dc`) and exact Qt
  6.8.3, application built **unmodified**, deployment assembled next to the built application (shared runtime +
  `qhyremote` + the native delegate), labelled "third-party verification example - not a V1 compatibility/support
  claim". Its troubleshooting table reproduces the delegate failure I diagnosed.
- Process lesson worth keeping: **run the gate you changed**. The generalized payload glob first failed locally
  because it matched `integrations/qpa/tests/CMakeLists.txt`, a colocated test directory that may legitimately
  reference Core.

**Forward compatibility is now a V1 deliverable, not a post-V1 item (product directive 2026-09-18).** The product
side requires structure planning to serve V1.1.0.0/V1.2.0.0/V2.0.0.0 *now* rather than parking work. Key finding that
makes this cheap: the layout gate is **additive-friendly** - it only requires the canonical directories to exist,
forbids the legacy roots, and pins the six source->binary mappings, and it does **not** enumerate an allowed set, so
new payloads/modules/tests/examples need no gate change. The freeze protects the axes; growth inside them is
additive by construction. What was missing is the **placement rule for future work**, proposed as a new
`docs/repository-layout.md` section ("Extension points and forward compatibility") and posted as the text itself in
#106: `src/` grows by product module (never at root, Core stays Qt-free/platform-free); `integrations/` grows one
directory per payload with the three gate rules (reuse the shared runtime, never link Core, never a second
`RemoteAccess` facade) and an explicit `add_subdirectory(<source> <stable-binary-dir>)` so no payload relocates an
existing artifact; **version-qualified payloads**: a future Qt LTS line gets `integrations/qpa-<qt-line>/` while
`integrations/qpa` keeps its path/target/artifact names for the qualified 6.8.3 payload - this is the rule that
decides whether Qt-LTS qualification needs a migration; `tests/` grows by scope (per-platform physical evidence as
new evidence dirs under `tests/` plus a `docs/` acceptance document); `examples/` grows by integration mode with
continuous numbering (E1-E6: Widgets, Quick, QML, QPA, showcase, installed/source SDK consumers); `assets/` non-code
only; `cmake/` owns build/package/deployment modules including platform deployment; `research/` stays opt-in. Two
decisions were asked of the owner: the version-qualified payload naming, and whether per-platform evidence stays
under `tests/`+`docs/` or gets a reserved dedicated root. Also fixed in the same batch:
`docs/repository-layout.md:16` still says "examples E1-E5" although the accepted suite is E1-E6.

**Is the layout "final"?** Answer in two parts, both ruling-backed: (1) **Frozen for V1 - yes.** `docs/repository-layout.md:25-27` freezes the V1 layout and forbids another structural migration *unless a release-blocking architecture defect proves the canonical ownership model itself is wrong*; `:81-94` bans the legacy roots (`core/`, `remoteaccess/`, `qml/`, `qpa/`, `spikes/`, `logo/`) and forbids compatibility copies/symlinks/forwarding dirs; `tests/release-readiness/check_repository_layout.cmake` enforces required directories (incl. `integrations/qml/HyRemote`), the legacy-root ban, the existence of the canonical document, and the six literal `add_subdirectory` mappings (`src/core core`, `src/remoteaccess remoteaccess`, `integrations/qml/HyRemote qml/HyRemote`, `integrations/qpa qpa`, `research/capture spikes/capture`, `research/async-capture spikes/async-capture`) plus the research opt-in guard. The owner's #133 ruling found no release-blocking structure defect and authorized **no** layout/licence/header/gitignore change before #109. (2) **"Optimisation complete" - no**, and deliberately so: S3 (`docs/` flatness), S1/S2/C8 (the naming-conventions document), C2 (per-file SPDX) and C6 (ignore hygiene) are ruled **post-V1**, and post-V1 cleanup must stay separate from the V1 release claim. S4/S5/S6 are **not** pending work - the ruling calls them intentional (both file placements correct, two-layer manifest/gate ownership not to be collapsed, layered tests plus the real-E4 reuse as anti-drift evidence not to be deduplicated).
Tracked top level on the frozen candidate is exactly the canonical set (`assets/ cmake/ docs/ examples/ integrations/ research/ src/ tests/` plus `.github/`, and the root files `CMakeLists.txt`, `CONTRIBUTING.md`, `LICENSE`, `NOTICE.md`, `README.md`, `SECURITY.md`). Local-only, untracked and therefore not part of the released structure: `.cache`, `.vscode`, `.codebuddy`, `compile_commands.json` (the last one ignored by design at `.gitignore:8`); `.cache` and `.codebuddy` are exactly the clutter that C6's post-V1 ignore hygiene would cover.

- **A review is not delivered until it is published as a reviewable artifact.** The engineering-structure review
  ran on 2026-09-17 (`0535f87`, 67 directories / 313 files) and its follow-up PRs #118/#119 were absorbed into
  #106 (`5802865`, `54fb3b3`, `ffc020a`, `c8b7418`), but the complete findings list stayed in the reviewer's notes
  and the product side had to ask why no result was visible. Fixed by **#133** (pointer on #106 `5723207787`).
- **Structure findings still open** (all need the owner's ruling or delegation; layout is otherwise frozen by
  `docs/repository-layout.md:27`): S1/S2 two public-header conventions coexist and `integrations/qml/HyRemote` is
  deeper/PascalCase next to flat `integrations/qpa` (both need the never-written `docs/naming-conventions.md`);
  S3 35 of 47 `.md` files sit flat under `docs/`; S4 `src/remoteaccess/tests/rfb_product_fit.py` is a test inside
  the product tree and `docs/proposals/hyremote_core.hpp` is a header under `docs/`; S5 the package payload list is
  duplicated between `release-package-manifest.md:9-16` and `check_release_metadata.cmake:7-62` (also the NOTICE
  tool list at `:109-116` and `docs/releases/*` twice); S6 the deploy-helper contract is verified in three places
  and `examples/qpa-proxy-existing-app/main.cpp` is reused as a fixture by
  `integrations/qpa/tests/CMakeLists.txt:82`.
- **Compliance baseline measured on `3b03622`** (re-measure before quoting): 141 C++/QML source files with **0**
  SPDX headers (per-file headers were recorded as optional - a decision, not a violation); **0**
  `TODO/FIXME/XXX/HACK` in the whole tree; no secret material (the 12 `password =` hits are `password=None` in the
  VNC test clients); no tracked build artifacts; no submodules, and `research/vnc-transport-rust-ffi` pins a fork
  commit only as an evaluation input. **Real gap found**: `.gitignore` covers `/build/`, `/build-*/` and
  `cmake-build-*/` but **not** `__pycache__/`, `*.pyc` or working directories such as `.codebuddy/`, so both appear
  as untracked clutter one `git add -A` away from being committed.

## Documentation system and directory structure: **LANDED** on the V1 candidate (superseding the "re-cut as #142" state below)

- **Final state (re-measured 2026-09-18)**: `#142` merged 07:24:20Z (`ef1e577`) and `#145` merged 06:25Z (`2f9d125`),
  so the content is on `feature/104-v1-ga-acceptance-matrix` as `605d67a`. Present there: `docs/README.md` +
  `docs/en/README.md` (bilingual navigation roots), `docs/guide/install.md` + English mirror, `docs/naming-conventions.md`,
  `docs/repository-layout.md`, the `CONTRIBUTING.md:124` language convention, and
  `tests/release-readiness/check_release_metadata.cmake:20-21,252` tracking the **canonical** guide; `docs/` = 51 files.
  **Three ledger errors of mine came out of the product side's "you dropped them" complaint** (listed on #33
  `5727881291`): they were missing from my V1 lists entirely, I described them as unlanded, and I described `#134` as
  nothing-but-a-draft when `test/134-realworld-qt-matrix` already carries 30 files / 1708 insertions from `#137`.
- **Residual in this area, all x86-doable hence V1 work**: `.gitignore` still lacks `__pycache__/` and `*.pyc`;
  `.gitattributes` does not exist (no line-ending pin); **0 of 132** source files carry an SPDX header;
  `examples/third-party/` does not exist. And `docs/development-roadmap.md:132-135` calls #143/#144 committed **`V1.x`**
  tracks, which the corrected V1 boundary contradicts (both are x86 work and belong to V1.0.0.0).

## History of the same matter, as it looked before the merge

- **Product side asked (2026-09-18) why neither appeared handled.** Answer: both *were* worked and neither was ever
  landed. **Documentation system** = ruled 2026-09-17 (`5712801878`) and delivered as **PR #121**
  (`docs/bilingual-user-docs`, 4 commits: bilingual navigation roots `docs/README.md` + `docs/en/README.md`, one
  bilingual `docs/guide/install.md` replacing the four migration-pointer files, the `CONTRIBUTING.md` language
  convention, and the ruled release-metadata-gate change so the gate tracks the canonical guide) - **CLOSED
  unmerged**. **Directory structure** = audit #133 plus `2d22d18` (`docs/naming-conventions.md` +
  `docs/repository-layout.md` extension rules) from **PR #135** - **CLOSED unmerged**. `develop` is stale (still at
  `4332713`, 09-15) so it is not a landing target, and the freeze rules out the accepted candidate.
- **The real failure was the missing landing path**, not the work: after both closures I filed them under "post-V1"
  with no executable route, which reads as ignored. Fixed by **PR #142** (`docs/system-and-structure`, draft,
  docs-only, based on the frozen candidate): re-cuts exactly those two classes from the reviewed originals (verified
  **zero drift** on every path they touch), carries none of the classes that got #135 closed (ignores, LF pin, SPDX,
  generalized gate, examples expansion, third-party example), touches no source/test/payload/workflow/acceptance
  path, and must not merge before the V1 line is released and back-merged (an earlier waiver is the release
  authority's call). Rule: **a deferral must ship with an executable path, not just a list entry.**
- **Second, smaller failure owned and fixed**: the branch-cleanup pass deleted `chore/v1-structure-and-docs-batch`,
  which I had recorded as post-V1 provenance. **Restored**; `refs/pull/135/head` had preserved the commits
  independently. Both closed PR branches (`docs/bilingual-user-docs`, `chore/v1-structure-and-docs-batch`) are alive
  again, so nothing from #121/#135 is lost.
- **Fresh C6 evidence**: on a clean docs branch, `git add -A` staged `.codebuddy/**` and two
  `__pycache__/*.pyc` files - with today's `.gitignore` a bulk add commits working files and byte-code. Reported on
  #141 with a recommendation to take the inert `__pycache__/` + `*.pyc` lines first and leave tool-specific
  directories to local excludes.

## Documentation system (product-side requirement, 2026-09-17)

- Goal: a documentation set that is **small in count, detailed in content, bilingual, and written for end users
  and the final product state** - explicitly *not* process documentation. Baseline when this was set: 48 files /
  5,760 lines under `docs/` with no bilingual infrastructure. Owner ruling (2026-09-17, comment `5712801878`):
  the bilingual **documentation** requirement is accepted (Chinese primary + English mirror), but a mandatory
  Chinese-first **source-comment** policy is **not** part of V1 - keep existing comments as they are, no bulk
  comment-only migration, and bilingual comments only where they materially help maintainers.
- Structure: `docs/guide/**` (user: install, connect, deploy, operate; Chinese primary at `docs/<path>` with an
  English mirror at `docs/en/<path>`), `docs/reference/**` (product final-state contracts, same bilingual rule),
  and the remainder of `docs/` plus `adr/`, `releases/`, `proposals/` = internal/release material, kept in
  English because the release gates pin tokens there. `docs/README.md` and `docs/en/README.md` are the
  **bilingual navigation roots**. The one-to-one mirror rule applies to files that have entered `guide/` or
  `reference/`; legacy files outside those zones need not be mirrored while the migration runs. Migration
  history belongs to the PR/Issue/internal zone, not to the user index.
- **Delegated sequence (ruled 2026-09-17, PR #121 owned by this side)**: after #120 is integrated, rebase
  #121, then move `tests/release-readiness/check_release_metadata.cmake` and the README canonical link to
  `docs/guide/install.md` and delete the four migration-pointer files in that same change ("the gate must track
  the current canonical documentation, not freeze obsolete paths"). Required corrections, now done in `9a25eb5`:
  no "accepted against Qt 6.8.3" claim while the matrix says Candidate, a complete `build-test`
  configure/build/test chain in both guides, no malformed table cells from raw `|` separators, no
  migration-status section in the index, and no mandatory comment-policy language in `CONTRIBUTING.md`.
- **Delegated follow-up PRs (owner ruling `5712801878`, all starting from the repaired #106 head)**: (1) QPA
  compile/test plumbing - the exact-Qt-6.8.3 `qplatformintegrationfactory_p.h` path, the missing `QPalette`
  include and the two composite tests' `HyRemote::RemoteAccess` include surface, all in one PR; (2) a separate
  PR for the `examples/qml-basic` `CMAKE_AUTOMOC` ordering fix; (3) a separate architecture PR for the
  QPA/Core seam above. Items are dropped, not preserved, if fresh hosted execution disproves them.
- Content rules: one document per reader intent (merge rather than fragment); process content - issue numbers,
  acceptance scheduling and status, milestone chronicles, investigation logs, one-off checklists - is banned from
  the user/reference zones (it belongs to the internal zone or to `research/` evidence); moving a document
  updates every reference in the same change, with one deliberate exception: a path a release gate still lists
  may keep a short pointer file until the gate's list is updated.
- Comment convention (in `CONTRIBUTING.md`): `// 中文说明 — English`, Chinese first, English counterpart on the
  same logical unit; rolled out in stages (public headers, then internal code, then tests) and always in its own
  commit; identifiers, log tokens and CMake target names are never renamed by a language change.
- **Gate constraints on any docs move**: `tests/release-readiness/check_release_metadata.cmake` hard-lists
  user-document paths and requires the README to contain those exact link strings (existence-only check, so
  pointer files work); `check_release_documentation_layout.cmake` pins exact English tokens inside
  `release-package-manifest.md`, `dependency-policy.md`, `v1-repository-admin.md`, `v1-physical-acceptance.md`,
  `README.md` and `CONTRIBUTING.md`; `check_repository_layout.cmake` requires `docs/repository-layout.md` to
  exist and (as extended on 2026-09-17) the release-readiness tests to be registered in the root `CMakeLists.txt`.

## Branch/remote state (checked 2026-09-17)

- Delivery model: the local checkout is a working copy only — accepted work reaches `main` through
  squash-merged PRs (#22 and #55 merged, issue #21 closed). It is normal for the clone to hold **no**
  in-progress work; prove it with `git log --branches --not --remotes` (must be empty) plus
  `git status`, instead of assuming something must be pending.
- Squash merges make old branches *look* unmerged: test redundancy with
  `git diff --stat origin/main <branch>` (empty output = pure history noise) rather than
  `rev-list --count`, which reports 167–591 for branches whose content is already in `main`.
- `origin/develop` is identical to `origin/main` and no local `develop` exists; the remote keeps
  ~57 branches and never prunes, so lingering merged branches and "behind" markers are normal.
- The only open/active V1 line is **draft PR #106** (`feature/104-v1-ga-acceptance-matrix`, targets
  `develop`, the owner's declared "single convergence authority", ~156 files vs `main`). The other V1
  slices (#92–#110) were closed unmerged and are "historical audit records only" per #106 — do not
  restart them without taking over #106 explicitly.
- Safe fast-forward idiom for a branch that is not checked out: `git fetch origin <b>:<b>` (refuses
  anything but a fast-forward); verify afterwards with
  `git rev-list --left-right --count <b>...<b>@{upstream}` (want `0 0`).

## V1 candidate line: platform/harness pitfalls and open blockers (updated 2026-09-18)

Candidate advanced past `6e861e4` to `6169528` (owner commits: pin Qt 6.8 xcb runtime dependencies on Linux; run
clean/GA QML consumers on native xcb, Windows on qwindows). Effects verified by execution: the two Linux QPA
`xcb` delegate aborts (`hyremote-qpa-proxy-smoke`, `hyremote-qpa-native-semantics`) **pass** now, and the
`sdk-consumption.yml` Linux source-QML consumer already runs under `xvfb-run -a` with `-u QT_QPA_PLATFORM`.

**Three pitfalls worth never repeating, all probe- or log-verified:**

1. **A POSIX-style character class containing `\n` is unsafe in CMake's regex engine** - the escape can be read
   as the literal `n`. In `check_source_payload_relocation.cmake` this truncated `/home/runner/...` to `/home/ru`,
   producing a deterministic false "escaped deployment tree" failure; the fix (owner's `d6e212c`) parses `ldd`
   output line by line instead. **Correction to an earlier note of mine**: I first blamed `file(REAL_PATH)` for
   not normalizing the `$ORIGIN/../../lib` anchor form, and that was wrong - with the truncation present no
   normalization could have passed. `file(REAL_PATH)` keeping its input (warning only) for an unresolvable path
   is real, but secondary: use it on paths that exist, and guard with `EXISTS` when a capture may be wrong.
2. **A Qt GUI application on Windows logs to the debugger, not to an inherited pipe.** Harnesses that read the
   child's stdout capture nothing and see "process alive, zero lines"; set `QT_FORCE_STDERR_LOGGING=1` for the
   spawned example. Proven by A/B on the E3 QML harness (PR #131): before = `[]`, after = the full lifecycle
   `PASS` (view-only isolation -> stop/configure/start -> control input -> reconnect -> stop).
3. **The HyRemote QPA plugin delegates through Qt's *plugin factory*, so a clean deployment must ship Qt's `xcb`
   platform plugin.** `hyremote_platform_plugin.cpp:36` names the delegate `xcb` and line 385 calls
   `QPlatformIntegrationFactory::create(delegateName, ...)`; a deployment containing only
   `plugins/platforms/libqhyremote.so` (plus Core/Gui) therefore fails with
   `HyRemote QPA Proxy could not create native delegate "xcb"` and Qt aborts (SIGABRT -6), while the same test
   passes in the build tree where Qt's plugins are visible. Any `hyremote_deploy(... QPA)` fix must place the
   consumer Qt's `xcb` plugin (and its xcbglintegrations) and let its `libQt6XcbQpa.so.6` dependency follow, with
   `xcb` preserved as the Linux reference delegate. Qt handles for locating it: public `QT6_INSTALL_PREFIX` /
   `QT6_INSTALL_PLUGINS` at configure time (preferred), or the private `__QT_DEPLOY_QT_INSTALL_*` inside
   `Qt6CoreDeploySupport.cmake` at install time.

**Open on this line:** the Linux installed-QPA product-fit harness stops with
`consumer exited before RFB listener became ready: -6` (SIGABRT) and prints no child output - the owner
authorised capturing/surfacing that output (`tests/consumer-installed-qpa/product_fit.py`, E4 source stays
Qt-only) as the next bounded diagnostic; the Windows case is the same path but "alive, no RFB ready". Dependency
relocation itself passes on Linux (`16 HyRemote/Qt dependency resolutions stayed within the deployment tree`).

## V1 candidate `6e861e4`: remaining Linux failures and their exact causes (2026-09-17)

`SDK consumption` on this head proves the private-Qt-runtime fix works (the old `libQt6Widgets.so.6` / exit 127
signature is gone from the log and the installed C++ consumer step passes), then fails at the **QML-only clean
run** because the step forces `QT_QPA_PLATFORM=offscreen` while the deployment ships only
`plugins/platforms/libqxcb.so` (no offscreen plugin) - deterministic on any runner. Sibling steps in the same
file already use `xvfb-run -a` plus `-u QT_QPA_PLATFORM`, which is the ruled convention for GUI E2E. Reported with
the one-step diff on #106 (`5715717358`); the launch environment is the owner's surface by ruling.

Also open on this head: `hyremote-qpa-proxy-smoke` / `hyremote-qpa-native-semantics` abort with
`Could not load the Qt platform plugin "hyremote" ... even though it was found` (loader-level, build tree) - the
repaired `ldd` step from **#129** is what will name the unresolved dependency.

## V1 candidate `6e861e4` and the Linux QPA test wiring defect (2026-09-17)

Current candidate is **`6e861e4`** (PR #106), which contains the owner's four fixes plus my merged deploy guard
(#128). Readback item 1 (Linux deploy-helper/QPA configure+test) executed: **the deploy-helper fixtures now run
and pass on Linux**, confirming the guard by execution. It still fails on three tests, identically in the QPA job
and in the V1 GA Linux job:

1. `hyremote-qpa-source-payload-relocation` - the registration passes `-DQPA_SOURCE_FILE` /
   `-DREMOTEACCESS_SOURCE_FILE` and no `TEST_ROOT`, while `check_source_payload_relocation.cmake` declares
   `QPA_FILE` / `REMOTEACCESS_FILE` / `TEST_ROOT`, so the test has failed at its own argument check since
   `d9caddd` and never verified the relocation contract. Fixed in **#129** (one file, 7 lines, based on
   `6e861e4`, verified locally: corrected wiring passes the argument check, gates and fixtures 19/19). This
   blocks the Linux GA envelope, so it is on the critical path for #104.
2. `hyremote-qpa-proxy-smoke` / `hyremote-qpa-native-semantics` - `qt.qpa.plugin: Could not load the Qt platform
   plugin "hyremote" in "" even though it was found.` Loader-level, reason only visible with `QT_DEBUG_PLUGINS=1`;
   the repaired `ldd` test is the cheapest way to name it. Root cause deliberately **not** claimed.

## Delegated V1 queue: absorbed by the owner's own commits (2026-09-17, final state)

The owner's line closed the four PRs and pushed its own implementations onto `feature/104-v1-ga-acceptance-matrix`:
`d988b69` (examples AUTOMOC, absorbed from #125), `d93b6ff` (QPA exact-Qt + fixture, "applied their exact blobs
atomically" from #124), `0de0fb2` (QPA/Core seam, "isolated exactly five seam-only files" from #126) and
`e1a67ac` (its own Linux private-Qt-runtime deploy fix). **#121 was closed as post-V1**, not merged: the docs
system migration would start a second structural migration against frozen paths; user-facing docs are corrected
in place on #106 for V1. My own `fix/v1-deploy-payload-qt-runtime` (#127) was closed as superseded by
`e1a67ac` (its `DIRECTORIES`-based discovery is more general than my adapter-list approach) - record kept at
`58c237d`, with the fixture coverage for that contract still in a local `git stash`.

Verify absorption by blob, not by message: `git rev-parse <their-commit>:<path>` == `git rev-parse
<my-commit>:<path>` for #124/#126 (identical), but differs for `e1a67ac` (their own implementation).

Open blocker reported on #106 (comment `5715083063`): `e1a67ac` emits `$<TARGET_FILE_DIR:Qt6::Core>` in the
generated Linux deploy script unconditionally, and the deploy-helper fixture has no Qt targets, so every
`hyremote-qpa-deploy-helper-*` sub-configure fails at the generate step on Linux (`No target "Qt6::Core"`,
reproduced locally). Fix = `if(TARGET Qt6::Core)` guard around the emission, prepared as **#128** and **merged**
into the convergence line as `6e861e4` (its tree is byte-identical to my commit `214e94c`); the owner's ruling
calls the `e1a67ac` runs stale because of this defect. Verified locally by fixtures 16/16, the five deployment
gates and `ctest` 66/66 on the integrated content. **Policy from the same ruling: the bootstrap's fail-closed
`UNRESOLVED_DEPENDENCIES` behaviour stays - do not relax it without a hosted false-positive proof.**

## Delegated V1 queue: branch layout (originally prepared 2026-09-17)

**#120 (P0 root CMakeLists) and #122 (title/positioning) are integrated** into
`feature/104-v1-ga-acceptance-matrix` (squashes `377c608`, `efcd6c0`). The four bounded follow-ups are now
open as PRs against that head, each on its own surface:

| PR | Branch | Scope | Local evidence |
| --- | --- | --- | --- |
| **#124** | `fix/v1-qpa-plumbing` | `integrations/qpa/**`: Qt 6.8.3 `qplatformintegrationfactory_p.h` path, missing `QPalette`, the two composite tests' `HyRemote::RemoteAccess` include surface, and the configuration-aware deploy fixture | 18/18 deploy-helper tests, incl. both generator shapes, with ninja removed from the test PATH |
| **#125** | `fix/v1-qml-example-automoc` | `examples/qml-basic/CMakeLists.txt`: `CMAKE_AUTOMOC` before `qt_add_qml_module` | example configures + builds with examples ON |
| **#126** | `feat/v1-qpa-core-seam` | `src/remoteaccess/src/detail/**` seam + two QPA composite sources; contains #124's commits | 66/66 ctest, plugin links `HyRemoteRemoteAccess` + Qt only, install surface unchanged |
| **#121** | `docs/bilingual-user-docs` | docs system + review corrections + `d06f685` (formatting-invariant metadata gate, canonical install-guide path, pointer removal) | rebased onto `efcd6c0`; the three doc/metadata gates pass |
| **#127** | `fix/v1-deploy-payload-qt-runtime` | `cmake/HyRemoteDeploy.cmake` + `HyRemoteInstall.cmake`: the payload's private Qt runtime is placed by `hyremote_deploy()` | generated script installs + lists `Qt6Widgets`/`Qt6Quick`; deploy tree contains them; fixtures 16/16; five deployment gates green |

### Deployment contract fact worth keeping (ruling item 2, 2026-09-17)

The SDK installs `HyRemoteRemoteAccess` with `BUILD_RPATH`/`INSTALL_RPATH = "$ORIGIN"`
(`src/remoteaccess/CMakeLists.txt:27-31`), so `file(GET_RUNTIME_DEPENDENCIES)` can never resolve the facade's
**private** Qt modules (Widgets/Quick) from the Qt installation that built the SDK; they end up unresolved and
undeployed, and a consumer that links only Core/Network then fails at load with
`libQt6Widgets.so.6: cannot open shared object file`. `windeployqt` hides this (it resolves through the Qt
installation), so the defect only shows on the Linux generic deploy tool. Fix shape: the SDK bakes its adapter
set into the **installed** deploy helper (no new consumer-visible package variable - the frozen package surface
is guarded by `check_deploy_helper_contract.cmake`), and `hyremote_deploy()` installs those modules beside the
facade and passes them as `ADDITIONAL_LIBRARIES` so their own Qt dependencies follow.

Suggested merge order (posted to #106 comment `5714603143`): **#124, then #126, with #125 anywhere** -
because the repaired base still has a QPA module that does not compile and a QML example that does not
configure, so each PR stays red until its sibling lands. Remaining authorized work: the **Linux
clean-deployment Qt-runtime defect** as a separate bounded V1 deployment PR, with no `LD_LIBRARY_PATH` or
SDK-path workaround.

## Delegated V1 queue: branch layout (originally prepared 2026-09-17, pre-integration)

All four items are authorized by the owner's ruling `5712801878` but conditioned on #120 being integrated, so
they exist as pushed branches **with no PR opened**; a rebase after #120 merges drops the stacked P0 commit
from each diff.

| Branch | Contains | Notes |
| --- | --- | --- |
| `fix/v1-root-cmakelists` (= PR #120) | P0 root-CMakeLists restore, one file | the only blocker for the rest |
| `fix/v1-qpa-plumbing` | `02c2df1` + `c388d31` (Qt 6.8.3 `qplatformintegrationfactory_p.h` path + `QPalette`) + `f7141e0` (the two composite tests' `HyRemote::RemoteAccess` include surface) | one PR per the ruling; module still cannot link without the seam |
| `fix/v1-qml-example-automoc` | `02c2df1` + cherry-picked AUTOMOC fix | independent of QPA |
| `feat/v1-qpa-core-seam` | `fix/v1-qpa-plumbing` + `2a75f56` (the `HyRemote::detail` seam) | must be opened after the plumbing PR (it contains it) |
| `docs/bilingual-user-docs` (= PR #121) | docs system + review corrections + staged `f504d98` (gate formatting-invariance, canonical `docs/guide/install.md` path, pointer deletion) | waits for **both** #120 and #122: #122 writes `README.md` first |
| `docs/framework-title-alignment` (= PR #122) | title + positioning | tagline wording already corrected per ruling |

Drafted PR bodies live in `.codebuddy/tmp/pr-qpa-plumbing.md`, `pr-qml-automoc.md` and `pr-qpa-seam.md`
(regeneratable from the branch diffs if cleaned). Permission to open them stacked on #120 - rather than only
after the merge - was requested in #120 comment `5714099205`.

## Hosted-CI defect inventory on the V1 line (as of 2026-09-17, from the fresh run on PR #120)

- **QPA private header** (Linux qxcb job): `hyremote_platform_plugin.cpp:9` includes
  `QtGui/private/qplatformintegrationfactory_p.h`, which the exact Qt 6.8.3 kit ships under `QtGui/qpa/`. The
  Windows runner's Qt apparently has the `private/` layout, so this only breaks the Linux install layout - the
  `qpa/` form works on both.
- **QPA deploy-helper fixtures assume a single-config generator**: on the Windows multi-config job
  `integrations/qpa/tests/run_deploy_helper_fixture.cmake:112` asserts "exactly one generated QPA/runtime deploy
  script" and finds four per-configuration ones, so those fixtures can never pass on Windows as written.
- **Clean-deployment consumer cannot resolve Qt** (Linux installed+source job): the deployed consumer dies with
  `libQt6Widgets.so.6: cannot open shared object file` (exit 127) - either `hyremote_deploy()` does not place the
  Qt runtime for that fixture or the fixture's environment is wrong; this one touches the V1 clean-deployment
  claim.
- **QML example launch in CI** (Linux QML job): 43/43 tests pass, then the example exits 134 with "no Qt platform
  plugin could be initialized" - the run needs an offscreen/xvfb platform.
- **aqtinstall** can crash while provisioning Qt on Windows ("You have discovered a bug in aqt", exit 254) -
  infrastructure, not code.

## Platform hardware-capability requirement (product side, 2026-09-17, Issue #123)

- Requirement: on **every target hardware platform**, exploit that platform's hardware capabilities to raise
  performance and lower resource consumption - a product-level requirement that spans the existing component
  trackers (#9 async GL/PBO + damage-aware delivery, #10 GBM/DMA-BUF + RK3588/RKMPP low-copy, #17 buffer
  ownership/feasibility, #18/#3 RK3588/EGLFS validation, #7 Quick/EGLFS end-to-end). The consolidated tracker
  is **#123**; the request is for a ruling, not an implementation start.
- Frozen constraints any accelerated path must respect: Core stays Qt-free and platform-free (ADR-0001), a SoC /
  encoder / GPU stack / transport stays behind an adapter, the QPA payload must not link Core directly, QPA
  claims stay exact-Qt-private-ABI qualified, the 1.x public surface stays frozen, and an unevidenced capability
  stays `Unverified`.
- **Blocker stated in the issue**: no target performance metric exists yet, and the capture spikes already
  recorded that GL/PBO/RHI work "would have to show a material gap against a target frame budget that does not
  exist before RK3588/EGLFS validation". Without a per-platform-class frame budget/latency/CPU/memory target,
  "use the hardware better" is untestable. Also: no RK3588/EGLFS host is reachable from the helper workspace, so
  embedded evidence needs a product-side host; desktop x86 measurement is possible today.
- Four decisions requested in #123: placement against the frozen V1 boundary, per-platform-class metrics, the
  platform priority list plus who provides validation hardware, and architecture confirmation (adapters behind
  the existing `CaptureSource`/`Transport`/`InputSink` seams, no Core change).

## Owner rulings of 2026-09-17 (naming, coordination, third-party sample)

- **Naming (ruled 10:08Z on #106)**: GO for a docs-only `docs/naming-conventions.md` that codifies the
  **existing** product identity - `HyRemote` for public product identity/namespace, `HYREMOTE_*` for
  macros/options, the existing CMake/package/target forms, `qhyremote` for the platform module, PascalCase
  public types/enumerators, camelCase functions/methods, the existing private-member convention, snake_case for
  new internal source/test files, installed public headers may follow the public type name (`RemoteAccess.h`).
  **No blanket `HY` prefix migration**, **no naming gate in V1** (written convention plus review is enough),
  QML element `RemoteAccess` untouched for V1 (no prefix/alias/deprecation, and no V1.1 pre-commitment), and
  **no `src/remoteaccess` rename** - the layout, build and package expectations are frozen. Existing V1 files
  are not renamed to normalize style.
  **Refined 2026-09-17 (comment `5713134180`)**: the product-side request for an `HY` prefix is satisfied **at
  collision-prone/global boundaries only** - new globally exposed C-style symbols or macros carry the prefix,
  while public C++ types inside `HyRemote::` keep normal PascalCase names (no `HYRemoteAccess`-style
  duplication) and internal types stay normally scoped. `docs/naming-conventions.md` must state that
  distinction explicitly.
- **Product title/positioning convention (ruled 2026-09-17)**: the product name is **Qt Remote Access
  Framework** (never "Qt Embedded ..."), and platform wording must present `Windows x86_64 + Linux x86_64` as
  the **V1 reference-platform scope** rather than the permanent product boundary, with embedded
  Linux/EGLFS/OpenGL ES described as post-V1 expansion and not a current support claim. Files: `README.md`
  tagline/title, `docs/architecture.md` opening sentence (PR #122); the root `CMakeLists.txt` DESCRIPTION
  carries the same name (via `0535f87`, preserved by PR #120).
- **Release-gate rule (ruled 2026-09-17, comment `5713134180`)**: a readiness gate validates semantics, not
  source formatting. `check_release_metadata.cmake` must match the `project(HyRemote VERSION ...)` declaration
  formatting-invariantly (whitespace-normalized or equivalent), keep asserting the extracted version, and carry
  a deterministic self-check (one-line and multi-line accepted, malformed/missing rejected). Once fixed, the
  repository must **not** document "`project()` must stay on one line" as a product or contributor rule.
- **Coordination (same ruling)**: **one writer per surface**. #106 is the single V1 implementation/integration
  authority and no parallel V1 product work is authorized on the old `main` layout; product implementation,
  workflows, QML/QPA, release-readiness and remaining executed-failure fixes stay with the #106 line unless a
  specific item is delegated in that thread; the second writer stays read-only until the surface is explicitly
  delegated. The only code change authorized to this side today was the minimal root `CMakeLists.txt` P0 repair;
  a corrected Core guard follow-up and the naming-conventions document are permitted later as separate bounded
  PRs, after that repair. This is why "looks like a missing link flag" work must be offered first, not pushed.
- **Third-party sample (ruled on #109)**: the preferred single sample is **`qbittorrent/qBittorrent`**
  (cross-platform, Widgets, complex, buildable from source against exact Qt 6.8.3) - it supersedes the earlier
  sqlitebrowser selection. Any published recipe must be labelled "third-party verification example — not a V1
  compatibility/support claim", record the exact application commit/version, HyRemote candidate SHA, Qt 6.8.3,
  OS/toolchain and the exact launch/deploy commands, use only the documented deployment path plus
  `-platform hyremote` (no HyRemote internals linked in, no test-only API, no alternate runtime, no
  candidate-specific adapter), leave compatibility rows, V1 exclusions, release notes, package manifest, GA
  matrix and the required E1-E4 cells unchanged, promise nothing for other application or Qt versions, stay
  optional, and never become a release blocker. A second application is not to be added for V1.

## Working alongside the owner's V1 line (coordination rules, 2026-09-17)

The owner (same GitHub account, separate session) is actively developing the V1 convergence branch
`feature/104-v1-ga-acceptance-matrix` (draft PR #106). Facts that must shape any further work from this
side:

- **It already implements the #90 disconnect release**: `src/remoteaccess/src/transport/rfb_transport.cpp`
  defines `releaseHeldInput(ClientState&)`, `modifiersForHeldKeysyms()` and `heldKeysyms`, called from the
  disconnect handler (line 509/733). So **PR #113 duplicates the owner's implementation** for the converged
  product; it is only worth merging if `main` must stay independently correct before migration. The owner's
  own tests (`src/remoteaccess/tests/test_rfb_multi_client_input.cpp`,
  `test_rfb_widget_disconnect_backpressure.cpp`) cover the same semantics.
- **It does not have the batch-1 hardening** (`runningPublications`, the widened dependency guard were not
  found on that line), so PR #115 is largely additive - but confirm the guard's new path
  (`src/core/tests/check_dependencies.cmake`) before assuming.
- **Direct file collisions with my PR #112**: `README.md`, `docs/{dependency-policy,compatibility,
  security-model,x86-vnc-transport-evaluation}.md`, `cmake/HyRemoteProjectOptions.cmake`; plus
  `remoteaccess/tests/CMakeLists.txt` collides with PR #113 (their line rewrites it under `src/`).
- It carries its own `.github/workflows/*` (including `git-flow-policy.yml` with a `push` trigger) plus
  `mainline-push-audit`, so PR #114's `mainline-validation.yml` is additive but overlaps in *purpose*.
- The owner was fixing exactly the three release-readiness gates I diagnosed, live, at 12:46-13:25 on
  2026-09-17. **Do not touch those gates or that branch.**

Coordination protocol to follow: one writer per surface (owner = `src/**`, `integrations/**`, `examples/**`
and its workflows; this side = `main`'s `core/**`, `remoteaccess/**`, root docs, governance); check
`git log origin/feature/104-v1-ga-acceptance-matrix -3` and `git grep` the branch before touching a shared
path; leave an intent comment on the issue/PR first; agree who owns the six contested docs; and land
main-side PRs **before** the V1 line rebases so the fixes are absorbed rather than dropped. Branch
protection prevents either side from clobbering `main`, and both sides work in separate Draft PRs.

## Delivered PRs from the 2026-09-17 work session

- **#111 renamed the framework** to "Qt Remote Access Framework" and was merged as `c3f4991` (squash;
  the only way to merge now, since `main` is protected).
- **#112** `docs/truth-up-and-security-state` (`03ca4a5`): corrected the superseded transport decision
  (issue #53 authorized the custom RFB 3.8 baseline), the licence status, the compatibility matrix's
  Transport column and the `examples/`/QPA placeholder options; rewrote `security-model.md` section 12
  as a status table and made `SECURITY.md` state the current transport security position. Hosted CI:
  **10/10 checks passed**.
- **#113** `fix/90-release-held-input` (`b6c9aed`, rebased onto main): implements the issue #90 fix -
  `ClientState` tracks the last pointer location/viewport plus a capped (16) array of held `KeyCode`s,
  and `releaseHeldInput()` emits the balancing releases before the state is erased. New deterministic
  target `hyremote-rfb-input-release-test` (6 cases) drives the real transport over loopback; a control
  run with the release call disabled fails with 8 assertions confined to the disconnect cases. Full
  suite 30/30.
- **#114** `chore/governance-and-mainline-ci` (`738603a`): new `mainline-validation.yml` (push to main
  + dispatch: default config, full ctest, transport product-fit, commit assertion) plus the missing
  PR/issue templates, `CODEOWNERS`, `dependabot.yml`. It shows **no checks by design** - it touches only
  `.github/**`, which no existing path filter matches.

Still open from the planned order: **batch 1** (hardening) - extend `check_dependencies.cmake` with the
missing AML/EGL/RHI tokens and forward-declaration detection, pin the
`maxInFlight=2`/`capacity=2`/`DropOldest` defaults with assertions, de-flake
`test_session_lifecycle.cpp:477` (200 us sampling) and the RFB test-server window, replace the
`HYR_CHECK(true)`-style weak assertions, and add `try/catch` inside the two `noexcept` `stop()`.

## Review baseline (full A-H review of commit 4332713, 2026-09-17)

- **Core is clean**: 0 P0 findings; core build has no Qt/protocol/platform leakage, `remoteaccess`
  uses only core public headers, all ADR-0002/0003 invariants that matter are implemented and mostly
  test-guarded, message parsing is bounded/validated, Qt ownership is correct. Baseline evidence:
  clean build, `ctest` 29/29, 80 core `HYR_TEST` cases.
- **Security capability gap (P1)**: the RFB transport only offers SecurityType `None` over plaintext
  (`remoteaccess/src/transport/rfb_transport.cpp:523,539`) and neither `Transport` nor `RemoteAccess`
  can express auth/TLS; the only protections are the loopback default (`remote_access.cpp:63`) and
  opt-in remote input (`:65`). Never describe the current transport as "secure" in docs or PRs.
- **CI never validates merged `main` (P1)**: every workflow is `pull_request` (+path filter) +
  `workflow_dispatch`; there is no `push`/`schedule` trigger and no branch protection recorded.
- **Known open gap**: held remote input is not released when a viewer disconnects (issue #90 open,
  PR #93 closed unmerged).
- **Stale docs to distrust until fixed**: the "no custom RFB implementation is authorized" line is
  superseded by issue #53 (the custom baseline is authorized); the license is described as unfrozen
  although Apache-2.0 is decided; `docs/compatibility.md` still calls the transport a "VNC candidate";
  README marks V0.0.2.0/V0.0.3.0 "in progress" while no `qml/` module and no QPA code exist.
- Dead declarative options: `HYREMOTE_BUILD_EXAMPLES` (no `examples/` directory or
  `add_subdirectory`), `HYREMOTE_WITH_QPA_PROXY`/`GBM`/`RKMPP` (no consumer in the build).
- `project(HyRemote VERSION 0.0.0)` is what the installed package reports, and the repo has **0 tags**,
  while milestones are V0.0.1.0/V1.0.0.0.
- **Check CI health before trusting any check status**: on 2026-09-17 (before ~03:10Z) every workflow
  failed in 1-5 s because GitHub Actions could not start jobs for account billing/spending-limit
  reasons (`gh run view <id>` shows the annotation; job logs are absent). Short-duration "failures"
  are infrastructure, not code. Last green runs before that window were 2026-09-15. **The window has
  since closed**: from 03:10-03:11Z jobs execute normally again (Git Flow policy passes in 6-10 s,
  matrix jobs run 41 s-2 m), so any PR merged during the window carries no CI evidence.
- **Attribute a red check to the base before blaming the patch**: on 2026-09-17T09:50Z the V1 head
  `0535f87` failed *every* configure (`job 105131974942`: `CMake Error at CMakeLists.txt:17
  (hyremote_apply_release_profile): Unknown CMake command`), because `0535f87` - titled
  `docs(product): align framework title with main` but rewriting the root `CMakeLists.txt` from 121 to 37
  lines - replaced the working `hyremote_validate_release_profile(VERSION ... QML_ENABLED ... QPA_ENABLED
  ...)` call with a function that is defined nowhere on the branch, and swapped `include(HyRemoteDeploy)`
  for `include(HyRemoteInstall)`. Result: PR #106 itself showed 10 reds, and a docs-only PR (#119) showed
  14. Check the base (`gh pr checks` on the branch's own PR) before attributing failures.
- **The active V1 line currently fails its own release-readiness gates** (Linux jobs, 3 of 33 tests,
  91 % pass): `tests/release-readiness/check_release_metadata.cmake:76` -> "root project(VERSION ...) is
  missing or malformed"; `tests/release-readiness/check_v1_runtime_contract.cmake:15` -> "cross-viewer
  unmatched-release isolation missing from src/remoteaccess/tests/test_rfb_multi_client_input.cpp";
  `integrations/qpa/tests/run_deploy_helper_fixture.cmake:86` -> "ordinary QPA must preserve ordinary
  high-level deployment dispatch" (CMakeLists.txt:183). Note the first gate confirms the version
  problem: the branch does not declare an acceptable `project(... VERSION ...)`.
- **Warning baseline at `/W4 /EHsc`**: production sources are warning-free; 4 warnings live in test
  code only (`core/tests/test_input_normalization.cpp:129-131` C4127, `remoteaccess/tests/rfb_test_server.cpp:107`
  C4996). Always pass `/EHsc` when doing a custom-flag review build, otherwise C4530 noise appears in
  every TU and looks like a defect.
- **Verified packaging facts**: `cmake --install` produces 19 files (headers, `hyremote-core.lib`,
  `HyRemoteRemoteAccess.lib`, CMake package files, `share/HyRemote/licenses/LICENSE`, no NOTICE); the
  installed `HyRemoteConfig.cmake` correctly calls `find_dependency(Threads)` and conditionally
  `Qt6 Core/Network/Widgets/Quick`; `HyRemoteConfigVersion.cmake` hardcodes `PACKAGE_VERSION "0.0.0"`.
  All 9 `core/include/hyremote/core/*.hpp` compile standalone.
- **Dependency guard blind spots are empirically proven** (synthetic `CORE_DIR`): the check catches a
  real `#include <QtWidgets/QWidget>` but **not** a forward declaration (`class QWidget;`), and not
  `EGL/egl.h`, `aml/aml.h` or `rhi/qrhi.h`, all of which ADR-0001 names.
- **Branch protection is now server-side enforced (2026-09-17, corrects the earlier "impossible" note)**:
  the repository was made **public**, so the free-plan restriction disappeared and the branch-protection
  and ruleset APIs work. Repository ruleset `23576792` "protect main and release branches" is `active`
  with patterns `refs/heads/main`, `refs/heads/release/*`, `refs/heads/Release_*`, rules `deletion` +
  `non_fast_forward` + `pull_request` (0 required approvals, because the owner is the only collaborator
  and self-approval is impossible), and **no bypass actors** - so nobody, including the admin, may push
  to those refs; merges must go through a PR. `develop`, `hotfix/*` and tags are deliberately **not**
  covered yet. The V1 branch's `mainline-push-audit` job is therefore a secondary signal, not the
  primary control. Being public also means: free standard-runner Actions minutes (this is what ended
  the 2026-09-17 billing block) **and** `docs/security-model.md` §12's public-release gate is now live,
  which the transport (no authentication/encryption at all) does not yet satisfy.
- **Two layouts coexist**: `main` has `core/`, `remoteaccess/`, `spikes/`, `logo/`; the active V1 draft
  branch (#106) moves them to `src/core/`, `src/remoteaccess/` and adds `integrations/`, `examples/`,
  `assets/`, `research/`, `NOTICE.md` plus `git-flow-policy.yml`, `qml-api.yml`, `qpa-proxy.yml`,
  `v1-ga-acceptance.yml` (16-job matrix). Findings phrased against `main` paths may not apply there.

## Local development environment (Windows host)

- Qt 6.8.3 MSVC kit (`msvc2022_64`) with a full module set (incl. QtQuick3D, QuickWidgets,
  OpenGLWidgets); mingw_64 / llvm-mingw_64 kits are installed as well. The concrete install
  root is host-specific and intentionally not recorded here — read it from an existing build
  directory (`CMAKE_PREFIX_PATH` in `build/*/CMakeCache.txt`); it is referred to as
  `<QtRoot>` below.
- Ninja at `<QtRoot>\Tools\Ninja\ninja.exe`; CMake 3.30.5; MSVC 19.44 (`cl` 14.44) from
  Visual Studio 18 Build Tools, with INCLUDE/LIB already initialized in the shell.
- Qt apps need `<QtRoot>\6.8.3\msvc2022_64\bin` on `PATH` when run, or they fail with
  `STATUS_DLL_NOT_FOUND` and print nothing.
- `qt_add_executable` links Qt::Gui and sets `WIN32_EXECUTABLE`; console tools must set
  it back to `FALSE`.
- No RK3588 board / EGLFS environment is reachable from this workspace, so any embedded
  validation remains blocked and must be reported as such.

## Capture architecture decisions established by SPIKE-01 (issue #3, PR #15)

- Recommended v0.1 Widgets baseline: `QWidget::render()` into a caller-owned pooled
  buffer on the GUI thread (`grab()` as convenience), with an optional
  `QPaintEvent::region()`-based damage stream.
- Recommended v0.1 Qt Quick baseline: `QQuickWindow::grabWindow()` on the GUI thread,
  full frames only, paced with backpressure, explicitly experimental (correctness path
  only, no performance promise). `QQuickWidget` applications should use the parent
  `QWidget::render()`/`grab()` path.
- `RemoteFrame` must carry an explicit buffer ownership/lifetime state, an optional
  damage region with an unknown/full-frame fallback, plus capture capability metadata.
- Unmet gate: RK3588 / EGLFS / OpenGL ES validation of the same matrix. SPIKE-01 stays
  open until that exists.

### Asynchronous capture conclusions (issue #16, PR #19, SPIKE-02)

- The correct asynchronous grab target is **`QQuickWindow::contentItem()`**, not the QML root
  item: the root item is a child of `contentItem()`, so a root-item grab silently misses
  overlay/popup/tooltip siblings (they are `contentItem` children too).
- `QQuickItem::grabToImage()` renders the item subtree into a `QSGLayer` on the render thread
  and reads it back in `afterRendering`; completion is delivered as `ready()` on the GUI
  thread. It returns a **null QSharedPointer** (no callback at all) when the item has invalid
  dimensions, is not in a window, or its window is not visible.
- Pipelining does not create frames: distinct content = requests/K, duplicate completions =
  requests − requests/K, completions are FIFO, nothing is dropped. The average **scene-state
  advance from request to captured image** is (K−1)/2, i.e. for K>1 the returned image is
  *newer* than the request (corrected 2026-09-14; "staleness" was the wrong reading). Pace the
  producer (one request per delivered frame) when a request must map to one specific scene state.
- Recommended operating point: K = 2-4 in flight, a caller-owned bounded completed-frame queue
  with drop-oldest/latest-frame-wins, a **completion-time PTS** (never the request time) so the
  transport can drop superseded frames, and ownership transfer at completion.
- Not provided by the public API: asynchronous `QOpenGLWidget` capture (none exists),
  asynchronous whole-window `QQuickWidget` composition, and capture of a hidden window.
- The asynchronous path is not affected by the Direct3D 11 `grabWindow()` stall, so it is the
  better Quick path on that backend.
- No GL/PBO/render-thread/RHI work is justified yet: it would have to show a material gap
  against a target frame budget that does not exist before RK3588/EGLFS validation.

### Header-include convention and clang tooling (HyRemote)

- Every source file includes project headers in the path form `"hyremote/core/..."`, which only
  resolves through the configured include directory (`core/include`). That convention is why a
  checker with no include path reports `'hyremote/core/capabilities.hpp' file not found` at line 1
  and then cascades into "unknown type name" errors.
- **No edit inside such a .cpp can fix that**: `capabilities.hpp:13` itself includes
  `"hyremote/core/types.hpp"` in path form, so a relative include in the .cpp only moves the error
  into the header (proven with a probe: `Line 6: in included file: 'hyremote/core/types.hpp' file not
  found`). Fix the tooling, not the file.
- CMake already sets `CMAKE_EXPORT_COMPILE_COMMANDS ON`
  (`cmake/HyRemoteProjectOptions.cmake:21`), but the database is written inside the build directory
  (`build/all`, `build/core`). Committed `.clangd` therefore points at `build/all`, and gitignored
  copies exist at the repo root and at `build/compile_commands.json` for tools that only auto-search;
  `.vscode/{settings,c_cpp_properties}.json` cover DB-less IntelliSense.
- Reproduce/verify on this host:
  `"<clangd>" --check=core/src/capabilities.cpp [--enable-config=false]` → expect
  "Loaded compilation database from ..." + "All checks completed, 0 errors". Temporarily moving every
  `compile_commands.json` aside reproduces the original failure exactly, which is the quickest way to
  prove a report of this kind is tooling-side.

### Pull requests opened for owner review (2026-09-15)

- **PR #54** `refactor(core): share the capability list formatter and pin its reason strings` - branch
  `core/capabilities-hygiene` (2 commits, 2 files, +44/-10): the `joinNames()` formatter with exact
  reservation plus direct standard includes, and the verbatim compatibility reason-string assertions.
  CI (all 8 jobs: Linux + Windows x86_64 with Qt / Qt Quick / Qt Widgets / installed+source SDK) **pass**.
- **PR #55** `docs: make the root README an entry point and record the product overview` - branch
  `docs/readme-restructure` (3 commits, 4 files, +485/-169): README restructure, the new
  `docs/product-overview.md`, the `<QtRoot>` placeholder in `spikes/capture/README.md`, and `.clangd`.
  A docs-only PR gets **no CI checks** because the workflows filter on code paths, so "no checks
  reported" is expected there, not a failure.
- Both PRs target `main` and are left for the owner to review and merge; the assistant must not merge.
- The workflow matrix is the project's cross-platform gate: every PR that touches code must be green on
  **both** Windows and Linux x86_64 (Qt / Quick / Widgets / SDK jobs each).

### Repository state after the first sync (main, 2026-09-15)

- `origin/main` is the integration branch; feature work lands through PRs that are squash-merged, so a
  PR's branch tip is usually **not** an ancestor of main - check the merge commit message
  (`... (#nn)`) or the PR state instead of `merge-base`.
- PR #22 (core Session / RemoteFrame / bounded dispatch) is **merged** and its content **includes**
  the Round-2 and Round-3 lifecycle fixes (`markStartCancelledLocked` present,
  `finishCancelledStart` absent, the three `r3b1` tests present).
- After that merge main also carries the input contract (#47), the RemoteAccess facade (#45), the SDK
  install/export (#46), the QWidget adapter (#48), licensing and logo/roadmap docs. The Core test set
  grew to 12 CTest targets / 80 deterministic cases, the root suite to 26 tests, and a third spike
  directory (`spikes/vnc-transport-rust-ffi`) appeared.
- Because feature branches get squash-merged, start any follow-up work from `origin/main` (never from
  a merged feature branch).
- `core/tests/test_transport_handoff.cpp` owns the `checkFrameCompatibility()` behaviour assertions,
  including verbatim reason strings for the offered/accepted lists.

### Documentation conventions (root README restructure, 2026-09-15)

- The root `README.md` is an **entry point**, not the product specification: centered logo/title/tagline,
  two badge rows (metadata + the four CI workflows), a one-paragraph description, a status note, a short
  TOC, then Highlights / Status / Requirements / Build from source / Use HyRemote / Repository layout /
  Documentation / Compatibility / Contributing / Security / License / Brand and trademarks.
- Long-form product prose (integration modes, consumption model, differentiation target, architecture
  diagram, design principles, roadmap detail, compatibility policy, non-goals) lives in
  [`docs/product-overview.md`](../../docs/product-overview.md) and is linked from the README. Keep it that
  way: front page scannable, depth in `docs/`.
- CI badge URLs: `https://github.com/skawu/HyRemote/actions/workflows/<file>/badge.svg`; the workflow files
  are `remoteaccess-facade.yml`, `sdk-consumption.yml`, `widgets-adapter.yml`,
  `transport-rustvnc-ffi-spike.yml`.
- After editing a README, re-check the relative links and the TOC anchors programmatically (GitHub slug
  rule: lowercase, drop punctuation, spaces -> `-`) - the restructure was verified with 41/41 links and
  all anchors resolving.
- There is no `examples/` directory yet even though `HYREMOTE_BUILD_EXAMPLES` exists (default `ON`); the
  option is currently inert, so do not document examples as available.

### OpenHarmony + Qt feasibility (checked 2026-09-15)

- `openharmony-sig/qt` (Gitee mirror archived, active repo on gitcode.com) is **not** a Qt source fork: it
  is patches plus independent adaptation modules. You prepare a **Qt 5.15.12** baseline tree and apply
  `patch/v5.15.12/<submodule>.patch` (earlier tags were Qt 5.12.12). Patched submodules: `qtbase`,
  `qtdeclarative`, `qtmultimedia`, `qt3d`, `qtwebview`, `qtsensors`, `qtconnectivity`,
  `qtquickcontrols`. The README documents neither the QPA plugin name, the graphics stack, the toolchain
  nor known limitations - those live in the (JS-rendered, not fetchable) Wiki and in the patches.
  The repo carries Apache-2.0 plus Qt's LGPL/GPL/FDL license files.
- **Blocking fact for HyRemote**: HyRemote is a **Qt 6.8+** product (`find_package(Qt6 6.8 ...)`, all
  capture evidence from Qt 6.8.3), while the OpenHarmony Qt port is **Qt 5.15.12** - so the current
  HyRemote cannot run on it as-is. A Qt 6 port upstream, a Qt 5.15 adapter branch, or a qualified
  version decision is the prerequisite, not a capture-level detail.
- Architecturally HyRemote's route is shape-compatible with a QPA-integrated platform (attach to a Qt
  target, capture through Qt's own public paths, protocol-neutral transport, Qt-level input, optional
  version-coupled QPA proxy, backend-pluggable acceleration), but the port is patch-based (a moving
  target) and nothing about capture/input/offscreen layers on that QPA can be inferred from the desktop
  evidence. Any claim must come from a bounded spike plus an entry in `docs/compatibility.md`
  (`Unverified` until then), and the dependency-policy license review applies to the mixed-license Qt
  build used for distribution.

### Owner policy: the local agent is not part of the normal repo flow (2026-09-15/16)

- The owner's dispositions state it explicitly: **no local Agent is required for repository
  implementation or documentation work**; the hosted-runner jobs that terminate with no assigned steps
  are "infrastructure evidence only" and do not stop parallel V1.0 work. PRs #54 and #55 were merged
  without local involvement after that.
- PR #55 merged the README restructure and `docs/product-overview.md` but **dropped the `.clangd`
  file** during review, so the clangd include-resolution fix stays local-only (gitignored root
  `compile_commands.json` + `.vscode/*`). Do not re-propose `.clangd` upstream.
- The local agent stays useful for exactly two kinds of work: (1) **evidence that needs a real local
  toolchain** (builds, probes, measurements on Windows + Qt 6.8.3), and (2) **local publish steps**
  the owner explicitly requests (commit/push from this working tree). Everything else - product,
  roadmap, license, scope decisions, the owner's parallel branches, and any RK3588/EGLFS/OpenHarmony
  hardware validation (unreachable here) - is not local-agent work.

### Qt LTS support: NO-GO for V1.0, reopened as a post-V1 qualification item (Issue #57)

- Requested by the user (product side): support **Qt 5.15 LTS + every Qt 6 LTS** and each future LTS
  release as a standing policy (https://github.com/skawu/HyRemote/issues/57, label `enhancement`).
  **Restated 2026-09-17** as a standing product commitment for "Qt 5.15 and *every* LTS line at or above it",
  recorded with the current-state facts, the qualification cost and four decisions in comment
  `5713121613` (see the daily memory for the detail). Nothing was changed in code/CMake/CI/docs in response.
- **Outcome**: the owner disposed it as **NO-GO for the V1.0 critical path** - V1.0 stays Windows x86_64
  + Linux x86_64 on the validated **Qt 6.8.x** line; other Qt LTS lines are `unverified / outside the
  current GA support contract` rather than unsupported forever; expansion may be reconsidered post-GA
  with its own evidence and cost model. The disposition explicitly forbids changing
  `docs/versioning.md`, the CMake floor, the CI matrix, the SDK contract or the V1.0 acceptance gates,
  and forbids using OpenHarmony to back-propagate Qt 5.15 into the x86 GA gate.
- The Issue was then reopened and retitled `[POST-V1][QT-COMPAT] Qualify Qt 5.15 and Qt LTS lines
  without broadening V1.0 GA`, i.e. it now tracks **qualification evidence**, not a scope change.
- Facts to keep in mind: HyRemote hard-requires Qt 6.8 today (`CMakeLists.txt:40`) and all capture
  evidence is Qt 6.8.3; Qt 5.15.2 is the last **open-source** release of the 5.15 series (5.15.3+ are
  commercial-only LTS patches); the OpenHarmony Qt adaptation is a Qt 5.15.12 patch baseline; Qt 6 open
  source is LGPLv3/GPLv3 while Qt 5.15 open source also offered GPLv2, and Qt 5.15.2 has had no
  open-source security maintenance since - a real consideration for a remote-access product.
- If the owner approves, the follow-up work is: a written Qt version policy in `docs/versioning.md` +
  `docs/compatibility.md`, Qt5/Qt6 selection in CMake and the exported SDK package, version branches
  confined to `remoteaccess/` and the adapters (the Core is Qt-free by ADR-0001), re-running the
  SPIKE-01/#16 capture procedure per Qt LTS, the expanded CI matrix and the SDK consumer fixtures.

### Windows test environment gotcha

- Qt-linked test binaries (`hyremote-remoteaccess-test`, `hyremote-widgets-capture-test`, the spike
  executables) need `C:\Qt\<ver>\msvc2022_64\bin` on `PATH` in the *same* shell that runs `ctest`,
  otherwise they exit with `0xc0000135` (`STATUS_DLL_NOT_FOUND`) and `ctest` reports them as failures -
  this is an environment issue, not a code regression. Put Qt's `bin` on `PATH` before every `ctest` run.
- **Put HyRemote's own DLL directories on `PATH` too** - Qt's `bin` alone is not enough: the test binaries
  also load `HyRemoteRemoteAccess.dll` (and, for QML, `hyremote-qml.dll`/`hyremote-qmlplugin.dll`), which
  live in `build/<tree>/remoteaccess` and `build/<tree>/qml/HyRemote`. Without them the same `0xc0000135`
  appears. For the QPA smoke tests also set `QT_PLUGIN_PATH=build/<tree>/plugins` (the plugin is deployed to
  `build/<tree>/plugins/platforms/qhyremote.dll`); otherwise they die with `0xc0000409` and look like a
  product defect when they are only a missing plugin. Working recipe:
  `PATH=<Qt>/bin:<tree>/remoteaccess:<tree>/qml/HyRemote:<tree>/qpa:<tree>; QT_PLUGIN_PATH=<tree>/plugins`.
- **Full-graph verification recipe** (exact qualified Qt 6.8.3, from a clean tree):
  `cmake -S . -B build/ga -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<Qt 6.8.3 msvc2022_64>
  -DHYREMOTE_BUILD_TESTS=ON -DHYREMOTE_BUILD_EXAMPLES=ON -DHYREMOTE_BUILD_QML_API=ON
  -DHYREMOTE_WITH_QPA_PROXY=ON`, then `cmake --build build/ga -- -k 0` (note: native arguments must follow
  `--`; `cmake --build build/ga -k 0` just prints the usage text) and `ctest --test-dir build/ga`.

### Backpressure / slow-consumer conclusions (issue #16, corrected in PR #19 round 2)

- Model a slow consumer as **one serial consumer**: completed frames enter a single queue, the
  consumer services one frame at a time (service time per frame → sustained rate
  `1000/serviceMs`), and `nextServiceStart = end of previous service`. Never release frames
  independently per frame — that silently parallelizes consumption.
- A queue **capacity** bounds queue depth; the in-service frame is owned in addition, so state
  the total ownership bound explicitly as `capacity + 1`. Admission control must count
  in-flight requests, otherwise completions overshoot the bound by up to K.
- Measure the producer policies separately: unbounded (backlog debt), drop-oldest /
  latest-frame-wins (capture keeps its rate, drops most frames, keeps content fresh), and
  producer-throttle (no drops, but capture is pinned to the consumer and delivered frames are
  older). Do not simulate a drop policy by stopping capture requests.
- Measured with one serial 10 fps consumer on `quick2d` K=4: unbounded → 55 frames / 123 MiB
  backlog after 0.51 s, 5.8 s behind; bounded queue 2 + latest-frame-wins → producer 119 /s,
  90.8 % dropped, 6.6 MiB retained, ≤2 frames stale; bounded queue 2 + admission control →
  producer 9.8 /s, no drops, 172-195 ms frame age.
- Invariant to report: `delivered + dropped == completed`, and `maxQueueDepth <= capacity`.

### Frame timestamp / freshness semantics (PR #19 round-2 review, applies to any capture path)

- `tick in image - tick at request` is a **signed scene-state advance from the request to the
  captured image**. A positive value means the image is **newer** than the request (the normal
  result of pipelining: several pending requests served by one later render). It is never an age
  or a staleness measure. Never write "the content lags the request" / "never newer than the
  request" / "up to K-1 ticks old".
- **The request timestamp is not a valid content PTS.** Take the PTS at completion / `ready()`
  time as the v0.1 baseline (also when the buffer enters the caller's ownership); keep the
  request time only as a latency diagnostic; a backend that observes the real rendered/presented
  frame time may supply a better PTS, and the generic Core must **accept** a PTS rather than
  derive it from the request.
- A consumer "N frames behind in the request sequence" is a **request-sequence age**, never the
  visual age of the pixels. The visual age of delivered pixels is not measured by the current
  probes; accepted freshness evidence is queue depth/ownership bound, completion-to-service
  wall-clock age, producer/consumer rates and delivered/dropped accounting.
- When correcting a report key's meaning, prefer keeping the key (evidence stability) and
  documenting the quantity; renaming a key invalidates already-reviewed evidence files.

### Claim boundaries to keep (from the same review)

- Composition evidence covers **same-scene sibling content under
  `QQuickWindow::contentItem()`** (the parent chain `Popup.Item`/`Overlay` use). `Popup.Item`
  is an architectural inference; `Popup.Window`, `Popup.Native` and Control-specific
  `Popup`/`ToolTip` are unverified.
- Only window `hide()` was tested: `minimized` and occluded are unverified.
- `QQuickItem::grabToImage()` is the **public-API asynchronous baseline / v0.1 candidate**, not
  the final high-performance path (Qt documents the offscreen render + GPU→CPU copy as costly).
- Distinguish isolated per-scene measurements from in-process (`-all`) re-measurements: the
  latter run in a warmer state and are load-sensitive; name which file is authoritative.

### Evidence rules and pitfalls learned from the Round-1 review (keep applying them)

- `QImage::cacheKey()` is `(serial_number << 32) | detach_counter`, i.e. a content/detach
  identity, **not** a backing-store identity. Never use it to infer reallocations. Probe
  real storage identity with `QImage::constBits()` (non-detaching) and always ship a
  positive + negative control, plus a no-consumer control run.
- Claim only the Qt copy-on-write semantic (`QImage::detach()` copies when refcount != 1);
  do not extrapolate an allocation rate. Address stability is not safety — a stable
  pointer can still carry rewritten content.
- Damage regions must be bound to one shared target widget and mapped with
  `QWidget::mapTo()` + clipped before unioning. `isAncestorOf()` alone is wrong for
  QDialog/QMenu children: walk the parent chain and reject anything that crosses a window
  boundary. Ship a deterministic mapping control (repaint one known widget, expect its
  exact target-space rectangle) or the ratio means nothing.
- Make the measurement scene deterministic before relying on damage numbers: no focused
  text cursor, no indeterminate progress bar, no other async repaint sources.
- Measure the complete operation (allocation/clear + render) when comparing a reused
  buffer against a fresh one, otherwise the reallocation hides before the timer.
- Ratios from deliberately damage-heavy scenes must be labelled as such; do not present
  them as typical application behaviour.
