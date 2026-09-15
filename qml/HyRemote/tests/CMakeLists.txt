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

# QML imports load the generated plugin from build/qml/HyRemote at runtime. Its backing shared
# library must be discoverable through the platform loader as well as through QML's import path.
# Do not depend on the developer machine's current directory or global PATH/LD_LIBRARY_PATH.
if(WIN32)
    set_tests_properties(hyremote-qml-module-test PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:hyremote-qml>"
    )
elseif(UNIX)
    set_tests_properties(hyremote-qml-module-test PROPERTIES
        ENVIRONMENT_MODIFICATION
            "LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:hyremote-qml>"
    )
endif()
