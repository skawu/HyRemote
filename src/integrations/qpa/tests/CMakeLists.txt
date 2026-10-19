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

# Native-semantics qualification remains mandatory for the exact Qt private-ABI QPA payload.
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

# QPA owns only launch/configuration vocabulary. Application-surface composition is tested by
# src/runtime/tests and must not be rebuilt through QPA-owned predecessor sources.
add_executable(hyremote-qpa-remote-config-test
    qpa_remote_config_test.cpp
    ../qpa_config.cpp
)
target_compile_features(hyremote-qpa-remote-config-test PRIVATE cxx_std_17)
target_include_directories(hyremote-qpa-remote-config-test PRIVATE ..)
target_link_libraries(hyremote-qpa-remote-config-test PRIVATE Qt6::Core Qt6::Network)

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

    # Production capture classification for QWidget + QOpenGLWidget through the real sidecar path.
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

include("${PROJECT_SOURCE_DIR}/tests/semantic_ctest_labels.cmake")
hyremote_label_test_family("integration;qpa;fast"
    hyremote-qpa-proxy-smoke
    hyremote-qpa-native-semantics
    hyremote-qpa-auto-remoteaccess-smoke
    hyremote-qpa-remote-failure-native-survival-smoke
    hyremote-qpa-multi-surface-connection-smoke
    hyremote-qpa-widget-popup-connection-smoke
    hyremote-qpa-widget-opengl-capture-smoke
    hyremote-qpa-quick-multi-window-connection-smoke
)
hyremote_label_test_family("contract;qpa;fast"
    hyremote-qpa-remote-config-test
)

# The parent QPA directory registers the T4 deployment matrix after this subdirectory returns.
cmake_language(DEFER DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.." CALL hyremote_apply_qpa_deploy_semantic_test_labels)
