#pragma once

#include <memory>

namespace hyremote { class CaptureSource; }
class QQuickWindow;

namespace HyRemote::detail {
// R3 non-shipping source: wraps existing public Qt grab with a bounded
// afterFrameEnd OpenGL RHI readback candidate. Requires double opt-in.
std::unique_ptr<hyremote::CaptureSource> makeExperimentalQuickGlFrameTap(
    QQuickWindow *window, std::unique_ptr<hyremote::CaptureSource> publicFallback);
} // namespace HyRemote::detail
