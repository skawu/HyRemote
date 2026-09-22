if(NOT DEFINED TEST_EXE OR TEST_EXE STREQUAL "")
    message(FATAL_ERROR "TEST_EXE is required")
endif()
if(NOT DEFINED MODULE_RUNTIME_DIR OR MODULE_RUNTIME_DIR STREQUAL "")
    message(FATAL_ERROR "MODULE_RUNTIME_DIR is required")
endif()

# A test that links the shared runtime directly (rather than only loading the QML module) also needs the
# runtime's own directory on the loader path, because a Windows executable has no rpath.
if(DEFINED ADDITIONAL_RUNTIME_DIR AND NOT ADDITIONAL_RUNTIME_DIR STREQUAL "")
    set(_loader_prefix "${MODULE_RUNTIME_DIR};${ADDITIONAL_RUNTIME_DIR}")
else()
    set(_loader_prefix "${MODULE_RUNTIME_DIR}")
endif()

if(WIN32)
    set(_loader_value "${_loader_prefix};$ENV{PATH}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "PATH=${_loader_value}" "${TEST_EXE}"
        RESULT_VARIABLE _result
    )
elseif(APPLE)
    set(_loader_value "${_loader_prefix}:$ENV{DYLD_LIBRARY_PATH}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "DYLD_LIBRARY_PATH=${_loader_value}" "${TEST_EXE}"
        RESULT_VARIABLE _result
    )
else()
    string(REPLACE ";" ":" _loader_prefix "${_loader_prefix}")
    set(_loader_value "${_loader_prefix}:$ENV{LD_LIBRARY_PATH}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "LD_LIBRARY_PATH=${_loader_value}" "${TEST_EXE}"
        RESULT_VARIABLE _result
    )
endif()

if(NOT _result EQUAL 0)
    message(FATAL_ERROR "${TEST_EXE} exited with ${_result}")
endif()
