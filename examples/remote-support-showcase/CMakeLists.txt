cmake_minimum_required(VERSION 3.21)

project(HyRemoteRemoteSupportShowcase LANGUAGES CXX)

add_executable(hyremote-remote-support-showcase
    main.cpp
    input_probe.cpp
    ../hyremote_branding.cpp
    ../../logo/hyremote-branding.qrc
)

target_compile_features(hyremote-remote-support-showcase PRIVATE cxx_std_17)
target_link_libraries(hyremote-remote-support-showcase
    PRIVATE
        Qt6::Widgets
        HyRemote::RemoteAccess
)

# It is a user-facing example like the learning ones: it installs into the same shared prefix and is deployed by the
# same helper, so it is reachable from the product tree instead of only from the build tree.
install(TARGETS hyremote-remote-support-showcase RUNTIME DESTINATION bin)
hyremote_deploy(TARGET hyremote-remote-support-showcase)
