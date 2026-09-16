cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

set(required_files
    "cmake/HyRemoteDeploy.cmake"
    "qpa/CMakeLists.txt"
    "qpa/tests/deploy_helper_fixture/CMakeLists.txt"
    "qpa/tests/run_deploy_helper_fixture.cmake")
foreach(path IN LISTS required_files)
    if(NOT EXISTS "${HYREMOTE_SOURCE_DIR}/${path}")
        message(FATAL_ERROR "deploy-helper-contract: missing evidence file: ${path}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/cmake/HyRemoteDeploy.cmake" deploy_helper)
foreach(required_token
        [=[set(options QML QPA)]=]
        [=[HYREMOTE_BUILD_QML_API=ON]=]
        [=[HyRemote_QML_IMPORT_PATH]=]
        [=[HYREMOTE_QML_SOURCE_DEPLOY_TARGETS]=]
        [=[HyRemote_QPA_AVAILABLE]=]
        [=[HyRemote::QpaPlatform]=]
        [=[_hyremote_add_local_build_dependency]=]
        [=[ALIASED_TARGET]=]
        [=[IMPORTED]=]
        [=[add_dependencies]=]
        [=[qt_generate_deploy_qml_app_script]=]
        [=[qt_generate_deploy_app_script]=])
    string(FIND "${deploy_helper}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "deploy-helper-contract: public helper lost required dispatch/fail-closed behavior: ${required_token}")
    endif()
endforeach()

# QML and QPA are optional payloads, but asking for an unavailable selected mode must fail during
# configuration rather than produce an apparently successful incomplete deployment.
foreach(required_phrase
        [=[requires a HyRemote QML payload]=]
        [=[requested an SDK that was built without Transparent QPA]=]
        [=[requires exact Qt]=])
    string(FIND "${deploy_helper}" "${required_phrase}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "deploy-helper-contract: optional payload no longer fails closed: ${required_phrase}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/qpa/tests/deploy_helper_fixture/CMakeLists.txt" fixture)
foreach(required_token
        [=[TEST_DEPLOY_QPA]=]
        [=[TEST_QML_AVAILABLE]=]
        [=[HYREMOTE_QML_SOURCE_DEPLOY_TARGETS]=]
        [=[QT_QML_IMPORT_PATH]=]
        [=[MANUALLY_ADDED_DEPENDENCIES]=]
        [=[installed payloads must not become local build dependencies]=])
    string(FIND "${fixture}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "deploy-helper-contract: deterministic fixture lost required source/installed proof: ${required_token}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/qpa/tests/run_deploy_helper_fixture.cmake" runner)
foreach(required_token
        [=[hyremote-runtime-deploy-deploy-probe]=]
        [=[hyremote-qpa-deploy-deploy-probe]=]
        [=[ADDITIONAL_MODULES]=]
        [=[RPATH_CHANGE]=]
        [=[HyRemoteRemoteAccess]=]
        [=[fake-remoteaccess]=])
    string(FIND "${runner}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "deploy-helper-contract: generated-script verifier lost a deployment distinction: ${required_token}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/qpa/CMakeLists.txt" qpa_cmake)
foreach(required_token
        [=[LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/plugins/platforms"]=]
        [=[RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/plugins/platforms"]=])
    string(FIND "${qpa_cmake}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "deploy-helper-contract: source qhyremote no longer stays inside the HyRemote sub-build: ${required_token}")
    endif()
endforeach()
string(FIND "${qpa_cmake}" [=[CMAKE_BINARY_DIR}/plugins/platforms]=] leaked_host_output)
if(NOT leaked_host_output EQUAL -1)
    message(FATAL_ERROR
        "deploy-helper-contract: qhyremote must not write into a source consumer's top-level plugin directory")
endif()

foreach(required_test
        "hyremote-qpa-deploy-helper-ordinary"
        "hyremote-qpa-deploy-helper-qml-only"
        "hyremote-qpa-deploy-helper-qml-composed"
        "hyremote-qpa-deploy-helper-installed-payload"
        "hyremote-qpa-deploy-helper-installed-payload-qml-only"
        "hyremote-qpa-deploy-helper-installed-payload-qml"
        "hyremote-qpa-deploy-helper-reject-missing-qml"
        "hyremote-qpa-deploy-helper-reject-missing-package"
        "hyremote-qpa-deploy-helper-reject-qt-mismatch"
        "hyremote-qpa-source-payload-relocation")
    string(FIND "${qpa_cmake}" "${required_test}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "deploy-helper-contract: deterministic deploy test missing: ${required_test}")
    endif()
endforeach()

message(STATUS
    "HyRemote deploy-helper contract gate: PASS "
    "(four public shapes remain distinct; optional QML/QPA fail closed; source build-only payload wiring and QPA sub-build isolation frozen)")
