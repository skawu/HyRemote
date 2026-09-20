cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

file(READ "${HYREMOTE_SOURCE_DIR}/CMakeLists.txt" root_cmake)

foreach(required_token
        "project(HyRemote VERSION"
        "include(HyRemoteReleaseProfile)"
        "include(HyRemoteDeploy)")
    string(FIND "${root_cmake}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "release-readiness: root metadata contract missing: ${required_token}")
    endif()
endforeach()

# Keep the project declaration on one line: downstream metadata parsing and package version generation
# intentionally consume one authoritative project(VERSION ...) declaration.
string(REGEX MATCH "project\\(HyRemote[ \t]+VERSION[ \t]+[0-9]+\\.[0-9]+\\.[0-9]+[ \t]+DESCRIPTION" project_version_decl "${root_cmake}")
if(project_version_decl STREQUAL "")
    message(FATAL_ERROR "release-readiness: project(VERSION ...) is missing or malformed")
endif()

file(READ "${HYREMOTE_SOURCE_DIR}/cmake/HyRemoteProjectOptions.cmake" options_cmake)
foreach(required_token
        "option(HYREMOTE_BUILD_CORE"
        "option(HYREMOTE_BUILD_REMOTE_ACCESS"
        "option(HYREMOTE_BUILD_QML_API"
        "option(HYREMOTE_WITH_QPA_PROXY"
        "option(HYREMOTE_WITH_VNC"
        "HYREMOTE_DEFAULT_PORT")
    string(FIND "${options_cmake}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "release-readiness: project option contract missing: ${required_token}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/cmake/HyRemoteInstall.cmake" install_cmake)
foreach(required_token
        "HyRemoteTargets"
        "HyRemoteConfig.cmake"
        "HyRemoteConfigVersion.cmake")
    string(FIND "${install_cmake}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "release-readiness: install/package contract missing: ${required_token}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/cmake/HyRemoteConfig.cmake.in" package_config)
foreach(required_token
        "HyRemoteTargets.cmake"
        "find_dependency(Qt6"
        "HyRemoteDeploy.cmake")
    string(FIND "${package_config}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "release-readiness: package config contract missing: ${required_token}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/src/core/CMakeLists.txt" core_cmake)
string(FIND "${core_cmake}" "add_library(hyremote-core STATIC" core_static)
if(core_static EQUAL -1)
    message(FATAL_ERROR "release-readiness: V1 Core must remain an internal STATIC composition target")
endif()
string(FIND "${core_cmake}" "install(" core_install)
if(NOT core_install EQUAL -1)
    message(FATAL_ERROR "release-readiness: V1 Core must not be installed/exported as a second product target")
endif()

# #219 moved shared-runtime target ownership out of the Embedded C++ frontend and into src/runtime.
# The delivered binary/API contract itself remains unchanged: one SHARED target exported as
# HyRemote::RemoteAccess with the established output name.
file(READ "${HYREMOTE_SOURCE_DIR}/src/runtime/CMakeLists.txt" remoteaccess_cmake)
foreach(required_token
        "add_library(hyremote-remoteaccess SHARED"
        "OUTPUT_NAME HyRemoteRemoteAccess"
        "EXPORT_NAME RemoteAccess")
    string(FIND "${remoteaccess_cmake}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "release-readiness: RemoteAccess shared runtime contract missing: ${required_token}")
    endif()
endforeach()

# Embedded C++ remains the owner of the installed facade/header, not the owner of the common runtime target.
file(READ "${HYREMOTE_SOURCE_DIR}/src/cpp/CMakeLists.txt" cpp_cmake)
foreach(required_token
        "src/remote_access.cpp"
        "RemoteAccessExport.h")
    string(FIND "${cpp_cmake}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "release-readiness: Embedded C++ facade contract missing: ${required_token}")
    endif()
endforeach()
string(FIND "${cpp_cmake}" "add_library(hyremote-remoteaccess SHARED" cpp_owns_runtime)
if(NOT cpp_owns_runtime EQUAL -1)
    message(FATAL_ERROR "release-readiness: Embedded C++ frontend must not re-own the common shared runtime target")
endif()

file(READ "${HYREMOTE_SOURCE_DIR}/src/qml/CMakeLists.txt" qml_cmake)
string(FIND "${qml_cmake}" "TARGETS hyremote-qml\n    EXPORT HyRemoteTargets" qml_export)
if(NOT qml_export EQUAL -1)
    message(FATAL_ERROR
        "release-readiness: declarative QML backing library must not become a second C++ SDK target")
endif()

file(READ "${HYREMOTE_SOURCE_DIR}/src/qpa/CMakeLists.txt" qpa_cmake)
string(FIND "${qpa_cmake}" "add_library(hyremote-qpa-platform MODULE" qpa_module)
if(qpa_module EQUAL -1)
    message(FATAL_ERROR "release-readiness: Transparent QPA must remain a platform MODULE")
endif()
string(FIND "${qpa_cmake}" "EXPORT HyRemoteTargets" qpa_export)
if(NOT qpa_export EQUAL -1)
    message(FATAL_ERROR
        "release-readiness: qhyremote must install as payload, not export HyRemote::QpaPlatform")
endif()

message(STATUS "HyRemote release metadata gate: PASS")
