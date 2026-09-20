cmake_minimum_required(VERSION 3.21)

project(HyRemoteProductionShowcase LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)

qt_add_executable(hyremote-remote-support-showcase
    main.cpp
)

qt_add_qml_module(hyremote-remote-support-showcase
    URI HyRemoteShowcase
    VERSION 1.0
    QML_FILES Main.qml
)

include("${CMAKE_CURRENT_LIST_DIR}/../../common/HyRemoteExampleBranding.cmake")
hyremote_example_enable_branding(hyremote-remote-support-showcase)

target_compile_features(hyremote-remote-support-showcase PRIVATE cxx_std_17)
target_link_libraries(hyremote-remote-support-showcase
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
)
add_dependencies(hyremote-remote-support-showcase hyremote-qml hyremote-qmlplugin)

target_compile_definitions(hyremote-remote-support-showcase PRIVATE
    HYREMOTE_BUILD_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
)
