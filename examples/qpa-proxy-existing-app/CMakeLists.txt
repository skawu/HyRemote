cmake_minimum_required(VERSION 3.21)

project(HyRemoteQpaExistingAppExample LANGUAGES CXX)

find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Widgets)

add_executable(hyremote-qpa-proxy-existing-app
    main.cpp
)

target_compile_features(hyremote-qpa-proxy-existing-app PRIVATE cxx_std_17)
target_link_libraries(hyremote-qpa-proxy-existing-app PRIVATE Qt6::Widgets)

# Deliberately no HyRemote target is linked here. This project is an ordinary Qt application.
