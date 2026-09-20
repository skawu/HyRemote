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

require_file_token("src/remoteaccess/src/transport/rfb_transport.cpp" "m_keyHolderCounts" "per-key concurrent-viewer holder accounting")
require_file_token("src/remoteaccess/src/transport/rfb_transport.cpp" "m_buttonHolderCounts" "per-button concurrent-viewer holder accounting")
require_file_token("src/remoteaccess/src/transport/rfb_transport.cpp" "aggregateModifiers() const" "aggregate remote modifier state")
require_file_token("src/remoteaccess/tests/test_rfb_multi_client_input.cpp" "testConcurrentViewerHeldStateIsolation" "two-viewer deterministic held-state regression")
require_file_token("src/remoteaccess/tests/test_rfb_multi_client_input.cpp" "countKey(inputs, hyremote::KeyCode::B, true) == 2" "same-viewer key repeat preservation")
require_file_token("src/remoteaccess/tests/test_rfb_multi_client_input.cpp" "countKey(inputs, hyremote::KeyCode::C, false) == 0" "cross-viewer unmatched-release isolation")
require_file_token("src/remoteaccess/tests/CMakeLists.txt" "hyremote-rfb-multi-client-input-test" "registered concurrent-viewer CTest")
require_file_token("src/remoteaccess/tests/CMakeLists.txt" "TIMEOUT 15" "bounded concurrent-viewer CTest runtime")
require_file_token("docs/input-model.md" "reference-counted inside the private transport normalization layer" "canonical concurrent-viewer input model")
require_file_token("docs/known-limitations.md" "simultaneous viewers as contributors to one shared logical Qt input device" "truthful shared-target multi-viewer boundary")

# A viewer-disconnect release is part of the held-state lifecycle, not disposable overload traffic.
# Widgets and Quick share one internal admission state machine: normal input retains the historical
# 64-event budget while a mathematically bounded protected lane preserves releases for holds that the
# adapter previously accepted. Presses rejected by normal backpressure do not earn a protected release.
require_file_token("src/remoteaccess/src/detail/input_mailbox_admission.hpp" "kProtectedReleaseCapacity =" "shared protected-release admission bound")
require_file_token("src/remoteaccess/src/detail/input_mailbox_admission.hpp" "kProtectedReleaseCapacity == 136U" "machine-pinned V1 protected-release capacity")
require_file_token("src/remoteaccess/src/detail/input_mailbox_admission.hpp" "DropUnmatchedRelease" "rejected-press release suppression")
require_file_token("src/remoteaccess/tests/test_input_mailbox_admission.cpp" "testProtectedReserveBoundCoversWorstAcceptedLifecycle" "pure admission worst-case proof")
require_file_token("src/remoteaccess/tests/CMakeLists.txt" "hyremote-input-mailbox-admission-test" "registered pure admission CTest")
require_file_token("src/remoteaccess/src/widgets/widget_target.cpp" "InputMailboxAdmission admission" "Widgets shared protected-release admission")
require_file_token("src/remoteaccess/src/quick/quick_target.cpp" "InputMailboxAdmission admission" "Quick shared protected-release admission")
require_file_token("src/remoteaccess/tests/test_widgets_input_backpressure.cpp" "testProtectedReleaseSurvivesNormalMailboxSaturation" "Widgets saturated-mailbox release regression")
require_file_token("src/remoteaccess/tests/test_quick_input_backpressure.cpp" "testProtectedReleaseSurvivesNormalMailboxSaturation" "Quick saturated-mailbox release parity regression")
require_file_token("src/remoteaccess/tests/test_rfb_widget_disconnect_backpressure.cpp" "testDisconnectCleanupCrossesSaturatedAdapterMailbox" "RFB -> Session -> Widgets saturation/disconnect composition regression")
require_file_token("src/remoteaccess/tests/test_rfb_widget_disconnect_backpressure.cpp" "afterDisconnect.inputPostFailures == saturated.inputPostFailures" "disconnect release crosses Session without additional sink post failure")
require_file_token("src/remoteaccess/tests/CMakeLists.txt" "hyremote-rfb-widget-disconnect-backpressure-test" "registered real disconnect/backpressure composition CTest")
require_file_token("docs/input-model.md" "bounded protected-release lane" "canonical backpressure/disconnect composition rule")
require_file_token("docs/input-model.md" "normal backpressure may not prevent the final accepted key/button release" "canonical #90 saturation invariant")

