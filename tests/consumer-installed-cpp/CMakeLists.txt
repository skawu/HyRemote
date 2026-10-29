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

# Acquire the package first so its own Qt-major identity drives the consumer's
# Qt choice. Selecting Qt6 before `find_package(HyRemote)` lets a system Qt6
# masquerade as the consumer's runtime even when the SDK is Qt5.
find_package(HyRemote CONFIG REQUIRED)
if(HyRemote_QT_MAJOR STREQUAL "5")
    set(_hyremote_consumer_qt Qt5)
    find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui Widgets)
else()
    set(_hyremote_consumer_qt Qt6)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets)
endif()

if(NOT TARGET HyRemote::RemoteAccess)
    message(FATAL_ERROR "Installed HyRemote package must export HyRemote::RemoteAccess")
endif()
if(NOT COMMAND hyremote_deploy)
    message(FATAL_ERROR "Installed HyRemote package must provide hyremote_deploy()")
endif()

if(HYREMOTE_CPP_CONSUMER_KIND STREQUAL "widgets")
    set(consumer_target hyremote-cpp-widgets-consumer)
    set(consumer_source widgets_main.cpp)
    if(_hyremote_consumer_qt STREQUAL "Qt5")
        set(consumer_libraries Qt5::Core Qt5::Gui Qt5::Widgets HyRemote::RemoteAccess)
    else()
        set(consumer_libraries Qt6::Core Qt6::Gui Qt6::Widgets HyRemote::RemoteAccess)
    endif()
elseif(HYREMOTE_CPP_CONSUMER_KIND STREQUAL "quick")
    set(consumer_target hyremote-cpp-quick-consumer)
    set(consumer_source quick_main.cpp)
    if(_hyremote_consumer_qt STREQUAL "Qt5")
        find_package(Qt5 5.15 REQUIRED COMPONENTS Quick)
        set(consumer_libraries Qt5::Core Qt5::Gui Qt5::Quick HyRemote::RemoteAccess)
    else()
        find_package(Qt6 6.8 REQUIRED COMPONENTS Quick)
        set(consumer_libraries Qt6::Core Qt6::Gui Qt6::Quick HyRemote::RemoteAccess)
    endif()
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
