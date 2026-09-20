cmake_minimum_required(VERSION 3.21)

project(HyRemoteWidgetsControl LANGUAGES CXX)

add_executable(hyremote-widgets-basic
    main.cpp
)

include("${CMAKE_CURRENT_LIST_DIR}/../../common/HyRemoteExampleBranding.cmake")
hyremote_example_enable_branding(hyremote-widgets-basic)

target_compile_features(hyremote-widgets-basic PRIVATE cxx_std_17)
target_link_libraries(hyremote-widgets-basic
    PRIVATE
        Qt6::Widgets
        HyRemote::RemoteAccess
)
