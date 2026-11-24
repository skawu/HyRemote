#pragma once
// Internal Qt6 Quick D3D11 staging provider. Compiled only for WIN32 and an
// exact explicitly qualified Qt Kit; never exported to the public SDK.
#include <memory>
class QQuickWindow;
#include "hyremote/core/capture_source.hpp"
#include "quick/quick_native_stop_probe.hpp"
namespace HyRemote::detail {
// Repository-only deterministic failure-injection seam, not public SDK API.
bool runQuickD3d11DispatchRejectionRegression();

// Repository-only evidence of worker-originated stop posting. These test
// hooks are not public transport/SDK APIs and never select a GPU backend.
void quickD3d11RejectNextStopPostForTest(unsigned count) noexcept;
QuickNativeStopPostEvidence quickD3d11StopPostEvidence() noexcept;

std::unique_ptr<hyremote::CaptureSource>
createExperimentalQuickD3d11StagingCaptureSource(QQuickWindow *window);
} // namespace HyRemote::detail
