include_guard(GLOBAL)

# Public deployment entry point installed with the HyRemote CMake package.
#
# HyRemote owns one application-facing deployment hook. It delegates ordinary Qt applications to
# Qt's supported app deployment API and declarative applications to Qt's QML-aware deployment API;
# later HyRemote transport/QPA payloads extend this same hook instead of creating backend-specific
# commands that application developers must understand.
#
# Usage:
#   hyremote_deploy(TARGET MyWidgetsApp)
#   hyremote_deploy(TARGET MyQmlApp QML)
function(hyremote_deploy)
    set(options QML)
    set(oneValueArgs TARGET)
    set(multiValueArgs)
    cmake_parse_arguments(HYREMOTE_DEPLOY "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(HYREMOTE_DEPLOY_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "hyremote_deploy: unknown arguments: ${HYREMOTE_DEPLOY_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT HYREMOTE_DEPLOY_TARGET)
        message(FATAL_ERROR "hyremote_deploy(TARGET <application> [QML]) requires TARGET")
    endif()
    if(NOT TARGET "${HYREMOTE_DEPLOY_TARGET}")
        message(FATAL_ERROR "hyremote_deploy: '${HYREMOTE_DEPLOY_TARGET}' is not a CMake target")
    endif()

    if(HYREMOTE_DEPLOY_QML)
        if(NOT COMMAND qt_generate_deploy_qml_app_script)
            message(FATAL_ERROR
                "hyremote_deploy(TARGET ${HYREMOTE_DEPLOY_TARGET} QML) requires Qt's "
                "qt_generate_deploy_qml_app_script(). Find Qt6 Qml before calling the helper.")
        endif()

        # An installed HyRemote package publishes the root containing its `HyRemote/qmldir` as
        # HyRemote_QML_IMPORT_PATH. Qt 6.8's QML tooling reads the target's QT_QML_IMPORT_PATH when
        # qmlimportscanner prepares deployment metadata. Bridge the package-owned path into that
        # normal Qt mechanism here so external consumers do not have to know/copy the plugin or
        # manually reproduce a scanner command.
        #
        # This remains intentionally conditional: source/build-tree users may not have the installed
        # package variable, and their project can already provide its own qt_add_qml_module
        # IMPORT_PATH. APPEND preserves every caller-provided import path.
        if(DEFINED HyRemote_QML_IMPORT_PATH AND NOT "${HyRemote_QML_IMPORT_PATH}" STREQUAL "")
            if(NOT IS_ABSOLUTE "${HyRemote_QML_IMPORT_PATH}")
                message(FATAL_ERROR
                    "hyremote_deploy: HyRemote_QML_IMPORT_PATH must be an absolute installed QML import root")
            endif()
            set_property(TARGET "${HYREMOTE_DEPLOY_TARGET}" APPEND PROPERTY
                QT_QML_IMPORT_PATH "${HyRemote_QML_IMPORT_PATH}")
        endif()

        # Qt documents that QML applications must use the QML-aware deployment script and that it is
        # an error to generate both the QML and non-QML deploy scripts for the same target. On a host
        # where generic runtime dependency deployment is unsupported, still deploy project QML
        # modules rather than silently omitting the HyRemote module.
        qt_generate_deploy_qml_app_script(
            TARGET "${HYREMOTE_DEPLOY_TARGET}"
            OUTPUT_SCRIPT _hyremote_qt_deploy_script
            NO_UNSUPPORTED_PLATFORM_ERROR
            DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM
        )
        install(SCRIPT "${_hyremote_qt_deploy_script}")
        return()
    endif()

    if(COMMAND qt_generate_deploy_app_script)
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
