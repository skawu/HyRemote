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

# Project logo on the example window. Examples are user-facing demos and are not part of the product build graph:
# docs/repository-layout.md keeps assets/ out of product compilation, and check_repository_layout.cmake forbids
# assets/branding in product/integration modules only - which examples are not. The .qrc carries the resource alias
# because Qt rejects an absolute-path resource file.
qt_add_resources(hyremote-qml-basic hyremote_branding
    FILES
        hyremote-branding.qrc
)
