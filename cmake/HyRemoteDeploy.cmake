include_guard(GLOBAL)

function(_hyremote_runtime_deploy_dir output_var)
    if(WIN32)
        set(${output_var} "\${QT_DEPLOY_BIN_DIR}" PARENT_SCOPE)
    else()
        set(${output_var} "\${QT_DEPLOY_LIB_DIR}" PARENT_SCOPE)
    endif()
endfunction()

function(_hyremote_target_is_local target output_var)
    set(_hyremote_local FALSE)
    if(TARGET "${target}")
        get_target_property(_hyremote_aliased_target "${target}" ALIASED_TARGET)
        if(_hyremote_aliased_target)
            set(_hyremote_build_target "${_hyremote_aliased_target}")
        else()
            set(_hyremote_build_target "${target}")
        endif()
        get_target_property(_hyremote_dependency_imported "${_hyremote_build_target}" IMPORTED)
        if(NOT _hyremote_dependency_imported)
            set(_hyremote_local TRUE)
        endif()
    endif()
    set(${output_var} "${_hyremote_local}" PARENT_SCOPE)
endfunction()

function(_hyremote_add_local_build_dependency consumer dependency)
    _hyremote_target_is_local("${dependency}" _hyremote_dependency_local)
    if(NOT _hyremote_dependency_local)
        return()
    endif()

    get_target_property(_hyremote_aliased_target "${dependency}" ALIASED_TARGET)
    if(_hyremote_aliased_target)
        set(_hyremote_build_target "${_hyremote_aliased_target}")
    else()
        set(_hyremote_build_target "${dependency}")
    endif()
    add_dependencies("${consumer}" "${_hyremote_build_target}")
endfunction()

