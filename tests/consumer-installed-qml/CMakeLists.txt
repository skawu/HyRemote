cmake_minimum_required(VERSION 3.21)

project(HyRemoteInstalledQmlConsumer LANGUAGES CXX)

find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Core Gui Qml Quick)
find_package(HyRemote CONFIG REQUIRED)

if(NOT COMMAND hyremote_deploy)
    message(FATAL_ERROR "Installed HyRemote package must provide hyremote_deploy()")
endif()
if(NOT DEFINED HyRemote_QML_IMPORT_PATH OR "${HyRemote_QML_IMPORT_PATH}" STREQUAL "")
    message(FATAL_ERROR "Installed HyRemote package must publish HyRemote_QML_IMPORT_PATH")
endif()
if(NOT IS_ABSOLUTE "${HyRemote_QML_IMPORT_PATH}")
    message(FATAL_ERROR "HyRemote_QML_IMPORT_PATH must be an absolute installed import root")
endif()

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(hyremote-installed-qml-consumer
    main.cpp
)

qt_policy(SET QTP0001 NEW)
qt_add_qml_module(hyremote-installed-qml-consumer
    URI HyRemoteInstalledConsumer
    VERSION 1.0
    QML_FILES Main.qml
)

target_compile_features(hyremote-installed-qml-consumer PRIVATE cxx_std_17)
target_link_libraries(hyremote-installed-qml-consumer PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
)

install(TARGETS hyremote-installed-qml-consumer
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

# This is deliberately the only HyRemote-owned consumer integration call. The helper bridges the
# installed module root into Qt deployment and also carries the same shared RemoteAccess runtime used
# by the QML module. The consumer does not name/copy HyRemote plugin/runtime files itself.
hyremote_deploy(TARGET hyremote-installed-qml-consumer QML)
