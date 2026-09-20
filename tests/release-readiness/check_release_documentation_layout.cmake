cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

function(require_doc_token relative_path token description)
    set(path "${HYREMOTE_SOURCE_DIR}/${relative_path}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "release-documentation-layout: missing ${relative_path}")
    endif()
    file(READ "${path}" text)
    string(FIND "${text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "release-documentation-layout: ${description} missing from ${relative_path}: ${token}")
    endif()
endfunction()

function(forbid_doc_token relative_path token description)
    set(path "${HYREMOTE_SOURCE_DIR}/${relative_path}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "release-documentation-layout: missing ${relative_path}")
    endif()
    file(READ "${path}" text)
    string(FIND "${text}" "${token}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "release-documentation-layout: ${description} remains in ${relative_path}: ${token}")
    endif()
endfunction()

# The package manifest is the release-facing source-tree inventory. Keep its physical paths aligned
# with docs/internal/repository-layout.md instead of allowing the old root-level module names to become facts
# again. Conceptual prose such as "Core" or "architecture research" is intentionally not forbidden.
require_doc_token("docs/release-package-manifest.md" "`src/core/` low-level implementation headers and tests"
                  "canonical Core source path")
require_doc_token("docs/release-package-manifest.md" "architecture research/evidence under `research/`"
                  "canonical research/evidence path")
forbid_doc_token("docs/release-package-manifest.md" "`core/` low-level implementation headers and tests"
                 "legacy root Core path")
forbid_doc_token("docs/release-package-manifest.md" "architecture spikes under `spikes/`"
                 "legacy root spikes path")

# Historical engineering documents must still point at paths that exist after the canonical repository
# layout migration. Keep this list intentionally narrow: docs/internal/repository-layout.md itself is allowed to
# mention legacy build-directory names when explaining the migration, but reproduction/source links may
# not silently regress to removed root-level directories.
require_doc_token("docs/internal/capture-spike.md" "[`research/capture/`](../research/capture/)"
                  "canonical capture research source path")
require_doc_token("docs/internal/capture-spike.md" "cmake -S research/capture -B build/spike-capture"
                  "canonical capture reproducer path")
forbid_doc_token("docs/internal/capture-spike.md" "../spikes/capture/"
                 "removed capture spike source path")

require_doc_token("docs/internal/async-capture-spike.md" "[`research/async-capture/`](../research/async-capture/)"
                  "canonical async-capture research source path")
require_doc_token("docs/internal/async-capture-spike.md" "cmake -S research/async-capture -B build/async-spike"
                  "canonical async-capture reproducer path")
forbid_doc_token("docs/internal/async-capture-spike.md" "../spikes/async-capture/"
                 "removed async-capture spike source path")

require_doc_token("docs/internal/qpa-capture-classification-qt-6.8.3.md"
                  "`src/remoteaccess/src/widgets/widget_target.cpp`"
                  "canonical Widgets target source citation")
require_doc_token("docs/internal/qpa-capture-classification-qt-6.8.3.md"
                  "`src/remoteaccess/src/quick/quick_target.cpp`"
                  "canonical Quick target source citation")
forbid_doc_token("docs/internal/qpa-capture-classification-qt-6.8.3.md"
                 "`remoteaccess/src/"
                 "removed root RemoteAccess source citation")

require_doc_token("docs/internal/x86-vnc-transport-evaluation.md" "`research/vnc-transport-rust-ffi/`"
                  "canonical historical Rust research path")
forbid_doc_token("docs/internal/x86-vnc-transport-evaluation.md" "`spikes/vnc-transport-rust-ffi/`"
                 "removed historical Rust spike path")

# ARCH-01 remains useful historical design input, but it must not compete with the frozen V1 architecture.
# Pin the authority statement rather than rewriting proposal-era diagrams into fake present-day evidence.
require_doc_token("docs/internal/core-architecture.md" "Status: **ARCH-01 proposal (proposal-era design input)**"
                  "proposal-era Core architecture status")
require_doc_token("docs/internal/core-architecture.md" "Canonical authority: `docs/architecture.md` is the frozen V1 architecture"
                  "frozen architecture authority")
require_doc_token("docs/internal/core-architecture.md" "`research/` trees stay non-production"
                  "canonical research path in Core architecture history")

require_doc_token("src/core/CMakeLists.txt" "src/core/tests/check_dependencies.cmake enforces the include/declaration/link part"
                  "canonical Core dependency-guard path and scope")

# Dependency-policy prose is also release-facing architecture truth. Keep historical experiments under
# the canonical research/ tree and keep CI-only tooling clearly separated from shipped runtime payloads.
require_doc_token("docs/dependency-policy.md" "Source under `research/` may remain as historical research"
                  "canonical research path in dependency policy")
forbid_doc_token("docs/dependency-policy.md" "Source under `spikes/`"
                 "legacy spikes path in dependency policy")
require_doc_token("docs/dependency-policy.md" "### Repository test/CI-only tools"
                  "CI-only tooling boundary")
require_doc_token("docs/dependency-policy.md" "`vncdotool==1.3.0`"
                  "pinned maintained VNC test client")
require_doc_token("docs/dependency-policy.md" "These tools are pinned/used by repository automation and acceptance harnesses"
                  "CI tools are not runtime payloads")

# User-facing entry points must identify the canonical layout document so repository contributors do
# not infer module ownership from historical root names.
require_doc_token("README.md" "docs/internal/repository-layout.md" "repository-layout documentation link")
require_doc_token("CONTRIBUTING.md" "docs/internal/repository-layout.md" "contributor layout authority")
require_doc_token("CONTRIBUTING.md" "docs/internal/branch-lifecycle.md" "contributor branch-lifecycle authority")

# Repository administration is a release-preparation gate, not product acceptance. Keep the exact
# recovery/cleanup entry points versioned so workflow state, security reporting and branch hygiene do
# not become chat-only release knowledge.
set(admin_doc "docs/internal/v1-repository-admin.md")
foreach(required_token
        "repository preparation / administration gate; not product acceptance evidence"
        "pwsh .github/scripts/drain-superseded-v1-runs.ps1"
        "pwsh .github/scripts/drain-superseded-v1-runs.ps1 -Execute"
        "pwsh .github/scripts/restore-v1-workflows.ps1"
        "pwsh .github/scripts/finalize-v1-repository-settings.ps1"
        "pwsh .github/scripts/prune-stale-branches.ps1 -Execute"
        "Current-HEAD runs are always retained"
        "delete_branch_on_merge=true"
        "private vulnerability reporting"
        "Do not describe the current workflow/audit layer as equivalent to branch protection."
        "#104 actual Windows/Linux acceptance complete"
        "#109 physical/native coexistence evidence complete"
        "The first seven items are repository governance readiness. The last two are product acceptance.")
    require_doc_token("${admin_doc}" "${required_token}" "V1 repository administration preparation contract")
endforeach()
foreach(admin_script IN ITEMS
        ".github/scripts/drain-superseded-v1-runs.ps1"
        ".github/scripts/restore-v1-workflows.ps1"
        ".github/scripts/finalize-v1-repository-settings.ps1"
        ".github/scripts/prune-stale-branches.ps1")
    if(NOT EXISTS "${HYREMOTE_SOURCE_DIR}/${admin_script}")
        message(FATAL_ERROR "release-documentation-layout: missing repository administration helper: ${admin_script}")
    endif()
endforeach()
require_doc_token(".github/scripts/drain-superseded-v1-runs.ps1"
                  "if ($headBeforeCancel -ne $head)"
                  "stale-run drain must abort if the candidate branch moved")
require_doc_token(".github/scripts/drain-superseded-v1-runs.ps1"
                  "if ($item.Sha -eq $head)"
                  "stale-run drain must retain current-HEAD runs")

# #109 cannot be reduced to a chat-only checklist. Keep a versioned runbook with the exact V1
# lifecycle boundaries ready before physical execution begins. This gate proves preparation only;
# it deliberately also pins the statement that the document itself is NOT acceptance evidence.
set(physical_doc "docs/internal/v1-physical-acceptance.md")
foreach(required_token
        "repository preparation only — this document is not physical acceptance evidence"
        "Candidate commit SHA: `<required>`"
        "Windows x86_64"
        "Linux x86_64"
        "## E1 — Embedded C++ / Widgets"
        "## E2 — Embedded C++ / Qt Quick"
        "## E3 — Declarative QML"
        "## E4 — Transparent QPA Proxy / existing Qt-only application"
        "stop -> configure -> start"
        "hyremote-input=true"
        "Abrupt-disconnect case"
        "Explicit-stop case"
        "no previously queued remote key/button is delivered late"
        "bounded slow-viewer"
        "#109 decision: `<OPEN until evidence reviewed; PASS only after review>`"
        "Do not mark #109 or V1.0.0.0 accepted merely because this runbook exists")
    require_doc_token("${physical_doc}" "${required_token}" "#109 physical/native preparation contract")
endforeach()

message(STATUS
    "HyRemote release documentation layout gate: PASS "
    "(canonical source/research paths + architecture authority + package/dependency governance + safe repository-admin recovery + versioned #109 physical preparation; neither admin readiness nor runbooks imply product PASS)")
