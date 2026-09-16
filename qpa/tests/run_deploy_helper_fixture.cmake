if(NOT FIXTURE_SOURCE_DIR OR NOT FIXTURE_BINARY_DIR OR NOT HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "FIXTURE_SOURCE_DIR, FIXTURE_BINARY_DIR and HYREMOTE_SOURCE_DIR are required")
endif()

if(NOT DEFINED TEST_QML)
    set(TEST_QML OFF)
endif()
if(NOT DEFINED TEST_DEPLOY_QPA)
    set(TEST_DEPLOY_QPA ON)
endif()
if(NOT DEFINED TEST_QML_AVAILABLE)
    set(TEST_QML_AVAILABLE ON)
endif()
if(NOT DEFINED TEST_STALE_QML_METADATA)
    set(TEST_STALE_QML_METADATA OFF)
endif()
if(NOT DEFINED TEST_QPA_AVAILABLE)
    set(TEST_QPA_AVAILABLE ON)
endif()
if(NOT DEFINED TEST_STALE_QPA_METADATA)
    set(TEST_STALE_QPA_METADATA OFF)
endif()
if(NOT DEFINED TEST_STALE_QPA_NEGATIVE_METADATA)
    set(TEST_STALE_QPA_NEGATIVE_METADATA OFF)
endif()
if(NOT DEFINED TEST_QT_VERSION)
    set(TEST_QT_VERSION "6.8.3")
endif()
if(NOT DEFINED TEST_INSTALLED_PAYLOAD)
    set(TEST_INSTALLED_PAYLOAD OFF)
endif()
if(NOT DEFINED EXPECT_CONFIGURE_FAILURE)
    set(EXPECT_CONFIGURE_FAILURE OFF)
endif()

file(REMOVE_RECURSE "${FIXTURE_BINARY_DIR}")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -S "${FIXTURE_SOURCE_DIR}"
        -B "${FIXTURE_BINARY_DIR}"
        "-DHYREMOTE_SOURCE_DIR=${HYREMOTE_SOURCE_DIR}"
        "-DTEST_QML=${TEST_QML}"
        "-DTEST_DEPLOY_QPA=${TEST_DEPLOY_QPA}"
        "-DTEST_QML_AVAILABLE=${TEST_QML_AVAILABLE}"
        "-DTEST_STALE_QML_METADATA=${TEST_STALE_QML_METADATA}"
        "-DTEST_QPA_AVAILABLE=${TEST_QPA_AVAILABLE}"
        "-DTEST_STALE_QPA_METADATA=${TEST_STALE_QPA_METADATA}"
        "-DTEST_STALE_QPA_NEGATIVE_METADATA=${TEST_STALE_QPA_NEGATIVE_METADATA}"
        "-DTEST_QT_VERSION=${TEST_QT_VERSION}"
        "-DTEST_INSTALLED_PAYLOAD=${TEST_INSTALLED_PAYLOAD}"
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_stdout
    ERROR_VARIABLE configure_stderr
)

if(EXPECT_CONFIGURE_FAILURE)
    if(configure_result EQUAL 0)
        message(FATAL_ERROR
            "deploy fixture unexpectedly configured successfully\n${configure_stdout}\n${configure_stderr}")
    endif()
    return()
endif()

if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR
        "deploy fixture configuration failed\n${configure_stdout}\n${configure_stderr}")
endif()

if(TEST_DEPLOY_QPA)
    file(GLOB generated_scripts
        "${FIXTURE_BINARY_DIR}/hyremote-qpa-deploy-deploy-probe-*.cmake")
    set(_script_kind "QPA")
else()
    file(GLOB generated_scripts
        "${FIXTURE_BINARY_DIR}/hyremote-runtime-deploy-deploy-probe-*.cmake")
    set(_script_kind "runtime")
endif()

list(LENGTH generated_scripts generated_count)
if(NOT generated_count EQUAL 1)
    message(FATAL_ERROR
        "expected exactly one generated ${_script_kind} deploy script, found ${generated_count}: ${generated_scripts}")
