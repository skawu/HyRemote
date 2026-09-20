cmake_minimum_required(VERSION 3.21)

project(HyRemoteQpaExistingWidgetsExample LANGUAGES CXX)

find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Widgets)

add_executable(hyremote-example-qpa-existing-widgets
    main.cpp
)

include("${CMAKE_CURRENT_LIST_DIR}/../../../common/HyRemoteExampleBranding.cmake")
hyremote_example_enable_branding(hyremote-example-qpa-existing-widgets)

target_compile_features(hyremote-example-qpa-existing-widgets PRIVATE cxx_std_17)
target_link_libraries(hyremote-example-qpa-existing-widgets PRIVATE Qt6::Widgets)

# The application target always remains Qt-only. For an ordinary native build no HyRemote package
# is needed at all. Installed QPA deployment is an opt-in packaging concern: enabling this option
# locates HyRemote solely for its package-owned deploy helper/module metadata and never adds a
# HyRemote application/runtime link dependency to the executable.
option(HYREMOTE_EXAMPLE_DEPLOY_QPA
    "Install/deploy this ordinary Qt application with the HyRemote QPA package"
    OFF)

if(HYREMOTE_EXAMPLE_DEPLOY_QPA)
    include(GNUInstallDirs)
    find_package(HyRemote CONFIG REQUIRED)

    install(TARGETS hyremote-example-qpa-existing-widgets
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        BUNDLE DESTINATION .
    )
    hyremote_deploy(TARGET hyremote-example-qpa-existing-widgets QPA)
endif()
