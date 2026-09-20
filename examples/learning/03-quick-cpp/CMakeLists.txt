cmake_minimum_required(VERSION 3.21)

project(HyRemoteQuickCpp LANGUAGES CXX)

qt_add_executable(hyremote-example-quick-cpp
    main.cpp
)

qt_add_resources(hyremote-example-quick-cpp quick_cpp_resources
    PREFIX "/hyremote/examples/quick-cpp"
    FILES
        Main.qml
)

include("${CMAKE_CURRENT_LIST_DIR}/../../common/HyRemoteExampleBranding.cmake")
hyremote_example_enable_branding(hyremote-example-quick-cpp)

target_compile_features(hyremote-example-quick-cpp PRIVATE cxx_std_17)
target_link_libraries(hyremote-example-quick-cpp
    PRIVATE
        Qt6::Quick
        Qt6::Qml
        HyRemote::RemoteAccess
)
