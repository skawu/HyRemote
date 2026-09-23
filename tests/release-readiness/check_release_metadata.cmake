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
    "examples/learning/01-widgets-cpp/CMakeLists.txt"
    "examples/learning/01-widgets-cpp/README.md"
    "examples/learning/02-quick-cpp/CMakeLists.txt"
    "examples/learning/02-quick-cpp/README.md"
    "examples/learning/03-zero-code-generic/README.md"
    "examples/learning/03-zero-code-generic/widgets-app/CMakeLists.txt"
    "examples/learning/03-zero-code-generic/quick-app/CMakeLists.txt"
    "examples/qml-basic/CMakeLists.txt"
    "examples/qml-basic/README.md"
    "examples/qpa-proxy-existing-app/CMakeLists.txt"
    "examples/qpa-proxy-existing-app/main.cpp"
    "examples/qpa-proxy-existing-app/README.md"
    "examples/remote-support-showcase/CMakeLists.txt"
    "examples/remote-support-showcase/README.md"
    "tests/consumer-installed-sdk/CMakeLists.txt"
    "tests/consumer-source/CMakeLists.txt"
    "tests/consumer-installed-qml/CMakeLists.txt"
    "tests/consumer-installed-qpa/CMakeLists.txt"
    "tests/consumer-installed-qpa/product_fit.py"
    "tests/public-api-contract/CMakeLists.txt"
    "src/integrations/cpp/tests/test_remote_access.cpp"
    "src/integrations/cpp/tests/test_widgets_input_backpressure.cpp"
    "src/integrations/cpp/tests/test_quick_input_backpressure.cpp"
    "src/integrations/cpp/tests/test_rfb_widget_disconnect_backpressure.cpp"
    "src/runtime/tests/automatic_composite_input_test.cpp"
    "src/runtime/tests/automatic_composite_capture_test.cpp"
    "src/runtime/tests/automatic_application_surface_model_test.cpp"
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
        "0.2.0.0"
        "1.0.0.0")
    set(release_note_path "${HYREMOTE_SOURCE_DIR}/docs/releases/v${milestone_version}.md")
    file(READ "${release_note_path}" milestone_notes)
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

# ---------------------------------------------------------------- #332 bilingual release notes (V0.2 and later)
#
# A release that states its facts in one language has not stated them to its users, so from V0.2 the two surfaces are
# required and the load-bearing facts must appear in both. This is deliberately not whole-prose equivalence and not
# machine translation: it asserts that each surface states the same few facts a user acts on, which is the part that
# can be checked mechanically without pretending two languages can be compared as strings.
function(hyremote_notes_surfaces notes_text out_zh out_en out_ok)
    string(FIND "${notes_text}" "# 中文" zh_at)
    string(FIND "${notes_text}" "# English" en_at)
    if(zh_at EQUAL -1 OR en_at EQUAL -1 OR NOT zh_at LESS en_at)
        set(${out_ok} FALSE PARENT_SCOPE)
        return()
    endif()

    math(EXPR zh_len "${en_at} - ${zh_at}")
    string(SUBSTRING "${notes_text}" ${zh_at} ${zh_len} zh_surface)
    string(SUBSTRING "${notes_text}" ${en_at} -1 en_surface)
    string(LENGTH "${zh_surface}" zh_size)
    string(LENGTH "${en_surface}" en_size)

    # A heading with nothing under it is not a surface.
    if(zh_size LESS 400 OR en_size LESS 400)
        set(${out_ok} FALSE PARENT_SCOPE)
        return()
    endif()

    foreach(fact IN ITEMS "V0.2.0.0" "0.0.0.0:5921" "SecurityType None" "认证" "未加密" "不适合暴露到 Internet")
        string(FIND "${zh_surface}" "${fact}" at)
        if(at EQUAL -1)
            set(${out_ok} FALSE PARENT_SCOPE)
            return()
        endif()
    endforeach()
    foreach(fact IN ITEMS "V0.2.0.0" "0.0.0.0:5921" "SecurityType None" "authentication" "unencrypted"
            "not Internet-safe")
        string(FIND "${en_surface}" "${fact}" at)
        if(at EQUAL -1)
            set(${out_ok} FALSE PARENT_SCOPE)
            return()
        endif()
    endforeach()

    set(${out_zh} "${zh_surface}" PARENT_SCOPE)
    set(${out_en} "${en_surface}" PARENT_SCOPE)
    set(${out_ok} TRUE PARENT_SCOPE)
