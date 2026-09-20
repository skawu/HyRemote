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
set(HYREMOTE_PACKAGE_QPA_QT_VERSION "6.8.3")
set(HYREMOTE_PACKAGE_QPA_PLUGIN_SUBDIR "")
set(HYREMOTE_PACKAGE_QPA_PLUGIN_FILENAME "")
if(TARGET hyremote-qpa-platform)
    set(HYREMOTE_PACKAGE_WITH_QPA TRUE)
    set(HYREMOTE_PACKAGE_QPA_PLUGIN_SUBDIR "${CMAKE_INSTALL_LIBDIR}/HyRemote/plugins/platforms")
    hyremote_target_artifact_name(hyremote-qpa-platform HYREMOTE_PACKAGE_QPA_PLUGIN_FILENAME)
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
