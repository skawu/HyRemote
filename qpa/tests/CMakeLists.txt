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

# Parse/config coverage is kept independent from plugin loading so safe defaults and fail-closed
# parameter handling stay deterministic even when hosted GUI runners are unavailable.
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
