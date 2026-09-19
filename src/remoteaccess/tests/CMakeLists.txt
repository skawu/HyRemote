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
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

add_test(NAME hyremote-remoteaccess-test COMMAND hyremote-remoteaccess-test)

add_executable(hyremote-security-descriptor-test
    test_security_descriptor.cpp
    ../src/detail/security_descriptor.cpp
)
target_link_libraries(hyremote-security-descriptor-test
    PRIVATE
        hyremote-remoteaccess
        Qt6::Core
        Qt6::Network
)
target_include_directories(hyremote-security-descriptor-test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
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
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
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
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
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
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
add_test(NAME hyremote-target-component-provider-test COMMAND hyremote-target-component-provider-test)

add_executable(hyremote-input-mailbox-admission-test
    test_input_mailbox_admission.cpp
)
target_link_libraries(hyremote-input-mailbox-admission-test
    PRIVATE
        HyRemote::Core
)
target_include_directories(hyremote-input-mailbox-admission-test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
add_test(NAME hyremote-input-mailbox-admission-test COMMAND hyremote-input-mailbox-admission-test)
set_tests_properties(hyremote-input-mailbox-admission-test PROPERTIES TIMEOUT 15)

if(HYREMOTE_WITH_VNC)
    # The auth test belongs to the capability, not to the request for it: it is built when a provider actually
    # supplied OpenSSL, which is the same condition that compiles the implementation it exercises.
    if(HYREMOTE_TRANSPORT_SECURITY_AVAILABLE)
        add_executable(hyremote-vnc-auth-test
            test_vnc_auth.cpp
            ../src/transport/vnc_auth.cpp
        )
        target_link_libraries(hyremote-vnc-auth-test
            PRIVATE
                hyremote-remoteaccess
                Qt6::Core
                ${HYREMOTE_OPENSSL_LINK_TARGETS}
        )
        target_include_directories(hyremote-vnc-auth-test
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/../src
        )
        add_test(NAME hyremote-vnc-auth-test COMMAND hyremote-vnc-auth-test)
        set_tests_properties(hyremote-vnc-auth-test PROPERTIES TIMEOUT 15)
    endif()

    # Network interoperability harness used by the dual-OS maintained-client workflow. It links the
    # same private factory as RemoteAccess; it is not installed and is not part of the public SDK.
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
            ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )

    # Deterministic transport-level regression for simultaneous viewers sharing one Qt target.
    # Per-client held-state accounting must not let one disconnect release another viewer's hold.
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
            ${CMAKE_CURRENT_SOURCE_DIR}/../src
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
            Qt6::Widgets
    )
    target_include_directories(hyremote-widgets-capture-test
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../src
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
            Qt6::Widgets
    )
    target_include_directories(hyremote-widgets-input-backpressure-test
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )
    add_test(NAME hyremote-widgets-input-backpressure-test COMMAND hyremote-widgets-input-backpressure-test)
    set_tests_properties(hyremote-widgets-input-backpressure-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    if(HYREMOTE_WITH_VNC)
        # Real composition proof for #90: RFB disconnect cleanup must cross Session and a saturated
        # Widgets mailbox without dropping the final accepted key/button releases.
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
                ${CMAKE_CURRENT_SOURCE_DIR}/../src
        )
        add_test(NAME hyremote-rfb-widget-disconnect-backpressure-test
                 COMMAND hyremote-rfb-widget-disconnect-backpressure-test)
        set_tests_properties(hyremote-rfb-widget-disconnect-backpressure-test PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
            TIMEOUT 20
        )
    endif()
endif()

if(HYREMOTE_REMOTEACCESS_WITH_QUICK)
    add_executable(hyremote-quick-capture-test
        test_quick_capture.cpp
    )
    target_link_libraries(hyremote-quick-capture-test
        PRIVATE
            hyremote-remoteaccess
            HyRemote::Core
            Qt6::Quick
    )
    target_include_directories(hyremote-quick-capture-test
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )
    add_test(NAME hyremote-quick-capture-test COMMAND hyremote-quick-capture-test)
    set_tests_properties(hyremote-quick-capture-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
    )

    add_executable(hyremote-quick-input-backpressure-test
        test_quick_input_backpressure.cpp
    )
    target_link_libraries(hyremote-quick-input-backpressure-test
        PRIVATE
            hyremote-remoteaccess
            HyRemote::Core
            Qt6::Quick
    )
    target_include_directories(hyremote-quick-input-backpressure-test
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )
    add_test(NAME hyremote-quick-input-backpressure-test COMMAND hyremote-quick-input-backpressure-test)
    set_tests_properties(hyremote-quick-input-backpressure-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
    )
endif()
