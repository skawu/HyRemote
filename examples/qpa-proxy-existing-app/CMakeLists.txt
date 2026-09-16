cmake_minimum_required(VERSION 3.21)

project(HyRemoteQpaExistingAppExample LANGUAGES CXX)

find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Widgets)

add_executable(hyremote-qpa-proxy-existing-app
    main.cpp
)

target_compile_features(hyremote-qpa-proxy-existing-app PRIVATE cxx_std_17)
target_link_libraries(hyremote-qpa-proxy-existing-app PRIVATE Qt6::Widgets)

# The application target always remains Qt-only. For an ordinary native build no HyRemote package
# is needed at all. Installed Transparent-QPA deployment is an opt-in packaging concern: enabling
# this option locates HyRemote solely for its package-owned deploy helper/module metadata and never
# adds a HyRemote link dependency to the executable.
option(HYREMOTE_EXAMPLE_DEPLOY_QPA
    "Install/deploy this ordinary Qt application with the HyRemote Transparent QPA package"
    OFF)

if(HYREMOTE_EXAMPLE_DEPLOY_QPA)
    include(GNUInstallDirs)
    find_package(HyRemote CONFIG REQUIRED)

    install(TARGETS hyremote-qpa-proxy-existing-app
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        BUNDLE DESTINATION .
    )
    hyremote_deploy(TARGET hyremote-qpa-proxy-existing-app QPA)
endif()
