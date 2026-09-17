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

# These names are deliberately forbidden at repository root. Product source belongs under src/,
# integration payloads under integrations/, non-product experiments under research/, and branding
# under assets/. Keeping aliases/copies would recreate the ambiguity this layout removes.
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

if(NOT EXISTS "${HYREMOTE_SOURCE_DIR}/docs/repository-layout.md")
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

# Research must remain opt-in and outside the V1 product graph. The existing option name is retained
# for source compatibility, but its physical source tree is canonicalized under research/.
string(FIND "${root_cmake}" "if(HYREMOTE_BUILD_SPIKES)" research_guard)
string(FIND "${root_cmake}" "add_subdirectory(research/capture spikes/capture)" research_path)
if(research_guard EQUAL -1 OR research_path EQUAL -1 OR research_path LESS research_guard)
    message(FATAL_ERROR "repository-layout: research sources escaped their explicit opt-in guard")
endif()

# A clean directory tree is not enough: the physical module boundaries must continue to represent
# the frozen V1 architecture. QML and QPA are integration payloads over the one shared C++ runtime;
# they may use private implementation seams where necessary, but they must not become a second Core
# or RemoteAccess product/runtime by linking Core directly or compiling the facade implementation.
read_repo_file("integrations/qml/HyRemote/CMakeLists.txt" qml_cmake)
require_token("${qml_cmake}" "HyRemote::RemoteAccess"
              "QML integration stopped reusing the shared RemoteAccess runtime")
forbid_token("${qml_cmake}" "HyRemote::Core"
             "QML integration must not link Core directly")
forbid_token("${qml_cmake}" "remote_access.cpp"
             "QML integration must not compile a second RemoteAccess facade")

read_repo_file("integrations/qpa/CMakeLists.txt" qpa_cmake)
require_token("${qpa_cmake}" "HyRemote::RemoteAccess"
              "QPA integration stopped reusing the shared RemoteAccess runtime")
forbid_token("${qpa_cmake}" "HyRemote::Core"
             "QPA platform payload must not link Core directly")
forbid_token("${qpa_cmake}" "remote_access.cpp"
             "QPA platform payload must not compile a second RemoteAccess facade")

# Product and integration build definitions must never consume research evidence or branding assets.
# Those trees are repository-support material and cannot become hidden runtime/build dependencies.
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

message(STATUS
    "HyRemote repository layout gate: PASS "
    "(canonical source layout + stable binary mapping + one shared RemoteAccess runtime across integrations + research/assets isolated)")
