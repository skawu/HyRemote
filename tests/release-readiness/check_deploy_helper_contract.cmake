cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

set(required_files
    "CMakeLists.txt"
    "cmake/HyRemoteConfig.cmake.in"
    "cmake/HyRemoteInstall.cmake"
    "cmake/HyRemoteDeploy.cmake"
    "src/integrations/cpp/CMakeLists.txt"
    "src/integrations/qpa/CMakeLists.txt"
    "src/integrations/qpa/tests/deploy_helper_fixture/CMakeLists.txt"
    "src/integrations/qpa/tests/run_deploy_helper_fixture.cmake"
    "tests/release-readiness/check_package_acquisition_isolation.cmake")
foreach(path IN LISTS required_files)
    if(NOT EXISTS "${HYREMOTE_SOURCE_DIR}/${path}")
        message(FATAL_ERROR "deploy-helper-contract: missing evidence file: ${path}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/CMakeLists.txt" root_cmake)
foreach(required_token
        [=[unset(HyRemote_QML_IMPORT_PATH CACHE)]=]
        [=[set_property(GLOBAL PROPERTY HYREMOTE_QML_SOURCE_DEPLOY_TARGETS "")]=]
        [=[hyremote-release-readiness-source-qpa-authority]=]
        [=[TEST_STALE_QPA_NEGATIVE_METADATA=ON]=])
    string(FIND "${root_cmake}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "deploy-helper-contract: root deployment/readiness wiring drifted: ${required_token}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/cmake/HyRemoteConfig.cmake.in" package_config)
foreach(required_token
        [=[set(_hyremote_package_prefix "${PACKAGE_PREFIX_DIR}")]=]
        [=[HYREMOTE_INSTALLED_PACKAGE_PREFIX]=]
        [=[refusing a second installed prefix]=]
        [=[find_package(HyRemote) cannot be combined with]=]
        [=[if(@HYREMOTE_PACKAGE_WITH_QML@)]=]
        [=[set(HyRemote_QML_IMPORT_PATH "${_hyremote_package_prefix}/@HYREMOTE_PACKAGE_QML_IMPORT_SUBDIR@")]=]
        [=[set(HyRemote_QML_IMPORT_PATH "")]=]
        [=["${_hyremote_package_prefix}/@HYREMOTE_PACKAGE_QPA_PLUGIN_SUBDIR@/@HYREMOTE_PACKAGE_QPA_PLUGIN_FILENAME@"]=])
    string(FIND "${package_config}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "deploy-helper-contract: installed package acquisition/prefix metadata drifted: ${required_token}")
    endif()
endforeach()

string(FIND "${package_config}" [=[set(_hyremote_package_prefix "${PACKAGE_PREFIX_DIR}")]=] prefix_snapshot_pos)
string(FIND "${package_config}" [=[find_dependency(Qt6 6.8 COMPONENTS Core Network)]=] dependency_pos)
if(prefix_snapshot_pos EQUAL -1 OR dependency_pos EQUAL -1 OR prefix_snapshot_pos GREATER dependency_pos)
    message(FATAL_ERROR "deploy-helper-contract: HyRemote package prefix must be preserved before Qt dependency discovery")
endif()
string(FIND "${package_config}" [=[${PACKAGE_PREFIX_DIR}/@HYREMOTE_PACKAGE_]=] unstable_prefix_use)
if(NOT unstable_prefix_use EQUAL -1)
    message(FATAL_ERROR "deploy-helper-contract: optional installed payloads must not use mutable PACKAGE_PREFIX_DIR after dependency discovery")
endif()
string(FIND "${package_config}" "HyRemote_QML_AVAILABLE" leaked_qml_api)
if(NOT leaked_qml_api EQUAL -1)
    message(FATAL_ERROR "deploy-helper-contract: do not expand the frozen installed package surface with QML availability API")
endif()

# The acquisition contract is enforced where it lives: the frontend requires the root build to own the shared runtime
# (so a source tree can never silently reuse an installed one), and the installed package refuses the conflict in the
# project's own words. The previous tokens ("source acquisition conflict", "do not combine") had stopped existing
# anywhere but in this gate, so they guarded nothing.
file(READ "${HYREMOTE_SOURCE_DIR}/src/integrations/cpp/CMakeLists.txt" remoteaccess_cmake)
foreach(required_token
        [=[TARGET hyremote-remoteaccess]=]
        [=[TARGET HyRemote::RemoteAccess]=]
        [=[FATAL_ERROR]=])
    string(FIND "${remoteaccess_cmake}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "deploy-helper-contract: the C++ frontend must require the root build to own the shared runtime: ${required_token}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/cmake/HyRemoteConfig.cmake.in" acquisition_guard)
string(FIND "${acquisition_guard}" [=[package acquisition conflict]=] found)
if(found EQUAL -1)
    message(FATAL_ERROR
        "deploy-helper-contract: the installed package must fail a source/package acquisition conflict closed")
endif()

file(READ "${HYREMOTE_SOURCE_DIR}/cmake/HyRemoteInstall.cmake" install_rules)
foreach(required_token
        [=[set(HYREMOTE_PACKAGE_QPA_PLUGIN_SUBDIR "${CMAKE_INSTALL_LIBDIR}/HyRemote/plugins/platforms")]=]
        [=[function(hyremote_target_artifact_name target out_var)]=]
        [=[get_target_property(_artifact_output ${target} OUTPUT_NAME)]=]
        [=[get_target_property(_artifact_prefix ${target} PREFIX)]=]
        [=[hyremote_target_artifact_name(hyremote-qml HYREMOTE_PACKAGE_QML_BACKING_FILENAME)]=]
        [=[hyremote_target_artifact_name(hyremote-qpa-platform HYREMOTE_PACKAGE_QPA_PLUGIN_FILENAME)]=])
    string(FIND "${install_rules}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "deploy-helper-contract: installed package metadata must describe the target's own artifact: ${required_token}")
    endif()
endforeach()
# A payload name must not be re-derived from the toolchain-global filename variables. They describe CMake's default
# for a target type, and a target may override them - Qt's QML module support sets PREFIX to empty on the QML
# backing library - which is exactly how the declared name and the installed artifact came apart.
string(FIND "${install_rules}" [=["${CMAKE_SHARED_LIBRARY_PREFIX}hyremote-qml${CMAKE_SHARED_LIBRARY_SUFFIX}"]=] guessed_qml_payload)
if(NOT guessed_qml_payload EQUAL -1)
    message(FATAL_ERROR
        "deploy-helper-contract: package payload name must come from the target artifact, not from toolchain-global filename guessing")
endif()

file(READ "${HYREMOTE_SOURCE_DIR}/cmake/HyRemoteDeploy.cmake" deploy_helper)
foreach(required_token
        [=[set(options QML QPA GENERIC)]=]
        [=[_hyremote_target_is_local]=]
        [=[_hyremote_source_acquisition]=]
        [=[_hyremote_qpa_source_acquisition]=]
        [=[TARGET hyremote-qml]=]
        [=[HYREMOTE_BUILD_QML_API=ON]=]
        [=[HyRemote_QML_IMPORT_PATH]=]
        [=[IS_DIRECTORY "${HyRemote_QML_IMPORT_PATH}"]=]
        [=[IS_DIRECTORY "${HyRemote_QML_IMPORT_PATH}/HyRemote"]=]
        [=[HYREMOTE_QML_SOURCE_DEPLOY_TARGETS]=]
        [=[HyRemote_QPA_AVAILABLE]=]
        [=[HyRemote::QpaPlatform]=]
        [=[installed QPA metadata cannot satisfy a source deployment]=]
        [=[if(_hyremote_qpa_source_acquisition)]=]
        [=[set(_required_qt_version "6.8.3")]=]
        [=[if(NOT _hyremote_source_acquisition]=]
        [=[ALIASED_TARGET]=]
        [=[IMPORTED]=]
        [=[add_dependencies]=]
        [=[qt_generate_deploy_qml_app_script]=]
        [=[qt_generate_deploy_app_script]=])
    string(FIND "${deploy_helper}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "deploy-helper-contract: public helper lost required dispatch/acquisition isolation: ${required_token}")
    endif()
endforeach()

string(REGEX MATCHALL "qt6_deploy_runtime_dependencies\\(" qt6_runtime_deploy_calls "${deploy_helper}")
list(LENGTH qt6_runtime_deploy_calls qt6_runtime_deploy_call_count)
if(NOT qt6_runtime_deploy_call_count EQUAL 3)
    message(FATAL_ERROR
        "deploy-helper-contract: expected exactly three direct Qt6 runtime deploy calls (ordinary/QML + QPA + Generic), "
        "found ${qt6_runtime_deploy_call_count}")
endif()

# Generic is a peer frontend deployed through the same helper, and it must stay a plugin payload: it goes to Qt's
# generic plugin directory, it never installs to or references the platform plugin directory, and it never ships the
# native platform delegate. A regression that routed Generic through the QPA path would destroy the native
# QPA identity Generic exists to preserve.
foreach(required_token
        [=[_hyremote_resolve_generic_payload]=]
        [=[HyRemote_GENERIC_AVAILABLE]=]
        [=[HyRemote_GENERIC_PLUGIN_FILE]=]
        [=[TARGET hyremote-generic-plugin]=]
        [=[HYREMOTE_WITH_GENERIC_PLUGIN=ON]=]
        [=[installed Generic metadata cannot satisfy a source deployment]=]
        [=[\${QT_DEPLOY_PLUGINS_DIR}/generic]=]
        [=[GENERIC QPA) is not a supported combination]=])
    string(FIND "${deploy_helper}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "deploy-helper-contract: Generic deployment contract missing: ${required_token}")
    endif()
endforeach()

string(REGEX MATCH "function\\(_hyremote_generate_generic_deploy_script[^)]*\\)(.*)function\\(hyremote_deploy\\)" _generic_body "${deploy_helper}")
if("${CMAKE_MATCH_1}" STREQUAL "")
    message(FATAL_ERROR "deploy-helper-contract: Generic deploy script generator is missing")
endif()
foreach(forbidden_token
        [=[QT_DEPLOY_PLUGINS_DIR}/platforms]=]
        [=[_hyremote_resolve_native_qpa_delegate_payload]=]
        [=[HyRemote_QPA_PLUGIN_FILE]=])
    string(FIND "${CMAKE_MATCH_1}" "${forbidden_token}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "deploy-helper-contract: Generic deployment must not reach the platform-plugin path: ${forbidden_token}")
    endif()
endforeach()
string(FIND "${deploy_helper}"
    [=[${_qml_backing_install}${_linux_private_runtime_bootstrap}qt_deploy_runtime_dependencies(]=]
    versionless_runtime_deploy_call)
if(NOT versionless_runtime_deploy_call EQUAL -1)
    message(FATAL_ERROR
        "deploy-helper-contract: versionless Qt runtime deploy wrapper returned; paths with spaces would split on Qt 6.8.3")
endif()

string(FIND "${deploy_helper}" "HyRemote_QML_AVAILABLE" leaked_helper_api)
if(NOT leaked_helper_api EQUAL -1)
    message(FATAL_ERROR "deploy-helper-contract: QML source availability must remain internal target/build metadata")
endif()

foreach(required_phrase
        [=[requires a HyRemote QML payload]=]
        [=[QML import root does not exist]=]
        [=[QML module directory does not exist]=]
        [=[requires the current HyRemote source build]=]
        [=[requested an SDK that was built without Transparent QPA]=]
        [=[requires exact Qt]=])
    string(FIND "${deploy_helper}" "${required_phrase}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "deploy-helper-contract: optional payload no longer fails closed: ${required_phrase}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/src/integrations/qpa/tests/deploy_helper_fixture/CMakeLists.txt" fixture)
foreach(required_token
        [=[TEST_DEPLOY_QPA]=]
        [=[TEST_QML_AVAILABLE]=]
        [=[TEST_QML_IMPORT_PATH_EXISTS]=]
        [=[TEST_QML_MODULE_DIR_EXISTS]=]
        [=[TEST_STALE_QML_METADATA]=]
        [=[TEST_QPA_AVAILABLE]=]
        [=[TEST_STALE_QPA_METADATA]=]
        [=[TEST_STALE_QPA_NEGATIVE_METADATA]=]
        [=[${CMAKE_SHARED_MODULE_PREFIX}stale-qhyremote${CMAKE_SHARED_MODULE_SUFFIX}]=]
        [=[${CMAKE_SHARED_MODULE_PREFIX}qhyremote${CMAKE_SHARED_MODULE_SUFFIX}]=]
        [=[set(HyRemote_QPA_AVAILABLE FALSE)]=]
        [=[set(HyRemote_QPA_QT_VERSION "6.8.2")]=]
        [=[add_library(hyremote-qml ALIAS qml-backing)]=]
        [=[HYREMOTE_QML_SOURCE_DEPLOY_TARGETS]=]
        [=[QT_QML_IMPORT_PATH]=]
        [=[MANUALLY_ADDED_DEPENDENCIES]=]
        [=[installed payloads must not become local build dependencies]=])
    string(FIND "${fixture}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "deploy-helper-contract: deterministic fixture lost required source/installed proof: ${required_token}")
    endif()
endforeach()
string(FIND "${fixture}" [=[/qhyremote${CMAKE_SHARED_MODULE_SUFFIX}]=] fixture_missing_module_prefix)
if(NOT fixture_missing_module_prefix EQUAL -1)
    message(FATAL_ERROR "deploy-helper-contract: installed QPA fixture must model the platform MODULE prefix")
endif()
string(FIND "${fixture}" "HyRemote_QML_AVAILABLE" leaked_fixture_api)
if(NOT leaked_fixture_api EQUAL -1)
    message(FATAL_ERROR "deploy-helper-contract: deterministic fixture must not rely on a new QML package API")
endif()

file(READ "${HYREMOTE_SOURCE_DIR}/src/integrations/qpa/tests/run_deploy_helper_fixture.cmake" runner)
foreach(required_token
        [=[TEST_STALE_QML_METADATA]=]
        [=[TEST_QML_IMPORT_PATH_EXISTS]=]
        [=[TEST_QML_MODULE_DIR_EXISTS]=]
        [=[TEST_STALE_QPA_METADATA]=]
        [=[TEST_STALE_QPA_NEGATIVE_METADATA]=]
        [=[EXPECT_FAILURE_FRAGMENT]=]
        [=[string(REPLACE "_" " " _expected_fragment]=]
        [=[string(REGEX REPLACE "[ \t\r\n]+" " " _configure_output]=]
        [=[failed for the wrong reason]=]
        [=[_expected_qpa_name]=]
        [=[${CMAKE_SHARED_MODULE_PREFIX}qhyremote${CMAKE_SHARED_MODULE_SUFFIX}]=]
        [=[hyremote-runtime-deploy-deploy-probe]=]
        [=[hyremote-qpa-deploy-deploy-probe]=]
        [=[ADDITIONAL_MODULES]=]
        [=[RPATH_CHANGE]=]
        [=[HyRemoteRemoteAccess]=]
        [=[fake-remoteaccess]=])
    string(FIND "${runner}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "deploy-helper-contract: generated-script verifier lost a deployment distinction: ${required_token}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/src/integrations/qpa/CMakeLists.txt" qpa_cmake)
foreach(required_token
        [=[LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/plugins/platforms"]=]
        [=[RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/plugins/platforms"]=]
        [=[TEST_QML_AVAILABLE=OFF]=]
        [=[TEST_STALE_QML_METADATA=ON]=]
        [=[TEST_QML_IMPORT_PATH_EXISTS=OFF]=]
        [=[TEST_QML_MODULE_DIR_EXISTS=OFF]=]
        [=[TEST_STALE_QPA_METADATA=ON]=]
        [=[TEST_QT_VERSION=6.8.2]=]
        [=[TEST_QPA_AVAILABLE=OFF]=]
        [=[EXPECT_FAILURE_FRAGMENT=requires_a_HyRemote_QML_payload]=]
        [=[EXPECT_FAILURE_FRAGMENT=QML_import_root_does_not_exist]=]
        [=[EXPECT_FAILURE_FRAGMENT=QML_module_directory_does_not_exist]=]
        [=[EXPECT_FAILURE_FRAGMENT=requires_the_current_HyRemote_source_build]=]
        [=[EXPECT_FAILURE_FRAGMENT=requires_exact_Qt_6.8.3]=]
        [=[EXPECT_FAILURE_FRAGMENT=requested_an_SDK_that_was_built_without_Transparent_QPA]=])
    string(FIND "${qpa_cmake}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "deploy-helper-contract: deterministic deploy test lost canonical semantic rejection wiring: ${required_token}")
    endif()
endforeach()
foreach(forbidden_legacy_token
        [=[TEST_MISSING_QML_PAYLOAD]=]
        [=[TEST_MISSING_QML_ROOT]=]
        [=[TEST_MISSING_QML_MODULE_DIR]=]
        [=[TEST_QT_MISMATCH]=]
        [=[TEST_MISSING_PACKAGE]=])
    string(FIND "${qpa_cmake}" "${forbidden_legacy_token}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "deploy-helper-contract: legacy negative-fixture wiring returned: ${forbidden_legacy_token}")
    endif()
endforeach()
string(FIND "${qpa_cmake}" [=[CMAKE_BINARY_DIR}/plugins/platforms]=] leaked_host_output)
if(NOT leaked_host_output EQUAL -1)
    message(FATAL_ERROR "deploy-helper-contract: qhyremote must not write into a source consumer's top-level plugin directory")
endif()

foreach(required_test
        "hyremote-qpa-deploy-helper-ordinary"
        "hyremote-qpa-deploy-helper-qml-only"
        "hyremote-qpa-deploy-helper-qml-composed"
        "hyremote-qpa-deploy-helper-installed-payload"
        "hyremote-qpa-deploy-helper-installed-payload-qml-only"
        "hyremote-qpa-deploy-helper-installed-payload-qml"
        "hyremote-qpa-deploy-helper-reject-missing-qml"
        "hyremote-qpa-deploy-helper-reject-stale-qml-metadata"
        "hyremote-qpa-deploy-helper-reject-missing-qml-root"
        "hyremote-qpa-deploy-helper-reject-missing-qml-module-dir"
        "hyremote-qpa-deploy-helper-reject-stale-qpa-metadata"
        "hyremote-qpa-deploy-helper-reject-missing-package"
        "hyremote-qpa-deploy-helper-reject-qt-mismatch"
        "hyremote-qpa-source-payload-relocation")
    string(FIND "${qpa_cmake}" "${required_test}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "deploy-helper-contract: deterministic deploy test missing: ${required_test}")
    endif()
endforeach()

# Forward the toolchain of the build under test: the probe configures the project source tree, and without these the
# nested configure fell back to whatever the host prefers (MSVC NMake here) and failed before reaching the contract.
set(package_probe_toolchain "")
foreach(package_probe_toolchain_key IN ITEMS GENERATOR MAKE_PROGRAM C_COMPILER CXX_COMPILER)
    if(DEFINED HYREMOTE_GATE_${package_probe_toolchain_key}
       AND NOT HYREMOTE_GATE_${package_probe_toolchain_key} STREQUAL "")
        list(APPEND package_probe_toolchain
            "-DHYREMOTE_GATE_${package_probe_toolchain_key}=${HYREMOTE_GATE_${package_probe_toolchain_key}}")
    endif()
endforeach()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -DHYREMOTE_SOURCE_DIR=${HYREMOTE_SOURCE_DIR}
        ${package_probe_toolchain}
        -P "${HYREMOTE_SOURCE_DIR}/tests/release-readiness/check_package_acquisition_isolation.cmake"
    RESULT_VARIABLE package_probe_result
    OUTPUT_VARIABLE package_probe_stdout
    ERROR_VARIABLE package_probe_stderr
)
if(NOT package_probe_result EQUAL 0)
    message(FATAL_ERROR
        "deploy-helper-contract: package acquisition behavior probe failed:\n"
        "${package_probe_stdout}\n${package_probe_stderr}")
endif()

message(STATUS
    "HyRemote deploy-helper contract gate: PASS "
    "(one acquisition/prefix per configure; CMake-3.21-safe package prefix preservation behavior; Qt6 deploy argument boundaries preserved; established V1 deploy shapes stay distinct while source ownership is grouped)")
