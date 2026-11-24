#pragma once

#include "detail/component_factories.hpp"

namespace HyRemote::detail {

// Composite captures one pixel per logical canvas unit (#806); direct Quick
// uses the physical Qt render grid. Internal only, never installed SDK.
TargetComponents createQuickTargetComponents(
    QObject *target, bool remoteInputEnabled,
    bool logicalPixelsForComposite = false);

}  // namespace HyRemote::detail
