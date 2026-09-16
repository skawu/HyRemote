include_guard(GLOBAL)

# Generate the supplemental install-time script for the Transparent QPA package. The application
# does not link the proxy module; the installed SDK exports HyRemote::QpaPlatform only so this
# helper can locate the exact module file that was qualified with the SDK.
function(_hyremote_generate_qpa_deploy_script target output_var)
    if(NOT TARGET HyRemote::QpaPlatform)
        message(FATAL_ERROR
            "hyremote_deploy(TARGET ${target} QPA) requires an installed/source HyRemote SDK built with HYREMOTE_WITH_QPA_PROXY=ON")
    endif()

    if(APPLE OR (NOT WIN32 AND NOT UNIX))
        message(FATAL_ERROR
            "hyremote_deploy(TARGET ${target} QPA) is supported only for the V1 Windows/Linux reference platforms")
    endif()

    set(_required_qt_version "${HyRemote_QPA_QT_VERSION}")
    if(_required_qt_version STREQUAL "")
        # Source-tree use reaches this helper before installed package metadata exists. The QPA
        # target itself is hard-gated to the same exact version in the root build.
        set(_required_qt_version "6.8.3")
    endif()

    if(DEFINED Qt6Core_VERSION)
        set(_consumer_qt_version "${Qt6Core_VERSION}")
    elseif(DEFINED Qt6_VERSION)
        set(_consumer_qt_version "${Qt6_VERSION}")
    else()
        message(FATAL_ERROR
            "hyremote_deploy(TARGET ${target} QPA) cannot determine the consumer Qt version; find Qt6 Core before deployment")
    endif()

    if(NOT _consumer_qt_version VERSION_EQUAL _required_qt_version)
        message(FATAL_ERROR
            "hyremote_deploy(TARGET ${target} QPA) requires exact Qt ${_required_qt_version}; consumer resolved Qt ${_consumer_qt_version}")
    endif()

    if(NOT DEFINED QT_DEPLOY_SUPPORT OR "${QT_DEPLOY_SUPPORT}" STREQUAL "")
        message(FATAL_ERROR
            "hyremote_deploy(TARGET ${target} QPA) requires Qt's deployment support script from Qt6 Core")
    endif()

    set(_runtime_copy_commands "")
    set(_additional_library_args "")
    set(_additional_library_section "")

    if(WIN32)
        set(_runtime_deploy_dir "\${QT_DEPLOY_BIN_DIR}")
    else()
        set(_runtime_deploy_dir "\${QT_DEPLOY_LIB_DIR}")
    endif()

    if(HyRemote_QPA_SHARED_RUNTIME)
        foreach(_runtime_target IN ITEMS HyRemote::RemoteAccess HyRemote::Core)
            if(NOT TARGET ${_runtime_target})
                message(FATAL_ERROR
                    "hyremote_deploy(TARGET ${target} QPA) expected shared runtime target ${_runtime_target} in this SDK")
            endif()

            string(APPEND _runtime_copy_commands
                "file(INSTALL DESTINATION \"\${QT_DEPLOY_PREFIX}/${_runtime_deploy_dir}\" TYPE FILE FILES \"$<TARGET_FILE:${_runtime_target}>\")\n")
            string(APPEND _additional_library_args
                "\n        \"${_runtime_deploy_dir}/$<TARGET_FILE_NAME:${_runtime_target}>\"")
        endforeach()
        set(_additional_library_section
            "\n    ADDITIONAL_LIBRARIES${_additional_library_args}")
    endif()

    # Qt's high-level deployment command owns the application's normal Qt/native-platform payload.
    # This second bounded script adds only HyRemote's project-owned platform module and asks Qt's
    # low-level deploy API to resolve dependencies of that additional module. ADDITIONAL_MODULES is
    # specifically intended for plugins not linked by the executable.
    set(_qpa_script "${CMAKE_CURRENT_BINARY_DIR}/hyremote-qpa-deploy-${target}-$<CONFIG>.cmake")
    file(GENERATE
        OUTPUT "${_qpa_script}"
        CONTENT
"include(\"${QT_DEPLOY_SUPPORT}\")
file(INSTALL DESTINATION \"\${QT_DEPLOY_PREFIX}/\${QT_DEPLOY_PLUGINS_DIR}/platforms\" TYPE FILE FILES \"$<TARGET_FILE:HyRemote::QpaPlatform>\")
${_runtime_copy_commands}qt_deploy_runtime_dependencies(
    EXECUTABLE \"\${QT_DEPLOY_BIN_DIR}/$<TARGET_FILE_NAME:${target}>\"
    ADDITIONAL_MODULES \"\${QT_DEPLOY_PLUGINS_DIR}/platforms/$<TARGET_FILE_NAME:HyRemote::QpaPlatform>\"${_additional_library_section}
)
")

    set(${output_var} "${_qpa_script}" PARENT_SCOPE)
endfunction()

# Public deployment entry point installed with the HyRemote CMake package.
#
# HyRemote owns one application-facing deployment hook. QML and QPA are orthogonal options: a QML
# application may also use Transparent QPA without inventing a fourth deployment API. The ordinary
# mode remains unchanged when neither option is requested.
#
# Usage:
#   hyremote_deploy(TARGET MyWidgetsApp)
#   hyremote_deploy(TARGET MyQmlApp QML)
#   hyremote_deploy(TARGET ExistingQtApp QPA)
#   hyremote_deploy(TARGET ExistingQmlApp QML QPA)
function(hyremote_deploy)
    set(options QML QPA)
    set(oneValueArgs TARGET)
    set(multiValueArgs)
    cmake_parse_arguments(HYREMOTE_DEPLOY "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(HYREMOTE_DEPLOY_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "hyremote_deploy: unknown arguments: ${HYREMOTE_DEPLOY_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT HYREMOTE_DEPLOY_TARGET)
        message(FATAL_ERROR "hyremote_deploy(TARGET <application> [QML] [QPA]) requires TARGET")
    endif()
    if(NOT TARGET "${HYREMOTE_DEPLOY_TARGET}")
        message(FATAL_ERROR "hyremote_deploy: '${HYREMOTE_DEPLOY_TARGET}' is not a CMake target")
    endif()

    if(HYREMOTE_DEPLOY_QPA)
        if(DEFINED HyRemote_QPA_AVAILABLE AND NOT HyRemote_QPA_AVAILABLE)
            message(FATAL_ERROR
                "hyremote_deploy(TARGET ${HYREMOTE_DEPLOY_TARGET} QPA) requested an SDK that was built without Transparent QPA")
        endif()
        _hyremote_generate_qpa_deploy_script(
            "${HYREMOTE_DEPLOY_TARGET}" _hyremote_qpa_deploy_script)
    endif()

    if(HYREMOTE_DEPLOY_QML)
        if(NOT COMMAND qt_generate_deploy_qml_app_script)
            message(FATAL_ERROR
                "hyremote_deploy(TARGET ${HYREMOTE_DEPLOY_TARGET} QML) requires Qt's "
                "qt_generate_deploy_qml_app_script(). Find Qt6 Qml before calling the helper.")
        endif()

        # An installed HyRemote package publishes the root containing its `HyRemote/qmldir` as
        # HyRemote_QML_IMPORT_PATH. Qt 6.8's QML tooling reads the target's QT_QML_IMPORT_PATH when
        # qmlimportscanner prepares deployment metadata. APPEND preserves caller-owned paths.
        if(DEFINED HyRemote_QML_IMPORT_PATH AND NOT "${HyRemote_QML_IMPORT_PATH}" STREQUAL "")
            if(NOT IS_ABSOLUTE "${HyRemote_QML_IMPORT_PATH}")
                message(FATAL_ERROR
                    "hyremote_deploy: HyRemote_QML_IMPORT_PATH must be an absolute installed QML import root")
            endif()
            set_property(TARGET "${HYREMOTE_DEPLOY_TARGET}" APPEND PROPERTY
                QT_QML_IMPORT_PATH "${HyRemote_QML_IMPORT_PATH}")
        endif()

        qt_generate_deploy_qml_app_script(
            TARGET "${HYREMOTE_DEPLOY_TARGET}"
            OUTPUT_SCRIPT _hyremote_qt_deploy_script
            NO_UNSUPPORTED_PLATFORM_ERROR
            DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM
        )
        install(SCRIPT "${_hyremote_qt_deploy_script}")
    else()
        if(COMMAND qt_generate_deploy_app_script)
            qt_generate_deploy_app_script(
                TARGET "${HYREMOTE_DEPLOY_TARGET}"
                OUTPUT_SCRIPT _hyremote_qt_deploy_script
                NO_UNSUPPORTED_PLATFORM_ERROR
            )
            install(SCRIPT "${_hyremote_qt_deploy_script}")
        elseif(HYREMOTE_DEPLOY_QPA)
            message(FATAL_ERROR
                "hyremote_deploy(TARGET ${HYREMOTE_DEPLOY_TARGET} QPA) requires Qt's qt_generate_deploy_app_script()")
        else()
            message(STATUS
                "hyremote_deploy(${HYREMOTE_DEPLOY_TARGET}): Qt deployment helper unavailable; "
                "HyRemote has no extra runtime payload in this build")
        endif()
    endif()

    # Run after Qt's normal application/QML deployment so the native qwindows/qxcb delegate and
    # standard Qt runtime layout are already established. No QT_PLUGIN_PATH is needed afterwards.
    if(HYREMOTE_DEPLOY_QPA)
        install(SCRIPT "${_hyremote_qpa_deploy_script}")
    endif()
endfunction()
