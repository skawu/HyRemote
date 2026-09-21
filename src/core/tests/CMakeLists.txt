# hyremote-core tests: host-runnable, no GUI/GPU dependency.
#
# Each test file is one executable and one CTest case. `support/` holds the tiny assertion helper
# and the deterministic fakes; `../src` is on the include path so the tests can exercise the
# internal bounded mailbox directly (C4).

function(hyremote_add_core_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE hyremote-core)
    target_include_directories(${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/support
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )
    add_test(NAME ${name} COMMAND ${name})
endfunction()

hyremote_add_core_test(hyremote-core-test-frame-lifetime test_frame_lifetime.cpp)
hyremote_add_core_test(hyremote-core-test-damage test_damage.cpp)
hyremote_add_core_test(hyremote-core-test-timing test_timing.cpp)
hyremote_add_core_test(hyremote-core-test-mailbox test_mailbox.cpp)
hyremote_add_core_test(hyremote-core-test-transport-handoff test_transport_handoff.cpp)
hyremote_add_core_test(hyremote-core-test-session-lifecycle test_session_lifecycle.cpp)
hyremote_add_core_test(hyremote-core-test-session-defaults test_session_defaults.cpp)
hyremote_add_core_test(hyremote-core-test-input-routing test_input_routing.cpp)
hyremote_add_core_test(hyremote-core-test-input-normalization test_input_normalization.cpp)
hyremote_add_core_test(hyremote-core-test-callback-lifetime test_callback_lifetime.cpp)
hyremote_add_core_test(hyremote-core-test-callback-exception-boundary test_callback_exception_boundary.cpp)

# The dependency rule of ADR-0001 is testable: no Core source may include or declare a Qt GUI/private,
# protocol or platform dependency, and the Core target may not link a Qt target.
add_test(NAME hyremote-core-test-dependency-boundary
         COMMAND ${CMAKE_COMMAND}
                 -DCORE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/..
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/check_dependencies.cmake)
