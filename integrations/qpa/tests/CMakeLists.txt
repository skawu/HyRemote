add_executable(hyremote-qpa-proxy-smoke
    qpa_proxy_smoke.cpp
)

set_target_properties(hyremote-qpa-proxy-smoke PROPERTIES AUTOMOC ON)
target_compile_features(hyremote-qpa-proxy-smoke PRIVATE cxx_std_17)
target_link_libraries(hyremote-qpa-proxy-smoke PRIVATE Qt6::Core Qt6::Gui)
add_dependencies(hyremote-qpa-proxy-smoke hyremote-qpa-platform)

add_test(NAME hyremote-qpa-proxy-smoke COMMAND hyremote-qpa-proxy-smoke)
set_tests_properties(hyremote-qpa-proxy-smoke PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=hyremote;QT_PLUGIN_PATH=${CMAKE_BINARY_DIR}/plugins"
    TIMEOUT 15
)

# QPA-02 native-semantics qualification remains mandatory in every successor branch.
add_executable(hyremote-qpa-native-semantics
    qpa_native_semantics.cpp
)
set_target_properties(hyremote-qpa-native-semantics PROPERTIES AUTOMOC ON)
target_compile_features(hyremote-qpa-native-semantics PRIVATE cxx_std_17)
target_link_libraries(hyremote-qpa-native-semantics PRIVATE Qt6::Core Qt6::Gui Qt6::GuiPrivate)
add_dependencies(hyremote-qpa-native-semantics hyremote-qpa-platform)

add_test(NAME hyremote-qpa-native-semantics COMMAND hyremote-qpa-native-semantics)
set_tests_properties(hyremote-qpa-native-semantics PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=hyremote;QT_PLUGIN_PATH=${CMAKE_BINARY_DIR}/plugins"
    TIMEOUT 15
)

add_executable(hyremote-qpa-interception-test
    qpa_interception_test.cpp
    ../hyremote_qpa_interception.cpp
)
target_compile_features(hyremote-qpa-interception-test PRIVATE cxx_std_17)
target_link_libraries(hyremote-qpa-interception-test PRIVATE Qt6::Core Qt6::Gui)

add_test(NAME hyremote-qpa-interception-test COMMAND hyremote-qpa-interception-test)
set_tests_properties(hyremote-qpa-interception-test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 15
)

add_executable(hyremote-qpa-remote-config-test
    qpa_remote_config_test.cpp
    ../application_surface_model.cpp
    ../composite_target.cpp
    ../composite_target_routing.cpp
    ../interactive_composite_target.cpp
    ../hyremote_qpa_remote_controller.cpp
)
target_compile_features(hyremote-qpa-remote-config-test PRIVATE cxx_std_17)
target_include_directories(hyremote-qpa-remote-config-test
    PRIVATE
        ..
        ${_hyremote_root_dir}/src/remoteaccess/src
)
target_link_libraries(hyremote-qpa-remote-config-test
    PRIVATE
        Qt6::Core
        Qt6::Gui
        HyRemote::Core
        HyRemote::RemoteAccess
)
if(TARGET Qt6::Widgets)
    target_link_libraries(hyremote-qpa-remote-config-test PRIVATE Qt6::Widgets)
    target_compile_definitions(hyremote-qpa-remote-config-test PRIVATE HYREMOTE_QPA_HAS_WIDGETS=1)
endif()
if(TARGET Qt6::Quick)
    target_link_libraries(hyremote-qpa-remote-config-test PRIVATE Qt6::Quick)
    target_compile_definitions(hyremote-qpa-remote-config-test PRIVATE HYREMOTE_QPA_HAS_QUICK=1)
endif()

add_test(NAME hyremote-qpa-remote-config-test COMMAND hyremote-qpa-remote-config-test)
set_tests_properties(hyremote-qpa-remote-config-test PROPERTIES TIMEOUT 15)

