include(GNUInstallDirs)

# Transparent QPA Proxy — exact Qt 6.8.3 private-ABI package.
# Private QPA types remain isolated here, while remote runtime composition reuses the same public
# HyRemote::RemoteAccess facade as the C++ and QML modes.
add_library(hyremote-qpa-platform MODULE
    hyremote_platform_plugin.cpp
    hyremote_qpa_remote_controller.cpp
)

set_target_properties(hyremote-qpa-platform PROPERTIES
    AUTOMOC ON
    OUTPUT_NAME qhyremote
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/platforms"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/platforms"
)

target_compile_features(hyremote-qpa-platform PRIVATE cxx_std_17)
target_link_libraries(hyremote-qpa-platform
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::GuiPrivate
        HyRemote::RemoteAccess
)

# A QApplication can expose its real top-level QWidget as the RemoteAccess target. This is optional
# at the QPA package level so pure QGuiApplication/Qt Quick programs do not require Widgets.
if(TARGET Qt6::Widgets)
    target_link_libraries(hyremote-qpa-platform PRIVATE Qt6::Widgets)
    target_compile_definitions(hyremote-qpa-platform PRIVATE HYREMOTE_QPA_HAS_WIDGETS=1)
endif()

# The plugin is version-coupled by design. Refuse accidental compilation against a different private ABI.
target_compile_definitions(hyremote-qpa-platform PRIVATE
    HYREMOTE_QPA_QT_VERSION_MAJOR=6
    HYREMOTE_QPA_QT_VERSION_MINOR=8
    HYREMOTE_QPA_QT_VERSION_PATCH=3
)

install(TARGETS hyremote-qpa-platform
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/HyRemote/plugins/platforms"
    RUNTIME DESTINATION "${CMAKE_INSTALL_LIBDIR}/HyRemote/plugins/platforms"
)

if(HYREMOTE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
