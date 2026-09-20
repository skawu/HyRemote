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
)
set_target_properties(hyremote-generic-plugin PROPERTIES
    AUTOMOC ON
    OUTPUT_NAME qhyremote
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/plugins/generic"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/plugins/generic"
)
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
