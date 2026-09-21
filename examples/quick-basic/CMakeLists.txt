cmake_minimum_required(VERSION 3.21)

project(HyRemoteQuickBasic LANGUAGES CXX)

qt_add_executable(hyremote-quick-basic
    main.cpp
    ../hyremote_branding.cpp
    ../../logo/hyremote-branding.qrc
)

qt_add_resources(hyremote-quick-basic quick_basic_resources
    PREFIX "/hyremote/quick-basic"
    FILES
        Main.qml
)

target_compile_features(hyremote-quick-basic PRIVATE cxx_std_17)
target_link_libraries(hyremote-quick-basic
    PRIVATE
        Qt6::Quick
        Qt6::Qml
        HyRemote::RemoteAccess
)
