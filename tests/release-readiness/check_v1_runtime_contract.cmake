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

# Concrete transport and target adapters are Common Runtime implementation, not Embedded C++ frontend code.
require_file_token("src/runtime/src/transport/rfb_transport.cpp" "m_keyHolderCounts" "per-key concurrent-viewer holder accounting")
require_file_token("src/runtime/src/transport/rfb_transport.cpp" "m_buttonHolderCounts" "per-button concurrent-viewer holder accounting")
require_file_token("src/runtime/src/transport/rfb_transport.cpp" "aggregateModifiers() const" "aggregate remote modifier state")
require_file_token("src/runtime/tests/test_rfb_multi_client_input.cpp" "testConcurrentViewerHeldStateIsolation" "two-viewer deterministic held-state regression")
require_file_token("src/runtime/tests/test_rfb_multi_client_input.cpp" "countKey(inputs, hyremote::KeyCode::B, true) == 2" "same-viewer key repeat preservation")
require_file_token("src/runtime/tests/test_rfb_multi_client_input.cpp" "countKey(inputs, hyremote::KeyCode::C, false) == 0" "cross-viewer unmatched-release isolation")
require_file_token("src/runtime/tests/CMakeLists.txt" "hyremote-rfb-multi-client-input-test" "registered concurrent-viewer CTest")
require_file_token("src/runtime/tests/CMakeLists.txt" "TIMEOUT 15" "bounded concurrent-viewer CTest runtime")
# Pin the behaviour the document states rather than one historical sentence that used to state it: the document is a
# product contract and rewording it is legitimate, while losing these three semantics is not.
require_file_token("docs/input-model.md" "Per-viewer protocol state remains separate" "canonical concurrent-viewer input model")
require_file_token("docs/input-model.md" "the target release occurs when the final holder releases/disconnects" "final-holder release semantics")
require_file_token("docs/input-model.md" "A release is protected only when its corresponding press was accepted" "protected-release admission semantics")
require_file_token("docs/known-limitations.md" "multiple viewers contribute to one logical application input device" "truthful shared-target multi-viewer boundary")
require_file_token("docs/input-model.md" "distinguish ordinary bounded input from release delivery" "protected-release versus ordinary bounded input")
require_file_token("docs/input-model.md" "If a press was rejected by backpressure" "rejected-press release suppression")
require_file_token("docs/v1-api-stability.md" "stays observable as `Faulted` until the owner calls `stop()`" "non-recoverable failure stays Faulted until stop")
require_file_token("docs/v1-api-stability.md" "acknowledging an error does not resurrect the session that produced it" "acknowledgment does not resurrect the failed session")
require_file_token("docs/v1-api-stability.md" "the same condition occurs again after it was acknowledged" "a later occurrence becomes visible again")

require_file_token("src/runtime/src/detail/input_mailbox_admission.hpp" "kProtectedReleaseCapacity =" "shared protected-release admission bound")
require_file_token("src/runtime/src/detail/input_mailbox_admission.hpp" "kProtectedReleaseCapacity == 136U" "machine-pinned V1 protected-release capacity")
require_file_token("src/runtime/src/detail/input_mailbox_admission.hpp" "DropUnmatchedRelease" "rejected-press release suppression")
require_file_token("src/runtime/tests/test_input_mailbox_admission.cpp" "testProtectedReserveBoundCoversWorstAcceptedLifecycle" "pure admission worst-case proof")
require_file_token("src/runtime/tests/CMakeLists.txt" "hyremote-input-mailbox-admission-test" "registered pure admission CTest")
require_file_token("src/runtime/src/widgets/widget_target.cpp" "InputMailboxAdmission admission" "Widgets shared protected-release admission")
require_file_token("src/runtime/src/quick/quick_target.cpp" "InputMailboxAdmission admission" "Quick shared protected-release admission")
require_file_token("src/integrations/cpp/tests/test_widgets_input_backpressure.cpp" "testProtectedReleaseSurvivesNormalMailboxSaturation" "Widgets saturated-mailbox release regression")
require_file_token("src/integrations/cpp/tests/test_quick_input_backpressure.cpp" "testProtectedReleaseSurvivesNormalMailboxSaturation" "Quick saturated-mailbox release parity regression")
require_file_token("src/integrations/cpp/tests/test_rfb_widget_disconnect_backpressure.cpp" "testDisconnectCleanupCrossesSaturatedAdapterMailbox" "RFB -> Session -> Widgets saturation/disconnect composition regression")
require_file_token("src/integrations/cpp/tests/test_rfb_widget_disconnect_backpressure.cpp" "afterDisconnect.inputPostFailures == saturated.inputPostFailures" "disconnect release crosses Session without additional sink post failure")
require_file_token("src/integrations/cpp/tests/CMakeLists.txt" "hyremote-rfb-widget-disconnect-backpressure-test" "registered real disconnect/backpressure composition CTest")
require_file_token("docs/input-model.md" "must not create an unbounded queue" "canonical bounded-delivery rule")
require_file_token("docs/input-model.md" "This keeps overload behavior bounded while preserving input neutrality" "canonical saturation invariant")

