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

# Implementation targets are intentionally not frozen during bootstrap.
# They will be added after SPIKE-01 and ARCH-01 establish the first stable
# capture/frame/session boundaries.

message(STATUS "Configuring HyRemote ${PROJECT_VERSION}")
message(STATUS "  Build tests:    ${HYREMOTE_BUILD_TESTS}")
message(STATUS "  Build examples: ${HYREMOTE_BUILD_EXAMPLES}")
message(STATUS "  VNC backend:    ${HYREMOTE_WITH_VNC}")
message(STATUS "  Build spikes:   ${HYREMOTE_BUILD_SPIKES}")

# Throwaway architecture spike harnesses. They are not part of the core build
# graph, define no public API, and are expected to be deleted once the captured
# architecture decisions are implemented. See docs/capture-spike.md.
if(HYREMOTE_BUILD_SPIKES)
    add_subdirectory(spikes/capture)
    add_subdirectory(spikes/async-capture)
endif()
