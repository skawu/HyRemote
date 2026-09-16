include(GenerateExportHeader)
include(GNUInstallDirs)

# V1 primary product artifact: applications link one HyRemote shared library and use the public
# C++ facade. Core is statically composed behind this boundary and is not a second installed product
# target; QPA reuses this same runtime through its platform MODULE.
add_library(hyremote-remoteaccess SHARED
    src/remote_access.cpp
    src/detail/component_factories.cpp
)
add_library(HyRemote::RemoteAccess ALIAS hyremote-remoteaccess)
set_target_properties(hyremote-remoteaccess PROPERTIES
    EXPORT_NAME RemoteAccess
    OUTPUT_NAME HyRemoteRemoteAccess
    POSITION_INDEPENDENT_CODE ON
)

target_compile_features(hyremote-remoteaccess PUBLIC cxx_std_17)

target_include_directories(hyremote-remoteaccess
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(hyremote-remoteaccess
    PUBLIC
        Qt6::Core
        Qt6::Network
    PRIVATE
        HyRemote::Core
)

# V0.0.1 correctness transport. It is intentionally an internal implementation detail of
# RemoteAccess rather than a public application target. The backend uses Qt Network, which is
# already part of the facade dependency surface, and introduces no protocol type into Core/API.
if(HYREMOTE_WITH_VNC)
    target_sources(hyremote-remoteaccess PRIVATE
        src/transport/rfb_transport.cpp
    )
    target_compile_definitions(hyremote-remoteaccess PRIVATE HYREMOTE_HAS_RFB_TRANSPORT=1)
endif()

set(HYREMOTE_REMOTEACCESS_WITH_WIDGETS FALSE CACHE INTERNAL
    "RemoteAccess was built with the Qt Widgets adapter" FORCE)
if(HYREMOTE_BUILD_WIDGETS_ADAPTER)
    if(NOT TARGET Qt6::Widgets)
        find_package(Qt6 6.8 QUIET COMPONENTS Widgets)
    endif()

    if(TARGET Qt6::Widgets)
        target_sources(hyremote-remoteaccess PRIVATE
            src/widgets/widget_target.cpp
        )
        target_link_libraries(hyremote-remoteaccess PRIVATE Qt6::Widgets)
        target_compile_definitions(hyremote-remoteaccess PRIVATE HYREMOTE_HAS_WIDGETS_ADAPTER=1)
        set(HYREMOTE_REMOTEACCESS_WITH_WIDGETS TRUE CACHE INTERNAL
            "RemoteAccess was built with the Qt Widgets adapter" FORCE)
    else()
        message(STATUS "  Widgets adapter skipped: Qt 6.8+ Widgets not found")
    endif()
endif()

set(HYREMOTE_REMOTEACCESS_WITH_QUICK FALSE CACHE INTERNAL
    "RemoteAccess was built with the Qt Quick adapter" FORCE)
if(HYREMOTE_BUILD_QUICK_ADAPTER)
    if(NOT TARGET Qt6::Quick)
        find_package(Qt6 6.8 QUIET COMPONENTS Quick)
    endif()

    if(TARGET Qt6::Quick)
        target_sources(hyremote-remoteaccess PRIVATE
            src/quick/quick_target.cpp
        )
        target_link_libraries(hyremote-remoteaccess PRIVATE Qt6::Quick)
        target_compile_definitions(hyremote-remoteaccess PRIVATE HYREMOTE_HAS_QUICK_ADAPTER=1)
        set(HYREMOTE_REMOTEACCESS_WITH_QUICK TRUE CACHE INTERNAL
            "RemoteAccess was built with the Qt Quick adapter" FORCE)
    else()
        message(STATUS "  Quick adapter skipped: Qt 6.8+ Quick not found")
    endif()
endif()

generate_export_header(hyremote-remoteaccess
    BASE_NAME HYREMOTE_REMOTEACCESS
    EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/generated/HyRemote/RemoteAccessExport.h
)

install(
    TARGETS hyremote-remoteaccess
    EXPORT HyRemoteTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.h"
)

install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/HyRemote/RemoteAccessExport.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/HyRemote"
)

if(HYREMOTE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
