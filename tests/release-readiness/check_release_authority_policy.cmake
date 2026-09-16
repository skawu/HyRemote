cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

set(workflow_path "${HYREMOTE_SOURCE_DIR}/.github/workflows/git-flow-policy.yml")
set(checklist_path "${HYREMOTE_SOURCE_DIR}/docs/release-candidate-checklist.md")
if(NOT EXISTS "${workflow_path}" OR NOT EXISTS "${checklist_path}")
    message(FATAL_ERROR "release-authority-policy: workflow/checklist is missing")
endif()

file(READ "${workflow_path}" policy)
foreach(required_token
        [=[issues: read]=]
        [=[Require accepted milestone authorities before release PR]=]
        [=[required_issues=(30)]=]
        [=[required_issues=(30 31)]=]
        [=[required_issues=(30 31 32)]=]
        [=[required_issues=(30 31 32 39 41 101 104 107 109 33)]=]
        [=[state}" != "closed"]=]
        [=[reason}" == "not_planned"]=]
        [=[release tag ${GITHUB_REF_NAME} must be an annotated tag]=]
        [=[must point to the exact current main release head]=]
        [=[backmerge requires the already-published annotated release tag]=])
    # The workflow is YAML/bash text. CMake's bracket arguments above preserve characters literally;
    # normalize the two shell-quote probes here rather than relying on CMake escape processing.
    string(REPLACE [=[\"]=] [=["]=] normalized_token "${required_token}")
    string(FIND "${policy}" "${normalized_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "release-authority-policy: Git Flow lost required authority invariant: ${normalized_token}")
    endif()
endforeach()

# The workflow may read authority state, but it must never mutate/close issues as part of release
# authorization. Acceptance remains a deliberate governance action backed by evidence.
foreach(forbidden_token
        [=[gh issue close]=]
        [=[-X PATCH]=]
        [=[--method PATCH]=])
    string(FIND "${policy}" "${forbidden_token}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "release-authority-policy: workflow must not manufacture issue acceptance: ${forbidden_token}")
    endif()
endforeach()

file(READ "${checklist_path}" checklist)
foreach(required_phrase
        [=[closed as completed]=]
        [=[`not_planned` is not release acceptance]=]
        [=[#30/#31/#32/#39/#41, #101/#104/#107/#109 and final GA authority #33]=]
        [=[Editing release notes or toggling PR Draft state cannot substitute for authority closure]=]
        [=[release branch is therefore a versioned verification/finalization line]=])
    string(FIND "${checklist}" "${required_phrase}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "release-authority-policy: canonical release checklist drifted: ${required_phrase}")
    endif()
endforeach()

message(STATUS
    "HyRemote release-authority policy gate: PASS "
    "(issue-backed acceptance precedes release branch; workflow remains read-only; tag/backmerge facts preserved)")
