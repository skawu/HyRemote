cmake_minimum_required(VERSION 3.21)

project(HyRemoteRemoteSupportShowcase LANGUAGES CXX)

add_executable(hyremote-remote-support-showcase
    main.cpp
    input_probe.cpp
)

target_compile_features(hyremote-remote-support-showcase PRIVATE cxx_std_17)
target_link_libraries(hyremote-remote-support-showcase
    PRIVATE
        Qt6::Widgets
        HyRemote::RemoteAccess
)
