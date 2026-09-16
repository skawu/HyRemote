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
if(TARGET hyremote-qml)
    set(HYREMOTE_PACKAGE_WITH_QML TRUE)
endif()
set(HYREMOTE_PACKAGE_QML_IMPORT_SUBDIR "${CMAKE_INSTALL_LIBDIR}/qml")

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
    set(HYREMOTE_PACKAGE_QPA_PLUGIN_FILENAME
        "${CMAKE_SHARED_MODULE_PREFIX}qhyremote${CMAKE_SHARED_MODULE_SUFFIX}")
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
