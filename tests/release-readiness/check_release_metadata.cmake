cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

# V1 release metadata, complete user documentation and the E1-E6 acceptance entry points are product
# artifacts, not post-release polish. Keep this list repository-relative so the same deterministic
# gate runs on feature/develop and later on authorized release branches.
set(required_files
    "LICENSE"
    "NOTICE.md"
    "README.md"
    "docs/versioning.md"
    "docs/release-package-manifest.md"
    "docs/releases/v0.0.1.0.md"
    "docs/releases/v0.0.2.0.md"
    "docs/releases/v0.0.3.0.md"
    "docs/releases/v1.0.0.0.md"
    "docs/getting-started/windows.md"
    "docs/getting-started/linux.md"
    "docs/getting-started/cpp.md"
    "docs/getting-started/qml.md"
    "docs/getting-started/qpa-proxy.md"
    "docs/sdk-installation.md"
    "docs/source-consumption.md"
    "docs/deployment.md"
    "docs/qml-consumption.md"
    "docs/security.md"
    "docs/viewer-connection.md"
    "docs/troubleshooting.md"
    "docs/compatibility.md"
    "docs/known-limitations.md"
    "examples/README.md"
    "examples/CMakeLists.txt"
    "examples/widgets-basic/CMakeLists.txt"
    "examples/widgets-basic/README.md"
    "examples/quick-basic/CMakeLists.txt"
    "examples/quick-basic/README.md"
    "examples/qml-basic/CMakeLists.txt"
    "examples/qml-basic/README.md"
    "examples/qpa-proxy-existing-app/CMakeLists.txt"
    "examples/qpa-proxy-existing-app/README.md"
    "examples/remote-support-showcase/CMakeLists.txt"
    "examples/remote-support-showcase/README.md"
    "tests/consumer-installed-sdk/CMakeLists.txt"
    "tests/consumer-source/CMakeLists.txt"
)

foreach(path IN LISTS required_files)
    if(NOT EXISTS "${HYREMOTE_SOURCE_DIR}/${path}")
        message(FATAL_ERROR "release-readiness: missing required V1 product artifact: ${path}")
    endif()
endforeach()

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

# Every taggable product milestone owns release notes before its release branch is cut. This generic
# CTest validates stable facts only; candidate-vs-final status belongs to the Git Flow PR/tag gate so
# the exact same test suite remains runnable both during acceptance and after note finalization.
foreach(milestone_version
        "0.0.1.0"
        "0.0.2.0"
        "0.0.3.0"
        "1.0.0.0")
    set(release_note_path "${HYREMOTE_SOURCE_DIR}/docs/releases/v${milestone_version}.md")
    file(READ "${release_note_path}" milestone_notes)
    foreach(required_phrase
            "v${milestone_version}"
            "SecurityType None"
            "release/v${milestone_version}")
        string(FIND "${milestone_notes}" "${required_phrase}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "release-readiness: v${milestone_version} notes missing stable release fact: ${required_phrase}")
        endif()
    endforeach()
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
foreach(forbidden_token
        "HYREMOTE_PACKAGE_WITH_WIDGETS"
        "HYREMOTE_PACKAGE_WITH_QUICK")
    string(FIND "${install_rules}" "${forbidden_token}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: private UI adapter leaked into installed package dependency model: ${forbidden_token}")
    endif()
endforeach()

# Freeze the simple V1 product shape. Internal architecture remains rich, but ordinary users get one
# C++ shared facade; QML/QPA are package payloads behind import/deploy entry points.
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

file(READ "${HYREMOTE_SOURCE_DIR}/qml/HyRemote/CMakeLists.txt" qml_cmake)
string(FIND "${qml_cmake}" "TARGETS hyremote-qml\n    EXPORT HyRemoteTargets" qml_export)
if(NOT qml_export EQUAL -1)
    message(FATAL_ERROR
        "release-readiness: declarative QML backing library must not become a second C++ SDK target")
endif()

file(READ "${HYREMOTE_SOURCE_DIR}/qpa/CMakeLists.txt" qpa_cmake)
string(FIND "${qpa_cmake}" "add_library(hyremote-qpa-platform MODULE" qpa_module)
if(qpa_module EQUAL -1)
    message(FATAL_ERROR "release-readiness: Transparent QPA must remain a platform MODULE")
endif()
string(FIND "${qpa_cmake}" "EXPORT HyRemoteTargets" qpa_export)
if(NOT qpa_export EQUAL -1)
    message(FATAL_ERROR
        "release-readiness: qhyremote must install as payload, not export HyRemote::QpaPlatform")
endif()

file(READ "${HYREMOTE_SOURCE_DIR}/cmake/HyRemoteConfig.cmake.in" package_config)
foreach(forbidden_token
        "HyRemote::Core"
        "HyRemote::QpaPlatform"
        "HyRemote_QPA_SHARED_RUNTIME"
        "find_dependency(Threads)"
        "COMPONENTS Widgets"
        "COMPONENTS Quick"
        "COMPONENTS Qml")
    string(FIND "${package_config}" "${forbidden_token}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: installed package leaked internal/unused consumer dependency: ${forbidden_token}")
    endif()
endforeach()
foreach(required_token
        "find_dependency(Qt6 6.8 COMPONENTS Core Network)"
        "HyRemote_QML_IMPORT_PATH"
        "HyRemote_QPA_QT_VERSION"
        "HyRemote_QPA_PLUGIN_FILE")
    string(FIND "${package_config}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: installed package missing required minimal product metadata: ${required_token}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/docs/release-package-manifest.md" package_manifest)
foreach(required_phrase
        "HyRemote::RemoteAccess"
        "not installed/exported"
        "does not export `HyRemote::QpaPlatform`"
        "qhyremote"
        "hyremote_deploy(TARGET MyApp)")
    string(FIND "${package_manifest}" "${required_phrase}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: package manifest missing simple V1 product contract: ${required_phrase}")
    endif()
endforeach()

# README is the external-developer entry point. Every required V1 user path must remain directly
# discoverable instead of forcing users through architecture/internal documentation.
file(READ "${HYREMOTE_SOURCE_DIR}/README.md" readme_text)
foreach(required_link
        "docs/getting-started/cpp.md"
        "docs/getting-started/qml.md"
        "docs/getting-started/qpa-proxy.md"
        "docs/getting-started/windows.md"
        "docs/getting-started/linux.md"
        "docs/sdk-installation.md"
        "docs/source-consumption.md"
        "docs/deployment.md"
        "docs/viewer-connection.md"
        "docs/security.md"
        "docs/troubleshooting.md"
        "docs/compatibility.md"
        "docs/known-limitations.md")
    string(FIND "${readme_text}" "${required_link}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: README does not expose required V1 user guide: ${required_link}")
    endif()
endforeach()

# E1-E5 must participate in the common examples graph and each must carry its own user-facing README;
# E6 is the existing clean installed/source consumer fixture above and is intentionally not duplicated
# as a toy example target.
file(READ "${HYREMOTE_SOURCE_DIR}/examples/CMakeLists.txt" examples_cmake)
foreach(required_example
        "add_subdirectory(widgets-basic)"
        "add_subdirectory(quick-basic)"
        "add_subdirectory(qml-basic)"
        "add_subdirectory(qpa-proxy-existing-app)"
        "add_subdirectory(remote-support-showcase)")
    string(FIND "${examples_cmake}" "${required_example}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: common V1 examples graph missing required example: ${required_example}")
    endif()
endforeach()

message(STATUS
    "HyRemote release-readiness metadata gate: PASS "
    "(project ${source_project_version}, milestone notes + minimal SDK surface + complete V1 user entry points)")
