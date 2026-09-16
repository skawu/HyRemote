cmake_minimum_required(VERSION 3.21)

if(NOT QPA_FILE OR NOT REMOTEACCESS_FILE OR NOT TEST_ROOT)
    message(FATAL_ERROR "QPA_FILE, REMOTEACCESS_FILE and TEST_ROOT are required")
endif()
if(NOT EXISTS "${QPA_FILE}")
    message(FATAL_ERROR "source qhyremote payload does not exist: ${QPA_FILE}")
endif()
if(NOT EXISTS "${REMOTEACCESS_FILE}")
    message(FATAL_ERROR "source RemoteAccess runtime does not exist: ${REMOTEACCESS_FILE}")
endif()

file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_ROOT}/plugins/platforms" "${TEST_ROOT}/lib")

get_filename_component(qpa_name "${QPA_FILE}" NAME)
get_filename_component(remoteaccess_name "${REMOTEACCESS_FILE}" NAME)
set(deployed_qpa "${TEST_ROOT}/plugins/platforms/${qpa_name}")
set(deployed_remoteaccess "${TEST_ROOT}/lib/${remoteaccess_name}")

file(COPY_FILE "${QPA_FILE}" "${deployed_qpa}")
file(COPY_FILE "${REMOTEACCESS_FILE}" "${deployed_remoteaccess}")

# Exercise the exact Linux relocation contract used by HyRemoteDeploy.cmake. The source-built module
# must contain this stable non-toolchain anchor even though CMake may append other build/toolchain
# entries. If the anchor disappears, RPATH_CHANGE itself fails and this test fails before GA.
file(RPATH_CHANGE
    FILE "${deployed_qpa}"
    OLD_RPATH "$ORIGIN/../../.."
    NEW_RPATH "$ORIGIN/../../lib"
)

# Resolve the copied plugin with no LD_LIBRARY_PATH assistance and prove that its product-runtime
# dependency comes from the temporary deployed tree, not from the source build. Qt dependencies may
# still use the reference SDK in this focused CTest; the clean integrated consumer separately proves
# the complete deployed Qt dependency envelope.
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env --unset=LD_LIBRARY_PATH ldd "${deployed_qpa}"
    RESULT_VARIABLE ldd_result
    OUTPUT_VARIABLE ldd_output
    ERROR_VARIABLE ldd_error
)
if(NOT ldd_result EQUAL 0)
    message(FATAL_ERROR
        "ldd failed after source qhyremote relocation (${ldd_result}):\n${ldd_output}\n${ldd_error}")
endif()
if(ldd_output MATCHES "not found")
    message(FATAL_ERROR
        "source qhyremote has an unresolved dependency after relocation:\n${ldd_output}")
endif()

file(REAL_PATH "${deployed_remoteaccess}" expected_remoteaccess)
string(REGEX MATCH
    "libHyRemoteRemoteAccess\\.so[^ ]*[ \\t]+=>[ \\t]+([^ \\t\\n]+)"
    remoteaccess_match
    "${ldd_output}")
if(NOT remoteaccess_match)
    message(FATAL_ERROR
        "relocated qhyremote did not report its shared RemoteAccess dependency:\n${ldd_output}")
endif()
file(REAL_PATH "${CMAKE_MATCH_1}" resolved_remoteaccess)
if(NOT resolved_remoteaccess STREQUAL expected_remoteaccess)
    message(FATAL_ERROR
        "relocated qhyremote escaped deployment tree: expected ${expected_remoteaccess}, got ${resolved_remoteaccess}\n${ldd_output}")
endif()

message(STATUS
    "HyRemote source QPA payload relocation: PASS (${resolved_remoteaccess})")
