include(GNUInstallDirs)

# `src/integrations/cpp` is the Embedded C++ integration frontend. The common HyRemote product runtime is owned
# by `src/runtime`; this directory contributes only the public RemoteAccess facade and its tests/headers.
if(NOT HYREMOTE_BUILD_CPP_API)
    return()
endif()

if(NOT TARGET hyremote-remoteaccess OR NOT TARGET HyRemote::RemoteAccess)
    message(FATAL_ERROR
        "Embedded C++ integration requires the root build to create the common HyRemote runtime first")
endif()

# Qt imported targets are directory scoped. The common runtime resolves optional Widgets/Quick targets
# in its own child scope, but Embedded C++ tests are configured from this directory and need the same
# targets visible here. Re-resolve only the capabilities the runtime actually enabled.
if(HYREMOTE_REMOTEACCESS_WITH_WIDGETS AND NOT TARGET Qt6::Widgets)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)
endif()
if(HYREMOTE_REMOTEACCESS_WITH_QUICK AND NOT TARGET Qt6::Quick)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Quick)
endif()

# Embedded C++ facade only. The implementation delegates target/transport construction to the Common Runtime.
target_sources(hyremote-remoteaccess PRIVATE
    src/remote_access.cpp
)

target_include_directories(hyremote-remoteaccess
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.h"
)

if(HYREMOTE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
