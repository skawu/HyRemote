add_executable(hyremote-qml-module-test
    test_qml_module.cpp
)

target_link_libraries(hyremote-qml-module-test
    PRIVATE
        Qt6::Core
        Qt6::Qml
)

target_compile_definitions(hyremote-qml-module-test
    PRIVATE
        HYREMOTE_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
)

add_dependencies(hyremote-qml-module-test hyremote-qml hyremote-qmlplugin)
add_test(NAME hyremote-qml-module-test COMMAND hyremote-qml-module-test)
