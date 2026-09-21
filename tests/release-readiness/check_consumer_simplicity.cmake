cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

# Normal source consumption remains small: the shared runtime plus one application-facing frontend.
# Frontend grouping and the fourth Generic mode must not force consumers to select internal Core/adapters.
file(READ "${HYREMOTE_SOURCE_DIR}/cmake/HyRemoteProjectOptions.cmake" options_text)
# The contract is each option's identity and its default, and the frontend independence asserted further down. The
# human-readable description is not a contract, so it is no longer pinned character for character: pinning it turned a
# wording change into a release-gate failure while proving nothing about the consumer.
set(required_option_defaults
    HYREMOTE_BUILD_TESTS=OFF
    HYREMOTE_BUILD_EXAMPLES=OFF
    HYREMOTE_BUILD_CORE=ON
    HYREMOTE_BUILD_REMOTE_ACCESS=ON
    HYREMOTE_BUILD_WIDGETS_ADAPTER=ON
    HYREMOTE_BUILD_QUICK_ADAPTER=ON
    HYREMOTE_WITH_VNC=ON
    HYREMOTE_BUILD_QML_API=OFF
    HYREMOTE_WITH_GENERIC_PLUGIN=OFF
    HYREMOTE_WITH_QPA_PROXY=OFF
    HYREMOTE_WITH_TRANSPORT_SECURITY=OFF
)
foreach(required_option IN LISTS required_option_defaults)
    string(REPLACE "=" ";" required_option_parts "${required_option}")
    list(GET required_option_parts 0 required_option_name)
    list(GET required_option_parts 1 required_option_default)
    string(REGEX MATCH "option\\(${required_option_name} \"[^\"]*\" (ON|OFF)\\)" declared_option "${options_text}")
    if(NOT declared_option)
        message(FATAL_ERROR
            "consumer-simplicity: ${required_option_name} is not declared as an option with an explicit default")
    endif()
    if(NOT CMAKE_MATCH_1 STREQUAL "${required_option_default}")
        message(FATAL_ERROR
            "consumer-simplicity: ${required_option_name} default is ${CMAKE_MATCH_1}, the contract default is ${required_option_default}")
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
    [=[GENERIC_ENABLED "${HYREMOTE_WITH_GENERIC_PLUGIN}"]=]
    [=[QPA_ENABLED "${HYREMOTE_WITH_QPA_PROXY}"]=]
    [=[if(HYREMOTE_BUILD_TESTS)
    include(CTest)]=]
    [=[if(HYREMOTE_BUILD_REMOTE_ACCESS)]=]
    [=[if(HYREMOTE_WITH_GENERIC_PLUGIN)]=]
    [=[if(HYREMOTE_WITH_QPA_PROXY)]=]
    [=[add_subdirectory(src/core core)]=]
    [=[add_subdirectory(src/integrations/cpp integrations/cpp)]=]
    [=[add_subdirectory(src/integrations/qml qml/HyRemote)]=]
    [=[add_subdirectory(src/integrations/generic generic)]=]
    [=[add_subdirectory(src/integrations/qpa qpa)]=]
    [=[hyremote-release-profile-retire-v001]=]
    [=[hyremote-release-profile-retire-v002]=]
    [=[hyremote-release-profile-retire-v003]=]
    [=[hyremote-release-profile-v100-all]=]
)
foreach(required_token IN LISTS required_root_tokens)
    string(FIND "${root_cmake}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "consumer-simplicity: root build/release graph lost required contract: ${required_token}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/cmake/HyRemoteReleaseProfile.cmake" release_profile)
# V1 no longer sequences cpp/qml/generic/qpa through VERSION_LESS milestones; applicability is decided by the
# first-GA policy. The boundaries asserted here are the developer label and the rejection of retired pre-GA labels,
# and the removed sequential thresholds are forbidden rather than required, so the retired mechanism cannot return
# while the gate still proves the profile bounds a version.
foreach(required_token
        [=[HYREMOTE_PROFILE_VERSION STREQUAL "0.0.0"]=]
        [=[HYREMOTE_PROFILE_VERSION VERSION_LESS "0.1.0.0"]=])
    string(FIND "${release_profile}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "consumer-simplicity: release-profile boundary missing: ${required_token}")
    endif()
endforeach()
foreach(retired_token
        [=[VERSION_LESS "0.0.2.0"]=]
        [=[VERSION_LESS "0.0.3.0"]=]
        [=[VERSION_LESS "1.0.0.0"]=])
    string(FIND "${release_profile}" "${retired_token}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "consumer-simplicity: sequential frontend milestone returned to the release profile: ${retired_token}")
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
            "consumer-simplicity: source fixture no longer proves the simple product path: ${required_token}")
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

file(READ "${HYREMOTE_SOURCE_DIR}/src/integrations/qml/CMakeLists.txt" qml_cmake)
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

file(READ "${HYREMOTE_SOURCE_DIR}/src/integrations/qpa/CMakeLists.txt" qpa_cmake)
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

# The consolidated CI topology keeps no per-lane acceptance workflow any more, and the combined installed QML + QPA
# consumer moved to the release evidence runner (checklist section 10), which is run on each reference operating
# system. The capability is asserted where it is produced rather than in a retired workflow file.
file(READ "${HYREMOTE_SOURCE_DIR}/tests/release-readiness/run_release_evidence.cmake" evidence_runner)
foreach(required_token
        "installed-qml-qpa"
        "installed-qpa-product-fit"
        "installed-sdk")
    string(FIND "${evidence_runner}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "consumer-simplicity: combined installed consumer evidence lost its executable cell: ${required_token}")
    endif()
endforeach()

# The source shapes are exercised from the source tree by the same runner: one source consumer of the shared runtime
# and one source QPA product-fit, both acquiring HyRemote through its source instead of an installed package.
foreach(required_token
        "source-consumer"
        "source-qpa-product-fit"
        "HYREMOTE_CONSUMER_SOURCE_DIR")
    string(FIND "${evidence_runner}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "consumer-simplicity: source-tree consumer evidence lost its executable cell: ${required_token}")
    endif()
endforeach()

message(STATUS
    "HyRemote consumer-simplicity gate: PASS "
    "(product-only defaults, grouped integration layout, one shared runtime, peer frontends, build-only source payload wiring and executable version-derived release profiles)")
