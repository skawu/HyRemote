cmake_minimum_required(VERSION 3.21)

project(HyRemoteWidgetsBasic LANGUAGES CXX)

add_executable(hyremote-widgets-basic
    main.cpp
    ../../logo/hyremote-branding.qrc
)

target_compile_features(hyremote-widgets-basic PRIVATE cxx_std_17)
target_link_libraries(hyremote-widgets-basic
    PRIVATE
        Qt6::Widgets
        HyRemote::RemoteAccess
)
