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
    "src/runtime"
    "src/cpp"
    "src/qml"
    "src/qpa"
    "tests"
    "examples"
    "logo"
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
    "runtime"
    "remoteaccess"
    "qml"
    "qpa"
    "spikes"
    "integrations"
    "verification"
    "assets"
    "research")
foreach(path IN LISTS forbidden_root_directories)
    if(EXISTS "${HYREMOTE_SOURCE_DIR}/${path}")
        message(FATAL_ERROR "repository-layout: legacy root directory must not return: ${path}")
    endif()
endforeach()

if(NOT EXISTS "${HYREMOTE_SOURCE_DIR}/docs/internal/repository-layout.md")
    message(FATAL_ERROR "repository-layout: canonical layout documentation is missing")
endif()

# Access-mode payload directories are named for their integration technology. `src/runtime` is the
# one shared product-runtime implementation layer and is not an integration frontend. Historical
# access-quality/artifact names must not return.
foreach(stale_src IN ITEMS "src/remoteaccess" "src/embedded" "src/declarative" "src/transparent" "docs/assets")
    if(EXISTS "${HYREMOTE_SOURCE_DIR}/${stale_src}")
        message(FATAL_ERROR "repository-layout: stale directory must not return: ${stale_src}")
    endif()
endforeach()

file(GLOB_RECURSE _layout_drivers RELATIVE "${HYREMOTE_SOURCE_DIR}"
     "${HYREMOTE_SOURCE_DIR}/.github/workflows/*.yml"
     "${HYREMOTE_SOURCE_DIR}/.github/workflows/*.yaml"
     "${HYREMOTE_SOURCE_DIR}/.github/scripts/*.ps1"
     "${HYREMOTE_SOURCE_DIR}/.github/scripts/*.sh"
     "${HYREMOTE_SOURCE_DIR}/cmake/*.cmake"
     "${HYREMOTE_SOURCE_DIR}/src/*/CMakeLists.txt"
     "${HYREMOTE_SOURCE_DIR}/examples/*/CMakeLists.txt")
list(APPEND _layout_drivers "CMakeLists.txt")
foreach(_driver IN LISTS _layout_drivers)
    file(READ "${HYREMOTE_SOURCE_DIR}/${_driver}" _driver_text)
    string(REPLACE "\\" "/" _driver_text "${_driver_text}")
    foreach(stale_src IN ITEMS "src/remoteaccess" "src/embedded" "src/declarative" "src/transparent" "docs/assets/logo")
        if(_driver_text MATCHES "${stale_src}")
            message(FATAL_ERROR
                "repository-layout: ${_driver} still refers to the stale source path ${stale_src}; canonical "
                "ownership is src/runtime plus peer src/cpp, src/qml and src/qpa frontends")
        endif()
    endforeach()
endforeach()

read_repo_file("CMakeLists.txt" root_cmake)
foreach(required_token
        [=[add_subdirectory(src/core core)]=]
        [=[add_subdirectory(src/cpp remoteaccess)]=]
        [=[add_subdirectory(src/qml qml/HyRemote)]=]
        [=[add_subdirectory(src/qpa qpa)]=])
    require_token("${root_cmake}" "${required_token}"
                  "root build graph lost canonical-source / stable-binary mapping")
endforeach()

# The current migration keeps the stable `remoteaccess` binary directory by entering runtime from the
# C++ frontend directory. Pin that ownership explicitly until the later root-level mapping is changed in
# a dedicated, evidence-backed step.
read_repo_file("src/cpp/CMakeLists.txt" cpp_cmake)
require_token("${cpp_cmake}" [=[add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../runtime" "${CMAKE_CURRENT_BINARY_DIR}/runtime")]=]
              "Embedded C++ frontend stopped composing the common runtime migration target")
forbid_token("${cpp_cmake}" "add_library(hyremote-remoteaccess SHARED"
             "Embedded C++ frontend must not own the shared runtime target")
require_token("${cpp_cmake}" "src/remote_access.cpp"
              "Embedded C++ frontend lost its RemoteAccess facade")

read_repo_file("src/runtime/CMakeLists.txt" runtime_cmake)
require_token("${runtime_cmake}" "add_library(hyremote-remoteaccess SHARED"
              "common runtime no longer owns the shared RemoteAccess target")
require_link_target("${runtime_cmake}" "HyRemote::Core"
                    "common runtime stopped composing the shared Core implementation")
forbid_token("${runtime_cmake}" "remote_access.cpp"
             "common runtime must not own the Embedded C++ facade")

require_token("${root_cmake}" "NAME hyremote-release-readiness-package-acquisition-isolation"
              "package-acquisition isolation gate is not registered in CTest")
require_token("${root_cmake}" "check_package_acquisition_isolation.cmake"
              "package-acquisition isolation gate lost its executable script")

forbid_token("${root_cmake}" "HYREMOTE_BUILD_SPIKES"
             "retired spike harness switch returned to the root build")
forbid_token("${root_cmake}" "add_subdirectory(research/"
             "root build graph includes non-product research again")

read_repo_file("src/qml/CMakeLists.txt" qml_cmake)
require_link_target("${qml_cmake}" "HyRemote::RemoteAccess"
                    "QML integration stopped linking the shared RemoteAccess runtime")
forbid_link_target("${qml_cmake}" "HyRemote::Core"
                   "QML integration must not link Core directly")
forbid_token("${qml_cmake}" "remote_access.cpp"
             "QML integration must not compile a second RemoteAccess facade")

read_repo_file("src/qpa/CMakeLists.txt" qpa_cmake)
require_link_target("${qpa_cmake}" "HyRemote::RemoteAccess"
                    "QPA integration stopped linking the shared RemoteAccess runtime")
forbid_link_target("${qpa_cmake}" "HyRemote::Core"
                   "QPA platform payload must not link Core directly")
forbid_token("${qpa_cmake}" "remote_access.cpp"
             "QPA platform payload must not compile a second RemoteAccess facade")

foreach(module_cmake IN ITEMS
        "src/core/CMakeLists.txt"
        "src/runtime/CMakeLists.txt"
        "src/cpp/CMakeLists.txt"
        "src/qml/CMakeLists.txt"
        "src/qpa/CMakeLists.txt")
    read_repo_file("${module_cmake}" module_text)
    forbid_token("${module_text}" "research/"
                 "product/integration module depends on non-product research (${module_cmake})")
    forbid_token("${module_text}" "logo/"
                 "product/integration module depends on branding assets (${module_cmake})")
endforeach()

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
    "(UI-neutral Core + one common runtime + peer integration frontends + stable binary mapping + one shared RemoteAccess runtime + retired non-product trees + bounded/cancellable workflow fan-out)")
