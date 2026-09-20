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
