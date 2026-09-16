include_guard(GLOBAL)

# Validate which integration modes a formal HyRemote version is allowed to expose.
#
# The development sentinel 0.0.0 intentionally permits all in-flight V1 modes so the single
# convergence branch can integrate and test them together. Formal milestone versions are release
# facts: source for a later mode may already exist, but the earlier tagged product must reject that
# mode until its own milestone is reached.
#
# This is an internal build rule, not another application-facing product/profile option.
function(hyremote_validate_release_profile)
    set(one_value_args VERSION QML_ENABLED QPA_ENABLED)
    cmake_parse_arguments(HYREMOTE_PROFILE "" "${one_value_args}" "" ${ARGN})

    if(NOT DEFINED HYREMOTE_PROFILE_VERSION OR HYREMOTE_PROFILE_VERSION STREQUAL "")
        message(FATAL_ERROR "hyremote_validate_release_profile requires VERSION")
    endif()
    if(NOT DEFINED HYREMOTE_PROFILE_QML_ENABLED)
        message(FATAL_ERROR "hyremote_validate_release_profile requires QML_ENABLED")
    endif()
    if(NOT DEFINED HYREMOTE_PROFILE_QPA_ENABLED)
        message(FATAL_ERROR "hyremote_validate_release_profile requires QPA_ENABLED")
    endif()

    # develop/integration sentinel: all future V1 modes may coexist behind explicit mode options.
    if(HYREMOTE_PROFILE_VERSION STREQUAL "0.0.0")
        return()
    endif()

    if(HYREMOTE_PROFILE_VERSION VERSION_LESS "0.0.2.0" AND HYREMOTE_PROFILE_QML_ENABLED)
        message(FATAL_ERROR
            "HyRemote ${HYREMOTE_PROFILE_VERSION} does not release the Declarative QML integration mode. "
            "QML becomes a released product surface at v0.0.2.0.")
    endif()

    if(HYREMOTE_PROFILE_VERSION VERSION_LESS "0.0.3.0" AND HYREMOTE_PROFILE_QPA_ENABLED)
        message(FATAL_ERROR
            "HyRemote ${HYREMOTE_PROFILE_VERSION} does not release the Transparent QPA integration mode. "
            "QPA becomes a released product surface at v0.0.3.0.")
    endif()
endfunction()
