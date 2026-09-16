cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

# V1 normal use is deliberately small: one shared C++ facade or the qhyremote plugin. Internal
# developer/test switches must not become setup work for an application that vendors HyRemote.
file(READ "${HYREMOTE_SOURCE_DIR}/cmake/HyRemoteProjectOptions.cmake" options_text)
set(required_option_tokens
    [=[if(PROJECT_IS_TOP_LEVEL)]=]
    [=[set(_hyremote_developer_default ON)]=]
    [=[set(_hyremote_developer_default OFF)]=]
    [=[option(HYREMOTE_BUILD_TESTS "Build HyRemote tests" ${_hyremote_developer_default})]=]
    [=[option(HYREMOTE_BUILD_EXAMPLES "Build HyRemote examples" ${_hyremote_developer_default})]=]
    [=[option(HYREMOTE_BUILD_CORE "Build the internal hyremote-core session/frame/dispatch library" ON)]=]
    [=[option(HYREMOTE_BUILD_REMOTE_ACCESS "Build the public HyRemote::RemoteAccess C++ facade when Qt is available" ON)]=]
    [=[option(HYREMOTE_BUILD_WIDGETS_ADAPTER "Build the Qt Widgets target adapter when Qt Widgets is available" ON)]=]
    [=[option(HYREMOTE_BUILD_QUICK_ADAPTER "Build the Qt Quick target adapter when Qt Quick is available" ON)]=]
    [=[option(HYREMOTE_WITH_VNC "Enable the VNC/RFB correctness transport backend" ON)]=]
    [=[option(HYREMOTE_BUILD_QML_API "Build the declarative 'import HyRemote' QML API when Qt Qml is available" OFF)]=]
    [=[option(HYREMOTE_WITH_QPA_PROXY "Enable the Transparent QPA Proxy integration mode" OFF)]=]
)
foreach(required_token IN LISTS required_option_tokens)
    string(FIND "${options_text}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "consumer-simplicity: required V1 option/default contract missing: ${required_token}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/CMakeLists.txt" root_cmake)
set(required_root_tokens
    [=[if(HYREMOTE_BUILD_TESTS)
    include(CTest)]=]
    [=[if(HYREMOTE_BUILD_REMOTE_ACCESS)]=]
    [=[if(HYREMOTE_WITH_QPA_PROXY)]=]
)
foreach(required_token IN LISTS required_root_tokens)
    string(FIND "${root_cmake}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "consumer-simplicity: root build graph lost required product/developer separation: ${required_token}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/tests/consumer-source/CMakeLists.txt" source_consumer)
foreach(forbidden_token
        "set(HYREMOTE_BUILD_TESTS"
        "set(HYREMOTE_BUILD_EXAMPLES"
        "set(HYREMOTE_BUILD_SPIKES"
        "set(HYREMOTE_BUILD_CORE"
        "set(HYREMOTE_BUILD_REMOTE_ACCESS")
    string(FIND "${source_consumer}" "${forbidden_token}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "consumer-simplicity: source fixture is hiding a bad default with manual setup: ${forbidden_token}")
    endif()
endforeach()
set(required_source_tokens
    [=[add_subdirectory("${HYREMOTE_SOURCE_DIR}" hyremote EXCLUDE_FROM_ALL)]=]
    [=[TARGET HyRemote::RemoteAccess]=]
    [=[hyremote_deploy(TARGET hyremote-source-consumer)]=]
)
foreach(required_token IN LISTS required_source_tokens)
    string(FIND "${source_consumer}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "consumer-simplicity: source fixture no longer proves the simple V1 path: ${required_token}")
    endif()
endforeach()

# Installed CMake surface must not reintroduce alternate low-level/QPA/runtime-personality choices.
file(READ "${HYREMOTE_SOURCE_DIR}/cmake/HyRemoteConfig.cmake.in" package_config)
foreach(forbidden_token
        "HyRemote::Core"
        "HyRemote::QpaPlatform"
        "HyRemote_QPA_SHARED_RUNTIME"
        "COMPONENTS Widgets"
        "COMPONENTS Quick"
        "COMPONENTS Qml")
    string(FIND "${package_config}" "${forbidden_token}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "consumer-simplicity: installed package leaked a non-product choice: ${forbidden_token}")
    endif()
endforeach()
foreach(required_token
        "HyRemote_QPA_AVAILABLE"
        "HyRemote_QPA_QT_VERSION"
        "HyRemote_QPA_PLUGIN_FILE"
        "HyRemote_QML_IMPORT_PATH")
    string(FIND "${package_config}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "consumer-simplicity: installed package lost required payload metadata: ${required_token}")
    endif()
endforeach()

message(STATUS "HyRemote consumer-simplicity gate: PASS")
