cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

function(require_file_token relative_path token description)
    set(path "${HYREMOTE_SOURCE_DIR}/${relative_path}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "release-readiness runtime contract: missing ${relative_path}")
    endif()
    file(READ "${path}" text)
    string(FIND "${text}" "${token}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "release-readiness runtime contract: ${description} missing from ${relative_path}: ${token}")
    endif()
endfunction()

# Shared-target simultaneous-viewer input correctness stays private to the bounded RFB transport.
# The public application contract must not gain a per-client ownership/authorization API simply to
# solve protocol bookkeeping.
require_file_token(
    "remoteaccess/src/transport/rfb_transport.cpp"
    "m_keyHolderCounts"
    "per-key concurrent-viewer holder accounting")
require_file_token(
    "remoteaccess/src/transport/rfb_transport.cpp"
    "m_buttonHolderCounts"
    "per-button concurrent-viewer holder accounting")
require_file_token(
    "remoteaccess/src/transport/rfb_transport.cpp"
    "aggregateModifiers() const"
    "aggregate remote modifier state")
require_file_token(
    "remoteaccess/tests/test_rfb_multi_client_input.cpp"
    "testConcurrentViewerHeldStateIsolation"
    "two-viewer deterministic held-state regression")
require_file_token(
    "remoteaccess/tests/test_rfb_multi_client_input.cpp"
    "countKey(inputs, hyremote::KeyCode::B, true) == 2"
    "same-viewer key repeat preservation")
require_file_token(
    "remoteaccess/tests/CMakeLists.txt"
    "hyremote-rfb-multi-client-input-test"
    "registered concurrent-viewer CTest")
require_file_token(
    "remoteaccess/tests/CMakeLists.txt"
    "TIMEOUT 15"
    "bounded concurrent-viewer CTest runtime")
require_file_token(
    "docs/input-model.md"
    "reference-counted inside the private transport normalization layer"
    "canonical concurrent-viewer input model")
require_file_token(
    "docs/known-limitations.md"
    "simultaneous viewers as contributors to one shared logical Qt input device"
    "truthful shared-target multi-viewer boundary")

# The already-frozen product clearError() API must actually acknowledge live recoverable runtime
# diagnostics, while an active non-recoverable fault must remain visible until explicit stop().
require_file_token(
    "remoteaccess/src/remote_access.cpp"
    "acknowledgedRecoverableError"
    "recoverable runtime error acknowledgement")
require_file_token(
    "remoteaccess/src/remote_access.cpp"
    "acknowledgeCurrentRecoverableError()"
    "clearError live-runtime acknowledgement path")
require_file_token(
    "remoteaccess/tests/test_remote_access.cpp"
    "testRecoverableRuntimeErrorCanBeAcknowledgedAndReappearsOnNewFailure"
    "recoverable clear/reoccurrence regression")
require_file_token(
    "remoteaccess/tests/test_remote_access.cpp"
    "testFaultedRuntimeRequiresExplicitStopAndKeepsFatalDiagnostic"
    "Faulted explicit-stop recovery regression")
require_file_token(
    "remoteaccess/include/HyRemote/RemoteAccess.h"
    "A non-recoverable runtime failure is observable as"
    "installed-header Faulted lifecycle contract")
require_file_token(
    "remoteaccess/include/HyRemote/RemoteAccess.h"
    "Acknowledge/clear product-level and live recoverable diagnostics"
    "installed-header clearError contract")
require_file_token(
    "docs/v1-api-stability.md"
    "a non-recoverable runtime failure remains observable as `Faulted` until the owner explicitly calls `stop()`"
    "V1 Faulted API freeze")
require_file_token(
    "docs/v1-api-stability.md"
    "a later occurrence must become visible again"
    "V1 clearError recurrence semantics")

# Widgets and Quick must remain peers when their attached target disappears during an active capture
# path. A target loss is non-recoverable and flows through Core to Faulted rather than being silently
# treated as a recoverable hidden-window condition.
require_file_token(
    "remoteaccess/tests/test_widgets_capture.cpp"
    "testDestroyedTargetReportsTargetLost"
    "Widgets target-loss regression")
require_file_token(
    "remoteaccess/tests/test_quick_capture.cpp"
    "testDestroyedQuickTargetReportsTargetLost"
    "Quick target-loss parity regression")
require_file_token(
    "remoteaccess/src/widgets/widget_target.cpp"
    "the QWidget target was destroyed"
    "Widgets target-loss publication")
require_file_token(
    "remoteaccess/src/quick/quick_target.cpp"
    "the QQuickWindow target was destroyed"
    "Quick target-loss publication")

message(STATUS
    "HyRemote V1 runtime behavior contract gate: PASS "
    "(multi-viewer input isolation + error/Faulted semantics + Widgets/Quick target-loss parity)")
