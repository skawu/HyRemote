get_filename_component(_hyremote_runtime_src
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/src" ABSOLUTE)

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
        HyRemote::Core
)
target_include_directories(hyremote-input-mailbox-admission-test
    PRIVATE
        ${_hyremote_runtime_src}
)
add_test(NAME hyremote-input-mailbox-admission-test COMMAND hyremote-input-mailbox-admission-test)
set_tests_properties(hyremote-input-mailbox-admission-test PROPERTIES TIMEOUT 15)

if(HYREMOTE_TRANSPORT_SECURITY_AVAILABLE)
    # The capability definition is private to the runtime, but a test that asserts what a secure profile does has to
    # know whether the mechanism exists in this build. It is derived from the same CMake decision as the library's
    # own definition, so the two can never disagree.
    target_compile_definitions(hyremote-remoteaccess-test PRIVATE HYREMOTE_HAS_TRANSPORT_SECURITY=1)
endif()

if(HYREMOTE_WITH_VNC)
    # The auth test belongs to the capability, not to the request for it: it is built when a provider actually
    # supplied OpenSSL, which is the same condition that compiles the implementation it exercises.
    if(HYREMOTE_TRANSPORT_SECURITY_AVAILABLE)
        add_executable(hyremote-vnc-auth-test
            test_vnc_auth.cpp
            ${_hyremote_runtime_src}/transport/vnc_auth.cpp
        )
        target_link_libraries(hyremote-vnc-auth-test
            PRIVATE
                hyremote-remoteaccess
                Qt6::Core
                OpenSSL::Crypto
        )
        target_include_directories(hyremote-vnc-auth-test
            PRIVATE
                ${_hyremote_runtime_src}
        )
        add_test(NAME hyremote-vnc-auth-test COMMAND hyremote-vnc-auth-test)
        set_tests_properties(hyremote-vnc-auth-test PROPERTIES TIMEOUT 15)
    endif()

    # #143: the handshake itself is measured over a real socket - what is offered, what is verified, what is
    # refused and what is bounded. It links the same private factory as RemoteAccess, and only exists when the
    # capability does.
    if(HYREMOTE_TRANSPORT_SECURITY_AVAILABLE)
        add_executable(hyremote-rfb-vnc-auth-handshake-test
            test_rfb_vnc_auth_handshake.cpp
            ${_hyremote_runtime_src}/transport/vnc_auth.cpp
        )
        target_link_libraries(hyremote-rfb-vnc-auth-handshake-test
            PRIVATE
                hyremote-remoteaccess
                HyRemote::Core
                Qt6::Core
                Qt6::Network
                OpenSSL::Crypto
        )
        target_include_directories(hyremote-rfb-vnc-auth-handshake-test
            PRIVATE
                ${_hyremote_runtime_src}
        )
        add_test(NAME hyremote-rfb-vnc-auth-handshake-test
                 COMMAND hyremote-rfb-vnc-auth-handshake-test)
        set_tests_properties(hyremote-rfb-vnc-auth-handshake-test PROPERTIES TIMEOUT 60)
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
            ${_hyremote_runtime_src}
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

    # #162 criterion 4: the same binary again at a non-1 device pixel ratio. QT_SCALE_FACTOR is applied by
    # Qt before any window exists, so this needs no display change and stays deterministic in CI. A physical
    # DPR transition remains the runbook's cell; this is the deterministic half that must not be discovered
    # there first.
    add_test(NAME hyremote-widgets-capture-test-dpr150 COMMAND hyremote-widgets-capture-test)
    set_tests_properties(hyremote-widgets-capture-test-dpr150 PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_SCALE_FACTOR=1.5;HYREMOTE_EXPECT_DPR=1.5"
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
            ${_hyremote_runtime_src}
    )
    add_test(NAME hyremote-widgets-input-backpressure-test COMMAND hyremote-widgets-input-backpressure-test)
    set_tests_properties(hyremote-widgets-input-backpressure-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    # #162 criterion 1: receiver semantics (nested geometry, transparent and disabled children, the
    # implicit mouse grab during a drag). The repository had no coverage for any of these.
    add_executable(hyremote-widgets-input-routing-test
        test_widgets_input_routing.cpp
    )
    target_link_libraries(hyremote-widgets-input-routing-test
        PRIVATE
            hyremote-remoteaccess
            HyRemote::Core
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

    # #174: listener-address contract rows that need a real socket. Every port in the test is chosen by the
    # operating system (a probe binds port 0 and reports what it got), so no fixed port is required and the
    # rows cannot collide with a developer's machine.
    if(HYREMOTE_WITH_VNC)
        add_executable(hyremote-listener-address-matrix-test
            test_listener_address_matrix.cpp
        )
        target_link_libraries(hyremote-listener-address-matrix-test
            PRIVATE
                hyremote-remoteaccess
                HyRemote::Core
                Qt6::Widgets
                Qt6::Network
        )
        target_include_directories(hyremote-listener-address-matrix-test
            PRIVATE
                ${_hyremote_runtime_src}
        )
        add_test(NAME hyremote-listener-address-matrix-test COMMAND hyremote-listener-address-matrix-test)
        set_tests_properties(hyremote-listener-address-matrix-test PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
            TIMEOUT 30
        )
    endif()

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
                ${_hyremote_runtime_src}
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
            ${_hyremote_runtime_src}
    )
    add_test(NAME hyremote-quick-capture-test COMMAND hyremote-quick-capture-test)
    set_tests_properties(hyremote-quick-capture-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
    )

    # #162 criterion 4, Quick half: same binary at a non-1 device pixel ratio, with the expected ratio
    # asserted so the coverage cannot be vacuous.
    add_test(NAME hyremote-quick-capture-test-dpr150 COMMAND hyremote-quick-capture-test)
    set_tests_properties(hyremote-quick-capture-test-dpr150 PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_SCALE_FACTOR=1.5;HYREMOTE_EXPECT_DPR=1.5"
    )

    # #162 criterion 2: item-level input routing and focus, not just window-level arrival.
    add_executable(hyremote-quick-input-routing-test
        test_quick_input_routing.cpp
    )
    target_link_libraries(hyremote-quick-input-routing-test
        PRIVATE
            hyremote-remoteaccess
            HyRemote::Core
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
