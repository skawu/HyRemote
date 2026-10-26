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
require_doc_token("docs/internal/repository-layout.md" "`src/core` is the shared Core implementation"
                  "canonical Core source path")
require_doc_token("docs/internal/repository-layout.md" "`src/runtime` owns implementation shared by multiple/all integration frontends"
                  "canonical Runtime source path")
require_doc_token("docs/release-package-manifest.md" "一个共享 Runtime"
                  "single shipped Runtime")
require_doc_token("docs/release-package-manifest.md" "Core 是内部组合"
                  "Core is internal composition, not an application SDK target")
forbid_doc_token("docs/release-package-manifest.md" "`core/` low-level implementation headers and tests"
                 "legacy root Core path")
forbid_doc_token("docs/release-package-manifest.md" "architecture spikes under `spikes/`"
                 "legacy root spikes path")

# Historical engineering documents must still point at paths that exist after the canonical repository
# layout migration. Keep this list intentionally narrow: docs/internal/repository-layout.md itself is allowed to
# mention legacy build-directory names when explaining the migration, but reproduction/source links may
# not silently regress to removed root-level directories.
require_doc_token("docs/internal/capture-spike.md" "**Retired.**"
                  "retired capture harness status")
forbid_doc_token("docs/internal/capture-spike.md" "cmake -S research/capture"
                 "reproducer command for a removed capture harness")

require_doc_token("docs/internal/async-capture-spike.md" "**Retired.**"
                  "retired async-capture harness status")
forbid_doc_token("docs/internal/async-capture-spike.md" "cmake -S research/async-capture"
                 "reproducer command for a removed async-capture harness")

require_doc_token("docs/internal/qpa-capture-classification-qt-6.8.3.md"
                  "`src/runtime/src/widgets/widget_target.cpp`"
                  "canonical Widgets target source citation")
require_doc_token("docs/internal/qpa-capture-classification-qt-6.8.3.md"
                  "`src/runtime/src/quick/quick_target.cpp`"
                  "canonical Quick target source citation")
forbid_doc_token("docs/internal/qpa-capture-classification-qt-6.8.3.md"
                 "`src/cpp/src/"
                 "pre-#219 target adapter source citation")
forbid_doc_token("docs/internal/qpa-capture-classification-qt-6.8.3.md"
                 "`remoteaccess/src/"
                 "removed root RemoteAccess source citation")

require_doc_token("docs/internal/x86-vnc-transport-evaluation.md" "retrievable from git history at commit `3e6e191`"
                  "retired Rust spike recovery pointer")
forbid_doc_token("docs/internal/x86-vnc-transport-evaluation.md" "`spikes/vnc-transport-rust-ffi/`"
                 "removed historical Rust spike path")

# ARCH-01 remains useful historical design input, but it must not compete with the frozen V1 architecture.
# Pin the authority statement rather than rewriting proposal-era diagrams into fake present-day evidence.
require_doc_token("docs/internal/core-architecture.md" "Status: **ARCH-01 proposal (proposal-era design input)**"
                  "proposal-era Core architecture status")
require_doc_token("docs/internal/core-architecture.md" "Canonical authority: `docs/architecture.md` is the frozen V1 architecture"
                  "frozen architecture authority")
require_doc_token("docs/internal/core-architecture.md" "No experimental tree is kept in the repository"
                  "retired research-tree statement in Core architecture history")

require_doc_token("src/core/CMakeLists.txt" "src/core/tests/check_dependencies.cmake enforces the include/declaration/link part"
                  "canonical Core dependency-guard path and scope")

# Dependency-policy prose is release-facing architecture truth. Pin the current product statements - the frontends
# are not separate toolchains, development tooling does not become a runtime prerequisite, and dependencies are not
# fetched behind the consumer back - instead of the historical wording and version pins that used to carry them.
require_doc_token("docs/dependency-policy.md" "C++/QML/Generic/QPA 是接入前端，不是独立后端工具链"
                  "frontends are not separate backend toolchains")
