include_guard(GLOBAL)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(HYREMOTE_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/HyRemote")
set(HYREMOTE_PACKAGE_WITH_REMOTE_ACCESS FALSE)
if(TARGET hyremote-remoteaccess)
    set(HYREMOTE_PACKAGE_WITH_REMOTE_ACCESS TRUE)
endif()

# Widgets and Quick adapters are private implementation inside the shared RemoteAccess runtime. Each
# application resolves only the Qt UI modules it actually uses.
set(HYREMOTE_PACKAGE_WITH_QML FALSE)
set(HYREMOTE_PACKAGE_QML_IMPORT_SUBDIR "${CMAKE_INSTALL_LIBDIR}/qml")
set(HYREMOTE_PACKAGE_QML_BACKING_SUBDIR "")
set(HYREMOTE_PACKAGE_QML_BACKING_FILENAME "")
# The installed package must describe the artifact a target actually installs. CMake composes a target's artifact
# name as <PREFIX><OUTPUT_NAME or target name><SUFFIX>, and a target may set any of those properties: Qt's QML
# module support, for example, sets PREFIX to empty on the QML backing library, so the emitted name is
# `hyremote-qml.dll` on Windows even where CMAKE_SHARED_LIBRARY_PREFIX is `lib`. Reading the target's own artifact
# identity is the single source of truth; the toolchain-global CMAKE_SHARED_* values describe only CMake's default
# for the target's type, so they are used for the properties the target leaves unset - which is how CMake itself
# resolves the name - and never in place of a property the target sets.
function(hyremote_target_artifact_name target out_var)
    get_target_property(_artifact_type ${target} TYPE)
    get_target_property(_artifact_output ${target} OUTPUT_NAME)
    get_target_property(_artifact_prefix ${target} PREFIX)
    get_target_property(_artifact_suffix ${target} SUFFIX)

    if(_artifact_output MATCHES "-NOTFOUND$")
        set(_artifact_output "${target}")
    endif()
    if(_artifact_prefix MATCHES "-NOTFOUND$")
        if(_artifact_type STREQUAL "MODULE_LIBRARY")
            set(_artifact_prefix "${CMAKE_SHARED_MODULE_PREFIX}")
        else()
            set(_artifact_prefix "${CMAKE_SHARED_LIBRARY_PREFIX}")
        endif()
    endif()
    if(_artifact_suffix MATCHES "-NOTFOUND$")
        if(_artifact_type STREQUAL "MODULE_LIBRARY")
            set(_artifact_suffix "${CMAKE_SHARED_MODULE_SUFFIX}")
        else()
            set(_artifact_suffix "${CMAKE_SHARED_LIBRARY_SUFFIX}")
        endif()
    endif()

    set(${out_var} "${_artifact_prefix}${_artifact_output}${_artifact_suffix}" PARENT_SCOPE)
endfunction()

if(TARGET hyremote-qml)
    set(HYREMOTE_PACKAGE_WITH_QML TRUE)
    if(WIN32)
        set(HYREMOTE_PACKAGE_QML_BACKING_SUBDIR "${CMAKE_INSTALL_BINDIR}")
    else()
        set(HYREMOTE_PACKAGE_QML_BACKING_SUBDIR "${CMAKE_INSTALL_LIBDIR}")
    endif()
    hyremote_target_artifact_name(hyremote-qml HYREMOTE_PACKAGE_QML_BACKING_FILENAME)
endif()

# Transparent QPA is package payload, not a C++ link target. Export only availability, exact Qt ABI
# metadata and the installed plugin location used internally by hyremote_deploy(... QPA). V1 has one
# fixed shared RemoteAccess runtime, so no static/shared QPA personality flag is published.
set(HYREMOTE_PACKAGE_WITH_QPA FALSE)
# The exact Qt this package was built against, taken from the Qt CMake found rather than written down. A literal
# here is right only on the machine it was typed on and silently wrong everywhere else, which turns "exact Qt
# compatibility metadata" into a guess. `Qt6_VERSION` is the same fact the project already prints and gates on.
set(HYREMOTE_PACKAGE_QPA_QT_VERSION "${Qt6_VERSION}")
set(HYREMOTE_PACKAGE_QPA_PLUGIN_SUBDIR "")
set(HYREMOTE_PACKAGE_QPA_PLUGIN_FILENAME "")
if(TARGET hyremote-qpa-platform)
    set(HYREMOTE_PACKAGE_WITH_QPA TRUE)
    set(HYREMOTE_PACKAGE_QPA_PLUGIN_SUBDIR "${CMAKE_INSTALL_LIBDIR}/HyRemote/plugins/platforms")
    hyremote_target_artifact_name(hyremote-qpa-platform HYREMOTE_PACKAGE_QPA_PLUGIN_FILENAME)
endif()

# Generic Plugin is the fourth peer frontend and, like QPA, a package payload rather than a C++ link target. Export
# availability and the installed plugin location so the package describes the zero-code QGenericPlugin payload it
# ships, without introducing another link target or runtime personality.
set(HYREMOTE_PACKAGE_WITH_GENERIC FALSE)
set(HYREMOTE_PACKAGE_GENERIC_PLUGIN_SUBDIR "")
set(HYREMOTE_PACKAGE_GENERIC_PLUGIN_FILENAME "")
if(TARGET hyremote-generic-plugin)
    set(HYREMOTE_PACKAGE_WITH_GENERIC TRUE)
    set(HYREMOTE_PACKAGE_GENERIC_PLUGIN_SUBDIR "${CMAKE_INSTALL_LIBDIR}/HyRemote/plugins/generic")
    hyremote_target_artifact_name(hyremote-generic-plugin HYREMOTE_PACKAGE_GENERIC_PLUGIN_FILENAME)
endif()

# One canonical human-readable install manifest for every CMake install entry point. The source
# identity uses the same fail-closed rule as Runtime build diagnostics: an enclosing consumer repository
# is never accepted as HyRemote's source SHA. file(GENERATE) keeps BUILD_TYPE truthful for both single-
# and multi-config generators, then install() places the exact same file at the install root.
set(_hyremote_manifest_source_sha "unknown")
execute_process(
    COMMAND git -C "${PROJECT_SOURCE_DIR}" rev-parse --show-toplevel
    RESULT_VARIABLE _hyremote_manifest_root_rc
    OUTPUT_VARIABLE _hyremote_manifest_git_root
    ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
if(_hyremote_manifest_root_rc EQUAL 0 AND NOT _hyremote_manifest_git_root STREQUAL "")
    file(REAL_PATH "${PROJECT_SOURCE_DIR}" _hyremote_manifest_source_root)
    file(REAL_PATH "${_hyremote_manifest_git_root}" _hyremote_manifest_git_root)
    if(_hyremote_manifest_source_root STREQUAL _hyremote_manifest_git_root)
        execute_process(
            COMMAND git -C "${PROJECT_SOURCE_DIR}" rev-parse HEAD
            RESULT_VARIABLE _hyremote_manifest_git_rc
            OUTPUT_VARIABLE _hyremote_manifest_git_out
            ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(_hyremote_manifest_git_rc EQUAL 0 AND NOT _hyremote_manifest_git_out STREQUAL "")
            set(_hyremote_manifest_source_sha "${_hyremote_manifest_git_out}")
        endif()
    endif()
endif()

set(_hyremote_manifest_arch "${CMAKE_HOST_SYSTEM_PROCESSOR}")
if(_hyremote_manifest_arch STREQUAL "")
    set(_hyremote_manifest_arch "$ENV{PROCESSOR_ARCHITECTURE}")
endif()
if(_hyremote_manifest_arch STREQUAL "")
    set(_hyremote_manifest_arch "${CMAKE_SYSTEM_PROCESSOR}")
endif()
if(_hyremote_manifest_arch STREQUAL "")
    set(_hyremote_manifest_arch "unknown")
endif()
set(_hyremote_manifest_qt_version "${Qt6_VERSION}")
if(_hyremote_manifest_qt_version STREQUAL "")
    set(_hyremote_manifest_qt_version "unknown")
endif()

set(_hyremote_manifest_file "${CMAKE_CURRENT_BINARY_DIR}/HYREMOTE-MANIFEST-$<CONFIG>.txt")
file(GENERATE OUTPUT "${_hyremote_manifest_file}" CONTENT
"SOURCE_SHA=${_hyremote_manifest_source_sha}\nOS=${CMAKE_HOST_SYSTEM_NAME}\nARCH=${_hyremote_manifest_arch}\nQT_VERSION=${_hyremote_manifest_qt_version}\nBUILD_TYPE=$<CONFIG>\nCPP=${HYREMOTE_BUILD_CPP_API}\nQML=${HYREMOTE_BUILD_QML_API}\nGENERIC=${HYREMOTE_WITH_GENERIC_PLUGIN}\nQPA=${HYREMOTE_WITH_QPA_PROXY}\nLISTENER_DEFAULT=0.0.0.0:${HYREMOTE_DEFAULT_PORT}\nAUTHENTICATION_ENABLED=OFF\nAUTHENTICATION_PROFILE=none\nTRANSPORT_ENCRYPTION_ENABLED=OFF\nTRANSPORT_ENCRYPTION_PROFILE=none\nREMOTE_INPUT_DEFAULT=OFF\nTRANSPORT_SECURITY_CAPABILITY=${HYREMOTE_TRANSPORT_SECURITY_AVAILABLE}\n")
install(FILES "${_hyremote_manifest_file}" DESTINATION "." RENAME "HYREMOTE-MANIFEST.txt")

# The transport security runtime, installed as a package-owned private payload beside the shared runtime. It is
# resolved from the OpenSSL this build links against (cmake/HyRemoteProjectOptions.cmake), because the deploy helper
# runs before these rules are processed. There is no consumer target, no link interface and no OpenSSL dependency a
# consumer has to resolve.
if(HYREMOTE_PACKAGE_WITH_SECURITY_RUNTIME)
    foreach(_hyremote_security_source IN LISTS HYREMOTE_PACKAGE_SECURITY_RUNTIME_SOURCE_FILES)
        install(FILES "${_hyremote_security_source}" DESTINATION "${CMAKE_INSTALL_BINDIR}")
    endforeach()
endif()

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/HyRemoteConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/HyRemoteConfig.cmake"
    INSTALL_DESTINATION "${HYREMOTE_INSTALL_CMAKEDIR}"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/HyRemoteConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
)

install(
    EXPORT HyRemoteTargets
    FILE HyRemoteTargets.cmake
    NAMESPACE HyRemote::
    DESTINATION "${HYREMOTE_INSTALL_CMAKEDIR}"
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/HyRemoteConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/HyRemoteConfigVersion.cmake"
        "${CMAKE_CURRENT_LIST_DIR}/HyRemoteDeploy.cmake"
    DESTINATION "${HYREMOTE_INSTALL_CMAKEDIR}"
)

install(
    FILES
        "${PROJECT_SOURCE_DIR}/LICENSE"
        "${PROJECT_SOURCE_DIR}/NOTICE.md"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/HyRemote/licenses"
)
