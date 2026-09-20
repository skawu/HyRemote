cmake_minimum_required(VERSION 3.21)

project(HyRemoteQpaQuickExistingAppExample LANGUAGES CXX)

find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Quick Qml)

qt_add_executable(hyremote-qpa-quick-existing-app
    main.cpp
)

qt_add_resources(hyremote-qpa-quick-existing-app qpa_quick_resources
    PREFIX "/hyremote/qpa-quick"
    FILES
        Main.qml
)

include("${CMAKE_CURRENT_LIST_DIR}/../../../common/HyRemoteExampleBranding.cmake")
hyremote_example_enable_branding(hyremote-qpa-quick-existing-app)

target_compile_features(hyremote-qpa-quick-existing-app PRIVATE cxx_std_17)
target_link_libraries(hyremote-qpa-quick-existing-app PRIVATE Qt6::Quick Qt6::Qml)

option(HYREMOTE_EXAMPLE_DEPLOY_QPA
    "Install/deploy this ordinary Qt Quick application with the HyRemote QPA package"
    OFF)

if(HYREMOTE_EXAMPLE_DEPLOY_QPA)
    include(GNUInstallDirs)
    find_package(HyRemote CONFIG REQUIRED)

    install(TARGETS hyremote-qpa-quick-existing-app
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        BUNDLE DESTINATION .
    )
    hyremote_deploy(TARGET hyremote-qpa-quick-existing-app QPA)
endif()
