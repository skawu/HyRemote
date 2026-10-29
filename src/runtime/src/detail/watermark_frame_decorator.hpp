#pragma once

// #589 W2b: adapts the pixel compositor to the transport's frame hook. The transport stays free of Qt Gui; the
// Runtime builds this decorator once per started session, from the resolved policy and the target's device pixel
// ratio, and hands it to the RFB transport.

#include "transport/rfb_transport.hpp"
#include "watermark_policy.hpp"

namespace HyRemote::detail {

// Returns an empty decorator when the policy is disabled, which is what keeps the disabled path copy-free: the
// transport then encodes the captured frame itself.
RfbFrameDecorator makeWatermarkFrameDecorator(const WatermarkPolicy &policy, double devicePixelRatio);

}  // namespace HyRemote::detail
