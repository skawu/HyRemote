set(CMAKE_AUTOMOC ON)

qt_add_executable(hyremote-qml-basic
    main.cpp
    ../hyremote-branding.qrc
)

qt_add_qml_module(hyremote-qml-basic
    URI HyRemoteExample
    VERSION 1.0
    QML_FILES Main.qml
)

target_compile_features(hyremote-qml-basic PRIVATE cxx_std_17)
target_link_libraries(hyremote-qml-basic PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)
add_dependencies(hyremote-qml-basic hyremote-qml hyremote-qmlplugin)

target_compile_definitions(hyremote-qml-basic PRIVATE
    HYREMOTE_BUILD_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
)
