cmake_minimum_required(VERSION 3.21)

project(HyRemoteInstalledConsumer LANGUAGES CXX)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Network)
find_package(HyRemote CONFIG REQUIRED)

add_executable(hyremote-installed-consumer main.cpp)
target_compile_features(hyremote-installed-consumer PRIVATE cxx_std_17)
target_link_libraries(hyremote-installed-consumer PRIVATE HyRemote::RemoteAccess)
