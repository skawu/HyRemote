cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED GROUP OR NOT DEFINED QT_PLUGIN_PATH)
    message(FATAL_ERROR "GROUP and QT_PLUGIN_PATH are required")
endif()

# Each case is the real QPA smoke executable with its original platform/plugin
# environment. Grouping only removes CTest process bookkeeping; it does not
# merge the fixtures or weaken their assertions.
function(run_case name executable)
    if(NOT executable)
        message(STATUS "QPA runtime case SKIP: ${name} (target unavailable)")
        return()
    endif()
    set(env_args ${ARGN})
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env ${env_args} "${executable}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR
            "QPA runtime case '${name}' failed (${result}):\n${output}\n${error}")
    endif()
    message(STATUS "QPA runtime case PASS: ${name}")
endfunction()

if(GROUP STREQUAL "basic" OR GROUP STREQUAL "all")
    run_case(proxy-smoke "${PROXY_EXE}"
        "QT_QPA_PLATFORM=hyremote"
        "QT_PLUGIN_PATH=${QT_PLUGIN_PATH}"
    )
    run_case(native-semantics "${NATIVE_EXE}"
        "QT_QPA_PLATFORM=hyremote"
        "QT_PLUGIN_PATH=${QT_PLUGIN_PATH}"
    )
    run_case(remote-config "${REMOTE_CONFIG_EXE}")
    run_case(widget-opengl-capture "${OPENGL_EXE}"
        "QT_QPA_PLATFORM=hyremote:hyremote-port=5994"
        "QT_PLUGIN_PATH=${QT_PLUGIN_PATH}"
    )
endif()
if(GROUP STREQUAL "surfaces" OR GROUP STREQUAL "all")
    run_case(multi-surface-connection "${MULTI_EXE}"
        "QT_QPA_PLATFORM=hyremote:hyremote-port=5997"
        "QT_PLUGIN_PATH=${QT_PLUGIN_PATH}"
    )
    run_case(widget-popup-connection "${POPUP_EXE}"
        "QT_QPA_PLATFORM=hyremote:hyremote-port=5996"
        "QT_PLUGIN_PATH=${QT_PLUGIN_PATH}"
    )
    run_case(quick-multi-window-connection "${QUICK_MULTI_EXE}"
        "QT_QPA_PLATFORM=hyremote:hyremote-port=5995"
        "QT_PLUGIN_PATH=${QT_PLUGIN_PATH}"
        "QT_QUICK_BACKEND=software"
    )
endif()
if(GROUP STREQUAL "recovery" OR GROUP STREQUAL "all")
    run_case(auto-remoteaccess "${AUTO_EXE}"
        "QT_QPA_PLATFORM=hyremote:hyremote-port=5998"
        "QT_PLUGIN_PATH=${QT_PLUGIN_PATH}"
    )
    run_case(remote-failure-native-survival "${FAILURE_EXE}"
        "QT_PLUGIN_PATH=${QT_PLUGIN_PATH}"
    )
endif()
if(NOT GROUP MATCHES "^(basic|surfaces|recovery|all)$")
    message(FATAL_ERROR "unknown QPA runtime group: ${GROUP}")
endif()
