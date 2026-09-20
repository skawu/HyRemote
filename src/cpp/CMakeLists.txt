include(GenerateExportHeader)
include(GNUInstallDirs)

# `src/cpp` is the Embedded C++ integration frontend. The common HyRemote product runtime is owned
# by `src/runtime`; this directory contributes the public RemoteAccess facade and its tests/headers to
# that existing shared target instead of creating a second runtime personality.
#
# A consumer configure must still acquire HyRemote only once. If an installed HyRemote package was
# already loaded, adding this source tree would collide at the public alias and could mix SDK metadata.
if(TARGET HyRemote::RemoteAccess)
    message(FATAL_ERROR
        "HyRemote source acquisition conflict: HyRemote::RemoteAccess already exists; do not combine "
        "find_package(HyRemote) with add_subdirectory(HyRemote) in the same configure")
endif()
if(TARGET hyremote-remoteaccess)
    message(FATAL_ERROR
        "HyRemote source acquisition conflict: hyremote-remoteaccess already exists; do not combine "
        "find_package(HyRemote) with add_subdirectory(HyRemote) in the same configure")
endif()

# First migration step for #219: runtime target ownership moves now, while common implementation files
# are physically relocated in subsequent commits. Keeping target identity and output names stable avoids
# turning an ownership refactor into an SDK/deployment break.
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../runtime" "${CMAKE_CURRENT_BINARY_DIR}/runtime")

# Qt imported targets are directory scoped. The common runtime resolves optional Widgets/Quick targets
# in its own child scope, but Embedded C++ tests are configured from this directory and need the same
# targets visible here. Re-resolve only the capabilities the runtime actually enabled; this does not
# change the compiled product profile.
if(HYREMOTE_REMOTEACCESS_WITH_WIDGETS AND NOT TARGET Qt6::Widgets)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)
endif()
if(HYREMOTE_REMOTEACCESS_WITH_QUICK AND NOT TARGET Qt6::Quick)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Quick)
endif()

# Embedded C++ facade only. The implementation delegates target/transport construction to the common
# runtime and continues to own the public application-facing RemoteAccess ABI.
target_sources(hyremote-remoteaccess PRIVATE
    src/remote_access.cpp
)

target_include_directories(hyremote-remoteaccess
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

generate_export_header(hyremote-remoteaccess
    BASE_NAME HYREMOTE_REMOTEACCESS
    EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/generated/HyRemote/RemoteAccessExport.h
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