if(TARGET Qt6::Widgets)
    # E4 is deliberately an ordinary Qt application. Compile its real source in the QPA build graph
    # with Qt Widgets only; do not add HyRemote targets here. The platform plugin remains a launch /
    # deployment concern rather than an application-source dependency.
    add_executable(hyremote-qpa-existing-app-build-check
        ${_hyremote_root_dir}/examples/qpa-proxy-existing-app/main.cpp
    )
    target_compile_features(hyremote-qpa-existing-app-build-check PRIVATE cxx_std_17)
    target_link_libraries(hyremote-qpa-existing-app-build-check PRIVATE Qt6::Widgets)

    add_executable(hyremote-qpa-auto-remoteaccess-smoke
        qpa_auto_remoteaccess_smoke.cpp
    )
    target_compile_features(hyremote-qpa-auto-remoteaccess-smoke PRIVATE cxx_std_17)
    target_link_libraries(hyremote-qpa-auto-remoteaccess-smoke
        PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::Widgets
    )
    add_dependencies(hyremote-qpa-auto-remoteaccess-smoke hyremote-qpa-platform)

    add_test(NAME hyremote-qpa-auto-remoteaccess-smoke COMMAND hyremote-qpa-auto-remoteaccess-smoke)
    set_tests_properties(hyremote-qpa-auto-remoteaccess-smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=hyremote:hyremote-port=5998;QT_PLUGIN_PATH=${CMAKE_BINARY_DIR}/plugins"
        TIMEOUT 15
    )

    # Transparent QPA is additive to the native application. A remote listener bind failure must
    # never turn into a native-window/event-loop failure. The test reserves an ephemeral loopback
    # port before QApplication construction and asks HyRemote to bind the occupied port.
    add_executable(hyremote-qpa-remote-failure-native-survival-smoke
        qpa_remote_failure_native_survival_smoke.cpp
    )
    target_compile_features(hyremote-qpa-remote-failure-native-survival-smoke PRIVATE cxx_std_17)
    target_link_libraries(hyremote-qpa-remote-failure-native-survival-smoke
        PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets
    )
    if(WIN32)
        target_link_libraries(hyremote-qpa-remote-failure-native-survival-smoke PRIVATE ws2_32)
    endif()
    add_dependencies(hyremote-qpa-remote-failure-native-survival-smoke hyremote-qpa-platform)

    add_test(NAME hyremote-qpa-remote-failure-native-survival-smoke
             COMMAND hyremote-qpa-remote-failure-native-survival-smoke)
    set_tests_properties(hyremote-qpa-remote-failure-native-survival-smoke PROPERTIES
        ENVIRONMENT "QT_PLUGIN_PATH=${CMAKE_BINARY_DIR}/plugins"
        TIMEOUT 15
    )

    add_executable(hyremote-qpa-multi-surface-connection-smoke
        qpa_multi_surface_connection_smoke.cpp
    )
    target_compile_features(hyremote-qpa-multi-surface-connection-smoke PRIVATE cxx_std_17)
    target_link_libraries(hyremote-qpa-multi-surface-connection-smoke
        PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::Widgets
    )
    add_dependencies(hyremote-qpa-multi-surface-connection-smoke hyremote-qpa-platform)

    add_test(NAME hyremote-qpa-multi-surface-connection-smoke
             COMMAND hyremote-qpa-multi-surface-connection-smoke)
    set_tests_properties(hyremote-qpa-multi-surface-connection-smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=hyremote:hyremote-port=5997;QT_PLUGIN_PATH=${CMAKE_BINARY_DIR}/plugins"
        TIMEOUT 25
    )

    add_executable(hyremote-qpa-widget-popup-connection-smoke
        qpa_widget_popup_connection_smoke.cpp
    )
    target_compile_features(hyremote-qpa-widget-popup-connection-smoke PRIVATE cxx_std_17)
    target_link_libraries(hyremote-qpa-widget-popup-connection-smoke
        PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::Widgets
    )
    add_dependencies(hyremote-qpa-widget-popup-connection-smoke hyremote-qpa-platform)

    add_test(NAME hyremote-qpa-widget-popup-connection-smoke
             COMMAND hyremote-qpa-widget-popup-connection-smoke)
    set_tests_properties(hyremote-qpa-widget-popup-connection-smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=hyremote:hyremote-port=5996;QT_PLUGIN_PATH=${CMAKE_BINARY_DIR}/plugins"
        TIMEOUT 25
    )

    # QPA-05 production capture classification. Qt's own 6.8.3 tests cover QWidget::render() with
    # QOpenGLWidget, but HyRemote must qualify its actual end-to-end adapter/transport path rather
    # than inheriting that support claim. Discover OpenGLWidgets only for this bounded test.
    find_package(Qt6 6.8.3 EXACT QUIET COMPONENTS OpenGLWidgets)
    if(TARGET Qt6::OpenGLWidgets)
        add_executable(hyremote-qpa-widget-opengl-capture-smoke
            qpa_widget_opengl_capture_smoke.cpp
        )
        target_compile_features(hyremote-qpa-widget-opengl-capture-smoke PRIVATE cxx_std_17)
        target_link_libraries(hyremote-qpa-widget-opengl-capture-smoke
            PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::Widgets Qt6::OpenGLWidgets
        )
        add_dependencies(hyremote-qpa-widget-opengl-capture-smoke hyremote-qpa-platform)

        add_test(NAME hyremote-qpa-widget-opengl-capture-smoke
                 COMMAND hyremote-qpa-widget-opengl-capture-smoke)
        set_tests_properties(hyremote-qpa-widget-opengl-capture-smoke PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=hyremote:hyremote-port=5994;QT_PLUGIN_PATH=${CMAKE_BINARY_DIR}/plugins"
            TIMEOUT 25
        )
    endif()
