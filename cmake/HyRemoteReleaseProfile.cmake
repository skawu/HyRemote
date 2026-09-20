include_guard(GLOBAL)

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

    # #24 retired the sequential V0 frontend trains. Rejecting these values prevents an old planning
    # label from accidentally becoming a release branch/tag or an architecture gate again.
    if(HYREMOTE_PROFILE_VERSION VERSION_LESS "1.0.0.0")
        message(FATAL_ERROR
            "HyRemote ${HYREMOTE_PROFILE_VERSION} is a retired pre-GA planning label, not a releasable product profile. "
            "V1.0.0.0 is the first formal GA; integration frontends are capability dimensions, not version slots.")
    endif()

    # V1+ does not sequence cpp/qml/generic/qpa through VERSION_LESS thresholds. Applicability and
    # qualification are recorded by the compatibility/capability matrix (QPA remains exact-private-ABI
    # qualified), while the release version identifies the coherent product state.
endfunction()
