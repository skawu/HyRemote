include_guard(GLOBAL)

function(_hyremote_runtime_deploy_dir output_var)
    if(WIN32)
        set(${output_var} "\${QT_DEPLOY_BIN_DIR}" PARENT_SCOPE)
    else()
        set(${output_var} "\${QT_DEPLOY_LIB_DIR}" PARENT_SCOPE)
    endif()
endfunction()

# Generate the supplemental deployment script for the normal C++/QML product path. The V1 facade
# is always shared, while Core is statically composed behind it; therefore users deploy one HyRemote
# runtime library and never need to discover internal libraries by filename.
function(_hyremote_generate_remoteaccess_deploy_script target output_var)
    if(NOT TARGET HyRemote::RemoteAccess)
        message(FATAL_ERROR
            "hyremote_deploy(TARGET ${target}) expected shared runtime target HyRemote::RemoteAccess in this SDK")
    endif()
    if(NOT DEFINED QT_DEPLOY_SUPPORT OR "${QT_DEPLOY_SUPPORT}" STREQUAL "")
        message(FATAL_ERROR
            "hyremote_deploy(TARGET ${target}) requires Qt's deployment support script from Qt6 Core")
    endif()

    _hyremote_runtime_deploy_dir(_runtime_deploy_dir)
    set(_runtime_script "${CMAKE_CURRENT_BINARY_DIR}/hyremote-runtime-deploy-${target}-$<CONFIG>.cmake")
    file(GENERATE
        OUTPUT "${_runtime_script}"
        CONTENT
"include(\"${QT_DEPLOY_SUPPORT}\")
file(INSTALL DESTINATION \"\${QT_DEPLOY_PREFIX}/${_runtime_deploy_dir}\" TYPE FILE FILES \"$<TARGET_FILE:HyRemote::RemoteAccess>\")
qt_deploy_runtime_dependencies(
    EXECUTABLE \"\${QT_DEPLOY_BIN_DIR}/$<TARGET_FILE_NAME:${target}>\"
    ADDITIONAL_LIBRARIES \"${_runtime_deploy_dir}/$<TARGET_FILE_NAME:HyRemote::RemoteAccess>\"
)
")
    set(${output_var} "${_runtime_script}" PARENT_SCOPE)
endfunction()

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
    if(NOT TARGET HyRemote::RemoteAccess)
        message(FATAL_ERROR
            "hyremote_deploy(TARGET ${target} QPA) expected shared runtime target HyRemote::RemoteAccess in this SDK")
    endif()

    _hyremote_runtime_deploy_dir(_runtime_deploy_dir)

    set(_linux_plugin_rpath_rewrite "")
    if(UNIX AND NOT APPLE)
        # In the SDK qhyremote lives at <libdir>/HyRemote/plugins/platforms and resolves the shared
        # facade via $ORIGIN/../../... Deployment relocates it to <plugins>/platforms while the
        # facade goes to QT_DEPLOY_LIB_DIR; rewrite only that controlled product RPATH.
        set(_linux_plugin_rpath_rewrite
"file(RPATH_CHANGE
    FILE \"\${QT_DEPLOY_PREFIX}/\${QT_DEPLOY_PLUGINS_DIR}/platforms/$<TARGET_FILE_NAME:HyRemote::QpaPlatform>\"
    OLD_RPATH \"$ORIGIN/../../..\"
    NEW_RPATH \"$ORIGIN/../../\${QT_DEPLOY_LIB_DIR}\"
)\n")
    endif()

    set(_qpa_script "${CMAKE_CURRENT_BINARY_DIR}/hyremote-qpa-deploy-${target}-$<CONFIG>.cmake")
    file(GENERATE
        OUTPUT "${_qpa_script}"
        CONTENT
"include(\"${QT_DEPLOY_SUPPORT}\")
file(INSTALL DESTINATION \"\${QT_DEPLOY_PREFIX}/\${QT_DEPLOY_PLUGINS_DIR}/platforms\" TYPE FILE FILES \"$<TARGET_FILE:HyRemote::QpaPlatform>\")
${_linux_plugin_rpath_rewrite}file(INSTALL DESTINATION \"\${QT_DEPLOY_PREFIX}/${_runtime_deploy_dir}\" TYPE FILE FILES \"$<TARGET_FILE:HyRemote::RemoteAccess>\")
qt_deploy_runtime_dependencies(
    EXECUTABLE \"\${QT_DEPLOY_BIN_DIR}/$<TARGET_FILE_NAME:${target}>\"
    ADDITIONAL_MODULES \"\${QT_DEPLOY_PLUGINS_DIR}/platforms/$<TARGET_FILE_NAME:HyRemote::QpaPlatform>\"
    ADDITIONAL_LIBRARIES \"${_runtime_deploy_dir}/$<TARGET_FILE_NAME:HyRemote::RemoteAccess>\"
)
")

    set(${output_var} "${_qpa_script}" PARENT_SCOPE)
endfunction()

# Public deployment entry point installed with the HyRemote CMake package.
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
            "${HYREMOTE_DEPLOY_TARGET}" _hyremote_supplemental_deploy_script)
    elseif(TARGET HyRemote::RemoteAccess)
        _hyremote_generate_remoteaccess_deploy_script(
            "${HYREMOTE_DEPLOY_TARGET}" _hyremote_supplemental_deploy_script)
    endif()

    if(HYREMOTE_DEPLOY_QML)
        if(NOT COMMAND qt_generate_deploy_qml_app_script)
            message(FATAL_ERROR
                "hyremote_deploy(TARGET ${HYREMOTE_DEPLOY_TARGET} QML) requires Qt's "
                "qt_generate_deploy_qml_app_script(). Find Qt6 Qml before calling the helper.")
        endif()

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
        elseif(HYREMOTE_DEPLOY_QPA OR DEFINED _hyremote_supplemental_deploy_script)
            message(FATAL_ERROR
                "hyremote_deploy(TARGET ${HYREMOTE_DEPLOY_TARGET}) requires Qt's qt_generate_deploy_app_script()")
        endif()
    endif()

    # Always run the HyRemote-owned supplemental script after Qt's normal deployment. For C++/QML it
    # carries the single shared facade; for QPA it carries that facade plus qhyremote. No manual
    # backend/runtime filenames or QT_PLUGIN_PATH are part of the normal user workflow.
    if(DEFINED _hyremote_supplemental_deploy_script AND
       NOT "${_hyremote_supplemental_deploy_script}" STREQUAL "")
        install(SCRIPT "${_hyremote_supplemental_deploy_script}")
    endif()
endfunction()
