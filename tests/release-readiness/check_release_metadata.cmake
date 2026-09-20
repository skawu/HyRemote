cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

set(required_files
    "LICENSE"
    "NOTICE.md"
    "README.md"
    "docs/versioning.md"
    "docs/internal/git-flow-release.md"
    "docs/internal/repository-layout.md"
    "docs/internal/release-candidate-checklist.md"
    "docs/release-package-manifest.md"
    "docs/releases/v0.0.1.0.md"
    "docs/releases/v0.0.2.0.md"
    "docs/releases/v0.0.3.0.md"
    "docs/releases/v1.0.0.0.md"
    "docs/guide/install.md"
    "docs/en/guide/install.md"
    "docs/getting-started/cpp.md"
    "docs/getting-started/qml.md"
    "docs/getting-started/qpa-proxy.md"
    "docs/guide/deployment.md"
    "docs/qml-consumption.md"
    "docs/input-model.md"
    "docs/security.md"
    "docs/security-model.md"
    "docs/guide/viewer-connection.md"
    "docs/guide/troubleshooting.md"
    "docs/compatibility.md"
    "docs/known-limitations.md"
    "docs/v1-api-stability.md"
    "docs/internal/v1-ga-acceptance.md"
    "examples/README.md"
    "examples/CMakeLists.txt"
    "examples/common/HyRemoteExampleBranding.cmake"
    "examples/common/hyremote-branding.qrc"
    "examples/common/hyremote-example-branding.cpp"
    "examples/learning/01-widgets-basic/CMakeLists.txt"
    "examples/learning/01-widgets-basic/main.cpp"
    "examples/learning/01-widgets-basic/README.md"
    "examples/learning/02-widgets-control/CMakeLists.txt"
    "examples/learning/02-widgets-control/main.cpp"
    "examples/learning/02-widgets-control/README.md"
    "examples/learning/03-quick-cpp/CMakeLists.txt"
    "examples/learning/03-quick-cpp/main.cpp"
    "examples/learning/03-quick-cpp/Main.qml"
    "examples/learning/03-quick-cpp/README.md"
    "examples/learning/04-quick-qml/CMakeLists.txt"
    "examples/learning/04-quick-qml/main.cpp"
    "examples/learning/04-quick-qml/Main.qml"
    "examples/learning/04-quick-qml/README.md"
    "examples/learning/05-qpa-existing-app/CMakeLists.txt"
    "examples/learning/05-qpa-existing-app/widgets-app/CMakeLists.txt"
    "examples/learning/05-qpa-existing-app/widgets-app/main.cpp"
    "examples/learning/05-qpa-existing-app/widgets-app/README.md"
    "examples/learning/05-qpa-existing-app/quick-app/CMakeLists.txt"
    "examples/learning/05-qpa-existing-app/quick-app/main.cpp"
    "examples/learning/05-qpa-existing-app/quick-app/Main.qml"
    "examples/learning/05-qpa-existing-app/quick-app/README.md"
    "examples/learning/07-production-showcase/CMakeLists.txt"
    "examples/learning/07-production-showcase/main.cpp"
    "examples/learning/07-production-showcase/Main.qml"
    "examples/learning/07-production-showcase/README.md"
    "examples/realworld/README.md"
    "examples/realworld/qbittorrent/README.md"
    "examples/realworld/qbittorrent/manifest.json"
    "examples/realworld/musescore/README.md"
    "examples/realworld/musescore/manifest.json"
    "logo/huayan-logo-single.png"
    "tests/consumer-installed-sdk/CMakeLists.txt"
    "tests/consumer-source/CMakeLists.txt"
    "tests/consumer-installed-qml/CMakeLists.txt"
    "tests/consumer-installed-qpa/CMakeLists.txt"
    "tests/consumer-installed-qpa/product_fit.py"
    "tests/public-api-contract/CMakeLists.txt"
    "src/cpp/tests/test_remote_access.cpp"
    "src/cpp/tests/test_widgets_input_backpressure.cpp"
    "src/cpp/tests/test_quick_input_backpressure.cpp"
    "src/cpp/tests/test_rfb_widget_disconnect_backpressure.cpp"
    "src/qpa/tests/qpa_composite_input_test.cpp"
)

foreach(path IN LISTS required_files)
    if(NOT EXISTS "${HYREMOTE_SOURCE_DIR}/${path}")
        message(FATAL_ERROR "release-readiness: missing required V1 product artifact: ${path}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/CMakeLists.txt" root_cmake)

