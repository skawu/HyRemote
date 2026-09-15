cmake_minimum_required(VERSION 3.21)

project(
    HyRemote
    VERSION 0.0.0
    DESCRIPTION "Qt Embedded Remote Access Framework"
    LANGUAGES C CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(HyRemoteProjectOptions)

include(CTest)

message(STATUS "Configuring HyRemote ${PROJECT_VERSION}")
message(STATUS "  Build core:          ${HYREMOTE_BUILD_CORE}")
message(STATUS "  Build RemoteAccess:  ${HYREMOTE_BUILD_REMOTE_ACCESS}")
message(STATUS "  Build tests:         ${HYREMOTE_BUILD_TESTS}")
message(STATUS "  Build examples:      ${HYREMOTE_BUILD_EXAMPLES}")
message(STATUS "  VNC backend:         ${HYREMOTE_WITH_VNC}")
message(STATUS "  Build spikes:        ${HYREMOTE_BUILD_SPIKES}")

# hyremote-core: the production session/frame/dispatch contract frozen by ARCH-01.
# UI-neutral and free of Qt/protocol/platform types.
if(HYREMOTE_BUILD_CORE)
    add_subdirectory(core)
endif()

# Product-facing C++ facade. Keep bare Core builds usable on hosts without Qt: when HyRemote is
# configured as the top-level project and Qt is unavailable, the product target is skipped with an
# explicit status message. A source-consumer subproject, however, is expected to provide its Qt SDK
# and fails if the product target was requested but Qt Core/Network cannot be resolved.
if(HYREMOTE_BUILD_REMOTE_ACCESS)
    if(NOT TARGET HyRemote::Core)
        message(FATAL_ERROR "HyRemote::RemoteAccess requires HYREMOTE_BUILD_CORE=ON")
    endif()

    if(NOT TARGET Qt6::Core OR NOT TARGET Qt6::Network)
        find_package(Qt6 6.8 QUIET COMPONENTS Core Network)
    endif()

    if(TARGET Qt6::Core AND TARGET Qt6::Network)
        add_subdirectory(remoteaccess)
    elseif(PROJECT_IS_TOP_LEVEL)
        message(STATUS "  RemoteAccess target skipped: Qt 6.8+ Core/Network not found")
    else()
        message(FATAL_ERROR
            "HYREMOTE_BUILD_REMOTE_ACCESS=ON requires Qt 6.8+ Core and Network. "
            "Find/provide the target Qt SDK before add_subdirectory(HyRemote), or disable the product target explicitly.")
    endif()
endif()

# Throwaway architecture spike harnesses. They are not part of the production build graph.
if(HYREMOTE_BUILD_SPIKES)
    add_subdirectory(spikes/capture)
    add_subdirectory(spikes/async-capture)
endif()

# Install/export is part of the SDK contract. A bare Core-only package remains valid; when the Qt
# product facade is present it is exported from the same HyRemote package and namespace.
if(TARGET hyremote-core)
    include(HyRemoteInstall)
endif()
