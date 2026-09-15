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
