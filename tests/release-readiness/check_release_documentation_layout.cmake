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
# with docs/repository-layout.md instead of allowing the old root-level module names to become facts
# again. Conceptual prose such as "Core" or "architecture research" is intentionally not forbidden.
require_doc_token("docs/release-package-manifest.md" "`src/core/` low-level implementation headers and tests"
                  "canonical Core source path")
require_doc_token("docs/release-package-manifest.md" "architecture research/evidence under `research/`"
                  "canonical research/evidence path")
forbid_doc_token("docs/release-package-manifest.md" "`core/` low-level implementation headers and tests"
                 "legacy root Core path")
forbid_doc_token("docs/release-package-manifest.md" "architecture spikes under `spikes/`"
                 "legacy root spikes path")

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
require_doc_token("README.md" "docs/repository-layout.md" "repository-layout documentation link")
require_doc_token("CONTRIBUTING.md" "docs/repository-layout.md" "contributor layout authority")
require_doc_token("CONTRIBUTING.md" "docs/branch-lifecycle.md" "contributor branch-lifecycle authority")

# #109 cannot be reduced to a chat-only checklist. Keep a versioned runbook with the exact V1
# lifecycle boundaries ready before physical execution begins. This gate proves preparation only;
# it deliberately also pins the statement that the document itself is NOT acceptance evidence.
set(physical_doc "docs/v1-physical-acceptance.md")
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
    "(package/layout/dependency governance + versioned #109 physical acceptance preparation; no physical PASS implied)")
