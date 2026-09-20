cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

function(read_repo_file relative_path output_var)
    set(path "${HYREMOTE_SOURCE_DIR}/${relative_path}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "repository-layout: missing required file: ${relative_path}")
    endif()
    file(READ "${path}" text)
    set(${output_var} "${text}" PARENT_SCOPE)
endfunction()

function(require_token text token description)
    string(FIND "${text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "repository-layout: ${description} missing: ${token}")
    endif()
endfunction()

function(forbid_token text token description)
    string(FIND "${text}" "${token}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "repository-layout: ${description}: ${token}")
    endif()
endfunction()

function(require_link_target text target description)
    string(REGEX MATCH "target_link_libraries[ \t\r\n]*\\([^)]*${target}" match "${text}")
    if("${match}" STREQUAL "")
        message(FATAL_ERROR "repository-layout: ${description}: ${target}")
    endif()
endfunction()

function(forbid_link_target text target description)
    string(REGEX MATCH "target_link_libraries[ \t\r\n]*\\([^)]*${target}" match "${text}")
    if(NOT "${match}" STREQUAL "")
        message(FATAL_ERROR "repository-layout: ${description}: ${target}")
    endif()
endfunction()

set(required_directories
    "src/core"
    "src/remoteaccess"
    "integrations/qml/HyRemote"
    "integrations/qpa"
    "tests"
    "examples"
    "research"
    "assets/branding"
    "cmake"
    "docs"
    ".github")
foreach(path IN LISTS required_directories)
    if(NOT IS_DIRECTORY "${HYREMOTE_SOURCE_DIR}/${path}")
        message(FATAL_ERROR "repository-layout: missing canonical directory: ${path}")
    endif()
endforeach()

set(forbidden_root_directories
    "core"
    "remoteaccess"
    "qml"
    "qpa"
    "spikes"
    "logo")
foreach(path IN LISTS forbidden_root_directories)
    if(EXISTS "${HYREMOTE_SOURCE_DIR}/${path}")
        message(FATAL_ERROR "repository-layout: legacy root directory must not return: ${path}")
    endif()
endforeach()

if(NOT EXISTS "${HYREMOTE_SOURCE_DIR}/docs/internal/repository-layout.md")
    message(FATAL_ERROR "repository-layout: canonical layout documentation is missing")
endif()

read_repo_file("CMakeLists.txt" root_cmake)
foreach(required_token
        [=[add_subdirectory(src/core core)]=]
        [=[add_subdirectory(src/remoteaccess remoteaccess)]=]
        [=[add_subdirectory(integrations/qml/HyRemote qml/HyRemote)]=]
        [=[add_subdirectory(integrations/qpa qpa)]=]
        [=[add_subdirectory(research/capture spikes/capture)]=]
        [=[add_subdirectory(research/async-capture spikes/async-capture)]=])
    require_token("${root_cmake}" "${required_token}"
                  "root build graph lost canonical-source / stable-binary mapping")
endforeach()

require_token("${root_cmake}" "NAME hyremote-release-readiness-package-acquisition-isolation"
              "package-acquisition isolation gate is not registered in CTest")
require_token("${root_cmake}" "check_package_acquisition_isolation.cmake"
              "package-acquisition isolation gate lost its executable script")

string(FIND "${root_cmake}" "if(HYREMOTE_BUILD_SPIKES)" research_guard)
string(FIND "${root_cmake}" "add_subdirectory(research/capture spikes/capture)" research_path)
if(research_guard EQUAL -1 OR research_path EQUAL -1 OR research_path LESS research_guard)
    message(FATAL_ERROR "repository-layout: research sources escaped their explicit opt-in guard")
endif()

read_repo_file("integrations/qml/HyRemote/CMakeLists.txt" qml_cmake)
require_link_target("${qml_cmake}" "HyRemote::RemoteAccess"
                    "QML integration stopped linking the shared RemoteAccess runtime")
forbid_link_target("${qml_cmake}" "HyRemote::Core"
                   "QML integration must not link Core directly")
forbid_token("${qml_cmake}" "remote_access.cpp"
             "QML integration must not compile a second RemoteAccess facade")

read_repo_file("integrations/qpa/CMakeLists.txt" qpa_cmake)
require_link_target("${qpa_cmake}" "HyRemote::RemoteAccess"
                    "QPA integration stopped linking the shared RemoteAccess runtime")
forbid_link_target("${qpa_cmake}" "HyRemote::Core"
                   "QPA platform payload must not link Core directly")
forbid_token("${qpa_cmake}" "remote_access.cpp"
             "QPA platform payload must not compile a second RemoteAccess facade")

foreach(module_cmake IN ITEMS
        "src/core/CMakeLists.txt"
        "src/remoteaccess/CMakeLists.txt"
        "integrations/qml/HyRemote/CMakeLists.txt"
        "integrations/qpa/CMakeLists.txt")
    read_repo_file("${module_cmake}" module_text)
    forbid_token("${module_text}" "research/"
                 "product/integration module depends on non-product research (${module_cmake})")
    forbid_token("${module_text}" "assets/branding"
                 "product/integration module depends on branding assets (${module_cmake})")
endforeach()

# Every continuously triggered workflow must cancel superseded runs on the same ref. Without this,
# rapid convergence commits can create a backlog in which the current candidate never reaches a runner.
# Mainline validation uses the same rule so multiple main pushes cannot queue stale post-merge evidence.
foreach(workflow IN ITEMS
        "git-flow-policy.yml"
        "qml-api.yml"
        "qpa-proxy.yml"
        "quick-adapter.yml"
        "remoteaccess-facade.yml"
        "rfb-transport.yml"
        "sdk-consumption.yml"
        "v1-ga-acceptance.yml"
        "widgets-adapter.yml"
        "mainline-validation.yml")
    read_repo_file(".github/workflows/${workflow}" workflow_text)
    require_token("${workflow_text}" "concurrency:"
                  "${workflow} lost the superseded-run concurrency guard")
    require_token("${workflow_text}" "cancel-in-progress: true"
                  "${workflow} stopped cancelling superseded runs")
    require_token("${workflow_text}" [=[group: ${{ github.workflow }}-${{ github.ref }}]=]
                  "${workflow} lost its per-workflow/per-ref concurrency identity")
endforeach()

# Every workflow that builds the default facade graph or explicit Qt GUI/Widgets/Quick/QML/QPA code
# on the Ubuntu reference runner must use the same repository-owned host dependency baseline.
# Focused and integrated GA jobs must not carry subtly different XCB/OpenGL provisioning, otherwise
# CI failures become workflow-specific noise rather than product evidence. The RFB-only workflow is
# intentionally excluded because it disables Widgets/Quick and exercises only Core/Network paths.
if(NOT EXISTS "${HYREMOTE_SOURCE_DIR}/.github/scripts/install-linux-qt-desktop-deps.sh")
    message(FATAL_ERROR "repository-layout: shared Linux Qt desktop dependency script is missing")
endif()
foreach(workflow IN ITEMS
        "remoteaccess-facade.yml"
        "widgets-adapter.yml"
        "quick-adapter.yml"
        "qml-api.yml"
        "qpa-proxy.yml"
        "sdk-consumption.yml"
        "v1-ga-acceptance.yml"
        "mainline-validation.yml")
    read_repo_file(".github/workflows/${workflow}" workflow_text)
    require_token("${workflow_text}" ".github/scripts/install-linux-qt-desktop-deps.sh"
                  "${workflow} does not use the shared Linux Qt desktop dependency baseline")
endforeach()

message(STATUS
    "HyRemote repository layout gate: PASS "
    "(canonical source layout + stable binary mapping + executable acquisition gate + one shared RemoteAccess runtime across integrations + research/assets isolated + bounded/cancellable workflow fan-out + shared Linux Qt desktop CI baseline)")
