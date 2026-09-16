include_guard(GLOBAL)

function(_hyremote_runtime_deploy_dir output_var)
    if(WIN32)
        set(${output_var} "\${QT_DEPLOY_BIN_DIR}" PARENT_SCOPE)
    else()
        set(${output_var} "\${QT_DEPLOY_LIB_DIR}" PARENT_SCOPE)
    endif()
endfunction()

# Return whether a target belongs to the current build graph rather than an installed/imported SDK.
# This lets source deployment validate current-configure payload targets without publishing another
# application-facing package variable. Resolve aliases explicitly for the CMake 3.21 baseline.
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

# add_subdirectory(... EXCLUDE_FROM_ALL) is the normal source-consumption shape. A generated install
# script that references $<TARGET_FILE:...> does not itself make that local target part of the
# consumer application's build. Add build-only dependencies for local HyRemote payload targets while
# leaving installed/imported SDK targets untouched and, crucially, without adding application links.
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

# Generate the supplemental deployment script for the normal C++/QML product path. The V1 facade
# is always shared, while Core is statically composed behind it; users deploy one HyRemote runtime.
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

# Resolve the QPA payload without exposing an installed C++ link target. Source-tree calls use the
# internal build target; installed-package calls use the absolute payload path published by config.
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

    # Source QPA is compiled in this exact configure and its V1 private-ABI contract is fixed to
    # Qt 6.8.3. Do not let an unrelated installed SDK's metadata in the parent scope override that
    # source truth. Installed acquisition uses the package-published exact version instead.
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
qt_deploy_runtime_dependencies(
    EXECUTABLE \"\${QT_DEPLOY_BIN_DIR}/$<TARGET_FILE_NAME:${target}>\"
    ADDITIONAL_MODULES \"\${QT_DEPLOY_PLUGINS_DIR}/platforms/${_qpa_plugin_name}\"
    ADDITIONAL_LIBRARIES \"${_runtime_deploy_dir}/$<TARGET_FILE_NAME:HyRemote::RemoteAccess>\"
)
")

    set(${output_var} "${_qpa_script}" PARENT_SCOPE)
endfunction()

# Public deployment entry point installed with the HyRemote CMake package.
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

    # Acquisition identity is derived from the one public runtime target. Source/add_subdirectory
    # provides a local target; installed find_package provides an imported target. Optional source
    # payload requests must be satisfied by targets from this exact configure and may not fall back
    # to stale metadata left by another installed SDK in the parent CMake scope.
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
