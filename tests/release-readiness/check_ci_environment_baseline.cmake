cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

function(read_required relative_path output_var)
    set(path "${HYREMOTE_SOURCE_DIR}/${relative_path}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "ci-baseline: missing required file: ${relative_path}")
    endif()
    file(READ "${path}" text)
    set(${output_var} "${text}" PARENT_SCOPE)
endfunction()

function(require_token text token description)
    string(FIND "${text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "ci-baseline: ${description} missing: ${token}")
    endif()
endfunction()

function(forbid_token text token description)
    string(FIND "${text}" "${token}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "ci-baseline: ${description}: ${token}")
    endif()
endfunction()

# V1 foundation uses a small workflow set rather than one workflow per source directory/feature.
foreach(required_workflow IN ITEMS
        ".github/workflows/git-flow-policy.yml"
        ".github/workflows/ci.yml"
        ".github/workflows/mainline-validation.yml")
    if(NOT EXISTS "${HYREMOTE_SOURCE_DIR}/${required_workflow}")
        message(FATAL_ERROR "ci-baseline: missing consolidated workflow: ${required_workflow}")
    endif()
endforeach()

foreach(retired_workflow IN ITEMS
        "remoteaccess-facade.yml"
        "widgets-adapter.yml"
        "quick-adapter.yml"
        "qml-api.yml"
        "qpa-proxy.yml"
        "rfb-transport.yml"
        "sdk-consumption.yml"
        "v1-ga-acceptance.yml")
    if(EXISTS "${HYREMOTE_SOURCE_DIR}/.github/workflows/${retired_workflow}")
        message(FATAL_ERROR "ci-baseline: retired per-slice workflow returned: ${retired_workflow}")
    endif()
endforeach()

read_required(".github/workflows/ci.yml" ci)
foreach(token IN ITEMS
        "cancel-in-progress: true"
        "Resolve affected capabilities"
        [=[integrations: ${{ steps.scope.outputs.integrations }}]=]
        "src/core/"
        "src/runtime/"
        "src/integrations/cpp/"
        "src/integrations/qml/"
        "src/integrations/generic/"
        "src/integrations/qpa/"
        "--integrations=\"$INTEGRATIONS\""
        "hyremote-qt-v4-"
        "qt_tree_valid"
        "Qt6CoreConfig.cmake"
        "Qt6GuiConfig.cmake"
        "Qt6WidgetsConfig.cmake"
        "Qt6QuickConfig.cmake"
        "Qt6QmlConfig.cmake"
        "qguiapplication_p.h"
        "install-linux-qt-desktop-deps.sh qpa"
        "install-linux-qt-desktop-deps.sh public")
    require_token("${ci}" "${token}" "consolidated PR CI contract")
endforeach()
forbid_token("${ci}" "--integrations=cpp,qml,generic,qpa"
             "PR CI must not hard-code all four frontends for every product change")
forbid_token("${ci}" ".hyremote-complete-"
             "Qt cache integrity must not be marker-only")

read_required(".github/scripts/install-linux-qt-desktop-deps.sh" deps)
foreach(token IN ITEMS
        [=[profile="${1:-public}"]=]
        "public"
        "qpa"
        "dpkg-query"
        "libxcb-cursor0"
        "libxkbcommon-x11-0"
        "libxcb-icccm4"
        "libxkbcommon-x11-dev")
    require_token("${deps}" "${token}" "capability-scoped Linux host dependency contract")
endforeach()
# ubuntu-24.04 provides xvfb. Comments may mention it, but the package list must not install it.
forbid_token("${deps}" [=[  xvfb
]=]
             "Linux dependency script must not reinstall runner-provided xvfb")

read_required(".github/workflows/mainline-validation.yml" mainline)
foreach(token IN ITEMS
        "compile.cmd"
        "--integrations=cpp,qml,generic,qpa"
        "install-linux-qt-desktop-deps.sh qpa"
        "src/integrations/cpp/tests/rfb_product_fit.py"
        "build-mainline/runtime"
        "build-mainline/integrations/cpp/tests/hyremote-rfb-test-server")
    require_token("${mainline}" "${token}" "mainline validation contract")
endforeach()
forbid_token("${mainline}" "cmake -S ."
             "mainline product validation must use the unified compile.cmd authority")
forbid_token("${mainline}" "src/cpp/tests"
             "mainline validation must not use the legacy flat C++ frontend path")
forbid_token("${mainline}" "build-mainline/remoteaccess"
             "mainline validation must not use the historical runtime binary-directory alias")

message(STATUS
    "HyRemote CI environment baseline gate: PASS "
    "(consolidated workflows + capability-scoped PR CI + validated Qt cache + scoped Linux deps + unified mainline build authority)")
