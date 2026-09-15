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