endfunction()

set(notes_fixture_dir "${CMAKE_CURRENT_BINARY_DIR}/hyremote-release-notes-fixtures")
file(MAKE_DIRECTORY "${notes_fixture_dir}")

# Every V0.2.x release note that exists carries the contract, so a later note in this train cannot be written in one
# language only while its predecessor is bilingual.
file(GLOB v02_notes_paths "${HYREMOTE_SOURCE_DIR}/docs/releases/v0.2.*.md")
list(SORT v02_notes_paths)
if(NOT v02_notes_paths)
    message(FATAL_ERROR "release-readiness: the V0.2 train has no release notes to check")
endif()
foreach(v02_notes_path IN LISTS v02_notes_paths)
    get_filename_component(v02_notes_name "${v02_notes_path}" NAME_WE)
    file(READ "${v02_notes_path}" v02_notes_text)
    hyremote_notes_surfaces("${v02_notes_text}" v02_zh v02_en v02_surfaces_ok)
    if(NOT v02_surfaces_ok)
        message(FATAL_ERROR
            "release-readiness: ${v02_notes_name} must carry a non-empty Chinese surface and a non-empty English "
            "surface, and each must state the release identity, the default listener, the security type, the "
            "authentication boundary and the unencrypted / not-Internet-safe warning")
    endif()
endforeach()

set(v020_notes_path "${HYREMOTE_SOURCE_DIR}/docs/releases/v0.2.0.0.md")
file(READ "${v020_notes_path}" v020_notes)

# Network trust is not a state the product can verify for itself, so the vocabulary that presents it as one must not
# reappear in a release note. The honest statement is what the artifact does: authentication off, no transport
# encryption.
foreach(forbidden_trust_token IN ITEMS "BASIC_TRUSTED_LAN" "OPEN_TRUSTED_LAN" "AUTHENTICATED_TRUSTED_LAN")
    string(FIND "${v020_notes}" "${forbidden_trust_token}" trust_at)
    if(NOT trust_at EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: the V0.2.0.0 notes name '${forbidden_trust_token}', which presents network trust as a "
            "state the product can verify for itself")
    endif()
endforeach()

# Fixtures. The real notes are the positive case; the two drifts a bilingual contract exists to catch are the
# negatives, and they are written out so a failure can be reproduced by reading the file that caused it.
string(FIND "${v020_notes}" "# English" fixture_en_at)
string(SUBSTRING "${v020_notes}" 0 ${fixture_en_at} fixture_no_english)
file(WRITE "${notes_fixture_dir}/v0.2.0.0-missing-english.md" "${fixture_no_english}")
hyremote_notes_surfaces("${fixture_no_english}" _zh _en no_english_ok)
if(no_english_ok)
    message(FATAL_ERROR "release-readiness: the missing-English fixture was accepted")
endif()

string(SUBSTRING "${v020_notes}" ${fixture_en_at} -1 fixture_no_chinese)
file(WRITE "${notes_fixture_dir}/v0.2.0.0-missing-chinese.md" "${fixture_no_chinese}")
hyremote_notes_surfaces("${fixture_no_chinese}" _zh _en no_chinese_ok)
if(no_chinese_ok)
    message(FATAL_ERROR "release-readiness: the missing-Chinese fixture was accepted")
endif()

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