forbid_doc_token("docs/dependency-policy.md" "Source under `spikes/`"
                 "legacy spikes path in dependency policy")
require_doc_token("docs/dependency-policy.md" "开发工具不会因为仓库使用它就成为 SDK/runtime 前提"
                  "development tooling is not an SDK/runtime prerequisite")
require_doc_token("docs/dependency-policy.md" "没有隐藏依赖下载"
                  "no hidden dependency downloads")
require_doc_token("docs/dependency-policy.md" "产品依赖不会通过隐藏的"
                  "product dependencies are not fetched implicitly")

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
        "#33/#343/#57/#95 Windows Qt5.15/Qt6.8"
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


# #594/#209: Today's Qt LTS WBS is a planning/execution ledger, not release activation.
# Keep the 22 accountable work packages and the four concrete audit fixes reachable from
# the current outcome roadmap. These checks prove document coverage only: NOT code/Viewer acceptance.
set(qt_lts_wbs_doc "docs/internal/qt-lts-architecture-wbs-2026-10-24.md")
set(qt_lts_roadmap_doc "docs/internal/product-version-delivery-roadmap-2026-10-23.md")
require_doc_token("${qt_lts_roadmap_doc}" "qt-lts-architecture-wbs-2026-10-24.md"
                  "product roadmap to 22-item Qt LTS execution ledger")
require_doc_token("${qt_lts_roadmap_doc}" "V0.3.3.0 已合法发布"
                  "current legally reissued v0.3.3.0 release status")
forbid_doc_token("${qt_lts_roadmap_doc}" "PUBLIC TAG RETRACTED"
                 "outdated assertion that v0.3.3.0 remains retracted")

set(qt_lts_expected_wbs
    A01 A02 A03 A04 A05
    B01 B02 B03
    C01 C02 C03 C04
    D01 D02 D03 D04 D05
    E01 E02
    F01 F02 F03)
foreach(qt_lts_wbs IN LISTS qt_lts_expected_wbs)
    require_doc_token("${qt_lts_wbs_doc}" "| **${qt_lts_wbs}** |"
                      "Qt LTS work package ${qt_lts_wbs} acceptance row")
endforeach()
file(STRINGS "${HYREMOTE_SOURCE_DIR}/${qt_lts_wbs_doc}"
     qt_lts_actual_wbs_rows REGEX "^\\| \\*\\*[A-F][0-9][0-9]\\*\\* \\|")
list(LENGTH qt_lts_actual_wbs_rows qt_lts_actual_wbs_count)
if(NOT qt_lts_actual_wbs_count EQUAL 22)
    message(FATAL_ERROR
        "release-documentation-layout: Qt LTS WBS must have exactly 22 accountable rows; "
        "found ${qt_lts_actual_wbs_count}")
endif()
foreach(audit_defect IN ITEMS 641 642 643 644)
    require_doc_token("${qt_lts_wbs_doc}" "[#${audit_defect}]"
                      "Qt LTS ledger ownership for the 2026-10-24 defect #${audit_defect}")
    require_doc_token("${qt_lts_roadmap_doc}" "[#${audit_defect}]"
                      "product roadmap defect routing for #${audit_defect}")
endforeach()

# 2026-10-26: local Owner has an actual next developer version, not an indefinite
# unnumbered Feature that could be silently deferred to the V0.4 qualification-only RC.
set(next_feature_charter "docs/internal/v0.3.4.0-windows-qt-lts-development-charter.md")
require_doc_token("${next_feature_charter}" "TARGET_VERSION=V0.3.4.0"
                  "exact local Owner V0.3.4.0 development handoff")
require_doc_token("${next_feature_charter}" "V0.4_RC_ACTIVE=false"
                  "RC remains inactive until Qt5/6.8 product breadth is ready")
