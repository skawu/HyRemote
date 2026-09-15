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

# Parse/config coverage is independent from plugin loading so safe defaults and fail-closed
# parameter handling stay deterministic even when hosted native-display execution is unavailable.
add_executable(hyremote-qpa-remote-config-test
    qpa_remote_config_test.cpp
    ../hyremote_qpa_remote_controller.cpp
)
target_compile_features(hyremote-qpa-remote-config-test PRIVATE cxx_std_17)
target_include_directories(hyremote-qpa-remote-config-test PRIVATE ..)
target_link_libraries(hyremote-qpa-remote-config-test
    PRIVATE
        Qt6::Core
        Qt6::Gui
        HyRemote::RemoteAccess
)
if(TARGET Qt6::Widgets)
    target_link_libraries(hyremote-qpa-remote-config-test PRIVATE Qt6::Widgets)
    target_compile_definitions(hyremote-qpa-remote-config-test PRIVATE HYREMOTE_QPA_HAS_WIDGETS=1)
endif()

add_test(NAME hyremote-qpa-remote-config-test COMMAND hyremote-qpa-remote-config-test)
set_tests_properties(hyremote-qpa-remote-config-test PROPERTIES TIMEOUT 15)

# QPA-03 product-composition smoke: a normal QWidget app selects only the `hyremote` QPA plugin;
# the qualified native proxy must attach the shared RemoteAccess runtime automatically, then release
# the listener when its visible primary target disappears. Hosted evidence is not physical-display proof.
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
endif()