# #219 moved ownership of the single delivered shared runtime out of the Embedded C++ frontend.
# Preserve the binary/export contract while validating its new owner explicitly.
file(READ "${HYREMOTE_SOURCE_DIR}/src/runtime/CMakeLists.txt" remoteaccess_cmake)
foreach(required_token
        "add_library(hyremote-remoteaccess SHARED"
        "OUTPUT_NAME HyRemoteRemoteAccess"
        "EXPORT_NAME RemoteAccess")
    string(FIND "${remoteaccess_cmake}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "release-readiness: RemoteAccess shared runtime contract missing: ${required_token}")
    endif()
endforeach()

# The delivered export header is generated and installed by the shared runtime that owns the target, and the public
# facade includes it by its documented name. The frontend CMakeLists no longer names the generated file, so the
# contract is asserted where it is defined rather than where it used to be spelled out.
file(READ "${HYREMOTE_SOURCE_DIR}/src/integrations/cpp/CMakeLists.txt" cpp_cmake)
string(FIND "${cpp_cmake}" "src/remote_access.cpp" found)
if(found EQUAL -1)
    message(FATAL_ERROR "release-readiness: the C++ facade lost its RemoteAccess implementation")
endif()
file(READ "${HYREMOTE_SOURCE_DIR}/src/runtime/CMakeLists.txt" runtime_export_cmake)
foreach(required_token
        "generate_export_header(hyremote-remoteaccess"
        "generated/HyRemote/RemoteAccessExport.h")
    string(FIND "${runtime_export_cmake}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "release-readiness: shared runtime export contract missing: ${required_token}")
    endif()
endforeach()
file(READ "${HYREMOTE_SOURCE_DIR}/src/integrations/cpp/include/HyRemote/RemoteAccess.h" facade_header)
string(FIND "${facade_header}" "HyRemote/RemoteAccessExport.h" found)
if(found EQUAL -1)
    message(FATAL_ERROR "release-readiness: the public C++ facade must include its generated export header")
endif()
string(FIND "${cpp_cmake}" "add_library(hyremote-remoteaccess SHARED" cpp_owns_runtime)
if(NOT cpp_owns_runtime EQUAL -1)
    message(FATAL_ERROR "release-readiness: Embedded C++ frontend must not re-own the common shared runtime")
endif()

file(READ "${HYREMOTE_SOURCE_DIR}/src/integrations/qml/CMakeLists.txt" qml_cmake)
string(FIND "${qml_cmake}" "TARGETS hyremote-qml\n    EXPORT HyRemoteTargets" qml_export)
if(NOT qml_export EQUAL -1)
    message(FATAL_ERROR
        "release-readiness: declarative QML backing library must not become a second C++ SDK target")
endif()

file(READ "${HYREMOTE_SOURCE_DIR}/src/integrations/qpa/CMakeLists.txt" qpa_cmake)
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
        "is not a second application Runtime or ordinary exported SDK target"
        "does not expose `HyRemote::QpaPlatform` as an application target"
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

# The V0.1 security boundary is product truth a stale document can over-claim, and it has no other owner: V0.1 is a
# loopback-only Developer Preview with no encrypted or authenticated-encrypted profile, and the VeNCrypt/TLS work
# belongs to the V0.2 train under #143. This is asserted here - in the gate that already owns release-facing document
# truth - rather than in a separate prose checker with its own drifting authority.
foreach(security_document IN ITEMS
        docs/known-limitations.md docs/security-model.md docs/security.md docs/getting-started/generic.md)
    file(READ "${HYREMOTE_SOURCE_DIR}/${security_document}" security_text)
    string(REPLACE "\r\n" "\n" security_body "${security_text}")
    string(REPLACE "\n" ";" security_lines "${security_body}")
    foreach(line IN LISTS security_lines)
        foreach(token IN ITEMS "VeNCrypt" "TLS")
            string(FIND "${line}" "${token}" token_found)
            if(token_found EQUAL -1)
                continue()
            endif()
            string(FIND "${line}" "V1.0" v1_found)
            if(NOT v1_found EQUAL -1)
                # A line that denies the V1.0 claim is correct documentation, not a mislabel. Release-facing
                # documents exist in both languages, so the denial has to be recognised in either.
                set(denial_found -1)
                foreach(denial IN ITEMS "not " "不是" "不 是")
                    string(FIND "${line}" "${denial}" _denial_hit)
                    if(NOT _denial_hit EQUAL -1)
                        set(denial_found 0)
                        break()
                    endif()
                endforeach()
                if(denial_found EQUAL -1)
                    message(FATAL_ERROR
                        "release-readiness: ${security_document} labels the ${token} work as V1.0.0.0; it is V0.2 "
                        "work under #143")
                endif()
            endif()
        endforeach()
    endforeach()
endforeach()

# `examples/README.md` is the single V0.1 developer entry, so it has to stay a user entry rather than a release
# catalogue. These are stable structural facts, not prose: the three canonical adoption paths must be reachable from
# it, the reference matrix and security boundary must be stated, and the retired release storytelling (the E-numbered
# taxonomy and the V0.0.x frontend-coded labels) must not reappear as current navigation.
file(READ "${HYREMOTE_SOURCE_DIR}/examples/README.md" examples_entry)
foreach(required_entry_token IN ITEMS
        "learning/01-widgets-cpp"
        "learning/02-quick-cpp"
        "learning/03-zero-code-generic"
        "docs/getting-started/cpp.md"
        "docs/getting-started/generic.md"
        "docs/guide/deployment.md"
        "Embedded C++"
        "Generic Plugin"
        "Declarative QML API"
        "Transparent QPA"
        "Preview"
        "Qt 6.8.3"
        "Windows x86_64"
        "Linux x86_64"
        # #174: the boundary is expressed by the shipped listener and its trust statement, not by a
        # loopback-only policy that no longer exists.
        "0.0.0.0:5921"
        "trusted LAN"
        "Qt 5.15 is not yet qualified")
    string(FIND "${examples_entry}" "${required_entry_token}" entry_token_found)
    if(entry_token_found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: examples/README.md no longer states '${required_entry_token}'; the V0.1 entry must "
            "keep its adoption paths, reference matrix and security boundary")
    endif()
endforeach()
foreach(retired_entry_token IN ITEMS
        "| E1 |" "| E2 |" "| E3 |" "| E4 |" "| E5 |" "| E6 |" "| E7 |"
        "V0.0.1.0" "V0.0.2.0" "V0.0.3.0"
        "# HyRemote V1 examples")
    string(FIND "${examples_entry}" "${retired_entry_token}" retired_token_found)
    if(NOT retired_token_found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: examples/README.md carries retired release storytelling as current navigation: "
            "'${retired_entry_token}'")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/examples/CMakeLists.txt" examples_cmake)
foreach(required_example
        "add_subdirectory(learning/01-widgets-cpp)"
        "add_subdirectory(learning/02-quick-cpp)"
        "add_subdirectory(learning/03-zero-code-generic/widgets-app)"
        "add_subdirectory(learning/03-zero-code-generic/quick-app)"
        "add_subdirectory(qml-basic)"
        "add_subdirectory(qpa-proxy-existing-app)"
        "add_subdirectory(remote-support-showcase)")
    string(FIND "${examples_cmake}" "${required_example}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: common V1 examples graph missing required example: ${required_example}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/examples/qpa-proxy-existing-app/CMakeLists.txt" e4_cmake)
foreach(required_token
        "target_link_libraries(hyremote-qpa-proxy-existing-app PRIVATE Qt6::Widgets)"
        "HYREMOTE_EXAMPLE_DEPLOY_QPA"
        "find_package(HyRemote CONFIG REQUIRED)"
        "hyremote_deploy(TARGET hyremote-qpa-proxy-existing-app QPA)")
    string(FIND "${e4_cmake}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: E4 ordinary-Qt/deployment contract missing: ${required_token}")
    endif()
endforeach()
foreach(forbidden_token
        "HyRemote::RemoteAccess"
        "HyRemote::QpaPlatform")
    string(FIND "${e4_cmake}" "${forbidden_token}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: E4 application project leaked a HyRemote link target: ${forbidden_token}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/tests/consumer-installed-qpa/CMakeLists.txt" clean_qpa_cmake)
foreach(required_token
        "examples/qpa-proxy-existing-app/main.cpp"
        "target_link_libraries(hyremote-installed-qpa-consumer PRIVATE Qt6::Widgets)"
        "hyremote_deploy(TARGET hyremote-installed-qpa-consumer QPA)")
    string(FIND "${clean_qpa_cmake}" "${required_token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness: clean installed-QPA E4 evidence is not bound to the real example: ${required_token}")
    endif()
endforeach()
if(EXISTS "${HYREMOTE_SOURCE_DIR}/tests/consumer-installed-qpa/main.cpp")
    message(FATAL_ERROR
        "release-readiness: clean installed-QPA fixture must not maintain a duplicate E4 application source")
endif()

file(READ "${HYREMOTE_SOURCE_DIR}/src/core/include/hyremote/core/input.hpp" input_contract)
string(FIND "${input_contract}" "virtual void shutdown() noexcept" input_shutdown_contract)
if(input_shutdown_contract EQUAL -1)
    message(FATAL_ERROR
        "release-readiness: internal InputSink terminal shutdown contract was removed")
endif()

# The shared AccessInstance now owns runtime teardown; the Embedded C++ facade only maps public API.
file(READ "${HYREMOTE_SOURCE_DIR}/src/runtime/src/access_instance.cpp" remoteaccess_source)
string(FIND "${remoteaccess_source}" "session->stop();" session_stop_pos)
string(FIND "${remoteaccess_source}" "inputSink->shutdown();" input_shutdown_pos)
if(session_stop_pos EQUAL -1 OR input_shutdown_pos EQUAL -1 OR input_shutdown_pos LESS session_stop_pos)
    message(FATAL_ERROR
        "release-readiness: common runtime must quiesce Session before terminal target-input shutdown")
endif()

foreach(adapter_file
        "src/runtime/src/widgets/widget_target.cpp"
        "src/runtime/src/quick/quick_target.cpp")
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

# The composite target is runtime-owned now: #219 finished moving it out of the QPA frontend, so the terminal
# child-input behavior is pinned at the canonical Runtime::Automatic path instead of the retired frontend copy.
file(READ "${HYREMOTE_SOURCE_DIR}/src/runtime/src/automatic/interactive_composite_target.cpp" qpa_input_source)
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
        "src/integrations/cpp/tests/test_remote_access.cpp|inputShutdowns"
        "src/integrations/cpp/tests/test_widgets_input_backpressure.cpp|testShutdownBalancesDeliveredStateAndDropsPendingInput"
        "src/integrations/cpp/tests/test_quick_input_backpressure.cpp|testShutdownBalancesDeliveredStateAndDropsPendingInput"
        "src/integrations/cpp/tests/test_rfb_widget_disconnect_backpressure.cpp|testDisconnectCleanupCrossesSaturatedAdapterMailbox"
        "src/runtime/tests/automatic_composite_input_test.cpp|shutdownCalls")
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
        "the transport balances recognized held state contributed by that viewer"
        "repeated teardown is idempotent and does not synthesize duplicate releases"
        "rejected"
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
        "docs/compatibility.md|explicit Runtime lifecycle"
        "docs/known-limitations.md|Stopping the runtime is explicit and complete")
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
    "(project ${source_project_version}, canonical repository layout + milestone notes + minimal SDK surface + complete V1 user entry points)")

