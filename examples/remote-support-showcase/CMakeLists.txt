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

# Project logo on the example window. Examples are user-facing demos and are not part of the product build graph:
# docs/repository-layout.md keeps assets/ out of product compilation, and check_repository_layout.cmake forbids
# assets/branding in product/integration modules only - which examples are not. The .qrc carries the resource alias
# because Qt rejects an absolute-path resource file.
qt_add_resources(hyremote-remote-support-showcase hyremote_branding
    FILES
        hyremote-branding.qrc
)