endif()

list(GET generated_scripts 0 generated_script)
file(READ "${generated_script}" generated_content)

foreach(required_fragment IN ITEMS
        "qt_deploy_runtime_dependencies"
        "ADDITIONAL_LIBRARIES")
    string(FIND "${generated_content}" "${required_fragment}" fragment_pos)
    if(fragment_pos EQUAL -1)
        message(FATAL_ERROR
            "generated ${_script_kind} deploy script is missing '${required_fragment}':\n${generated_content}")
    endif()
endforeach()

if(TEST_INSTALLED_PAYLOAD)
    set(_expected_runtime "HyRemoteRemoteAccess")
    set(_forbidden_runtime "fake-remoteaccess")
else()
    set(_expected_runtime "fake-remoteaccess")
    set(_forbidden_runtime "HyRemoteRemoteAccess")
endif()
string(FIND "${generated_content}" "${_expected_runtime}" runtime_pos)
if(runtime_pos EQUAL -1)
    message(FATAL_ERROR
        "generated ${_script_kind} deploy script is missing expected runtime '${_expected_runtime}':\n${generated_content}")
endif()
string(FIND "${generated_content}" "${_forbidden_runtime}" wrong_runtime_pos)
if(NOT wrong_runtime_pos EQUAL -1)
    message(FATAL_ERROR
        "generated ${_script_kind} deploy script mixed source/installed runtime identity '${_forbidden_runtime}':\n${generated_content}")
endif()

if(TEST_DEPLOY_QPA)
    foreach(required_fragment IN ITEMS
            "ADDITIONAL_MODULES"
            "platforms")
        string(FIND "${generated_content}" "${required_fragment}" fragment_pos)
        if(fragment_pos EQUAL -1)
            message(FATAL_ERROR
                "generated QPA deploy script is missing '${required_fragment}':\n${generated_content}")
        endif()
    endforeach()

    if(TEST_INSTALLED_PAYLOAD)
        string(FIND "${generated_content}" "qhyremote${CMAKE_SHARED_MODULE_SUFFIX}" installed_payload_pos)
        if(installed_payload_pos EQUAL -1)
            message(FATAL_ERROR
                "installed-payload QPA deployment did not use published plugin file:\n${generated_content}")
        endif()
    endif()
else()
    foreach(forbidden_fragment IN ITEMS
            "ADDITIONAL_MODULES"
            "qhyremote"
            "RPATH_CHANGE")
        string(FIND "${generated_content}" "${forbidden_fragment}" forbidden_pos)
        if(NOT forbidden_pos EQUAL -1)
            message(FATAL_ERROR
                "QML-only runtime deploy script unexpectedly contains '${forbidden_fragment}':\n${generated_content}")
        endif()
    endforeach()
endif()

string(FIND "${generated_content}" "fake-core" fake_core_pos)
if(NOT fake_core_pos EQUAL -1)
    message(FATAL_ERROR
        "deployment unexpectedly exposes a separate Core runtime:\n${generated_content}")
endif()

string(FIND "${generated_content}" "QT_PLUGIN_PATH" plugin_path_pos)
if(NOT plugin_path_pos EQUAL -1)
    message(FATAL_ERROR
        "normal deployment must not require QT_PLUGIN_PATH: ${generated_content}")
endif()

if(TEST_DEPLOY_QPA AND UNIX AND NOT APPLE)
    set(_literal_deploy_lib_dir "$ORIGIN/../../\${QT_DEPLOY_LIB_DIR}")
    foreach(rpath_fragment IN ITEMS
            "RPATH_CHANGE"
            "$ORIGIN/../../../."
            "${_literal_deploy_lib_dir}")
        string(FIND "${generated_content}" "${rpath_fragment}" rpath_pos)
        if(rpath_pos EQUAL -1)
            message(FATAL_ERROR
                "shared-facade QPA deployment script is missing RPATH relocation '${rpath_fragment}':\n${generated_content}")
        endif()
    endforeach()
endif()
