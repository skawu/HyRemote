include(GNUInstallDirs)

# Transparent QPA Proxy — exact Qt 6.8.3 private-ABI package.
# Private QPA types and the QPA-02 interception seam remain isolated here, while QPA-03/04 remote
# runtime composition reuses the same public HyRemote::RemoteAccess facade as C++ and QML modes.
add_library(hyremote-qpa-platform MODULE
    application_surface_model.cpp
    composite_target.cpp
    composite_target_routing.cpp
    interactive_composite_target.cpp
    hyremote_platform_plugin.cpp
    hyremote_qpa_interception.cpp
    hyremote_qpa_remote_controller.cpp
)
add_library(HyRemote::QpaPlatform ALIAS hyremote-qpa-platform)

set_target_properties(hyremote-qpa-platform PROPERTIES
    AUTOMOC ON
    OUTPUT_NAME qhyremote
    EXPORT_NAME QpaPlatform
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/platforms"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/platforms"
)

target_compile_features(hyremote-qpa-platform PRIVATE cxx_std_17)
target_include_directories(hyremote-qpa-platform
    PRIVATE
        ${PROJECT_SOURCE_DIR}/remoteaccess/src
)
target_link_libraries(hyremote-qpa-platform
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::GuiPrivate
        HyRemote::RemoteAccess
)

# QApplication can expose its real top-level QWidget through the shared RemoteAccess adapter chain.
# Keep Widgets optional at the QPA package level so QGuiApplication/Qt Quick applications do not
# acquire a Widgets dependency solely because transparent mode is enabled.
if(TARGET Qt6::Widgets)
    target_link_libraries(hyremote-qpa-platform PRIVATE Qt6::Widgets)
    target_compile_definitions(hyremote-qpa-platform PRIVATE HYREMOTE_QPA_HAS_WIDGETS=1)
endif()

# Surface enumeration must distinguish a qualified QQuickWindow from an arbitrary QWindow. Link
# Qt Quick only when the normal RemoteAccess build already resolved that product adapter; this does
# not add a new rendering/capture implementation to the QPA plugin.
if(TARGET Qt6::Quick)
    target_link_libraries(hyremote-qpa-platform PRIVATE Qt6::Quick)
    target_compile_definitions(hyremote-qpa-platform PRIVATE HYREMOTE_QPA_HAS_QUICK=1)
endif()

# The plugin is version-coupled by design. Refuse accidental compilation against a different private ABI.
target_compile_definitions(hyremote-qpa-platform PRIVATE
    HYREMOTE_QPA_QT_VERSION_MAJOR=6
    HYREMOTE_QPA_QT_VERSION_MINOR=8
    HYREMOTE_QPA_QT_VERSION_PATCH=3
)

# Export the module target as package metadata only. Applications never link this target; the one
# HyRemote deployment helper uses its imported LOCATION to copy the exact SDK-built platform plugin
# into the application's Qt plugin tree. PRIVATE QPA dependencies therefore remain isolated.
install(TARGETS hyremote-qpa-platform
    EXPORT HyRemoteTargets
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/HyRemote/plugins/platforms"
    RUNTIME DESTINATION "${CMAKE_INSTALL_LIBDIR}/HyRemote/plugins/platforms"
)

if(HYREMOTE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
