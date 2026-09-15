include_guard(GLOBAL)

# Public deployment entry point installed with the HyRemote CMake package.
#
# V0.0.1.0 currently has no independently deployed protocol runtime yet. The helper therefore
# delegates Qt runtime deployment to Qt's own supported CMake deployment API and establishes the
# stable HyRemote hook where transport/plugin payloads will be added when #27 lands. Applications
# should call this helper rather than learning backend-specific DLL/SO names.
function(hyremote_deploy)
    set(options)
    set(oneValueArgs TARGET)
    set(multiValueArgs)
    cmake_parse_arguments(HYREMOTE_DEPLOY "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT HYREMOTE_DEPLOY_TARGET)
        message(FATAL_ERROR "hyremote_deploy(TARGET <application>) requires TARGET")
    endif()
    if(NOT TARGET "${HYREMOTE_DEPLOY_TARGET}")
        message(FATAL_ERROR "hyremote_deploy: '${HYREMOTE_DEPLOY_TARGET}' is not a CMake target")
    endif()

    if(COMMAND qt_generate_deploy_app_script)
        set(_hyremote_deploy_script
            "${CMAKE_CURRENT_BINARY_DIR}/hyremote-deploy-${HYREMOTE_DEPLOY_TARGET}.cmake")
        qt_generate_deploy_app_script(
            TARGET "${HYREMOTE_DEPLOY_TARGET}"
            OUTPUT_SCRIPT _hyremote_qt_deploy_script
            NO_UNSUPPORTED_PLATFORM_ERROR
        )
        install(SCRIPT "${_hyremote_qt_deploy_script}")
    else()
        message(STATUS
            "hyremote_deploy(${HYREMOTE_DEPLOY_TARGET}): Qt deployment helper unavailable; "
            "HyRemote has no extra runtime payload in this build")
    endif()
endfunction()
