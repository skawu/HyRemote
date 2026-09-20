cmake_minimum_required(VERSION 3.21)

project(HyRemoteInstalledQmlConsumer LANGUAGES CXX)

# The same clean external QML application is reused across acquisition/deployment combinations.
# Default: installed SDK + QML. GA also configures installed QML+QPA, source QML, and source QML+QPA
# without duplicating application source or inventing another integration surface.
option(HYREMOTE_CONSUMER_WITH_QPA "Exercise combined QML + QPA deployment" OFF)
set(HYREMOTE_CONSUMER_SOURCE_DIR "" CACHE PATH
    "Optional HyRemote source tree; when set, exercise add_subdirectory() instead of find_package()")

find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Core Gui Qml Quick)
qt_standard_project_setup(REQUIRES 6.8)

if(HYREMOTE_CONSUMER_SOURCE_DIR)
    if(NOT EXISTS "${HYREMOTE_CONSUMER_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "HYREMOTE_CONSUMER_SOURCE_DIR is not a HyRemote source tree: ${HYREMOTE_CONSUMER_SOURCE_DIR}")
    endif()

    # Declarative/QPA packages are optional source-consumption modes. Select only the modes under
    # test; ordinary HyRemote tests/examples/spikes remain at product defaults.
    set(HYREMOTE_BUILD_QML_API ON CACHE BOOL "" FORCE)
    if(HYREMOTE_CONSUMER_WITH_QPA)
        set(HYREMOTE_WITH_QPA_PROXY ON CACHE BOOL "" FORCE)
    endif()
    add_subdirectory("${HYREMOTE_CONSUMER_SOURCE_DIR}" hyremote-source EXCLUDE_FROM_ALL)

    if(NOT COMMAND hyremote_deploy)
        message(FATAL_ERROR "HyRemote source consumption must provide hyremote_deploy()")
    endif()
    if(NOT TARGET hyremote-qml)
        message(FATAL_ERROR "source HyRemote QML selection did not create the HyRemote module")
    endif()
    if(NOT DEFINED HyRemote_QML_IMPORT_PATH OR "${HyRemote_QML_IMPORT_PATH}" STREQUAL "")
        message(FATAL_ERROR "source HyRemote QML selection did not publish its build import root")
    endif()
    if(NOT IS_ABSOLUTE "${HyRemote_QML_IMPORT_PATH}")
        message(FATAL_ERROR "source HyRemote QML import root must be absolute")
    endif()
    if(HYREMOTE_CONSUMER_WITH_QPA AND NOT TARGET HyRemote::QpaPlatform)
        message(FATAL_ERROR "source combined QML+QPA selection did not create qhyremote")
    endif()
else()
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
    if(HYREMOTE_CONSUMER_WITH_QPA)
        if(NOT DEFINED HyRemote_QPA_AVAILABLE OR NOT HyRemote_QPA_AVAILABLE)
            message(FATAL_ERROR
                "Combined QML+QPA consumer requires an installed HyRemote package with QPA")
        endif()
    endif()
endif()

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

# Declarative and transparent-QPA application code must remain link-neutral with respect to HyRemote;
# the QML import and platform payload are deployment/runtime surfaces, not extra C++ SDK targets.
get_target_property(_consumer_links hyremote-installed-qml-consumer LINK_LIBRARIES)
foreach(_consumer_link IN LISTS _consumer_links)
    if(_consumer_link MATCHES "^HyRemote::")
        message(FATAL_ERROR
            "QML/QPA consumer must not link HyRemote C++ targets: ${_consumer_links}")
    endif()
endforeach()

install(TARGETS hyremote-installed-qml-consumer
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

# This remains the only HyRemote-owned consumer integration call. Acquisition changes only how the
# payload is obtained; QML and QML+QPA use the same public deployment helper in source and installed
# forms, and the application never names or copies private HyRemote artifacts.
if(HYREMOTE_CONSUMER_WITH_QPA)
    hyremote_deploy(TARGET hyremote-installed-qml-consumer QML QPA)
else()
    hyremote_deploy(TARGET hyremote-installed-qml-consumer QML)
endif()