# This gate validates metadata semantics, not source formatting: a multi-line project() declaration is normal
# CMake style and must be accepted. Whitespace is normalized before matching, and CMake's regex dialect does
# not provide portable interval quantifiers such as {2,3}, so the accepted three/four-part shape is spelled
# out explicitly.
function(hyremote_match_project_version content out_version)
    string(REGEX REPLACE "[ \t\r\n]+" " " normalized "${content}")
    set(matched_version "")
    if(normalized MATCHES "project *\\( *HyRemote +VERSION +([0-9]+\\.[0-9]+\\.[0-9]+(\\.[0-9]+)?)")
        set(matched_version "${CMAKE_MATCH_1}")
    endif()
    set(${out_version} "${matched_version}" PARENT_SCOPE)
endfunction()

# Deterministic self-check: both declaration layouts must be accepted and a malformed or absent version must
# still fail, so this property cannot be lost silently in a later edit of the matcher.
hyremote_match_project_version(
    "project(HyRemote VERSION 1.0.0.0 DESCRIPTION \"Qt Remote Access Framework\" LANGUAGES C CXX)"
    self_check_one_line)
hyremote_match_project_version(
    "project(\n    HyRemote\n    VERSION 0.0.0\n    DESCRIPTION \"Qt Remote Access Framework\"\n    LANGUAGES C CXX\n)"
    self_check_multi_line)
hyremote_match_project_version("project(HyRemote VERSION 0.0 LANGUAGES C CXX)" self_check_malformed)
hyremote_match_project_version("add_subdirectory(src/core core)" self_check_missing)
if(NOT self_check_one_line STREQUAL "1.0.0.0"
        OR NOT self_check_multi_line STREQUAL "0.0.0"
        OR NOT self_check_malformed STREQUAL ""
        OR NOT self_check_missing STREQUAL "")
    message(FATAL_ERROR
        "release-readiness: project-version matcher self-check failed "
        "(one-line='${self_check_one_line}', multi-line='${self_check_multi_line}', "
        "malformed='${self_check_malformed}', missing='${self_check_missing}')")
endif()

hyremote_match_project_version("${root_cmake}" source_project_version)
if(NOT source_project_version)
    message(FATAL_ERROR "release-readiness: root project(VERSION ...) is missing or malformed")
endif()

if(DEFINED HYREMOTE_PROJECT_VERSION
        AND NOT "${HYREMOTE_PROJECT_VERSION}" STREQUAL "${source_project_version}")
    message(FATAL_ERROR
        "release-readiness: configured project version ${HYREMOTE_PROJECT_VERSION} does not match "
        "source project version ${source_project_version}")
endif()

