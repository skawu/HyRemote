cmake_minimum_required(VERSION 3.21)

project(HyRemoteInstalledQmlConsumer LANGUAGES CXX)

find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Core Gui Qml Quick)
find_package(HyRemote CONFIG REQUIRED)

# The same clean installed-QML consumer is configured twice by GA acceptance: once for the normal
# declarative path and once for the frozen combined `QML QPA` deployment call. Keeping one source
# fixture prevents a second look-alike application from drifting away from the contract it is meant
# to prove.
option(HYREMOTE_CONSUMER_WITH_QPA "Exercise combined installed QML + Transparent QPA deployment" OFF)

if(NOT COMMAND hyremote_deploy)
    message(FATAL_ERROR "Installed HyRemote package must provide hyremote_deploy()")
endif()
if(NOT DEFINED HyRemote_QML_IMPORT_PATH OR "${HyRemote_QML_IMPORT_PATH}" STREQUAL "")
    message(FATAL_ERROR "Installed HyRemote package must publish HyRemote_QML_IMPORT_PATH")
endif()
if(NOT IS_ABSOLUTE "${HyRemote_QML_IMPORT_PATH}")
    message(FATAL_ERROR "HyRemote_QML_IMPORT_PATH must be an absolute installed import root")
endif()
if(HYREMOTE_CONSUMER_WITH_QPA)
    if(NOT DEFINED HyRemote_QPA_AVAILABLE OR NOT HyRemote_QPA_AVAILABLE)
        message(FATAL_ERROR
            "Combined QML+QPA consumer requires an installed HyRemote package with Transparent QPA")
    endif()
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

# This remains the only HyRemote-owned consumer integration call. The normal fixture proves QML
# deployment by itself; the second GA configuration proves the documented combined call carries the
# same QML module, shared RemoteAccess runtime and qhyremote payload together. The application never
# names or copies private HyRemote artifacts.
if(HYREMOTE_CONSUMER_WITH_QPA)
    hyremote_deploy(TARGET hyremote-installed-qml-consumer QML QPA)
else()
    hyremote_deploy(TARGET hyremote-installed-qml-consumer QML)
endif()
