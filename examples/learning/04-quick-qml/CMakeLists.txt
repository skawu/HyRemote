set(CMAKE_AUTOMOC ON)

qt_add_executable(hyremote-example-quick-qml
    main.cpp
)

qt_add_qml_module(hyremote-example-quick-qml
    URI HyRemote.Examples.QuickQml
    VERSION 1.0
    QML_FILES Main.qml
)

include("${CMAKE_CURRENT_LIST_DIR}/../../common/HyRemoteExampleBranding.cmake")
hyremote_example_enable_branding(hyremote-example-quick-qml)

target_compile_features(hyremote-example-quick-qml PRIVATE cxx_std_17)
target_link_libraries(hyremote-example-quick-qml PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)
add_dependencies(hyremote-example-quick-qml hyremote-qml hyremote-qmlplugin)

target_compile_definitions(hyremote-example-quick-qml PRIVATE
    HYREMOTE_BUILD_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
)
