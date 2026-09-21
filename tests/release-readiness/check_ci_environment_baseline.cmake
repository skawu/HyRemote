cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

# Consolidated topology: the CI model this branch converged on plus branch hygiene, which develop merged
# separately. The retired per-lane workflows must not come back through a merge or a replay - the capability
# dimension is selected by the classifier, not by one workflow per source directory.
set(required_workflows
    ".github/workflows/ci.yml"
    ".github/workflows/git-flow-policy.yml"
    ".github/workflows/mainline-validation.yml"
    ".github/workflows/branch-hygiene.yml")

set(retired_workflows
    ".github/workflows/remoteaccess-facade.yml"
    ".github/workflows/widgets-adapter.yml"
    ".github/workflows/quick-adapter.yml"
    ".github/workflows/qml-api.yml"
    ".github/workflows/qpa-proxy.yml"
    ".github/workflows/rfb-transport.yml"
    ".github/workflows/sdk-consumption.yml"
    ".github/workflows/v1-ga-acceptance.yml")

# The reference Linux host baseline is one shared script, and the workflows that need it must call it and retrigger
# when it changes. Workflows that never touch a Linux Qt desktop host are not required to call it.
set(shared_script ".github/scripts/install-linux-qt-desktop-deps.sh")
set(shared_script_path "${HYREMOTE_SOURCE_DIR}/${shared_script}")
if(NOT EXISTS "${shared_script_path}")
    message(FATAL_ERROR "ci-baseline: missing shared Linux Qt desktop dependency script")
endif()

