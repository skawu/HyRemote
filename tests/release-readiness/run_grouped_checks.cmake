cmake_minimum_required(VERSION 3.21)

# TS-1 / MERGE_CASES (#393): all assertions still run. Function scope isolates
# each former script-mode CMake invocation's local variables and a FATAL_ERROR
# in any case propagates to the grouped CTest. No case may silently disappear.
if(NOT DEFINED HYREMOTE_SOURCE_DIR OR NOT IS_DIRECTORY "${HYREMOTE_SOURCE_DIR}")
    message(FATAL_ERROR "readiness group: HYREMOTE_SOURCE_DIR must be an existing directory")
endif()
if(HYREMOTE_READINESS_GROUP STREQUAL "facts-layout")
    set(_readiness_cases
        check_release_metadata.cmake
        check_compatibility_cells.cmake
        check_ga_scope_reconciliation.cmake
        check_v1_runtime_contract.cmake
        check_release_authority_policy.cmake
        check_repository_layout.cmake
        check_release_documentation_layout.cmake
        check_documentation_paths.cmake
        check_licensing_boundary.cmake
        check_ci_environment_baseline.cmake
    )
elseif(HYREMOTE_READINESS_GROUP STREQUAL "product-claims")
    set(_readiness_cases
        check_security_runtime_deploy.cmake
        check_build_install_contract.cmake
        check_deployment_relocation.cmake
        check_consumer_simplicity.cmake
        check_package_acquisition_isolation.cmake
    )
else()
    message(FATAL_ERROR "readiness group: unknown group '${HYREMOTE_READINESS_GROUP}'")
endif()
function(_hyremote_run_readiness_case case_file)
    set(_case_path "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/${case_file}")
    if(NOT EXISTS "${_case_path}")
        message(FATAL_ERROR "readiness group: missing case ${_case_path}")
    endif()
    message(STATUS "READINESS CASE BEGIN: ${case_file}")
    include("${_case_path}")
    message(STATUS "READINESS CASE PASS: ${case_file}")
endfunction()
list(LENGTH _readiness_cases _case_count)
if(_case_count EQUAL 0)
    message(FATAL_ERROR "readiness group: zero cases selected (false green)")
endif()
foreach(_case IN LISTS _readiness_cases)
    _hyremote_run_readiness_case("${_case}")
endforeach()
message(STATUS "READINESS GROUP PASS: ${HYREMOTE_READINESS_GROUP} (${_case_count} cases)")
