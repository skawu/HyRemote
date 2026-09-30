include(GenerateExportHeader)
include(GNUInstallDirs)

# Common product runtime for all HyRemote integration frontends.
#
# The delivered HyRemoteRemoteAccess binary/target identity stays stable while implementation ownership
# lives here. Integration frontends contribute only their entry/facade sources and consume this runtime.
if(TARGET hyremote-remoteaccess OR TARGET HyRemote::RemoteAccess)
    message(FATAL_ERROR "HyRemote common runtime target already exists")
endif()

add_library(hyremote-remoteaccess SHARED
    src/access_instance.cpp
    src/detail/component_factories.cpp
    src/detail/runtime_notifications.cpp
    src/detail/runtime_diagnostics.cpp
    src/detail/security_descriptor.cpp
    src/detail/composition_support.cpp
    src/detail/listener_binding.cpp
)
add_library(HyRemote::RemoteAccess ALIAS hyremote-remoteaccess)

set_target_properties(hyremote-remoteaccess PROPERTIES
    EXPORT_NAME RemoteAccess
    OUTPUT_NAME HyRemoteRemoteAccess
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

if(UNIX AND NOT APPLE)
    set_property(TARGET hyremote-remoteaccess PROPERTY BUILD_RPATH "$ORIGIN")
    set_property(TARGET hyremote-remoteaccess PROPERTY BUILD_RPATH_USE_ORIGIN TRUE)
    set_property(TARGET hyremote-remoteaccess PROPERTY INSTALL_RPATH "$ORIGIN")
endif()

target_compile_features(hyremote-remoteaccess PUBLIC cxx_std_17)

generate_export_header(hyremote-remoteaccess
    BASE_NAME HYREMOTE_REMOTEACCESS
    EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/generated/HyRemote/RemoteAccessExport.h
)

target_include_directories(hyremote-remoteaccess
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

target_link_libraries(hyremote-remoteaccess
    PUBLIC
        Qt6::Core
        Qt6::Network
    PRIVATE
        HyRemote::Core
)

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
        target_sources(hyremote-remoteaccess PRIVATE
            src/transport/vnc_auth.cpp
        )
        target_link_libraries(hyremote-remoteaccess PRIVATE OpenSSL::Crypto)
        target_compile_definitions(hyremote-remoteaccess PRIVATE HYREMOTE_HAS_TRANSPORT_SECURITY=1)
    endif()
endif()

set(HYREMOTE_REMOTEACCESS_WITH_WIDGETS FALSE CACHE INTERNAL
    "Common runtime was built with the Qt Widgets target adapter" FORCE)
if(HYREMOTE_BUILD_WIDGETS_ADAPTER)
    if(NOT TARGET Qt6::Widgets)
        find_package(Qt6 6.8 QUIET COMPONENTS Widgets)
    endif()

    if(TARGET Qt6::Widgets)
        target_sources(hyremote-remoteaccess PRIVATE
            src/widgets/widget_target.cpp
            src/widgets/widget_surface_scope.cpp
        )
        target_link_libraries(hyremote-remoteaccess PRIVATE Qt6::Widgets)
        target_compile_definitions(hyremote-remoteaccess PRIVATE HYREMOTE_HAS_WIDGETS_ADAPTER=1)
        set(HYREMOTE_REMOTEACCESS_WITH_WIDGETS TRUE CACHE INTERNAL
            "Common runtime was built with the Qt Widgets target adapter" FORCE)
    else()
        message(STATUS "  Widgets adapter skipped: Qt 6.8+ Widgets not found")
    endif()
endif()

set(HYREMOTE_REMOTEACCESS_WITH_QUICK FALSE CACHE INTERNAL
    "Common runtime was built with the Qt Quick target adapter" FORCE)
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
            "Common runtime contains the Qt Quick target adapter" FORCE)
    else()
        message(STATUS "  Quick adapter skipped: Qt 6.8+ Quick not found")
    endif()
endif()

# Pointer input enters Qt at the same QPA/window-system boundary used by platform plugins. Keep the
# private dependency isolated to one Runtime translation unit; integration frontends remain peers and
# never need to include or understand Qt private headers themselves.
if(HYREMOTE_REMOTEACCESS_WITH_WIDGETS OR HYREMOTE_REMOTEACCESS_WITH_QUICK)
    if(NOT TARGET Qt6::Gui)
        find_package(Qt6 6.8 QUIET COMPONENTS Gui)
    endif()
    if(NOT TARGET Qt6::GuiPrivate)
        message(FATAL_ERROR
            "HyRemote Qt input ingress requires the Qt Gui private development target (Qt6::GuiPrivate) "
            "matching the Qt build used by the application")
    endif()

    target_sources(hyremote-remoteaccess PRIVATE
        src/detail/qt_window_system_input.cpp
    )
    target_link_libraries(hyremote-remoteaccess PRIVATE Qt6::Gui Qt6::GuiPrivate)
endif()

set(HYREMOTE_AUTOMATIC_RUNTIME_AVAILABLE FALSE CACHE INTERNAL
    "Common runtime contains the application-level automatic access controller" FORCE)
if(HYREMOTE_REMOTEACCESS_WITH_WIDGETS
   OR HYREMOTE_REMOTEACCESS_WITH_QUICK
   OR HYREMOTE_WITH_GENERIC_PLUGIN
   OR HYREMOTE_WITH_QPA_PROXY)
    if(NOT TARGET Qt6::Gui)
        find_package(Qt6 6.8 QUIET COMPONENTS Gui)
    endif()

    if(TARGET Qt6::Gui)
        target_sources(hyremote-remoteaccess PRIVATE
            src/automatic/application_surface_model.cpp
            src/automatic/composite_target.cpp
            src/automatic/composite_target_routing.cpp
            src/automatic/interactive_composite_target.cpp
            src/automatic/automatic_access_controller.cpp
        )
        target_link_libraries(hyremote-remoteaccess PRIVATE Qt6::Gui)
        set(HYREMOTE_AUTOMATIC_RUNTIME_AVAILABLE TRUE CACHE INTERNAL
            "Common runtime contains the application-level automatic access controller" FORCE)
    elseif(HYREMOTE_WITH_GENERIC_PLUGIN OR HYREMOTE_WITH_QPA_PROXY)
        message(FATAL_ERROR
            "HyRemote zero-code integration requires Qt 6.8+ Gui for application-surface discovery")
    endif()
endif()

if(HYREMOTE_BUILD_TESTS)
    add_executable(hyremote-runtime-diagnostics-test
        tests/test_runtime_diagnostics.cpp
    )
    target_compile_features(hyremote-runtime-diagnostics-test PRIVATE cxx_std_17)
    target_include_directories(hyremote-runtime-diagnostics-test PRIVATE src)
    target_link_libraries(hyremote-runtime-diagnostics-test PRIVATE Qt6::Core HyRemote::RemoteAccess)
    add_test(NAME hyremote-runtime-diagnostics-test COMMAND hyremote-runtime-diagnostics-test)
    set_tests_properties(hyremote-runtime-diagnostics-test PROPERTIES TIMEOUT 15)

    add_subdirectory(tests)
endif()

install(
    TARGETS hyremote-remoteaccess
    EXPORT HyRemoteTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/HyRemote/RemoteAccessExport.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/HyRemote"
)
