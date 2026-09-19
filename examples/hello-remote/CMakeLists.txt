cmake_minimum_required(VERSION 3.21)

project(HyRemoteHelloRemote LANGUAGES CXX)

add_executable(hyremote-hello-remote
    main.cpp
)

target_compile_features(hyremote-hello-remote PRIVATE cxx_std_17)
target_link_libraries(hyremote-hello-remote
    PRIVATE
        Qt6::Widgets
        HyRemote::RemoteAccess
)
