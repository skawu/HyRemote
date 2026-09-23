# The #174 listener binding contract: mode inference, the four interface resolutions and the locality rule, decided
# from an injected interface snapshot so none of it depends on the host or on timing.
add_executable(hyremote-runtime-listener-binding-test
    test_listener_binding.cpp
)
target_compile_features(hyremote-runtime-listener-binding-test PRIVATE cxx_std_17)
target_include_directories(hyremote-runtime-listener-binding-test PRIVATE ../src)
target_link_libraries(hyremote-runtime-listener-binding-test PRIVATE Qt6::Core HyRemote::RemoteAccess)
add_test(NAME hyremote-runtime-listener-binding-test
         COMMAND hyremote-runtime-listener-binding-test)
set_tests_properties(hyremote-runtime-listener-binding-test PROPERTIES TIMEOUT 15)

# Application-level automatic access controller tests plus the private notification seam. These were
# only ever reachable because the whole tests directory used to be entered behind
# HYREMOTE_AUTOMATIC_RUNTIME_AVAILABLE, so they keep that same effective guard here: freeing the
# Runtime-owned regressions below must not silently change what the runtime-only and frontend-less
# configurations register.
if(HYREMOTE_AUTOMATIC_RUNTIME_AVAILABLE)
    add_executable(hyremote-runtime-automatic-surface-model-test
        automatic_application_surface_model_test.cpp
        ../src/automatic/application_surface_model.cpp
    )
    target_compile_features(hyremote-runtime-automatic-surface-model-test PRIVATE cxx_std_17)
    target_include_directories(hyremote-runtime-automatic-surface-model-test PRIVATE ../src)
    target_link_libraries(hyremote-runtime-automatic-surface-model-test PRIVATE Qt6::Core)
    add_test(NAME hyremote-runtime-automatic-surface-model-test
             COMMAND hyremote-runtime-automatic-surface-model-test)
    set_tests_properties(hyremote-runtime-automatic-surface-model-test PROPERTIES TIMEOUT 15)

    add_executable(hyremote-runtime-automatic-composite-capture-test
        automatic_composite_capture_test.cpp
        ../src/automatic/application_surface_model.cpp
        ../src/automatic/composite_target.cpp
    )
    target_compile_features(hyremote-runtime-automatic-composite-capture-test PRIVATE cxx_std_17)
    target_include_directories(hyremote-runtime-automatic-composite-capture-test PRIVATE ../src)
    target_link_libraries(hyremote-runtime-automatic-composite-capture-test
        PRIVATE Qt6::Core Qt6::Gui HyRemote::Core HyRemote::RemoteAccess
    )
    add_test(NAME hyremote-runtime-automatic-composite-capture-test
             COMMAND hyremote-runtime-automatic-composite-capture-test)
    set_tests_properties(hyremote-runtime-automatic-composite-capture-test PROPERTIES TIMEOUT 15)

    add_executable(hyremote-runtime-automatic-composite-input-test
        automatic_composite_input_test.cpp
        ../src/automatic/application_surface_model.cpp
        ../src/automatic/composite_target.cpp
        ../src/automatic/composite_target_routing.cpp
        ../src/automatic/interactive_composite_target.cpp
    )
    target_compile_features(hyremote-runtime-automatic-composite-input-test PRIVATE cxx_std_17)
    target_include_directories(hyremote-runtime-automatic-composite-input-test PRIVATE ../src)
    target_link_libraries(hyremote-runtime-automatic-composite-input-test
        PRIVATE Qt6::Core Qt6::Gui HyRemote::Core HyRemote::RemoteAccess
    )
    add_test(NAME hyremote-runtime-automatic-composite-input-test
             COMMAND hyremote-runtime-automatic-composite-input-test)
    set_tests_properties(hyremote-runtime-automatic-composite-input-test PROPERTIES TIMEOUT 15)

    # #259: the private typed notification seam of the shared runtime. It drives real event boundaries
    # through the private test factories, so a notification is proven by the event that produced it rather
    # than by a timer or a wall-clock wait.
    add_executable(hyremote-runtime-notifications-test
        access_instance_notifications_test.cpp
    )
    target_compile_features(hyremote-runtime-notifications-test PRIVATE cxx_std_17)
    target_include_directories(hyremote-runtime-notifications-test PRIVATE ../src)
    target_link_libraries(hyremote-runtime-notifications-test
        PRIVATE Qt6::Core HyRemote::Core HyRemote::RemoteAccess
    )
    add_test(NAME hyremote-runtime-notifications-test
             COMMAND hyremote-runtime-notifications-test)
    set_tests_properties(hyremote-runtime-notifications-test PROPERTIES TIMEOUT 60)
endif()

# Runtime-owned regressions. These exercise Shared Runtime internals through the C++ test-exports the
# runtime target already exposes to the build tree, so they register whenever tests are enabled -
# not only when the C++ frontend or the application-level automatic controller is built.
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
        ../src
)
add_test(NAME hyremote-security-descriptor-test COMMAND hyremote-security-descriptor-test)
set_tests_properties(hyremote-security-descriptor-test PROPERTIES TIMEOUT 15)

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
        ../src
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
        ../src
)
add_test(NAME hyremote-input-mailbox-admission-test COMMAND hyremote-input-mailbox-admission-test)
set_tests_properties(hyremote-input-mailbox-admission-test PROPERTIES TIMEOUT 15)

