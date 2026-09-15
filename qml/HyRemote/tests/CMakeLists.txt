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

# The QML engine loads hyremote-qmlplugin from the module import directory. On Windows the plugin's
# dependent hyremote-qml DLL is not guaranteed to be found from that loaded-module directory when
# the test executable lives in qml/HyRemote/tests. Put the module runtime next to the test process
# so this in-tree test has the same explicit runtime-visibility property that the installed SDK's
# deployment helper provides. Linux resolves the module through its normal build-tree rpath.
if(WIN32)
    add_custom_command(TARGET hyremote-qml-module-test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:hyremote-qml>
            $<TARGET_FILE_DIR:hyremote-qml-module-test>
        VERBATIM
    )
endif()

add_test(NAME hyremote-qml-module-test COMMAND hyremote-qml-module-test)