require_doc_token("docs/internal/product-version-delivery-roadmap-2026-10-23.md"
                  "即时开发版本 = V0.3.4.0" "current product version delivery target")
require_doc_token("docs/versioning.md" "V0.3.4.0" "Chinese version assignment")
require_doc_token("docs/en/versioning.md" "V0.3.4.0" "English version assignment")

# #594 Owner 2026-10-26: a local developer must have the COMPLETE known
# outcome-to-exact-planning-version ladder, not just the next unnamed Feature.
# This validates planning traceability, NEVER machine activation or product PASS.
set(full_roadmap_doc "docs/internal/product-version-delivery-roadmap-2026-10-23.md")
require_doc_token("${full_roadmap_doc}" "## 零、全量编号版本路线"
                  "single current numbered product-outcome roadmap")
require_doc_token("${full_roadmap_doc}" "版本目标变更"
                  "reviewable old->new version change procedure")
require_doc_token("${full_roadmap_doc}" "V0.4.0.0 继续保持"
                  "RC remains inactive until pre-GA Feature implementation")
foreach(planned_version IN ITEMS
        V0.3.4.0 V0.3.5.0 V0.3.6.0 V0.3.7.0
        V0.4.0.0 V1.0.0.0
        V1.0.1.0 V1.0.2.0 V1.0.3.0 V1.0.4.0
        V1.0.5.0 V1.0.6.0 V1.0.7.0
        V1.1.0.0 V1.1.1.0
        V1.2.0.0 V1.2.1.0 V1.2.2.0 V1.2.3.0
        V1.2.4.0 V1.2.5.0 V1.2.6.0 V1.2.7.0)
    require_doc_token("${full_roadmap_doc}" "**${planned_version}**"
                      "full known product version planning: ${planned_version}")
    require_doc_token("docs/versioning.md" "${planned_version}"
                      "Chinese version ladder: ${planned_version}")
    require_doc_token("docs/en/versioning.md" "${planned_version}"
                      "English version ladder: ${planned_version}")
endforeach()
file(STRINGS "${HYREMOTE_SOURCE_DIR}/${full_roadmap_doc}"
     planned_v03_feature_rows REGEX "^\\| \\*\\*V0\\.3\\.[4-7]\\.0\\*\\* \\|")
list(LENGTH planned_v03_feature_rows planned_v03_feature_count)
if(NOT planned_v03_feature_count EQUAL 4)
    message(FATAL_ERROR
        "release-documentation-layout: expected exactly four distinct "
        "V0.3.4.0–V0.3.7.0 product Feature rows, found ${planned_v03_feature_count}")
endif()
foreach(planned_owner IN ITEMS 265 566 603 591 589 581 582 583 584 585 586 590
                               592 605 587 588 593 204 171 190 170
                               149 203 7 236 18 154 148 346 172 225 173 246 151 180)
    require_doc_token("${full_roadmap_doc}" "#${planned_owner}"
                      "known product deliverable Issue #${planned_owner} is absent from roadmap")
endforeach()

# Product-first future work must be rooted in real users and distinguish current
# field defects from conditional post-GA product capabilities.
require_doc_token("${full_roadmap_doc}" "## 产品北极星"
                  "product-centered user outcomes before release numbers")
foreach(field_issue IN ITEMS 565 567 362)
    require_doc_token("${full_roadmap_doc}" "#${field_issue}"
                      "known current field-user problem #${field_issue}")
endforeach()
require_doc_token("${full_roadmap_doc}" "GO/NO-GO"
                  "uncertain features must have explicit discovery decisions")
require_doc_token("${full_roadmap_doc}" "V1.2.7.0"
                  "planning continues beyond V1.0, V1.1 and early post-GA horizons")

message(STATUS
    "HyRemote release documentation layout gate: PASS "
    "(canonical source layout + architecture authority + package/dependency governance + safe repository-admin recovery + versioned #109 physical preparation; neither admin readiness nor runbooks imply product PASS)")