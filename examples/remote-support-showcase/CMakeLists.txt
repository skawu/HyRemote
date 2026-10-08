cmake_minimum_required(VERSION 3.21)

project(HyRemoteTool LANGUAGES CXX)

add_executable(hyremote-tool
    main.cpp
    input_probe.cpp
    ../hyremote_branding.cpp
    ../../logo/hyremote-branding.qrc
)

target_compile_features(hyremote-tool PRIVATE cxx_std_17)
target_link_libraries(hyremote-tool
    PRIVATE
        Qt6::Widgets
        HyRemote::RemoteAccess
)

# It is a user-facing example like the learning ones: it installs into the same shared prefix and is deployed by the
# same helper, so it is reachable from the product tree instead of only from the build tree.
install(TARGETS hyremote-tool RUNTIME DESTINATION bin)
hyremote_deploy(TARGET hyremote-tool)
