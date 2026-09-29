if(TARGET Qt6::Widgets)
    add_executable(hyremote-generic-plugin-smoke
        generic_plugin_smoke.cpp
    )
    target_compile_features(hyremote-generic-plugin-smoke PRIVATE cxx_std_17)
    target_link_libraries(hyremote-generic-plugin-smoke
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Network
            Qt6::Widgets
    )
    add_dependencies(hyremote-generic-plugin-smoke hyremote-generic-plugin)

    add_test(NAME hyremote-generic-plugin-smoke COMMAND hyremote-generic-plugin-smoke)
    set_tests_properties(hyremote-generic-plugin-smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_GENERIC_PLUGINS=hyremote:port=5992;QT_PLUGIN_PATH=${PROJECT_BINARY_DIR}/plugins"
        TIMEOUT 10
    )
endif()

# #405: exercise the zero-code Generic boundary rather than duplicating the Shared Runtime's control matrix.
# The plugin is loaded through Qt's normal generic-plugin environment, arms automatic access with input enabled,
# and a raw production RFB PointerEvent must reach a normal QQuickItem while the native platform stays offscreen.
add_executable(hyremote-generic-quick-input-smoke
    generic_quick_input_smoke.cpp
)
target_compile_features(hyremote-generic-quick-input-smoke PRIVATE cxx_std_17)
target_link_libraries(hyremote-generic-quick-input-smoke
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        Qt6::Quick
)
add_dependencies(hyremote-generic-quick-input-smoke hyremote-generic-plugin)

add_test(NAME hyremote-generic-quick-input-smoke COMMAND hyremote-generic-quick-input-smoke)
set_tests_properties(hyremote-generic-quick-input-smoke PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_QPA_GENERIC_PLUGINS=hyremote:port=5993\;input=true;QT_PLUGIN_PATH=${PROJECT_BINARY_DIR}/plugins"
    TIMEOUT 10
)

# #174: the Generic Plugin launch-syntax mapping, on its own so it neither needs Qt's plugin loader nor a real
# adapter. It compiles the parser directly, the way the QPA frontend's config test does.
add_executable(hyremote-generic-config-test
    generic_config_test.cpp
    ../generic_config.cpp
)
target_compile_features(hyremote-generic-config-test PRIVATE cxx_std_17)
target_include_directories(hyremote-generic-config-test PRIVATE ${CMAKE_SOURCE_DIR}/src/runtime/src)
target_link_libraries(hyremote-generic-config-test PRIVATE Qt6::Core Qt6::Network)
add_test(NAME hyremote-generic-config-test COMMAND hyremote-generic-config-test)
set_tests_properties(hyremote-generic-config-test PROPERTIES TIMEOUT 15)

include("${PROJECT_SOURCE_DIR}/tests/semantic_ctest_labels.cmake")
hyremote_label_test_family("integration;generic;fast"
    hyremote-generic-plugin-smoke
    hyremote-generic-quick-input-smoke
)
hyremote_label_test_family("contract;generic;fast"
    hyremote-generic-config-test
)
