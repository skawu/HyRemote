cmake_minimum_required(VERSION 3.21)

project(HyRemoteQpaExistingQuickExample LANGUAGES CXX)

find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Quick Qml)

qt_add_executable(hyremote-example-qpa-existing-quick
    main.cpp
)

qt_add_resources(hyremote-example-qpa-existing-quick qpa_existing_quick_resources
    PREFIX "/hyremote/examples/qpa-existing-quick"
    FILES
        Main.qml
)

include("${CMAKE_CURRENT_LIST_DIR}/../../../common/HyRemoteExampleBranding.cmake")
hyremote_example_enable_branding(hyremote-example-qpa-existing-quick)

target_compile_features(hyremote-example-qpa-existing-quick PRIVATE cxx_std_17)
target_link_libraries(hyremote-example-qpa-existing-quick PRIVATE Qt6::Quick Qt6::Qml)

option(HYREMOTE_EXAMPLE_DEPLOY_QPA
    "Install/deploy this ordinary Qt Quick application with the HyRemote QPA package"
    OFF)

if(HYREMOTE_EXAMPLE_DEPLOY_QPA)
    include(GNUInstallDirs)
    find_package(HyRemote CONFIG REQUIRED)

    install(TARGETS hyremote-example-qpa-existing-quick
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        BUNDLE DESTINATION .
    )
    hyremote_deploy(TARGET hyremote-example-qpa-existing-quick QPA)
endif()
