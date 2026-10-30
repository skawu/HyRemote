cmake_minimum_required(VERSION 3.21)
project(HyRemoteLearning03GenericQuick LANGUAGES CXX)

# An ordinary Qt Quick application: no HyRemote target is linked and no HyRemote header is included.
if(NOT TARGET Qt6::Quick OR NOT TARGET Qt6::Qml)
    find_package(Qt6 6.8 QUIET COMPONENTS Core Gui Quick Qml)
endif()
if(NOT TARGET Qt6::Quick OR NOT TARGET Qt6::Qml)
    find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui Quick Qml)
    add_library(Qt6::Core ALIAS Qt5::Core)
    add_library(Qt6::Gui ALIAS Qt5::Gui)
    add_library(Qt6::Quick ALIAS Qt5::Quick)
    add_library(Qt6::Qml ALIAS Qt5::Qml)
endif()

add_executable(hyremote-learning-03-quick main.cpp)
# Canonical project-owned mark and application icon; no HyRemote Runtime API is linked for Generic examples.
target_sources(hyremote-learning-03-quick PRIVATE ../../../hyremote_branding.cpp ../../../../logo/hyremote-branding.qrc)
# #241: the native Windows file icon is separate acceptance from the Qt window icon.
if(WIN32 AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../../../logo/hyremote-app-icon.rc")
    target_sources(hyremote-learning-03-quick PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/../../../../logo/hyremote-app-icon.rc")
endif()
set_target_properties(hyremote-learning-03-quick PROPERTIES AUTORCC ON)
target_compile_features(hyremote-learning-03-quick PRIVATE cxx_std_17)
target_link_libraries(hyremote-learning-03-quick PRIVATE Qt6::Quick Qt6::Qml)

qt_add_resources(hyremote-learning-03-quick learning_03_quick_resources
    PREFIX "/hyremote/learning/03-generic"
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
