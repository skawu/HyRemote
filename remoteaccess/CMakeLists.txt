include(GenerateExportHeader)
include(GNUInstallDirs)

add_library(hyremote-remoteaccess
    src/remote_access.cpp
    src/detail/component_factories.cpp
)
add_library(HyRemote::RemoteAccess ALIAS hyremote-remoteaccess)
set_target_properties(hyremote-remoteaccess PROPERTIES
    EXPORT_NAME RemoteAccess
    OUTPUT_NAME HyRemoteRemoteAccess
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
