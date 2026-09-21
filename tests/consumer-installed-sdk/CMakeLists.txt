cmake_minimum_required(VERSION 3.21)

project(HyRemoteInstalledConsumer LANGUAGES CXX)

include(GNUInstallDirs)

# Gui is required because a deployable HyRemote application runs a Qt GUI application: the deploy helper resolves
# the native platform plugin through Qt6Gui_DIR, so a fixture that deploys but never asks for Gui would describe an
# application that cannot exist and would hide the platform closure contract.
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Network Gui)
find_package(HyRemote CONFIG REQUIRED)

if(NOT COMMAND hyremote_deploy)
    message(FATAL_ERROR "Installed HyRemote package must provide hyremote_deploy()")
endif()

qt_standard_project_setup(REQUIRES 6.8)

add_executable(hyremote-installed-consumer main.cpp)
target_compile_features(hyremote-installed-consumer PRIVATE cxx_std_17)
target_link_libraries(hyremote-installed-consumer PRIVATE HyRemote::RemoteAccess)

install(TARGETS hyremote-installed-consumer
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    BUNDLE DESTINATION .
)

# One deployment call must be enough for the normal C++ product path: Qt runtime plus the shared
# HyRemoteRemoteAccess facade. Applications never name/copy Core or transport libraries.
hyremote_deploy(TARGET hyremote-installed-consumer)
