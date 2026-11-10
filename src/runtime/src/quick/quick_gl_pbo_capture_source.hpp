#pragma once

#include "hyremote/core/capture_source.hpp"
#include <memory>

class QQuickWindow;

namespace HyRemote::detail {

// Experimental Qt6.8 OpenGL-RHI SOURCE, not a general Qt6 GPU interface.
// Called only when a real QQuickWindow reports OpenGL as its graphics API.
// The normal public grabToImage route stays the unconditional default.
std::unique_ptr<hyremote::CaptureSource>
createExperimentalQuickGlPboCaptureSource(QQuickWindow *window);

} // namespace HyRemote::detail
