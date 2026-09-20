cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

set(workflow_path "${HYREMOTE_SOURCE_DIR}/.github/workflows/git-flow-policy.yml")
set(checklist_path "${HYREMOTE_SOURCE_DIR}/docs/internal/release-candidate-checklist.md")
set(authority_path "${HYREMOTE_SOURCE_DIR}/.github/release/v1-mandatory-issues.json")

foreach(required_file "${workflow_path}" "${checklist_path}" "${authority_path}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "release-authority-policy: required file is missing: ${required_file}")
    endif()
endforeach()

file(READ "${authority_path}" authority_json)
string(JSON authority_schema GET "${authority_json}" schema)
string(JSON authority_version GET "${authority_json}" version)
string(JSON scope_authority GET "${authority_json}" scope_authority)
string(JSON freeze_authority GET "${authority_json}" candidate_freeze_authority)

if(NOT authority_schema STREQUAL "1")
    message(FATAL_ERROR "release-authority-policy: unsupported V1 authority schema ${authority_schema}")
endif()
if(NOT authority_version STREQUAL "1.0.0.0")
    message(FATAL_ERROR "release-authority-policy: V1 authority version drifted: ${authority_version}")
endif()
if(NOT scope_authority STREQUAL "157" OR NOT freeze_authority STREQUAL "165")
    message(FATAL_ERROR
        "release-authority-policy: scope/freeze authorities must remain #157/#165; "
        "found #${scope_authority}/#${freeze_authority}")
endif()

set(expected_v1_issues
    9 30 31 32 33 39 41 57 101 104 107 109 143 144 157 158 159 162 163 164 165 170 174 175 176)
string(JSON authority_count LENGTH "${authority_json}" required_issue_numbers)
math(EXPR authority_last "${authority_count} - 1")
set(actual_v1_issues)
foreach(index RANGE 0 ${authority_last})
    string(JSON issue GET "${authority_json}" required_issue_numbers ${index})
    list(APPEND actual_v1_issues "${issue}")
endforeach()

if(NOT "${actual_v1_issues}" STREQUAL "${expected_v1_issues}")
    message(FATAL_ERROR
        "release-authority-policy: V1 mandatory issue manifest drifted. "
        "expected='${expected_v1_issues}' actual='${actual_v1_issues}'")
endif()

set(expected_embedded_deferred 7 10 17 18)

set(expected_classified_referenced 14 74 90 91 95 106 134 147 156 166)
string(JSON classified_count LENGTH "${authority_json}" classified_referenced_issue_numbers)
math(EXPR classified_last "${classified_count} - 1")
set(actual_classified_referenced)
foreach(index RANGE 0 ${classified_last})
    string(JSON issue GET "${authority_json}" classified_referenced_issue_numbers ${index})
    list(APPEND actual_classified_referenced "${issue}")
endforeach()
if(NOT "${actual_classified_referenced}" STREQUAL "${expected_classified_referenced}")
    message(FATAL_ERROR
        "release-authority-policy: classified-reference list drifted. "
        "expected='${expected_classified_referenced}' actual='${actual_classified_referenced}'")
endif()

# Anti-leak: every issue number mentioned by an in-tree release authority must be classified, so a
# future V1-labelled blocker cannot silently exist outside the mandatory manifest.
set(declared_authority_documents
    docs/internal/release-candidate-checklist.md
    docs/internal/v1-ga-acceptance.md
    docs/internal/development-roadmap.md
    docs/known-limitations.md
    docs/compatibility.md
    docs/internal/v1-physical-acceptance.md
    docs/security-model.md)
set(known_issue_numbers ${expected_v1_issues} ${expected_embedded_deferred} ${expected_classified_referenced})
foreach(document IN LISTS declared_authority_documents)
    set(document_path "${HYREMOTE_SOURCE_DIR}/${document}")
    if(NOT EXISTS "${document_path}")
        message(FATAL_ERROR "release-authority-policy: declared authority document is missing: ${document}")
    endif()
    file(READ "${document_path}" document_text)
    string(REGEX MATCHALL "#[0-9]+" mentioned "${document_text}")
    foreach(token IN LISTS mentioned)
        string(REGEX REPLACE "#" "" number "${token}")
        if(NOT number IN_LIST known_issue_numbers)
            message(FATAL_ERROR
                "release-authority-policy: unclassified issue reference #${number} in ${document}; "
                "either add it to the mandatory set or classify it in "
                ".github/release/v1-mandatory-issues.json")
        endif()
    endforeach()
endforeach()

string(JSON deferred_count LENGTH "${authority_json}" embedded_deferred_issue_numbers)
math(EXPR deferred_last "${deferred_count} - 1")
set(actual_embedded_deferred)
foreach(index RANGE 0 ${deferred_last})
    string(JSON issue GET "${authority_json}" embedded_deferred_issue_numbers ${index})
    list(APPEND actual_embedded_deferred "${issue}")
endforeach()
if(NOT "${actual_embedded_deferred}" STREQUAL "${expected_embedded_deferred}")
    message(FATAL_ERROR
        "release-authority-policy: embedded-only defer list drifted. "
        "expected='${expected_embedded_deferred}' actual='${actual_embedded_deferred}'")
endif()

file(READ "${workflow_path}" policy)
foreach(required_token
        [=[issues: read]=]
        [=[pull-requests: read]=]
        [=[mainline-push-audit]=]
        [=[commits/${GITHUB_SHA}/pulls]=]
        [=[select(.merged_at != null and .base.ref ==]=]
        [=[after-the-fact audit only]=]
        [=[cannot undo a direct push]=]
        [=[must never be described as branch protection]=]
        [=[Require accepted milestone authorities before release PR]=]
        [=[required_issues=(30)]=]
        [=[required_issues=(30 31)]=]
        [=[required_issues=(30 31 32)]=]
        [=[.github/release/v1-mandatory-issues.json]=]
        [=[.required_issue_numbers[]]=]
        [=[state}" != "closed"]=]
        [=[reason}" == "not_planned"]=]
        [=[## Security boundary]=]
        [=[must be annotated]=]
        [=[exact current main HEAD]=]
        [=[backmerge requires annotated]=])
    string(FIND "${policy}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "release-authority-policy: Git Flow lost required authority invariant: ${required_token}")
    endif()
endforeach()

# Release authorization may read issue state but must never manufacture acceptance or rewrite history.
foreach(forbidden_token
        [=[gh issue close]=]
        [=[-X PATCH]=]
        [=[--method PATCH]=]
        [=[git reset --hard]=]
        [=[git push --force]=])
    string(FIND "${policy}" "${forbidden_token}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "release-authority-policy: workflow must not manufacture acceptance/rollback: ${forbidden_token}")
    endif()
endforeach()

file(READ "${checklist_path}" checklist)
foreach(required_phrase
        [=[closed as completed]=]
        [=[`not_planned` is not release acceptance]=]
        [=[`.github/release/v1-mandatory-issues.json`]=]
        [=[CONVERGING]=]
        [=[RC-FROZEN]=]
        [=[PHYSICAL-ACCEPTANCE]=]
        [=[Editing release notes or toggling PR Draft state cannot substitute for authority closure]=]
        [=[release branch is therefore a versioned verification/finalization line]=]
        [=[after-the-fact direct-push audit]=]
        [=[cannot undo a push]=]
        [=[must not be described as branch protection]=]
        [=[The canonical repository execution/evidence template for #109 is `docs/internal/v1-physical-acceptance.md`]=]
        [=[same RC-FROZEN SHA]=]
        [=[## Security boundary]=])
    string(FIND "${checklist}" "${required_phrase}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "release-authority-policy: canonical release checklist drifted: ${required_phrase}")
    endif()
endforeach()

message(STATUS
    "HyRemote release-authority policy gate: PASS "
    "(V1 manifest=${actual_v1_issues}; scope=#157; freeze=#165; "
    "issue-backed acceptance precedes release; physical evidence is RC-FROZEN-SHA bound; "
    "mainline push audit remains detection-only; tag/backmerge facts preserved)")
