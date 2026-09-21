cmake_minimum_required(VERSION 3.21)
project(HyRemoteLearning03GenericWidgets LANGUAGES CXX)

# An ordinary Qt Widgets application. Note what is NOT here: the application target links no HyRemote target and
# includes no HyRemote header, and that stays true whether or not HyRemote is present at all.
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets)

add_executable(hyremote-learning-03-widgets main.cpp)
target_compile_features(hyremote-learning-03-widgets PRIVATE cxx_std_17)
target_link_libraries(hyremote-learning-03-widgets PRIVATE Qt6::Widgets)

install(TARGETS hyremote-learning-03-widgets RUNTIME DESTINATION bin)

# Packaging and deployment integration only. `hyremote_deploy(... GENERIC)` is what makes the zero-code route work:
# it deploys the Generic payload next to the untouched native platform plugin, plus the shared runtime and the Qt
# runtime closure. Requiring it here the way an application would requires the HyRemote package; requiring the
# Generic *payload* additionally needs an SDK that ships it, so the guard keeps this example buildable as a plain
# Qt application in an SDK built without Generic support.
if(NOT COMMAND hyremote_deploy)
    find_package(HyRemote CONFIG QUIET)
endif()
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
