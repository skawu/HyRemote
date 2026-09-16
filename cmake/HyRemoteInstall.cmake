include_guard(GLOBAL)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(HYREMOTE_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/HyRemote")
set(HYREMOTE_PACKAGE_WITH_REMOTE_ACCESS FALSE)
if(TARGET hyremote-remoteaccess)
    set(HYREMOTE_PACKAGE_WITH_REMOTE_ACCESS TRUE)
endif()

set(HYREMOTE_PACKAGE_WITH_WIDGETS FALSE)
if(HYREMOTE_REMOTEACCESS_WITH_WIDGETS)
    set(HYREMOTE_PACKAGE_WITH_WIDGETS TRUE)
endif()

set(HYREMOTE_PACKAGE_WITH_QUICK FALSE)
if(HYREMOTE_REMOTEACCESS_WITH_QUICK)
    set(HYREMOTE_PACKAGE_WITH_QUICK TRUE)
endif()

set(HYREMOTE_PACKAGE_WITH_QML FALSE)
if(TARGET hyremote-qml)
    set(HYREMOTE_PACKAGE_WITH_QML TRUE)
endif()
set(HYREMOTE_PACKAGE_QML_IMPORT_SUBDIR "${CMAKE_INSTALL_LIBDIR}/qml")

# Transparent QPA is separately version-coupled. Export only package metadata and the installed
# MODULE target location; consuming applications do not link it. The deployment helper uses this
# information to copy the exact SDK-built proxy into the application's Qt plugin tree.
set(HYREMOTE_PACKAGE_WITH_QPA FALSE)
set(HYREMOTE_PACKAGE_QPA_QT_VERSION "6.8.3")
set(HYREMOTE_PACKAGE_QPA_SHARED_RUNTIME FALSE)
if(TARGET hyremote-qpa-platform)
    set(HYREMOTE_PACKAGE_WITH_QPA TRUE)
    if(BUILD_SHARED_LIBS)
        set(HYREMOTE_PACKAGE_QPA_SHARED_RUNTIME TRUE)
    endif()
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