if(HYREMOTE_WITH_VNC)
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
            ../src
    )
    add_test(NAME hyremote-rfb-multi-client-input-test COMMAND hyremote-rfb-multi-client-input-test)
    set_tests_properties(hyremote-rfb-multi-client-input-test PROPERTIES TIMEOUT 15)
endif()

# Private VNC Authentication contracts. They need the real capability pair - the RFB transport has to
# exist and the transport-security implementation has to be built - because they drive the actual wire
# exchange. Linking the Shared Runtime target is component identity, not a C++ frontend dependency.
if(HYREMOTE_WITH_VNC AND HYREMOTE_TRANSPORT_SECURITY_AVAILABLE)
    add_executable(hyremote-vnc-auth-test
        test_vnc_auth.cpp
        ../src/transport/vnc_auth.cpp
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
            ../src
    )
    add_test(NAME hyremote-vnc-auth-test COMMAND hyremote-vnc-auth-test)
    set_tests_properties(hyremote-vnc-auth-test PROPERTIES TIMEOUT 15)

    add_executable(hyremote-rfb-vnc-auth-handshake-test
        test_rfb_vnc_auth_handshake.cpp
        ../src/transport/vnc_auth.cpp
    )
    target_link_libraries(hyremote-rfb-vnc-auth-handshake-test
        PRIVATE
            hyremote-remoteaccess
            OpenSSL::Crypto
            # This test includes hyremote/core/transport.hpp, so it needs the Core interface the other
            # Runtime tests already link. Without it the target cannot compile in its own configuration.
            HyRemote::Core
            Qt6::Core
            Qt6::Network
    )
    target_include_directories(hyremote-rfb-vnc-auth-handshake-test
        PRIVATE
            ../src
    )
    add_test(NAME hyremote-rfb-vnc-auth-handshake-test
             COMMAND hyremote-rfb-vnc-auth-handshake-test)
    set_tests_properties(hyremote-rfb-vnc-auth-handshake-test PROPERTIES TIMEOUT 15)
endif()

# Widgets target-adapter contracts belong to the Shared Runtime. Their registration follows the
# adapter capability itself and is independent of whether the Embedded C++ facade is built.
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
    target_include_directories(hyremote-widgets-capture-test PRIVATE ../src)
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
    target_include_directories(hyremote-widgets-input-backpressure-test PRIVATE ../src)
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
    target_include_directories(hyremote-widgets-input-routing-test PRIVATE ../src)
    add_test(NAME hyremote-widgets-input-routing-test COMMAND hyremote-widgets-input-routing-test)
    set_tests_properties(hyremote-widgets-input-routing-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    # TG-020: this composition test drives the real RFB transport as well as the Widgets adapter.
    # When VNC is unavailable it must be absent from the test graph rather than left unbuildable or skipped.
    if(HYREMOTE_WITH_VNC)
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
        target_include_directories(hyremote-rfb-widget-disconnect-backpressure-test PRIVATE ../src)
        add_test(NAME hyremote-rfb-widget-disconnect-backpressure-test
                 COMMAND hyremote-rfb-widget-disconnect-backpressure-test)
        set_tests_properties(hyremote-rfb-widget-disconnect-backpressure-test PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        )
    endif()
endif()

# Qt Quick target-adapter contracts are likewise Runtime-owned and follow only the Quick adapter capability.
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
    target_include_directories(hyremote-quick-capture-test PRIVATE ../src)
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
    target_include_directories(hyremote-quick-input-routing-test PRIVATE ../src)
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
    target_include_directories(hyremote-quick-input-backpressure-test PRIVATE ../src)
    add_test(NAME hyremote-quick-input-backpressure-test COMMAND hyremote-quick-input-backpressure-test)
    set_tests_properties(hyremote-quick-input-backpressure-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
    )
endif()

# Real RFB listener reachability/reconciliation is a Runtime integration contract. It requires the
# Widgets target adapter used by the fixture and the VNC/RFB backend, but not the Embedded C++ facade.
if(HYREMOTE_REMOTEACCESS_WITH_WIDGETS AND HYREMOTE_WITH_VNC)
    add_executable(hyremote-rfb-listener-reachability-test
        test_rfb_listener_reachability.cpp
    )
    target_link_libraries(hyremote-rfb-listener-reachability-test
        PRIVATE
            hyremote-remoteaccess
            HyRemote::Core
            Qt6::Core
            Qt6::Network
            Qt6::Widgets
    )
    target_include_directories(hyremote-rfb-listener-reachability-test PRIVATE ../src)
    add_test(NAME hyremote-rfb-listener-reachability-test
             COMMAND hyremote-rfb-listener-reachability-test)
    set_tests_properties(hyremote-rfb-listener-reachability-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()