# The installed shared facade intentionally exposes only Qt Core/Network through its CMake link interface,
# even when the runtime itself was built with private Widgets/Quick adapters. On Linux, Qt's generic deploy
# helper cannot resolve those private DT_NEEDED entries from a relocated HyRemote SDK because the installed
# facade correctly carries only an $ORIGIN RPATH. Bootstrap just those runtime dependencies from the consumer's
# own Qt prefix before handing control back to Qt's normal deploy helper. This keeps the public target surface
# minimal and keeps the deployed tree independent of both the HyRemote SDK and the Qt SDK locations.
function(_hyremote_linux_private_runtime_bootstrap runtime_deploy_dir output_var)
    if(NOT UNIX OR APPLE)
        set(${output_var} "" PARENT_SCOPE)
        return()
    endif()

    # The bootstrap names the consumer Qt prefix through target-dependent generator expressions, and
    # file(GENERATE) evaluates those in the deploying project. A project that fakes Qt deployment - the
    # deploy-helper fixtures do exactly that - has no Qt targets, and a project that cannot link Qt6::Core
    # cannot link HyRemote::RemoteAccess either, so an absent Core target means there is no private Qt
    # runtime to bootstrap. Without this guard the generate step fails ("No target \"Qt6::Core\"") before any
    # assertion can run, which takes out every deploy-helper fixture case on Linux.
    if(NOT TARGET Qt6::Core)
        set(${output_var} "" PARENT_SCOPE)
        return()
    endif()

    set(_bootstrap
"file(GET_RUNTIME_DEPENDENCIES
    LIBRARIES \"\${QT_DEPLOY_PREFIX}/${runtime_deploy_dir}/$<TARGET_FILE_NAME:HyRemote::RemoteAccess>\"
    DIRECTORIES \"$<TARGET_FILE_DIR:Qt6::Core>\"
    RESOLVED_DEPENDENCIES_VAR _hyremote_private_runtime_dependencies
    UNRESOLVED_DEPENDENCIES_VAR _hyremote_private_runtime_unresolved
)
if(_hyremote_private_runtime_unresolved)
    message(FATAL_ERROR
        \"HyRemote runtime deployment could not resolve private dependencies: \${_hyremote_private_runtime_unresolved}\")
endif()
foreach(_hyremote_dependency IN LISTS _hyremote_private_runtime_dependencies)
    string(FIND \"\${_hyremote_dependency}\" \"$<TARGET_FILE_DIR:Qt6::Core>/\" _hyremote_qt_prefix_index)
    if(_hyremote_qt_prefix_index EQUAL 0)
        file(COPY \"\${_hyremote_dependency}\"
             DESTINATION \"\${QT_DEPLOY_PREFIX}/${runtime_deploy_dir}\"
             FOLLOW_SYMLINK_CHAIN)
    endif()
endforeach()
")
    set(${output_var} "${_bootstrap}" PARENT_SCOPE)
endfunction()

# Resolve the declarative module's backing shared library without creating a public C++ package target.
# Source acquisition names the concrete local qt_add_qml_module backing target; installed acquisition
# consumes absolute package metadata. Applications still consume only the stable `import HyRemote` URI.
function(_hyremote_resolve_qml_backing_payload file_var name_var)
    if(TARGET hyremote-qml)
        set(${file_var} "$<TARGET_FILE:hyremote-qml>" PARENT_SCOPE)
        set(${name_var} "$<TARGET_FILE_NAME:hyremote-qml>" PARENT_SCOPE)
        return()
    endif()

    if(DEFINED HyRemote_QML_BACKING_FILE AND NOT "${HyRemote_QML_BACKING_FILE}" STREQUAL "")
        if(NOT IS_ABSOLUTE "${HyRemote_QML_BACKING_FILE}")
            message(FATAL_ERROR "HyRemote_QML_BACKING_FILE must be an absolute installed payload path")
        endif()
        if(NOT EXISTS "${HyRemote_QML_BACKING_FILE}")
            message(FATAL_ERROR
                "installed HyRemote QML backing payload is missing: ${HyRemote_QML_BACKING_FILE}")
        endif()
        get_filename_component(_qml_backing_name "${HyRemote_QML_BACKING_FILE}" NAME)
        set(${file_var} "${HyRemote_QML_BACKING_FILE}" PARENT_SCOPE)
        set(${name_var} "${_qml_backing_name}" PARENT_SCOPE)
        return()
    endif()

    message(FATAL_ERROR
        "hyremote_deploy QML backing payload is unavailable; install/build HyRemote with HYREMOTE_BUILD_QML_API=ON")
endfunction()

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
    _hyremote_linux_private_runtime_bootstrap("${_runtime_deploy_dir}" _linux_private_runtime_bootstrap)

    set(_qml_backing_install "")
    set(_qml_additional_library "")
    if(HYREMOTE_DEPLOY_QML)
        _hyremote_resolve_qml_backing_payload(_qml_backing_file _qml_backing_name)
        set(_qml_backing_install
"file(INSTALL DESTINATION \"\${QT_DEPLOY_PREFIX}/${_runtime_deploy_dir}\" TYPE FILE FILES \"${_qml_backing_file}\")\n")
        set(_qml_additional_library
"\n    \"${_runtime_deploy_dir}/${_qml_backing_name}\"")
    endif()

    set(_runtime_script "${CMAKE_CURRENT_BINARY_DIR}/hyremote-runtime-deploy-${target}-$<CONFIG>.cmake")
    file(GENERATE
        OUTPUT "${_runtime_script}"
        CONTENT
"include(\"${QT_DEPLOY_SUPPORT}\")
file(INSTALL DESTINATION \"\${QT_DEPLOY_PREFIX}/${_runtime_deploy_dir}\" TYPE FILE FILES \"$<TARGET_FILE:HyRemote::RemoteAccess>\")
${_qml_backing_install}${_linux_private_runtime_bootstrap}qt_deploy_runtime_dependencies(
    EXECUTABLE \"\${QT_DEPLOY_BIN_DIR}/$<TARGET_FILE_NAME:${target}>\"
    ADDITIONAL_LIBRARIES
    \"${_runtime_deploy_dir}/$<TARGET_FILE_NAME:HyRemote::RemoteAccess>\"${_qml_additional_library}
)
")
    set(${output_var} "${_runtime_script}" PARENT_SCOPE)
endfunction()

function(_hyremote_resolve_qpa_payload file_var name_var)
    if(TARGET HyRemote::QpaPlatform)
        set(${file_var} "$<TARGET_FILE:HyRemote::QpaPlatform>" PARENT_SCOPE)
        set(${name_var} "$<TARGET_FILE_NAME:HyRemote::QpaPlatform>" PARENT_SCOPE)
        return()
    endif()

    if(DEFINED HyRemote_QPA_PLUGIN_FILE AND NOT "${HyRemote_QPA_PLUGIN_FILE}" STREQUAL "")
        if(NOT IS_ABSOLUTE "${HyRemote_QPA_PLUGIN_FILE}")
            message(FATAL_ERROR "HyRemote_QPA_PLUGIN_FILE must be an absolute installed payload path")
        endif()
        if(NOT EXISTS "${HyRemote_QPA_PLUGIN_FILE}")
            message(FATAL_ERROR
                "installed HyRemote QPA payload is missing: ${HyRemote_QPA_PLUGIN_FILE}")
        endif()
        get_filename_component(_qpa_name "${HyRemote_QPA_PLUGIN_FILE}" NAME)
        set(${file_var} "${HyRemote_QPA_PLUGIN_FILE}" PARENT_SCOPE)
        set(${name_var} "${_qpa_name}" PARENT_SCOPE)
        return()
    endif()

    message(FATAL_ERROR
        "Transparent QPA payload is unavailable; install/build HyRemote with HYREMOTE_WITH_QPA_PROXY=ON")
endfunction()

function(_hyremote_generate_qpa_deploy_script target output_var)
    if(APPLE OR (NOT WIN32 AND NOT UNIX))
        message(FATAL_ERROR
            "hyremote_deploy(TARGET ${target} QPA) is supported only for the V1 Windows/Linux reference platforms")
    endif()

    _hyremote_target_is_local(HyRemote::RemoteAccess _hyremote_qpa_source_acquisition)
    if(_hyremote_qpa_source_acquisition)
        set(_required_qt_version "6.8.3")
    else()
        set(_required_qt_version "${HyRemote_QPA_QT_VERSION}")
        if(_required_qt_version STREQUAL "")
            set(_required_qt_version "6.8.3")
        endif()
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

    _hyremote_resolve_qpa_payload(_qpa_plugin_file _qpa_plugin_name)
    _hyremote_runtime_deploy_dir(_runtime_deploy_dir)
    _hyremote_linux_private_runtime_bootstrap("${_runtime_deploy_dir}" _linux_private_runtime_bootstrap)

    set(_qml_backing_install "")
    set(_qml_additional_library "")
    if(HYREMOTE_DEPLOY_QML)
        _hyremote_resolve_qml_backing_payload(_qml_backing_file _qml_backing_name)
        set(_qml_backing_install
"file(INSTALL DESTINATION \"\${QT_DEPLOY_PREFIX}/${_runtime_deploy_dir}\" TYPE FILE FILES \"${_qml_backing_file}\")\n")
        set(_qml_additional_library
"\n    \"${_runtime_deploy_dir}/${_qml_backing_name}\"")
    endif()

    set(_linux_plugin_rpath_rewrite "")
    if(UNIX AND NOT APPLE)
        set(_linux_plugin_rpath_rewrite
"file(RPATH_CHANGE
    FILE \"\${QT_DEPLOY_PREFIX}/\${QT_DEPLOY_PLUGINS_DIR}/platforms/${_qpa_plugin_name}\"
    OLD_RPATH \"$ORIGIN/../../../.\"
    NEW_RPATH \"$ORIGIN/../../\${QT_DEPLOY_LIB_DIR}\"
)\n")
    endif()

    set(_qpa_script "${CMAKE_CURRENT_BINARY_DIR}/hyremote-qpa-deploy-${target}-$<CONFIG>.cmake")
    file(GENERATE
        OUTPUT "${_qpa_script}"
        CONTENT
"include(\"${QT_DEPLOY_SUPPORT}\")
file(INSTALL DESTINATION \"\${QT_DEPLOY_PREFIX}/\${QT_DEPLOY_PLUGINS_DIR}/platforms\" TYPE FILE FILES \"${_qpa_plugin_file}\")
${_linux_plugin_rpath_rewrite}file(INSTALL DESTINATION \"\${QT_DEPLOY_PREFIX}/${_runtime_deploy_dir}\" TYPE FILE FILES \"$<TARGET_FILE:HyRemote::RemoteAccess>\")
${_qml_backing_install}${_linux_private_runtime_bootstrap}qt_deploy_runtime_dependencies(
    EXECUTABLE \"\${QT_DEPLOY_BIN_DIR}/$<TARGET_FILE_NAME:${target}>\"
    ADDITIONAL_MODULES \"\${QT_DEPLOY_PLUGINS_DIR}/platforms/${_qpa_plugin_name}\"
    ADDITIONAL_LIBRARIES
    \"${_runtime_deploy_dir}/$<TARGET_FILE_NAME:HyRemote::RemoteAccess>\"${_qml_additional_library}
)
")

    set(${output_var} "${_qpa_script}" PARENT_SCOPE)
endfunction()

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

    _hyremote_target_is_local(HyRemote::RemoteAccess _hyremote_source_acquisition)

    if(HYREMOTE_DEPLOY_QML)
        if(_hyremote_source_acquisition AND NOT TARGET hyremote-qml)
            message(FATAL_ERROR
                "hyremote_deploy(TARGET ${HYREMOTE_DEPLOY_TARGET} QML) requires a HyRemote QML payload; "
                "install/build HyRemote with HYREMOTE_BUILD_QML_API=ON")
        endif()
        if(NOT DEFINED HyRemote_QML_IMPORT_PATH OR "${HyRemote_QML_IMPORT_PATH}" STREQUAL "")
            message(FATAL_ERROR
                "hyremote_deploy(TARGET ${HYREMOTE_DEPLOY_TARGET} QML) requires a HyRemote QML payload; "
                "install/build HyRemote with HYREMOTE_BUILD_QML_API=ON")
        endif()
        if(NOT IS_ABSOLUTE "${HyRemote_QML_IMPORT_PATH}")
            message(FATAL_ERROR
                "hyremote_deploy: HyRemote_QML_IMPORT_PATH must be an absolute QML import root")
        endif()
        if(NOT IS_DIRECTORY "${HyRemote_QML_IMPORT_PATH}")
            message(FATAL_ERROR
                "hyremote_deploy: HyRemote QML import root does not exist: ${HyRemote_QML_IMPORT_PATH}")
        endif()
        if(NOT IS_DIRECTORY "${HyRemote_QML_IMPORT_PATH}/HyRemote")
            message(FATAL_ERROR
                "hyremote_deploy: HyRemote QML module directory does not exist under import root: "
                "${HyRemote_QML_IMPORT_PATH}/HyRemote")
        endif()

        # Fail closed on a damaged installed SDK before generating Qt deployment scripts. Source
        # acquisition is validated by the concrete local backing target; installed acquisition is
        # validated through absolute package payload metadata without adding a public QML link target.
        _hyremote_resolve_qml_backing_payload(_hyremote_qml_backing_probe _hyremote_qml_backing_name_probe)
    endif()

    if(HYREMOTE_DEPLOY_QPA AND _hyremote_source_acquisition AND NOT TARGET HyRemote::QpaPlatform)
        message(FATAL_ERROR
            "hyremote_deploy(TARGET ${HYREMOTE_DEPLOY_TARGET} QPA) requires the current HyRemote source build "
            "to enable HYREMOTE_WITH_QPA_PROXY=ON; installed QPA metadata cannot satisfy a source deployment")
    endif()

    _hyremote_add_local_build_dependency(
        "${HYREMOTE_DEPLOY_TARGET}" HyRemote::RemoteAccess)
    if(HYREMOTE_DEPLOY_QPA)
        _hyremote_add_local_build_dependency(
            "${HYREMOTE_DEPLOY_TARGET}" HyRemote::QpaPlatform)
    endif()
    if(HYREMOTE_DEPLOY_QML)
        get_property(_hyremote_qml_source_targets GLOBAL PROPERTY HYREMOTE_QML_SOURCE_DEPLOY_TARGETS)
        foreach(_hyremote_qml_source_target IN LISTS _hyremote_qml_source_targets)
            _hyremote_add_local_build_dependency(
                "${HYREMOTE_DEPLOY_TARGET}" "${_hyremote_qml_source_target}")
        endforeach()
    endif()

    if(HYREMOTE_DEPLOY_QPA)
        if(NOT _hyremote_source_acquisition
           AND DEFINED HyRemote_QPA_AVAILABLE AND NOT HyRemote_QPA_AVAILABLE)
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

        set_property(TARGET "${HYREMOTE_DEPLOY_TARGET}" APPEND PROPERTY
            QT_QML_IMPORT_PATH "${HyRemote_QML_IMPORT_PATH}")

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

    if(DEFINED _hyremote_supplemental_deploy_script AND
       NOT "${_hyremote_supplemental_deploy_script}" STREQUAL "")
        install(SCRIPT "${_hyremote_supplemental_deploy_script}")
    endif()
endfunction()