cmake_minimum_required(VERSION 3.21)
project(HyRemoteDeployHelperProbe LANGUAGES CXX)

if(NOT HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

add_executable(deploy-probe EXCLUDE_FROM_ALL dummy.cpp)

# These deterministic test doubles verify only HyRemote's dispatch contract. The product helper must
# select exactly one Qt deployment API: the QML-aware command for QML applications and the ordinary
# command otherwise.
function(qt_generate_deploy_app_script)
    set(options NO_UNSUPPORTED_PLATFORM_ERROR)
    set(oneValueArgs TARGET OUTPUT_SCRIPT)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "" ${ARGN})

    if(TEST_QML)
        message(FATAL_ERROR "non-QML Qt deploy command called for a QML HyRemote application")
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
endfunction()

function(qt_generate_deploy_qml_app_script)
    set(options NO_UNSUPPORTED_PLATFORM_ERROR DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM)
    set(oneValueArgs TARGET OUTPUT_SCRIPT)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "" ${ARGN})

    if(NOT TEST_QML)
        message(FATAL_ERROR "QML Qt deploy command called for a non-QML HyRemote application")
    endif()
    if(NOT ARG_TARGET STREQUAL "deploy-probe")
        message(FATAL_ERROR "unexpected deployment target: ${ARG_TARGET}")
    endif()
    if(NOT ARG_NO_UNSUPPORTED_PLATFORM_ERROR)
        message(FATAL_ERROR "HyRemote must preserve NO_UNSUPPORTED_PLATFORM_ERROR")
    endif()
    if(NOT ARG_DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM)
        message(FATAL_ERROR "HyRemote must preserve user QML module deployment on unsupported hosts")
    endif()

    set(script "${CMAKE_CURRENT_BINARY_DIR}/qt-qml-deploy.cmake")
    file(WRITE "${script}" "# QML deployment probe\n")
    set(${ARG_OUTPUT_SCRIPT} "${script}" PARENT_SCOPE)
endfunction()

include("${HYREMOTE_SOURCE_DIR}/cmake/HyRemoteDeploy.cmake")

if(TEST_QML)
    hyremote_deploy(TARGET deploy-probe QML)
else()
    hyremote_deploy(TARGET deploy-probe)
endif()
