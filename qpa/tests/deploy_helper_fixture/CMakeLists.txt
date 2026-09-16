cmake_minimum_required(VERSION 3.21)
project(HyRemoteQpaDeployHelperProbe LANGUAGES CXX)

if(NOT HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

if(NOT DEFINED TEST_QML)
    set(TEST_QML OFF)
endif()
if(NOT DEFINED TEST_QPA_AVAILABLE)
    set(TEST_QPA_AVAILABLE ON)
endif()
if(NOT DEFINED TEST_SHARED_RUNTIME)
    set(TEST_SHARED_RUNTIME OFF)
endif()
if(NOT DEFINED TEST_QT_VERSION)
    set(TEST_QT_VERSION "6.8.3")
endif()

add_executable(deploy-probe EXCLUDE_FROM_ALL dummy.cpp)

if(TEST_QPA_AVAILABLE)
    add_library(qpa-probe MODULE EXCLUDE_FROM_ALL dummy.cpp)
    add_library(HyRemote::QpaPlatform ALIAS qpa-probe)
    set(HyRemote_QPA_AVAILABLE TRUE)
else()
    set(HyRemote_QPA_AVAILABLE FALSE)
endif()
set(HyRemote_QPA_QT_VERSION "6.8.3")

if(TEST_SHARED_RUNTIME)
    add_library(fake-remoteaccess SHARED EXCLUDE_FROM_ALL dummy.cpp)
    add_library(fake-core SHARED EXCLUDE_FROM_ALL dummy.cpp)
    add_library(HyRemote::RemoteAccess ALIAS fake-remoteaccess)
    add_library(HyRemote::Core ALIAS fake-core)
    set(HyRemote_QPA_SHARED_RUNTIME TRUE)
else()
    set(HyRemote_QPA_SHARED_RUNTIME FALSE)
endif()

set(Qt6Core_VERSION "${TEST_QT_VERSION}")

# The supplemental script includes Qt's low-level deployment support only at install time. A tiny
# deterministic stand-in is sufficient for configure/generate contract tests; the clean installed
# consumer exercises the real Qt 6.8.3 deployment implementation.
set(QT_DEPLOY_SUPPORT "${CMAKE_CURRENT_BINARY_DIR}/fake-qt-deploy-support.cmake")
file(WRITE "${QT_DEPLOY_SUPPORT}"
"function(qt_deploy_runtime_dependencies)\nendfunction()\n")

set(APP_DEPLOY_CALLED FALSE)
set(QML_DEPLOY_CALLED FALSE)

function(qt_generate_deploy_app_script)
    set(options NO_UNSUPPORTED_PLATFORM_ERROR)
    set(oneValueArgs TARGET OUTPUT_SCRIPT)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "" ${ARGN})

    if(TEST_QML)
        message(FATAL_ERROR "ordinary Qt deploy command called for QML+QPA probe")
    endif()
    if(NOT ARG_TARGET STREQUAL "deploy-probe")
        message(FATAL_ERROR "unexpected deployment target: ${ARG_TARGET}")
    endif()
    if(NOT ARG_NO_UNSUPPORTED_PLATFORM_ERROR)
        message(FATAL_ERROR "HyRemote must preserve NO_UNSUPPORTED_PLATFORM_ERROR")
    endif()

    set(script "${CMAKE_CURRENT_BINARY_DIR}/qt-app-deploy.cmake")
    file(WRITE "${script}" "# ordinary deployment probe\n")
    set(${ARG_OUTPUT_SCRIPT} "${script}" PARENT_SCOPE)
    set(APP_DEPLOY_CALLED TRUE PARENT_SCOPE)
endfunction()

function(qt_generate_deploy_qml_app_script)
    set(options NO_UNSUPPORTED_PLATFORM_ERROR DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM)
    set(oneValueArgs TARGET OUTPUT_SCRIPT)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "" ${ARGN})

    if(NOT TEST_QML)
        message(FATAL_ERROR "QML Qt deploy command called for ordinary QPA probe")
    endif()
    if(NOT ARG_TARGET STREQUAL "deploy-probe")
        message(FATAL_ERROR "unexpected deployment target: ${ARG_TARGET}")
    endif()
    if(NOT ARG_NO_UNSUPPORTED_PLATFORM_ERROR OR
       NOT ARG_DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM)
        message(FATAL_ERROR "HyRemote QML deployment options regressed")
    endif()

    set(script "${CMAKE_CURRENT_BINARY_DIR}/qt-qml-deploy.cmake")
    file(WRITE "${script}" "# QML deployment probe\n")
    set(${ARG_OUTPUT_SCRIPT} "${script}" PARENT_SCOPE)
    set(QML_DEPLOY_CALLED TRUE PARENT_SCOPE)
endfunction()

include("${HYREMOTE_SOURCE_DIR}/cmake/HyRemoteDeploy.cmake")

if(TEST_QML)
    hyremote_deploy(TARGET deploy-probe QML QPA)
    if(NOT QML_DEPLOY_CALLED OR APP_DEPLOY_CALLED)
        message(FATAL_ERROR "QML+QPA must preserve exclusive QML high-level deployment dispatch")
    endif()
else()
    hyremote_deploy(TARGET deploy-probe QPA)
    if(NOT APP_DEPLOY_CALLED OR QML_DEPLOY_CALLED)
        message(FATAL_ERROR "ordinary QPA must preserve ordinary high-level deployment dispatch")
    endif()
endif()