endif()

if(TARGET Qt6::Quick)
    add_executable(hyremote-qpa-quick-multi-window-connection-smoke
        qpa_quick_multi_window_connection_smoke.cpp
    )
    target_compile_features(hyremote-qpa-quick-multi-window-connection-smoke PRIVATE cxx_std_17)
    target_link_libraries(hyremote-qpa-quick-multi-window-connection-smoke
        PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::Quick
    )
    add_dependencies(hyremote-qpa-quick-multi-window-connection-smoke hyremote-qpa-platform)

    add_test(NAME hyremote-qpa-quick-multi-window-connection-smoke
             COMMAND hyremote-qpa-quick-multi-window-connection-smoke)
    set_tests_properties(hyremote-qpa-quick-multi-window-connection-smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=hyremote:hyremote-port=5995;QT_PLUGIN_PATH=${CMAKE_BINARY_DIR}/plugins;QT_QUICK_BACKEND=software"
        TIMEOUT 30
    )
endif()

add_executable(hyremote-qpa-application-surface-model-test
    qpa_application_surface_model_test.cpp
    ../application_surface_model.cpp
)
target_compile_features(hyremote-qpa-application-surface-model-test PRIVATE cxx_std_17)
target_link_libraries(hyremote-qpa-application-surface-model-test PRIVATE Qt6::Core)

add_test(NAME hyremote-qpa-application-surface-model-test COMMAND hyremote-qpa-application-surface-model-test)
set_tests_properties(hyremote-qpa-application-surface-model-test PROPERTIES TIMEOUT 15)

add_executable(hyremote-qpa-composite-capture-test
    qpa_composite_capture_test.cpp
    ../application_surface_model.cpp
    ../composite_target.cpp
)
target_compile_features(hyremote-qpa-composite-capture-test PRIVATE cxx_std_17)
target_include_directories(hyremote-qpa-composite-capture-test
    PRIVATE .. ${_hyremote_root_dir}/src/remoteaccess/src
)
# Same reason as hyremote-qpa-composite-input-test: the compiled sources include the RemoteAccess
# private component-factory declarations, so the product target provides their include directories.
target_link_libraries(hyremote-qpa-composite-capture-test
    PRIVATE Qt6::Core Qt6::Gui HyRemote::Core HyRemote::RemoteAccess
)

add_test(NAME hyremote-qpa-composite-capture-test COMMAND hyremote-qpa-composite-capture-test)
set_tests_properties(hyremote-qpa-composite-capture-test PROPERTIES TIMEOUT 15)

add_executable(hyremote-qpa-composite-input-test
    qpa_composite_input_test.cpp
    ../application_surface_model.cpp
    ../composite_target.cpp
    ../composite_target_routing.cpp
    ../interactive_composite_target.cpp
)
target_compile_features(hyremote-qpa-composite-input-test PRIVATE cxx_std_17)
target_include_directories(hyremote-qpa-composite-input-test
    PRIVATE .. ${_hyremote_root_dir}/src/remoteaccess/src
)
# This target compiles the interactive composite target and the RemoteAccess private component-factory
# declarations, which reference QHostAddress and the generated RemoteAccess export header. Linking the
# product target (as hyremote-qpa-remote-config-test above already does) supplies the public include
# directory, the generated header directory and Qt6::Network transitively.
target_link_libraries(hyremote-qpa-composite-input-test
    PRIVATE Qt6::Core Qt6::Gui HyRemote::Core HyRemote::RemoteAccess
)

add_test(NAME hyremote-qpa-composite-input-test COMMAND hyremote-qpa-composite-input-test)
set_tests_properties(hyremote-qpa-composite-input-test PROPERTIES TIMEOUT 15)
