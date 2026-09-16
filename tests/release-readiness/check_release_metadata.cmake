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

file(READ "${HYREMOTE_SOURCE_DIR}/CMakeLists.txt" root_cmake)
if(root_cmake MATCHES "VERSION[ \\t\\r\\n]+1\\.0\\.0\\.0")
    message(FATAL_ERROR
        "release-readiness: feature/develop tree must not set project VERSION 1.0.0.0; "
        "that change belongs on release/v1.0.0.0")
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

message(STATUS "HyRemote release-readiness metadata gate: PASS")
