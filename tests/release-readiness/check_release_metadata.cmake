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

# This test validates repository-owned release metadata only. Git branch/version authorization is
# deliberately owned by the Git Flow policy workflow: feature/develop PRs must not impersonate a
# release, while release/vX.Y.Z.W -> main must carry the exact matching four-part project version.
# Keeping those responsibilities separate lets the exact same CTest suite run on a real release
# candidate after project(VERSION ...) is changed to the authorized milestone version.
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

message(STATUS
    "HyRemote release-readiness metadata gate: PASS (project ${source_project_version})")
