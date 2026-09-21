cmake_minimum_required(VERSION 3.21)
project(HyRemoteLearning03GenericQuick LANGUAGES CXX)

# An ordinary Qt Quick application: no HyRemote target is linked and no HyRemote header is included.
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Quick Qml)

add_executable(hyremote-learning-03-quick main.cpp)
target_compile_features(hyremote-learning-03-quick PRIVATE cxx_std_17)
target_link_libraries(hyremote-learning-03-quick PRIVATE Qt6::Quick Qt6::Qml)

qt_add_resources(hyremote-learning-03-quick learning_03_quick_resources
    PREFIX "/hyremote/learning/03-zero-code-generic"
    FILES Main.qml
)

install(TARGETS hyremote-learning-03-quick RUNTIME DESTINATION bin)

# Deployment integration only; the application source stays Qt-only. See the Widgets application for why the guard
# keeps this buildable as a plain Qt application when the SDK has no Generic payload.
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
    hyremote_deploy(TARGET hyremote-learning-03-quick GENERIC)
else()
    message(STATUS
        "  hyremote-learning-03-quick: building as a plain Qt application; Generic deployment is unavailable in this "
        "SDK (configure with HYREMOTE_WITH_GENERIC_PLUGIN=ON to deploy the zero-code route)")
endif()
