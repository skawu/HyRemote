get_filename_component(_hyremote_runtime_src
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../runtime/src" ABSOLUTE)

add_executable(hyremote-remoteaccess-test
    test_remote_access.cpp
)

target_link_libraries(hyremote-remoteaccess-test
    PRIVATE
        hyremote-remoteaccess
        HyRemote::Core
)

target_include_directories(hyremote-remoteaccess-test
    PRIVATE
        ${_hyremote_runtime_src}
)

add_test(NAME hyremote-remoteaccess-test COMMAND hyremote-remoteaccess-test)

add_executable(hyremote-security-descriptor-test
    test_security_descriptor.cpp
    ${_hyremote_runtime_src}/detail/security_descriptor.cpp
)
target_link_libraries(hyremote-security-descriptor-test
    PRIVATE
        hyremote-remoteaccess
        Qt6::Core
        Qt6::Network
)
target_include_directories(hyremote-security-descriptor-test
    PRIVATE
        ${_hyremote_runtime_src}
)
add_test(NAME hyremote-security-descriptor-test COMMAND hyremote-security-descriptor-test)
set_tests_properties(hyremote-security-descriptor-test PROPERTIES TIMEOUT 15)

add_executable(hyremote-remoteaccess-error-ack-test
    test_remote_access_error_ack.cpp
)
target_link_libraries(hyremote-remoteaccess-error-ack-test
    PRIVATE
        hyremote-remoteaccess
        HyRemote::Core
        Qt6::Core
)
target_include_directories(hyremote-remoteaccess-error-ack-test
    PRIVATE
        ${_hyremote_runtime_src}
)
add_test(NAME hyremote-remoteaccess-error-ack-test COMMAND hyremote-remoteaccess-error-ack-test)
set_tests_properties(hyremote-remoteaccess-error-ack-test PROPERTIES TIMEOUT 15)

add_executable(hyremote-remoteaccess-target-loss-test
    test_remote_access_target_loss.cpp
)
target_link_libraries(hyremote-remoteaccess-target-loss-test
    PRIVATE
        hyremote-remoteaccess
        HyRemote::Core
        Qt6::Core
)
target_include_directories(hyremote-remoteaccess-target-loss-test
    PRIVATE
        ${_hyremote_runtime_src}
)
add_test(NAME hyremote-remoteaccess-target-loss-test COMMAND hyremote-remoteaccess-target-loss-test)
set_tests_properties(hyremote-remoteaccess-target-loss-test PROPERTIES TIMEOUT 15)

add_executable(hyremote-target-component-provider-test
    test_target_component_provider.cpp
)
target_link_libraries(hyremote-target-component-provider-test
    PRIVATE
        hyremote-remoteaccess
        HyRemote::Core
        Qt6::Core
)
target_include_directories(hyremote-target-component-provider-test
    PRIVATE
        ${_hyremote_runtime_src}
)
add_test(NAME hyremote-target-component-provider-test COMMAND hyremote-target-component-provider-test)

add_executable(hyremote-input-mailbox-admission-test
    test_input_mailbox_admission.cpp
)
target_link_libraries(hyremote-input-mailbox-admission-test
    PRIVATE
        hyremote-remoteaccess
        HyRemote::Core
        Qt6::Core
)
target_include_directories(hyremote-input-mailbox-admission-test
    PRIVATE
        ${_hyremote_runtime_src}
)
add_test(NAME hyremote-input-mailbox-admission-test COMMAND hyremote-input-mailbox-admission-test)
set_tests_properties(hyremote-input-mailbox-admission-test PROPERTIES TIMEOUT 15)

if(HYREMOTE_WITH_VNC)
    if(HYREMOTE_TRANSPORT_SECURITY_AVAILABLE)
        add_executable(hyremote-vnc-auth-test
            test_vnc_auth.cpp
            ${_hyremote_runtime_src}/transport/vnc_auth.cpp
        )
        target_link_libraries(hyremote-vnc-auth-test
            PRIVATE
                hyremote-remoteaccess
                OpenSSL::Crypto
                Qt6::Core
                Qt6::Network
        )
        target_include_directories(hyremote-vnc-auth-test
            PRIVATE
                ${_hyremote_runtime_src}
        )
        add_test(NAME hyremote-vnc-auth-test COMMAND hyremote-vnc-auth-test)
        set_tests_properties(hyremote-vnc-auth-test PROPERTIES TIMEOUT 15)
    endif()

    if(HYREMOTE_TRANSPORT_SECURITY_AVAILABLE)
        add_executable(hyremote-rfb-vnc-auth-handshake-test
            test_rfb_vnc_auth_handshake.cpp
            ${_hyremote_runtime_src}/transport/vnc_auth.cpp
        )
        target_link_libraries(hyremote-rfb-vnc-auth-handshake-test
            PRIVATE
                hyremote-remoteaccess
                OpenSSL::Crypto
                # This test includes hyremote/core/transport.hpp, so it needs the Core interface the other tests in
                # this file already link. Without it the target cannot compile in its own configuration, which is how
                # a registered transport-security test could sit unbuildable while security was off by default.
                HyRemote::Core
                Qt6::Core
                Qt6::Network
        )
        target_include_directories(hyremote-rfb-vnc-auth-handshake-test
            PRIVATE
                ${_hyremote_runtime_src}
        )
        add_test(NAME hyremote-rfb-vnc-auth-handshake-test
                 COMMAND hyremote-rfb-vnc-auth-handshake-test)
        set_tests_properties(hyremote-rfb-vnc-auth-handshake-test PROPERTIES TIMEOUT 15)
    endif()

    add_executable(hyremote-rfb-test-server
        rfb_test_server.cpp
    )
    target_link_libraries(hyremote-rfb-test-server
        PRIVATE
            hyremote-remoteaccess
            HyRemote::Core
            Qt6::Core
            Qt6::Network
    )
    target_include_directories(hyremote-rfb-test-server
        PRIVATE
            ${_hyremote_runtime_src}
    )

    # Deterministic transport-level regression for simultaneous viewers sharing one Qt target.
    add_executable(hyremote-rfb-multi-client-input-test
        test_rfb_multi_client_input.cpp
    )
    target_link_libraries(hyremote-rfb-multi-client-input-test
        PRIVATE
            hyremote-remoteaccess
            HyRemote::Core
            Qt6::Core
            Qt6::Network
    )
    target_include_directories(hyremote-rfb-multi-client-input-test
        PRIVATE
            ${_hyremote_runtime_src}
    )
    add_test(NAME hyremote-rfb-multi-client-input-test COMMAND hyremote-rfb-multi-client-input-test)
    set_tests_properties(hyremote-rfb-multi-client-input-test PROPERTIES TIMEOUT 15)
endif()

if(HYREMOTE_REMOTEACCESS_WITH_WIDGETS)
    add_executable(hyremote-widgets-capture-test
        test_widgets_capture.cpp
    )
    target_link_libraries(hyremote-widgets-capture-test
        PRIVATE
            hyremote-remoteaccess
            HyRemote::Core
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
    )
    target_include_directories(hyremote-widgets-capture-test
        PRIVATE
            ${_hyremote_runtime_src}
    )
    add_test(NAME hyremote-widgets-capture-test COMMAND hyremote-widgets-capture-test)
    set_tests_properties(hyremote-widgets-capture-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    add_executable(hyremote-widgets-input-backpressure-test
        test_widgets_input_backpressure.cpp
    )
    target_link_libraries(hyremote-widgets-input-backpressure-test
        PRIVATE
            hyremote-remoteaccess
            HyRemote::Core
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
    )
    target_include_directories(hyremote-widgets-input-backpressure-test
        PRIVATE
            ${_hyremote_runtime_src}
    )
    add_test(NAME hyremote-widgets-input-backpressure-test COMMAND hyremote-widgets-input-backpressure-test)
    set_tests_properties(hyremote-widgets-input-backpressure-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    add_executable(hyremote-widgets-input-routing-test
        test_widgets_input_routing.cpp
    )
    target_link_libraries(hyremote-widgets-input-routing-test
        PRIVATE
            hyremote-remoteaccess
            HyRemote::Core
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
    )
    target_include_directories(hyremote-widgets-input-routing-test
        PRIVATE
            ${_hyremote_runtime_src}
    )
    add_test(NAME hyremote-widgets-input-routing-test COMMAND hyremote-widgets-input-routing-test)
    set_tests_properties(hyremote-widgets-input-routing-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    add_executable(hyremote-listener-address-matrix-test
        test_listener_address_matrix.cpp
    )
    target_link_libraries(hyremote-listener-address-matrix-test
        PRIVATE
            hyremote-remoteaccess
            HyRemote::Core
            Qt6::Core
            Qt6::Network
            Qt6::Widgets
    )
    target_include_directories(hyremote-listener-address-matrix-test
        PRIVATE
            ${_hyremote_runtime_src}
    )
    add_test(NAME hyremote-listener-address-matrix-test COMMAND hyremote-listener-address-matrix-test)
    set_tests_properties(hyremote-listener-address-matrix-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    add_executable(hyremote-rfb-widget-disconnect-backpressure-test
        test_rfb_widget_disconnect_backpressure.cpp
    )
    target_link_libraries(hyremote-rfb-widget-disconnect-backpressure-test
        PRIVATE
            hyremote-remoteaccess
            HyRemote::Core
            Qt6::Core
            Qt6::Network
            Qt6::Widgets
    )
    target_include_directories(hyremote-rfb-widget-disconnect-backpressure-test
        PRIVATE
            ${_hyremote_runtime_src}
    )
    add_test(NAME hyremote-rfb-widget-disconnect-backpressure-test
             COMMAND hyremote-rfb-widget-disconnect-backpressure-test)
    set_tests_properties(hyremote-rfb-widget-disconnect-backpressure-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()

if(HYREMOTE_REMOTEACCESS_WITH_QUICK)
    add_executable(hyremote-quick-capture-test
        test_quick_capture.cpp
    )
    target_link_libraries(hyremote-quick-capture-test
        PRIVATE
            hyremote-remoteaccess
            HyRemote::Core
            Qt6::Core
            Qt6::Gui
            Qt6::Quick
    )
    target_include_directories(hyremote-quick-capture-test
        PRIVATE
            ${_hyremote_runtime_src}
    )
    add_test(NAME hyremote-quick-capture-test COMMAND hyremote-quick-capture-test)
    set_tests_properties(hyremote-quick-capture-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
    )

    add_executable(hyremote-quick-input-routing-test
        test_quick_input_routing.cpp
    )
    target_link_libraries(hyremote-quick-input-routing-test
        PRIVATE
            hyremote-remoteaccess
            HyRemote::Core
            Qt6::Core
            Qt6::Gui
            Qt6::Quick
    )
    target_include_directories(hyremote-quick-input-routing-test
        PRIVATE
            ${_hyremote_runtime_src}
    )
    add_test(NAME hyremote-quick-input-routing-test COMMAND hyremote-quick-input-routing-test)
    set_tests_properties(hyremote-quick-input-routing-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
    )

    add_executable(hyremote-quick-input-backpressure-test
        test_quick_input_backpressure.cpp
    )
    target_link_libraries(hyremote-quick-input-backpressure-test
        PRIVATE
            hyremote-remoteaccess
            HyRemote::Core
            Qt6::Core
            Qt6::Gui
            Qt6::Quick
    )
    target_include_directories(hyremote-quick-input-backpressure-test
        PRIVATE
            ${_hyremote_runtime_src}
    )
    add_test(NAME hyremote-quick-input-backpressure-test COMMAND hyremote-quick-input-backpressure-test)
    set_tests_properties(hyremote-quick-input-backpressure-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
    )
endif()

unset(_hyremote_runtime_src)
