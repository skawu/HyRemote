include_guard(GLOBAL)

# Validate which integration modes a formal HyRemote version is allowed to expose.
#
# The development sentinel 0.0.0 intentionally permits in-flight integration work so one convergence
# branch can qualify the complete product graph. Formal versions are release facts: source for a later
# mode may already exist, but an earlier tagged product must reject that mode until its release
# milestone is explicitly assigned.
#
# This is an internal build rule, not another application-facing product/profile option.
function(hyremote_validate_release_profile)
    set(one_value_args VERSION QML_ENABLED GENERIC_ENABLED QPA_ENABLED SECURITY_ENABLED)
    cmake_parse_arguments(HYREMOTE_PROFILE "" "${one_value_args}" "" ${ARGN})

    if(NOT DEFINED HYREMOTE_PROFILE_VERSION OR HYREMOTE_PROFILE_VERSION STREQUAL "")
        message(FATAL_ERROR "hyremote_validate_release_profile requires VERSION")
    endif()
    if(NOT DEFINED HYREMOTE_PROFILE_QML_ENABLED)
        message(FATAL_ERROR "hyremote_validate_release_profile requires QML_ENABLED")
    endif()
    # Keep old release-profile fixtures source-compatible while #219 introduces the fourth frontend;
    # callers that do not mention Generic are equivalent to GENERIC_ENABLED=OFF.
    if(NOT DEFINED HYREMOTE_PROFILE_GENERIC_ENABLED)
        set(HYREMOTE_PROFILE_GENERIC_ENABLED OFF)
    endif()
    if(NOT DEFINED HYREMOTE_PROFILE_QPA_ENABLED)
        message(FATAL_ERROR "hyremote_validate_release_profile requires QPA_ENABLED")
    endif()
    if(NOT DEFINED HYREMOTE_PROFILE_SECURITY_ENABLED)
        message(FATAL_ERROR "hyremote_validate_release_profile requires SECURITY_ENABLED")
    endif()

    # Development/integration sentinel: all in-flight frontends may coexist behind explicit options.
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

    # Generic Plugin is introduced by the post-V1 #219 architecture migration. Its first formal
    # release version has not been assigned yet, so no existing tagged/milestone profile may silently
    # acquire it. When product planning assigns that version, replace this fail-closed rule with the
    # corresponding VERSION_LESS threshold and add explicit release-profile fixtures.
    if(HYREMOTE_PROFILE_GENERIC_ENABLED)
        message(FATAL_ERROR
            "HyRemote ${HYREMOTE_PROFILE_VERSION} does not yet release the Generic Plugin integration mode. "
            "The #219 frontend is development-only until a formal release milestone is assigned.")
    endif()

    if(HYREMOTE_PROFILE_VERSION VERSION_LESS "1.0.0.0" AND HYREMOTE_PROFILE_SECURITY_ENABLED)
        message(FATAL_ERROR
            "HyRemote ${HYREMOTE_PROFILE_VERSION} does not release the authenticated/encrypted transport mode. "
            "Transport security becomes a released product surface at v1.0.0.0.")
    endif()
endfunction()
