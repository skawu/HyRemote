if(NOT DEFINED TEST_EXE OR TEST_EXE STREQUAL "")
    message(FATAL_ERROR "TEST_EXE is required")
endif()
if(NOT DEFINED RUNTIME_DIR OR RUNTIME_DIR STREQUAL "")
    message(FATAL_ERROR "RUNTIME_DIR is required")
endif()
if(NOT DEFINED OUTPUT_FILE OR OUTPUT_FILE STREQUAL "")
    message(FATAL_ERROR "OUTPUT_FILE is required")
endif()

file(REMOVE "${OUTPUT_FILE}")

# The build-tree executable must find Qt/Runtime through its CMake build RPATH on Unix.
# Do not give it a loader-path repair that would be forbidden for deployed consumers.
# On Windows the build-tree DLL directory is needed, and the PATH assignment
# must stay quoted: inherited PATH contains semicolons.
if(WIN32)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
            "QT_QPA_PLATFORM=offscreen"
            "PATH=${RUNTIME_DIR};$ENV{PATH}"
            "${TEST_EXE}"
            --diagnostic-report-file "${OUTPUT_FILE}" --test-seconds 1
        RESULT_VARIABLE _result OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr
    )
else()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
            "QT_QPA_PLATFORM=offscreen"
            "${TEST_EXE}"
            --diagnostic-report-file "${OUTPUT_FILE}" --test-seconds 1
        RESULT_VARIABLE _result OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr
    )
endif()

if(NOT _result EQUAL 0)
    message(FATAL_ERROR
        "HyRemoteTool diagnostic export exited with ${_result}\nstdout:\n${_stdout}\nstderr:\n${_stderr}")
endif()

if(NOT EXISTS "${OUTPUT_FILE}")
    message(FATAL_ERROR
        "HyRemoteTool did not create diagnostic report\nstdout:\n${_stdout}\nstderr:\n${_stderr}")
endif()

file(READ "${OUTPUT_FILE}" _report)
foreach(_required IN ITEMS
        "INTEGRATION_ROUTE=cpp"
        "STATE=Stopped"
        "REMOTE_INPUT=false"
        "LISTENER_EFFECTIVE=none")
    string(FIND "${_report}" "${_required}" _hit)
    if(_hit EQUAL -1)
        message(FATAL_ERROR "saved diagnostic report is missing ${_required}:\n${_report}")
    endif()
endforeach()

string(FIND "${_stdout}" "DIAGNOSTIC_SAVED " _saved_marker)
if(_saved_marker EQUAL -1)
    message(FATAL_ERROR "HyRemoteTool did not confirm diagnostic export:\n${_stdout}")
endif()

foreach(_required_activity IN ITEMS
        "TOOL_ACTIVITY Tool ready"
        "TOOL_ACTIVITY Runtime state: Stopped"
        "TOOL_ACTIVITY Remote input: disabled")
    string(FIND "${_stdout}" "${_required_activity}" _activity_hit)
    if(_activity_hit EQUAL -1)
        message(FATAL_ERROR "HyRemoteTool activity log is missing ${_required_activity}:\n${_stdout}")
    endif()
endforeach()