require_file_token("src/remoteaccess/src/remote_access.cpp" "acknowledgedRecoverableError" "recoverable runtime error acknowledgement")
require_file_token("src/remoteaccess/src/remote_access.cpp" "acknowledgeCurrentRecoverableError()" "clearError live-runtime acknowledgement path")
require_file_token("src/remoteaccess/tests/test_remote_access.cpp" "testRecoverableRuntimeErrorCanBeAcknowledgedAndReappearsOnNewFailure" "recoverable clear/reoccurrence regression")
require_file_token("src/remoteaccess/tests/test_remote_access_error_ack.cpp" "testAcknowledgedRecoverableErrorSurvivesUnrelatedTransportEvents" "recoverable acknowledgement ignores unrelated transport activity")
require_file_token("src/remoteaccess/tests/CMakeLists.txt" "hyremote-remoteaccess-error-ack-test" "registered recoverable occurrence-isolation CTest")
require_file_token("src/remoteaccess/tests/test_remote_access.cpp" "testFaultedRuntimeRequiresExplicitStopAndKeepsFatalDiagnostic" "Faulted explicit-stop recovery regression")
require_file_token("src/remoteaccess/include/HyRemote/RemoteAccess.h" "A non-recoverable runtime failure is observable as" "installed-header Faulted lifecycle contract")
require_file_token("src/remoteaccess/include/HyRemote/RemoteAccess.h" "Acknowledge/clear product-level and live recoverable diagnostics" "installed-header clearError contract")
require_file_token("docs/v1-api-stability.md" "a non-recoverable runtime failure remains observable as `Faulted` until the owner explicitly calls `stop()`" "V1 Faulted API freeze")
require_file_token("docs/v1-api-stability.md" "unrelated viewer/capture/transport activity does not resurrect the same acknowledged occurrence" "V1 clearError occurrence isolation")
require_file_token("docs/v1-api-stability.md" "a later occurrence must become visible again" "V1 clearError recurrence semantics")

# Target lifetime must remain one facade/runtime state machine. Concrete adapters report TargetLost,
# the public facade remains Faulted until explicit stop, and QML only observes the same facade target
# weakly so a destroyed QObject becomes null and emits the declared targetChanged notification.
require_file_token("src/remoteaccess/tests/test_remote_access_target_loss.cpp" "testTargetLossFaultStopReplaceRestart" "facade target-loss stop/replace/restart regression")
require_file_token("src/remoteaccess/tests/test_remote_access_target_loss.cpp" "remote.connectedClientCount() == 1" "Faulted runtime retains viewer diagnostic until stop")
require_file_token("src/remoteaccess/tests/CMakeLists.txt" "hyremote-remoteaccess-target-loss-test" "registered facade target-loss CTest")
require_file_token("src/remoteaccess/tests/test_widgets_capture.cpp" "testDestroyedTargetReportsTargetLost" "Widgets target-loss regression")
require_file_token("src/remoteaccess/tests/test_quick_capture.cpp" "testDestroyedQuickTargetReportsTargetLost" "Quick target-loss parity regression")
require_file_token("src/remoteaccess/src/widgets/widget_target.cpp" "the QWidget target was destroyed" "Widgets target-loss publication")
require_file_token("src/remoteaccess/src/quick/quick_target.cpp" "the QQuickWindow target was destroyed" "Quick target-loss publication")
require_file_token("integrations/qml/HyRemote/QmlRemoteAccess.cpp" "m_targetDestroyedConnection" "QML target lifetime observation")
require_file_token("integrations/qml/HyRemote/QmlRemoteAccess.cpp" "emit targetChanged();" "QML destroyed target notification")
require_file_token("integrations/qml/HyRemote/tests/test_qml_module.cpp" "testTargetDestructionNotifiesDeclarativeProperty" "QML target destruction regression")
require_file_token("integrations/qml/HyRemote/tests/CMakeLists.txt" "hyremote-qml-module-test" "registered QML module lifecycle CTest")

