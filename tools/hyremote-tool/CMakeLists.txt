cmake_minimum_required(VERSION 3.21)

project(HyRemoteRemoteSupportShowcase LANGUAGES CXX)

# HyRemoteTool is an out-of-process companion client: it connects to a remote
# HyRemote-enabled application and does not link or embed the HyRemote Runtime.
add_executable(hyremote-remote-support-showcase
    main.cpp
    framebuffer_view.cpp
    framebuffer_view.h
    rfb_client.cpp
    rfb_client.h
    language_preference.hpp
    connection_profile_store.cpp
    connection_profile_store.hpp
    basic_diagnostics.cpp
    basic_diagnostics.hpp
    tool_diagnostic_session.cpp
    tool_diagnostic_session.hpp
    tool_diagnostics_dialog.cpp
    tool_diagnostics_dialog.hpp
    ../../examples/hyremote_branding.cpp
    ../../logo/hyremote-branding.qrc
)
set_target_properties(hyremote-remote-support-showcase PROPERTIES
    OUTPUT_NAME hyremote-tool
    AUTOMOC ON
)

# Ensure the canonical Qt logo .qrc is compiled into the application, including standalone GUI builds.
set_target_properties(hyremote-remote-support-showcase PROPERTIES AUTORCC ON)
# #241: the native Windows file icon is separate acceptance from the Qt window icon.
if(WIN32 AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../logo/hyremote-app-icon.rc")
    target_sources(hyremote-remote-support-showcase PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/../../logo/hyremote-app-icon.rc")
endif()
target_compile_features(hyremote-remote-support-showcase PRIVATE cxx_std_17)
if(CMAKE_PROJECT_VERSION STREQUAL "")
    set(HYREMOTE_TOOL_BUILD_VERSION "0.0.0")
else()
    set(HYREMOTE_TOOL_BUILD_VERSION "${CMAKE_PROJECT_VERSION}")
endif()
target_compile_definitions(hyremote-remote-support-showcase PRIVATE HYREMOTE_TOOL_VERSION="${HYREMOTE_TOOL_BUILD_VERSION}")
if(MSVC)
    target_compile_options(hyremote-remote-support-showcase PRIVATE /utf-8)
endif()
target_link_libraries(hyremote-remote-support-showcase
    PRIVATE
        Qt6::Widgets
        Qt6::Network
)

install(TARGETS hyremote-remote-support-showcase RUNTIME DESTINATION bin)

# H-T01=B: the developer workbench is a Qt 6.8-only mode of the SAME tool
# executable. Qt 5.15 keeps the existing Widgets-only compatibility lane and
# never sees these sources, imports, compile definition or test.
set(HYREMOTE_TOOL_WORKBENCH_DEPLOY OFF)
if(HYREMOTE_QT_MAJOR STREQUAL "6"
   AND COMMAND qt_add_qml_module
   AND TARGET Qt6::Qml
   AND TARGET Qt6::Quick
   AND TARGET Qt6::QuickControls2)
    set(HYREMOTE_TOOL_WORKBENCH_DEPLOY ON)
    find_package(Qt6 6.8 REQUIRED COMPONENTS QuickControls2)
    qt_policy(SET QTP0001 NEW)
    target_sources(hyremote-remote-support-showcase PRIVATE
        hy_sdk_inspector.hpp
        hy_sdk_inspector.cpp
        hy_tool_workbench.hpp
        hy_tool_workbench.cpp
    )
    target_compile_definitions(hyremote-remote-support-showcase PRIVATE HYREMOTE_TOOL_WORKBENCH=1)
    target_link_libraries(hyremote-remote-support-showcase PRIVATE
        Qt6::Qml Qt6::Quick Qt6::QuickControls2)
    qt_add_qml_module(hyremote-remote-support-showcase
        URI HyRemoteToolWorkbench
        VERSION 1.0
        QML_FILES HyToolWorkbench.qml
    )
    qt_finalize_executable(hyremote-remote-support-showcase)
elseif(HYREMOTE_QT_MAJOR STREQUAL "6")
    message(FATAL_ERROR "HyRemoteTool developer workbench requires Qt 6.8 Qml, Quick and QuickControls2")
endif()

# The tool needs Qt itself, not the HyRemote Runtime payload. Use Qt's own
# deployment script when the Qt major provides it; the Qt 5.15 build is a
# build-only compatibility lane until its packaging closure is qualified.
if(DEFINED QT_DEPLOY_SUPPORT AND NOT "${QT_DEPLOY_SUPPORT}" STREQUAL "")
    if(HYREMOTE_TOOL_WORKBENCH_DEPLOY)
        if(NOT COMMAND qt_generate_deploy_qml_app_script)
            message(FATAL_ERROR "HyRemoteTool workbench requires Qt 6 QML deployment support")
        endif()
        set_property(TARGET hyremote-remote-support-showcase APPEND PROPERTY
            QT_QML_IMPORT_PATH "${HyRemote_QML_IMPORT_PATH}")
        qt_generate_deploy_qml_app_script(
            TARGET hyremote-remote-support-showcase
            OUTPUT_SCRIPT hyremote_tool_deploy_script
            NO_UNSUPPORTED_PLATFORM_ERROR
            DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM
        )
    else()
        qt_generate_deploy_app_script(
            TARGET hyremote-remote-support-showcase
            OUTPUT_SCRIPT hyremote_tool_deploy_script
            NO_UNSUPPORTED_PLATFORM_ERROR
        )
    endif()
    install(SCRIPT ${hyremote_tool_deploy_script})
else()
    message(STATUS "  HyRemoteTool: Qt deployment helper unavailable; build only")
endif()

if(HYREMOTE_BUILD_TESTS)
    add_executable(hyremote-tool-basic-diagnostics-test
        test_basic_diagnostics.cpp
        basic_diagnostics.cpp
        basic_diagnostics.hpp
    )
    target_compile_features(hyremote-tool-basic-diagnostics-test PRIVATE cxx_std_17)
    target_link_libraries(hyremote-tool-basic-diagnostics-test
        PRIVATE Qt6::Core Qt6::Network)
    add_test(NAME hyremote-tool-basic-diagnostics
             COMMAND $<TARGET_FILE:hyremote-tool-basic-diagnostics-test>)
    set_tests_properties(hyremote-tool-basic-diagnostics PROPERTIES TIMEOUT 20)
    if(COMMAND hyremote_label_test_family)
        hyremote_label_test_family("unit;tool;fast" hyremote-tool-basic-diagnostics)
    endif()

    add_executable(hyremote-tool-diagnostic-session-test
        test_tool_diagnostic_session.cpp
        tool_diagnostic_session.cpp
        tool_diagnostic_session.hpp
        basic_diagnostics.cpp
        basic_diagnostics.hpp
    )
    target_compile_features(hyremote-tool-diagnostic-session-test PRIVATE cxx_std_17)
    target_link_libraries(hyremote-tool-diagnostic-session-test
        PRIVATE Qt6::Core Qt6::Gui Qt6::Network)
    add_test(NAME hyremote-tool-diagnostic-session
             COMMAND $<TARGET_FILE:hyremote-tool-diagnostic-session-test>)
    set_tests_properties(hyremote-tool-diagnostic-session PROPERTIES TIMEOUT 20)
    if(COMMAND hyremote_label_test_family)
        hyremote_label_test_family("unit;tool;fast" hyremote-tool-diagnostic-session)
    endif()

    add_executable(hyremote-tool-connection-profiles-test
        test_connection_profile_store.cpp
        connection_profile_store.cpp
        connection_profile_store.hpp
    )
    target_compile_features(hyremote-tool-connection-profiles-test PRIVATE cxx_std_17)
    target_link_libraries(hyremote-tool-connection-profiles-test PRIVATE Qt6::Core)
    add_test(NAME hyremote-tool-connection-profiles
             COMMAND $<TARGET_FILE:hyremote-tool-connection-profiles-test>)
    set_tests_properties(hyremote-tool-connection-profiles PROPERTIES TIMEOUT 30)
    if(COMMAND hyremote_label_test_family)
        hyremote_label_test_family("unit;tool;fast" hyremote-tool-connection-profiles)
    endif()

    add_executable(hyremote-tool-language-preference-test
        test_language_preference.cpp
        language_preference.hpp
    )
    target_compile_features(hyremote-tool-language-preference-test PRIVATE cxx_std_17)
    target_link_libraries(hyremote-tool-language-preference-test PRIVATE Qt6::Core)
    add_test(NAME hyremote-tool-language-preference
             COMMAND $<TARGET_FILE:hyremote-tool-language-preference-test>)
    set_tests_properties(hyremote-tool-language-preference PROPERTIES TIMEOUT 20)
    if(COMMAND hyremote_label_test_family)
        hyremote_label_test_family("unit;tool;fast" hyremote-tool-language-preference)
    endif()

    add_executable(hyremote-tool-rfb-client-test
        test_rfb_client.cpp
        rfb_client.cpp
        rfb_client.h
        tool_diagnostic_session.cpp
        tool_diagnostic_session.hpp
        basic_diagnostics.cpp
        basic_diagnostics.hpp
    )
    target_compile_features(hyremote-tool-rfb-client-test PRIVATE cxx_std_17)
    target_link_libraries(hyremote-tool-rfb-client-test
        PRIVATE Qt6::Core Qt6::Gui Qt6::Network)
    set_target_properties(hyremote-tool-rfb-client-test PROPERTIES AUTOMOC ON)
    add_test(NAME hyremote-tool-rfb-client
             COMMAND $<TARGET_FILE:hyremote-tool-rfb-client-test>)
    set_tests_properties(hyremote-tool-rfb-client PROPERTIES
        TIMEOUT 30
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
    if(COMMAND hyremote_label_test_family)
        hyremote_label_test_family("e2e;tool;fast" hyremote-tool-rfb-client)
    endif()

    # Construct the real client window offscreen. This must not connect to any
    # endpoint or open a local listener.
    add_test(
        NAME hyremote-tool-client-ui
        COMMAND $<TARGET_FILE:hyremote-remote-support-showcase> --verify-ui
    )
    set_tests_properties(hyremote-tool-client-ui PROPERTIES
        TIMEOUT 20
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
    if(COMMAND hyremote_label_test_family)
        hyremote_label_test_family("e2e;tool;fast" hyremote-tool-client-ui)
    endif()

    # Exercise the production QComboBox::activated signal with an already
    # selected item and a local settings write failure. The verification
    # command uses throwaway QSettings, never the developer's real profile.
    add_test(
        NAME hyremote-tool-client-ui-language-repeat
        COMMAND $<TARGET_FILE:hyremote-remote-support-showcase>
                --verify-ui --language en
    )
    set_tests_properties(hyremote-tool-client-ui-language-repeat PROPERTIES
        TIMEOUT 20
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;HYREMOTE_TOOL_TEST_LANGUAGE_CASE=repeat"
        PASS_REGULAR_EXPRESSION "TOOL_LANGUAGE_UI_PASS mode=repeat-select"
    )
    add_test(
        NAME hyremote-tool-client-ui-language-save-failure
        COMMAND $<TARGET_FILE:hyremote-remote-support-showcase>
                --verify-ui --language en
    )
    set_tests_properties(hyremote-tool-client-ui-language-save-failure PROPERTIES
        TIMEOUT 20
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;HYREMOTE_TOOL_TEST_LANGUAGE_CASE=save-failure"
        PASS_REGULAR_EXPRESSION "TOOL_LANGUAGE_UI_PASS mode=save-failure"
    )
    if(COMMAND hyremote_label_test_family)
        hyremote_label_test_family("e2e;tool;fast"
            hyremote-tool-client-ui-language-repeat
            hyremote-tool-client-ui-language-save-failure)
    endif()

    # Real offscreen window exercises local CRUD through the production
    # selector's activated(int), verifies persistence and no auto-connect.
    # Uses a fresh temporary profile file, never the runner's profile.
    add_test(
        NAME hyremote-tool-client-ui-profiles
        COMMAND $<TARGET_FILE:hyremote-remote-support-showcase> --verify-ui
    )
    set_tests_properties(hyremote-tool-client-ui-profiles PROPERTIES
        TIMEOUT 20
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;HYREMOTE_TOOL_TEST_PROFILE_CASE=crud"
        PASS_REGULAR_EXPRESSION "TOOL_PROFILE_UI_PASS"
    )
    if(COMMAND hyremote_label_test_family)
        hyremote_label_test_family("e2e;tool;fast" hyremote-tool-client-ui-profiles)
    endif()

    # Production Tool UI: untrusted RFB ServerInit metadata is not a grant,
    # cannot forge log lines, and cannot carry local control intent across
    # disconnect/reconnect. Real TCP sending is checked in the RFB test.
    add_test(
        NAME hyremote-tool-client-ui-servername
        COMMAND $<TARGET_FILE:hyremote-remote-support-showcase>
                --verify-ui --language en
    )
    set_tests_properties(hyremote-tool-client-ui-servername PROPERTIES
        TIMEOUT 20
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;HYREMOTE_TOOL_TEST_SERVER_NAME_CASE=metadata"
        PASS_REGULAR_EXPRESSION "TOOL_SERVERNAME_UI_PASS metadata=1 explicit-intent=1 revoked=1 reconnect-view-only=1 deactivation-view-only=1"
        FAIL_REGULAR_EXPRESSION "SERVERNAME_PRIVATE_MARKER"
    )
    if(COMMAND hyremote_label_test_family)
        hyremote_label_test_family("e2e;tool;fast" hyremote-tool-client-ui-servername)
    endif()

    # Production diagnostics navigation and manual privacy actions at two
    # window sizes and both supported languages, no remote socket or real
    # operator profile. Clipboard is touched only by explicit test click.
    add_test(
        NAME hyremote-tool-client-ui-diagnostics-compact
        COMMAND $<TARGET_FILE:hyremote-remote-support-showcase>
                --verify-ui --language zh
    )
    set_tests_properties(hyremote-tool-client-ui-diagnostics-compact PROPERTIES
        TIMEOUT 20
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;HYREMOTE_TOOL_TEST_DIAGNOSTICS_CASE=compact"
        PASS_REGULAR_EXPRESSION "TOOL_DIAGNOSTICS_UI_PASS viewport=800x600 actual-geometry=1 reopened-masked=1 lang=zh"
    )
    add_test(
        NAME hyremote-tool-client-ui-diagnostics-wide
        COMMAND $<TARGET_FILE:hyremote-remote-support-showcase>
                --verify-ui --language en
    )
    set_tests_properties(hyremote-tool-client-ui-diagnostics-wide PROPERTIES
        TIMEOUT 20
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;HYREMOTE_TOOL_TEST_DIAGNOSTICS_CASE=wide"
        PASS_REGULAR_EXPRESSION "TOOL_DIAGNOSTICS_UI_PASS viewport=1366x768 actual-geometry=1 reopened-masked=1 lang=en"
    )
    if(COMMAND hyremote_label_test_family)
        hyremote_label_test_family("e2e;tool;fast"
            hyremote-tool-client-ui-diagnostics-compact
            hyremote-tool-client-ui-diagnostics-wide)
    endif()

    # Smoke the REAL application command-line precedence, independent of any
    # saved UI preference in the runner's home directory.
    add_test(
        NAME hyremote-tool-client-ui-cli-en
        COMMAND $<TARGET_FILE:hyremote-remote-support-showcase>
                --verify-ui --language en
    )
    set_tests_properties(hyremote-tool-client-ui-cli-en PROPERTIES
        TIMEOUT 20
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION "TOOL_CLIENT_UI_PASS.*lang=en"
    )
    if(COMMAND hyremote_label_test_family)
        hyremote_label_test_family("e2e;tool;fast" hyremote-tool-client-ui-cli-en)
    endif()

    if(HYREMOTE_QT_MAJOR STREQUAL "6" AND TARGET Qt6::QuickControls2)
        add_test(
            NAME hyremote-tool-developer-workbench
            COMMAND $<TARGET_FILE:hyremote-remote-support-showcase>
                    --verify-workbench --language en
        )
        set_tests_properties(hyremote-tool-developer-workbench PROPERTIES
            TIMEOUT 30
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;HYREMOTE_TOOL_WORKBENCH_FIXTURES=${CMAKE_CURRENT_BINARY_DIR}/workbench-fixtures"
            PASS_REGULAR_EXPRESSION "TOOL_DEVELOPER_WORKBENCH_PASS root=1 pages=4 layouts=5"
        )
        if(COMMAND hyremote_label_test_family)
            hyremote_label_test_family("e2e;tool;fast" hyremote-tool-developer-workbench)
        endif()
    endif()
endif()
