cmake_minimum_required(VERSION 3.21)

project(HyRemoteInstalledQpaConsumer LANGUAGES CXX)

include(GNUInstallDirs)

# Transparent QPA remains an ordinary Qt application at the source/link layer. HyRemote is found
# only for package/deployment metadata; the executable itself links Qt Widgets and nothing HyRemote.
find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Widgets)
find_package(HyRemote CONFIG REQUIRED)

if(NOT HyRemote_QPA_AVAILABLE)
    message(FATAL_ERROR "installed HyRemote SDK does not contain the Transparent QPA package")
endif()
if(NOT TARGET HyRemote::QpaPlatform)
    message(FATAL_ERROR "installed HyRemote SDK did not export its deployable QPA module target")
endif()

add_executable(hyremote-installed-qpa-consumer
    main.cpp
)
target_compile_features(hyremote-installed-qpa-consumer PRIVATE cxx_std_17)
target_link_libraries(hyremote-installed-qpa-consumer PRIVATE Qt6::Widgets)

get_target_property(_consumer_links hyremote-installed-qpa-consumer LINK_LIBRARIES)
foreach(_consumer_link IN LISTS _consumer_links)
    if(_consumer_link MATCHES "^HyRemote::")
        message(FATAL_ERROR
            "Transparent QPA consumer must not link HyRemote application/runtime targets: ${_consumer_links}")
    endif()
endforeach()

install(TARGETS hyremote-installed-qpa-consumer
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    BUNDLE DESTINATION .
)

# The single installed-SDK deployment entry point owns the proxy plugin/runtime placement.
hyremote_deploy(TARGET hyremote-installed-qpa-consumer QPA)
