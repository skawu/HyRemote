cmake_minimum_required(VERSION 3.21)
project(HyRemoteLearning02QuickCpp LANGUAGES CXX)

# Same two supported ways of consuming this example as 01: inside the repository build, or standalone against an
# installed SDK. Only the public surface is used.
if(NOT TARGET HyRemote::RemoteAccess)
    find_package(HyRemote CONFIG REQUIRED)
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Quick Qml)

add_executable(hyremote-learning-02-quick main.cpp)
target_compile_features(hyremote-learning-02-quick PRIVATE cxx_std_17)
# Qt6::Qml is the ordinary Qt QML runtime used by the UI. It is NOT the HyRemote QML frontend: nothing here links
# `hyremote-qml` or imports the `HyRemote` URI, because a Quick window does not need the declarative integration.
target_link_libraries(hyremote-learning-02-quick PRIVATE Qt6::Quick Qt6::Qml HyRemote::RemoteAccess)

# The QML file ships as a resource of this example, so the executable needs no QML import path at run time.
qt_add_resources(hyremote-learning-02-quick learning_02_quick_resources
    PREFIX "/hyremote/learning/02-quick-cpp"
    FILES Main.qml
)

install(TARGETS hyremote-learning-02-quick RUNTIME DESTINATION bin)

if(COMMAND hyremote_deploy)
    hyremote_deploy(TARGET hyremote-learning-02-quick)
endif()
