include(GenerateExportHeader)
include(GNUInstallDirs)

# V1 is one runtime acquisition per consumer configure. If a parent already loaded an installed
# HyRemote package, adding the source tree as a second acquisition would otherwise collide at the
# namespace alias and could leave optional package metadata from a different SDK in scope.
if(TARGET HyRemote::RemoteAccess)
    message(FATAL_ERROR
        "HyRemote source acquisition conflict: HyRemote::RemoteAccess already exists; do not combine "
        "find_package(HyRemote) with add_subdirectory(HyRemote) in the same configure")
endif()

# V1 primary product artifact: applications link one HyRemote shared library and use the public
# C++ facade. Core is statically composed behind this boundary and is not a second installed product
# target; QPA reuses this same runtime through its platform MODULE.
add_library(hyremote-remoteaccess SHARED
    src/remote_access.cpp
    src/detail/component_factories.cpp
    src/detail/security_descriptor.cpp
    # Source-private composition seam for the QPA payload. It carries the Core-owned operations
    # the QPA composite path needs, so the payload never links Core directly (see the repository-layout gate).
    # The header is not installed; only the three explicitly annotated bridge symbols live in this shared runtime.
    src/detail/qpa_composition_seam.cpp
)
add_library(HyRemote::RemoteAccess ALIAS hyremote-remoteaccess)
set_target_properties(hyremote-remoteaccess PROPERTIES
    EXPORT_NAME RemoteAccess
    OUTPUT_NAME HyRemoteRemoteAccess
    POSITION_INDEPENDENT_CODE ON
    # #164: the installed shared runtime is an explicit-export ABI. Unannotated implementation
    # symbols stay local on ELF/Mach-O, matching Windows' default DLL behaviour. The public facade
    # and the unavoidable source-private QPA payload bridge are annotated deliberately.
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)

if(UNIX AND NOT APPLE)
    set_property(TARGET hyremote-remoteaccess PROPERTY BUILD_RPATH "$ORIGIN")
    set_property(TARGET hyremote-remoteaccess PROPERTY BUILD_RPATH_USE_ORIGIN TRUE)
    set_property(TARGET hyremote-remoteaccess PROPERTY INSTALL_RPATH "$ORIGIN")
endif()

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

# Source tests replace private factories so they can exercise lifecycle/error paths deterministically
# without opening real listeners. Export those functions only in a test-enabled build tree; the
# BUILD_INTERFACE definition is never present in an installed SDK, and a normal/release build with
# HYREMOTE_BUILD_TESTS=OFF therefore cannot advertise these seams from the shared library.
if(HYREMOTE_BUILD_TESTS)
    target_compile_definitions(hyremote-remoteaccess
        PUBLIC $<BUILD_INTERFACE:HYREMOTE_ENABLE_PRIVATE_TEST_EXPORTS=1>
    )
endif()

if(HYREMOTE_WITH_VNC)
    target_sources(hyremote-remoteaccess PRIVATE
        src/transport/rfb_transport.cpp
    )
    target_compile_definitions(hyremote-remoteaccess PRIVATE HYREMOTE_HAS_RFB_TRANSPORT=1)

    if(HYREMOTE_TRANSPORT_SECURITY_AVAILABLE)
        # Transport security is a private implementation capability of the one shared RemoteAccess runtime.
        # The consumer supplies OpenSSL through the normal CMake environment; HyRemote exposes no provider
        # selector or bundled crypto toolchain and never exports OpenSSL as an SDK application target.
        target_sources(hyremote-remoteaccess PRIVATE
            src/transport/vnc_auth.cpp
        )
        target_link_libraries(hyremote-remoteaccess PRIVATE OpenSSL::Crypto OpenSSL::SSL)
        target_compile_definitions(hyremote-remoteaccess PRIVATE HYREMOTE_HAS_TRANSPORT_SECURITY=1)
    endif()
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
