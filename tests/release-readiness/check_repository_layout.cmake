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
    "src/integrations"
    "src/integrations/cpp"
    "src/integrations/qml"
    "src/integrations/generic"
    "src/integrations/qpa"
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

# Shared implementation lives at src/core + src/runtime. Application integration technologies are
# grouped below src/integrations. Legacy flat frontend roots and historical quality/artifact names must
# not return as compatibility copies, symlinks or forwarding directories.
foreach(stale_src IN ITEMS
        "src/cpp"
        "src/qml"
        "src/generic"
        "src/qpa"
        "src/remoteaccess"
        "src/embedded"
        "src/declarative"
        "src/transparent"
        "docs/assets")
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
     "${HYREMOTE_SOURCE_DIR}/src/integrations/*/CMakeLists.txt"
     "${HYREMOTE_SOURCE_DIR}/examples/*/CMakeLists.txt")
list(APPEND _layout_drivers "CMakeLists.txt")
foreach(_driver IN LISTS _layout_drivers)
    file(READ "${HYREMOTE_SOURCE_DIR}/${_driver}" _driver_text)
    string(REPLACE "\\" "/" _driver_text "${_driver_text}")
    foreach(stale_src IN ITEMS
            "src/remoteaccess"
            "src/embedded"
            "src/declarative"
            "src/transparent"
            "docs/assets/logo")
        if(_driver_text MATCHES "${stale_src}")
            message(FATAL_ERROR
                "repository-layout: ${_driver} still refers to stale path ${stale_src}; canonical ownership is "
                "src/core + src/runtime + peer src/integrations/* frontends")
        endif()
    endforeach()
endforeach()

read_repo_file("CMakeLists.txt" root_cmake)
foreach(required_token
        [=[add_subdirectory(src/core core)]=]
        [=[add_subdirectory(src/runtime remoteaccess)]=]
        [=[add_subdirectory(src/integrations/cpp remoteaccess/cpp)]=]
        [=[add_subdirectory(src/integrations/qml qml/HyRemote)]=]
        [=[add_subdirectory(src/integrations/generic generic)]=]
        [=[add_subdirectory(src/integrations/qpa qpa)]=])
    require_token("${root_cmake}" "${required_token}"
                  "root build graph lost canonical-source / stable-binary mapping")
endforeach()
forbid_token("${root_cmake}" "add_subdirectory(src/cpp "
             "root build graph must not use the legacy flat C++ frontend path")
forbid_token("${root_cmake}" "add_subdirectory(src/qml "
             "root build graph must not use the legacy flat QML frontend path")
forbid_token("${root_cmake}" "add_subdirectory(src/qpa "
             "root build graph must not use the legacy flat QPA frontend path")

# Runtime is a first-class shared implementation layer. No integration frontend may create/enter it.
read_repo_file("src/integrations/cpp/CMakeLists.txt" cpp_cmake)
forbid_token("${cpp_cmake}" "add_subdirectory("
             "Embedded C++ frontend must not enter another product layer")
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

read_repo_file("src/integrations/qml/CMakeLists.txt" qml_cmake)
require_link_target("${qml_cmake}" "HyRemote::RemoteAccess"
                    "QML integration stopped linking the shared runtime payload")
forbid_link_target("${qml_cmake}" "HyRemote::Core"
                   "QML integration must not link Core directly")
forbid_token("${qml_cmake}" "remote_access.cpp"
             "QML integration must not compile a second RemoteAccess facade")

read_repo_file("src/integrations/generic/CMakeLists.txt" generic_cmake)
require_link_target("${generic_cmake}" "HyRemote::RemoteAccess"
                    "Generic Plugin stopped linking the shared runtime payload")
forbid_link_target("${generic_cmake}" "HyRemote::Core"
                   "Generic Plugin must not link Core directly")
forbid_token("${generic_cmake}" "GuiPrivate"
             "Generic Plugin must not depend on Qt private/QPA APIs")
forbid_token("${generic_cmake}" "remote_access.cpp"
             "Generic Plugin must not compile the Embedded C++ facade")

read_repo_file("src/integrations/qpa/CMakeLists.txt" qpa_cmake)
require_link_target("${qpa_cmake}" "HyRemote::RemoteAccess"
                    "QPA integration stopped linking the shared runtime payload")
forbid_link_target("${qpa_cmake}" "HyRemote::Core"
                   "QPA platform payload must not link Core directly")
forbid_token("${qpa_cmake}" "remote_access.cpp"
             "QPA platform payload must not compile a second RemoteAccess facade")

foreach(module_cmake IN ITEMS
        "src/core/CMakeLists.txt"
        "src/runtime/CMakeLists.txt"
        "src/integrations/cpp/CMakeLists.txt"
        "src/integrations/qml/CMakeLists.txt"
        "src/integrations/generic/CMakeLists.txt"
        "src/integrations/qpa/CMakeLists.txt")
    read_repo_file("${module_cmake}" module_text)
    forbid_token("${module_text}" "research/"
                 "product/integration module depends on non-product research (${module_cmake})")
    forbid_token("${module_text}" "logo/"
                 "product/integration module depends on branding assets (${module_cmake})")
endforeach()

# New consolidated CI owns product validation. Legacy per-slice workflows must not be required by the
# architecture gate; when present during migration they remain subject to their own policy checks.
if(EXISTS "${HYREMOTE_SOURCE_DIR}/.github/workflows/ci.yml")
    read_repo_file(".github/workflows/ci.yml" ci_workflow)
    require_token("${ci_workflow}" "concurrency:"
                  "ci.yml lost the superseded-run concurrency guard")
    require_token("${ci_workflow}" "cancel-in-progress: true"
                  "ci.yml stopped cancelling superseded runs")
    require_token("${ci_workflow}" "--integrations=cpp,qml,generic,qpa"
                  "consolidated CI stopped exercising all four integration frontends")
endif()

if(NOT EXISTS "${HYREMOTE_SOURCE_DIR}/.github/scripts/install-linux-qt-desktop-deps.sh")
    message(FATAL_ERROR "repository-layout: shared Linux Qt desktop dependency script is missing")
endif()

message(STATUS
    "HyRemote repository layout gate: PASS "
    "(UI-neutral Core + root-owned common runtime + grouped peer integration frontends + stable binary mapping + retired legacy trees)")
