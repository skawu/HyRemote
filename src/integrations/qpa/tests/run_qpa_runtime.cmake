cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED GROUP OR NOT DEFINED QT_PLUGIN_PATH)
    message(FATAL_ERROR "GROUP and QT_PLUGIN_PATH are required")
endif()

set(_qpa_runtime_cases_run 0)

# Each case is the real QPA smoke executable with its original platform/plugin
# environment. Grouping only removes CTest process bookkeeping; it does not
# merge the fixtures or weaken their assertions.
#
# A case whose executable was not passed is a *registration* defect, not a
# capability gap: CMake only adds a case it knows about, so an empty variable
# means the runner and the registration disagree. Treating it as a status
# message would let a group silently execute fewer cases than it claims - the
# group would stay green while a whole fixture never ran. Capability-conditional
# fixtures must say so explicitly by using run_optional_case.
function(run_case name executable)
    if(NOT executable)
        message(FATAL_ERROR
            "QPA runtime case '${name}' has no executable: the registration did not pass one. "
            "Pass the target's path, or use run_optional_case if the fixture is genuinely optional.")
    endif()
    _run_qpa_runtime_executable("${name}" "${executable}" ${ARGN})
    # Counted here, not in the helper: the helper's PARENT_SCOPE is this
    # function, and the zero-case guard below reads the file scope.
    math(EXPR ran "${_qpa_runtime_cases_run} + 1")
    set(_qpa_runtime_cases_run "${ran}" PARENT_SCOPE)
endfunction()

# Reserved for fixtures that depend on a Qt module the configuration may not
# have. The skip is announced, counted separately, and printed at the end so a
# reader can see that the group ran fewer cases than its siblings.
set(_qpa_runtime_cases_skipped 0)
function(run_optional_case name executable)
    if(NOT executable)
        message(STATUS "QPA runtime case SKIP: ${name} (fixture not built in this configuration)")
        math(EXPR skipped "${_qpa_runtime_cases_skipped} + 1")
        set(_qpa_runtime_cases_skipped "${skipped}" PARENT_SCOPE)
        return()
    endif()
    _run_qpa_runtime_executable("${name}" "${executable}" ${ARGN})
    math(EXPR ran "${_qpa_runtime_cases_run} + 1")
    set(_qpa_runtime_cases_run "${ran}" PARENT_SCOPE)
endfunction()

function(_run_qpa_runtime_executable name executable)
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
    math(EXPR ran "${_qpa_runtime_cases_run} + 1")
    set(_qpa_runtime_cases_run "${ran}" PARENT_SCOPE)
    message(STATUS "QPA runtime case PASS: ${name}")
endfunction()

if(GROUP STREQUAL "basic")
    run_case(proxy-smoke "${PROXY_EXE}"
        "QT_QPA_PLATFORM=hyremote"
        "QT_PLUGIN_PATH=${QT_PLUGIN_PATH}"
    )
    run_case(native-semantics "${NATIVE_EXE}"
        "QT_QPA_PLATFORM=hyremote"
        "QT_PLUGIN_PATH=${QT_PLUGIN_PATH}"
    )
    run_case(remote-config "${REMOTE_CONFIG_EXE}")
    # The OpenGL fixture is built only where the Qt OpenGL module exists.
    run_optional_case(widget-opengl-capture "${OPENGL_EXE}"
        "QT_QPA_PLATFORM=hyremote:hyremote-port=5994"
        "QT_PLUGIN_PATH=${QT_PLUGIN_PATH}"
    )
elseif(GROUP STREQUAL "surfaces")
    run_case(multi-surface-connection "${MULTI_EXE}"
        "QT_QPA_PLATFORM=hyremote:hyremote-port=5997"
        "QT_PLUGIN_PATH=${QT_PLUGIN_PATH}"
    )
    run_case(widget-popup-connection "${POPUP_EXE}"
        "QT_QPA_PLATFORM=hyremote:hyremote-port=5996"
        "QT_PLUGIN_PATH=${QT_PLUGIN_PATH}"
    )
    # The Quick multi-window fixture has its own CTest registration on its own
    # port, so it is deliberately not duplicated here. It used to be listed with
    # a variable the registration never passed, which meant this group silently
    # ran two cases while claiming three.
elseif(GROUP STREQUAL "recovery")
    run_case(auto-remoteaccess "${AUTO_EXE}"
        "QT_QPA_PLATFORM=hyremote:hyremote-port=5998"
        "QT_PLUGIN_PATH=${QT_PLUGIN_PATH}"
    )
    run_case(remote-failure-native-survival "${FAILURE_EXE}"
        "QT_PLUGIN_PATH=${QT_PLUGIN_PATH}"
    )
else()
    message(FATAL_ERROR "unknown QPA runtime group: ${GROUP}")
endif()

if(_qpa_runtime_cases_run EQUAL 0)
    message(FATAL_ERROR "QPA runtime group '${GROUP}' executed zero cases (false green)")
endif()
message(STATUS "QPA runtime group PASS: ${GROUP} (${_qpa_runtime_cases_run} cases, "
               "${_qpa_runtime_cases_skipped} skipped)")
