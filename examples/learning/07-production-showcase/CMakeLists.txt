cmake_minimum_required(VERSION 3.21)

project(HyRemoteProductionShowcase LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)

qt_add_executable(hyremote-example-production-showcase
    main.cpp
)

qt_add_qml_module(hyremote-example-production-showcase
    URI HyRemote.Examples.ProductionShowcase
    VERSION 1.0
    QML_FILES Main.qml
)

include("${CMAKE_CURRENT_LIST_DIR}/../../common/HyRemoteExampleBranding.cmake")
hyremote_example_enable_branding(hyremote-example-production-showcase)

target_compile_features(hyremote-example-production-showcase PRIVATE cxx_std_17)
target_link_libraries(hyremote-example-production-showcase
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
)
add_dependencies(hyremote-example-production-showcase hyremote-qml hyremote-qmlplugin)

target_compile_definitions(hyremote-example-production-showcase PRIVATE
    HYREMOTE_BUILD_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
)
