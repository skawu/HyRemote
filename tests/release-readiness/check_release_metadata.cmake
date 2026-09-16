cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

set(required_files
    "LICENSE"
    "NOTICE.md"
    "docs/versioning.md"
    "docs/release-package-manifest.md"
    "docs/releases/v1.0.0.0.md"
)

foreach(path IN LISTS required_files)
    if(NOT EXISTS "${HYREMOTE_SOURCE_DIR}/${path}")
        message(FATAL_ERROR "release-readiness: missing required file: ${path}")
    endif()
endforeach()

# Repository metadata and configured project version must describe the same candidate. Git branch
# authorization remains the responsibility of the Git Flow workflow so this same test can run on a
# real release/vX.Y.Z.W tree.
file(READ "${HYREMOTE_SOURCE_DIR}/CMakeLists.txt" root_cmake)
string(REGEX MATCH
    "project[ \\t\\r\\n]*\\([ \\t\\r\\n]*HyRemote[ \\t\\r\\n]+VERSION[ \\t\\r\\n]+([0-9]+(\\.[0-9]+){2,3})"
    project_match
    "${root_cmake}")
if(NOT project_match)
    message(FATAL_ERROR "release-readiness: root project(VERSION ...) is missing or malformed")
endif()
set(source_project_version "${CMAKE_MATCH_1}")

if(DEFINED HYREMOTE_PROJECT_VERSION
        AND NOT "${HYREMOTE_PROJECT_VERSION}" STREQUAL "${source_project_version}")
    message(FATAL_ERROR
        "release-readiness: configured project version ${HYREMOTE_PROJECT_VERSION} does not match "
        "source project version ${source_project_version}")
endif()

file(READ "${HYREMOTE_SOURCE_DIR}/docs/releases/v1.0.0.0.md" release_notes)
foreach(required_phrase
        "candidate / acceptance pending"
        "SecurityType None"
        "v1.0.0.0"
        "release/v1.0.0.0")
    string(FIND "${release_notes}" "${required_phrase}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: release notes missing required boundary text: ${required_phrase}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/NOTICE.md" notice_text)
foreach(required_phrase "Qt" "Apache License 2.0" "vncdotool" "Pillow")
    string(FIND "${notice_text}" "${required_phrase}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "release-readiness: NOTICE.md missing classification: ${required_phrase}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/cmake/HyRemoteInstall.cmake" install_rules)
foreach(required_token "LICENSE" "NOTICE.md" "HyRemote/licenses")
    string(FIND "${install_rules}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: install rules do not preserve required release metadata token: ${required_token}")
    endif()
endforeach()

# Freeze the simple V1 product shape. Internal architecture remains rich, but ordinary users get one
# C++ shared facade or the QPA MODULE; Core must not reappear as a second installed application SDK.
file(READ "${HYREMOTE_SOURCE_DIR}/core/CMakeLists.txt" core_cmake)
string(FIND "${core_cmake}" "add_library(hyremote-core STATIC" core_static)
if(core_static EQUAL -1)
    message(FATAL_ERROR "release-readiness: V1 Core must remain an internal STATIC composition target")
endif()
string(FIND "${core_cmake}" "install(" core_install)
if(NOT core_install EQUAL -1)
    message(FATAL_ERROR "release-readiness: V1 Core must not be installed/exported as a second product target")
endif()

file(READ "${HYREMOTE_SOURCE_DIR}/remoteaccess/CMakeLists.txt" remoteaccess_cmake)
foreach(required_token
        "add_library(hyremote-remoteaccess SHARED"
        "OUTPUT_NAME HyRemoteRemoteAccess"
        "EXPORT_NAME RemoteAccess")
    string(FIND "${remoteaccess_cmake}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "release-readiness: RemoteAccess shared facade contract missing: ${required_token}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/qpa/CMakeLists.txt" qpa_cmake)
string(FIND "${qpa_cmake}" "add_library(hyremote-qpa-platform MODULE" qpa_module)
if(qpa_module EQUAL -1)
    message(FATAL_ERROR "release-readiness: Transparent QPA must remain a platform MODULE")
endif()

file(READ "${HYREMOTE_SOURCE_DIR}/cmake/HyRemoteConfig.cmake.in" package_config)
foreach(forbidden_token "HyRemote::Core" "find_dependency(Threads)")
    string(FIND "${package_config}" "${forbidden_token}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: installed package leaked internal Core contract: ${forbidden_token}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/docs/release-package-manifest.md" package_manifest)
foreach(required_phrase
        "HyRemote::RemoteAccess"
        "not installed/exported"
        "qhyremote"
        "hyremote_deploy(TARGET MyApp)")
    string(FIND "${package_manifest}" "${required_phrase}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: package manifest missing simple V1 product contract: ${required_phrase}")
    endif()
endforeach()

message(STATUS
    "HyRemote release-readiness metadata gate: PASS (project ${source_project_version}, simple V1 artifact surface)")
