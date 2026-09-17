cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

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

file(READ "${HYREMOTE_SOURCE_DIR}/CMakeLists.txt" root_cmake)
foreach(required_token
        [=[add_subdirectory(src/core core)]=]
        [=[add_subdirectory(src/remoteaccess remoteaccess)]=]
        [=[add_subdirectory(integrations/qml/HyRemote qml/HyRemote)]=]
        [=[add_subdirectory(integrations/qpa qpa)]=]
        [=[add_subdirectory(research/capture spikes/capture)]=]
        [=[add_subdirectory(research/async-capture spikes/async-capture)]=])
    string(FIND "${root_cmake}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "repository-layout: root build graph lost canonical-source / stable-binary mapping: ${required_token}")
    endif()
endforeach()

# Research must remain opt-in and outside the V1 product graph. The existing option name is retained
# for source compatibility, but its physical source tree is canonicalized under research/.
string(FIND "${root_cmake}" "if(HYREMOTE_BUILD_SPIKES)" research_guard)
string(FIND "${root_cmake}" "add_subdirectory(research/capture spikes/capture)" research_path)
if(research_guard EQUAL -1 OR research_path EQUAL -1 OR research_path LESS research_guard)
    message(FATAL_ERROR "repository-layout: research sources escaped their explicit opt-in guard")
endif()

message(STATUS
    "HyRemote repository layout gate: PASS "
    "(src product core/runtime + integrations payloads + research evidence + assets branding; legacy root module directories absent)")
