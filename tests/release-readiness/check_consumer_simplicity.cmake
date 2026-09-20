cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

# V1 normal use is deliberately small: one shared C++ facade or the qhyremote plugin. A plain source
# configure builds the product, not repository tests/examples/research, and an add_subdirectory()
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
    [=[option(HYREMOTE_WITH_TRANSPORT_SECURITY "Enable authenticated and encrypted transport (OpenSSL from your environment, or the project source tree)" OFF)]=]
    # The OpenSSL provider choice is part of the consumer contract: the user's own environment first, the
    # project's own source tree last, and a valid build with the capability reported unavailable when neither
    # is there. Pinning the default and the three accepted settings keeps that order from drifting.
    [=[set(HYREMOTE_OPENSSL_PROVIDER "AUTO" CACHE STRING]=]
    [=[set_property(CACHE HYREMOTE_OPENSSL_PROVIDER PROPERTY STRINGS AUTO SYSTEM BUNDLED)]=]
)
foreach(required_token IN LISTS required_option_tokens)
    string(FIND "${options_text}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "consumer-simplicity: required V1 option/default contract missing: ${required_token}")
    endif()
endforeach()

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
    [=[include(HyRemoteReleaseProfile)]=]
    [=[hyremote_validate_release_profile(]=]
    [=[VERSION "${PROJECT_VERSION}"]=]
    [=[QML_ENABLED "${HYREMOTE_BUILD_QML_API}"]=]
    [=[QPA_ENABLED "${HYREMOTE_WITH_QPA_PROXY}"]=]
    [=[if(HYREMOTE_BUILD_TESTS)
    include(CTest)]=]
    [=[if(HYREMOTE_BUILD_REMOTE_ACCESS)]=]
    [=[if(HYREMOTE_WITH_QPA_PROXY)]=]
    [=[add_subdirectory(src/core core)]=]
    [=[add_subdirectory(src/remoteaccess remoteaccess)]=]
    [=[add_subdirectory(src/qml/HyRemote qml/HyRemote)]=]
    [=[add_subdirectory(src/qpa qpa)]=]
    [=[hyremote-release-profile-v001-reject-qml]=]
    [=[hyremote-release-profile-v002-reject-qpa]=]
    [=[hyremote-release-profile-v100-all-modes]=]
)
foreach(required_token IN LISTS required_root_tokens)
    string(FIND "${root_cmake}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "consumer-simplicity: root build/release graph lost required contract: ${required_token}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/cmake/HyRemoteReleaseProfile.cmake" release_profile)
foreach(required_token
        [=[HYREMOTE_PROFILE_VERSION STREQUAL "0.0.0"]=]
        [=[HYREMOTE_PROFILE_VERSION VERSION_LESS "0.0.2.0" AND HYREMOTE_PROFILE_QML_ENABLED]=]
        [=[HYREMOTE_PROFILE_VERSION VERSION_LESS "0.0.3.0" AND HYREMOTE_PROFILE_QPA_ENABLED]=])
    string(FIND "${release_profile}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "consumer-simplicity: milestone release-profile boundary missing: ${required_token}")
    endif()
endforeach()
foreach(forbidden_token
        "HYREMOTE_RELEASE_PROFILE"
        "HYREMOTE_PRODUCT_PROFILE")
    string(FIND "${options_text}${root_cmake}${release_profile}" "${forbidden_token}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "consumer-simplicity: release profile must be derived from project version, not another user option: ${forbidden_token}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/tests/consumer-source/CMakeLists.txt" source_consumer)
foreach(forbidden_token
        "set(HYREMOTE_BUILD_TESTS"
        "set(HYREMOTE_BUILD_EXAMPLES"
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

# Source and installed QML acquisition publish the same abstract import-root input to the one deploy
# helper. Source payload targets are internal build metadata: they may become build-only dependencies
# of the consumer target, but never application link targets or installed SDK choices.
file(READ "${HYREMOTE_SOURCE_DIR}/src/qml/HyRemote/CMakeLists.txt" qml_cmake)
foreach(required_token
        "_hyremote_qml_build_import_root"
        "HyRemote_QML_IMPORT_PATH"
        "HYREMOTE_QML_SOURCE_DEPLOY_TARGETS"
        [=["hyremote-qml;${HYREMOTE_QML_PLUGIN_TARGET}"]=]
        [=[CACHE INTERNAL
    "HyRemote QML import root for source-tree deployment" FORCE)]=])
    string(FIND "${qml_cmake}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "consumer-simplicity: source QML module lost deploy metadata: ${required_token}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/src/qpa/CMakeLists.txt" qpa_cmake)
foreach(required_token
        [=[BUILD_RPATH "$ORIGIN/../../../."]=]
        [=[BUILD_RPATH_USE_ORIGIN TRUE]=]
        [=[INSTALL_RPATH "$ORIGIN/../../../."]=])
    string(FIND "${qpa_cmake}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "consumer-simplicity: qhyremote lost its source/install relocation anchor: ${required_token}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/cmake/HyRemoteDeploy.cmake" deploy_helper)
foreach(required_token
        "_hyremote_add_local_build_dependency"
        "ALIASED_TARGET"
        "IMPORTED"
        "add_dependencies"
        "HYREMOTE_QML_SOURCE_DEPLOY_TARGETS"
        "HyRemote::RemoteAccess"
        "HyRemote::QpaPlatform"
        [=[file(RPATH_CHANGE]=]
        [=[OLD_RPATH \"$ORIGIN/../../../.\"]=]
        [=[NEW_RPATH \"$ORIGIN/../../\${QT_DEPLOY_LIB_DIR}\"]=])
    string(FIND "${deploy_helper}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "consumer-simplicity: source/deployed payload wiring lost required contract: ${required_token}")
    endif()
endforeach()
string(FIND "${deploy_helper}" "file(READ_ELF" undocumented_readelf)
if(NOT undocumented_readelf EQUAL -1)
    message(FATAL_ERROR
        "consumer-simplicity: deployment must not depend on CMake's undocumented READ_ELF mode")
endif()

file(READ "${HYREMOTE_SOURCE_DIR}/tests/consumer-installed-qml/CMakeLists.txt" qml_consumer)
foreach(required_token
        "HYREMOTE_CONSUMER_SOURCE_DIR"
        "HYREMOTE_CONSUMER_WITH_QPA"
        "set(HYREMOTE_BUILD_QML_API ON"
        [=[add_subdirectory("${HYREMOTE_CONSUMER_SOURCE_DIR}" hyremote-source EXCLUDE_FROM_ALL)]=]
        "HyRemote_QML_IMPORT_PATH"
        [=[if(_consumer_link MATCHES "^HyRemote::")]=]
        "hyremote_deploy(TARGET hyremote-installed-qml-consumer QML)"
        "hyremote_deploy(TARGET hyremote-installed-qml-consumer QML QPA)")
    string(FIND "${qml_consumer}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "consumer-simplicity: clean QML fixture lost installed/source contract evidence: ${required_token}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/tests/consumer-installed-qpa/CMakeLists.txt" qpa_consumer)
foreach(required_token
        "HYREMOTE_CONSUMER_SOURCE_DIR"
        "set(HYREMOTE_WITH_QPA_PROXY ON"
        [=[add_subdirectory("${HYREMOTE_CONSUMER_SOURCE_DIR}" hyremote-source EXCLUDE_FROM_ALL)]=]
        [=[if(_consumer_link MATCHES "^HyRemote::")]=]
        "hyremote_deploy(TARGET hyremote-installed-qpa-consumer QPA)")
    string(FIND "${qpa_consumer}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "consumer-simplicity: clean QPA fixture lost installed/source contract evidence: ${required_token}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/tests/consumer-installed-qml/main.cpp" qml_consumer_main)
foreach(required_token
        "contractOk"
        "--test-seconds"
        "loadedProductLibrariesComeFromDeployment"
        "libhyremote-qml.so")
    string(FIND "${qml_consumer_main}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "consumer-simplicity: clean QML consumer lost runtime/deployment proof: ${required_token}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/.github/workflows/v1-ga-acceptance.yml" ga_workflow)
foreach(required_token
        "Installed combined QML + QPA consumer — Linux"
        "Installed combined QML + QPA consumer — Windows"
        "-DHYREMOTE_CONSUMER_WITH_QPA=ON"
        "ga-qml-qpa-consumer.log")
    string(FIND "${ga_workflow}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "consumer-simplicity: integrated GA lost combined installed QML+QPA evidence: ${required_token}")
    endif()
endforeach()

# #39 source-consumption evidence must execute all four public deployment call shapes, not infer QML
# from the QML+QPA case because those paths use different supplemental deployment scripts.
file(READ "${HYREMOTE_SOURCE_DIR}/.github/workflows/sdk-consumption.yml" sdk_workflow)
foreach(required_token
        "Source shape 1/4: Embedded C++"
        "Source shape 2/4: Declarative QML only"
        "Source shape 3/4: Transparent QPA"
        "Source shape 4/4: QML + QPA"
        "build-consumer-source-qml"
        "build-consumer-source-qpa"
        "build-consumer-source-qml-qpa"
        "HYREMOTE_CONSUMER_SOURCE_DIR"
        "--port 5994"
        "--port 5995")
    string(FIND "${sdk_workflow}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "consumer-simplicity: SDK workflow lost executable source deployment evidence: ${required_token}")
    endif()
endforeach()

message(STATUS
    "HyRemote consumer-simplicity gate: PASS "
    "(product-only defaults, canonical repository layout, one public C++ target, four deployment shapes, build-only source payload wiring, installed/source QML+QPA acquisition, executable version-derived milestone profiles)")
