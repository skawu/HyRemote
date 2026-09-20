if(NOT DEFINED TEST_EXE OR TEST_EXE STREQUAL "")
    message(FATAL_ERROR "TEST_EXE is required")
endif()
if(NOT DEFINED MODULE_RUNTIME_DIR OR MODULE_RUNTIME_DIR STREQUAL "")
    message(FATAL_ERROR "MODULE_RUNTIME_DIR is required")
endif()

if(WIN32)
    set(_loader_value "${MODULE_RUNTIME_DIR};$ENV{PATH}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "PATH=${_loader_value}" "${TEST_EXE}"
        RESULT_VARIABLE _result
    )
elseif(APPLE)
    set(_loader_value "${MODULE_RUNTIME_DIR}:$ENV{DYLD_LIBRARY_PATH}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "DYLD_LIBRARY_PATH=${_loader_value}" "${TEST_EXE}"
        RESULT_VARIABLE _result
    )
else()
    set(_loader_value "${MODULE_RUNTIME_DIR}:$ENV{LD_LIBRARY_PATH}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "LD_LIBRARY_PATH=${_loader_value}" "${TEST_EXE}"
        RESULT_VARIABLE _result
    )
endif()

if(NOT _result EQUAL 0)
    message(FATAL_ERROR "hyremote-qml-module-test exited with ${_result}")
endif()