# Runtime state/error semantics moved into the private AccessInstance; the C++ facade is now only a type/API mapping.
require_file_token("src/runtime/src/access_instance.cpp" "acknowledgedRecoverableError" "recoverable runtime error acknowledgement")
require_file_token("src/runtime/src/access_instance.cpp" "acknowledgeCurrentRecoverableError()" "clearError live-runtime acknowledgement path")
require_file_token("src/integrations/cpp/tests/test_remote_access.cpp" "testRecoverableRuntimeErrorCanBeAcknowledgedAndReappearsOnNewFailure" "recoverable clear/reoccurrence regression")
require_file_token("src/integrations/cpp/tests/test_remote_access_error_ack.cpp" "testAcknowledgedRecoverableErrorSurvivesUnrelatedTransportEvents" "recoverable acknowledgement ignores unrelated transport activity")
require_file_token("src/integrations/cpp/tests/CMakeLists.txt" "hyremote-remoteaccess-error-ack-test" "registered recoverable occurrence-isolation CTest")
require_file_token("src/integrations/cpp/tests/test_remote_access.cpp" "testFaultedRuntimeRequiresExplicitStopAndKeepsFatalDiagnostic" "Faulted explicit-stop recovery regression")
require_file_token("src/integrations/cpp/include/HyRemote/RemoteAccess.h" "A non-recoverable runtime failure is observable as" "installed-header Faulted lifecycle contract")
require_file_token("src/integrations/cpp/include/HyRemote/RemoteAccess.h" "Acknowledge/clear product-level and live recoverable diagnostics" "installed-header clearError contract")
require_file_token("docs/v1-api-stability.md" "a non-recoverable runtime failure stays observable as `Faulted` until the owner calls `stop()`" "V1 Faulted API freeze")
require_file_token("docs/v1-api-stability.md" "unrelated viewer, capture or transport activity must not make the same acknowledged failure current again" "V1 clearError occurrence isolation")
require_file_token("docs/v1-api-stability.md" "that later occurrence must become visible again" "V1 clearError recurrence semantics")

require_file_token("src/integrations/cpp/tests/test_remote_access_target_loss.cpp" "testTargetLossFaultStopReplaceRestart" "facade target-loss stop/replace/restart regression")
require_file_token("src/integrations/cpp/tests/test_remote_access_target_loss.cpp" "remote.connectedClientCount() == 1" "Faulted runtime retains viewer diagnostic until stop")
require_file_token("src/integrations/cpp/tests/CMakeLists.txt" "hyremote-remoteaccess-target-loss-test" "registered facade target-loss CTest")
require_file_token("src/integrations/cpp/tests/test_widgets_capture.cpp" "testDestroyedTargetReportsTargetLost" "Widgets target-loss regression")
require_file_token("src/integrations/cpp/tests/test_quick_capture.cpp" "testDestroyedQuickTargetReportsTargetLost" "Quick target-loss parity regression")
require_file_token("src/runtime/src/widgets/widget_target.cpp" "the QWidget target was destroyed" "Widgets target-loss publication")
require_file_token("src/runtime/src/quick/quick_target.cpp" "the QQuickWindow target was destroyed" "Quick target-loss publication")
require_file_token("src/integrations/qml/QmlRemoteAccess.cpp" "m_targetDestroyedConnection" "QML target lifetime observation")
require_file_token("src/integrations/qml/QmlRemoteAccess.cpp" "emit targetChanged();" "QML destroyed target notification")
require_file_token("src/integrations/qml/tests/test_qml_module.cpp" "testTargetDestructionNotifiesDeclarativeProperty" "QML target destruction regression")
require_file_token("src/integrations/qml/tests/CMakeLists.txt" "hyremote-qml-module-test" "registered QML module lifecycle CTest")

