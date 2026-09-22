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
# even when the runtime itself was built with private Widgets/Quick adapters. The Linux native qxcb delegate
# likewise carries a private Qt runtime dependency that is not discoverable merely from the consuming
# executable. Bootstrap those private Qt dependencies from the consumer's own exact Qt prefix before handing
# control back to Qt's normal deploy helper. This keeps the public target surface minimal and keeps the deployed
# tree independent of both the HyRemote SDK and the Qt SDK locations.
#
# The optional third argument is the already-selected native QPA plugin filename. Ordinary C++/QML deployment
# scans only RemoteAccess; QPA deployment scans RemoteAccess plus that native delegate so both source and
# installed-SDK acquisition receive the same self-contained Linux runtime closure.
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

    set(_hyremote_bootstrap_libraries
"    \"\${QT_DEPLOY_PREFIX}/${runtime_deploy_dir}/$<TARGET_FILE_NAME:HyRemote::RemoteAccess>\"")
    if(ARGC GREATER 2 AND NOT "${ARGV2}" STREQUAL "")
        string(APPEND _hyremote_bootstrap_libraries
"\n    \"\${QT_DEPLOY_PREFIX}/\${QT_DEPLOY_PLUGINS_DIR}/platforms/${ARGV2}\"")
    endif()

    set(_bootstrap
"file(GET_RUNTIME_DEPENDENCIES
    LIBRARIES
${_hyremote_bootstrap_libraries}
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

# The encrypted profile's private runtime payload, as a deploy-script fragment. Source acquisition knows the exact
# files because it linked against them; installed acquisition reads the prefix-relative location the package
# publishes. Both produce the same deployed semantics: the application completes a TLS session with no OpenSSL on
# PATH and no environment variable pointing at one. A security-enabled deploy that cannot find the payload fails here
# rather than producing a tree that dies at the first TLS use.
function(_hyremote_security_private_runtime_install runtime_deploy_dir output_var)
    # The deploy contract is driven by the runtime mode, because the modes mean genuinely different things:
    #   NONE   - the capability is not part of this build; nothing is added, nothing is required.
    #   BUNDLED- Windows: Qt's TLS backend plugin loads libssl/libcrypto at run time, Qt's own tooling does not copy
    #            them, and a Windows executable has no rpath. The package carries them and a deploy that cannot find
    #            them fails here rather than producing a tree that dies at the first TLS use.
    #   SYSTEM - Linux: the distribution's runtime is the platform's own and every deployed application resolves it
    #            through the standard loader paths. Empty files are correct here, and demanding a payload would be
    #            the Windows invariant applied to a platform that does not have that problem.
    #   STATIC - OpenSSL is linked into the runtime itself; there is no runtime payload at all.
    set(_hyremote_mode "${HYREMOTE_PACKAGE_SECURITY_RUNTIME_MODE}")
    if(_hyremote_mode STREQUAL "")
        set(_hyremote_mode "NONE")
    endif()
    if(DEFINED HyRemote_SECURITY_RUNTIME_MODE AND NOT "${HyRemote_SECURITY_RUNTIME_MODE}" STREQUAL "")
        set(_hyremote_mode "${HyRemote_SECURITY_RUNTIME_MODE}")
    endif()

    set(_hyremote_security_install "")
    set(_hyremote_security_files "")

    if(DEFINED HYREMOTE_PACKAGE_WITH_SECURITY_RUNTIME AND HYREMOTE_PACKAGE_WITH_SECURITY_RUNTIME)
        set(_hyremote_security_files "${HYREMOTE_PACKAGE_SECURITY_RUNTIME_SOURCE_FILES}")
    elseif(DEFINED HyRemote_SECURITY_RUNTIME_FILES AND NOT "${HyRemote_SECURITY_RUNTIME_FILES}" STREQUAL "")
        set(_hyremote_security_files "${HyRemote_SECURITY_RUNTIME_DIR}|${HyRemote_SECURITY_RUNTIME_FILES}")
    endif()

    if(_hyremote_mode STREQUAL "NONE" OR _hyremote_mode STREQUAL "STATIC" OR _hyremote_mode STREQUAL "SYSTEM")
        # Nothing is added. SYSTEM deliberately tolerates an empty payload; NONE and STATIC have nothing to add.
        set(${output_var} "" PARENT_SCOPE)
        return()
    endif()

    if(NOT _hyremote_mode STREQUAL "BUNDLED")
        message(FATAL_ERROR
            "hyremote_deploy: unknown transport-security runtime mode '${_hyremote_mode}'. The deployment closure "
            "cannot be described honestly for a mode this project does not define.")
    endif()

    if(_hyremote_security_files STREQUAL "")
        message(FATAL_ERROR
            "hyremote_deploy: this HyRemote runs the encrypted profile with a bundled OpenSSL runtime but publishes "
            "no runtime payload, so a deployed application would start and fail at the first TLS use. Rebuild the SDK "
            "so the capability and its payload come from the same configuration.")
    endif()

    if(DEFINED HYREMOTE_PACKAGE_WITH_SECURITY_RUNTIME AND HYREMOTE_PACKAGE_WITH_SECURITY_RUNTIME)
        foreach(_hyremote_security_file IN LISTS _hyremote_security_files)
            string(APPEND _hyremote_security_install
"file(INSTALL DESTINATION \"\${QT_DEPLOY_PREFIX}/${runtime_deploy_dir}\" TYPE FILE FILES \"${_hyremote_security_file}\")\n")
        endforeach()
    else()
        foreach(_hyremote_security_pair IN LISTS _hyremote_security_files)
            string(REPLACE "|" ";" _hyremote_split "${_hyremote_security_pair}")
            list(GET _hyremote_split 0 _hyremote_dir)
            list(GET _hyremote_split 1 _hyremote_files)
            foreach(_hyremote_security_file IN LISTS _hyremote_files)
                string(APPEND _hyremote_security_install
"if(NOT EXISTS \"${_hyremote_dir}/${_hyremote_security_file}\")
    message(FATAL_ERROR \"HyRemote: the installed package does not carry the OpenSSL runtime its encrypted profile needs (\"${_hyremote_dir}/${_hyremote_security_file}\" is missing); reinstall the SDK or rebuild without HYREMOTE_WITH_TRANSPORT_SECURITY.\")
endif()
file(INSTALL DESTINATION \"\${QT_DEPLOY_PREFIX}/${runtime_deploy_dir}\" TYPE FILE FILES \"${_hyremote_dir}/${_hyremote_security_file}\")\n")
            endforeach()
        endforeach()
    endif()

    set(${output_var} "${_hyremote_security_install}" PARENT_SCOPE)
endfunction()

# Qt's deployment tool owns the TLS backend plugin, but it only deploys it when it is told which OpenSSL to take it
# from: hosted Windows logged "Skipping plugin qopensslbackend.dll. Use -force-openssl or specify -openssl-root".
# DEPLOY_TOOL_OPTIONS is Qt's own documented way to pass that through, so the plugin keeps being deployed by its owner.
# The prefix comes from the source build or from this package's own prefix, never from the build machine.
function(_hyremote_security_qt_deploy_options output_var)
    set(_hyremote_mode "${HYREMOTE_PACKAGE_SECURITY_RUNTIME_MODE}")
    if(DEFINED HyRemote_SECURITY_RUNTIME_MODE AND NOT "${HyRemote_SECURITY_RUNTIME_MODE}" STREQUAL "")
        set(_hyremote_mode "${HyRemote_SECURITY_RUNTIME_MODE}")
    endif()
    if(NOT WIN32 OR NOT _hyremote_mode STREQUAL "BUNDLED")
        set(${output_var} "" PARENT_SCOPE)
        return()
    endif()

    set(_hyremote_root "")
    if(DEFINED HYREMOTE_PACKAGE_WITH_SECURITY_RUNTIME AND HYREMOTE_PACKAGE_WITH_SECURITY_RUNTIME
       AND NOT "${HYREMOTE_PACKAGE_SECURITY_RUNTIME_SOURCE_FILES}" STREQUAL "")
        # Source acquisition: the runtime libraries were resolved from the OpenSSL this build linked against, so the
        # prefix is the directory above their bin directory. Nothing has to be stored anywhere for it.
        list(GET HYREMOTE_PACKAGE_SECURITY_RUNTIME_SOURCE_FILES 0 _hyremote_first_payload)
        get_filename_component(_hyremote_payload_dir "${_hyremote_first_payload}" DIRECTORY)
        get_filename_component(_hyremote_root "${_hyremote_payload_dir}" DIRECTORY)
    elseif(DEFINED HyRemote_SECURITY_OPENSSL_ROOT AND NOT "${HyRemote_SECURITY_OPENSSL_ROOT}" STREQUAL "")
        set(_hyremote_root "${HyRemote_SECURITY_OPENSSL_ROOT}")
    endif()
    if(_hyremote_root STREQUAL "")
        message(FATAL_ERROR
            "hyremote_deploy: the encrypted profile runs on Qt's OpenSSL backend, and Qt's deployment tool needs the "
            "OpenSSL prefix to deploy that backend's plugin, but no prefix is known for this acquisition.")
    endif()

    set(${output_var}
"    DEPLOY_TOOL_OPTIONS
    \"--openssl-root=${_hyremote_root}\"")
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
    # The ordinary C++ path keeps the application on its native Qt platform, so the deployed tree has to carry that
    # platform plugin exactly as the Generic path does: without it a deployed application cannot start at all unless
    # the machine happens to have the Qt SDK, which is what a clean deployment must not require. The plugin identity
    # comes from the same neutral resolver the Generic and QPA paths use, so no platform-conditional file name is
    # guessed here and the contract cannot drift into a Windows-only accidental pass.
    _hyremote_resolve_native_platform_payload(
        _native_platform_plugin_file _native_platform_plugin_name)
    # The Linux native delegate carries its own private Qt runtime dependency, so the shared bootstrap is given the
    # plugin and produces the closure that lets the deployed plugin resolve against the deployed Qt runtime.
    _hyremote_linux_private_runtime_bootstrap(
        "${_runtime_deploy_dir}" _linux_private_runtime_bootstrap "${_native_platform_plugin_name}")

    # The deployed platform plugin must find the deployed Qt runtime rather than the SDK it was built against - the
    # same relocation the QPA and Generic paths perform for the same plugin.
    set(_linux_platform_rpath_rewrite "")
    if(UNIX AND NOT APPLE)
        set(_linux_platform_rpath_rewrite
"file(RPATH_CHANGE
    FILE \"\${QT_DEPLOY_PREFIX}/\${QT_DEPLOY_PLUGINS_DIR}/platforms/${_native_platform_plugin_name}\"
    OLD_RPATH \"$ORIGIN/../../../.\"
    NEW_RPATH \"$ORIGIN/../../\${QT_DEPLOY_LIB_DIR}\"
)
")
    endif()

    set(_qml_backing_install "")
    set(_qml_additional_library "")
    if(HYREMOTE_DEPLOY_QML)
        _hyremote_resolve_qml_backing_payload(_qml_backing_file _qml_backing_name)
        set(_qml_backing_install
"file(INSTALL DESTINATION \"\${QT_DEPLOY_PREFIX}/${_runtime_deploy_dir}\" TYPE FILE FILES \"${_qml_backing_file}\")\n")
        set(_qml_additional_library
"\n    \"${_runtime_deploy_dir}/${_qml_backing_name}\"")
    endif()

    _hyremote_security_private_runtime_install("${_runtime_deploy_dir}" _security_private_runtime_install)
    _hyremote_security_qt_deploy_options(_security_qt_deploy_options)

    # Qt 6.8.3's versionless qt_deploy_runtime_dependencies() wrapper forwards ${ARGV}
    # unquoted and therefore loses argument boundaries when an executable filename contains spaces.
    # HyRemote is a Qt 6 package, so call the Qt 6 implementation directly and preserve PARSE_ARGV semantics.
    set(_runtime_script "${CMAKE_CURRENT_BINARY_DIR}/hyremote-runtime-deploy-${target}-$<CONFIG>.cmake")
    file(GENERATE
        OUTPUT "${_runtime_script}"
        CONTENT
"include(\"${QT_DEPLOY_SUPPORT}\")
file(INSTALL DESTINATION \"\${QT_DEPLOY_PREFIX}/${_runtime_deploy_dir}\" TYPE FILE FILES \"$<TARGET_FILE:HyRemote::RemoteAccess>\")
file(INSTALL DESTINATION \"\${QT_DEPLOY_PREFIX}/\${QT_DEPLOY_PLUGINS_DIR}/platforms\" TYPE FILE FILES
    \"${_native_platform_plugin_file}\")
${_linux_platform_rpath_rewrite}${_qml_backing_install}${_linux_private_runtime_bootstrap}${_security_private_runtime_install}qt6_deploy_runtime_dependencies(
${_security_qt_deploy_options}
    EXECUTABLE \"\${QT_DEPLOY_BIN_DIR}/$<TARGET_FILE_NAME:${target}>\"
    ADDITIONAL_MODULES
    \"\${QT_DEPLOY_PLUGINS_DIR}/platforms/${_native_platform_plugin_name}\"
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

# Resolve the native Qt platform plugin of the consuming Qt build. Qt 6 publishes platform plugins as individual
# CMake packages below Qt6Gui_DIR, and this resolver reads only that: it is deliberately neutral about who needs the
# plugin or why.
#
# Two different contracts need it, and they need it for opposite reasons:
#   - Transparent QPA replaces the application's platform integration, so qhyremote delegates to this plugin and the
#     proxy module therefore cannot link it directly; one hyremote_deploy(... QPA) call owns the complete transparent
#     platform chain in both source and installed SDK consumption.
#   - Generic Plugin preserves the application's platform integration, so the native plugin must simply be present in
#     the deployed tree alongside the generic payload. Without it a deployed Generic application cannot start at all
#     unless the machine happens to have the Qt SDK, which is exactly what a clean deployment must not require.
# Sharing the resolver is about locating the plugin, not about QPA semantics: nothing here reads HyRemote_QPA_QT_VERSION
# or any exact-private-ABI qualification, and Generic must never inherit those (Generic uses public Qt APIs only).
function(_hyremote_resolve_native_platform_payload file_var name_var)
    if(WIN32)
        set(_native_platform_target "Qt6::QWindowsIntegrationPlugin")
        set(_native_platform_package "Qt6QWindowsIntegrationPlugin")
        set(_native_platform_key "windows")
    elseif(UNIX AND NOT APPLE)
        set(_native_platform_target "Qt6::QXcbIntegrationPlugin")
        set(_native_platform_package "Qt6QXcbIntegrationPlugin")
        set(_native_platform_key "xcb")
    else()
        # Neutral wording on purpose: this resolver locates the consumer's own Qt platform plugin for the ordinary
        # C++/QML deployment path, the Generic path and QPA alike, so none of them may be told a QPA-specific or
        # release-specific story here.
        message(FATAL_ERROR
            "hyremote_deploy: the native Qt platform payload is resolvable only on the supported reference desktop "
            "platforms (Windows, Linux)")
    endif()

    if(NOT TARGET "${_native_platform_target}")
        if(NOT DEFINED Qt6Gui_DIR OR "${Qt6Gui_DIR}" STREQUAL "")
            message(FATAL_ERROR
                "hyremote_deploy: cannot locate Qt6Gui_DIR, so the native '${_native_platform_key}' Qt platform "
                "plugin package cannot be resolved")
        endif()
        find_package(${_native_platform_package} QUIET PATHS "${Qt6Gui_DIR}")
    endif()

    if(NOT TARGET "${_native_platform_target}")
        message(FATAL_ERROR
            "hyremote_deploy: the Qt build in use does not provide the native '${_native_platform_key}' Qt platform "
            "plugin package (${_native_platform_package})")
    endif()

    set(${file_var} "$<TARGET_FILE:${_native_platform_target}>" PARENT_SCOPE)
    set(${name_var} "$<TARGET_FILE_NAME:${_native_platform_target}>" PARENT_SCOPE)
endfunction()

function(_hyremote_generate_qpa_deploy_script target output_var)
    if(APPLE OR (NOT WIN32 AND NOT UNIX))
        message(FATAL_ERROR
            "hyremote_deploy(TARGET ${target} QPA) is supported on the supported reference desktop platforms "
            "(Windows, Linux); the QPA proxy is the frontend that carries the exact-Qt private-ABI requirement")
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
    _hyremote_resolve_native_platform_payload(_native_qpa_plugin_file _native_qpa_plugin_name)
    _hyremote_runtime_deploy_dir(_runtime_deploy_dir)
    _hyremote_linux_private_runtime_bootstrap(
        "${_runtime_deploy_dir}" _linux_private_runtime_bootstrap "${_native_qpa_plugin_name}")

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

    # Keep the same direct Qt 6 call here as in the ordinary/QML supplemental deploy script. QPA
    # consumers can also have executable output names containing spaces, and the Qt 6.8.3 versionless
    # forwarding wrapper does not preserve those arguments.
    # produced by the one helper the ordinary path already uses, never by a second copy.
    # This path deploys the same shared runtime, so it carries the same private security closure -
    _hyremote_security_private_runtime_install("${_runtime_deploy_dir}" _security_private_runtime_install)
    _hyremote_security_qt_deploy_options(_security_qt_deploy_options)
    set(_qpa_script "${CMAKE_CURRENT_BINARY_DIR}/hyremote-qpa-deploy-${target}-$<CONFIG>.cmake")
    file(GENERATE
        OUTPUT "${_qpa_script}"
        CONTENT
"include(\"${QT_DEPLOY_SUPPORT}\")
file(INSTALL DESTINATION \"\${QT_DEPLOY_PREFIX}/\${QT_DEPLOY_PLUGINS_DIR}/platforms\" TYPE FILE FILES
    \"${_qpa_plugin_file}\"
    \"${_native_qpa_plugin_file}\")
${_linux_plugin_rpath_rewrite}file(INSTALL DESTINATION \"\${QT_DEPLOY_PREFIX}/${_runtime_deploy_dir}\" TYPE FILE FILES \"$<TARGET_FILE:HyRemote::RemoteAccess>\")
${_qml_backing_install}${_linux_private_runtime_bootstrap}${_security_private_runtime_install}qt6_deploy_runtime_dependencies(
${_security_qt_deploy_options}
    EXECUTABLE \"\${QT_DEPLOY_BIN_DIR}/$<TARGET_FILE_NAME:${target}>\"
    ADDITIONAL_MODULES
    \"\${QT_DEPLOY_PLUGINS_DIR}/platforms/${_qpa_plugin_name}\"
    \"\${QT_DEPLOY_PLUGINS_DIR}/platforms/${_native_qpa_plugin_name}\"
    ADDITIONAL_LIBRARIES
    \"${_runtime_deploy_dir}/$<TARGET_FILE_NAME:HyRemote::RemoteAccess>\"${_qml_additional_library}
)
")

    set(${output_var} "${_qpa_script}" PARENT_SCOPE)
endfunction()

# The Generic Plugin is the other package payload and it is deliberately not a platform plugin: it is a
# QGenericPlugin built from public Qt APIs that leaves the application's native platform integration exactly as it
# was. Its identity therefore comes from the target (source acquisition) or from the installed package metadata
# (installed acquisition) - never from a toolchain-global prefix/suffix guess and never from a build-tree search,
# which is what would let a declared name and an installed artifact drift apart again.
function(_hyremote_resolve_generic_payload file_var name_var)
    if(TARGET hyremote-generic-plugin)
        set(${file_var} "$<TARGET_FILE:hyremote-generic-plugin>" PARENT_SCOPE)
        set(${name_var} "$<TARGET_FILE_NAME:hyremote-generic-plugin>" PARENT_SCOPE)
        return()
    endif()

    if(DEFINED HyRemote_GENERIC_AVAILABLE AND NOT HyRemote_GENERIC_AVAILABLE)
        message(FATAL_ERROR
            "hyremote_deploy(... GENERIC) requested a HyRemote SDK that was built without the Generic Plugin")
    endif()

    if(DEFINED HyRemote_GENERIC_PLUGIN_FILE AND NOT "${HyRemote_GENERIC_PLUGIN_FILE}" STREQUAL "")
        if(NOT IS_ABSOLUTE "${HyRemote_GENERIC_PLUGIN_FILE}")
            message(FATAL_ERROR "HyRemote_GENERIC_PLUGIN_FILE must be an absolute installed payload path")
        endif()
        if(NOT EXISTS "${HyRemote_GENERIC_PLUGIN_FILE}")
            message(FATAL_ERROR
                "installed HyRemote Generic Plugin payload is missing: ${HyRemote_GENERIC_PLUGIN_FILE}")
        endif()
        get_filename_component(_generic_name "${HyRemote_GENERIC_PLUGIN_FILE}" NAME)
        set(${file_var} "${HyRemote_GENERIC_PLUGIN_FILE}" PARENT_SCOPE)
        set(${name_var} "${_generic_name}" PARENT_SCOPE)
        return()
    endif()

    message(FATAL_ERROR
        "Generic Plugin payload is unavailable; install/build HyRemote with HYREMOTE_WITH_GENERIC_PLUGIN=ON")
endfunction()

function(_hyremote_generate_generic_deploy_script target output_var)
    if(APPLE OR (NOT WIN32 AND NOT UNIX))
        message(FATAL_ERROR
            "hyremote_deploy(TARGET ${target} GENERIC) is supported on the supported reference desktop platforms "
            "(Windows, Linux); the Generic Plugin uses public Qt APIs and carries no private-ABI coupling")
    endif()

    if(NOT DEFINED QT_DEPLOY_SUPPORT OR "${QT_DEPLOY_SUPPORT}" STREQUAL "")
        message(FATAL_ERROR
            "hyremote_deploy(TARGET ${target} GENERIC) requires Qt's deployment support script from Qt6 Core")
    endif()
    if(NOT TARGET HyRemote::RemoteAccess)
        message(FATAL_ERROR
            "hyremote_deploy(TARGET ${target} GENERIC) expected shared runtime target HyRemote::RemoteAccess in this SDK")
    endif()

    _hyremote_resolve_generic_payload(_generic_plugin_file _generic_plugin_name)
    # Generic preserves the application's platform integration, so the native Qt platform plugin has to be in the
    # deployed tree as well. Two different payloads, two different directories, and neither substitutes for the
    # other: the generic payload makes HyRemote reachable with no code, and the native platform plugin is what keeps
    # the application on its normal platform identity. Without the native plugin a deployed Generic application
    # cannot start outside the Qt SDK at all, which would make the deployment look complete while remaining
    # SDK-dependent. The plugin is resolved through the same neutral resolver the QPA path uses, and its identity
    # comes from the target rather than from a platform-conditional file name.
    _hyremote_resolve_native_platform_payload(
        _native_platform_plugin_file _native_platform_plugin_name)
    _hyremote_runtime_deploy_dir(_runtime_deploy_dir)
    # The native platform plugin carries its own private Qt runtime dependency - the Linux xcb delegate is the
    # concrete case - so the shared bootstrap is given that plugin and produces the same closure it produces for the
    # QPA delegate.
    _hyremote_linux_private_runtime_bootstrap(
        "${_runtime_deploy_dir}" _linux_private_runtime_bootstrap "${_native_platform_plugin_name}")

    # The deployed platform plugin must find the deployed Qt runtime rather than the SDK it was built against, the
    # same relocation the QPA path performs for its delegate.
    set(_linux_platform_rpath_rewrite "")
    if(UNIX AND NOT APPLE)
        set(_linux_platform_rpath_rewrite
"file(RPATH_CHANGE
    FILE \"\${QT_DEPLOY_PREFIX}/\${QT_DEPLOY_PLUGINS_DIR}/platforms/${_native_platform_plugin_name}\"
    OLD_RPATH \"$ORIGIN/../../../.\"
    NEW_RPATH \"$ORIGIN/../../\${QT_DEPLOY_LIB_DIR}\"
)
")
    endif()

    _hyremote_security_private_runtime_install("${_runtime_deploy_dir}" _security_private_runtime_install)
    _hyremote_security_qt_deploy_options(_security_qt_deploy_options)
    set(_generic_script "${CMAKE_CURRENT_BINARY_DIR}/hyremote-generic-deploy-${target}-$<CONFIG>.cmake")
    file(GENERATE
        OUTPUT "${_generic_script}"
        CONTENT
"include(\"${QT_DEPLOY_SUPPORT}\")
file(INSTALL DESTINATION \"\${QT_DEPLOY_PREFIX}/\${QT_DEPLOY_PLUGINS_DIR}/platforms\" TYPE FILE FILES
    \"${_native_platform_plugin_file}\")
${_linux_platform_rpath_rewrite}file(INSTALL DESTINATION \"\${QT_DEPLOY_PREFIX}/\${QT_DEPLOY_PLUGINS_DIR}/generic\" TYPE FILE FILES
    \"${_generic_plugin_file}\")
file(INSTALL DESTINATION \"\${QT_DEPLOY_PREFIX}/${_runtime_deploy_dir}\" TYPE FILE FILES \"$<TARGET_FILE:HyRemote::RemoteAccess>\")
${_linux_private_runtime_bootstrap}${_security_private_runtime_install}qt6_deploy_runtime_dependencies(
${_security_qt_deploy_options}
    EXECUTABLE \"\${QT_DEPLOY_BIN_DIR}/$<TARGET_FILE_NAME:${target}>\"
    ADDITIONAL_MODULES
    \"\${QT_DEPLOY_PLUGINS_DIR}/platforms/${_native_platform_plugin_name}\"
    \"\${QT_DEPLOY_PLUGINS_DIR}/generic/${_generic_plugin_name}\"
    ADDITIONAL_LIBRARIES
    \"${_runtime_deploy_dir}/$<TARGET_FILE_NAME:HyRemote::RemoteAccess>\"
)
")

    set(${output_var} "${_generic_script}" PARENT_SCOPE)
endfunction()

function(hyremote_deploy)
    set(options QML QPA GENERIC)
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

    if(HYREMOTE_DEPLOY_GENERIC AND _hyremote_source_acquisition AND NOT TARGET hyremote-generic-plugin)
        message(FATAL_ERROR
            "hyremote_deploy(TARGET ${HYREMOTE_DEPLOY_TARGET} GENERIC) requires the current HyRemote source build "
            "to enable HYREMOTE_WITH_GENERIC_PLUGIN=ON; installed Generic metadata cannot satisfy a source deployment")
    endif()

    _hyremote_add_local_build_dependency(
        "${HYREMOTE_DEPLOY_TARGET}" HyRemote::RemoteAccess)
    if(HYREMOTE_DEPLOY_QPA)
        _hyremote_add_local_build_dependency(
            "${HYREMOTE_DEPLOY_TARGET}" HyRemote::QpaPlatform)
    endif()
    if(HYREMOTE_DEPLOY_GENERIC)
        _hyremote_add_local_build_dependency(
            "${HYREMOTE_DEPLOY_TARGET}" hyremote-generic-plugin)
    endif()
    if(HYREMOTE_DEPLOY_QML)
        get_property(_hyremote_qml_source_targets GLOBAL PROPERTY HYREMOTE_QML_SOURCE_DEPLOY_TARGETS)
        foreach(_hyremote_qml_source_target IN LISTS _hyremote_qml_source_targets)
            _hyremote_add_local_build_dependency(
                "${HYREMOTE_DEPLOY_TARGET}" "${_hyremote_qml_source_target}")
        endforeach()
    endif()

    if(HYREMOTE_DEPLOY_GENERIC AND HYREMOTE_DEPLOY_QPA)
        message(FATAL_ERROR
            "hyremote_deploy(TARGET ${HYREMOTE_DEPLOY_TARGET} GENERIC QPA) is not a supported combination: the "
            "Generic Plugin keeps the application's native platform integration, while Transparent QPA replaces it")
    endif()

    if(HYREMOTE_DEPLOY_GENERIC)
        if(NOT _hyremote_source_acquisition
           AND DEFINED HyRemote_GENERIC_AVAILABLE AND NOT HyRemote_GENERIC_AVAILABLE)
            message(FATAL_ERROR
                "hyremote_deploy(TARGET ${HYREMOTE_DEPLOY_TARGET} GENERIC) requested an SDK that was built without the Generic Plugin")
        endif()
        _hyremote_generate_generic_deploy_script(
            "${HYREMOTE_DEPLOY_TARGET}" _hyremote_supplemental_deploy_script)
    elseif(HYREMOTE_DEPLOY_QPA)
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