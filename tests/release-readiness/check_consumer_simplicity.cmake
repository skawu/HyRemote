cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

# V1 normal use is deliberately small: one shared C++ facade or the qhyremote plugin. A plain source
# configure builds the product, not repository tests/examples/spikes, and an add_subdirectory()
# consumer must not need to know internal component switches to obtain the standard C++ runtime.
file(READ "${HYREMOTE_SOURCE_DIR}/cmake/HyRemoteProjectOptions.cmake" options_text)
set(required_option_tokens
    [=[option(HYREMOTE_BUILD_TESTS "Build HyRemote tests" OFF)]=]
    [=[option(HYREMOTE_BUILD_EXAMPLES "Build HyRemote examples" OFF)]=]
    [=[option(HYREMOTE_BUILD_CORE "Build the internal hyremote-core session/frame/dispatch library" ON)]=]
    [=[option(HYREMOTE_BUILD_REMOTE_ACCESS "Build the public HyRemote::RemoteAccess C++ facade when Qt is available" ON)]=]
    [=[option(HYREMOTE_BUILD_WIDGETS_ADAPTER "Build the Qt Widgets target adapter when Qt Widgets is available" ON)]=]
    [=[option(HYREMOTE_BUILD_QUICK_ADAPTER "Build the Qt Quick target adapter when Qt Quick is available" ON)]=]
    [=[option(HYREMOTE_WITH_VNC "Enable the VNC/RFB correctness transport backend" ON)]=]
    [=[option(HYREMOTE_BUILD_QML_API "Build the declarative 'import HyRemote' QML API when Qt Qml is available" OFF)]=]
    [=[option(HYREMOTE_WITH_QPA_PROXY "Enable the Transparent QPA Proxy integration mode" OFF)]=]
    [=[option(HYREMOTE_BUILD_SPIKES "Build throwaway architecture spike harnesses (non-production)" OFF)]=]
)
foreach(required_token IN LISTS required_option_tokens)
    string(FIND "${options_text}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "consumer-simplicity: required V1 option/default contract missing: ${required_token}")
    endif()
endforeach()

# Do not let the old top-level-versus-subproject developer default return. It makes a plain source
# build unexpectedly compile tests/examples and creates two different default products.
foreach(forbidden_token
        "_hyremote_developer_default"
        "set(HYREMOTE_BUILD_TESTS ON"
        "set(HYREMOTE_BUILD_EXAMPLES ON")
    string(FIND "${options_text}" "${forbidden_token}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "consumer-simplicity: product-only default build regressed: ${forbidden_token}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/CMakeLists.txt" root_cmake)
set(required_root_tokens
    [=[if(HYREMOTE_BUILD_TESTS)
    include(CTest)]=]
    [=[if(HYREMOTE_BUILD_REMOTE_ACCESS)]=]
    [=[if(HYREMOTE_WITH_QPA_PROXY)]=]
    [=[if(NOT PROJECT_VERSION STREQUAL "0.0.0")]=]
    [=[PROJECT_VERSION VERSION_LESS "0.0.2.0" AND HYREMOTE_BUILD_QML_API]=]
    [=[PROJECT_VERSION VERSION_LESS "0.0.3.0" AND HYREMOTE_WITH_QPA_PROXY]=]
)
foreach(required_token IN LISTS required_root_tokens)
    string(FIND "${root_cmake}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "consumer-simplicity: root build/release graph lost required contract: ${required_token}")
    endif()
endforeach()

# Milestone source history may already contain later V1 implementation, but a tagged release must not
# let a user enable a product mode that has not reached its own accepted milestone yet. The project
# version itself is the release profile; no additional public profile switch is allowed.
foreach(forbidden_token
        "HYREMOTE_RELEASE_PROFILE"
        "HYREMOTE_PRODUCT_PROFILE")
    string(FIND "${root_cmake}" "${forbidden_token}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "consumer-simplicity: release profile must be derived from project version, not another user option: ${forbidden_token}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/tests/consumer-source/CMakeLists.txt" source_consumer)
foreach(forbidden_token
        "set(HYREMOTE_BUILD_TESTS"
        "set(HYREMOTE_BUILD_EXAMPLES"
        "set(HYREMOTE_BUILD_SPIKES"
        "set(HYREMOTE_BUILD_CORE"
        "set(HYREMOTE_BUILD_REMOTE_ACCESS"
        "set(HYREMOTE_BUILD_WIDGETS_ADAPTER"
        "set(HYREMOTE_BUILD_QUICK_ADAPTER"
        "set(HYREMOTE_WITH_VNC")
    string(FIND "${source_consumer}" "${forbidden_token}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "consumer-simplicity: source fixture is hiding a bad standard-product default with manual setup: ${forbidden_token}")
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

message(STATUS
    "HyRemote consumer-simplicity gate: PASS "
    "(product-only defaults, one public C++ target, version-derived milestone profiles)")
