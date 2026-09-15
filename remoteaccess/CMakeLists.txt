include(GenerateExportHeader)

add_library(hyremote-remoteaccess
    src/remote_access.cpp
    src/detail/component_factories.cpp
)
add_library(HyRemote::RemoteAccess ALIAS hyremote-remoteaccess)

target_compile_features(hyremote-remoteaccess PUBLIC cxx_std_17)

target_include_directories(hyremote-remoteaccess
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
        $<INSTALL_INTERFACE:include>
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

set_target_properties(hyremote-remoteaccess PROPERTIES
    OUTPUT_NAME HyRemoteRemoteAccess
)

if(HYREMOTE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
