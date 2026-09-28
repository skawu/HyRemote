include(GNUInstallDirs)

if(NOT TARGET HyRemote::RemoteAccess)
    message(FATAL_ERROR
        "HYREMOTE_WITH_GENERIC_PLUGIN=ON requires the common HyRemote runtime")
endif()
if(NOT HYREMOTE_AUTOMATIC_RUNTIME_AVAILABLE)
    message(FATAL_ERROR
        "HyRemote Generic Plugin requires the common automatic-access runtime")
endif()
if(NOT TARGET Qt6::Gui)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Gui)
endif()

add_library(hyremote-generic-plugin MODULE
    hyremote_generic_plugin.cpp
    generic_config.cpp
)
set_target_properties(hyremote-generic-plugin PROPERTIES
    AUTOMOC ON
    OUTPUT_NAME qhyremote
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/plugins/generic"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/plugins/generic"
)

# This payload is copied into a deployed tree, and a copy keeps the runtime path it was built with. Like the QPA
# payload it therefore reserves one package-owned segment: `$ORIGIN/../../../.` resolves to the shared runtime
# directory in the installed SDK layout (lib/HyRemote/plugins/generic -> lib) and gives the Linux deploy helper the
# bounded anchor it rewrites for the deployed plugins/generic -> lib layout. The build-tree artifact keeps the
# linker's own entries so the payload still loads from the build tree; it is the installed copy, which carries this
# anchor alone, that a deployment stages.
if(UNIX AND NOT APPLE)
    set_property(TARGET hyremote-generic-plugin PROPERTY BUILD_RPATH "$ORIGIN/../../../.")
    set_property(TARGET hyremote-generic-plugin PROPERTY BUILD_RPATH_USE_ORIGIN TRUE)
    set_property(TARGET hyremote-generic-plugin PROPERTY INSTALL_RPATH "$ORIGIN/../../../.")
endif()

target_compile_features(hyremote-generic-plugin PRIVATE cxx_std_17)
target_link_libraries(hyremote-generic-plugin
    PRIVATE
        Qt6::Core
        Qt6::Gui
        HyRemote::RemoteAccess
)

# The plugin is a deployment payload, not an application link target and not an exported SDK target.
install(TARGETS hyremote-generic-plugin
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/HyRemote/plugins/generic"
    RUNTIME DESTINATION "${CMAKE_INSTALL_LIBDIR}/HyRemote/plugins/generic"
)

if(HYREMOTE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
