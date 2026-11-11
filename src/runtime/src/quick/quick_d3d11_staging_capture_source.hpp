#pragma once
// Internal Qt6 Quick D3D11 staging provider. Compiled only for WIN32 and an
// exact explicitly qualified Qt Kit; never exported to the public SDK.
#include <memory>
class QQuickWindow;
namespace hyremote { class CaptureSource; }
namespace HyRemote::detail {
std::unique_ptr<hyremote::CaptureSource>
createExperimentalQuickD3d11StagingCaptureSource(QQuickWindow *window);
} // namespace HyRemote::detail
