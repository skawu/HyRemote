cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

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
        "libgl1-mesa-dev")
    string(FIND "${deps_script}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "ci-baseline: shared dependency script missing: ${required_token}")
    endif()
endforeach()

set(required_workflows
    ".github/workflows/widgets-adapter.yml"
    ".github/workflows/quick-adapter.yml"
    ".github/workflows/qml-api.yml"
    ".github/workflows/qpa-proxy.yml"
    ".github/workflows/sdk-consumption.yml"
    ".github/workflows/v1-ga-acceptance.yml")

foreach(workflow IN LISTS required_workflows)
    set(path "${HYREMOTE_SOURCE_DIR}/${workflow}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "ci-baseline: missing required workflow: ${workflow}")
    endif()
    file(READ "${path}" workflow_text)
    string(FIND "${workflow_text}" "bash ${shared_script}" shared_call)
    if(shared_call EQUAL -1)
        message(FATAL_ERROR
            "ci-baseline: ${workflow} drifted from the shared Linux Qt desktop host baseline")
    endif()
    string(FIND "${workflow_text}" "'${shared_script}'" trigger_path)
    if(trigger_path EQUAL -1)
        message(FATAL_ERROR
            "ci-baseline: ${workflow} does not retrigger when the shared host dependency baseline changes")
    endif()
endforeach()

message(STATUS
    "HyRemote CI environment baseline gate: PASS "
    "(Widgets/Quick/QML/QPA/SDK/V1-GA share one Linux Qt desktop host dependency authority)")
