cmake_minimum_required(VERSION 3.21)

project(
    HyRemote
    VERSION 0.0.0
    DESCRIPTION "Qt Remote Access Framework"
    LANGUAGES C CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(HyRemoteProjectOptions)
include(HyRemoteReleaseProfile)
include(HyRemoteInstall)

# Keep the development branch on a sentinel version. Formal four-part milestone versions are injected
# only by release/v* branches through HyRemoteReleaseProfile.cmake.
hyremote_apply_release_profile()

add_subdirectory(src/core core)
add_subdirectory(src/remoteaccess remoteaccess)

if(HYREMOTE_BUILD_QML_API)
    add_subdirectory(integrations/qml/HyRemote qml/HyRemote)
endif()

if(HYREMOTE_WITH_QPA_PROXY)
    add_subdirectory(integrations/qpa qpa)
endif()

if(HYREMOTE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(HYREMOTE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(HYREMOTE_BUILD_SPIKES)
    add_subdirectory(research/capture spikes/capture)
    add_subdirectory(research/async-capture spikes/async-capture)
endif()

hyremote_install_sdk()
