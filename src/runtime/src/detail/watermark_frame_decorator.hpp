#pragma once

// #589 W2b: adapts the pixel compositor to the transport's frame hook. The transport stays free of Qt Gui; the
// Runtime builds this decorator once per started session from the resolved policy. The device pixel ratio comes
// from each frame's own geometry (FrameGeometry::devicePixelRatio), so the watermark keeps its physical size when
// the captured target moves to a differently scaled screen instead of freezing the ratio seen at startup.

#include "transport/rfb_transport.hpp"
#include "watermark_policy.hpp"

namespace HyRemote::detail {

// Returns an empty decorator when the policy is disabled, which is what keeps the disabled path copy-free: the
// transport then encodes the captured frame itself.
RfbFrameDecorator makeWatermarkFrameDecorator(const WatermarkPolicy &policy);

}  // namespace HyRemote::detail
