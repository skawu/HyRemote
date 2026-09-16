if(NOT FIXTURE_SOURCE_DIR OR NOT FIXTURE_BINARY_DIR OR NOT HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "FIXTURE_SOURCE_DIR, FIXTURE_BINARY_DIR and HYREMOTE_SOURCE_DIR are required")
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
        "-DTEST_QPA_AVAILABLE=${TEST_QPA_AVAILABLE}"
        "-DTEST_SHARED_RUNTIME=${TEST_SHARED_RUNTIME}"
        "-DTEST_QT_VERSION=${TEST_QT_VERSION}"
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_stdout
    ERROR_VARIABLE configure_stderr
)

if(EXPECT_CONFIGURE_FAILURE)
    if(configure_result EQUAL 0)
        message(FATAL_ERROR
            "QPA deploy fixture unexpectedly configured successfully\n${configure_stdout}\n${configure_stderr}")
    endif()
    return()
endif()

if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR
        "QPA deploy fixture configuration failed\n${configure_stdout}\n${configure_stderr}")
endif()

file(GLOB generated_scripts
    "${FIXTURE_BINARY_DIR}/hyremote-qpa-deploy-deploy-probe-*.cmake")
list(LENGTH generated_scripts generated_count)
if(NOT generated_count EQUAL 1)
    message(FATAL_ERROR
        "expected exactly one generated QPA deploy script, found ${generated_count}: ${generated_scripts}")
endif()

list(GET generated_scripts 0 generated_script)
file(READ "${generated_script}" generated_content)

foreach(required_fragment IN ITEMS
        "qt_deploy_runtime_dependencies"
        "ADDITIONAL_MODULES"
        "platforms")
    string(FIND "${generated_content}" "${required_fragment}" fragment_pos)
    if(fragment_pos EQUAL -1)
        message(FATAL_ERROR
            "generated QPA deploy script is missing '${required_fragment}':\n${generated_content}")
    endif()
endforeach()

string(FIND "${generated_content}" "QT_PLUGIN_PATH" plugin_path_pos)
if(NOT plugin_path_pos EQUAL -1)
    message(FATAL_ERROR
        "normal QPA deployment must not require QT_PLUGIN_PATH: ${generated_content}")
endif()

string(FIND "${generated_content}" "ADDITIONAL_LIBRARIES" additional_libraries_pos)
if(TEST_SHARED_RUNTIME)
    if(additional_libraries_pos EQUAL -1)
        message(FATAL_ERROR
            "shared QPA deployment must classify HyRemote runtime DLL/SO files as ADDITIONAL_LIBRARIES:\n${generated_content}")
    endif()
    foreach(runtime_fragment IN ITEMS "fake-remoteaccess" "fake-core")
        string(FIND "${generated_content}" "${runtime_fragment}" runtime_pos)
        if(runtime_pos EQUAL -1)
            message(FATAL_ERROR
                "shared QPA deployment script is missing ${runtime_fragment}:\n${generated_content}")
        endif()
    endforeach()

    if(UNIX AND NOT APPLE)
        set(_literal_deploy_lib_dir "$ORIGIN/../../\${QT_DEPLOY_LIB_DIR}")
        foreach(rpath_fragment IN ITEMS
                "RPATH_CHANGE"
                "$ORIGIN/../../.."
                "${_literal_deploy_lib_dir}")
            string(FIND "${generated_content}" "${rpath_fragment}" rpath_pos)
            if(rpath_pos EQUAL -1)
                message(FATAL_ERROR
                    "shared Linux QPA deployment script is missing RPATH relocation '${rpath_fragment}':\n${generated_content}")
            endif()
        endforeach()
    endif()
else()
    if(NOT additional_libraries_pos EQUAL -1)
        message(FATAL_ERROR
            "static/default QPA deployment unexpectedly emitted ADDITIONAL_LIBRARIES:\n${generated_content}")
    endif()
    string(FIND "${generated_content}" "RPATH_CHANGE" static_rpath_pos)
    if(NOT static_rpath_pos EQUAL -1)
        message(FATAL_ERROR
            "static/default QPA deployment unexpectedly emitted shared-runtime RPATH rewrite:\n${generated_content}")
    endif()
endif()