foreach(milestone_version
        "0.0.1.0"
        "0.0.2.0"
        "0.0.3.0"
        "1.0.0.0")
    set(release_note_path "${HYREMOTE_SOURCE_DIR}/docs/releases/v${milestone_version}.md")
    file(READ "${release_note_path}" milestone_notes)
    # The security-boundary statement must describe what that milestone actually contains. The three pre-release
    # milestones shipped the unauthenticated correctness transport and their notes are the record of what they did,
    # so they keep saying so. v1.0.0.0 is the milestone that carries the RFB VNC authentication capability, so its
    # note has to state that authentication and must not claim encryption - TLS is a separate later step (#143).
    # Pinning a phrase that no longer describes the release would be exactly the "transient implementation
    # limitation hard-coded as a permanent release invariant" that docs/release-candidate-checklist.md forbids.
    if(milestone_version VERSION_LESS "1.0.0.0")
        set(required_security_phrases "SecurityType None")
    else()
        set(required_security_phrases "VNC authentication" "not encrypted")
    endif()
    foreach(required_phrase
            "v${milestone_version}"
            ${required_security_phrases}
            "release/v${milestone_version}")
        string(FIND "${milestone_notes}" "${required_phrase}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "release-readiness: v${milestone_version} notes missing stable release fact: ${required_phrase}")
        endif()
    endforeach()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/NOTICE.md" notice_text)
foreach(required_phrase
        "Qt"
        "Apache License 2.0"
        "aqtinstall"
        "Ninja"
        "GitHub Actions"
        "vncdotool"
        "Pillow")
    string(FIND "${notice_text}" "${required_phrase}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "release-readiness: NOTICE.md missing classification: ${required_phrase}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/cmake/HyRemoteInstall.cmake" install_rules)
foreach(required_token "LICENSE" "NOTICE.md" "HyRemote/licenses")
    string(FIND "${install_rules}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: install rules do not preserve required release metadata token: ${required_token}")
    endif()
endforeach()
foreach(forbidden_token
        "HYREMOTE_PACKAGE_WITH_WIDGETS"
        "HYREMOTE_PACKAGE_WITH_QUICK")
    string(FIND "${install_rules}" "${forbidden_token}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: private UI adapter leaked into installed package dependency model: ${forbidden_token}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/src/core/CMakeLists.txt" core_cmake)
string(FIND "${core_cmake}" "add_library(hyremote-core STATIC" core_static)
if(core_static EQUAL -1)
    message(FATAL_ERROR "release-readiness: V1 Core must remain an internal STATIC composition target")
endif()
string(FIND "${core_cmake}" "install(" core_install)
if(NOT core_install EQUAL -1)
    message(FATAL_ERROR "release-readiness: V1 Core must not be installed/exported as a second product target")
endif()

file(READ "${HYREMOTE_SOURCE_DIR}/src/cpp/CMakeLists.txt" remoteaccess_cmake)
foreach(required_token
        "add_library(hyremote-remoteaccess SHARED"
        "OUTPUT_NAME HyRemoteRemoteAccess"
        "EXPORT_NAME RemoteAccess")
    string(FIND "${remoteaccess_cmake}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "release-readiness: RemoteAccess shared facade contract missing: ${required_token}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/src/qml/CMakeLists.txt" qml_cmake)
string(FIND "${qml_cmake}" "TARGETS hyremote-qml\n    EXPORT HyRemoteTargets" qml_export)
if(NOT qml_export EQUAL -1)
    message(FATAL_ERROR
        "release-readiness: declarative QML backing library must not become a second C++ SDK target")
endif()

file(READ "${HYREMOTE_SOURCE_DIR}/src/qpa/CMakeLists.txt" qpa_cmake)
string(FIND "${qpa_cmake}" "add_library(hyremote-qpa-platform MODULE" qpa_module)
if(qpa_module EQUAL -1)
    message(FATAL_ERROR "release-readiness: Transparent QPA must remain a platform MODULE")
endif()
string(FIND "${qpa_cmake}" "EXPORT HyRemoteTargets" qpa_export)
if(NOT qpa_export EQUAL -1)
    message(FATAL_ERROR
        "release-readiness: qhyremote must install as payload, not export HyRemote::QpaPlatform")
endif()

file(READ "${HYREMOTE_SOURCE_DIR}/cmake/HyRemoteConfig.cmake.in" package_config)
foreach(forbidden_token
        "HyRemote::Core"
        "HyRemote::QpaPlatform"
        "HyRemote_QPA_SHARED_RUNTIME"
        "find_dependency(Threads)"
        "COMPONENTS Widgets"
        "COMPONENTS Quick"
        "COMPONENTS Qml")
    string(FIND "${package_config}" "${forbidden_token}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: installed package leaked internal/unused consumer dependency: ${forbidden_token}")
    endif()
endforeach()
foreach(required_token
        "find_dependency(Qt6 6.8 COMPONENTS Core Network)"
        "HyRemote_QML_IMPORT_PATH"
        "HyRemote_QPA_QT_VERSION"
        "HyRemote_QPA_PLUGIN_FILE")
    string(FIND "${package_config}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: installed package missing required minimal product metadata: ${required_token}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/docs/release-package-manifest.md" package_manifest)
foreach(required_phrase
        "HyRemote::RemoteAccess"
        "not installed/exported"
        "does not export `HyRemote::QpaPlatform`"
        "qhyremote"
        "hyremote_deploy(TARGET MyApp)")
    string(FIND "${package_manifest}" "${required_phrase}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: package manifest missing simple V1 product contract: ${required_phrase}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/README.md" readme_text)
foreach(required_link
        "docs/getting-started/cpp.md"
        "docs/getting-started/qml.md"
        "docs/getting-started/qpa-proxy.md"
        "docs/guide/install.md"
        "docs/guide/deployment.md"
        "docs/internal/repository-layout.md"
        "docs/guide/viewer-connection.md"
        "docs/security.md"
        "docs/guide/troubleshooting.md"
        "docs/compatibility.md"
        "docs/known-limitations.md")
    string(FIND "${readme_text}" "${required_link}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: README does not expose required V1 user guide: ${required_link}")
    endif()
endforeach()

# The V1 examples graph itself is a release contract: one progressive source taxonomy with no legacy
# binary-directory aliases. 06-session-security is added with #170 once its real public API exists.
file(READ "${HYREMOTE_SOURCE_DIR}/examples/CMakeLists.txt" examples_cmake)
foreach(required_example
        "add_subdirectory(learning/01-widgets-basic)"
        "add_subdirectory(learning/02-widgets-control)"
        "add_subdirectory(learning/03-quick-cpp)"
        "add_subdirectory(learning/04-quick-qml)"
        "add_subdirectory(learning/05-qpa-existing-app)"
        "add_subdirectory(learning/07-production-showcase)")
    string(FIND "${examples_cmake}" "${required_example}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: progressive V1 examples graph missing required example: ${required_example}")
    endif()
endforeach()
foreach(forbidden_legacy_example
        "add_subdirectory(widgets-basic)"
        "add_subdirectory(quick-basic)"
        "add_subdirectory(qml-basic)"
        "add_subdirectory(qpa-proxy-existing-app)"
        "add_subdirectory(remote-support-showcase)")
    string(FIND "${examples_cmake}" "${forbidden_legacy_example}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: legacy example taxonomy returned to the build graph: ${forbidden_legacy_example}")
    endif()
endforeach()

# Both QPA teaching fixtures are ordinary Qt applications. HyRemote is package/deployment metadata only;
# neither application may link a HyRemote application/runtime target.
file(READ "${HYREMOTE_SOURCE_DIR}/examples/learning/05-qpa-existing-app/widgets-app/CMakeLists.txt" qpa_widgets_cmake)
foreach(required_token
        "target_link_libraries(hyremote-example-qpa-existing-widgets PRIVATE Qt6::Widgets)"
        "HYREMOTE_EXAMPLE_DEPLOY_QPA"
        "find_package(HyRemote CONFIG REQUIRED)"
        "hyremote_deploy(TARGET hyremote-example-qpa-existing-widgets QPA)")
    string(FIND "${qpa_widgets_cmake}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: 05 Widgets QPA ordinary-Qt/deployment contract missing: ${required_token}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/examples/learning/05-qpa-existing-app/quick-app/CMakeLists.txt" qpa_quick_cmake)
foreach(required_token
        "target_link_libraries(hyremote-example-qpa-existing-quick PRIVATE Qt6::Quick Qt6::Qml)"
        "HYREMOTE_EXAMPLE_DEPLOY_QPA"
        "find_package(HyRemote CONFIG REQUIRED)"
        "hyremote_deploy(TARGET hyremote-example-qpa-existing-quick QPA)")
    string(FIND "${qpa_quick_cmake}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: 05 Quick QPA ordinary-Qt/deployment contract missing: ${required_token}")
    endif()
endforeach()
foreach(qpa_fixture IN ITEMS qpa_widgets_cmake qpa_quick_cmake)
    foreach(forbidden_token
            "HyRemote::RemoteAccess"
            "HyRemote::QpaPlatform")
        string(FIND "${${qpa_fixture}}" "${forbidden_token}" found)
        if(NOT found EQUAL -1)
            message(FATAL_ERROR
                "release-readiness: 05 QPA application project leaked a HyRemote link target: ${forbidden_token}")
        endif()
    endforeach()
endforeach()

# Real-world verification is deliberately bounded to one Widgets and one Quick/QML representative.
# The manifests are repository-owned pins; upstream application source/branding stays external and pristine.
foreach(realworld_manifest
        "examples/realworld/qbittorrent/manifest.json"
        "examples/realworld/musescore/manifest.json")
    file(READ "${HYREMOTE_SOURCE_DIR}/${realworld_manifest}" manifest_text)
    foreach(required_token
            "\"source_policy\": \"pristine-external\""
            "\"support_claim\": false")
        string(FIND "${manifest_text}" "${required_token}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "release-readiness: real-world example manifest lost pristine/non-support boundary: ${realworld_manifest}")
        endif()
    endforeach()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/tests/consumer-installed-qpa/CMakeLists.txt" clean_qpa_cmake)
foreach(required_token
        "examples/learning/05-qpa-existing-app/widgets-app/main.cpp"
        "target_link_libraries(hyremote-installed-qpa-consumer PRIVATE Qt6::Widgets)"
        "hyremote_deploy(TARGET hyremote-installed-qpa-consumer QPA)")
    string(FIND "${clean_qpa_cmake}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: clean installed-QPA evidence is not bound to the real 05 Widgets example: ${required_token}")
    endif()
endforeach()
if(EXISTS "${HYREMOTE_SOURCE_DIR}/tests/consumer-installed-qpa/main.cpp")
    message(FATAL_ERROR
        "release-readiness: clean installed-QPA fixture must not maintain a duplicate 05 application source")
endif()

file(READ "${HYREMOTE_SOURCE_DIR}/src/core/include/hyremote/core/input.hpp" input_contract)
string(FIND "${input_contract}" "virtual void shutdown() noexcept" input_shutdown_contract)
if(input_shutdown_contract EQUAL -1)
    message(FATAL_ERROR
        "release-readiness: internal InputSink terminal shutdown contract was removed")
endif()

file(READ "${HYREMOTE_SOURCE_DIR}/src/cpp/src/remote_access.cpp" remoteaccess_source)
string(FIND "${remoteaccess_source}" "session->stop();" session_stop_pos)
string(FIND "${remoteaccess_source}" "inputSink->shutdown();" input_shutdown_pos)
if(session_stop_pos EQUAL -1 OR input_shutdown_pos EQUAL -1 OR input_shutdown_pos LESS session_stop_pos)
    message(FATAL_ERROR
        "release-readiness: RemoteAccess must quiesce Session before terminal target-input shutdown")
endif()

foreach(adapter_file
        "src/cpp/src/widgets/widget_target.cpp"
        "src/cpp/src/quick/quick_target.cpp")
    file(READ "${HYREMOTE_SOURCE_DIR}/${adapter_file}" adapter_source)
    foreach(required_token
            "void shutdown() noexcept override"
            "override { shutdown(); }"
            "state->pending.clear()"
            "releaseHeldStateOnGuiThread")
        string(FIND "${adapter_source}" "${required_token}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "release-readiness: terminal input cleanup missing from ${adapter_file}: ${required_token}")
        endif()
    endforeach()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/src/qpa/interactive_composite_target.cpp" qpa_input_source)
foreach(required_token
        "void shutdown() noexcept override"
        "sink->shutdown()"
        "state->pending.clear()")
    string(FIND "${qpa_input_source}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: QPA terminal child-input propagation missing: ${required_token}")
    endif()
endforeach()

foreach(test_entry
        "src/cpp/tests/test_remote_access.cpp|inputShutdowns"
        "src/cpp/tests/test_widgets_input_backpressure.cpp|testShutdownBalancesDeliveredStateAndDropsPendingInput"
        "src/cpp/tests/test_quick_input_backpressure.cpp|testShutdownBalancesDeliveredStateAndDropsPendingInput"
        "src/cpp/tests/test_rfb_widget_disconnect_backpressure.cpp|testDisconnectCleanupCrossesSaturatedAdapterMailbox"
        "src/qpa/tests/qpa_composite_input_test.cpp|shutdownCalls")
    string(REPLACE "|" ";" test_parts "${test_entry}")
    list(GET test_parts 0 test_path)
    list(GET test_parts 1 required_token)
    file(READ "${HYREMOTE_SOURCE_DIR}/${test_path}" test_source)
    string(FIND "${test_source}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: terminal input lifecycle regression evidence missing: ${test_path}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/docs/internal/v1-ga-acceptance.md" ga_acceptance)
foreach(required_phrase
        "terminal remote-input lifecycle boundary"
        "discard remote input accepted into its pending mailbox"
        "explicit HyRemote stop/policy transition")
    string(FIND "${ga_acceptance}" "${required_phrase}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: canonical V1 GA acceptance omits terminal input lifecycle fact: ${required_phrase}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/docs/input-model.md" input_model)
foreach(required_phrase
        "Viewer disconnect cleanup belongs to the transport"
        "HyRemote runtime/target teardown cleanup belongs to the target `InputSink`"
        "pending"
        "delivered")
    string(FIND "${input_model}" "${required_phrase}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: input model omits terminal lifecycle boundary: ${required_phrase}")
    endif()
endforeach()

foreach(doc_check
        "docs/internal/release-candidate-checklist.md|explicit HyRemote runtime stop/policy transition"
        "docs/releases/v1.0.0.0.md|explicit HyRemote runtime stop/policy transition"
        "docs/compatibility.md|explicit HyRemote stop/policy transition"
        "docs/known-limitations.md|explicit HyRemote runtime stop")
    string(REPLACE "|" ";" doc_parts "${doc_check}")
    list(GET doc_parts 0 doc_path)
    list(GET doc_parts 1 required_phrase)
    file(READ "${HYREMOTE_SOURCE_DIR}/${doc_path}" doc_text)
    string(FIND "${doc_text}" "${required_phrase}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: V1 lifecycle documentation drift in ${doc_path}: ${required_phrase}")
    endif()
endforeach()

message(STATUS
    "HyRemote release-readiness metadata gate: PASS "
    "(project ${source_project_version}, canonical repository layout + milestone notes + minimal SDK surface + progressive V1 examples + bounded real-world verification)")