file(READ "${shared_script_path}" deps_script)
foreach(required_token
        "set -euo pipefail"
        "xvfb"
        "libxcb-cursor0"
        "libxkbcommon-x11-0"
        "libxkbcommon-dev"
        "libxkbcommon-x11-dev"
        "libgl1-mesa-dev")
    string(FIND "${deps_script}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "ci-baseline: shared dependency script missing: ${required_token}")
    endif()
endforeach()

foreach(workflow IN LISTS required_workflows)
    set(path "${HYREMOTE_SOURCE_DIR}/${workflow}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "ci-baseline: missing required workflow: ${workflow}")
    endif()
    file(READ "${path}" workflow_text)

    # Concurrency cancels superseded runs, but nothing stops a hung job from holding a runner for the platform's
    # default six hours, so every job must state its own bound. The repository-layout gate claims
    # "bounded/cancellable workflow fan-out"; this is the half of that claim which does not enforce itself.
    string(REGEX MATCHALL "runs-on:" _runs_on_declarations "${workflow_text}")
    string(REGEX MATCHALL "timeout-minutes:" _timeout_declarations "${workflow_text}")
    list(LENGTH _runs_on_declarations _job_count)
    list(LENGTH _timeout_declarations _timeout_count)
    if(_timeout_count LESS _job_count)
        message(FATAL_ERROR
            "ci-baseline: ${workflow} declares ${_job_count} job(s) but only ${_timeout_count} timer(s); every job "
            "must bound its own runtime with timeout-minutes")
    endif()
endforeach()

foreach(workflow IN LISTS retired_workflows)
    if(EXISTS "${HYREMOTE_SOURCE_DIR}/${workflow}")
        message(FATAL_ERROR "ci-baseline: retired per-lane workflow returned: ${workflow}")
    endif()
endforeach()

foreach(workflow IN ITEMS
        ".github/workflows/ci.yml"
        ".github/workflows/mainline-validation.yml")
    file(READ "${HYREMOTE_SOURCE_DIR}/${workflow}" workflow_text)
    string(FIND "${workflow_text}" "bash ${shared_script}" shared_call)
    if(shared_call EQUAL -1)
        message(FATAL_ERROR
            "ci-baseline: ${workflow} drifted from the shared Linux Qt desktop host baseline")
    endif()
endforeach()

# Two ways to retrigger when the shared baseline changes: no path filter at all (the workflow always runs), or a
# explicit path list that names the script. Requiring the path literal unconditionally was wrong for the
# consolidated lane, which deliberately has no filter.
foreach(workflow IN ITEMS
        ".github/workflows/ci.yml"
        ".github/workflows/mainline-validation.yml")
    file(READ "${HYREMOTE_SOURCE_DIR}/${workflow}" workflow_text)
    string(FIND "${workflow_text}" "paths:" declares_paths)
    if(NOT declares_paths EQUAL -1)
        string(FIND "${workflow_text}" "'${shared_script}'" trigger_path)
        if(trigger_path EQUAL -1)
            message(FATAL_ERROR
                "ci-baseline: ${workflow} filters paths but does not retrigger when the shared host dependency "
                "baseline changes")
        endif()
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/.github/workflows/ci.yml" ci)

function(require_token text token description)
    string(FIND "${text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "ci-baseline: missing ${description}: ${token}")
    endif()
endfunction()

function(forbid_token text token description)
    string(FIND "${text}" "${token}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "ci-baseline: ${description}: ${token}")
    endif()
endfunction()

# The consolidated PR lane keeps the capability contract: the classifier resolves the affected frontends, the lane
# builds exactly that union through the official entry point, and the Qt SDK contract is per capability rather than
# one fixed requirement list.
foreach(required_token IN ITEMS
        "cancel-in-progress: true"
        "Resolve affected capabilities"
        [=[integrations: ${{ steps.scope.outputs.integrations }}]=]
        "resolve-ci-scope.py"
        "--self-test"
        "readiness_evidence"
        "hyremote-qt-v4-"
        "qt_tree_valid"
        [=[--need-qml "$NEED_QML"]=]
        [=[--need-qpa "$NEED_QPA"]=]
        "install-linux-qt-desktop-deps.sh qpa"
        "install-linux-qt-desktop-deps.sh public")
    require_token("${ci}" "${required_token}" "consolidated PR CI contract")
endforeach()

# The capability prefixes live in the classifier script the workflow calls rather than inline in the workflow, so the
# lane a change selects and the tokens this gate checks cannot drift apart. Reading the script follows the same
# pattern already used for the Qt SDK validator below.
file(READ "${HYREMOTE_SOURCE_DIR}/.github/scripts/resolve-ci-scope.py" ci_scope_classifier)
foreach(required_token IN ITEMS
        "src/core/"
        "src/runtime/"
        "src/integrations/cpp/"
        "src/integrations/qml/"
        "src/integrations/generic/"
        "src/integrations/qpa/"
        "READINESS_PREFIXES"
        "hyremote-release-readiness-")
    require_token("${ci_scope_classifier}" "${required_token}" "classifier capability contract")
endforeach()

# The Qt components every product change needs, and the ones that stay capability-conditional.
foreach(required_token IN ITEMS
        "Qt6CoreConfig.cmake"
        "Qt6GuiConfig.cmake"
        "Qt6WidgetsConfig.cmake"
        "Qt6QuickConfig.cmake"
        "qguiapplication_p.h")
    require_token("${ci}" "${required_token}" "baseline Qt capability contract")
endforeach()

forbid_token("${ci}" "Qt6QmlConfig.cmake"
             "QML artifacts must stay capability-conditional, not unconditionally required")
forbid_token("${ci}" "--integrations=cpp,qml,generic,qpa"
             "PR CI must not hard-code all four frontends for every product change")

file(READ "${HYREMOTE_SOURCE_DIR}/.github/scripts/validate-qt-sdk.py" qt_validator)
foreach(required_token IN ITEMS
        "Qt6QmlConfig.cmake"
        "qmldir"
        "qwindows"
        "libqxcb")
    require_token("${qt_validator}" "${required_token}" "Qt capability check in the SDK validator")
endforeach()

# Windows clean-deployment runtime isolation is produced by the release evidence runner now: its installed and source
# product-fit cells construct an explicit runtime search path and launch the deployed consumer from the deployment
# tree, which is where the retired per-lane workflows used to assert that inline.
set(evidence_runner "${HYREMOTE_SOURCE_DIR}/tests/release-readiness/run_release_evidence.cmake")
if(NOT EXISTS "${evidence_runner}")
    message(FATAL_ERROR "ci-baseline: release evidence runner is missing")
endif()
file(READ "${evidence_runner}" evidence_runner_text)
foreach(required_token IN ITEMS
        "installed-qpa-product-fit"
        "source-qpa-product-fit"
        "installed-qml-qpa"
        "HYREMOTE_CONSUMER_SOURCE_DIR")
    require_token("${evidence_runner_text}" "${required_token}"
                  "executable deployed-consumer evidence cell")
endforeach()

message(STATUS
    "HyRemote CI environment baseline gate: PASS "
    "(consolidated capability-scoped workflows + bounded jobs + shared Linux Qt host baseline + capability-scoped "
    "Qt SDK contract + executable deployed-consumer evidence cells)")
