cmake_minimum_required(VERSION 3.21)

project(HyRemoteQuickBasic LANGUAGES CXX)

qt_add_executable(hyremote-quick-basic
    main.cpp
)

qt_add_resources(hyremote-quick-basic quick_basic_resources
    PREFIX "/hyremote/quick-basic"
    FILES
        Main.qml
)

target_compile_features(hyremote-quick-basic PRIVATE cxx_std_17)
target_link_libraries(hyremote-quick-basic
    PRIVATE
        Qt6::Quick
        Qt6::Qml
        HyRemote::RemoteAccess
)

# Project logo on the example window. Examples are user-facing demos and are not part of the product build graph:
# docs/repository-layout.md keeps assets/ out of product compilation, and check_repository_layout.cmake forbids
# assets/branding in product/integration modules only - which examples are not. The .qrc carries the resource alias
# because Qt rejects an absolute-path resource file.
qt_add_resources(hyremote-quick-basic hyremote_branding
    FILES
        hyremote-branding.qrc
)
