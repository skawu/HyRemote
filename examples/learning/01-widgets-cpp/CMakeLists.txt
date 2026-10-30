cmake_minimum_required(VERSION 3.21)
project(HyRemoteLearning01WidgetsCpp LANGUAGES CXX)

# This example works two ways, and both are supported on purpose:
#
#   * as part of the repository build, where the public product target and the deployment helper already exist;
#   * standalone against an installed SDK, which is what an application developer actually has.
#
# Either way it uses only the public surface: Qt plus HyRemote::RemoteAccess. It never reaches into the repository
# source tree, a product build tree, or an internal HyRemote target.
if(NOT TARGET HyRemote::RemoteAccess)
    find_package(HyRemote CONFIG REQUIRED)
endif()

if(NOT TARGET Qt6::Widgets)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets)
endif()

add_executable(hyremote-learning-01-widgets main.cpp)
# Canonical project-owned mark and application icon; no HyRemote Runtime API is linked for Generic examples.
target_sources(hyremote-learning-01-widgets PRIVATE ../../hyremote_branding.cpp ../../../logo/hyremote-branding.qrc)
set_target_properties(hyremote-learning-01-widgets PROPERTIES AUTORCC ON)
target_compile_features(hyremote-learning-01-widgets PRIVATE cxx_std_17)
target_link_libraries(hyremote-learning-01-widgets PRIVATE Qt6::Widgets HyRemote::RemoteAccess)

install(TARGETS hyremote-learning-01-widgets RUNTIME DESTINATION bin)

# The ordinary deployment call: it carries the shared runtime and the Qt runtime closure the deployed application
# needs, so the deployed tree runs without the SDK on its search path.
if(COMMAND hyremote_deploy AND NOT HYREMOTE_QT_MAJOR STREQUAL "5")
    hyremote_deploy(TARGET hyremote-learning-01-widgets)
elseif(HYREMOTE_QT_MAJOR STREQUAL "5")
    message(STATUS "  hyremote-learning-01-widgets: Qt5 deployment helper is not qualified; build only")
endif()
