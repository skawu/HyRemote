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
    ../../examples/hyremote_branding.cpp
    ../../logo/hyremote-branding.qrc
)
set_target_properties(hyremote-remote-support-showcase PROPERTIES
    OUTPUT_NAME hyremote-tool
    AUTOMOC ON
)

target_compile_features(hyremote-remote-support-showcase PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(hyremote-remote-support-showcase PRIVATE /utf-8)
endif()
target_link_libraries(hyremote-remote-support-showcase
    PRIVATE
        Qt6::Widgets
        Qt6::Network
)

install(TARGETS hyremote-remote-support-showcase RUNTIME DESTINATION bin)

# The tool needs Qt itself, not the HyRemote Runtime payload. Use Qt's own
# deployment script when the Qt major provides it; the Qt 5.15 build is a
# build-only compatibility lane until its packaging closure is qualified.
if(DEFINED QT_DEPLOY_SUPPORT AND NOT "${QT_DEPLOY_SUPPORT}" STREQUAL "")
    qt_generate_deploy_app_script(
        TARGET hyremote-remote-support-showcase
        OUTPUT_SCRIPT hyremote_tool_deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${hyremote_tool_deploy_script})
else()
    message(STATUS "  HyRemoteTool: Qt deployment helper unavailable; build only")
endif()

if(HYREMOTE_BUILD_TESTS)
    add_executable(hyremote-tool-rfb-client-test
        test_rfb_client.cpp
        rfb_client.cpp
        rfb_client.h
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
endif()
