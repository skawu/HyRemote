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

# Parse/config coverage keeps the controller source honest as it evolves through QPA-04. The
# controller now owns a private composite target, so compile the same private composition sources
# into this non-installed test target rather than hiding unresolved dependencies until plugin CI.
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
        ${PROJECT_SOURCE_DIR}/remoteaccess/src
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

add_test(NAME hyremote-qpa-remote-config-test COMMAND hyremote-qpa-remote-config-test)
set_tests_properties(hyremote-qpa-remote-config-test PROPERTIES TIMEOUT 15)

# Transparent-mode predecessor smoke: a normal QWidget application selects only the `hyremote`
# platform and gets the shared RemoteAccess listener automatically. QPA-04 intentionally supersedes
# the old single-target "hide primary => stop listener" behavior; continuity is tested below.
if(TARGET Qt6::Widgets)
    add_executable(hyremote-qpa-auto-remoteaccess-smoke
        qpa_auto_remoteaccess_smoke.cpp
    )
    target_compile_features(hyremote-qpa-auto-remoteaccess-smoke PRIVATE cxx_std_17)
    target_link_libraries(hyremote-qpa-auto-remoteaccess-smoke
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Network
            Qt6::Widgets
    )
    add_dependencies(hyremote-qpa-auto-remoteaccess-smoke hyremote-qpa-platform)

    add_test(NAME hyremote-qpa-auto-remoteaccess-smoke COMMAND hyremote-qpa-auto-remoteaccess-smoke)
    set_tests_properties(hyremote-qpa-auto-remoteaccess-smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=hyremote:hyremote-port=5998;QT_PLUGIN_PATH=${CMAKE_BINARY_DIR}/plugins"
        TIMEOUT 15
    )

    # QPA-04 product E2E: complete one RFB 3.8 connection, advertise DesktopSize, then keep that
    # exact socket alive while an independent dialog expands the application canvas, the primary
    # window hides, and the primary/dialog set changes again. A stop/restart listener implementation
    # necessarily disconnects this viewer and fails the test.
    add_executable(hyremote-qpa-multi-surface-connection-smoke
        qpa_multi_surface_connection_smoke.cpp
    )
    target_compile_features(hyremote-qpa-multi-surface-connection-smoke PRIVATE cxx_std_17)
    target_link_libraries(hyremote-qpa-multi-surface-connection-smoke
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Network
            Qt6::Widgets
    )
    add_dependencies(hyremote-qpa-multi-surface-connection-smoke hyremote-qpa-platform)

    add_test(NAME hyremote-qpa-multi-surface-connection-smoke
             COMMAND hyremote-qpa-multi-surface-connection-smoke)
    set_tests_properties(hyremote-qpa-multi-surface-connection-smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=hyremote:hyremote-port=5997;QT_PLUGIN_PATH=${CMAKE_BINARY_DIR}/plugins"
        TIMEOUT 25
    )
endif()

# QPA-04 deterministic model gate. This is deliberately transport/capture independent; it pins the
# application-canvas semantics before the same-runtime compositor is allowed to consume them.
add_executable(hyremote-qpa-application-surface-model-test
    qpa_application_surface_model_test.cpp
    ../application_surface_model.cpp
)
target_compile_features(hyremote-qpa-application-surface-model-test PRIVATE cxx_std_17)
target_link_libraries(hyremote-qpa-application-surface-model-test PRIVATE Qt6::Core)

add_test(NAME hyremote-qpa-application-surface-model-test COMMAND hyremote-qpa-application-surface-model-test)
set_tests_properties(hyremote-qpa-application-surface-model-test PROPERTIES TIMEOUT 15)

# QPA-04 capture-composition gate. Fake child adapters deliberately callback synchronously so this
# test also pins aggregation ordering against a stricter callback schedule than the real adapters.
add_executable(hyremote-qpa-composite-capture-test
    qpa_composite_capture_test.cpp
    ../application_surface_model.cpp
    ../composite_target.cpp
)
target_compile_features(hyremote-qpa-composite-capture-test PRIVATE cxx_std_17)
target_include_directories(hyremote-qpa-composite-capture-test
    PRIVATE
        ..
        ${PROJECT_SOURCE_DIR}/remoteaccess/src
)
target_link_libraries(hyremote-qpa-composite-capture-test
    PRIVATE
        Qt6::Core
        Qt6::Gui
        HyRemote::Core
)

add_test(NAME hyremote-qpa-composite-capture-test COMMAND hyremote-qpa-composite-capture-test)
set_tests_properties(hyremote-qpa-composite-capture-test PROPERTIES TIMEOUT 15)

# QPA-04 normalized-input gate. Child sinks are deterministic recorders; the composite router is
# responsible only for application-canvas routing and delegates target-specific Qt delivery to the
# same built-in adapter contract used by ordinary RemoteAccess.
add_executable(hyremote-qpa-composite-input-test
    qpa_composite_input_test.cpp
    ../application_surface_model.cpp
    ../composite_target.cpp
    ../composite_target_routing.cpp
    ../interactive_composite_target.cpp
)
target_compile_features(hyremote-qpa-composite-input-test PRIVATE cxx_std_17)
target_include_directories(hyremote-qpa-composite-input-test
    PRIVATE
        ..
        ${PROJECT_SOURCE_DIR}/remoteaccess/src
)
target_link_libraries(hyremote-qpa-composite-input-test
    PRIVATE
        Qt6::Core
        Qt6::Gui
        HyRemote::Core
)

add_test(NAME hyremote-qpa-composite-input-test COMMAND hyremote-qpa-composite-input-test)
set_tests_properties(hyremote-qpa-composite-input-test PROPERTIES TIMEOUT 15)
