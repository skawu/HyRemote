include(GNUInstallDirs)

# Common product runtime for all HyRemote integration frontends.
#
# The delivered HyRemoteRemoteAccess binary/target identity stays stable while implementation ownership
# moves out of the Embedded C++ frontend. `src/cpp` contributes only the public facade; shared Qt target,
# transport, security and composition implementation lives here.
if(TARGET hyremote-remoteaccess OR TARGET HyRemote::RemoteAccess)
    message(FATAL_ERROR "HyRemote common runtime target already exists")
endif()

add_library(hyremote-remoteaccess SHARED
    src/access_instance.cpp
    src/detail/component_factories.cpp
    src/detail/security_descriptor.cpp
    # Transitional bridge for the current QPA composite implementation. It disappears when the
    # QPA payload switches fully to the runtime-owned automatic composition path.
    src/detail/qpa_composition_seam.cpp
)
add_library(HyRemote::RemoteAccess ALIAS hyremote-remoteaccess)

# Preserve the established output location even though target ownership moved into src/runtime.
get_filename_component(_hyremote_runtime_output_dir "${CMAKE_CURRENT_BINARY_DIR}/.." ABSOLUTE)
set_target_properties(hyremote-remoteaccess PROPERTIES
    EXPORT_NAME RemoteAccess
    OUTPUT_NAME HyRemoteRemoteAccess
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
    ARCHIVE_OUTPUT_DIRECTORY "${_hyremote_runtime_output_dir}"
    LIBRARY_OUTPUT_DIRECTORY "${_hyremote_runtime_output_dir}"
    RUNTIME_OUTPUT_DIRECTORY "${_hyremote_runtime_output_dir}"
)
unset(_hyremote_runtime_output_dir)

if(UNIX AND NOT APPLE)
    set_property(TARGET hyremote-remoteaccess PROPERTY BUILD_RPATH "$ORIGIN")
    set_property(TARGET hyremote-remoteaccess PROPERTY BUILD_RPATH_USE_ORIGIN TRUE)
    set_property(TARGET hyremote-remoteaccess PROPERTY INSTALL_RPATH "$ORIGIN")
endif()

target_compile_features(hyremote-remoteaccess PUBLIC cxx_std_17)

target_include_directories(hyremote-remoteaccess
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    # Transitional build-tree seam for the current QPA implementation. QPA still consumes private
    # runtime declarations until it switches to AccessController. Nothing under src/runtime/src is
    # exposed by the installed SDK.
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

# Source tests replace private factories so they can exercise lifecycle/error paths deterministically.
# This definition remains build-tree only and is never an installed API commitment.
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
        target_link_libraries(hyremote-remoteaccess PRIVATE OpenSSL::Crypto OpenSSL::SSL)
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
            "Common runtime was built with the Qt Quick target adapter" FORCE)
    else()
        message(STATUS "  Quick adapter skipped: Qt 6.8+ Quick not found")
    endif()
endif()

# Application-level zero-code composition is part of the common runtime, but a Core/Network-only
# RFB build must remain valid. Compile this layer only when a GUI adapter is actually present or a
# zero-code frontend explicitly requests it.
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

install(
    TARGETS hyremote-remoteaccess
    EXPORT HyRemoteTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
