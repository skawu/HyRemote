cmake_minimum_required(VERSION 3.21)

project(HyRemoteProductionShowcase LANGUAGES CXX)

add_executable(hyremote-remote-support-showcase
    main.cpp
    input_probe.cpp
)

include("${CMAKE_CURRENT_LIST_DIR}/../../common/HyRemoteExampleBranding.cmake")
hyremote_example_enable_branding(hyremote-remote-support-showcase)

target_compile_features(hyremote-remote-support-showcase PRIVATE cxx_std_17)
target_link_libraries(hyremote-remote-support-showcase
    PRIVATE
        Qt6::Widgets
        HyRemote::RemoteAccess
)

# This target is retained only while the final Qt Quick/QML production-showcase rewrite is completed
# in #217. Do not treat the temporary Widgets implementation as the final V1 showcase architecture.