# E1/E2 use the same public C++ facade lifecycle required by the physical gate. Product-fit begins
# from view-only in one application process; the first viewer disconnect triggers stop -> configure
# -> start, and control/reconnect continues without relaunching the target application. The timer is
# only a bounded watchdog so acceptance is tied to an observed viewer lifecycle rather than a race.
foreach(example_source IN ITEMS
        "examples/widgets-basic/main.cpp"
        "examples/quick-basic/main.cpp")
    require_file_token("${example_source}" "policy-transition-ms" "E1/E2 stopped-runtime acceptance helper")
    require_file_token("${example_source}" "remote.stop();" "E1/E2 public facade stop during policy transition")
    require_file_token("${example_source}" "remote.setRemoteInputEnabled(true)" "E1/E2 stopped remote-input configuration")
    require_file_token("${example_source}" "POLICY_RESTART_REQUESTED" "E1/E2 public facade restart evidence")
endforeach()
require_file_token("tests/product-e2e/example_product_fit.py" "\"--policy-transition-ms\", \"15000\"" "E1/E2 bounded transition watchdog")
require_file_token("tests/product-e2e/example_product_fit.py" "The first disconnect is the deterministic trigger" "E1/E2 lifecycle-driven policy transition")
require_file_token("tests/product-e2e/example_product_fit.py" "public stop/configure/start" "E1/E2 same-process acceptance result")

# E3 must exercise the same stopped-runtime policy transition without a wall-clock race. The first
# viewer disconnect is observable through the public QML connected-client diagnostic and triggers
# stop -> configure -> start; a bounded timer remains only as a watchdog fallback.
require_file_token("examples/qml-basic/Main.qml" "property bool acceptanceSawViewer" "E3 first-viewer lifecycle observation")
require_file_token("examples/qml-basic/Main.qml" "function applyAcceptancePolicyTransition()" "E3 public-QML policy transition helper")
require_file_token("examples/qml-basic/Main.qml" "window.applyAcceptancePolicyTransition()" "E3 disconnect-driven policy transition")
require_file_token("examples/qml-basic/Main.qml" "policyTransitionTimer.stop()" "E3 watchdog cancellation after lifecycle trigger")
require_file_token("tests/product-e2e/qml_product_fit.py" "\"--policy-transition-ms\", \"15000\"" "E3 bounded policy-transition watchdog")
require_file_token("tests/product-e2e/qml_product_fit.py" "The disconnect above is also the deterministic policy-transition trigger" "E3 product-fit binds policy transition to viewer lifecycle")

require_file_token("integrations/qpa/tests/qpa_remote_failure_native_survival_smoke.cpp" "class PortReservation final" "deterministic occupied-port QPA failure setup")
require_file_token("integrations/qpa/tests/qpa_remote_failure_native_survival_smoke.cpp" "SO_EXCLUSIVEADDRUSE" "deterministic Windows occupied-port ownership")
require_file_token("integrations/qpa/tests/qpa_remote_failure_native_survival_smoke.cpp" "gRemoteStartFailureSeen" "proof that the QPA remote runtime actually attempted and failed startup")
require_file_token("integrations/qpa/tests/qpa_remote_failure_native_survival_smoke.cpp" "forced remote bind failure was observed and the native QPA application remained live" "native-survival assertion after observed remote bind failure")
require_file_token("integrations/qpa/tests/CMakeLists.txt" "hyremote-qpa-remote-failure-native-survival-smoke" "registered QPA native-survival CTest")
require_file_token("integrations/qpa/tests/CMakeLists.txt" "PRIVATE ws2_32" "Windows native socket support for QPA failure smoke")
require_file_token("integrations/qpa/hyremote_qpa_remote_controller.cpp" "HyRemote QPA Proxy could not start the composite RemoteAccess runtime" "QPA remote-start failure remains diagnostic rather than native-fatal")
require_file_token("docs/internal/v1-ga-acceptance.md" "remote-capability failure only" "canonical GA native-survival rule for QPA remote startup failure")

message(STATUS
    "HyRemote V1 runtime behavior contract gate: PASS "
    "(multi-viewer isolation + protected disconnect releases under backpressure + target-loss/QML lifetime parity + error/Faulted semantics + Widgets/Quick parity + same-process E1/E2/E3 policy lifecycle + observed QPA remote-failure native survival)")
