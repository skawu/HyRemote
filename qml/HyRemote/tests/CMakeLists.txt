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

# Keep the repository's CMake 3.21 contract: ENVIRONMENT_MODIFICATION was added after that
# baseline, so launch the test through a tiny CMake script that explicitly prepends the generated
# QML module runtime directory to the platform loader path.
add_test(
    NAME hyremote-qml-module-test
    COMMAND ${CMAKE_COMMAND}
        -DTEST_EXE=$<TARGET_FILE:hyremote-qml-module-test>
        -DMODULE_RUNTIME_DIR=$<TARGET_FILE_DIR:hyremote-qml>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/run_qml_module_test.cmake
)
