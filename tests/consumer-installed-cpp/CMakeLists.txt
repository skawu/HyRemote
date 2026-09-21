cmake_minimum_required(VERSION 3.21)
project(HyRemoteInstalledCppConsumer LANGUAGES CXX)

# A C++ consumer acquires HyRemote only through the installed package and links only the public product target:
# Qt plus HyRemote::RemoteAccess. Nothing here reaches into the repository source tree, the product build tree or
# an internal HyRemote target, and nothing links the QML frontend - a Quick user window is a C++ Quick window, not
# the declarative HyRemote API.
#
# One fixture builds exactly one target, selected by the evidence cell that drives it. A fixture that always built
# both kinds would make every cell ambiguous about which executable it was actually proving.
if(NOT DEFINED HYREMOTE_CPP_CONSUMER_KIND)
    message(FATAL_ERROR "HYREMOTE_CPP_CONSUMER_KIND must be 'widgets' or 'quick'")
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui)
find_package(HyRemote CONFIG REQUIRED)

if(NOT TARGET HyRemote::RemoteAccess)
    message(FATAL_ERROR "Installed HyRemote package must export HyRemote::RemoteAccess")
endif()
if(NOT COMMAND hyremote_deploy)
    message(FATAL_ERROR "Installed HyRemote package must provide hyremote_deploy()")
endif()

if(HYREMOTE_CPP_CONSUMER_KIND STREQUAL "widgets")
    set(consumer_target hyremote-cpp-widgets-consumer)
    set(consumer_source widgets_main.cpp)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)
    set(consumer_libraries Qt6::Core Qt6::Gui Qt6::Widgets HyRemote::RemoteAccess)
elseif(HYREMOTE_CPP_CONSUMER_KIND STREQUAL "quick")
    set(consumer_target hyremote-cpp-quick-consumer)
    set(consumer_source quick_main.cpp)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Quick)
    set(consumer_libraries Qt6::Core Qt6::Gui Qt6::Quick HyRemote::RemoteAccess)
else()
    message(FATAL_ERROR
        "HYREMOTE_CPP_CONSUMER_KIND must be 'widgets' or 'quick', got '${HYREMOTE_CPP_CONSUMER_KIND}'")
endif()

add_executable(${consumer_target} ${consumer_source})
target_compile_features(${consumer_target} PRIVATE cxx_std_17)
target_link_libraries(${consumer_target} PRIVATE ${consumer_libraries})

install(TARGETS ${consumer_target} RUNTIME DESTINATION bin)

# One deployment family, one call: the shared runtime plus the Qt runtime closure the deployed application needs,
# so the deployed tree runs without the HyRemote SDK or the Qt SDK on its search path.
hyremote_deploy(TARGET ${consumer_target})
