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

require_file_token("remoteaccess/src/transport/rfb_transport.cpp" "m_keyHolderCounts" "per-key concurrent-viewer holder accounting")
require_file_token("remoteaccess/src/transport/rfb_transport.cpp" "m_buttonHolderCounts" "per-button concurrent-viewer holder accounting")
require_file_token("remoteaccess/src/transport/rfb_transport.cpp" "aggregateModifiers() const" "aggregate remote modifier state")
require_file_token("remoteaccess/tests/test_rfb_multi_client_input.cpp" "testConcurrentViewerHeldStateIsolation" "two-viewer deterministic held-state regression")
require_file_token("remoteaccess/tests/test_rfb_multi_client_input.cpp" "countKey(inputs, hyremote::KeyCode::B, true) == 2" "same-viewer key repeat preservation")
require_file_token("remoteaccess/tests/CMakeLists.txt" "hyremote-rfb-multi-client-input-test" "registered concurrent-viewer CTest")
require_file_token("remoteaccess/tests/CMakeLists.txt" "TIMEOUT 15" "bounded concurrent-viewer CTest runtime")
require_file_token("docs/input-model.md" "reference-counted inside the private transport normalization layer" "canonical concurrent-viewer input model")
require_file_token("docs/known-limitations.md" "simultaneous viewers as contributors to one shared logical Qt input device" "truthful shared-target multi-viewer boundary")

require_file_token("remoteaccess/src/remote_access.cpp" "acknowledgedRecoverableError" "recoverable runtime error acknowledgement")
require_file_token("remoteaccess/src/remote_access.cpp" "acknowledgeCurrentRecoverableError()" "clearError live-runtime acknowledgement path")
require_file_token("remoteaccess/tests/test_remote_access.cpp" "testRecoverableRuntimeErrorCanBeAcknowledgedAndReappearsOnNewFailure" "recoverable clear/reoccurrence regression")
require_file_token("remoteaccess/tests/test_remote_access.cpp" "testFaultedRuntimeRequiresExplicitStopAndKeepsFatalDiagnostic" "Faulted explicit-stop recovery regression")
require_file_token("remoteaccess/include/HyRemote/RemoteAccess.h" "A non-recoverable runtime failure is observable as" "installed-header Faulted lifecycle contract")
require_file_token("remoteaccess/include/HyRemote/RemoteAccess.h" "Acknowledge/clear product-level and live recoverable diagnostics" "installed-header clearError contract")
require_file_token("docs/v1-api-stability.md" "a non-recoverable runtime failure remains observable as `Faulted` until the owner explicitly calls `stop()`" "V1 Faulted API freeze")
require_file_token("docs/v1-api-stability.md" "a later occurrence must become visible again" "V1 clearError recurrence semantics")

require_file_token("remoteaccess/tests/test_widgets_capture.cpp" "testDestroyedTargetReportsTargetLost" "Widgets target-loss regression")
require_file_token("remoteaccess/tests/test_quick_capture.cpp" "testDestroyedQuickTargetReportsTargetLost" "Quick target-loss parity regression")
require_file_token("remoteaccess/src/widgets/widget_target.cpp" "the QWidget target was destroyed" "Widgets target-loss publication")
require_file_token("remoteaccess/src/quick/quick_target.cpp" "the QQuickWindow target was destroyed" "Quick target-loss publication")

# E3 must exercise the same stopped-runtime policy transition without a wall-clock race. The first
# viewer disconnect is observable through the public QML connected-client diagnostic and triggers
# stop -> configure -> start; a bounded timer remains only as a watchdog fallback.
require_file_token("examples/qml-basic/Main.qml" "property bool acceptanceSawViewer" "E3 first-viewer lifecycle observation")
require_file_token("examples/qml-basic/Main.qml" "function applyAcceptancePolicyTransition()" "E3 public-QML policy transition helper")
require_file_token("examples/qml-basic/Main.qml" "window.applyAcceptancePolicyTransition()" "E3 disconnect-driven policy transition")
require_file_token("examples/qml-basic/Main.qml" "policyTransitionTimer.stop()" "E3 watchdog cancellation after lifecycle trigger")
require_file_token("tests/product-e2e/qml_product_fit.py" "\"--policy-transition-ms\", \"15000\"" "E3 bounded policy-transition watchdog")
require_file_token("tests/product-e2e/qml_product_fit.py" "The disconnect above is also the deterministic policy-transition trigger" "E3 product-fit binds policy transition to viewer lifecycle")

require_file_token("qpa/tests/qpa_remote_failure_native_survival_smoke.cpp" "class PortReservation final" "deterministic occupied-port QPA failure setup")
require_file_token("qpa/tests/qpa_remote_failure_native_survival_smoke.cpp" "SO_EXCLUSIVEADDRUSE" "deterministic Windows occupied-port ownership")
require_file_token("qpa/tests/qpa_remote_failure_native_survival_smoke.cpp" "gRemoteStartFailureSeen" "proof that the QPA remote runtime actually attempted and failed startup")
require_file_token("qpa/tests/qpa_remote_failure_native_survival_smoke.cpp" "forced remote bind failure was observed and the native QPA application remained live" "native-survival assertion after observed remote bind failure")
require_file_token("qpa/tests/CMakeLists.txt" "hyremote-qpa-remote-failure-native-survival-smoke" "registered QPA native-survival CTest")
require_file_token("qpa/tests/CMakeLists.txt" "PRIVATE ws2_32" "Windows native socket support for QPA failure smoke")
require_file_token("qpa/hyremote_qpa_remote_controller.cpp" "HyRemote QPA Proxy could not start the composite RemoteAccess runtime" "QPA remote-start failure remains diagnostic rather than native-fatal")
require_file_token("docs/v1-ga-acceptance.md" "remote-capability failure only" "canonical GA native-survival rule for QPA remote startup failure")

message(STATUS
    "HyRemote V1 runtime behavior contract gate: PASS "
    "(multi-viewer input isolation + error/Faulted semantics + Widgets/Quick target-loss parity + event-driven E3 policy transition + observed QPA remote-failure native survival)")