# The V0.1 learning examples are teaching paths, not acceptance harnesses: they must use the public facade and stop
# the runtime explicitly, and nothing more is required of them. The stopped-runtime policy mutation they used to
# carry is owned by the product test below, where it is asserted against the real runtime rather than against an
# example's instrumentation.
foreach(example_source IN ITEMS
        "examples/learning/01-widgets-cpp/main.cpp"
        "examples/learning/02-quick-cpp/main.cpp")
    require_file_token("${example_source}" "HyRemote::RemoteAccess" "V0.1 learning example public facade")
    require_file_token("${example_source}" "remote.start()" "V0.1 learning example explicit runtime start")
    require_file_token("${example_source}" "remote.stop();" "V0.1 learning example explicit runtime stop")
endforeach()
require_file_token("src/integrations/cpp/tests/test_remote_access.cpp"
    "CHECK(!remote.setRemoteInputEnabled(false));"
    "policy mutation refused while running")
require_file_token("src/integrations/cpp/tests/test_remote_access.cpp"
    "CHECK(remote.setRemoteInputEnabled(false));"
    "policy mutation accepted while stopped")
require_file_token("tests/product-e2e/example_product_fit.py" "\"--policy-transition-ms\", \"15000\"" "E1/E2 bounded transition watchdog")
require_file_token("tests/product-e2e/example_product_fit.py" "The first disconnect is the deterministic trigger" "E1/E2 lifecycle-driven policy transition")
require_file_token("tests/product-e2e/example_product_fit.py" "public stop/configure/start" "E1/E2 same-process acceptance result")

require_file_token("examples/qml-basic/Main.qml" "property bool acceptanceSawViewer" "E3 first-viewer lifecycle observation")
require_file_token("examples/qml-basic/Main.qml" "function applyAcceptancePolicyTransition()" "E3 public-QML policy transition helper")
require_file_token("examples/qml-basic/Main.qml" "window.applyAcceptancePolicyTransition()" "E3 disconnect-driven policy transition")
require_file_token("examples/qml-basic/Main.qml" "policyTransitionTimer.stop()" "E3 watchdog cancellation after lifecycle trigger")
require_file_token("tests/product-e2e/qml_product_fit.py" "\"--policy-transition-ms\", \"15000\"" "E3 bounded policy-transition watchdog")
require_file_token("tests/product-e2e/qml_product_fit.py" "The disconnect above is also the deterministic policy-transition trigger" "E3 product-fit binds policy transition to viewer lifecycle")

# QPA now delegates application-level automatic access to the Common Runtime. The failure smoke remains
# QPA evidence, while the actual failure diagnostic is owned by Runtime::Automatic.
require_file_token("src/integrations/qpa/tests/qpa_remote_failure_native_survival_smoke.cpp" "class PortReservation final" "deterministic occupied-port QPA failure setup")
require_file_token("src/integrations/qpa/tests/qpa_remote_failure_native_survival_smoke.cpp" "SO_EXCLUSIVEADDRUSE" "deterministic Windows occupied-port ownership")
require_file_token("src/integrations/qpa/tests/qpa_remote_failure_native_survival_smoke.cpp" "gRemoteStartFailureSeen" "proof that the QPA remote runtime actually attempted and failed startup")
require_file_token("src/integrations/qpa/tests/qpa_remote_failure_native_survival_smoke.cpp" "forced remote bind failure was observed and the native QPA application remained live" "native-survival assertion after observed remote bind failure")
require_file_token("src/integrations/qpa/tests/CMakeLists.txt" "hyremote-qpa-remote-failure-native-survival-smoke" "registered QPA native-survival CTest")
require_file_token("src/integrations/qpa/tests/CMakeLists.txt" "PRIVATE ws2_32" "Windows native socket support for QPA failure smoke")
require_file_token("src/runtime/src/automatic/automatic_access_controller.cpp" "HyRemote automatic access could not start the composite runtime" "shared automatic runtime failure remains diagnostic rather than native-fatal")
require_file_token("docs/internal/v1-ga-acceptance.md" "remote-capability failure only" "canonical GA native-survival rule for QPA remote startup failure")

message(STATUS
    "HyRemote V1 runtime behavior contract gate: PASS "
    "(multi-viewer isolation + protected disconnect releases under backpressure + target-loss/QML lifetime parity + error/Faulted semantics + Widgets/Quick parity + same-process E1/E2/E3 policy lifecycle + shared automatic-access QPA failure survival)")
