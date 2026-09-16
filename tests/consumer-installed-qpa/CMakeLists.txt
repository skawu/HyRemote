cmake_minimum_required(VERSION 3.21)

project(HyRemoteInstalledQpaConsumer LANGUAGES CXX)

include(GNUInstallDirs)

# Transparent QPA remains an ordinary Qt application at the source/link layer. HyRemote is found
# only for package/deployment metadata; the executable itself links Qt Widgets and nothing HyRemote.
find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Widgets)
find_package(HyRemote CONFIG REQUIRED)

# Use Qt's standard deployable application layout. On ELF platforms this gives the executable the
# normal origin-relative bin -> lib lookup; HyRemote's helper only adds qhyremote + RemoteAccess.
qt_standard_project_setup(REQUIRES 6.8)

if(NOT HyRemote_QPA_AVAILABLE)
    message(FATAL_ERROR "installed HyRemote SDK does not contain the Transparent QPA package")
endif()
if(TARGET HyRemote::QpaPlatform)
    message(FATAL_ERROR
        "installed V1 SDK must not expose qhyremote as a consumer CMake link target")
endif()
if(NOT DEFINED HyRemote_QPA_PLUGIN_FILE OR "${HyRemote_QPA_PLUGIN_FILE}" STREQUAL "")
    message(FATAL_ERROR "installed HyRemote SDK did not publish its QPA payload location")
endif()
if(NOT IS_ABSOLUTE "${HyRemote_QPA_PLUGIN_FILE}" OR NOT EXISTS "${HyRemote_QPA_PLUGIN_FILE}")
    message(FATAL_ERROR
        "installed HyRemote QPA payload path is invalid: ${HyRemote_QPA_PLUGIN_FILE}")
endif()

# Do not maintain a second toy application for package acceptance. Compile the exact E4
# `qpa-proxy-existing-app` application source through this clean external project instead. The E4
# source stays Qt-only; this fixture contributes only installed-package discovery/deployment and a
# bounded executable name expected by the product-fit harness.
set(_hyremote_e4_source
    "${CMAKE_CURRENT_LIST_DIR}/../../examples/qpa-proxy-existing-app/main.cpp")
if(NOT EXISTS "${_hyremote_e4_source}")
    message(FATAL_ERROR "E4 source is missing: ${_hyremote_e4_source}")
endif()

add_executable(hyremote-installed-qpa-consumer
    "${_hyremote_e4_source}"
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
