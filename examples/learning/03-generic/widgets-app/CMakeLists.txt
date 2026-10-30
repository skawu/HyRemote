cmake_minimum_required(VERSION 3.21)
project(HyRemoteLearning03GenericWidgets LANGUAGES CXX)

# An ordinary Qt Widgets application. Note what is NOT here: the application target links no HyRemote target and
# includes no HyRemote header, and that stays true whether or not HyRemote is present at all.
# Acquire the installed package first so a Qt5 SDK cannot be paired with a system Qt6 by accident.
if(NOT COMMAND hyremote_deploy)
    find_package(HyRemote CONFIG QUIET)
endif()
if(NOT TARGET Qt6::Widgets)
    find_package(Qt6 6.8 QUIET COMPONENTS Core Gui Widgets)
endif()
if(NOT TARGET Qt6::Widgets)
    find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui Widgets)
    add_library(Qt6::Core ALIAS Qt5::Core)
    add_library(Qt6::Gui ALIAS Qt5::Gui)
    add_library(Qt6::Widgets ALIAS Qt5::Widgets)
endif()

add_executable(hyremote-learning-03-widgets main.cpp)
# Canonical project-owned mark and application icon; no HyRemote Runtime API is linked for Generic examples.
target_sources(hyremote-learning-03-widgets PRIVATE ../../../hyremote_branding.cpp ../../../../logo/hyremote-branding.qrc)
# #241: the native Windows file icon is separate acceptance from the Qt window icon.
if(WIN32 AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../../../logo/hyremote-app-icon.rc")
    target_sources(hyremote-learning-03-widgets PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/../../../../logo/hyremote-app-icon.rc")
endif()
set_target_properties(hyremote-learning-03-widgets PROPERTIES AUTORCC ON)
target_compile_features(hyremote-learning-03-widgets PRIVATE cxx_std_17)
target_link_libraries(hyremote-learning-03-widgets PRIVATE Qt6::Widgets)

install(TARGETS hyremote-learning-03-widgets RUNTIME DESTINATION bin)

# Packaging and deployment integration only. `hyremote_deploy(... GENERIC)` is what makes the zero-code route work:
# it deploys the Generic payload next to the untouched native platform plugin, plus the shared runtime and the Qt
# runtime closure. Requiring it here the way an application would requires the HyRemote package; requiring the
# Generic *payload* additionally needs an SDK that ships it, so the guard keeps this example buildable as a plain
# Qt application in an SDK built without Generic support.
set(_generic_deploy_available FALSE)
if(COMMAND hyremote_deploy)
    if(TARGET hyremote-generic-plugin)
        # Repository build with the Generic frontend enabled.
        set(_generic_deploy_available TRUE)
    elseif(DEFINED HyRemote_GENERIC_AVAILABLE)
        # Installed SDK: the package metadata states whether it ships the Generic payload.
        set(_generic_deploy_available "${HyRemote_GENERIC_AVAILABLE}")
    endif()
endif()
if(_generic_deploy_available)
    hyremote_deploy(TARGET hyremote-learning-03-widgets GENERIC)
else()
    message(STATUS
        "  hyremote-learning-03-widgets: building as a plain Qt application; Generic deployment is unavailable in "
        "this SDK (configure with HYREMOTE_WITH_GENERIC_PLUGIN=ON to deploy the zero-code route)")
endif()
