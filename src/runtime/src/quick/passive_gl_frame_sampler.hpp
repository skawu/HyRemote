#pragma once

// #609 experimental, build-option gated Qt Quick frame source.
// A native OpenGL on-screen frame is read only from Qt's render thread.
// The default QuickCaptureSource/grabToImage route remains the safe fallback.
#include <optional>

#include <QSize>
#include "hyremote/core/frame.hpp"

class QQuickWindow;

namespace HyRemote::detail {

std::optional<hyremote::RemoteFrame> readPassiveQuickGlFrame(
    QQuickWindow *window, const QSize &expectedPixelSize, float devicePixelRatio);

}  // namespace HyRemote::detail
