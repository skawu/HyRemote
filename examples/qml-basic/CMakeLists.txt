# qt_add_qml_module() extracts QObject metadata while configuring the target, so AUTOMOC must be on
# before the target exists: without it the qt6_extract_metatypes step fails during configure and the
# example cannot be configured at all (control experiment, see the PR description). The same rule is
# already followed by integrations/qml/HyRemote for the module itself.
set(CMAKE_AUTOMOC ON)

qt_add_executable(hyremote-qml-basic
    main.cpp
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
