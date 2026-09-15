include_guard(GLOBAL)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(HYREMOTE_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/HyRemote")
set(HYREMOTE_PACKAGE_WITH_REMOTE_ACCESS FALSE)
if(TARGET hyremote-remoteaccess)
    set(HYREMOTE_PACKAGE_WITH_REMOTE_ACCESS TRUE)
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
    FILES "${PROJECT_SOURCE_DIR}/LICENSE"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/HyRemote/licenses"
)
