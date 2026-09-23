include_guard(GLOBAL)

# Phase-C root test metadata must not depend on Core being enabled. This module is loaded by the
# top-level project immediately after project options, so schedule the root-directory callback here;
# the deferred call runs after the repository/release tests have been registered.
if(HYREMOTE_BUILD_TESTS AND PROJECT_IS_TOP_LEVEL)
    include("${CMAKE_CURRENT_LIST_DIR}/../tests/semantic_ctest_labels.cmake")
    cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL hyremote_apply_root_semantic_test_labels)
endif()

# Validate release-version authority without encoding integration frontends into version digits.
#
# 0.0.0 is the development/integration sentinel. V1.0.0.0 is the first formal HyRemote GA.
# Historical V0.0.1.0/V0.0.2.0/V0.0.3.0 labels were planning references only and are not
# releasable product profiles. C++/QML/Generic/QPA are peer capability dimensions of one product.
function(hyremote_validate_release_profile)
    set(one_value_args VERSION CPP_ENABLED QML_ENABLED GENERIC_ENABLED QPA_ENABLED SECURITY_ENABLED)
    cmake_parse_arguments(HYREMOTE_PROFILE "" "${one_value_args}" "" ${ARGN})

    if(NOT DEFINED HYREMOTE_PROFILE_VERSION OR HYREMOTE_PROFILE_VERSION STREQUAL "")
        message(FATAL_ERROR "hyremote_validate_release_profile requires VERSION")
    endif()

    # Development/integration sentinel: any frontend/capability combination may be exercised while
    # the coherent V1 product is being assembled and qualified.
    if(HYREMOTE_PROFILE_VERSION STREQUAL "0.0.0")
        return()
    endif()

    # #24 retired the sequential V0.0.x frontend labels. Only that retired family is rejected: the planning
    # labels V0.0.1.0/V0.0.2.0/V0.0.3.0 must never become a release branch, tag or architecture gate again.
    if(HYREMOTE_PROFILE_VERSION VERSION_LESS "0.1.0.0")
        message(FATAL_ERROR
            "HyRemote ${HYREMOTE_PROFILE_VERSION} is a retired pre-GA planning label, not a releasable product profile. "
            "Integration frontends are capability dimensions, not version slots.")
    endif()

    # The progressive trains V0.1 -> V0.2 -> V0.3 -> V0.4 -> V1.0 are real releases and are accepted here without
    # regard to which frontends a build happens to enable; V0.4.0.x maintenance releases are inside the V0.4 line.
    #
    # This function validates that the value is a releasable profile, and nothing more. It is deliberately not a
    # release-authorization engine: whether a train may actually be released is decided by #1 (roadmap), #24 (version
    # semantics), #95 (release trains) and that train's own readiness scope, and frontend enablement, support level
    # and preview/qualified/supported status are release-train scope and compatibility authority - never version
    # digits. Applicability continues to be recorded by the compatibility/capability matrix, where QPA remains
    # exact-private-ABI qualified.
endfunction()
