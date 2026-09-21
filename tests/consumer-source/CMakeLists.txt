cmake_minimum_required(VERSION 3.21)

project(HyRemoteSourceConsumer LANGUAGES CXX)

if(NOT HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "Configure with -DHYREMOTE_SOURCE_DIR=<HyRemote source tree>")
endif()

include(GNUInstallDirs)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Network)

# Do not preconfigure HyRemote's developer-only switches here. This fixture intentionally proves
# that a normal add_subdirectory() consumer gets the product facade without inheriting HyRemote's
# own tests or examples into the application build.
add_subdirectory("${HYREMOTE_SOURCE_DIR}" hyremote EXCLUDE_FROM_ALL)

if(HYREMOTE_BUILD_TESTS)
    message(FATAL_ERROR "source consumer unexpectedly inherited HyRemote tests")
endif()
if(HYREMOTE_BUILD_EXAMPLES)
    message(FATAL_ERROR "source consumer unexpectedly inherited HyRemote examples")
endif()
if(NOT TARGET HyRemote::RemoteAccess)
    message(FATAL_ERROR "source consumption must provide the HyRemote::RemoteAccess product target")
endif()
if(NOT COMMAND hyremote_deploy)
    message(FATAL_ERROR "HyRemote source consumption must provide hyremote_deploy()")
endif()

qt_standard_project_setup(REQUIRES 6.8)

add_executable(hyremote-source-consumer main.cpp)
target_compile_features(hyremote-source-consumer PRIVATE cxx_std_17)
target_link_libraries(hyremote-source-consumer PRIVATE HyRemote::RemoteAccess)

install(TARGETS hyremote-source-consumer
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    BUNDLE DESTINATION .
)

# Source/vendored consumption has the same deployment experience as an installed SDK: one public
# target and one HyRemote deployment call, with no manual DLL/SO copying or backend assembly.
hyremote_deploy(TARGET hyremote-source-consumer)
