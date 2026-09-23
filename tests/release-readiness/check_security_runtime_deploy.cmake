# Structural guard for the transport security runtime's deployment closure (#312).
#
# The deployed tree must carry the OpenSSL runtime the shared runtime actually loads, and it must not carry one where
# the platform owns it. Both halves are decidable from the deploy helper alone, so they are decided here rather than on
# a runner.
#
# Cases:
#   A. SYSTEM  with an empty payload adds nothing and does not fail.
#   B. BUNDLED with an empty payload fails.
#   C. BUNDLED with two files emits each of them exactly once.
#   D. A build without the capability adds nothing and requires nothing.
#
# B is the one case that must fail, so it runs in a child process where the failure is observable.

include_guard(GLOBAL)

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

set(deploy_helper "${HYREMOTE_SOURCE_DIR}/cmake/HyRemoteDeploy.cmake")
if(NOT EXISTS "${deploy_helper}")
    message(FATAL_ERROR "security-runtime-deploy: ${deploy_helper} is missing")
endif()

if(DEFINED HYREMOTE_DEPLOY_CASE AND HYREMOTE_DEPLOY_CASE STREQUAL "BUNDLED_EMPTY")
    include("${deploy_helper}")
    set(HYREMOTE_PACKAGE_WITH_SECURITY_RUNTIME TRUE)
    set(HYREMOTE_PACKAGE_SECURITY_RUNTIME_MODE "BUNDLED")
    set(HYREMOTE_PACKAGE_SECURITY_RUNTIME_SOURCE_FILES "")
    _hyremote_security_runtime_deploy_fragment("bin" _unused_payload)
    message(FATAL_ERROR "security-runtime-deploy/BUNDLED_EMPTY: expected a failure and none was raised")
endif()

include("${deploy_helper}")

# A macro, not a function: a function would set these in its own scope and the helper would be asked about facts that
# were never applied.
macro(_reset_runtime_facts mode with_runtime sources)
    set(HYREMOTE_PACKAGE_WITH_SECURITY_RUNTIME "${with_runtime}")
    set(HYREMOTE_PACKAGE_SECURITY_RUNTIME_MODE "${mode}")
    set(HYREMOTE_PACKAGE_SECURITY_RUNTIME_SOURCE_FILES "${sources}")
    unset(HyRemote_SECURITY_RUNTIME_MODE)
    unset(HyRemote_SECURITY_RUNTIME_FILES)
    unset(HyRemote_SECURITY_RUNTIME_DIR)
endmacro()

# Two real files, so the existence check the helper performs is exercised rather than bypassed.
set(payload_dir "${CMAKE_CURRENT_BINARY_DIR}/hyremote-security-runtime-payload")
file(MAKE_DIRECTORY "${payload_dir}")
set(payload_ssl "${payload_dir}/libssl-3-x64.dll")
set(payload_crypto "${payload_dir}/libcrypto-3-x64.dll")
file(WRITE "${payload_ssl}" "ssl")
file(WRITE "${payload_crypto}" "crypto")

# A. Linux: the platform owns the runtime, an empty payload is correct, and nothing is emitted.
_reset_runtime_facts("SYSTEM" FALSE "")
_hyremote_security_runtime_deploy_fragment("bin" _system_fragment)
if(NOT _system_fragment STREQUAL "")
    message(FATAL_ERROR "security-runtime-deploy/SYSTEM: expected no payload, got '${_system_fragment}'")
endif()

# B. A bundled runtime without its files is a broken deployment and must fail.
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DHYREMOTE_SOURCE_DIR=${HYREMOTE_SOURCE_DIR}"
            "-DHYREMOTE_DEPLOY_CASE=BUNDLED_EMPTY" -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE bundled_empty_rc
    OUTPUT_VARIABLE bundled_empty_out
    ERROR_VARIABLE bundled_empty_err)
if(bundled_empty_rc EQUAL 0)
    message(FATAL_ERROR
        "security-runtime-deploy/BUNDLED_EMPTY: a bundled runtime with no payload was accepted\n${bundled_empty_out}${bundled_empty_err}")
endif()

# C. Windows: a two-entry payload emits each runtime exactly once - the case a directory joined to a semicolon list
#    could not describe at all.
_reset_runtime_facts("BUNDLED" TRUE "${payload_ssl};${payload_crypto}")
_hyremote_security_runtime_deploy_fragment("bin" _bundled_fragment)
foreach(needed IN ITEMS "libssl-3-x64.dll" "libcrypto-3-x64.dll")
    string(REGEX MATCHALL "${needed}" occurrences "${_bundled_fragment}")
    list(LENGTH occurrences count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR
            "security-runtime-deploy/BUNDLED: expected ${needed} exactly once, saw ${count}\n${_bundled_fragment}")
    endif()
endforeach()
string(FIND "${_bundled_fragment}" "TYPE FILE FILES" file_install)
if(file_install EQUAL -1)
    message(FATAL_ERROR "security-runtime-deploy/BUNDLED: the fragment is not a file(INSTALL) fragment")
endif()

# D. A build without the capability adds nothing and requires nothing.
foreach(mode IN ITEMS "NONE" "STATIC")
    _reset_runtime_facts("${mode}" FALSE "")
    _hyremote_security_runtime_deploy_fragment("bin" _no_payload)
    if(NOT _no_payload STREQUAL "")
        message(FATAL_ERROR "security-runtime-deploy/${mode}: expected no payload, got '${_no_payload}'")
    endif()
endforeach()

message(STATUS "security-runtime-deploy: PASS")
